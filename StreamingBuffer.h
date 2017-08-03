#pragma once


#include "Fwd.h"

#include "Common/PImplHelper.h"

#include <type_traits>

namespace Pisces
{
    enum class StreamingBufferResizeFlags {
        None = 0,
        // Keep data in the current frame
        KeepCurrentFrame = 1,
    };
    DECLARE_ENUM_FLAG(StreamingBufferResizeFlags);

    class StreamingBufferBase {
    public:
        StreamingBufferBase( Context *context, BufferType type, size_t size );
        ~StreamingBufferBase();

        StreamingBufferBase( const StreamingBufferBase& ) = delete;
        StreamingBufferBase& operator = ( const StreamingBufferBase& ) = delete;

        void* mapBuffer( size_t offset, size_t size );
        void unmapBuffer();

        void resize( size_t newSize, StreamingBufferResizeFlags flags );

        size_t currentFrameOffset();
        BufferHandle handle();

    protected:
        struct Impl;
        PImplHelper<Impl,32> mImpl;
    };

    template< typename Type_ >
    class StreamingBuffer :
        private StreamingBufferBase
    {
    public:
        using Type = Type_;

        static_assert( std::is_standard_layout<Type>::value, "Type must be standard layout!" );
    public:
        StreamingBuffer( Context *context, BufferType type, size_t count ) :
            StreamingBufferBase(context, type, count*sizeof(Type))
        {}

        StreamingBuffer( const StreamingBuffer& ) = delete;
        StreamingBuffer& operator = ( const StreamingBuffer& ) = delete;

        Type* mapBuffer( size_t first, size_t count ) {
            return (Type*)StreamingBufferBase::mapBuffer(first*sizeof(Type), count*sizeof(Type));
        }
        void unmapBuffer() {
            StreamingBufferBase::unmapBuffer();
        }

        // Buffer must be unmapped for the resize
        void resize( size_t newSize, StreamingBufferResizeFlags flags=StreamingBufferResizeFlags::None ) {
            return StreamingBufferBase::resize(newSize * sizeof(Type), flags);
        }

        size_t currentFrameOffset() {
            return StreamingBufferBase::currentFrameOffset() / sizeof(Type);
        }

        BufferHandle handle() {
            return StreamingBufferBase::handle();
        }
    };

    class StreamingUniformBuffer :
           private StreamingBufferBase
    {
    public:
        StreamingUniformBuffer( Context *context, size_t size, bool autoResize=false );
        StreamingUniformBuffer( const StreamingUniformBuffer& ) = delete;
        StreamingUniformBuffer& operator = ( const StreamingUniformBuffer& ) = delete;

        UniformBufferHandle allocate( const void *data, size_t size );

        template< typename Type >
        UniformBufferHandle allocate( const Type &type ) {
            return allocate(&type, sizeof(Type));
        }
        
        void beginAllocation();
        void endAllocation();

    private:
        size_t mAlignment=0, mCurrentOffset=0;

        void *mCurrentMapping = nullptr;
        bool mAutoResize = false;
    };
}