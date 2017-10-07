#pragma once

#include "../Fwd.h"

#include "Common/DataUnion.h"

#include <glbinding/gl33core/types.h>
using namespace gl33core;

#include <vector>

namespace Pisces
{
    namespace CompiledRenderQueueImpl 
    {
        enum class Type {
            Enable,
            Disable,


            SetProgram,
            SetBlendFunc,
            SetDepthMask,
            SetClipRect,
            SetClearColor,
            SetClearDepth,
            SetClearStencil,

            BindVertexArray,
            BindIndexBuffer,
            BindSampler,

            Clear,
            Draw,
            DrawIndexed,

            BindUniformInt,
            BindUniformFloat,

            BindUniformVec2,
            BindUniformVec3,
            BindUniformVec4,
            BindUniformMat4,
            
            BindImageTexture,
            DispatchCompute,

            BindBufferRange,
            BeginTransformFeedback,
            EndTransformFeedback,
        };

        using vec2 = std::array<float,2>;
        using vec3 = std::array<float,3>;
        using vec4 = std::array<float,4>;
        using mat4 = std::array<float,4*4>;

        CREATE_DATA_STRUCT( Enable, Type,
            (GLenum, cap)
        );
        CREATE_DATA_STRUCT( Disable, Type,
            (GLenum, cap)                  
        );

        CREATE_DATA_STRUCT( SetProgram, Type,
            (GLuint, program)                  
        );
        CREATE_DATA_STRUCT( SetBlendFunc, Type,
            (GLenum, sfactor),
            (GLenum, dfactor)
        );
        CREATE_DATA_STRUCT( SetDepthMask, Type,
            (bool, mask)                  
        );
        CREATE_DATA_STRUCT(SetClipRect, Type,
            (ClipRect, rect)                  
        );
        CREATE_DATA_STRUCT( SetClearColor, Type,
            (Color, color)                  
        );
        CREATE_DATA_STRUCT( SetClearDepth, Type,
            (float, depth)                  
        );
        CREATE_DATA_STRUCT( SetClearStencil, Type,
            (int, stencil)                  
        );

        CREATE_DATA_STRUCT( BindVertexArray, Type,
            (GLuint, vertexArray)
        );
        CREATE_DATA_STRUCT( BindIndexBuffer, Type,
            (GLuint, indexBuffer)                  
        );
        CREATE_DATA_STRUCT( BindSampler, Type,
            (int, unit),
            (GLenum, target),
            (GLuint, texture),
            (GLuint, sampler)
        );

        CREATE_DATA_STRUCT( Clear, Type,
            (GLenum, mask)                  
        );

        CREATE_DATA_STRUCT( Draw, Type,
            (GLenum, primitive),
            (size_t, first),
            (size_t, count)
        );

        CREATE_DATA_STRUCT( DrawIndexed, Type,
            (GLenum, primitive),
            (GLenum, indexType),
            (size_t, count),
            (size_t, base),
            (void*, offset)
        );
        CREATE_DATA_STRUCT(BindUniformInt, Type,
            (GLint, location),
            (GLint, value)
        );
        CREATE_DATA_STRUCT(BindUniformFloat, Type,
            (GLint, location),
            (GLfloat, value)
        );
        CREATE_DATA_STRUCT(BindUniformVec2, Type,
            (GLint, location),
            (vec2, vec)
        );
        CREATE_DATA_STRUCT(BindUniformVec3, Type,
            (GLint, location),
            (vec3, vec)
        );
        CREATE_DATA_STRUCT(BindUniformVec4, Type,
            (GLint, location),
            (vec4, vec)
        );
        CREATE_DATA_STRUCT(BindUniformMat4, Type,
            (GLint, location),
            (mat4, matrix)
        );

        CREATE_DATA_STRUCT(BindImageTexture, Type,
            (GLuint, unit),
            (GLuint, texture),
            (GLint, level),
            (GLboolean, layered),
            (GLint, layer),
            (GLenum, access),
            (GLenum, format)
        );
        CREATE_DATA_STRUCT(DispatchCompute, Type,
            (GLuint, size_x),
            (GLuint, size_y),
            (GLuint, size_z)
        );

        CREATE_DATA_STRUCT(BindBufferRange, Type,
            (GLenum, target),
            (GLuint, index),
            (GLuint, buffer),
            (GLintptr, offset),
            (GLsizeiptr, size)
        );

        CREATE_DATA_STRUCT(BeginTransformFeedback, Type,
            (GLenum, primitive)
        );
        CREATE_DATA_STRUCT(EndTransformFeedback, Type);

        CREATE_DATA_UNION( Command, Type,
            (EnableData, enable),
            (DisableData, disable),
            
            (SetProgramData, setProgram),
            (SetBlendFuncData, setBlendFunc),
            (SetDepthMaskData, setDepthMask),
            (SetClipRectData, setClipRect),
            (SetClearColorData, setClearColor),
            (SetClearDepthData, setClearDepth),
            (SetClearStencilData, setClearStencil),
            
            (BindVertexArrayData, bindVertexArray),
            (BindIndexBufferData, bindIndexBuffer),
            (BindSamplerData, bindSampler),
            
            (ClearData, clear),
            (DrawData, draw),
            (DrawIndexedData, drawIndexed),
            
            (BindUniformIntData, bindUniformInt),
            (BindUniformFloatData, bindUniformFloat),

            (BindUniformVec2Data, bindUniformVec2),
            (BindUniformVec3Data, bindUniformVec3),
            (BindUniformVec4Data, bindUniformVec4),
            (BindUniformMat4Data, bindUniformMat4),

            (BindImageTextureData, bindImageTexture),
            (DispatchComputeData, dispatchCompute),

            (BindBufferRangeData, bindBufferRange),

            (BeginTransformFeedbackData, beginTransformFeedback),
            (EndTransformFeedbackData, endTransformFeedback)
        );

        struct Impl {
            Context *context;

            RenderTargetHandle renderTarget;
            std::vector<Command> commands;

            Impl( Context *context_ ) :
                context(context_)
            {}
        };
    }
}