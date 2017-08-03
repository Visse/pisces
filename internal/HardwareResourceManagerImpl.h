#pragma once

#include "internal/GLTypes.h"
#include "internal/GLCompat.h"

#include "Common/HandleVector.h"
#include "Common/StringId.h"

namespace Pisces
{
    namespace HardwareResourceManagerImpl
    {
        struct TextureInfo {
            TextureInfo( GLTexture texture,   PixelFormat format_,
                         TextureFlags flags_, TextureType type_ ) :
                glTexture(std::move(texture)),
                format(format_),
                flags(flags_),
                type(type_)
            {}

            TextureInfo( TextureInfo &&move ) = default;
            
            Common::StringId name;
            GLTexture glTexture;
            PixelFormat format;
            TextureFlags flags;
            TextureType type;

            struct {
                int x=0, y=0, z=0;
            } size;
            int mipmaps = 0;
            size_t expectedMemoryUse = 0;
        };

        struct SamplerInfo {
            Common::StringId name;
            GLSampler glSampler;
            TextureHandle texture;
        };

        struct BufferInfo {
            GLBuffer glBuffer;
            BufferType type;
            BufferUsage usage;
            BufferFlags flags;

            size_t size;

            GLCompat::BufferPersistentMapping persistentMapping;
        };

        struct BuiltinDrawInfo {
            VertexArrayHandle vertexArray;
            Primitive primitive = Primitive(0);
            size_t first = 0, 
                   count = 0,
                   base = 0;
        };

        static const int TEXTURE_HANDLE_DATA_BITS = 4;
        using TextureHandleVector = HandleVector<TextureHandle, TextureInfo, HANDLE_VECTOR_DEFAULT, TEXTURE_HANDLE_DATA_BITS, 0>;
        using SamplerHandleVector = HandleVector<TextureHandle, SamplerInfo, HANDLE_VECTOR_DEFAULT, TEXTURE_HANDLE_DATA_BITS, 1>; 


        struct VertexArrayInfo {
            GLVertexArray glVertexArray;

            BufferHandle indexBuffer;
            IndexType indexType;
        };

        struct Impl {
            Context *context = nullptr;
            std::size_t textureMemoryUsage = 0;
            std::size_t bufferMemoryUsage = 0;

            TextureHandleVector textures;
            SamplerHandleVector samplers;

            HandleVector<BufferHandle, BufferInfo> buffers;
            HandleVector<VertexArrayHandle, VertexArrayInfo> vertexArrays;

            BuiltinDrawInfo builtinDrawInfo[BUILTIN_OBJECT_COUNT];

            TextureHandle missingTexture;

            size_t uniformBlockAllignment = 0,
                   maxTextxureUnits = 0;

            struct {
                BufferHandle buffer;
                size_t size = 0,
                       nextOffset = 0;
            } staticUniforms;

            Impl( Context *context_ ) :
                context(context_)
            {}

            void onTextureAllocation( std::size_t size ) {
                textureMemoryUsage += size;
            }
            void onBufferAllocation( std::size_t size ) {
                bufferMemoryUsage += size;
            }
        };

        void setRealSamplerParams( gl::GLuint sampler, const SamplerParams &params );
        void setRealSamplerParamsTexture( gl::GLenum target, gl::GLuint texture, const SamplerParams &params );
    }
}