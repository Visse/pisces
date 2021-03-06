#pragma once

#include "../Fwd.h"
#include "Common/ErrorUtils.h"

#include <glbinding/gl/enum.h>
#include <glbinding/Meta.h>

namespace Pisces
{
    inline int MipmapsForSize( int width, int height=0, int depth=0) 
    {
        int size = width;

        if (size < height) size = height;
        if (size < depth) size = depth;

        int mipmaps = 1;
        for (; (1<<mipmaps) < size; ++mipmaps);
        return mipmaps;
    }


    inline gl::GLenum InternalPixelFormat( PixelFormat format )
    {
        switch (format) {
        case PixelFormat::R8:
            return gl::GL_R8;
        case PixelFormat::RG8:
            return gl::GL_RG8;
        case PixelFormat::RGB8:
            return gl::GL_RGB8;
        case PixelFormat::RGBA8:
            return gl::GL_RGBA8;
        }

        FATAL_ERROR("Unknown PixelFormat %i", (int)format);
    }

    inline gl::GLenum SymbolicPixelFormat( PixelFormat format )
    {
        switch (format) {
        case PixelFormat::R8:
            return gl::GL_RED;
        case PixelFormat::RG8:
            return gl::GL_RG;
        case PixelFormat::RGB8:
            return gl::GL_RGB;
        case PixelFormat::RGBA8:
            return gl::GL_RGBA;
        }

        FATAL_ERROR("Unknown PixelFormat %i", (int)format);
    }

    inline gl::GLenum PixelType( PixelFormat format ) 
    {
        switch (format) {
        case PixelFormat::R8:
        case PixelFormat::RG8:
        case PixelFormat::RGB8:
        case PixelFormat::RGBA8:
            return gl::GL_UNSIGNED_BYTE;
        }

        FATAL_ERROR("Unknown PixelFormat %i", (int)format);
    }

    inline int PixelFormatSize( PixelFormat format ) 
    {
        switch (format) {
        case PixelFormat::R8:
            return 1;
        case PixelFormat::RG8:
            return 2;
        case PixelFormat::RGB8:
            return 3;
        case PixelFormat::RGBA8:
            return 4;
        }

        FATAL_ERROR("Unknown PixelFormat %i", (int)format);
    }

    inline bool PixelFormatHasAlpha( PixelFormat format ) 
    {
        switch (format) {
        case PixelFormat::R8:
        case PixelFormat::RG8:
        case PixelFormat::RGB8:
            return false;
        case PixelFormat::RGBA8:
            return true;
        }

        FATAL_ERROR("Unknown PixelFormat %i", (int)format);
    }


    inline size_t expectedMemoryUseTexture2D( PixelFormat format, int width, int height, int mipmaps )
    {
        size_t total = 0;
        for (int i=0; i < mipmaps; ++i) {
            switch (format) {
            case PixelFormat::R8:
                total += 1 * width * height;
                break;
            case PixelFormat::RG8:
                total += 2 * width * height;
                break;
            case PixelFormat::RGB8:
                total += 3 * width * height;
                break;
            case PixelFormat::RGBA8:
                total += 4 * width * height;
                break;
            default:
                FATAL_ERROR("Unknown PixelFormat %i", (int)format);
            }
            width = std::max(1, width/2);
            height = std::max(1, height/2);
        }
        return total;
    }

    inline size_t expectedMemoryUseCubemap( PixelFormat format, int size, int mipmaps )
    {
       return expectedMemoryUseTexture2D(format, size, size, mipmaps) * 6;
    }

    inline gl::GLenum BufferTarget( BufferType type )
    {
        switch (type) {
        case BufferType::Vertex:
            return gl::GL_ARRAY_BUFFER;
        case BufferType::Index:
            return gl::GL_ELEMENT_ARRAY_BUFFER;
        case BufferType::Uniform:
            return gl::GL_UNIFORM_BUFFER;
        }
        FATAL_ERROR("Unknown BufferType %i", (int)type);
    }

