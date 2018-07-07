#include "Context.h"
#include "HardwareResourceManager.h"
#include "PipelineManager.h"
#include "RenderCommandQueue.h"
#include "CompiledRenderQueue.h"
#include "IResourceLoader.h"
#include "TextureLoader.h"
#include "PipelineLoader.h"
#include "SpriteManager.h"
#include "SpriteLoader.h"
#include "ProgramLoader.h"

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
#include "Common/Archive.h"
#include "Common/MemStreamBuf.h"

#include "libyaml-cpp.h"

#include <glbinding/Binding.h>
#include <glbinding/gl33core/gl.h>
using namespace gl33core;

#include <glm/vec2.hpp>

#include <SDL.h>
#include <stdexcept>
#include <initializer_list>
#include <cassert>
#include <map>


#include <glbinding/Version.h>
#include <glbinding/Meta.h>
#include <glbinding/ContextInfo.h>

#include <Remotery.h>

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

    struct RemoteryDelete {
        void operator () ( Remotery *rmt ) {
            rmt_UnbindOpenGL();
            rmt_DestroyGlobalInstance(rmt);
        }
    };
    WRAP_HANDLE(RemoteryContext, Remotery*, RemoteryDelete, nullptr);

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

    struct ResourceInfo {
        IResourceLoader *loader;

        Common::StringId name;
        ResourceHandle handle;
    };
    struct ResourcePackInfo {
        Common::StringId name;
        std::vector<ResourceInfo> resources;
    };

    struct Context::Impl {
        SDLWindow window;
        SDLGLContext context;
        RemoteryContext remotery;

        std::unique_ptr<HardwareResourceManager> hardwareResourceMgr;
        std::unique_ptr<PipelineManager> pipelineMgr;
        std::unique_ptr<SpriteManager> spriteMgr;

        HandleVector<RenderTargetHandle, RenderTargetInfo> renderTargets;

        uint64_t currentFrame = 0;
        GLFrameSync frameSync[FRAMES_IN_FLIGHT];

        RenderTargetHandle mainRenderTarget;

        glm::ivec2 displaySize,
                   windowSize;
        bool isFullscreen = false;

        std::vector<DisplayResizedCallbackInfo> callbackDisplayResized;

        std::map<Common::StringId, IResourceLoader*> resourceLoaders;
        HandleVector<ResourcePackHandle, ResourcePackInfo> resourcePacks;

        std::vector<std::unique_ptr<IResourceLoader>> coreResourceLoaders;

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

    PISCES_API Context* Context::Initilize( const InitParams &params )
    {
        FATAL_ASSERT( gContext == nullptr, "Double initilization of context!");
        gContext = new Context( params );
        return gContext;
    }

    PISCES_API void Context::Shutdown()
    {
        delete gContext;
        gContext = nullptr;
    }
    
    PISCES_API Context* Context::GetContext()
    {
        return gContext;
    }

    PISCES_API Context::Context( const InitParams &params )
    {
        if (params.initRemotery) {
            rmt_CreateGlobalInstance(&mImpl->remotery.handle);
        }

        rmt_ScopedCPUSampleString("Pisces::Context::init", RMTSF_None);

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 
            (params.enableDebugContext ? SDL_GL_CONTEXT_DEBUG_FLAG : 0) | SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG
        );
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

        if (!context) {
            THROW( std::runtime_error,
                   "Failed to create gl context - %s",
                   SDL_GetError()
            );
        }

        SDL_GL_GetDrawableSize(window, &mImpl->displaySize.x, &mImpl->displaySize.y);
        SDL_GetWindowSize(window, &mImpl->windowSize.x, &mImpl->windowSize.y);

        mImpl->window  = std::move(window);
        mImpl->context = std::move(context);

        if (params.enableVSync) {
            SDL_GL_SetSwapInterval( 1 );
        }

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


#ifdef PISCES_CLIP_ZERO_TO_ONE
        if (!glbinding::ContextInfo::supported({gl::GLextension::GL_ARB_clip_control})) {
            LOG_ERROR("Opengl does not support extension ARB_clip_control - try updating your drivers");
            THROW(std::runtime_error, "Opengl required extension ARB_clip_control not supported");
        }
        gl::glClipControl(gl::GL_LOWER_LEFT, gl::GL_ZERO_TO_ONE);
#endif

        mImpl->limits.init();

        GLCompat::InitCompat(params.enableExtensions);

        if (params.enableDebugContext) {
            InstallGLDebugHooks();
        }

        rmt_BindOpenGL();

        // Mark the first frame as done by swapping
        glClearColor(0,0,0,0);
        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(mImpl->window);


        mImpl->hardwareResourceMgr.reset(new HardwareResourceManager(this));
        mImpl->pipelineMgr.reset(new PipelineManager(this));
        mImpl->spriteMgr.reset(new SpriteManager(this));

        RenderTargetInfo info;
            info.glFramebuffer = GLFrameBuffer(0);
            info.size = glm::ivec2(params.displayWidth, params.displayHeight);

        mImpl->mainRenderTarget = mImpl->renderTargets.create(std::move(info));

        /// Initilize the sync objects
        for (auto &sync : mImpl->frameSync) {
            sync = GLFrameSync(glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, GL_NONE_BIT));
        }
        // wait 10ms for sync
        glClientWaitSync(mImpl->frameSync[0], GL_SYNC_FLUSH_COMMANDS_BIT, 10000000);

        mImpl->coreResourceLoaders.emplace_back(new TextureLoader(mImpl->hardwareResourceMgr.get()));
        registerResourceLoader(Common::CreateStringId("Texture"), mImpl->coreResourceLoaders.back().get());

        RenderProgramLoader *renderProgramLoader = new RenderProgramLoader(mImpl->pipelineMgr.get());
        mImpl->coreResourceLoaders.emplace_back(renderProgramLoader);
        registerResourceLoader(Common::CreateStringId("RenderProgram"), renderProgramLoader);

        mImpl->coreResourceLoaders.emplace_back(new ComputeProgramLoader(mImpl->pipelineMgr.get()));
        registerResourceLoader(Common::CreateStringId("ComputeProgram"), mImpl->coreResourceLoaders.back().get());
        
        mImpl->coreResourceLoaders.emplace_back(new TransformProgramLoader(mImpl->pipelineMgr.get()));
        registerResourceLoader(Common::CreateStringId("TransformProgram"), mImpl->coreResourceLoaders.back().get());

        mImpl->coreResourceLoaders.emplace_back(new PipelineLoader(mImpl->pipelineMgr.get(), renderProgramLoader));
        registerResourceLoader(Common::CreateStringId("Pipeline"), mImpl->coreResourceLoaders.back().get());

        mImpl->coreResourceLoaders.emplace_back(new SpriteLoader(mImpl->spriteMgr.get(), mImpl->hardwareResourceMgr.get()));
        registerResourceLoader(Common::CreateStringId("Sprite"), mImpl->coreResourceLoaders.back().get());

        // Initilize the default state
        execute(std::make_shared<CompiledRenderQueue>(this, CompiledRenderQueue::InitDefaultState_tag{}));
    }

    PISCES_API HardwareResourceManager* Context::getHardwareResourceManager()
    {
        return mImpl->hardwareResourceMgr.get();
    }

    PISCES_API PipelineManager* Context::getPipelineManager()
    {
        return mImpl->pipelineMgr.get();
    }

    PISCES_API SpriteManager* Context::getSpriteManager()
    {
        return mImpl->spriteMgr.get();
    }

    PISCES_API RenderTargetHandle Context::getMainRenderTarget()
    {
        return mImpl->mainRenderTarget;
    }

    PISCES_API RenderCommandQueuePtr Context::createRenderCommandQueue( RenderTargetHandle target, RenderCommandQueueFlags flags )
    {
        if (!target) target = mImpl->mainRenderTarget;
        return std::make_shared<RenderCommandQueue>(this, target, flags);
    }

    PISCES_API CompiledRenderQueuePtr Context::compile( const RenderCommandQueuePtr &queue, const RenderQueueCompileOptions &options )
    {
        rmt_ScopedCPUSampleString("Pisces::Context::compile", RMTSF_None);
        return std::make_shared<CompiledRenderQueue>(this, queue, options);
    }

    PISCES_API void Context::execute( const RenderCommandQueuePtr &queue )
    {
        rmt_ScopedCPUSampleString("Pisces::Context::execute", RMTSF_None);
        CompiledRenderQueuePtr compiled = compile(queue);
        execute(compiled);
    }

    PISCES_API void Context::execute( const CompiledRenderQueuePtr &queue )
    {
        rmt_ScopedCPUSampleString("Pisces::Context::execute", RMTSF_Aggregate);
        rmt_ScopedOpenGLSampleString("Pisces::Context::execute");

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
            case Type::BindSampler:
                glBindSampler(cmd.bindSampler.unit, cmd.bindSampler.sampler);
                GLCompat::BindTexture(cmd.bindSampler.unit, cmd.bindSampler.target, cmd.bindSampler.texture);
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
            case Type::BindUniformUInt:
                glUniform1ui(cmd.bindUniformUInt.location, cmd.bindUniformUInt.value);
                break;
            case Type::BindUniformFloat:
                glUniform1f(cmd.bindUniformFloat.location, cmd.bindUniformFloat.value);
                break;
            case Type::BindUniformVec2:
                glUniform2fv(cmd.bindUniformVec2.location, 1, cmd.bindUniformVec2.vec.data());
                break;
            case Type::BindUniformVec3:
                glUniform3fv(cmd.bindUniformVec3.location, 1, cmd.bindUniformVec3.vec.data());
                break;
            case Type::BindUniformVec4:
                glUniform4fv(cmd.bindUniformVec4.location, 1, cmd.bindUniformVec4.vec.data());
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
            case Type::BindBufferRange:
                glBindBufferRange(cmd.bindBufferRange.target, cmd.bindBufferRange.index, cmd.bindBufferRange.buffer, cmd.bindBufferRange.offset, cmd.bindBufferRange.size);
                break;
            case Type::BeginTransformFeedback:
                glBeginTransformFeedback(cmd.beginTransformFeedback.primitive);
                break;
            case Type::EndTransformFeedback:
                glEndTransformFeedback();
                break;
            case Type::BindBuffer:
                glBindBuffer(cmd.bindBuffer.target, cmd.bindBuffer.buffer);
                break;
            case Type::CopyBufferSubData:
                glCopyBufferSubData(cmd.copyBufferSubData.readTarget, cmd.copyBufferSubData.writeTarget, cmd.copyBufferSubData.readOffset, cmd.copyBufferSubData.writeOffset, cmd.copyBufferSubData.size);
                break;
            case Type::PrimitiveRestartIndex:
                glPrimitiveRestartIndex(cmd.primitiveRestartIndex.index);
                break;
            }
        }
    }

    PISCES_API void Context::swapFrameBuffer()
    {
        rmt_ScopedCPUSampleString("Pisces::Context::swapFrameBuffer", RMTSF_None);
        rmt_ScopedOpenGLSampleString("Pisces::Context::swapFrameBuffer");

        int syncNum = mImpl->currentFrame % FRAMES_IN_FLIGHT;
        if (glClientWaitSync(mImpl->frameSync[syncNum], GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED) != GL_ALREADY_SIGNALED) {
            LOG_WARNING("Gpu had not finnished the frame when we expected it to, consider increasing FRAMES_IN_FLIGHT (currently %i)", FRAMES_IN_FLIGHT);
        }
        mImpl->frameSync[syncNum] = GLFrameSync(glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, GL_NONE_BIT));
        
        SDL_GL_SwapWindow(mImpl->window);
        mImpl->currentFrame++;
    }

    PISCES_API uint64_t Context::currentFrame()
    {
        return mImpl->currentFrame;
    }

    PISCES_API int Context::displayWidth()
    {
        return mImpl->displaySize.x;
    }

    PISCES_API int Context::displayHeight()
    {
        return mImpl->displaySize.y;
    }

    PISCES_API int Context::windowWidth()
    {
        return mImpl->windowSize.x;
    }

    PISCES_API int Context::windowHeight()
    {
        return mImpl->windowSize.y;
    }

    PISCES_API void Context::addDisplayResizedCallback( DisplayResizedCallback callback, const void *id )
    {
        mImpl->callbackDisplayResized.push_back({callback, id});
    }

    PISCES_API void Context::removeCallback( const void *id )
    {
        mImpl->removeAllCallbacks(id);
    }

    PISCES_API void Context::toogleFullscreen()
    {
        mImpl->isFullscreen = !mImpl->isFullscreen;
        if (mImpl->isFullscreen) {
            SDL_SetWindowFullscreen(mImpl->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        }
        else {
            SDL_SetWindowFullscreen(mImpl->window, 0);
        }

        SDL_GetWindowSize(mImpl->window, &mImpl->windowSize.x,&mImpl->windowSize.y);

        int width, height;
        SDL_GL_GetDrawableSize(mImpl->window, &width, &height);
        mImpl->onDisplayResized(width, height);
    }

    PISCES_API void Context::clearMainRenderTarget()
    {
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClearDepth(1.f);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    PISCES_API int Context::getHardwareLimit( HardwareLimitName limit )
    {
        switch (limit) {
        case HardwareLimitName::TextureUnits:
            return mImpl->limits.textureUnits;
        }
        FATAL_ERROR("Unknown HardwareLimitName %i", (int)limit);
    }

    PISCES_API void Context::registerResourceLoader( Common::StringId name, IResourceLoader *loader )
    {
        FATAL_ASSERT(loader != nullptr, "Loader can't be null!");

        LOG_INFORMATION("Registred resource loader for type \"%s\"", Common::GetCString(name));
        mImpl->resourceLoaders[name] = loader;
    }

    PISCES_API ResourcePackHandle Context::loadResourcePack( const char *name )
    {
        LOG_INFORMATION("Loading resource pack \"%s\"", name);

        std::vector<ResourceInfo> resources;
        try {
            Common::Archive archive = Common::Archive::OpenArchive(name);
            auto file = archive.openFile("resources.txt");

            if (!file) {
                LOG_ERROR("Failed to load resource pack \"%s\" - failed to open file \"resources.txt\"", name);
                return ResourcePackHandle{};
            }

            Common::MemStreamBuf buf(archive.mapFile(file), archive.fileSize(file));
            std::istream stream(&buf);

            auto node = libyaml::Node::LoadStream("resources.txt", stream);
            if (!node.isSequence()) {
                LOG_ERROR("Failed to load resource pack \"%s\" - top level node in \"resources.txt\" must be a sequence", name);
            }

            for (libyaml::Node entry : node) {
                auto resourceTypeNode = entry["Type"];
                if (!resourceTypeNode.isScalar()) {
                    auto mark = entry.endMark();
                    LOG_WARNING("Failed to load resource in pack \"%s\" from file \"resources.txt\" - missing attribute \"Type\" at %i:%i", name,  mark.line, mark.col);
                    continue;
                }
                auto resourceNameNode = entry["Name"];
                if (!resourceNameNode.isScalar()) {
                    auto mark = entry.startMark();
                    LOG_WARNING("Failed to load resource in pack \"%s\" from file \"resources.txt\" - missing attribute \"Name\" at %i:%i", name,  mark.line, mark.col);
                    continue;
                }
                
                Common::StringId resourceName = Common::CreateStringId(resourceNameNode.scalar());
                Common::StringId type = Common::CreateStringId(resourceTypeNode.scalar());

                auto iter = mImpl->resourceLoaders.find(type);
                if (iter == mImpl->resourceLoaders.end()) {
                    LOG_WARNING("Failed to load resource in pack \"%s\" from file \"resources.txt\" - missing loader for resource type \"%s\"", name, Common::GetCString(type));
                    continue;
                }

                LOG_INFORMATION("Loading resource \"%s\" of type \"%s\"", Common::GetCString(resourceName), Common::GetCString(type));

                IResourceLoader *loader = iter->second;

                try {
                    ResourceHandle resource = loader->loadResource(archive, entry);

                    ResourceInfo info;
                        info.handle = resource;
                        info.loader;
                        info.name = resourceName;

                    resources.push_back(std::move(info));
                } catch( const std::exception &e) {
                    LOG_WARNING("Failed to load resource \"%s\" of type \"%s\" in pack \"%s\" from file \"resources.cfg\" - resource loader failed with the error: %s", 
                                Common::GetCString(resourceName), Common::GetCString(type), name, e.what()
                    );
                }
            }
        }
        catch (const std::exception &e) {
            LOG_ERROR("Failed to load resource pack \"%s\" - caught exception \"%s\"", name, e.what());
            return ResourcePackHandle{};
        }

        LOG_INFORMATION("Finnished loading resoruce pack \"%s\", %zu resources was loaded", name, resources.size());
        ResourcePackInfo info;
            info.name = Common::CreateStringId(name);
            info.resources = std::move(resources);

        return mImpl->resourcePacks.create(std::move(info));
    }

    PISCES_API Remotery* Context::remoteryContext()
    {
        return mImpl->remotery;
    }
}
