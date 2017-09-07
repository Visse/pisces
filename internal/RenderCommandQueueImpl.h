#pragma once

#include <vector>
#include <array>


#include "Common/DataUnion.h"

namespace Pisces
{
    namespace RenderCommandQueueImpl 
    {
        using vec2 = std::array<float, 2>;
        using vec3 = std::array<float, 3>;
        using vec4 = std::array<float, 4>;
        using mat4 = std::array<float,4*4>;

        using uvec3 = std::array<uint32_t, 3>;

        enum class Type {
            Draw,
            DrawBuiltin,
            Clear,
            ExecuteCompute,

            UsePipeline,
            UseVertexArray,
            UseClipping,
            UseClipRect,

            BindSampler,
            BindBuiltinTexture,
            BindImageTexture,
            BindUniformBuffer,

            BindUniformInt,
            BindUniformFloat,

            BindUniformVec2,
            BindUniformVec3,
            BindUniformVec4,
            BindUniformMat4,
        };
        
        CREATE_DATA_STRUCT(Draw, Type,
            (Primitive, primitive),
            (size_t, first),
            (size_t, count),
            (size_t, base)
        );
        CREATE_DATA_STRUCT(DrawBuiltin, Type,
            (BuiltinObject, object)
        );
        CREATE_DATA_STRUCT(Clear, Type,
            (ClearFlags, flags),
            (Color, color),
            (float, depth),
            (int, stencil)
        );
        CREATE_DATA_STRUCT(ExecuteCompute, Type,
            (ComputeProgramHandle, program),
            (uvec3, size)
        );

        CREATE_DATA_STRUCT(UsePipeline, Type,
            (PipelineHandle, pipeline)                  
        );
        CREATE_DATA_STRUCT(UseVertexArray, Type,
            (VertexArrayHandle, vertexArray)                  
        );
        CREATE_DATA_STRUCT(UseClipping, Type,
            (bool, use)                  
        );
        CREATE_DATA_STRUCT(UseClipRect, Type,
            (ClipRect, clipRect)
        );

        CREATE_DATA_STRUCT(BindSampler, Type,
            (int, slot),
            (TextureHandle, sampler)
        );
        CREATE_DATA_STRUCT(BindBuiltinTexture, Type,
            (int, slot),
            (BuiltinTexture, texture)
        );
        CREATE_DATA_STRUCT(BindImageTexture, Type,
            (int, slot),
            (TextureHandle, texture),
            (ImageTextureAccess, access),
            (PixelFormat, format)
        );
        CREATE_DATA_STRUCT(BindUniformBuffer, Type,
            (int, slot),
            (UniformBufferHandle, buffer)
        );

        CREATE_DATA_STRUCT(BindUniformInt, Type,
            (int, location),
            (int, value)
        );
        CREATE_DATA_STRUCT(BindUniformFloat, Type,
            (int, location),
            (float, value)
        );

        CREATE_DATA_STRUCT(BindUniformVec2, Type,
            (int, location),
            (vec2, vec)
        );
        CREATE_DATA_STRUCT(BindUniformVec3, Type,
            (int, location),
            (vec3, vec)
        );
        CREATE_DATA_STRUCT(BindUniformVec4, Type,
            (int, location),
            (vec4, vec)
        );
        CREATE_DATA_STRUCT(BindUniformMat4, Type,
            (int, location),
            (mat4, matrix)                  
        );

        CREATE_DATA_UNION( Command, Type,
            (DrawData, draw),
            (DrawBuiltinData, drawBuiltin),
            (ClearData, clear),
            (ExecuteComputeData, executeCompute),

            (UsePipelineData, usePipeline),
            (UseVertexArrayData, useVertexArray),
            (UseClippingData, useClipping),
            (UseClipRectData, useClipRect),

            (BindSamplerData, bindSampler),
            (BindBuiltinTextureData, bindBuiltinTexture),
            (BindImageTextureData, bindImageTexture),
            (BindUniformBufferData, bindUniformBuffer),

            (BindUniformIntData, bindUniformInt),
            (BindUniformFloatData, bindUniformFloat),

            (BindUniformVec2Data, bindUniformVec2),
            (BindUniformVec3Data, bindUniformVec3),
            (BindUniformVec4Data, bindUniformVec4),
            (BindUniformMat4Data, bindUniformMat4)
        );

        struct Impl {
            Context *context;
            RenderTargetHandle renderTarget;
            RenderCommandQueueFlags flags;

            std::vector<Command> commands;

            Impl( Context *context_ , RenderTargetHandle renderTarget_ , RenderCommandQueueFlags flags_ ) :
                context(context_),
                renderTarget(renderTarget_),
                flags(flags_)
            {}
        };
    }

}