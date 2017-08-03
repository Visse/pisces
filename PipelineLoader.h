#pragma once

#include "Fwd.h"

#include "Common/PImplHelper.h"
#include "Common/Archive.h"

namespace Pisces
{
    class PipelineLoader
    {
    public:
        PipelineLoader( PipelineManager *pipelineMgr );
        virtual ~PipelineLoader();
        
        PipelineHandle loadFile( Common::Archive &archive, const std::string &filename );

    private:
        struct Impl;
        PImplHelper<Impl, 8> mImpl;
    };
}