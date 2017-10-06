#pragma once

#include "build_config.h"

#include <memory>

#include "Common/EnumFlagOp.h"
#include "Common/HandleType.h"
#include "Common/EnumString.h"

#include "Common/Color.h"

namespace Pisces
{
    using Color = Common::Color;
    namespace NamedColors = Common::NamedColors;

    static const int MAX_BOUND_SAMPLERS = 16;
    static const int MAX_BOUND_UNIFORMS = 16;
    static const int MAX_BOUND_UNIFORM_BUFFERS = 16;
    static const int MAX_BOUND_IMAGE_TEXTURES = 4;
    static const int MAX_TRANFORM_FEEDBACK_CAPTURE_VARIABLES = 4;

    static const int MIN_UNIFORM_BLOCK_BUFFER_SIZE = 1024*16; // 16 Kb

    static const int FRAMES_IN_FLIGHT = 3;

    MAKE_HANDLE( TextureHandle, uint32_t );
    MAKE_HANDLE( ProgramHandle, uint32_t );
    MAKE_HANDLE( BufferHandle, uint32_t );


    MAKE_HANDLE( ComputeProgramHandle, uint32_t );
    // Transform feed back program
    MAKE_HANDLE( TranformProgramHandle, uint32_t );

    MAKE_HANDLE( ResourceHandle, uint64_t );
    MAKE_HANDLE( ResourcePackHandle, uint32_t );

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
    class IResourceLoader;

    struct Sprite;
    class SpriteManager;

    enum class PixelFormat {
        R8, RG8, RGB8, RGBA8
    };
    DECL_ENUM_TO_FROM_STRING(PixelFormat, PISCES_API,
        (R8, "r"),
        (RG8, "rg"),
        (RGB8, "rgb"),
        (RGBA8, "rgba")                        
    );

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
    
    enum class TransformCaptureType {
        Float,
        FloatVec2, FloatVec3, FloatVec4,
        Double,
        DoubleVec2, DoubleVec3, DoubleVec4,
        Int,
        IntVec2, IntVec3, IntVec4,
        UInt,
        UIntVec2, UIntVec3, UIntVec4,
    };
    DECL_ENUM_TO_FROM_STRING(TransformCaptureType, PISCES_API,
        (Float),
        (FloatVec2), (FloatVec3), (FloatVec4),
        (Double),
        (DoubleVec2), (DoubleVec3), (DoubleVec4),
        (Int),
        (IntVec2), (IntVec3), (IntVec4),
        (UInt),
        (UIntVec2), (UIntVec3), (UIntVec4)
    );

    enum class VertexAttributeType {
        Int8,  Int16,  Int32,
        UInt8, UInt16, UInt32,

        NormInt8,  NormInt16,  NormInt32,
        NormUInt8, NormUInt16, NormUInt32,

        NormInt3x10_1x2,
        NormUInt3x10_1x2,

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

    enum class TransformProgramFlags {
        None = 0,
    };
    DECLARE_ENUM_FLAG(TransformProgramFlags);

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
    DECL_ENUM_TO_FROM_STRING(BlendMode, PISCES_API,
        (Replace),
        (Alpha),
        (PreMultipledAlpha, "pre_multipled_alpha", "premultiplied_alpha")
    );
    
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
    DECL_ENUM_TO_FROM_STRING(SamplerMinFilter, PISCES_API,
        (Nearest),
        (Linear),
        (NearestMipmapNearest, "nearest_mipmap_nearest"),
        (LinearMipmapNearest, "linear_mipmap_nearest"),
        (NearestMipmapLinear, "nearest_mipmap_linear"),
        (LinearMipmapLinear, "linear_mipmap_linear")
    );

    enum class SamplerMagFilter {
        Nearest = 0,
        Linear = 1,
    };
    DECL_ENUM_TO_FROM_STRING(SamplerMagFilter, PISCES_API,
        (Nearest),
        (Linear)
    );

    enum class EdgeSamplingMode {
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder,
    };
    DECL_ENUM_TO_FROM_STRING(EdgeSamplingMode, PISCES_API,
        (Repeat),
        (MirroredRepeat, "mirrored_repeat"),
        (ClampToEdge, "clamp_to_edge"),
        (ClampToBorder, "clamp_to_border")
    );

    enum class BuiltinObject {
        Quad = 0,
        Cube = 1,
        IcoSphere = 2,
        UVSphere = 3,

        Plane_XY = 4,
        Plane_XZ = 5,
        Plane_YZ = 6,
    };
    static const int BUILTIN_OBJECT_COUNT = 7;

    enum class BuiltinTexture {
        Missing = 0,
        White   = 1,
        Black   = 2,
    };
    static const int BUILTIN_TEXTURE_COUNT = 3;

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