    inline gl::GLenum ToGL( SwizzleMask mask ) {
        switch (mask) {
        case SwizzleMask::Red:
            return gl::GL_RED;
        case SwizzleMask::Green:
            return gl::GL_GREEN;
        case SwizzleMask::Blue:
            return gl::GL_BLUE;
        case SwizzleMask::Alpha:
            return gl::GL_ALPHA;
        case SwizzleMask::One:
            return gl::GL_ONE;
        case SwizzleMask::Zero:
            return gl::GL_ZERO;
        }
        FATAL_ERROR("Unknown SwizzleMask %i", (int)mask);
    }

    inline gl::GLenum ToGL( SamplerMinFilter filter ) {
        switch (filter) {
        case SamplerMinFilter::Nearest:
            return gl::GL_NEAREST;
        case SamplerMinFilter::Linear:
            return gl::GL_LINEAR;
        case SamplerMinFilter::NearestMipmapNearest:
            return gl::GL_NEAREST_MIPMAP_NEAREST;
        case SamplerMinFilter::NearestMipmapLinear:
            return gl::GL_NEAREST_MIPMAP_LINEAR;
        case SamplerMinFilter::LinearMipmapNearest:
            return gl::GL_LINEAR_MIPMAP_NEAREST;
        case SamplerMinFilter::LinearMipmapLinear:
            return gl::GL_LINEAR_MIPMAP_LINEAR;
        }
        FATAL_ERROR("Unknown SamplerMinFilter %i", (int)filter);
    }

    inline gl::GLenum ToGL( SamplerMagFilter filter ) {
        switch (filter) {
        case SamplerMagFilter::Nearest:
            return gl::GL_NEAREST;
        case SamplerMagFilter::Linear:
            return gl::GL_LINEAR;
        }
        FATAL_ERROR("Unknown SamplerMagFilter %i", (int)filter);
    }

    inline gl::GLenum ToGL( EdgeSamplingMode mode ) {
        switch (mode) {
        case EdgeSamplingMode::Repeat:
            return gl::GL_REPEAT;
        case EdgeSamplingMode::MirroredRepeat:
            return gl::GL_MIRRORED_REPEAT;
        case EdgeSamplingMode::ClampToEdge:
            return gl::GL_CLAMP_TO_EDGE;
        case EdgeSamplingMode::ClampToBorder:
            return gl::GL_CLAMP_TO_BORDER;
        }
        FATAL_ERROR("Unknown EdgeSamplingMode %i", (int)mode);
    }

    inline gl::GLenum ToGL( VertexAttributeType type ) {
        switch (type) {
        case VertexAttributeType::Int8:
        case VertexAttributeType::IInt8:
        case VertexAttributeType::NormInt8:
            return gl::GL_BYTE;
        case VertexAttributeType::Int16:
        case VertexAttributeType::IInt16:
        case VertexAttributeType::NormInt16:
            return gl::GL_SHORT;
        case VertexAttributeType::Int32:
        case VertexAttributeType::IInt32:
        case VertexAttributeType::NormInt32:
            return gl::GL_INT;
        case VertexAttributeType::UInt8:
        case VertexAttributeType::IUInt8:
        case VertexAttributeType::NormUInt8:
            return gl::GL_UNSIGNED_BYTE;
        case VertexAttributeType::UInt16:
        case VertexAttributeType::IUInt16:
        case VertexAttributeType::NormUInt16:
            return gl::GL_UNSIGNED_SHORT;
        case VertexAttributeType::UInt32:
        case VertexAttributeType::IUInt32:
        case VertexAttributeType::NormUInt32:
            return gl::GL_UNSIGNED_INT;
        case VertexAttributeType::NormInt3x10_1x2:
            return gl::GL_INT_2_10_10_10_REV;
        case VertexAttributeType::NormUInt3x10_1x2:
            return gl::GL_UNSIGNED_INT_2_10_10_10_REV;
        case VertexAttributeType::Float16:
            return gl::GL_HALF_FLOAT;
        case VertexAttributeType::Float32:
            return gl::GL_FLOAT;
        }
        FATAL_ERROR("Unknown VertexAttributeType %i", (int)type);
    }

