#pragma once

#include "Fwd.h"
#include "build_config.h"
#include "IResourceLoader.h"

#include "Common/PImplHelper.h"
#include "Common/Archive.h"

namespace Pisces
{
    class RenderProgramLoader;

    class PipelineLoader :
        public IResourceLoader
    {
    public:
        PISCES_API PipelineLoader( PipelineManager *pipelineMgr, RenderProgramLoader *renderProgramLoader );
        PISCES_API ~PipelineLoader();
        
        PISCES_API virtual ResourceHandle loadResource( Common::Archive &archive, libyaml::Node node ) override;

    private:
        struct Impl;
        PImplHelper<Impl, 16> mImpl;
    };
}