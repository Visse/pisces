#include "PipelineManagerImpl.h"

#include "Common/Throw.h"
#include "Common/FileUtils.h"

#include <glbinding/gl33core/gl.h>
using namespace gl33core;

#include <stdexcept>
#include <cassert>

namespace Pisces { 
namespace PipelineManagerImpl
{    
    GLShader LoadShader( GLenum type, const char *file ) {
        try {
            std::string sourceStr = FileUtils::getFileContent(file, false);

            const char *source = sourceStr.c_str();
            GLint lenght = (GLint)sourceStr.size();

            return CreateShader(type, 1, &source, &lenght);
        } catch (const std::exception &e) {
            THROW(std::runtime_error, "Failed to load shader from file \"%s\" error: %s", file, e.what());
        }
    }

    GLShader CreateShader( GLenum type, int count, const char* const *sources, const GLint *sourceLenghts )
    {
        GLShader shader(glCreateShader(type));

        glShaderSource(shader, count, sources, sourceLenghts);
        glCompileShader(shader);

        GLint status;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

        if (status != (GLint)GL_TRUE) {
            GLint logLenght = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLenght);

            std::vector<char> log(logLenght+1); // logLenght can be 0, so make sure to allocate atleast 1 byte
            glGetShaderInfoLog(shader, logLenght, nullptr, log.data());

            THROW(std::runtime_error, "%s", log.data());
        }

        return std::move(shader);
    }

    GLProgram CreateProgram( int count, const GLShader *shaders )
    {
        GLProgram program(glCreateProgram());

        for (int i=0; i < count; ++i) {
            glAttachShader(program, shaders[i]);
        }

        glLinkProgram(program);

        GLint status;
        glGetProgramiv(program, GL_LINK_STATUS, &status);

        if (status != (GLint)GL_TRUE) {
            GLint logLenght;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLenght);

            std::vector<char> log(logLenght+1); // logLenght can be 0, so make sure to allocate atleast 1 byte
            glGetProgramInfoLog(program, logLenght, nullptr, log.data());

            THROW(std::runtime_error, "Failed to link program: %s", log.data());
        }
        
        return std::move(program);
    }

    void OnProgramCreated( BaseProgramInfo *program, const ProgramInitBindings &bindings )
    {
        GLint count;
        glGetProgramiv(program->glProgram, GL_ACTIVE_UNIFORMS, &count);


        static const int MAX_NAME_LEN = 32;
        char name[MAX_NAME_LEN];
        GLenum glType;
        GLint size;
        GLsizei lenght;


        auto nameEqualString = [&]( const std::string &str ) -> bool {
            if (str.size() != lenght) return false;
            return str.compare(0, lenght, name, lenght) == 0;
        };

        // Map the uniforms found in the gl program to our side
        // So we later know how to translate our 'slots' to opengls locations.
        // This has the added benefit that its easy to add error check before calling to opengl
        for (int i=0; i < count; ++i) {
            glGetActiveUniform(program->glProgram, i, MAX_NAME_LEN, &lenght, &size, &glType, name);
            GLType type = FromGLType(glType);

            if (IsTypeSampler(type)) {
                bool found = false;
                int idx = 0;
                for (const auto &sampler : bindings.samplers) {
                    if (nameEqualString(sampler)) {
                        found = true;
                        program->samplers[idx].location = i;
                        program->samplers[idx].type = ToTextureType(type);
                        break;
                    }
                    ++idx;
                }

                if (!found) {
                    LOG_WARNING("Sampler \"%.*s\" is missing a binding point", lenght, name);
                }
            }
            else if(IsTypeImageTexture(type)) {
                bool found = false;
                int idx = 0;
                for (const auto &image : bindings.imageTextures) {
                    if (nameEqualString(image)) {
                        found = true;
                        program->imageTextures[idx].location = i;
                        program->imageTextures[idx].type = ToTextureType(type);
                        break;
                    }
                    ++idx;
                }

                if (!found) {
                    LOG_WARNING("Image Texture \"%.*s\" is missing a binding point", lenght, name);
                }
            }
            else { // Regular uniform
                bool found = false;
                int idx = 0;
                for (const auto &uniform : bindings.uniforms) {
                    if (nameEqualString(uniform)) {
                        found = true;
                        program->uniforms[idx].location = i;
                        program->uniforms[idx].type = type;
                        break;
                    }
                    ++idx;
                }

                if (!found) {
                    LOG_WARNING("Uniform \"%.*s\" is missing a binding point", lenght, name);
                }
            }
        }
    }
}}
