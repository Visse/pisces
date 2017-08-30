#include "ImGuiRenderer.h"

#include "Context.h"
#include "HardwareResourceManager.h"
#include "PipelineManager.h"
#include "RenderCommandQueue.h"
#include "StreamingBuffer.h"

#include "Common/ErrorUtils.h"
#include "Common/Throw.h"

#define PISCES_REGISTER_UNIFORM_REG
#include "UniformBlockInfo.h"

#include <imgui.h>
#include <SDL_events.h>

#include <glm/vec2.hpp>

namespace Pisces
{
    static const int ImDrawVertLayoutSize = 3;
    static const VertexAttribute ImDrawVertLayout[ImDrawVertLayoutSize] = {
        {VertexAttributeType::Float32, offsetof(ImDrawVert, pos), 2, sizeof(ImDrawVert), 0},
        {VertexAttributeType::Float32, offsetof(ImDrawVert, uv), 2, sizeof(ImDrawVert), 0},
        {VertexAttributeType::NormUInt8, offsetof(ImDrawVert, col), 4, sizeof(ImDrawVert), 0}
    };
    static const IndexType ImDrawIdxType = IndexType::UInt16;
    static_assert( sizeof(ImDrawIdx) == 2, "ImDrawIdx is of unexpected size!" );

    static const char *DefaultVertexShader = R"(
#version 330 core

layout(location=0) in vec2 gPosition;
layout(location=1) in vec2 gTexcoord;
layout(location=2) in vec4 gColor;

out vec2 vTexcoord;
out vec4 vColor;

uniform vec2 ScreenSize;

void main()
{
    mat4 ortho = mat4(
        2.0/ScreenSize.x, 0.0, 0.0, 0.0,
        0.0,-2.0/ScreenSize.y, 0.0, 0.0,
        0.0, 0.0,-1.0, 0.0,
       -1.0, 1.0, 0.0, 1.0   
    );
    
    gl_Position = ortho*vec4(gPosition,0.0,1.0);
    vTexcoord = gTexcoord;
    vColor = gColor;
}
)";
    static const char *DefaultFragmentShader = R"(
#version 330 core

in vec2 vTexcoord;
in vec4 vColor;

layout(location=0) out vec4 color;

uniform sampler2D Texture;

