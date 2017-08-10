#include "PipelineManager.h"

#include "UniformBlockInfo.h"

#include "internal/PipelineManagerImpl.h"
#include "PipelineLoader.h"

#include "Common/FileUtils.h"
#include "Common/Throw.h"
#include "Common/ErrorUtils.h"
#include "Common/StringEqual.h"

#include <glbinding/ContextInfo.h>

#include <glbinding/gl33core/gl.h>
using namespace gl33core;

#include <stdexcept>

namespace Pisces
{
    using namespace PipelineManagerImpl;

    PISCES_API PipelineManager::PipelineManager( Context *context ) :
        mImpl(context)
    {
        mImpl->supportsComputeShaders = glbinding::ContextInfo::supported({GLextension::GL_ARB_compute_shader});
        if (mImpl->supportsComputeShaders) {
            LOG_INFORMATION("GL supports compute shaders!");
        }
        else {
            LOG_WARNING("GL doesn't support compute shaders!");
        }
    }

    PISCES_API PipelineManager::~PipelineManager()
    {}
  
    PISCES_API ProgramHandle PipelineManager::createRenderProgram( const RenderProgramInitParams &params )
    {
        try {
            GLShader shaders[2];
            
            if (params.sourceIsFilename) {
                shaders[0] = LoadShader(GL_VERTEX_SHADER, params.vertexSource.c_str());
                shaders[1] = LoadShader(GL_FRAGMENT_SHADER, params.fragmentSource.c_str());
            }
            else {
                const char *fragmentSource = params.fragmentSource.c_str();
                GLint fragmentSourceLen = (GLint) params.fragmentSource.length();
                shaders[0] = CreateShader(GL_FRAGMENT_SHADER, 1, &fragmentSource, &fragmentSourceLen);
        
                const char *vertexSource = params.vertexSource.c_str();
                GLint vertexSourceLen = (GLint) params.vertexSource.size();
                shaders[1] = CreateShader(GL_VERTEX_SHADER, 1, &vertexSource, &vertexSourceLen);
            }

            RenderProgramInfo info;
                info.glProgram =  CreateProgram(2, shaders);
                info.flags = params.flags;

            OnProgramCreated(&info, params.bindings);

            return mImpl->renderPrograms.create(std::move(info));
        } catch (const std::exception &e) {
            LOG_ERROR("Failed to create render program - error: %s", e.what());
            return ProgramHandle();
        }
    }

    PISCES_API void PipelineManager::destroyProgram( ProgramHandle handle )
    {
        mImpl->renderPrograms.free(handle);
    }

    PISCES_API ComputeProgramHandle PipelineManager::createComputeProgram( const ComputeProgramInitParams &params )
    {
        if (!mImpl->supportsComputeShaders) {
            LOG_ERROR("GL context doesn't support compute shaders!");
            return {};
        }

        try {
            const char *source = params.source.c_str();
            GLint sourceLen = (GLint) params.source.size();

            GLShader shader = CreateShader(gl::GL_COMPUTE_SHADER, 1, &source, &sourceLen);
            GLProgram program = CreateProgram(1, &shader);

            ComputeProgramInfo info;
                info.glProgram = std::move(program);
                info.flags = params.flags;

            OnProgramCreated(&info, params.bindings);
            
            return mImpl->computePrograms.create(std::move(info));
        } catch (const std::exception &e) {
            LOG_ERROR("Failed to create compute program - error: %s", e.what());
            return ComputeProgramHandle();
        }
    }

    PISCES_API void PipelineManager::destroyProgram( ComputeProgramHandle handle )
    {
        mImpl->computePrograms.free(handle);
    }

    PISCES_API PipelineHandle PipelineManager::createPipeline( const PipelineInitParams &params )
    {
        try {
            PipelineInfo info;
                info.program = params.program;
                info.name = params.name;
                info.flags = params.flags;
                info.blendMode = params.blendMode;
        
            return mImpl->pipelines.create(std::move(info));

        } catch (const std::exception &e) {
            LOG_ERROR("Faield to create pipeline \"%s\" - error: %s", Common::GetCString(params.name), e.what());

            if (all(params.flags, PipelineFlags::OwnProgram)) {
                destroyProgram(params.program);
            }

            return PipelineHandle {};
        }
    }

    PISCES_API PipelineHandle PipelineManager::createPipeline( const PipelineProgramInitParams &params )
    {
        ProgramHandle program = createRenderProgram(params.programParams);
        if (!program) {
            LOG_ERROR("Failed to create pipeline \"%s\" - failed to create render program.", Common::GetCString(params.name));
            return PipelineHandle {};
        }

        PipelineInitParams tmp;
        
        tmp.program = program;
        tmp.blendMode = params.blendMode;
        tmp.flags = set(params.flags, PipelineFlags::OwnProgram);
        tmp.name = params.name;

        return createPipeline(tmp);
    }

    PISCES_API void PipelineManager::destroyPipeline( PipelineHandle handle )
    {
        const PipelineInfo *info = mImpl->pipelines.find(handle);
        if (!info) return;

        if (all(info->flags, PipelineFlags::OwnProgram)) {
            destroyProgram(info->program);
        }

        mImpl->pipelines.free(handle);
    }
    
    PISCES_API bool PipelineManager::findPipeline( Common::StringId name, PipelineHandle &pipeline )
    {
        PipelineHandle handle = mImpl->pipelines.findIf( [name](const PipelineInfo &info) {
                return info.name == name;
            }
        );
        if (handle) {
            pipeline = handle;
            return true;
        }
        return false;
    }
}