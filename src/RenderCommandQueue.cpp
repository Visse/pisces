#include "RenderCommandQueue.h"
#include "internal/RenderCommandQueueImpl.h"

#include "Common/ErrorUtils.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace Pisces
{
    using namespace RenderCommandQueueImpl;

    PISCES_API RenderCommandQueue::RenderCommandQueue( Context *context, RenderTargetHandle renderTarget, RenderCommandQueueFlags flags ) :
        mImpl(context, renderTarget, flags)
    {
    }

    PISCES_API RenderCommandQueue::~RenderCommandQueue()
    {
    }

    PISCES_API void RenderCommandQueue::usePipeline( PipelineHandle pipeline )
    {
        mImpl->commands.emplace_back(UsePipeline(pipeline));
    }

    PISCES_API void RenderCommandQueue::useVertexArray( VertexArrayHandle vertexArray )
    {
        mImpl->commands.emplace_back(UseVertexArray(vertexArray));
    }

    PISCES_API void RenderCommandQueue::bindTexture( int slot, TextureHandle texture )
    {
        if (slot < 0 || slot >= MAX_BOUND_SAMPLERS) {
            LOG_WARNING("Trying to bind a texture %i to the invalid slot %i!", (int)texture, slot);
            return;
        }
        mImpl->commands.emplace_back(BindSampler(slot, texture));
    }

    PISCES_API void RenderCommandQueue::bindUniformBuffer( int slot, UniformBufferHandle uniform )
    {
        if (slot < 0 || slot >= MAX_BOUND_UNIFORM_BUFFERS) {
            LOG_WARNING("Trying to bind a uniform buffer to the invalid slot %i!", slot);
            return;
        }
        mImpl->commands.emplace_back(BindUniformBuffer(slot, uniform));
    }

    PISCES_API void RenderCommandQueue::useClipping( bool use )
    {
        mImpl->commands.emplace_back(UseClipping(use));
    }

    PISCES_API void RenderCommandQueue::setClipRect( ClipRect rect )
    {
        mImpl->commands.emplace_back(UseClipRect(rect));
    }

    PISCES_API void RenderCommandQueue::bindImageTexture( int slot, TextureHandle texture, ImageTextureAccess access, PixelFormat format )
    {
        if (slot < 0 || slot >= MAX_BOUND_IMAGE_TEXTURES) {
            LOG_WARNING("Trying to bind a image texture to the invalid slot %i!", slot);
            return;
        }

        mImpl->commands.emplace_back(BindImageTexture(slot, texture, access, format));
    }

    PISCES_API void RenderCommandQueue::executeComputeProgram( ComputeProgramHandle program, glm::uvec3 count )
    {
        mImpl->commands.emplace_back(ExecuteCompute(program, {count.x,count.y,count.z}));
    }

    PISCES_API void RenderCommandQueue::draw( Primitive primitive, size_t first, size_t count, size_t base )
    {
        mImpl->commands.emplace_back(Draw(primitive, first, count, base));
    }

    PISCES_API void RenderCommandQueue::clear( ClearFlags flags, Color color, float depth, int stencil )
    {
        ClearData data;
            data.flags = flags;
            data.color = color;
            data.depth = depth;
            data.stencil = stencil;

        mImpl->commands.emplace_back(data);
    }

    PISCES_API void RenderCommandQueue::bindUniform( int location, glm::mat4 matrix )
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

    PISCES_API void RenderCommandQueue::bindUniform(int location, glm::vec2 vec)
    {
        if (location < 0 || location >= MAX_BOUND_UNIFORMS) {
            LOG_WARNING("Trying to bind uniform to invalid location %i", location);
            return;
        }

        vec2 uniform;
            uniform[0] = vec[0];
            uniform[1] = vec[1];
        
        mImpl->commands.emplace_back(BindUniformVec2(location, uniform));
    }

    PISCES_API void RenderCommandQueue::bindUniform(int location, glm::vec3 vec)
    {
        if (location < 0 || location >= MAX_BOUND_UNIFORMS) {
            LOG_WARNING("Trying to bind uniform to invalid location %i", location);
            return;
        }

        vec3 uniform;
            uniform[0] = vec[0];
            uniform[1] = vec[1];
            uniform[2] = vec[2];
        
        mImpl->commands.emplace_back(BindUniformVec3(location, uniform));
    }

    PISCES_API void RenderCommandQueue::bindUniform(int location, glm::vec4 vec)
    {
        if (location < 0 || location >= MAX_BOUND_UNIFORMS) {
            LOG_WARNING("Trying to bind uniform to invalid location %i", location);
            return;
        }

        vec4 uniform;
            uniform[0] = vec[0];
            uniform[1] = vec[1];
            uniform[2] = vec[2];
            uniform[3] = vec[3];
        
        mImpl->commands.emplace_back(BindUniformVec4(location, uniform));
    }

    PISCES_API void RenderCommandQueue::drawBuiltin( BuiltinObject object )
    {
        mImpl->commands.emplace_back(DrawBuiltin(object));
    }
}