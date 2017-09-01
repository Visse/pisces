#include "NkContext.h"

#include "nuklear.h"

#include "Pisces/Context.h"
#include "Pisces/HardwareResourceManager.h"
#include "Pisces/PipelineManager.h"
#include "Pisces/StreamingBuffer.h"
#include "Pisces/RenderCommandQueue.h"

#include "Common/HandleType.h"
#include "Common/ErrorUtils.h"
#include "Common/Throw.h"

#include <SDL.h>

#include <cassert>
#include <algorithm>

#include <glm/mat4x4.hpp>

namespace Pisces
{
    namespace 
    {
        void freeNkContext( nk_context *context ) {
            nk_free(context);
            delete context;
        }
        void freeNkFontAtlas( nk_font_atlas *atlas ) {
            nk_font_atlas_clear(atlas);
            delete atlas;
        }

        void freeNkBuffer( nk_buffer *buffer ) {
            nk_buffer_free(buffer);
            delete buffer;
        }

        WRAP_HANDLE_FUNC(NkContextHandle, nk_context*, freeNkContext, nullptr);
        WRAP_HANDLE_FUNC(NkFontAtlasHandle, nk_font_atlas*, freeNkFontAtlas, nullptr);
        WRAP_HANDLE_FUNC(NkBufferHandle, nk_buffer*, freeNkBuffer, nullptr);

        struct nkVertex {
            float pos[2];
            float uv[2];
            uint8_t col[4];
        };
    
        using nkIndex = uint16_t;
        static_assert( sizeof(nkIndex) == sizeof(nk_draw_index), "nk_draw_index unexpected type!");
        
        const IndexType nkIndexType = IndexType::UInt16;
    
        const nk_draw_vertex_layout_element nkVertexLayout[] = {
                {NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(nkVertex, pos)},
                {NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(nkVertex, uv)},
                {NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, offsetof(nkVertex, col)},
                {NK_VERTEX_LAYOUT_END}
        };


        const int NK_VERTEX_LAYOUT_SIZE = 3;
        const VertexAttribute NK_VERTEX_LAYOUT[NK_VERTEX_LAYOUT_SIZE] = {
            {VertexAttributeType::Float32, offsetof(nkVertex, pos), 2, sizeof(nkVertex), 0},
            {VertexAttributeType::Float32, offsetof(nkVertex, uv), 2, sizeof(nkVertex), 0},
            {VertexAttributeType::NormUInt8, offsetof(nkVertex, col), 4, sizeof(nkVertex), 0},
        };

        const size_t DEFAULT_VERTEX_BUFFER_SIZE = 4096,
                     DEFAULT_INDEX_BUFFER_SIZE  = 8192;

        const size_t MAX_VERTEX_BUFFER_SIZE = 32768,
                     MAX_INDEX_BUFFER_SIZE = 65536;

