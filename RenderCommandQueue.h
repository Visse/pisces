#pragma once


#include "Fwd.h"
#include "build_config.h"

#include "Common/PImplHelper.h"

#include <glm/fwd.hpp>

namespace Pisces
{
    namespace RenderCommandQueueImpl {
        struct Impl;
    }

    class RenderCommandQueue {
    public:
        PISCES_API RenderCommandQueue( Context *ctxcontext, RenderTargetHandle renderTarget, RenderCommandQueueFlags flags );
        PISCES_API ~RenderCommandQueue();

        PISCES_API RenderCommandQueue( const RenderCommandQueue& ) = delete;
        PISCES_API RenderCommandQueue& operator = ( const RenderCommandQueue& ) = delete;
    
    public:
        PISCES_API void usePipeline( PipelineHandle pipeline );
        PISCES_API void useVertexArray( VertexArrayHandle vertexArray );
        PISCES_API void bindTexture( int slot, TextureHandle texture );
        PISCES_API void bindTexture( int slot, BuiltinTexture texture );
        PISCES_API void bindUniformBuffer( int slot, UniformBufferHandle uniform );
        PISCES_API void useClipping( bool use );
        PISCES_API void setClipRect( ClipRect rect );

        PISCES_API void bindImageTexture( int slot, TextureHandle texture, ImageTextureAccess access, PixelFormat format );
        PISCES_API void executeComputeProgram( ComputeProgramHandle program, glm::uvec3 count );

        PISCES_API void beginTransformFeedback( TransformProgramHandle program, Primitive primitive, 
                                                BufferHandle buffer, size_t offset, size_t size );
        PISCES_API void endTransformFeedback();

        // base is only used together with a index array
        PISCES_API void draw( Primitive primitive, size_t first, size_t count, size_t base=0 );
        PISCES_API void clear( ClearFlags flags, Color color=NamedColors::Black, float depth=1.0f, int stencil=0 );

        PISCES_API void bindUniform( int location, glm::mat4 matrix );
        PISCES_API void bindUniform( int location, int value );
        PISCES_API void bindUniform( int location, unsigned int value );
        PISCES_API void bindUniform( int location, float value );
        PISCES_API void bindUniform( int location, glm::vec2 vec );
        PISCES_API void bindUniform( int location, glm::vec3 vec );
        PISCES_API void bindUniform( int location, glm::vec4 vec );

        PISCES_API void drawBuiltin( BuiltinObject object );

        PISCES_API void copyBuffer( BufferHandle target, size_t targetOffset, BufferHandle source, size_t sourceOffset, size_t size );


    private:
        PImplHelper<RenderCommandQueueImpl::Impl,128> mImpl;

    public:
        RenderCommandQueueImpl::Impl* impl() {
            return mImpl.impl();
        }
    };
}