    inline gl::GLboolean IsNormalized( VertexAttributeType type ) {
        switch (type) {
        case VertexAttributeType::Int8:
        case VertexAttributeType::Int16:
        case VertexAttributeType::Int32:
        case VertexAttributeType::UInt8:
        case VertexAttributeType::UInt16:
        case VertexAttributeType::UInt32:
        case VertexAttributeType::IInt8:
        case VertexAttributeType::IInt16:
        case VertexAttributeType::IInt32:
        case VertexAttributeType::IUInt8:
        case VertexAttributeType::IUInt16:
        case VertexAttributeType::IUInt32:
        case VertexAttributeType::Float16:
        case VertexAttributeType::Float32:
            return gl::GL_FALSE;
        case VertexAttributeType::NormInt8:
        case VertexAttributeType::NormInt16:
        case VertexAttributeType::NormInt32:
        case VertexAttributeType::NormUInt8:
        case VertexAttributeType::NormUInt16:
        case VertexAttributeType::NormUInt32:
        case VertexAttributeType::NormInt3x10_1x2:
        case VertexAttributeType::NormUInt3x10_1x2:
            return gl::GL_TRUE;
        }
        FATAL_ERROR("Unknown VertexAttributeType %i", (int)type);
    }

    inline gl::GLboolean IsIntegear( VertexAttributeType type )
    {
        switch (type) {
        case VertexAttributeType::IInt8:
        case VertexAttributeType::IInt16:
        case VertexAttributeType::IInt32:
        case VertexAttributeType::IUInt8:
        case VertexAttributeType::IUInt16:
        case VertexAttributeType::IUInt32:
            return gl::GL_TRUE;
        case VertexAttributeType::Int8:
        case VertexAttributeType::Int16:
        case VertexAttributeType::Int32:
        case VertexAttributeType::UInt8:
        case VertexAttributeType::UInt16:
        case VertexAttributeType::UInt32:
        case VertexAttributeType::Float16:
        case VertexAttributeType::Float32:
        case VertexAttributeType::NormInt8:
        case VertexAttributeType::NormInt16:
        case VertexAttributeType::NormInt32:
        case VertexAttributeType::NormUInt8:
        case VertexAttributeType::NormUInt16:
        case VertexAttributeType::NormUInt32:
        case VertexAttributeType::NormInt3x10_1x2:
        case VertexAttributeType::NormUInt3x10_1x2:
            return gl::GL_FALSE;
        }
        FATAL_ERROR("Unknown VertexAttributeType %i", (int)type);
    }

    inline gl::GLenum ToGL( IndexType type ) {
        switch (type) {
        case IndexType::UInt16:
            return gl::GL_UNSIGNED_SHORT;
        case IndexType::UInt32:
            return gl::GL_UNSIGNED_INT;
        }
        FATAL_ERROR("Unknown IndexType %i", (int)type);
    }

    inline gl::GLenum ToGL( Primitive primitive ) {
        switch (primitive) {
        case Primitive::Points:
            return gl::GL_POINTS;
        case Primitive::Lines:
            return gl::GL_LINES;
        case Primitive::Triangles:
            return gl::GL_TRIANGLES;
        }
        FATAL_ERROR("Unknown Primitive %i", (int)primitive);
    }

    inline uintptr_t OffsetForIndexType( IndexType type, uintptr_t index ) {
        switch (type) {
        case IndexType::UInt16:
            return 2 * index;
        case IndexType::UInt32:
            return 4* index;
        }
        FATAL_ERROR("Unknown IndexType %i", (int)type);
    }

    inline gl::GLenum TextureTarget( TextureType type )
    {
        switch (type) {
        case TextureType::Texture2D:
            return gl::GL_TEXTURE_2D;
        case TextureType::Cubemap:
            return gl::GL_TEXTURE_CUBE_MAP;
        }
        FATAL_ERROR("Unknown TextureType %i", (int)type);
    }

    inline gl::GLenum CubemapFaceToTarget( CubemapFace face )
    {
        switch (face) {
        case CubemapFace::PosX:
            return gl::GL_TEXTURE_CUBE_MAP_POSITIVE_X;
        case CubemapFace::NegX:
            return gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
        case CubemapFace::PosY:
            return gl::GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
        case CubemapFace::NegY:
            return gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
        case CubemapFace::PosZ:
            return gl::GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
        case CubemapFace::NegZ:
            return gl::GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
        }
        FATAL_ERROR("Unknown CubemapFace %i", (int)face);
    }