void main()
{
    color = vColor * texture(Texture, vTexcoord);
}
)";
    using ImVertexBuffer = StreamingBuffer<ImDrawVert>;
    using ImIndexBuffer = StreamingBuffer<ImDrawIdx>;

    using ImVertexBufferPtr = std::unique_ptr<ImVertexBuffer>;
    using ImIndexBufferPtr = std::unique_ptr<ImIndexBuffer>;

    struct ImGuiRenderer::Impl {
        Context *context;

        PipelineHandle pipeline;

        TextureHandle texture;

        glm::vec2 screenSize;

        ImVertexBufferPtr vertexBuffer;
        ImIndexBufferPtr  indexBuffer;

        VertexArrayHandle vertexArray;

        size_t maxVertexes = 0,
               maxIndexes = 0;

        CompiledRenderQueuePtr renderQueue;

        Impl( Context *context_ ) :
            context(context_)
        {}

        void onDisplayResized( int width, int height )
        {
            screenSize = glm::vec2(width, height);

            auto &io = ImGui::GetIO();
                io.DisplaySize.x = (float)width;
                io.DisplaySize.y = (float)height;
        }
    };

    PISCES_API ImGuiRenderer::ImGuiRenderer( Pisces::Context *context ) :
        mImpl(context)
    {
        HardwareResourceManager *hardwareMgr = context->getHardwareResourceManager();
        PipelineManager *pipelineMgr = context->getPipelineManager();

        float displayWidth = (float)context->displayWidth(),
              displayHeight = (float)context->displayHeight();

        auto &io = ImGui::GetIO();

        io.DisplaySize.x = displayWidth;
        io.DisplaySize.y = displayHeight;
        io.RenderDrawListsFn = []( ImDrawData* )
        {
            auto &io = ImGui::GetIO();
            ImGuiRenderer *this_ = (ImGuiRenderer*) io.UserData;

            imguiRender(this_->mImpl.impl());
        };
        io.UserData = this;

        unsigned char *pixels;
        int width, height;
        io.Fonts->GetTexDataAsAlpha8( &pixels, &width, &height );


        mImpl->texture = hardwareMgr->allocateTexture2D(PixelFormat::R8, TextureFlags::None, width, height );
        hardwareMgr->uploadTexture2D(mImpl->texture, 0, TextureUploadFlags::GenerateMipmaps, PixelFormat::R8, pixels);
        hardwareMgr->setSwizzleMask(mImpl->texture, SwizzleMask::Red, SwizzleMask::Red, SwizzleMask::Red, SwizzleMask::Red);

        SamplerParams params;
            params.magFilter = SamplerMagFilter::Linear;
            params.minFilter = SamplerMinFilter::LinearMipmapLinear;
        hardwareMgr->setSamplerParams(mImpl->texture, params);

        io.Fonts->SetTexID((void*)(uintptr_t)mImpl->texture);

        if (!pipelineMgr->findPipeline(Common::CreateStringId("ImGui"), mImpl->pipeline)) {
            LOG_WARNING("Failed to find pipeline 'ImGui' - trying to create one.");

            PipelineProgramInitParams params;
                params.blendMode = BlendMode::Alpha;
                params.flags = PipelineFlags::None;
                params.name = Common::CreateStringId("ImGui");
                params.programParams.vertexSource = DefaultVertexShader;
                params.programParams.fragmentSource = DefaultFragmentShader;
                params.programParams.bindings.samplers[0] = "Texture";
                params.programParams.bindings.uniforms[0] = "ScreenSize";

            mImpl->pipeline = pipelineMgr->createPipeline(params); 
            if (!mImpl->pipeline) {
                THROW(std::runtime_error, "Failed create ImGuiRenderer - failed to create pipeline \"ImGui\"");
            }
        }

        mImpl->screenSize = {displayWidth, displayHeight};

        mImpl->context->addDisplayResizedCallback( [this](int width, int height){mImpl->onDisplayResized(width, height);}, this);
    }

    PISCES_API ImGuiRenderer::~ImGuiRenderer()
    {
        mImpl->context->removeCallback(this);
    }

    PISCES_API void ImGuiRenderer::render()
    {
        if (mImpl->renderQueue) {
            mImpl->context->execute(mImpl->renderQueue);
        }
    }

    int SDLButtonToImGui( int button ) {
        switch( button ) {
        case( SDL_BUTTON_LEFT ): return 0;
        case( SDL_BUTTON_RIGHT ): return 1;
        case( SDL_BUTTON_MIDDLE ): return 2;
        case( SDL_BUTTON_X1 ): return 3;
        case( SDL_BUTTON_X2 ): return 4;
        default:
            return -1;
        }
    }

    PISCES_API bool ImGuiRenderer::injectEvent( const SDL_Event &event, bool handled )
    {
        if (handled) return false;

        // is mouse visible, if not ignore all events
        if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE || SDL_GetRelativeMouseMode()) return false;

        auto &io = ImGui::GetIO();
        switch( event.type ) {
        case( SDL_MOUSEBUTTONDOWN ):
        case( SDL_MOUSEBUTTONUP ): {
            int idx = SDLButtonToImGui( event.button.button );
            if( idx >= 0 && idx <= 5 ) {
                io.MouseDown[idx] = (event.type == SDL_MOUSEBUTTONDOWN);
            }
            io.MousePos.x = (float) event.button.x;
            io.MousePos.y = (float) event.button.y;
        } return io.WantCaptureMouse;
        case( SDL_MOUSEWHEEL ):
            io.MouseWheel = (float) event.wheel.y;
            return io.WantCaptureMouse;
        case( SDL_MOUSEMOTION ):
            io.MousePos.x = (float) event.motion.x;
            io.MousePos.y = (float) event.motion.y;
            return io.WantCaptureMouse;
        case( SDL_KEYDOWN ):
        case( SDL_KEYUP ): {
            int idx = event.key.keysym.scancode;

            if( idx >= 0 && idx < 512 ) {
                io.KeysDown[idx] = (event.type == SDL_KEYDOWN);
            }

            io.KeyAlt   = (event.key.keysym.mod & KMOD_ALT)   != 0;
            io.KeyCtrl  = (event.key.keysym.mod & KMOD_CTRL)  != 0;
            io.KeyShift = (event.key.keysym.mod & KMOD_SHIFT) != 0;
            io.KeySuper = (event.key.keysym.mod & KMOD_GUI)   != 0;

        } return io.WantCaptureKeyboard;
        case( SDL_TEXTINPUT ):
            io.AddInputCharactersUTF8( event.text.text );
            return io.WantCaptureKeyboard;
        }

        return false;
    }

    void imguiRender( ImGuiRenderer::Impl *impl )
    {
        auto &io = ImGui::GetIO();
        ImGuiRenderer *renderer = (ImGuiRenderer*)io.UserData;

        const ImDrawData *drawData = ImGui::GetDrawData();
        assert(drawData);
        assert(drawData->Valid);

        if (drawData->CmdListsCount == 0) return;

        HardwareResourceManager *hardwareMgr = impl->context->getHardwareResourceManager();

        bool recreateVertexArray = false;
        if (impl->maxVertexes < (size_t)drawData->TotalVtxCount) {
            size_t newSize = drawData->TotalVtxCount * 2;

            impl->vertexBuffer.reset( new ImVertexBuffer(impl->context, BufferType::Vertex, newSize) );
            recreateVertexArray = true;
            impl->maxVertexes = newSize;
        }
        if (impl->maxIndexes < (size_t)drawData->TotalIdxCount) {
            size_t newSize = drawData->TotalIdxCount * 2;

            impl->indexBuffer.reset( new ImIndexBuffer(impl->context, BufferType::Index, newSize));
            recreateVertexArray = true;
            impl->maxIndexes = newSize;
        }

        if (recreateVertexArray) {
            BufferHandle vertexBuffer = impl->vertexBuffer->handle();
            BufferHandle indexBuffer = impl->indexBuffer->handle();

            hardwareMgr->deleteVertexArray(impl->vertexArray);
            impl->vertexArray = hardwareMgr->createVertexArray(
                ImDrawVertLayout, ImDrawVertLayoutSize,
                &vertexBuffer, 1,
                indexBuffer, ImDrawIdxType
            );
        }

        size_t firstVertex = impl->vertexBuffer->currentFrameOffset();
        size_t firstIndex = impl->indexBuffer->currentFrameOffset();

        ImDrawVert *vertexes = impl->vertexBuffer->mapBuffer(0, drawData->TotalVtxCount);
        ImDrawIdx *indexes = impl->indexBuffer->mapBuffer(0, drawData->TotalIdxCount);
        assert(vertexes && indexes);

        RenderCommandQueuePtr renderQueue = impl->context->createRenderCommandQueue();
        renderQueue->usePipeline(impl->pipeline);
        renderQueue->useVertexArray(impl->vertexArray);
        renderQueue->bindUniform(0, impl->screenSize);

        float displayHeight = ImGui::GetIO().DisplaySize.y;

        for (int i=0, size=drawData->CmdListsCount; i < size; ++i) {
            size_t baseVertex = firstVertex;
            size_t baseIndex = firstIndex;

            const ImDrawList *list = drawData->CmdLists[i];
            std::memcpy(vertexes, list->VtxBuffer.Data, list->VtxBuffer.size()*sizeof(ImDrawVert));
            std::memcpy(indexes, list->IdxBuffer.Data, list->IdxBuffer.size()*sizeof(ImDrawIdx));
            firstVertex += list->VtxBuffer.Size;
            firstIndex += list->IdxBuffer.Size;
            vertexes += list->VtxBuffer.Size;
            indexes += list->IdxBuffer.Size;

            size_t first = 0;
            for (const auto &cmd : list->CmdBuffer) {
                size_t count = cmd.ElemCount;
                TextureHandle texture( (uint32_t)(uintptr_t)cmd.TextureId );
                renderQueue->bindTexture(0, texture);

                ClipRect clipRect;
                    clipRect.x = (int)cmd.ClipRect.x;
                    clipRect.y = (int)(displayHeight-cmd.ClipRect.w);
                    clipRect.w = (int)(cmd.ClipRect.z - cmd.ClipRect.x);
                    clipRect.h = (int)(cmd.ClipRect.w - cmd.ClipRect.y);
                renderQueue->setClipRect(clipRect);

                renderQueue->draw(Primitive::Triangles, first+baseIndex, count, baseVertex);
                first += count;
            }
        }

        impl->vertexBuffer->unmapBuffer();
        impl->indexBuffer->unmapBuffer();

        impl->renderQueue = impl->context->compile(renderQueue);
    }
}