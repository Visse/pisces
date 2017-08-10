#include "HardwareResourceManager.h"
#include "internal/HardwareResourceManagerImpl.h"
#include "internal/GLCompat.h"
#include "internal/Helpers.h"
#include "internal/BuiltinObjects.h"
#include "internal/BuiltinTextures.h"

#include "Common/HandleVector.h"
#include "Common/PointerHelpers.h"

#include <glbinding/gl33core/gl.h>
using namespace gl33core;

#include <cassert>

#include "stb_image.h"
WRAP_HANDLE_FUNC(stbi_image, stbi_uc*, stbi_image_free, nullptr);

namespace Pisces
{
    using namespace HardwareResourceManagerImpl;

    void loadBuiltinTypes( HardwareResourceManager *hardwareMgr );

    PISCES_API HardwareResourceManager::HardwareResourceManager( Context *context ) :
        mImpl(context)
    {
        int alignment;
        glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);

        LOG_INFORMATION("[OpenGL] Uniform block alignment: %i", alignment);
        mImpl->uniformBlockAllignment = alignment;

        int maxTextureUnits;
        glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTextureUnits);

        LOG_INFORMATION("[OpenGL] Max texture units is: %i", maxTextureUnits);
        mImpl->maxTextxureUnits = maxTextureUnits;


        loadBuiltinTypes(this);

        stbi_set_flip_vertically_on_load(true);

        const auto &MissingTexture = BuiltinTextures::MissingTexture;
        mImpl->missingTexture = allocateTexture2D(MissingTexture.format, TextureFlags::None, MissingTexture.width, MissingTexture.height);
        uploadTexture2D(mImpl->missingTexture, 0, TextureUploadFlags::GenerateMipmaps, MissingTexture.format, MissingTexture.pixels);
    }

    PISCES_API HardwareResourceManager::~HardwareResourceManager()
    {
    }

    PISCES_API TextureHandle HardwareResourceManager::allocateTexture2D( PixelFormat format, TextureFlags flags, int width, int height, int mipmaps )
    {
        if (mipmaps == 0) {
            mipmaps = MipmapsForSize(width, height);
        }

        GLTexture texture;
        glGenTextures(1, &texture.handle);
        glBindTexture(GL_TEXTURE_2D, texture);

        // Set default sampler params
        setRealSamplerParamsTexture(GL_TEXTURE_2D, texture, SamplerParams());

        GLenum internalFormat = InternalPixelFormat(format);
        GLCompat::TexStorage2D(GL_TEXTURE_2D, mipmaps, internalFormat, width, height);

        TextureInfo info(std::move(texture), format, flags, TextureType::Texture2D);
            info.size.x = width;
            info.size.y = height;
            info.mipmaps = mipmaps;
            info.expectedMemoryUse = expectedMemoryUseTexture2D(format, width, height, mipmaps);

        mImpl->onTextureAllocation(info.expectedMemoryUse);


        return mImpl->textures.create(std::move(info));
    }

    PISCES_API TextureHandle HardwareResourceManager::allocateCubemap( PixelFormat format, TextureFlags flags, int size, int mipmaps )
    {
        if (mipmaps == 0) {
            mipmaps = MipmapsForSize(size);
        }

        GLTexture texture;
        glGenTextures(1, &texture.handle);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

        // Set default sampler params
        setRealSamplerParamsTexture(GL_TEXTURE_CUBE_MAP, texture, SamplerParams());

        GLenum internalFormat = InternalPixelFormat(format);
        GLCompat::TexStorageCube(GL_TEXTURE_CUBE_MAP, mipmaps, internalFormat, size);

        TextureInfo info(std::move(texture), format, flags, TextureType::Cubemap);
            info.size.x = info.size.y = size;
            info.mipmaps = mipmaps;
            info.expectedMemoryUse = expectedMemoryUseCubemap(format, size, mipmaps);
            
        mImpl->onTextureAllocation(info.expectedMemoryUse);


        return mImpl->textures.create(std::move(info));
    }

    PISCES_API void HardwareResourceManager::freeTexture( TextureHandle texture )
    {
        if (TextureHandleVector::IsHandleFromThis(texture)) {
            mImpl->textures.free(texture);
        }
        if (SamplerHandleVector::IsHandleFromThis(texture)) {
            mImpl->samplers.free(texture);
        }
    }

    PISCES_API BufferHandle HardwareResourceManager::allocateBuffer( BufferType type, BufferUsage usage, BufferFlags flags, size_t size, const void *data )
    {
        GLBuffer buffer;
        glGenBuffers(1, &buffer.handle);

        GLenum target = BufferTarget(type);
        glBindBuffer(target, buffer);

        GLCompat::BufferStorage(target, usage, flags, size, data);

        BufferInfo info;
            info.glBuffer = std::move(buffer);
            info.type = type;
            info.usage = usage;
            info.flags = flags;
            info.size = size;

        mImpl->onBufferAllocation(size);

        return mImpl->buffers.create(std::move(info));
    }

    PISCES_API void HardwareResourceManager::freeBuffer( BufferHandle buffer )
    {
        mImpl->buffers.free(buffer);
    }

    template< typename PixelType >
    void flipTexture( PixelType *pixels, int w, int h )
    {
        for (int y=0; y < h/2; ++y) {
            PixelType *src = pixels + y * w;
            PixelType *dst = pixels + (h-y-1) * w;
            std::swap_ranges(src, src+w, dst);
        }
    }
    
    struct Pixel_R8 {
        uint8_t r;
    };
    struct Pixel_RG8 {
        uint8_t r, g;
    };
    struct Pixel_RGB8 {
        uint8_t r, g, b;
    };
    struct Pixel_RGBA8 {
        uint8_t r, g, b, a;
    };
    
    void premulAlpha( Pixel_RGBA8 *pixels, int w, int h )
    {
        auto mul = []( uint8_t a, uint8_t b ) -> uint8_t {
            return (uint8_t) (((unsigned)a * (unsigned)b) >> 8);
        };
        for (int i=0; i < (w*h); ++i) {
            pixels[i].r = mul(pixels[i].r, pixels[i].a);
            pixels[i].g = mul(pixels[i].g, pixels[i].a);
            pixels[i].b = mul(pixels[i].b, pixels[i].a);
        }
    }

    PISCES_API void HardwareResourceManager::uploadTexture2D( TextureHandle texture, int mipmap, TextureUploadFlags flags, PixelFormat format, const void *data )
    {
        TextureInfo *info = mImpl->textures.find(texture);
        if (!info) return;
        if (info->type != TextureType::Texture2D) {
            LOG_WARNING("Wrong texture type %i for uploadTexture2D to texture %i", (int)info->type, (int)texture);
            return;
        }

        GLenum symbolicFormat = SymbolicPixelFormat(format);
        GLenum pixelType = PixelType(format);
        
        glBindTexture(GL_TEXTURE_2D, info->glTexture);
        
        if (all(flags, TextureUploadFlags::PreMultiplyAlpha) && PixelFormatHasAlpha(format) == false) {
            flags = clear(flags, TextureUploadFlags::PreMultiplyAlpha);
            LOG_WARNING("Invalid TextureUploadFlags PreMultiplyAlpha - format don't have a alpha channel");
        }

        if (any(flags, TextureUploadFlags::PreMultiplyAlpha | TextureUploadFlags::FlipVerticaly)) {

            int w = info->size.x,
                h = info->size.y;

            switch(format) {
            case PixelFormat::R8: {
                std::vector<Pixel_R8> tmp((Pixel_R8*)data, ((Pixel_R8*)data) + w*h);
                flipTexture(tmp.data(), w, h); 
                glTexSubImage2D(GL_TEXTURE_2D, mipmap, 0, 0, info->size.x, info->size.y, symbolicFormat, pixelType, tmp.data());
              } break;
            case PixelFormat::RG8: {
                std::vector<Pixel_RG8> tmp((Pixel_RG8*)data, ((Pixel_RG8*)data) + w*h);
                flipTexture(tmp.data(), w, h); 
                glTexSubImage2D(GL_TEXTURE_2D, mipmap, 0, 0, info->size.x, info->size.y, symbolicFormat, pixelType, tmp.data());
              } break;
            case PixelFormat::RGB8: {
                std::vector<Pixel_RGB8> tmp((Pixel_RGB8*)data, ((Pixel_RGB8*)data) + w*h);
                flipTexture(tmp.data(), w, h); 
                glTexSubImage2D(GL_TEXTURE_2D, mipmap, 0, 0, info->size.x, info->size.y, symbolicFormat, pixelType, tmp.data());
              } break;
            case PixelFormat::RGBA8: {
                std::vector<Pixel_RGBA8> tmp((Pixel_RGBA8*)data, ((Pixel_RGBA8*)data) + w*h);
                if (all(flags, TextureUploadFlags::FlipVerticaly)) {
                    flipTexture(tmp.data(), w, h);
                }
                if (all(flags, TextureUploadFlags::PreMultiplyAlpha)) {
                    premulAlpha(tmp.data(), w, h);
                }
                glTexSubImage2D(GL_TEXTURE_2D, mipmap, 0, 0, info->size.x, info->size.y, symbolicFormat, pixelType, tmp.data());
              } break;
            }

        }
        else {
            glTexSubImage2D(GL_TEXTURE_2D, mipmap, 0, 0, info->size.x, info->size.y, symbolicFormat, pixelType, data);
        }

        if (all(flags, TextureUploadFlags::GenerateMipmaps)) {
            assert(mipmap == 0);
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    PISCES_API void HardwareResourceManager::uploadCubemap( TextureHandle texture, CubemapFace face, int mipmap, TextureUploadFlags flags, PixelFormat format, const void *data )
    {
        TextureInfo *info = mImpl->textures.find(texture);
        if (!info) return;
        if (info->type != TextureType::Cubemap) {
            LOG_WARNING("Wrong texture type %i for uploadCubemap to texture %i", (int)info->type, (int)texture);
            return;
        }


        GLenum symbolicFormat = SymbolicPixelFormat(format);
        GLenum pixelType = PixelType(format);

        glBindTexture(GL_TEXTURE_CUBE_MAP, info->glTexture);
        GLenum target = CubemapFaceToTarget(face);
        glTexSubImage2D(target, mipmap, 0, 0, info->size.x, info->size.y, symbolicFormat, pixelType, data);

        if (all(flags, TextureUploadFlags::GenerateMipmaps)) {
            LOG_WARNING("Unsupported to generate mipmaps for cubemaps :/");
        }
    }

    PISCES_API void HardwareResourceManager::uploadBuffer( BufferHandle buffer, size_t offset, size_t size, const void * data )
    {
        BufferInfo *info = mImpl->buffers.find(buffer);
        if (info == nullptr) return;

        GLenum target = BufferTarget(info->type);
        glBindBuffer(target, info->glBuffer);
        glBufferSubData(target, offset, size, data);
    }

    PISCES_API void HardwareResourceManager::setSwizzleMask( TextureHandle texture, SwizzleMask red, SwizzleMask green, SwizzleMask blue, SwizzleMask alpha )
    {
        TextureInfo *info = mImpl->textures.find(texture);
        if (!info) return;

        glBindTexture(GL_TEXTURE_2D, info->glTexture);

        GLenum swizzle[4] = {
            ToGL(red), ToGL(green), ToGL(blue), ToGL(alpha)
        };
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle);
    }

    PISCES_API TextureHandle HardwareResourceManager::createSampler( TextureHandle texture, const SamplerParams &params )
    {
        GLSampler sampler;
        glGenSamplers(1, &sampler.handle);

        setRealSamplerParams(sampler, params);

        TextureHandle realTexture;
        if (TextureHandleVector::IsHandleFromThis(texture)) {
            realTexture = texture;
        }
        else if (SamplerHandleVector::IsHandleFromThis(texture)) {
            SamplerInfo *info = mImpl->samplers.find(texture);
            if (info) {
                realTexture = info->texture;
            }
        }

        if (realTexture) {
            SamplerInfo info;
                info.glSampler = std::move(sampler);
                info.texture = texture;
            return mImpl->samplers.create(std::move(info));
        }
        return {};
    }

    PISCES_API void HardwareResourceManager::setSamplerParams( TextureHandle texture, const SamplerParams &params )
    {
        if (TextureHandleVector::IsHandleFromThis(texture)) {
            TextureInfo *info = mImpl->textures.find(texture);
            
            if (!info) return;

            GLenum target = TextureTarget(info->type);
            setRealSamplerParamsTexture(target, texture, params);
        }
        else if(SamplerHandleVector::IsHandleFromThis(texture)) {
            SamplerInfo *info = mImpl->samplers.find(texture);

            if (!info) return;

            setRealSamplerParams(info->glSampler, params);
        }
    }

    PISCES_API VertexArrayHandle HardwareResourceManager::createVertexArray( const VertexAttribute *attributes, int attributeCount, 
                                                                             const BufferHandle *sourceBuffers, int sourceCount, 
                                                                             BufferHandle indexBuffer, IndexType indexType )
    {
        GLVertexArray vertexArray;
        glGenVertexArrays(1, &vertexArray.handle);
        glBindVertexArray(vertexArray);

        for (int i=0; i < attributeCount; ++i) {
            const VertexAttribute &attribute = attributes[i];
            
            GLenum type = ToGL(attribute.type);
            GLboolean normalized = IsNormalized(attribute.type);

            BufferInfo *buffer = mImpl->buffers.find(sourceBuffers[attribute.source]);
            if (!buffer) continue;

            glBindBuffer(GL_ARRAY_BUFFER, buffer->glBuffer);
            glEnableVertexAttribArray(i);
            glVertexAttribPointer(i, attribute.count, type, normalized, attribute.stride, reinterpret_cast<void*>((uintptr_t)attribute.offset));
        }

        VertexArrayInfo info;
            info.glVertexArray = std::move(vertexArray);
            info.indexBuffer = indexBuffer;
            info.indexType = indexType;

        return mImpl->vertexArrays.create(std::move(info));
    }

    PISCES_API void HardwareResourceManager::deleteVertexArray( VertexArrayHandle vertexArray )
    {
        mImpl->vertexArrays.free(vertexArray);
    }

    PISCES_API TextureHandle HardwareResourceManager::loadTexture2D( PixelFormat format, TextureFlags flags, const char *filename )
    {
        int width, height, channels;

        int formatChannels = ChannelsInFormat(format);
        stbi_image image(stbi_load( filename, &width, &height, &channels, formatChannels));

        if (!image) {
            LOG_WARNING("Failed to load image %s: %s", filename, stbi_failure_reason());
        }

        TextureHandle texture = allocateTexture2D(format, flags, width, height);
        uploadTexture2D(texture, 0, TextureUploadFlags::GenerateMipmaps, PixelformatFromChannelCount(formatChannels), image);

        return texture;
    }

    PISCES_API TextureHandle HardwareResourceManager::loadCubemap( PixelFormat format, TextureFlags flags, const char *filetemplate )
    {
        stbi_image cubemap[6];

        const char* names[6] = {
            "pos_x", "neg_x",
            "pos_y", "neg_y",
            "pos_z", "neg_z",
        };
        CubemapFace faces[6] = {
            CubemapFace::PosX, CubemapFace::NegX,
            CubemapFace::PosY, CubemapFace::NegY,
            CubemapFace::PosZ, CubemapFace::NegZ,
        };

        const int MAX_TEMPLATE_LEN = 256;
        size_t len = strlen(filetemplate);
        if (len > MAX_TEMPLATE_LEN) {
            LOG_WARNING ("Filetemplate is to long. Max lenght is %i, was %i. Template: %s", MAX_TEMPLATE_LEN, len, filetemplate);
            return TextureHandle();
        }

        const char *delem = strstr(filetemplate, "%FACE%");
        if (delem == nullptr) {
            LOG_WARNING ("Filetemplate is missing indicator \"%%FACE%%\". Template %s", filetemplate);
            return TextureHandle();
        }
        size_t delemPos = delem - filetemplate,
               delemEnd = delem+sizeof("%FACE%")-1-filetemplate;

        int formatChannels = ChannelsInFormat(format);

        int size = 0;

        for (int i=0; i < 6; ++i) {
            char filename[MAX_TEMPLATE_LEN+10] = {};

#include "compiler_warnings/push.h"
#include "compiler_warnings/ignore_unsafe_function_crt.h"
            strncat(filename, filetemplate, delemPos); 
            strcat(filename, names[i]);
            strcat(filename, filetemplate+delemEnd);
#include "compiler_warnings/pop.h"

            int width, height, channels;
            cubemap[i] = stbi_image(stbi_load(filename, &width, &height, &channels, formatChannels));

            if (!cubemap[i]) {
                LOG_WARNING ("Failed to load image \"%s\" for cubemap face %s, from template \"%s\": %s", filename, names[i], filetemplate, stbi_failure_reason());
                return TextureHandle();
            }

            if (size != 0 && (width != size || height != size) || width != height) {
                LOG_WARNING ("Unespected image dimensions for image \"%s\" expected a size of %ix%i got %ix%i", filename, size, size, width, height);
                return TextureHandle();
            }

            size = width;
        }

        TextureHandle texture = allocateCubemap(format, flags, size);
        
        if (texture) {
            for (int i=0; i < 6; ++i) {
                uploadCubemap(texture, faces[i], 0, TextureUploadFlags::None, format, cubemap[i]);
            }
            
            TextureInfo *info = mImpl->textures.find(texture);
            assert (info != nullptr);
            assert (info->type == TextureType::Cubemap);

            glBindTexture(GL_TEXTURE_CUBE_MAP, info->glTexture);
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        }

        return texture;
    }

    PISCES_API void HardwareResourceManager::setTextureName( TextureHandle texture, Common::StringId name )
    {
        if (TextureHandleVector::IsHandleFromThis(texture)) {
            TextureInfo *info = mImpl->textures.find(texture);
            
            if (!info) return;
            info->name = name;
        }
        else if(SamplerHandleVector::IsHandleFromThis(texture)) {
            SamplerInfo *info = mImpl->samplers.find(texture);

            if (!info) return;
            info->name = name;
        }
    }

    PISCES_API TextureHandle HardwareResourceManager::findTextureByName( Common::StringId name )
    {
        TextureHandle texture = mImpl->samplers.findIf(
            [name] (const SamplerInfo &info) {
                return info.name == name;
            }
        );
        if (texture) return texture;

        texture = mImpl->textures.findIf(
            [name] (const TextureInfo &info) {
                return info.name == name;
            }
        );
        if (texture) return texture;

        return TextureHandle();
    }

    PISCES_API TextureHandle HardwareResourceManager::getMissingTexture()
    {
        return mImpl->missingTexture;
    }

    PISCES_API void* HardwareResourceManager::mapBuffer( BufferHandle buffer, size_t offset, size_t size, BufferMapFlags flags )
    {
        BufferInfo *info = mImpl->buffers.find(buffer);
        if (info == nullptr) return nullptr;

        if ((offset+size) > info->size) {
            LOG_WARNING("Invalid mapping interval [%zd, %zd] the buffer (%i) has a size of %zd", offset, offset+size, (int)buffer, info->size);
            return nullptr;
        }

        GLenum target = BufferTarget(info->type);
        glBindBuffer(target, info->glBuffer);
        return GLCompat::MapBuffer(target, offset, size, flags, info->size, info->persistentMapping);
    }

    PISCES_API bool HardwareResourceManager::unmapBuffer( BufferHandle buffer, bool keepPersistent )
    {
        BufferInfo *info = mImpl->buffers.find(buffer);
        if (info == nullptr) return false;

        GLenum target = BufferTarget(info->type);
        glBindBuffer(target, info->glBuffer);
        return GLCompat::UnMapBuffer(target, keepPersistent, info->persistentMapping);
    }

    PISCES_API UniformBufferHandle HardwareResourceManager::allocateStaticUniform( size_t size, const void *data )
    {
        size_t alignment = mImpl->uniformBlockAllignment;
        // Adjust size to the next multiple of aligment
        size_t alignedSize = ((size+alignment-1) / alignment) * alignment;

        size_t requiredSize = mImpl->staticUniforms.nextOffset+alignedSize;

        if (mImpl->staticUniforms.size < requiredSize) {
            size_t newSize = std::max( (size_t)MIN_UNIFORM_BLOCK_BUFFER_SIZE, 2*requiredSize);

            if (mImpl->staticUniforms.buffer) {
                resizeBuffer(mImpl->staticUniforms.buffer, newSize, BufferResizeFlags::KeepRange, 0, mImpl->staticUniforms.nextOffset);
            }
            else {
                mImpl->staticUniforms.buffer = allocateBuffer(BufferType::Uniform, BufferUsage::Static, BufferFlags::Upload, newSize, nullptr);;
            }

            if (!mImpl->staticUniforms.buffer) {   
                return UniformBufferHandle();
            }
            mImpl->staticUniforms.size = newSize;
        }

        UniformBufferHandle handle;
            handle.buffer = mImpl->staticUniforms.buffer;
            handle.offset = mImpl->staticUniforms.nextOffset;
            handle.size = size;

        mImpl->staticUniforms.nextOffset += alignedSize;

        uploadBuffer(handle.buffer, handle.offset, size, data);
        return handle;
    }

    PISCES_API void HardwareResourceManager::updateStaticUniform( UniformBufferHandle uniform, size_t size, const void *data )
    {
        if (!uniform) return;

        if (uniform.buffer != mImpl->staticUniforms.buffer) {
            LOG_WARNING("Trying to update uniform that wasn't created by allocateStaticUniform!");
            return;
        }

        if (uniform.size != size) {
            LOG_WARNING("Trying to update uniform that was created with a different size (original: %zu, updated: %zu)", uniform.size, size);
            return;
        }

        uploadBuffer(uniform.buffer, uniform.offset, size, data);
    }

    PISCES_API void HardwareResourceManager::copyBuffer( BufferHandle target, size_t targetOffset, BufferHandle source, size_t sourceOffset, size_t size )
    {
        BufferInfo *tgt = mImpl->buffers.find(target);
        BufferInfo *src = mImpl->buffers.find(source);

        if (!(tgt && src)) return;

        glBindBuffer(GL_COPY_READ_BUFFER, src->glBuffer);
        glBindBuffer(GL_COPY_WRITE_BUFFER, tgt->glBuffer);

        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, sourceOffset, targetOffset, size);
    }

    PISCES_API size_t HardwareResourceManager::getUniformAlignment()
    {
        return mImpl->uniformBlockAllignment;
    }

    PISCES_API void HardwareResourceManager::resizeBuffer( BufferHandle buffer, size_t newSize, BufferResizeFlags flags, size_t sourceOffset, size_t targetOffset, size_t size )
    {
        BufferInfo *info = mImpl->buffers.find(buffer);

        if (!info) return;

        GLBuffer newBuffer;
        glGenBuffers(1, &newBuffer.handle);
        
        GLenum target = BufferTarget(info->type);
        glBindBuffer(target, newBuffer);
        GLCompat::BufferStorage(target, info->usage, info->flags, newSize, nullptr);

        if (all(flags, BufferResizeFlags::KeepRange)) {
            glBindBuffer(GL_COPY_WRITE_BUFFER, newBuffer);
            glBindBuffer(GL_COPY_READ_BUFFER, info->glBuffer);

            glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, sourceOffset, targetOffset, size);
        }

        info->glBuffer = std::move(newBuffer);
        info->size = newSize;
    }

    void loadBuiltinTypes( HardwareResourceManager *hardwareMgr )
    {
        Impl *impl = hardwareMgr->impl();

        std::vector<BuiltinVertex> vertexes;
        std::vector<uint16_t> indexes;

        for (int i=0; i < BUILTIN_OBJECT_COUNT; ++i) {
            impl->builtinDrawInfo[i].primitive = BuiltinObjectInfos[i].primitive;
            impl->builtinDrawInfo[i].first = indexes.size();
            impl->builtinDrawInfo[i].count = BuiltinObjectInfos[i].indexCount;
            impl->builtinDrawInfo[i].base = vertexes.size();

            vertexes.insert(vertexes.end(), BuiltinObjectInfos[i].vertexes, BuiltinObjectInfos[i].vertexes+BuiltinObjectInfos[i].vertexCount);
            indexes.insert(indexes.end(), BuiltinObjectInfos[i].indexes, BuiltinObjectInfos[i].indexes+BuiltinObjectInfos[i].indexCount);
        }

        BufferHandle vertexBuffer = hardwareMgr->allocateBuffer(BufferType::Vertex, BufferUsage::Static, BufferFlags::None, sizeof(BuiltinVertex)*vertexes.size(), vertexes.data());
        BufferHandle indexBuffer = hardwareMgr->allocateBuffer(BufferType::Index, BufferUsage::Static, BufferFlags::None, sizeof(uint16_t)*indexes.size(), indexes.data());
    
        VertexArrayHandle vertexArray = hardwareMgr->createVertexArray(BUILTIN_VERTEX_LAYOUT, BUILTIN_VERTEX_LAYOUT_SIZE, &vertexBuffer, 1, indexBuffer, IndexType::UInt16);

        for (int i=0; i < BUILTIN_OBJECT_COUNT; ++i) {
            impl->builtinDrawInfo[i].vertexArray = vertexArray;
        }
    }
}
