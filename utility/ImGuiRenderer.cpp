#include "ImGuiRenderer.h"

#include "Context.h"
#include "HardwareResourceManager.h"
#include "PipelineManager.h"
#include "RenderCommandQueue.h"
#include "StreamingBuffer.h"

#include "Common/ErrorUtils.h"

#define PISCES_REGISTER_UNIFORM_REG
#include "UniformBlockInfo.h"

#include <imgui.h>

#include <glm/vec2.hpp>

using namespace Pisces;

static const int ImDrawVertLayoutSize = 3;
const VertexAttribute ImDrawVertLayout[ImDrawVertLayoutSize] = {
    {VertexAttributeType::Float32, offsetof(ImDrawVert, pos), 2, sizeof(ImDrawVert), 0},
    {VertexAttributeType::Float32, offsetof(ImDrawVert, uv), 2, sizeof(ImDrawVert), 0},
    {VertexAttributeType::NormUInt8, offsetof(ImDrawVert, col), 4, sizeof(ImDrawVert), 0}
};
static const IndexType ImDrawIdxType = IndexType::UInt16;
static_assert( sizeof(ImDrawIdx) == 2, "ImDrawIdx is of unexpected size!" );

struct ImGuiUniformBlock {
    glm::vec2 screenSize;
    float _padding[2];
};

REGISTER_UNIFORM_BLOCK( ImGuiUniformBlock,
    screenSize
);

using ImVertexBuffer = StreamingBuffer<ImDrawVert>;
using ImIndexBuffer = StreamingBuffer<ImDrawIdx>;

using ImVertexBufferPtr = std::unique_ptr<ImVertexBuffer>;
using ImIndexBufferPtr = std::unique_ptr<ImIndexBuffer>;

struct ImGuiRenderer::Impl {
    Context *context;

    PipelineHandle pipeline;

    TextureHandle texture;

    ImVertexBufferPtr vertexBuffer;
    ImIndexBufferPtr  indexBuffer;

    VertexArrayHandle vertexArray;

    UniformBufferHandle imguiUniform;

    size_t maxVertexes = 0,
           maxIndexes = 0;

    CompiledRenderQueuePtr renderQueue;

    Impl( Context *context_ ) :
        context(context_)
    {}

    void onDisplayResized( int width, int height )
    {
        ImGuiUniformBlock uniforms;
            uniforms.screenSize.x = (float)width;
            uniforms.screenSize.y = (float)height;

        HardwareResourceManager *hardwareMgr = context->getHardwareResourceManager();
        hardwareMgr->updateStaticUniform(imguiUniform, uniforms);

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

    if (pipelineMgr->findPipeline(Common::CreateStringId("ImGui"), mImpl->pipeline)) {
        LOG_ERROR("Failed create ImGuiRenderer - failed to find pipeline \"ImGui\"");
    }

    ImGuiUniformBlock imguiUniform;
        imguiUniform.screenSize = {displayWidth, displayHeight};
        
    mImpl->imguiUniform = hardwareMgr->allocateStaticUniform(imguiUniform);

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

    RenderCommandQueuePtr renderQueue = impl->context->createRenderCommandQueue();
    renderQueue->usePipeline(impl->pipeline);
    renderQueue->useVertexArray(impl->vertexArray);
    renderQueue->bindUniform(0, impl->imguiUniform);

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
