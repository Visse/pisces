#pragma once

#include "GLDebugCallback.h"
#include "Common/ErrorUtils.h"

#include <glbinding/gl43core/gl.h>
using namespace gl43core;

#include <glbinding/Binding.h>
#include <glbinding/Meta.h>

#include <set>

namespace Pisces
{
    void GL_APIENTRY gl_callback( GLenum source, GLenum type, GLuint id,
                            GLenum severity, GLsizei length,
                            const GLchar *message, const void *userParam )
    {
        static std::set<GLuint> SHOWED_ID;

        if( type != GL_DEBUG_TYPE_ERROR ) {
            if( SHOWED_ID.insert(id).second == false ) return;
        }

        const char *stype = "",
                *sseverity = "",
                *ssource = "";

#include "compiler_warnings/push.h"
#include "compiler_warnings/ignore_unhandled_enum_in_switch.h"
        switch (type) {
        case GL_DEBUG_TYPE_ERROR:
            stype = "ERROR";
            break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
            stype = "DEPRECATED_BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
            stype = "UNDEFINED_BEHAVIOR";
            break;
        case GL_DEBUG_TYPE_PORTABILITY:
            stype = "PORTABILITY";
            break;
        case GL_DEBUG_TYPE_PERFORMANCE:
            stype = "PERFORMANCE";
            break;
        case GL_DEBUG_TYPE_OTHER:
            stype = "OTHER";
            break;
        case GL_DEBUG_TYPE_MARKER:
            stype = "MARKER";
            break;
        case GL_DEBUG_TYPE_PUSH_GROUP:
            stype = "PUSH_GROUP";
            break;
        case GL_DEBUG_TYPE_POP_GROUP:
            stype = "POP_GROUP";
            break;
        default:
            stype = "UNKNOWN";
        }
        switch (severity){
        case GL_DEBUG_SEVERITY_LOW:
            sseverity = "LOW";
            break;
        case GL_DEBUG_SEVERITY_MEDIUM:
            sseverity = "MEDIUM";
            break;
        case GL_DEBUG_SEVERITY_HIGH:
            sseverity = "HIGH";
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION:
            sseverity = "NOTIFICATION";
            break;
        default:
            sseverity = "UNKNOWN";
        }
        switch (source){
        case GL_DEBUG_SOURCE_API:
            ssource = "API";
            break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
            ssource = "WINDOW_SYSTEM";
            break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER:
            ssource = "SHADER_COMPILER";
            break;
        case GL_DEBUG_SOURCE_THIRD_PARTY:
            ssource = "THIRD_PARTY";
            break;
        case GL_DEBUG_SOURCE_APPLICATION:
            ssource = "APPLICATION";
            break;
        case GL_DEBUG_SOURCE_OTHER:
            ssource = "OTHER";
            break;
        default:
            ssource = "UNKNOWN";
        }
#include "compiler_warnings/pop.h"
                
        LOG_INFORMATION( "[OpenGL] [%i:%s] [%i:%s] [%i:%s] %u: %*s",
                (int)severity, sseverity, 
                (int)type, stype, 
                (int)source, ssource, 
                id, length, message
        );
    }

    void InstallGLDebugHooks()
    {
        if( glbinding::Meta::extensions().count(gl::GLextension::GL_KHR_debug) ) {
            glEnable(gl::GL_DEBUG_OUTPUT_SYNCHRONOUS);
            glDebugMessageCallback( gl_callback, nullptr );
            glDebugMessageControl( gl::GL_DONT_CARE,
                                    gl::GL_DONT_CARE, 
                                    gl::GL_DONT_CARE,
                                    0,
                                    nullptr,
                                    gl::GL_TRUE
                                    );
                
            LOG_INFORMATION( "[OpenGL] Installed debug message callback for opengl." );
        }
        else {
            LOG_ERROR( "[OpenGL] Can't install debug message callback for opengl, missing extension 'GL_KHR_debug'." );
        }

        glbinding::setAfterCallback(
            []( const glbinding::FunctionCall &function ) {
                GLenum error = glGetError();
                if( error != gl::GL_NO_ERROR ) {
                    LOG_ERROR( "[OpenGL] The call to \"%s\" generated error \"%s\" (%i) - call: %s",
                            function.function->name(), glbinding::Meta::getString(error).c_str(), (int)error, function.toString().c_str()
                    );
                }
            }
        );
        glbinding::setCallbackMaskExcept( glbinding::CallbackMask::After | glbinding::CallbackMask::ParametersAndReturnValue,
                                            {"glGetError"}
        );

        LOG_INFORMATION( "[OpenGL] Installed glbinding after-callback." );
    }
}