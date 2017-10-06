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
                info.name = params.name;
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
                info.name = params.name;
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
    
    PISCES_API TransformProgramHandle PipelineManager::createTransformProgram( const TransformProgramInitParams &params, const TransformCaptureVariable *captureVariables, size_t count )
    {
        try {
            if (count > MAX_TRANFORM_FEEDBACK_CAPTURE_VARIABLES) {
                THROW(std::runtime_error,
                    "To many capture variables (%i), max supported is %i", 
                      (int)count, (int)MAX_TRANFORM_FEEDBACK_CAPTURE_VARIABLES
                );
            }
            const char *source = params.source.c_str();
            GLint sourceLen = (GLint) params.source.size();

            GLShader vertexShader = CreateShader(GL_VERTEX_SHADER, 1, &source, &sourceLen);
            GLProgram program(glCreateProgram());

            glAttachShader(program, vertexShader);

            const char *variables[MAX_TRANFORM_FEEDBACK_CAPTURE_VARIABLES];
            for (int i=0; i < count; ++i) {
                variables[i] = captureVariables[i].name.c_str();
            }
            glTransformFeedbackVaryings(program, (GLsizei)count, variables, GL_INTERLEAVED_ATTRIBS);

            LinkProgram(program);

            if (glbinding::ContextInfo::supported({gl::GLextension::GL_ARB_program_interface_query})) {
                for (int i=0; i < count; ++i) {
                    GLint index = gl::glGetProgramResourceIndex(program, gl::GL_TRANSFORM_FEEDBACK_VARYING, variables[i]);
                    if (index == GL_INVALID_INDEX) {
                        LOG_ERROR("Capture variable \"%s\" is not active!", variables[i]);
                        continue;
                    }

                    GLenum prop = gl::GL_TYPE;
                    GLsizei lenght = 1;
                    GLint type;
                    gl::glGetProgramResourceiv(program, GL_TRANSFORM_FEEDBACK_VARYINGS, index, 1, &prop, lenght, &lenght, &type);

                    TransformCaptureType captureType = ToTransformCaptureType((GLenum)type);
                    if (captureType != captureVariables[i].type) {
                        LOG_ERROR("Capture variable \"%s\" type missmatch, expected \"%s\" got \"%s\"", 
                                  variables[i], 
                                  TransformCaptureTypeToString(captureVariables[i].type),
                                  TransformCaptureTypeToString(captureType)
                        );
                    }
                }
            }
            else {
                LOG_WARNING("GL_ARB_program_interface_query not supported - can't validate capture variables!");
            }

            TransformProgramInfo info;
            info.name = params.name;
            info.glProgram = std::move(program);
            info.flags = params.flags;

            OnProgramCreated(&info, params.bindings);

            return mImpl->transformPrograms.create(std::move(info));
        } catch (const std::exception &e) {
            LOG_ERROR("Failed to create transform program - error: %s", e.what());
            return TransformProgramHandle();
        }
    }

    PISCES_API void PipelineManager::destroyProgram( TransformProgramHandle handle )
    {
        mImpl->transformPrograms.free(handle);
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

    PISCES_API PipelineHandle PipelineManager::findPipeline(Common::StringId name)
    {
        PipelineHandle pipeline;
        findPipeline(name, pipeline);
        return pipeline;
    }
}