    inline int ChannelsInFormat( PixelFormat format )
    {
        switch (format) {
        case PixelFormat::R8:
            return 1;
        case PixelFormat::RG8:
            return 2;
        case PixelFormat::RGB8:
            return 3;
        case PixelFormat::RGBA8:
            return 4;
        }
        FATAL_ERROR("Unknown PixelFormat %i", (int)format);
    }

    inline PixelFormat PixelformatFromChannelCount( int channelCount )
    {
        switch (channelCount) {
        case 1:
            return PixelFormat::R8;
        case 2:
            return PixelFormat::RG8;
        case 3:
            return PixelFormat::RGB8;
        case 4:
            return PixelFormat::RGBA8;
        }
        FATAL_ERROR("Invalid channelCount %i", channelCount);
    }

    inline gl::GLenum ToGL( ImageTextureAccess access ) {
        switch (access) {
        case ImageTextureAccess::ReadOnly:
            return gl::GL_READ_ONLY;
        case ImageTextureAccess::WriteOnly:
            return gl::GL_WRITE_ONLY;
        case ImageTextureAccess::ReadWrite:
            return gl::GL_READ_WRITE;
        }
        FATAL_ERROR("Unknown ImageTextureAccess %i", (int)access);
    }

    enum class GLType {
        Int, UInt,
        Float,
        Vec2, Vec3, Vec4,
        Mat2x2, Mat2x3, Mat2x4,
        Mat3x2, Mat3x3, Mat3x4,
        Mat4x2, Mat4x3, Mat4x4,
        
        Sampler1D,
        Sampler2D,

        ImageTexture1D,
        ImageTexture2D
    };
    // Sentinel value, not used in any functions
    static const GLType GLTypeNone = GLType(-1);

    inline GLType FromGLType( gl::GLenum type ) {
#include "compiler_warnings/push.h"
#include "compiler_warnings/ignore_unhandled_enum_in_switch.h"
        switch (type) {
        case gl::GL_INT:
            return GLType::Int;
        case gl::GL_UNSIGNED_INT:
            return GLType::UInt;
        case gl::GL_FLOAT:
            return GLType::Float;
        case gl::GL_FLOAT_VEC2:
            return GLType::Vec2;
        case gl::GL_FLOAT_VEC3:
            return GLType::Vec3;
        case gl::GL_FLOAT_VEC4:
            return GLType::Vec4;
        case gl::GL_FLOAT_MAT2:
            return GLType::Mat2x2;
        case gl::GL_FLOAT_MAT2x3:
            return GLType::Mat2x3;
        case gl::GL_FLOAT_MAT2x4:
            return GLType::Mat2x4;
        case gl::GL_FLOAT_MAT3x2:
            return GLType::Mat3x2;
        case gl::GL_FLOAT_MAT3:
            return GLType::Mat3x3;
        case gl::GL_FLOAT_MAT3x4:
            return GLType::Mat3x4;
        case gl::GL_FLOAT_MAT4x2:
            return GLType::Mat4x2;
        case gl::GL_FLOAT_MAT4x3:
            return GLType::Mat4x3;
        case gl::GL_FLOAT_MAT4:
            return GLType::Mat4x4;
        case gl::GL_SAMPLER_1D:
            return GLType::Sampler1D;
        case gl::GL_SAMPLER_2D:
            return GLType::Sampler2D;
        case gl::GL_IMAGE_1D:
            return GLType::ImageTexture1D;
        case gl::GL_IMAGE_2D:
            return GLType::ImageTexture2D;
        }
#include "compiler_warnings/pop.h"
        FATAL_ERROR("Unknown GL type %s (%i)", glbinding::Meta::getString(type).c_str(), (int)type);
    }

