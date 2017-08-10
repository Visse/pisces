#include "CompiledRenderQueue.h"
#include "RenderCommandQueue.h"

#include "Context.h"
#include "HardwareResourceManager.h"
#include "PipelineManager.h"

#include "internal/CompiledRenderQueueImpl.h"
#include "internal/RenderCommandQueueImpl.h"
#include "internal/HardwareResourceManagerImpl.h"
#include "internal/PipelineManagerImpl.h"
#include "internal/Helpers.h"
#include "internal/CommandQueueCompiler.h"

#include <glbinding/gl33core/enum.h>
#include <glbinding/gl33core/bitfield.h>
using namespace gl33core;

namespace Pisces
{
    using namespace CompiledRenderQueueImpl;
    namespace RenderQueue = RenderCommandQueueImpl;

    PISCES_API CompiledRenderQueue::CompiledRenderQueue( Context *context, const RenderCommandQueuePtr &queue,  const RenderQueueCompileOptions &options ) :
        mImpl(context)
    {
        mImpl->renderTarget = queue->impl()->renderTarget;
        mImpl->commands = Compile(context, options, queue);
    }

    PISCES_API CompiledRenderQueue::~CompiledRenderQueue()
    {
    }
}