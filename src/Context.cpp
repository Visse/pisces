#include "Context.h"
#include "HardwareResourceManager.h"
#include "PipelineManager.h"
#include "RenderCommandQueue.h"
#include "CompiledRenderQueue.h"

#include "internal/GLCompat.h"
#include "internal/GLDebugCallback.h"
#include "internal/GLTypes.h"
#include "internal/Helpers.h"
#include "internal/CompiledRenderQueueImpl.h"
#include "internal/PipelineManagerImpl.h"
#include "internal/HardwareResourceManagerImpl.h"

#include "Common/Throw.h"
#include "Common/ErrorUtils.h"
#include "Common/HandleType.h"
#include "Common/HandleVector.h"

#include <glbinding/Binding.h>
#include <glbinding/gl33core/gl.h>
using namespace gl33core;

#include <glm/vec2.hpp>

#include <SDL.h>
#include <stdexcept>
#include <initializer_list>
#include <cassert>


#include <glbinding/Version.h>
#include <glbinding/Meta.h>
#include <glbinding/ContextInfo.h>

namespace Pisces
{
    struct SDLWindowDelete {
        void operator () ( SDL_Window *window ) {
            SDL_DestroyWindow(window);
        }
    };
    WRAP_HANDLE(SDLWindow, SDL_Window*, SDLWindowDelete, nullptr);

    struct SDLGLContextDelete {
        void operator () ( SDL_GLContext context ) {
            SDL_GL_DeleteContext(context);
        }
    };
    WRAP_HANDLE(SDLGLContext, SDL_GLContext, SDLGLContextDelete, nullptr);

    struct RenderTargetInfo {
        GLFrameBuffer glFramebuffer;
        glm::ivec2 size;

        Color clearColor = NamedColors::Black;
        float clearDepth = 1.0f;
        int clearStencil = 0;
    };

    static Context *gContext;


    struct DisplayResizedCallbackInfo {
        DisplayResizedCallback callback;
        const void *id;
    };

    struct Context::Impl {
        SDLWindow window;
        SDLGLContext context;

        std::unique_ptr<HardwareResourceManager> hardwareResourceMgr;
        std::unique_ptr<PipelineManager> pipelineMgr;

        HandleVector<RenderTargetHandle, RenderTargetInfo> renderTargets;

        uint64_t currentFrame = 0;
        GLFrameSync frameSync[FRAMES_IN_FLIGHT];

        RenderTargetHandle mainRenderTarget;

        glm::ivec2 displaySize;
        bool isFullscreen = false;

        std::vector<DisplayResizedCallbackInfo> callbackDisplayResized;

        struct {
            int textureUnits = 0,
                textureLayers = 0,
                imageUnits = 0;

            void init() {
                glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &textureUnits);
                glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &textureLayers);
                glGetIntegerv(gl::GL_MAX_IMAGE_UNITS, &imageUnits);

