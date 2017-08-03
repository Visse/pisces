#pragma once

#include <memory>

#include "Common/EnumFlagOp.h"
#include "Common/HandleType.h"

#include "Common/Color.h"

namespace Pisces
{
    using Color = Common::Color;
    namespace NamedColors = Common::NamedColors;

    static const int MAX_BOUND_SAMPLERS = 16;
    static const int MAX_BOUND_UNIFORMS = 16;
    static const int MAX_BOUND_UNIFORM_BUFFERS = 16;
    static const int MAX_BOUND_IMAGE_TEXTURES = 4;

    static const int MIN_UNIFORM_BLOCK_BUFFER_SIZE = 1024*16; // 16 Kb

    static const int FRAMES_IN_FLIGHT = 3;
    
    MAKE_HANDLE( ResourceName, uint32_t );

    MAKE_HANDLE( TextureHandle, uint32_t );
    MAKE_HANDLE( ProgramHandle, uint32_t );
    MAKE_HANDLE( BufferHandle, uint32_t );

    MAKE_HANDLE( ComputeProgramHandle, uint32_t );

    MAKE_HANDLE( ProgramHandle, uint32_t );
    MAKE_HANDLE( PipelineHandle, uint32_t );

    MAKE_HANDLE( RenderTargetHandle, uint32_t );

    MAKE_HANDLE( VertexArrayHandle, uint32_t );

    class Context;
    class HardwareResourceManager;
    class PipelineManager;
    class RenderCommandQueue;
    using RenderCommandQueuePtr = std::shared_ptr<RenderCommandQueue>;
    class CompiledRenderQueue;
    using CompiledRenderQueuePtr = std::shared_ptr<CompiledRenderQueue>;

    enum class PixelFormat {
        R8, RG8, RGB8, RGBA8
    };
    bool PixelFormatFromString( const char *str, size_t len, PixelFormat &format );

    enum class SwizzleMask {
        Red, Green, Blue, Alpha,
        Zero, One
    };

    enum class TextureType {
        Texture2D = 0,
        Cubemap   = 1
    };

    enum class CubemapFace {
        PosX, NegX,
        PosY, NegY,
        PosZ, NegZ
    };

    enum class TextureFlags {
        None = 0,
        Dynamic = 1, // The texture may change durring the lifetime
        RenderTarget = 2, // The texture may be used as a render target
        ImageTexture = 4, // The texture may be used as a image texture
    };
    DECLARE_ENUM_FLAG( TextureFlags );

    enum class ImageTextureAccess {
        ReadOnly,
        WriteOnly,
        ReadWrite
    };

    enum class TextureUploadFlags {
        None = 0,
        GenerateMipmaps = 1, // Automatic genereate mipmaps for higher levels
        PreMultiplyAlpha = 2, // Pre multiply the alpha channel
        FlipVerticaly = 4, // Flip the texture verticaly before upload
    };
    DECLARE_ENUM_FLAG( TextureUploadFlags );

    enum class BufferUsage {
        Static, 
        StreamWrite,
    };

    enum class BufferFlags {
        None = 0,
        MapRead = 1,
        MapWrite = 2,
        MapPersistent = 4,

        Upload = 16,
    };
    DECLARE_ENUM_FLAG( BufferFlags );

    enum class BufferMapFlags {
        MapRead = 1,
        MapWrite = 2,

        DiscardRange = 4,
        DiscardBuffer = 8,

        Persistent = 16,
    };
    DECLARE_ENUM_FLAG( BufferMapFlags );

    enum class BufferResizeFlags {
        None = 0,
        KeepRange = 1
    };
    DECLARE_ENUM_FLAG( BufferResizeFlags );

    enum class VertexAttributeType {
        Int8,  Int16,  Int32,
        UInt8, UInt16, UInt32,

        NormInt8,  NormInt16,  NormInt32,
        NormUInt8, NormUInt16, NormUInt32,

        Float16, Float32
    };

    enum class PipelineFlags {
        None = 0,
        DepthWrite  = 1,
        DepthTest   = 2,
        FaceCulling = 4,

        OwnProgram  = 8,
    };
    DECLARE_ENUM_FLAG(PipelineFlags);

    enum class RenderProgramFlags {
        None = 0,
    };
    DECLARE_ENUM_FLAG(RenderProgramFlags);

