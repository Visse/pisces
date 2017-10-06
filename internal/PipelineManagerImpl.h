#pragma once

#include "Fwd.h"
#include "PipelineManager.h"
#include "internal/GLTypes.h"
#include "internal/Helpers.h"

#include "Common/HandleVector.h"
#include "Common/StringId.h"

namespace Pisces
{
    namespace PipelineManagerImpl
    {
        struct SamplerInfo {
            int location = -1;
            TextureType type = TextureType(-1);
        };
        struct UniformBufferInfo {
            int location = -1;
        };
        struct UniformInfo {
            int location = -1;
            GLType type = GLTypeNone;
        };
        struct ImageTextureInfo {
            int location = -1;
            TextureType type = TextureType(-1);
        };

        struct BaseProgramInfo {
            Common::StringId name;
            GLProgram glProgram;
            
            SamplerInfo samplers[MAX_BOUND_SAMPLERS];
            UniformBufferInfo uniformBuffers[MAX_BOUND_UNIFORM_BUFFERS];
            UniformInfo uniforms[MAX_BOUND_UNIFORMS];
            ImageTextureInfo imageTextures[MAX_BOUND_IMAGE_TEXTURES];
        };
        struct RenderProgramInfo :
            public BaseProgramInfo
        {
            RenderProgramFlags flags = RenderProgramFlags::None;
        };

        struct ComputeProgramInfo :
            public BaseProgramInfo
        {
            ComputeProgramFlags flags = ComputeProgramFlags::None;
        };

        struct TransformProgramInfo :
            public BaseProgramInfo
        {
            TransformProgramFlags flags = TransformProgramFlags::None;
        };

        struct PipelineInfo {
            ProgramHandle program;
            Common::StringId name;

            PipelineFlags flags = PipelineFlags::None;
            BlendMode blendMode = BlendMode::Replace;
        };

        struct Impl {
            Context *context;

            HandleVector<ProgramHandle, RenderProgramInfo> renderPrograms;
            HandleVector<PipelineHandle, PipelineInfo> pipelines;
            HandleVector<ComputeProgramHandle, ComputeProgramInfo> computePrograms;
            HandleVector<TransformProgramHandle, TransformProgramInfo> transformPrograms;

            bool veryfyUniformBlocks = true;

            bool supportsComputeShaders = false;

            Impl( Context *context_ ) :
                context(context_)
            {}
        };

        GLShader LoadShader( gl::GLenum type, const char *file );
        GLShader CreateShader( gl::GLenum type, int count, const char* const*sources, const gl::GLint *sourceLenghts );
        
        void LinkProgram( gl::GLuint program );
        GLProgram CreateProgram( int count, const GLShader *shaders );

        void OnProgramCreated( BaseProgramInfo *program, const ProgramInitBindings &bindings );
    }
}