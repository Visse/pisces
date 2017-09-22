#pragma once

#include "Fwd.h"
#include "build_config.h"
#include "IResourceLoader.h"

#include "Common/PImplHelper.h"
#include "Common/Archive.h"

namespace Pisces
{
    class PipelineLoader :
        public IResourceLoader
    {
    public:
        PISCES_API PipelineLoader( PipelineManager *pipelineMgr );
        PISCES_API ~PipelineLoader();
        
        PISCES_API virtual ResourceHandle loadResource( Common::Archive &archive, Common::YamlNode node ) override;

    private:
        struct Impl;
        PImplHelper<Impl, 8> mImpl;
    };
}