    enum class ComputeProgramFlags {
        None = 0,
    };
    DECLARE_ENUM_FLAG(ComputeProgramFlags);

    enum class RenderCommandQueueFlags {
        None = 0,
    };
    DECLARE_ENUM_FLAG(RenderCommandQueueFlags);

    enum class ClearFlags {
        Color = 1,
        Depth = 2,
        Stencil = 4,
        All = Color | Depth | Stencil,
    };
    DECLARE_ENUM_FLAG(ClearFlags);

    enum class RenderQueueCompileFlags {
        None = 0,
    };
    DECLARE_ENUM_FLAG(RenderQueueCompileFlags);

    struct VertexAttribute {
        VertexAttributeType type;
        int offset, count, stride, source;
    };

    enum class BufferType {
        Vertex, Index, Uniform
    };

    enum class IndexType {
        UInt16, UInt32
    };

    enum class BlendMode {
        Replace,
        Alpha,
        PreMultipledAlpha,
    };
    bool BlendModeFromString( const char *str, BlendMode &mode );
    bool BlendModeFromString( const char *str, size_t len, BlendMode &mode );

    enum class Shape {
        Cube,
        Sphere
    };

    enum class Primitive {
        Points,
        Lines,
        Triangles
    };

    enum class SamplerMinFilter {
        Nearest,
        Linear,
        NearestMipmapNearest,
        LinearMipmapNearest,
        NearestMipmapLinear,
        LinearMipmapLinear,
    };
    bool SamplerMinFilterFromString( const char *str, size_t len,  SamplerMinFilter &filter );

    enum class SamplerMagFilter {
        Nearest = 0,
        Linear = 1,
    };
    bool SamplerMagFilterFromString( const char *str, size_t len,  SamplerMagFilter &filter );

    enum class EdgeSamplingMode {
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder,
    };

    enum class BuiltinObject {
        Quad = 0,
        Cube = 1,
        IcoSphere = 2,
        UVSphere = 3,
    };
    static const int BUILTIN_OBJECT_COUNT = 4;

    struct BufferInitParams {
        const void *data = nullptr;
        int size = 0;

        BufferUsage usage;
        BufferFlags flags;
    };

    struct SamplerParams {
        SamplerMinFilter minFilter = SamplerMinFilter::Nearest;
        SamplerMagFilter magFilter = SamplerMagFilter::Nearest;

        EdgeSamplingMode edgeSamplingX = EdgeSamplingMode::ClampToBorder, 
                         edgeSamplingY = EdgeSamplingMode::ClampToBorder;

        Color borderColor = NamedColors::Black;
    };

    struct UniformBufferHandle {
        BufferHandle buffer;
        size_t offset=0, size=0;

        friend bool operator == ( const UniformBufferHandle &lhs, const UniformBufferHandle &rhs) {
            if (lhs.buffer != rhs.buffer) return false;
            if (lhs.offset != rhs.offset) return false;
            if (lhs.size   != rhs.size)   return false;
            return true;
        }
        friend bool operator != ( const UniformBufferHandle &lhs, const UniformBufferHandle &rhs) {
            return !(lhs == rhs);
        }

        operator bool () const {
            return (bool)buffer;
        }
    };

    struct ClipRect {
        int x=0, y=0, 
            w=0, h=0;

        friend bool operator == ( const ClipRect &lhs, const ClipRect &rhs) {
            if (lhs.x != rhs.x) return false;
            if (lhs.y != rhs.y) return false;
            if (lhs.w != rhs.w) return false;
            if (lhs.h != rhs.h) return false;
            return true;
        }
        friend bool operator != ( const ClipRect &lhs, const ClipRect &rhs) {
            return !(lhs == rhs);
        }
    };

    struct RenderQueueCompileOptions {
        RenderQueueCompileFlags flags = RenderQueueCompileFlags::None;

        // intended to be used with streaming buffers,
        // expecially when they can be resized,
        // This makes it posible to specify the base vertex 
        // after you have recorded a render queue - if you
        // specify it in the queue it will be messed up on resize
        size_t baseVertex = 0,
               firstIndex = 0;
        // Use this vertex array if the supplied one is null
        VertexArrayHandle defaultVertexArray;

        RenderQueueCompileOptions() = default;
        RenderQueueCompileOptions( RenderQueueCompileFlags flags_ ) : 
            flags(flags_)
        {}
    };
}