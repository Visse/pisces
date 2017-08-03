#pragma once

#include "Common/HandleType.h"

#include <glbinding/gl/types.h>
#include <glbinding/gl/functions.h>


namespace Pisces
{
    struct GLTextureDelete {
        void operator () ( gl::GLuint texture ) {
            gl::glDeleteTextures(1, &texture);
        }
    };
    WRAP_HANDLE(GLTexture, gl::GLuint, GLTextureDelete, 0);

    struct GLBufferDelete {
        void operator () ( gl::GLuint buffer ) {
            gl::glDeleteBuffers(1, &buffer);
        }
    };
    WRAP_HANDLE(GLBuffer, gl::GLuint, GLBufferDelete, 0);

    struct GLProgramDelete {
        void operator () ( gl::GLuint program ) {
            gl::glDeleteProgram(program);
        }
    };
    WRAP_HANDLE(GLProgram, gl::GLuint, GLProgramDelete, 0);

    struct GLShaderDelete {
        void operator () ( gl::GLuint shader ) {
            gl::glDeleteShader(shader);
        }
    };
    WRAP_HANDLE(GLShader, gl::GLuint, GLShaderDelete, 0);

    struct GLSamplerDelete {
        void operator () ( gl::GLuint sampler ) {
            gl::glDeleteSamplers(1, &sampler);
        }
    };
    WRAP_HANDLE(GLSampler, gl::GLuint, GLSamplerDelete, 0);

    struct GLVertexArrayDelete {
        void operator () ( gl::GLuint vertexArray ) {
            gl::glDeleteVertexArrays(1, &vertexArray);
        }
    };
    WRAP_HANDLE(GLVertexArray, gl::GLuint, GLVertexArrayDelete, 0);
    

    struct GLFrameBufferDelete {
        void operator () ( gl::GLuint framebuffer ) {
            gl::glDeleteFramebuffers(1, &framebuffer);
        }
    };
    WRAP_HANDLE(GLFrameBuffer, gl::GLuint, GLFrameBufferDelete, 0);

    struct GLFrameSyncDelete {
        void operator () ( gl::GLsync sync ) {
            gl::glDeleteSync(sync);
        }
    };
    WRAP_HANDLE(GLFrameSync, gl::GLsync, GLFrameSyncDelete, 0);
}