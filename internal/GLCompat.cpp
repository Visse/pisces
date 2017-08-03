#pragma once

#include "GLCompat.h"

#include "Common/ErrorUtils.h"
#include "Common/PointerHelpers.h"

#include <glbinding/gl/gl.h>

#include <glbinding/gl/extension.h>

#include <glbinding/ContextInfo.h>
#include <glbinding/Meta.h>

using namespace gl;

namespace Pisces
{
    namespace GLCompat {

        void  (*BindTexture)( int slot, gl::GLenum target, gl::GLuint texture );
        void  (*TexStorage2D)( gl::GLenum target, int levels, gl::GLenum internalFormat, gl::GLsizei width, gl::GLsizei height );
        void  (*TexStorageCube)( gl::GLenum target, int levels, gl::GLenum internalFormat, gl::GLsizei size );
        void  (*BufferStorage)( gl::GLenum target, BufferUsage usage, BufferFlags flags, size_t size, const void *data );

        void* (*MapBuffer)( gl::GLenum target, size_t offset, size_t size, BufferMapFlags flags, size_t bufferSize, BufferPersistentMapping &mapping );
        bool  (*UnMapBuffer)( gl::GLenum target, bool keepPersistent, BufferPersistentMapping &mapping );



        void BindTexture_NoUnit( int slot, gl::GLenum target, gl::GLuint texture )
        {
            glActiveTexture(GL_TEXTURE0 + slot);
            glBindTexture(target, texture);
        }
        void BindTexture_Unit  ( int slot, gl::GLenum target, gl::GLuint texture )
        {
            glBindTextureUnit(slot, texture);
        }

        void TexStorage2D_NoStorage( gl::GLenum target, int levels, gl::GLenum internalFormat, gl::GLsizei width, gl::GLsizei height )
        {
            for (int i = 0; i < levels; i++) {
                glTexImage2D(target, i, internalFormat, width, height, 0, GL_RED, GL_BYTE, NULL);
                width = std::max(1, (width / 2));
                height = std::max(1, (height / 2));
            }
        }
        void TexStorage2D_Storage  ( gl::GLenum target, int levels, gl::GLenum internalFormat, gl::GLsizei width, gl::GLsizei height )
        {
            glTexStorage2D(target, levels, internalFormat, width, height);
        }

        void TexStorageCube_NoStorage( gl::GLenum target, int levels, gl::GLenum internalFormat, gl::GLsizei size )
        {
            GLenum faces[6] = {
                GL_TEXTURE_CUBE_MAP_POSITIVE_X, GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                GL_TEXTURE_CUBE_MAP_POSITIVE_Y, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                GL_TEXTURE_CUBE_MAP_POSITIVE_Z, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
            };

            for (auto face : faces) {
                TexStorage2D_NoStorage(face, levels, internalFormat, size, size);
            }
        }
        void TexStorageCube_Storage  ( gl::GLenum target, int levels, gl::GLenum internalFormat, gl::GLsizei size )
        {
            TexStorage2D_Storage(target, levels, internalFormat, size, size);
        }

        void BufferStorage_NoStorage( gl::GLenum target, BufferUsage usage, BufferFlags flags, size_t size, const void *data )
        {
            GLenum buffUsage;
            switch(usage) {
            case BufferUsage::Static:
                buffUsage = GL_STATIC_DRAW;
                break;
            case BufferUsage::StreamWrite:
                buffUsage = GL_STREAM_DRAW;
                break;
            default:
                FATAL_ERROR("Unknown BufferUsage %i", (int)usage);
            }

            glBufferData( target, size, data, buffUsage );
        }
        void BufferStorage_Storage  ( gl::GLenum target, BufferUsage usage, BufferFlags flags, size_t size, const void *data )
        {
            gl::BufferStorageMask buffFlags = GL_NONE_BIT;

            if (all(flags, BufferFlags::MapRead)) {
                buffFlags = buffFlags | GL_MAP_READ_BIT;
            }
            if (all(flags, BufferFlags::MapWrite)) {
                buffFlags = buffFlags | GL_MAP_WRITE_BIT;
            }
            if (all(flags, BufferFlags::MapPersistent)) {
                buffFlags = buffFlags | GL_MAP_PERSISTENT_BIT;
            }
            if (all(flags, BufferFlags::MapPersistent)) {
                buffFlags = buffFlags | GL_MAP_PERSISTENT_BIT;
            }
            if (all(flags, BufferFlags::Upload)) {
                buffFlags = buffFlags | GL_DYNAMIC_STORAGE_BIT;
            }

            glBufferStorage( target, size, data, buffFlags );
        }

