#pragma once

#include "Fwd.h"

#include "Common/PImplHelper.h"

namespace Pisces
{
    namespace CompiledRenderQueueImpl {
        struct Impl;
    }
    class CompiledRenderQueue {
    public:
        PISCES_API CompiledRenderQueue( Context *context, const RenderCommandQueuePtr &queue, const RenderQueueCompileOptions &options );
        PISCES_API ~CompiledRenderQueue();

        CompiledRenderQueue( const CompiledRenderQueue& ) = delete;
        CompiledRenderQueue& operator = ( const CompiledRenderQueue& ) = delete;


        CompiledRenderQueueImpl::Impl* impl() {
            return mImpl.impl();
        }

    private:
        PImplHelper<CompiledRenderQueueImpl::Impl, 128> mImpl;
    };
}