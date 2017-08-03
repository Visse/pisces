#pragma once

#include "Fwd.h"

#include "Common/PImplHelper.h"
#include "Common/StringId.h"

#include <string>

namespace Pisces
{
    namespace PipelineManagerImpl {
        struct Impl;
    }

    struct ProgramInitBindings {
        std::string uniforms[MAX_BOUND_UNIFORMS];
        std::string uniformBuffers[MAX_BOUND_UNIFORM_BUFFERS];
        std::string samplers[MAX_BOUND_SAMPLERS];
        std::string imageTextures[MAX_BOUND_IMAGE_TEXTURES];
    };

    struct ComputeProgramInitParams {
        ComputeProgramFlags flags = ComputeProgramFlags::None;
        
        std::string source;

        ProgramInitBindings bindings;
    };
    struct RenderProgramInitParams {
        RenderProgramFlags flags = RenderProgramFlags::None;

        // If true vertexSource & fragmentSource holds the filename of the source
        bool sourceIsFilename = false;
        std::string vertexSource,
                    fragmentSource;

        ProgramInitBindings bindings;
    };

    struct PipelineInitParams {
        ProgramHandle program;

        BlendMode blendMode = BlendMode::Replace;
        PipelineFlags flags = PipelineFlags::None;

        Common::StringId name;
    };

    struct PipelineProgramInitParams {
        RenderProgramInitParams programParams;
        
        BlendMode blendMode = BlendMode::Replace;
        PipelineFlags flags = PipelineFlags::None;

        Common::StringId name;
    };

    class PipelineManager {
    public:
        PipelineManager( Context *context );
        ~PipelineManager();
        
        ProgramHandle createRenderProgram( const RenderProgramInitParams &params );
        void destroyProgram( ProgramHandle handle );

        ComputeProgramHandle createComputeProgram( const ComputeProgramInitParams &params );
        void destroyProgram( ComputeProgramHandle handle );

        PipelineHandle createPipeline( const PipelineInitParams &params );
        PipelineHandle createPipeline( const PipelineProgramInitParams &params );
        void destroyPipeline( PipelineHandle handle );

        bool findPipeline( Common::StringId name, PipelineHandle &pipeline );

        PipelineManagerImpl::Impl* impl() {
            return mImpl.impl();
        }
    private:
        PImplHelper<PipelineManagerImpl::Impl,1024> mImpl;
    };
}