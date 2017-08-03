#pragma once

#include "../Fwd.h"

#include <glbinding/gl/types.h>

namespace Pisces
{
    namespace GLCompat 
    {
        struct BufferPersistentMapping {
            void *data = nullptr;
            size_t offset, size;
        };

        extern void  (*BindTexture)( int slot, gl::GLenum target, gl::GLuint texture );
        extern void  (*TexStorage2D)( gl::GLenum target, int levels, gl::GLenum internalFormat, gl::GLsizei width, gl::GLsizei height );
        extern void  (*TexStorageCube)( gl::GLenum target, int levels, gl::GLenum internalFormat, gl::GLsizei size );
        extern void  (*BufferStorage)( gl::GLenum target, BufferUsage usage, BufferFlags flags, size_t size, const void *data );
        extern void* (*MapBuffer)( gl::GLenum target, size_t offset, size_t size, BufferMapFlags flags, size_t bufferSize, BufferPersistentMapping &mapping );
        extern bool  (*UnMapBuffer)( gl::GLenum target, bool keepPersistent, BufferPersistentMapping &mapping );

        void InitCompat( bool enableExtensions );
    };
}