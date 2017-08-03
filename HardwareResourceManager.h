#pragma once

#include "Fwd.h"

#include "Common/PImplHelper.h"
#include "Common/StringId.h"

#include <type_traits>

namespace Pisces
{
    namespace HardwareResourceManagerImpl {struct Impl;}
    class HardwareResourceManager {
    public:
        HardwareResourceManager( Context *context );
        ~HardwareResourceManager();

        // Allocates a new texture
        // if mipmaps is 0, its automatic calculated based on the size
        TextureHandle allocateTexture2D( PixelFormat format, TextureFlags flags, int width, int height, int mipmaps=0 );
        TextureHandle allocateCubemap( PixelFormat format, TextureFlags flags, int size, int mipmaps=0 );
        void freeTexture( TextureHandle texture );
        
        BufferHandle allocateBuffer( BufferType type, BufferUsage usage, BufferFlags flags, size_t size, const void *data );
        void freeBuffer( BufferHandle buffer );

        void uploadTexture2D( TextureHandle texture, int mipmap, TextureUploadFlags flags, PixelFormat format, const void *data );
        void uploadCubemap( TextureHandle texture, CubemapFace face, int mipmap, TextureUploadFlags flags, PixelFormat format, const void *data );
        void uploadBuffer( BufferHandle buffer, size_t offset, size_t size, const void *data );

        void setSwizzleMask( TextureHandle texture, SwizzleMask red, SwizzleMask green, SwizzleMask blue, SwizzleMask alpha );
        TextureHandle createSampler( TextureHandle texture, const SamplerParams &params );
        void setSamplerParams( TextureHandle texture, const SamplerParams &params );

        VertexArrayHandle createVertexArray( const VertexAttribute *attributes, int attributeCount,
                                             const BufferHandle *sourceBuffers, int sourceCount,
                                             BufferHandle indexBuffer, IndexType indexType );

        void deleteVertexArray( VertexArrayHandle vertexArray );

        TextureHandle loadTexture2D( PixelFormat format, TextureFlags flags, const char *filename );
        // filenameTemplate should have a %FACE% that gets replaces with pos_x, neg_x, etc..
        TextureHandle loadCubemap( PixelFormat format, TextureFlags flags, const char *filenameTemplate );
        
        void setTextureName( TextureHandle texture, Common::StringId name );
        TextureHandle findTextureByName( Common::StringId name );

        TextureHandle getMissingTexture();

        HardwareResourceManagerImpl::Impl* impl() {
            return mImpl.impl();
        }

        void* mapBuffer( BufferHandle buffer, size_t offset, size_t size, BufferMapFlags flags );
        // Returns true if persistent mapped
        bool unmapBuffer( BufferHandle buffer, bool keepPersistent=false );

        template< typename Type >
        Type* mapBuffer( BufferHandle buffer, size_t first, size_t count, BufferMapFlags flags )
        {
            return (Type*)mapBuffer(buffer, first*sizeof(Type), count*sizeof(Type), flags);
        }

        UniformBufferHandle allocateStaticUniform( size_t size, const void *data );
        void updateStaticUniform( UniformBufferHandle uniform, size_t size, const void *data );

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
        void copyBuffer( BufferHandle target, size_t targetOffset, BufferHandle source, size_t sourceOffset, size_t size );

        size_t getUniformAlignment();

        // WARNING this will internally recreate the buffer - this causes issuis if the buffer have been used in other structures,
        // such as, but not limited to, a source for a VertexArray - Use with care
        void resizeBuffer( BufferHandle buffer, size_t newSize, BufferResizeFlags flags, size_t sourceOffset=0, size_t targetOffset=0, size_t size=0);

    private:
        PImplHelper<HardwareResourceManagerImpl::Impl,1024> mImpl;
    };
}