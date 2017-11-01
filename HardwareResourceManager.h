#pragma once

#include "Fwd.h"
#include "build_config.h"

#include "Common/PImplHelper.h"
#include "Common/StringId.h"

#include <type_traits>

namespace Pisces
{
    namespace HardwareResourceManagerImpl {struct Impl;}
    class HardwareResourceManager {
    public:
        PISCES_API HardwareResourceManager( Context *context );
        PISCES_API ~HardwareResourceManager();

        // Allocates a new texture
        // if mipmaps is 0, its automatic calculated based on the size
        PISCES_API TextureHandle allocateTexture2D( PixelFormat format, TextureFlags flags, int width, int height, int mipmaps=0 );
        PISCES_API TextureHandle allocateCubemap( PixelFormat format, TextureFlags flags, int size, int mipmaps=0 );
        PISCES_API void freeTexture( TextureHandle texture );
        
        PISCES_API BufferHandle allocateBuffer( BufferType type, BufferUsage usage, BufferFlags flags, size_t size, const void *data );
        PISCES_API void freeBuffer( BufferHandle buffer );

        PISCES_API void uploadTexture2D( TextureHandle texture, int mipmap, TextureUploadFlags flags, PixelFormat format, const void *data );
        PISCES_API void uploadCubemap( TextureHandle texture, CubemapFace face, int mipmap, TextureUploadFlags flags, PixelFormat format, const void *data );
        PISCES_API void uploadBuffer( BufferHandle buffer, size_t offset, size_t size, const void *data );

        PISCES_API void setSwizzleMask( TextureHandle texture, SwizzleMask red, SwizzleMask green, SwizzleMask blue, SwizzleMask alpha );
        PISCES_API TextureHandle createSampler( TextureHandle texture, const SamplerParams &params );
        PISCES_API void setSamplerParams( TextureHandle texture, const SamplerParams &params );

        PISCES_API VertexArrayHandle createVertexArray( const VertexAttribute *attributes, int attributeCount,
                                                        const BufferHandle *sourceBuffers, int sourceCount,
                                                        BufferHandle indexBuffer, IndexType indexType, 
                                                        VertexArrayFlags flags = VertexArrayFlags::None );
        PISCES_API void deleteVertexArray( VertexArrayHandle vertexArray );

        PISCES_API TextureHandle loadTexture2D( PixelFormat format, TextureFlags flags, const char *filename );
        // filenameTemplate should have a %FACE% that gets replaces with pos_x, neg_x, etc..
        PISCES_API TextureHandle loadCubemap( PixelFormat format, TextureFlags flags, const char *filenameTemplate );
        
        PISCES_API void setTextureName( TextureHandle texture, Common::StringId name );
        PISCES_API TextureHandle findTextureByName( Common::StringId name );

        PISCES_API TextureHandle getBuiltinTexture( BuiltinTexture texture );


        PISCES_API void* mapBuffer( BufferHandle buffer, size_t offset, size_t size, BufferMapFlags flags );
        // Returns true if persistent mapped
        PISCES_API bool unmapBuffer( BufferHandle buffer, bool keepPersistent=false );

        template< typename Type >
        Type* mapBuffer( BufferHandle buffer, size_t first, size_t count, BufferMapFlags flags )
        {
            return (Type*)mapBuffer(buffer, first*sizeof(Type), count*sizeof(Type), flags);
        }

        PISCES_API UniformBufferHandle allocateStaticUniform( size_t size, const void *data );
        PISCES_API void updateStaticUniform( UniformBufferHandle uniform, size_t size, const void *data );

        template< typename Type >
        UniformBufferHandle allocateStaticUniform( const Type &type ) {
            static_assert(std::is_standard_layout<Type>::value, "Type must be of standard layout!");
            static_assert(std::is_trivially_destructible<Type>::value, "Type must be trivially destructible!");
            return allocateStaticUniform(sizeof(Type), &type);
        }

        template< typename Type >
        void updateStaticUniform( UniformBufferHandle uniform,  const Type &type ) {
            static_assert(std::is_standard_layout<Type>::value, "Type must be of standard layout!");
            static_assert(std::is_trivially_destructible<Type>::value, "Type must be trivially destructible!");
            updateStaticUniform(uniform, sizeof(Type), &type);
        }
        PISCES_API void copyBuffer( BufferHandle target, size_t targetOffset, BufferHandle source, size_t sourceOffset, size_t size );

        PISCES_API size_t getUniformAlignment();

        // WARNING this will internally recreate the buffer - this causes issuis if the buffer have been used in other structures,
        // such as, but not limited to, a source for a VertexArray - Use with care
        PISCES_API void resizeBuffer( BufferHandle buffer, size_t newSize, BufferResizeFlags flags, size_t sourceOffset=0, size_t targetOffset=0, size_t size=0);
        
        PISCES_API void downloadBuffer( BufferHandle buffer, size_t offset, size_t size, void *data );

        HardwareResourceManagerImpl::Impl* impl() {
            return mImpl.impl();
        }
    private:
        PImplHelper<HardwareResourceManagerImpl::Impl,1024> mImpl;
    };
}