    inline bool IsTypeSampler( GLType type ) 
    {
        switch (type) {
        case GLType::Sampler1D:
        case GLType::Sampler2D:
            return true;
        case GLType::Int:
        case GLType::UInt:
        case GLType::Float:
        case GLType::Vec2:
        case GLType::Vec3:
        case GLType::Vec4:
        case GLType::Mat2x2:
        case GLType::Mat2x3:
        case GLType::Mat2x4:
        case GLType::Mat3x2:
        case GLType::Mat3x3:
        case GLType::Mat3x4:
        case GLType::Mat4x2:
        case GLType::Mat4x3:
        case GLType::Mat4x4:
        case GLType::ImageTexture1D:
        case GLType::ImageTexture2D:
            return false;
        }
        FATAL_ERROR("Unknown GLType (%i)", (int)type);
    }

    inline bool IsTypeImageTexture( GLType type ) 
    {
        switch (type) {
        case GLType::ImageTexture1D:
        case GLType::ImageTexture2D:
            return true;
        case GLType::Int:
        case GLType::UInt:
        case GLType::Float:
        case GLType::Vec2:
        case GLType::Vec3:
        case GLType::Vec4:
        case GLType::Mat2x2:
        case GLType::Mat2x3:
        case GLType::Mat2x4:
        case GLType::Mat3x2:
        case GLType::Mat3x3:
        case GLType::Mat3x4:
        case GLType::Mat4x2:
        case GLType::Mat4x3:
        case GLType::Mat4x4:
        case GLType::Sampler1D:
        case GLType::Sampler2D:
            return false;
        }
        FATAL_ERROR("Unknown GLType (%i)", (int)type);
    
    }

    inline TextureType ToTextureType( GLType type ) {
        switch (type) {
        case GLType::Sampler1D:
        case GLType::ImageTexture1D:
            FATAL_ERROR("Unsupported Texture1D");
            break;
        case GLType::Sampler2D:
        case GLType::ImageTexture2D:
            return TextureType::Texture2D;
        case GLType::Int:
        case GLType::UInt:
        case GLType::Float:
        case GLType::Vec2:
        case GLType::Vec3:
        case GLType::Vec4:
        case GLType::Mat2x2:
        case GLType::Mat2x3:
        case GLType::Mat2x4:
        case GLType::Mat3x2:
        case GLType::Mat3x3:
        case GLType::Mat3x4:
        case GLType::Mat4x2:
        case GLType::Mat4x3:
        case GLType::Mat4x4:
            FATAL_ERROR("Type %i is not a sampler!", (int)type);
        }
        FATAL_ERROR("Unknown GLType (%i)", (int)type);
    }

    inline TransformCaptureType ToTransformCaptureType( gl::GLenum type )
    {
        switch (type) {
        case gl::GL_FLOAT:
            return TransformCaptureType::Float;
        case gl::GL_FLOAT_VEC2:
            return TransformCaptureType::FloatVec2;
        case gl::GL_FLOAT_VEC3:
            return TransformCaptureType::FloatVec3;
        case gl::GL_FLOAT_VEC4:
            return TransformCaptureType::FloatVec4;
        case gl::GL_DOUBLE:
            return TransformCaptureType::Double;
        case gl::GL_DOUBLE_VEC2:
            return TransformCaptureType::DoubleVec2;
        case gl::GL_DOUBLE_VEC3:
            return TransformCaptureType::DoubleVec3;
        case gl::GL_DOUBLE_VEC4:
            return TransformCaptureType::DoubleVec4;
        case gl::GL_INT:
            return TransformCaptureType::Int;
        case gl::GL_INT_VEC2:
            return TransformCaptureType::IntVec2;
        case gl::GL_INT_VEC3:
            return TransformCaptureType::IntVec3;
        case gl::GL_INT_VEC4:
            return TransformCaptureType::IntVec4;
        case gl::GL_UNSIGNED_INT:
            return TransformCaptureType::UInt;
        case gl::GL_UNSIGNED_INT_VEC2:
            return TransformCaptureType::UIntVec2;
        case gl::GL_UNSIGNED_INT_VEC3:
            return TransformCaptureType::UIntVec3;
        case gl::GL_UNSIGNED_INT_VEC4:
            return TransformCaptureType::UIntVec4;
        default:
            FATAL_ERROR("Unknown TransformCaptureType (%i)", (int)type);
        }
    }
}

