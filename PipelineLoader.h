#pragma once

#include "Fwd.h"
#include "build_config.h"

#include "Common/PImplHelper.h"
#include "Common/Archive.h"

namespace Pisces
{
    class PipelineLoader
    {
    public:
        PISCES_API PipelineLoader( PipelineManager *pipelineMgr );
        PISCES_API ~PipelineLoader();
        
        PISCES_API PipelineHandle loadFile( Common::Archive &archive, const std::string &filename );

    private:
        struct Impl;
        PImplHelper<Impl, 8> mImpl;
    };
}