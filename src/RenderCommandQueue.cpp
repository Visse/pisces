#include "RenderCommandQueue.h"
#include "internal/RenderCommandQueueImpl.h"

#include "Common/ErrorUtils.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Pisces
{
    using namespace RenderCommandQueueImpl;

    RenderCommandQueue::RenderCommandQueue( Context *context, RenderTargetHandle renderTarget, RenderCommandQueueFlags flags ) :
        mImpl(context, renderTarget, flags)
    {
    }

    RenderCommandQueue::~RenderCommandQueue()
    {
    }

    void RenderCommandQueue::usePipeline( PipelineHandle pipeline )
    {
        mImpl->commands.emplace_back(UsePipeline(pipeline));
    }

    void RenderCommandQueue::useVertexArray( VertexArrayHandle vertexArray )
    {
        mImpl->commands.emplace_back(UseVertexArray(vertexArray));
    }

    void RenderCommandQueue::bindTexture( int slot, TextureHandle texture )
    {
        if (slot < 0 || slot >= MAX_BOUND_SAMPLERS) {
            LOG_WARNING("Trying to bind a texture %i to the invalid slot %i!", (int)texture, slot);
            return;
        }
        mImpl->commands.emplace_back(BindSampler(slot, texture));
    }

    void RenderCommandQueue::bindUniform( int slot, UniformBufferHandle uniform )
    {
        if (slot < 0 || slot >= MAX_BOUND_UNIFORM_BUFFERS) {
            LOG_WARNING("Trying to bind a uniform buffer to the invalid slot %i!", slot);
            return;
        }
        mImpl->commands.emplace_back(BindUniformBuffer(slot, uniform));
    }

    void RenderCommandQueue::useClipping( bool use )
    {
        mImpl->commands.emplace_back(UseClipping(use));
    }

    void RenderCommandQueue::setClipRect( ClipRect rect )
    {
        mImpl->commands.emplace_back(UseClipRect(rect));
    }

    void RenderCommandQueue::bindImageTexture( int slot, TextureHandle texture, ImageTextureAccess access, PixelFormat format )
    {
        if (slot < 0 || slot >= MAX_BOUND_IMAGE_TEXTURES) {
            LOG_WARNING("Trying to bind a image texture to the invalid slot %i!", slot);
            return;
        }

        mImpl->commands.emplace_back(BindImageTexture(slot, texture, access, format));
    }

    void RenderCommandQueue::executeComputeProgram( ComputeProgramHandle program, glm::uvec3 count )
    {
        mImpl->commands.emplace_back(ExecuteCompute(program, {count.x,count.y,count.z}));
    }

    void RenderCommandQueue::draw( Primitive primitive, size_t first, size_t count, size_t base )
    {
        mImpl->commands.emplace_back(Draw(primitive, first, count, base));
    }

    void RenderCommandQueue::clear( ClearFlags flags, Color color, float depth, int stencil )
    {
        ClearData data;
            data.flags = flags;
            data.color = color;
            data.depth = depth;
            data.stencil = stencil;

        mImpl->commands.emplace_back(data);
    }

    void RenderCommandQueue::bindUniform( int location, glm::mat4 matrix )
    {
        if (location < 0 || location >= MAX_BOUND_UNIFORMS) {
            LOG_WARNING("Trying to bind uniform to invalid location %i", location);
            return;
        }

        BindUniformMat4Data data;
        data.location = location;
        for (int i=0; i < 4*4; ++i) {
            data.matrix[i] = matrix[i/4][i%4];
        }

        mImpl->commands.emplace_back(data);
    }

    void RenderCommandQueue::drawBuiltin( BuiltinObject object )
    {
        mImpl->commands.emplace_back(DrawBuiltin(object));
    }
}