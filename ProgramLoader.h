#pragma once

#include "Fwd.h"
#include "build_config.h"
#include "IResourceLoader.h"

#include "Common/PImplHelper.h"
#include "Common/Archive.h"
#include "Common/StringId.h"
namespace Pisces
{
    class RenderProgramLoader :
        public IResourceLoader
    {
        PipelineManager *mPipelineMgr;
    public:
        RenderProgramLoader( PipelineManager *pipelineMgr ) :
            mPipelineMgr(pipelineMgr)
        {}
        
        PISCES_API virtual ResourceHandle loadResource( Common::Archive &archive, libyaml::Node node ) override;

        ProgramHandle loadProgram( Common::Archive &archive, libyaml::Node node, Common::StringId defaultName = Common::StringId() );
    };

    class ComputeProgramLoader :
        public IResourceLoader
    {
        PipelineManager *mPipelineMgr;
    public:
        ComputeProgramLoader( PipelineManager *pipelineMgr ) :
            mPipelineMgr(pipelineMgr)
        {}
        
        PISCES_API virtual ResourceHandle loadResource( Common::Archive &archive, libyaml::Node node ) override;
    };

    class TransformProgramLoader :
        public IResourceLoader
    {
        PipelineManager *mPipelineMgr;
    public:
        TransformProgramLoader( PipelineManager *pipelineMgr ) :
            mPipelineMgr(pipelineMgr)
        {}
        
        PISCES_API virtual ResourceHandle loadResource( Common::Archive &archive, libyaml::Node node ) override;
    };
}