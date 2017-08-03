#pragma once

#include "Fwd.h"
#include "CompiledRenderQueueImpl.h"

namespace Pisces
{
    std::vector<CompiledRenderQueueImpl::Command> Compile( Context *context, const RenderQueueCompileOptions &options, const RenderCommandQueuePtr &queue );
}