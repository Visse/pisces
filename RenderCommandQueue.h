#pragma once


#include "Fwd.h"

#include "Common/PImplHelper.h"

#include <glm/fwd.hpp>

namespace Pisces
{
    namespace RenderCommandQueueImpl {
        struct Impl;
    }

    class RenderCommandQueue {
    public:
        RenderCommandQueue( Context *ctxcontext, RenderTargetHandle renderTarget, RenderCommandQueueFlags flags );
        ~RenderCommandQueue();

        RenderCommandQueue( const RenderCommandQueue& ) = delete;
        RenderCommandQueue& operator = ( const RenderCommandQueue& ) = delete;
    
    public:
        void usePipeline( PipelineHandle pipeline );
        void useVertexArray( VertexArrayHandle vertexArray );
        void bindTexture( int slot, TextureHandle texture );
        void bindUniform( int slot, UniformBufferHandle uniform );
        void useClipping( bool use );
        void setClipRect( ClipRect rect );

        void bindImageTexture( int slot, TextureHandle texture, ImageTextureAccess access, PixelFormat format );
        void executeComputeProgram( ComputeProgramHandle program, glm::uvec3 count );

        // base is only used together with a index array
        void draw( Primitive primitive, size_t first, size_t count, size_t base=0 );
        void clear( ClearFlags flags, Color color=NamedColors::Black, float depth=1.0f, int stencil=0 );

        void bindUniform( int location, glm::mat4 matrix );

        void drawBuiltin( BuiltinObject object );


    private:
        PImplHelper<RenderCommandQueueImpl::Impl,128> mImpl;

    public:
        RenderCommandQueueImpl::Impl* impl() {
            return mImpl.impl();
        }
    };
}