        void* MapBuffer_NoStorage( gl::GLenum target, size_t offset, size_t size, BufferMapFlags flags, size_t bufferSize, BufferPersistentMapping &mapping )
        {
            gl::BufferAccessMask mask = GL_NONE_BIT;
            if (all(flags, BufferMapFlags::MapRead)) mask |= GL_MAP_READ_BIT;
            if (all(flags, BufferMapFlags::MapWrite)) mask |= GL_MAP_WRITE_BIT;
            if (all(flags, BufferMapFlags::DiscardRange)) mask |= GL_MAP_INVALIDATE_RANGE_BIT;
            if (all(flags, BufferMapFlags::DiscardBuffer)) mask |= GL_MAP_INVALIDATE_BUFFER_BIT;

            return glMapBufferRange(target, offset, size, mask);
        }
        void* MapBuffer_Storage  ( gl::GLenum target, size_t offset, size_t size, BufferMapFlags flags, size_t bufferSize, BufferPersistentMapping &mapping )
        {
            if (all(flags, BufferMapFlags::Persistent) && mapping.data != nullptr) {
                return Common::advance(mapping.data, offset);
            }

            gl::BufferAccessMask mask = GL_NONE_BIT;
            if (all(flags, BufferMapFlags::MapRead)) mask |= GL_MAP_READ_BIT;
            if (all(flags, BufferMapFlags::MapWrite)) mask |= GL_MAP_WRITE_BIT;
            if (all(flags, BufferMapFlags::DiscardRange) && none(flags,BufferMapFlags::Persistent)) mask |= GL_MAP_INVALIDATE_RANGE_BIT;
            if (all(flags, BufferMapFlags::DiscardBuffer)) mask |= GL_MAP_INVALIDATE_BUFFER_BIT;
            if (all(flags, BufferMapFlags::Persistent)) mask |= GL_MAP_PERSISTENT_BIT;

            if (all(flags,BufferMapFlags::Persistent)) {
                mapping.data = glMapBufferRange(target, 0, bufferSize, mask | GL_MAP_FLUSH_EXPLICIT_BIT);
                mapping.offset = offset;
                mapping.size = size;
                return Common::advance(mapping.data, offset);
            }

            return glMapBufferRange(target, offset, size, mask);
        }

        bool UnMapBuffer_NoStorage( gl::GLenum target, bool keepPersistent, BufferPersistentMapping &mapping )
        {
            glUnmapBuffer(target);
            return false;
        }
        bool UnMapBuffer_Storage  ( gl::GLenum target, bool keepPersistent, BufferPersistentMapping &mapping )
        {
            if (keepPersistent) {
                glFlushMappedBufferRange(target, mapping.offset, mapping.size);
                return true;
            }
            glUnmapBuffer(target);
            mapping.data = nullptr;
            return false;
        }
        
        void InitCompat( bool enableExtensions )
        {
            using ContextInfo = glbinding::ContextInfo;
            using Meta = glbinding::Meta;

            if (enableExtensions && ContextInfo::supported(Meta::extensions("glBindTextureUnit"))) {
                BindTexture = BindTexture_Unit;
                LOG_INFORMATION("Context supports glBindTextureUnit");
            }
            else {
                BindTexture = BindTexture_NoUnit;
                LOG_INFORMATION("Context missing supports for glBindTextureUnit");
            }

            if (enableExtensions && ContextInfo::supported(Meta::extensions("glTexStorage2D"))) {
                TexStorage2D = TexStorage2D_Storage;
                TexStorageCube = TexStorageCube_Storage;
                LOG_INFORMATION("Context supports glTexStorage2D");
            }
            else {
                TexStorage2D = TexStorage2D_NoStorage;
                TexStorageCube = TexStorageCube_NoStorage;
                LOG_INFORMATION("Context missing supports for glTexStorage2D");
            }

            if (enableExtensions && ContextInfo::supported(Meta::extensions("glBufferStorage"))) {
                BufferStorage = BufferStorage_Storage;
                MapBuffer = MapBuffer_Storage;
                UnMapBuffer = UnMapBuffer_Storage;
                LOG_INFORMATION("Context supports glBufferStorage");
            }
            else {
                BufferStorage = BufferStorage_NoStorage;
                MapBuffer = MapBuffer_NoStorage;
                UnMapBuffer = UnMapBuffer_NoStorage;
                LOG_INFORMATION("Context missing supports for glBufferStorage");
            }
        }
    };
}