        static const char *DefaultVertexShader = R"(
#version 330

layout (location=0) in vec2 gPosition;
layout (location=1) in vec2 gTexcoord;
layout (location=2) in vec4 gColor;

out vec2 vTexcoord;
out vec4 vColor;

uniform mat4x4 Transform;

void main() {
    vec4 position = Transform * vec4(gPosition, 0.0, 1.0);
    gl_Position = position;
    vTexcoord = vec2(gTexcoord.x, gTexcoord.y);
    vColor = gColor;
}
)";
        static const char *DefaultFragmentShader = R"(
#version 330

in vec2 vTexcoord;
in vec4 vColor;

uniform sampler2D Texture;

layout (location=0) out vec4 pColor;

void main()
{
    pColor = vColor * texture(Texture, vTexcoord);
}
)";

    }
    struct NkContext::Impl
    {
        Context *ctx = nullptr;
        NkContextHandle nkCtx;
        NkFontAtlasHandle nkFontAtlas;
        NkBufferHandle nkCmdBuffer;
        nk_draw_null_texture nkNullTexture;


        TextureHandle fontTexture;
        PipelineHandle pipeline;

        VertexArrayHandle vertexArray;
        StreamingBuffer<nkVertex> vertexBuffer;
        StreamingBuffer<nkIndex> indexBuffer;

        size_t maxVertexCount = DEFAULT_VERTEX_BUFFER_SIZE,
               maxIndexCount = DEFAULT_INDEX_BUFFER_SIZE;

        Impl( Context *ctx_ ) :
            ctx(ctx_),
            vertexBuffer(ctx, BufferType::Vertex, DEFAULT_VERTEX_BUFFER_SIZE),
            indexBuffer(ctx, BufferType::Index, DEFAULT_INDEX_BUFFER_SIZE)
        {}
    };

    NkContext::NkContext( Context *ctx ) :
        mImpl(ctx)
    {
        mImpl->nkFontAtlas = NkFontAtlasHandle(new nk_font_atlas);
        nk_font_atlas_init_default(mImpl->nkFontAtlas);

        nk_font_atlas *atlas = mImpl->nkFontAtlas;
        nk_font_atlas_begin(atlas);
        nk_font *defaultFont = nk_font_atlas_add_default(atlas, 13.f, nullptr);

        int fontWidth, fontHeight;
        const void *img = nk_font_atlas_bake(atlas, &fontWidth, &fontHeight, NK_FONT_ATLAS_ALPHA8);

        HardwareResourceManager *hardwareMgr = mImpl->ctx->getHardwareResourceManager();
        mImpl->fontTexture = hardwareMgr->allocateTexture2D(PixelFormat::R8, TextureFlags::None, fontWidth, fontHeight);
        hardwareMgr->uploadTexture2D(mImpl->fontTexture, 0, TextureUploadFlags::GenerateMipmaps, PixelFormat::R8, img);
        hardwareMgr->setSwizzleMask(mImpl->fontTexture, SwizzleMask::Red, SwizzleMask::Red, SwizzleMask::Red, SwizzleMask::Red);

        nk_handle fontTexture;
            fontTexture.id = mImpl->fontTexture.handle;

        nk_font_atlas_end(atlas, fontTexture, &mImpl->nkNullTexture);

        mImpl->nkCtx = NkContextHandle(new nk_context);
        nk_init_default(nkContext(), &defaultFont->handle);

        mImpl->nkCmdBuffer = NkBufferHandle(new nk_buffer);
        nk_buffer_init_default(mImpl->nkCmdBuffer);

        PipelineManager *pipelineMgr = ctx->getPipelineManager();
        if (!pipelineMgr->findPipeline(Common::CreateStringId("Nuklear"), mImpl->pipeline)) {
            LOG_WARNING("Failed to find pipeline 'Nuklear' - trying to create one.");

            PipelineProgramInitParams params;
                params.blendMode = BlendMode::Alpha;
                params.flags = PipelineFlags::None;
                params.name = Common::CreateStringId("Nuklear");
                params.programParams.vertexSource = DefaultVertexShader;
                params.programParams.fragmentSource = DefaultFragmentShader;
                params.programParams.bindings.samplers[0] = "Texture";
                params.programParams.bindings.uniforms[0] = "Transform";

            mImpl->pipeline = pipelineMgr->createPipeline(params);
            if (!mImpl->pipeline) {
                THROW(std::runtime_error, "Failed create NkContext - failed to create pipeline \"Nuklear\"");
            }
        }
        
        mImpl->nkCtx->screen_size.x = (float)ctx->displayWidth();
        mImpl->nkCtx->screen_size.y = (float)ctx->displayHeight();        
    }

    NkContext::~NkContext()
    {
    }

    NkContext::operator nk_context* ()
    {
        return mImpl->nkCtx;
    }
    
    nk_context* NkContext::nkContext()
    {
        return mImpl->nkCtx;
    }

    void NkContext::beginFrame( float dt )
    {
        mImpl->nkCtx->delta_time_seconds = dt;
    }

    void NkContext::endFrame()
    {
        nk_clear(nkContext());
    }
    
    void NkContext::beginInput()
    {
        nk_input_begin(nkContext());
    }

    nk_buttons mouseButtonToNuklear( int button )
    {
        switch (button) {
        case SDL_BUTTON_LEFT:
            return NK_BUTTON_LEFT;
        case SDL_BUTTON_MIDDLE:
            return NK_BUTTON_MIDDLE;
        case SDL_BUTTON_RIGHT:
            return NK_BUTTON_RIGHT;
        default:
            return NK_BUTTON_MAX;
        }
    }

    bool NkContext::injectInputEvent( const SDL_Event &event, bool handled )
    {
        if (handled) return false;
        
        // is mouse visible, if not ignore all events
        if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE) return false;

        switch (event.type) {
        case SDL_MOUSEMOTION:
            nk_input_motion(nkContext(), event.motion.x, event.motion.y);
            return nk_window_is_any_hovered(nkContext()) == 1;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            nk_input_button(nkContext(), mouseButtonToNuklear(event.button.button), event.button.x, event.button.y, event.button.state == SDL_PRESSED ? 1 : 0);
            return nk_window_is_any_hovered(nkContext()) == 1;
        case SDL_MOUSEWHEEL:
            nk_input_scroll(nkContext(), nk_vec2((float)event.wheel.x, (float)event.wheel.y));
            return nk_window_is_any_hovered(nkContext()) == 1;
        }
        return false;
    }

    void NkContext::endInput()
    {
        nk_input_end(nkContext());
    }

    void NkContext::render()
    {
        nk_context *ctx = nkContext();

        nk_convert_config config = {};
            config.vertex_layout = nkVertexLayout;
            config.vertex_size = sizeof(nkVertex);
            config.vertex_alignment = alignof(nkVertex);
            config.circle_segment_count = 22;
            config.arc_segment_count = 22;
            config.global_alpha = 1.f;
            config.shape_AA = NK_ANTI_ALIASING_ON;
            config.line_AA = NK_ANTI_ALIASING_ON;
            config.null = mImpl->nkNullTexture;


        bool recreateVA = !mImpl->vertexArray;

        for (int tries=0; tries < 2; ++tries) {
            nk_buffer_clear(mImpl->nkCmdBuffer);

            nkVertex *vertexes = mImpl->vertexBuffer.mapBuffer(0, mImpl->maxVertexCount);
            nkIndex *indexes = mImpl->indexBuffer.mapBuffer(0, mImpl->maxIndexCount);

            assert(vertexes && indexes);

            nk_buffer vertexBuffer,
                      indexBuffer;

            nk_buffer_init_fixed(&vertexBuffer, vertexes, sizeof(nkVertex)*mImpl->maxVertexCount);
            nk_buffer_init_fixed(&indexBuffer, indexes, sizeof(nkIndex)*mImpl->maxIndexCount);

            nk_flags res = nk_convert(ctx, mImpl->nkCmdBuffer, &vertexBuffer, &indexBuffer, &config);

            mImpl->vertexBuffer.unmapBuffer();
            mImpl->indexBuffer.unmapBuffer();
            
            assert (res != NK_CONVERT_INVALID_PARAM);
            assert ((res&NK_CONVERT_COMMAND_BUFFER_FULL) == false);

            if (res == NK_CONVERT_SUCCESS) break;

            if (res & NK_CONVERT_VERTEX_BUFFER_FULL) {
                size_t newSize = (vertexBuffer.needed + vertexBuffer.needed / 2) / sizeof(nkVertex);
                newSize = std::min(newSize, MAX_VERTEX_BUFFER_SIZE);
                if (vertexBuffer.needed < newSize) {
                    mImpl->vertexBuffer.resize(newSize, StreamingBufferResizeFlags::None);
                    mImpl->maxVertexCount = newSize;
                    recreateVA = true;

                    LOG_INFORMATION("Nuklear vertex buffer size increased to %zu", newSize);
                }
                else {
                    // If this happends I need to investigate why, in the best case just increase MAX_VERTEX_BUFFER_SIZE
                    FATAL_ERROR("To big of a vertex buffer, max is %zu - needs %zu", MAX_VERTEX_BUFFER_SIZE, (size_t)vertexBuffer.needed);
                }
            }
            if (res & NK_CONVERT_ELEMENT_BUFFER_FULL) {
                size_t newSize = (indexBuffer.needed + indexBuffer.needed / 2) / sizeof(nkIndex);
                newSize = std::min(newSize, MAX_INDEX_BUFFER_SIZE);

                if (indexBuffer.needed < newSize) {
                    mImpl->indexBuffer.resize(newSize, StreamingBufferResizeFlags::None);
                    mImpl->maxIndexCount = newSize;
                    recreateVA = true;

                    LOG_INFORMATION("Nuklear index buffer size increased to %zu", newSize);
                }
                else {
                    // If this happends I need to investigate why, in the best case just increase MAX_INDEX_BUFFER_SIZE
                    FATAL_ERROR("To big of a vertex buffer, max is %zu - needs %zu", MAX_INDEX_BUFFER_SIZE, (size_t)vertexBuffer.needed);
                }
            }
        }

        if (recreateVA) {
            HardwareResourceManager *hardwareMgr = mImpl->ctx->getHardwareResourceManager();
            hardwareMgr->deleteVertexArray(mImpl->vertexArray);

            BufferHandle vertexBuffer = mImpl->vertexBuffer.handle();
            BufferHandle indexBuffer = mImpl->indexBuffer.handle();

            mImpl->vertexArray = hardwareMgr->createVertexArray(
                NK_VERTEX_LAYOUT, NK_VERTEX_LAYOUT_SIZE,
                &vertexBuffer, 1,
                indexBuffer, nkIndexType
            );
        }

        RenderCommandQueuePtr commandQueue = mImpl->ctx->createRenderCommandQueue();

        commandQueue->usePipeline(mImpl->pipeline);
        commandQueue->useVertexArray(mImpl->vertexArray);
        commandQueue->useClipping(true);

        glm::mat4 ortho = {
            2.0f, 0.0f, 0.0f, 0.0f,
            0.0f,-2.0f, 0.0f, 0.0f,
            0.0f, 0.0f,-1.0f, 0.0f,
           -1.0f, 1.0f, 0.0f, 1.0f,
        };

        struct nk_vec2 screenSize = ctx->screen_size;

        ortho[0][0] /= screenSize.x;
        ortho[1][1] /= screenSize.y;

        commandQueue->bindUniform(0, ortho);

        const nk_draw_command *cmd = nullptr;
        size_t base  = mImpl->vertexBuffer.currentFrameOffset(),
               first = mImpl->indexBuffer.currentFrameOffset();

        nk_draw_foreach(cmd, ctx, mImpl->nkCmdBuffer) {
            ClipRect rect;
                rect.x = (int)cmd->clip_rect.x;
                rect.y = (int)(screenSize.y - (cmd->clip_rect.y+cmd->clip_rect.h));
                rect.w = (int)cmd->clip_rect.w;
                rect.h = (int)cmd->clip_rect.h;

            TextureHandle texture(cmd->texture.id);

            commandQueue->bindTexture(0, texture);
            commandQueue->setClipRect(rect);
            commandQueue->draw(Primitive::Triangles, first, cmd->elem_count, base);

            first += cmd->elem_count;
        }

        mImpl->ctx->execute(commandQueue);
    }
}