                LOG_INFORMATION("Found limits of the graphics context:");
                LOG_INFORMATION("Maximum number of texture units: %i", textureUnits);
                LOG_INFORMATION("Maximum number of texture layers: %i", textureLayers);
                LOG_INFORMATION("Maximum number of image units: %i", imageUnits);
            }
        } limits;

        void onDisplayResized( int width, int height ) {
            glm::ivec2 newSize{width, height};
            if (displaySize == newSize) return;

            LOG_INFORMATION("Display size changed to %ix%i", width, height);

            displaySize = newSize;
            renderTargets.find(mainRenderTarget)->size = newSize;

            for (const auto &info : callbackDisplayResized) {
                info.callback(width, height);
            }
        }

        template< typename Container >
        void removeAllWithId( Container &container, const void *id ) {
            using value_type = Container::value_type;

            auto end = std::remove_if(container.begin(), container.end(), 
                [id]( const value_type &val ){
                    return val.id == id;
            });

            container.erase(end, container.end());
        }

        void removeAllCallbacks( const void *id )
        {
            removeAllWithId(callbackDisplayResized, id);
        }
    };

    Context* Context::Initilize( const InitParams &params )
    {
        FATAL_ASSERT( gContext == nullptr, "Double initilization of context!");
        gContext = new Context( params );
        return gContext;
    }

    void Context::Shutdown()
    {
        delete gContext;
        gContext = nullptr;
    }
    
    Context* Context::GetContext()
    {
        return gContext;
    }

    Context::Context( const InitParams &params )
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG|SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

        SDLWindow window(SDL_CreateWindow(params.windowTitle, 
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            params.displayWidth, params.displayHeight,
            SDL_WINDOW_OPENGL
        ));
        if (!window) {
            THROW( std::runtime_error,
                "Failed to create window"
            );
        }
        
        // @todo check if it valid to set version here,
        //       because according to documentation, SDL_GL_SetAttribute
        //       should be called before creating a window
       // SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
       // SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
        SDLGLContext context(SDL_GL_CreateContext(window));
        
        if (!context) {
            context = SDLGLContext(SDL_GL_CreateContext(window));
        }

        if( !context ) {
            THROW( std::runtime_error,
                   "Failed to create gl context - ",
                   SDL_GetError()
            );
        }

        SDL_GL_GetDrawableSize(window, &mImpl->displaySize.x, &mImpl->displaySize.y);

        mImpl->window  = std::move(window);
        mImpl->context = std::move(context);

        SDL_GL_SetSwapInterval( 1 );

        glbinding::Binding::initialize();
        LOG_INFORMATION("Using OpenGl version %s", glbinding::ContextInfo::version().toString().c_str());

        LOG_INFORMATION("Vendor is \"%s\"", (const char*)glGetString(GL_VENDOR));
        LOG_INFORMATION("Renderer is \"%s\"", (const char*)glGetString(GL_RENDERER));
        LOG_INFORMATION("Reported version is \"%s\"", (const char*)glGetString(GL_VERSION));

        const auto &supportedExtensions = glbinding::ContextInfo::extensions();


        int maxVersion = 0;
        for (auto version : glbinding::Meta::versions()) {
            const auto &versionExtensions = glbinding::Meta::extensions(version);

            if (std::includes(supportedExtensions.begin(), supportedExtensions.end(), versionExtensions.begin(), versionExtensions.end())) {
                maxVersion = std::max(maxVersion, version.majorVersion()*10 + version.minorVersion());
            }
        }
        LOG_INFORMATION("Opengl Extensions up to version %i.%i is supported", maxVersion/10, maxVersion%10);

        mImpl->limits.init();

        GLCompat::InitCompat(params.enableExtensions);

        InstallGLDebugHooks();

        // Mark the first frame as done by swapping
        glClearColor(0,0,0,0);
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(mImpl->window);


        mImpl->hardwareResourceMgr.reset(new HardwareResourceManager(this));
        mImpl->pipelineMgr.reset(new PipelineManager(this));

        RenderTargetInfo info;
            info.glFramebuffer = GLFrameBuffer(0);
            info.size = glm::ivec2(params.displayWidth, params.displayHeight);

        mImpl->mainRenderTarget = mImpl->renderTargets.create(std::move(info));

        /// Initilize the sync objects
        for (auto &sync : mImpl->frameSync) {
            sync = GLFrameSync(glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, GL_NONE_BIT));
        }
    }

    HardwareResourceManager* Context::getHardwareResourceManager()
    {
        return mImpl->hardwareResourceMgr.get();
    }

    PipelineManager* Context::getPipelineManager()
    {
        return mImpl->pipelineMgr.get();
    }

    RenderTargetHandle Context::getMainRenderTarget()
    {
        return mImpl->mainRenderTarget;
    }

    RenderCommandQueuePtr Context::createRenderCommandQueue( RenderTargetHandle target, RenderCommandQueueFlags flags )
    {
        if (!target) target = mImpl->mainRenderTarget;
        return std::make_shared<RenderCommandQueue>(this, target, flags);
    }

    CompiledRenderQueuePtr Context::compile( const RenderCommandQueuePtr &queue, const RenderQueueCompileOptions &options )
    {
        return std::make_shared<CompiledRenderQueue>(this, queue, options);
    }

    void Context::execute( const RenderCommandQueuePtr &queue )
    {
        CompiledRenderQueuePtr compiled = compile(queue);
        execute(compiled);

        // Restore state
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
    }

    void Context::execute( const CompiledRenderQueuePtr &queue )
    {
        using namespace CompiledRenderQueueImpl;
        CompiledRenderQueueImpl::Impl *queueImpl = queue->impl();

        // @todo cache current rendertarget
        RenderTargetInfo *info = mImpl->renderTargets.find(queueImpl->renderTarget);
        if (!info) return;

        glBindFramebuffer(GL_FRAMEBUFFER, info->glFramebuffer);
        glViewport(0, 0, info->size.x, info->size.y);

        for (const Command &cmd : queueImpl->commands) {
            switch (cmd.type) {
            case Type::Enable:
                glEnable(cmd.enable.cap);
                break;
            case Type::Disable:
                glDisable(cmd.disable.cap);
                break;
            case Type::SetProgram:
                glUseProgram(cmd.setProgram.program);
                break;
            case Type::SetBlendFunc:
                glBlendFunc(cmd.setBlendFunc.sfactor, cmd.setBlendFunc.dfactor);
                break;
            case Type::SetDepthMask:
                glDepthMask(cmd.setDepthMask.mask);
                break;
            case Type::SetClipRect: {
                ClipRect rect = cmd.setClipRect.rect;
                glScissor(rect.x, rect.y, rect.w, rect.h);
              } break;
            case Type::SetClearColor: {
                Color col = cmd.setClearColor.color;
                glClearColor(col.r/255.f, col.g/255.f, col.b/255.f, col.a/255.f);
              } break;
            case Type::SetClearDepth:
                glClearDepth(cmd.setClearDepth.depth);
                break;
            case Type::SetClearStencil:
                glClearStencil(cmd.setClearStencil.stencil);
                break;
            case Type::BindVertexArray:
                glBindVertexArray(cmd.bindVertexArray.vertexArray);
                break;
            case Type::BindIndexBuffer:
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cmd.bindIndexBuffer.indexBuffer);
                break;
            case Type::BindSampler:
                glBindSampler(cmd.bindSampler.unit, cmd.bindSampler.sampler);
                GLCompat::BindTexture(cmd.bindSampler.unit, cmd.bindSampler.target, cmd.bindSampler.texture);
                break;
            case Type::BindUniform:
                glBindBufferRange(GL_UNIFORM_BUFFER, cmd.bindUniform.unit, cmd.bindUniform.buffer, cmd.bindUniform.offset, cmd.bindUniform.size);
                break;
            case Type::Clear:
                glClear((gl::ClearBufferMask)cmd.clear.mask);
                break;
            case Type::Draw:
                glDrawArrays(cmd.draw.primitive, (GLint)cmd.draw.first, (GLsizei)cmd.draw.count);
                break;
            case Type::DrawIndexed:
                glDrawElementsBaseVertex(cmd.drawIndexed.primitive, (GLsizei)cmd.drawIndexed.count, cmd.drawIndexed.indexType, cmd.drawIndexed.offset, (GLint)cmd.drawIndexed.base);
                break;
            case Type::BindUniformInt:
                glUniform1i(cmd.bindUniformInt.location, cmd.bindUniformInt.value);
                break;
            case Type::BindUniformMat4:
                glUniformMatrix4fv(cmd.bindUniformMat4.location, 1, GL_FALSE, cmd.bindUniformMat4.matrix.data());
                break;
            case Type::BindImageTexture:
                gl::glBindImageTexture(cmd.bindImageTexture.unit, cmd.bindImageTexture.texture, cmd.bindImageTexture.level, cmd.bindImageTexture.layered, cmd.bindImageTexture.layer, cmd.bindImageTexture.access, cmd.bindImageTexture.format);
                break;
            case Type::DispatchCompute:
                gl::glDispatchCompute(cmd.dispatchCompute.size_x, cmd.dispatchCompute.size_y, cmd.dispatchCompute.size_z);
                break;
            }
        }
    }

    void Context::swapFrameBuffer()
    {
        int syncNum = mImpl->currentFrame % FRAMES_IN_FLIGHT;
        if (glClientWaitSync(mImpl->frameSync[syncNum], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED) != GL_ALREADY_SIGNALED) {
            LOG_WARNING("Gpu had not finnished the frame when we expected it to, consider increasing FRAMES_IN_FLIGHT (currently %i)", FRAMES_IN_FLIGHT);
        }
        mImpl->frameSync[syncNum] = GLFrameSync(glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, GL_NONE_BIT));
        
        SDL_GL_SwapWindow(mImpl->window);
        mImpl->currentFrame++;
    }

    uint64_t Context::currentFrame()
    {
        return mImpl->currentFrame;
    }

    int Context::displayWidth()
    {
        return mImpl->displaySize.x;
    }

    int Context::displayHeight()
    {
        return mImpl->displaySize.y;
    }

    void Context::addDisplayResizedCallback( DisplayResizedCallback callback, const void *id )
    {
        mImpl->callbackDisplayResized.push_back({callback, id});
    }

    void Context::removeCallback( const void *id )
    {
        mImpl->removeAllCallbacks(id);
    }

    void Context::toogleFullscreen()
    {
        mImpl->isFullscreen = !mImpl->isFullscreen;
        if (mImpl->isFullscreen) {
            SDL_SetWindowFullscreen(mImpl->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
        else {
            SDL_SetWindowFullscreen(mImpl->window, 0);
        }

        int width, height;
        SDL_GL_GetDrawableSize(mImpl->window, &width, &height);
        mImpl->onDisplayResized(width, height);
    }

    void Context::clearMainRenderTarget()
    {
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClearDepth(1.f);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    int Context::getHardwareLimit( HardwareLimitName limit )
    {
        switch (limit) {
        case HardwareLimitName::TextureUnits:
            return mImpl->limits.textureUnits;
        }
        FATAL_ERROR("Unknown HardwareLimitName %i", (int)limit);
    }
}
