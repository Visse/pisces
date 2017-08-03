#include "HardwareResourceManagerImpl.h"
#include "Helpers.h"

#include <glbinding/gl33core/gl.h>
using namespace gl33core;

namespace Pisces
{
    namespace HardwareResourceManagerImpl
    {
        void setRealSamplerParams( gl::GLuint sampler, const SamplerParams &params )
        {
            glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, ToGL(params.minFilter));
            glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, ToGL(params.magFilter));
            glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, ToGL(params.edgeSamplingX));
            glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, ToGL(params.edgeSamplingY));

            GLfloat borderColor[4] = {
                params.borderColor.r / (float)0xFF,
                params.borderColor.g / (float)0xFF,
                params.borderColor.b / (float)0xFF,
                params.borderColor.a / (float)0xFF,
            };
            glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, borderColor);
        }

        void setRealSamplerParamsTexture( gl::GLenum target, gl::GLuint texture, const SamplerParams &params )
        {
            glBindTexture(target, texture);

            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, ToGL(params.minFilter));
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, ToGL(params.magFilter));
            glTexParameteri(target, GL_TEXTURE_WRAP_S, ToGL(params.edgeSamplingX));
            glTexParameteri(target, GL_TEXTURE_WRAP_T, ToGL(params.edgeSamplingY));

            GLfloat borderColor[4] = {
                params.borderColor.r / (float)0xFF,
                params.borderColor.g / (float)0xFF,
                params.borderColor.b / (float)0xFF,
                params.borderColor.a / (float)0xFF,
            };
            glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, borderColor);
        }
    }
}