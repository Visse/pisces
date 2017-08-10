#include "StreamingBuffer.h"

#include "Context.h"
#include "HardwareResourceManager.h"

#include "Common/ErrorUtils.h"
#include "Common/PointerHelpers.h"

namespace Pisces
{
    struct StreamingBufferBase::Impl {
        Context *context;
        HardwareResourceManager *hardwareMgr;
        BufferHandle buffer;

        size_t size = 0;

        Impl( Context *context_ ) :
            context(context_), 
            hardwareMgr(context->getHardwareResourceManager())
        {}
    };

    size_t offsetForFrame( size_t size, size_t frame ) {
        return (frame%FRAMES_IN_FLIGHT) * size;
    }

    PISCES_API StreamingBufferBase::StreamingBufferBase( Context *context, BufferType type, size_t size ) :
        mImpl(context)
    {
        mImpl->buffer = mImpl->hardwareMgr->allocateBuffer(type, BufferUsage::StreamWrite, BufferFlags::MapWrite|BufferFlags::MapPersistent, size*FRAMES_IN_FLIGHT, nullptr);
        mImpl->size = size;
    }

    PISCES_API StreamingBufferBase::~StreamingBufferBase()
    {
        mImpl->hardwareMgr->freeBuffer(mImpl->buffer);
    }

    PISCES_API void* StreamingBufferBase::mapBuffer( size_t offset, size_t size )
    {
        if( (offset+size) > mImpl->size) {
            LOG_WARNING("Trying to map invalid range [%zu, %zu], when the buffer is only %zu bytes big!", offset, size, mImpl->size);
            return nullptr;
        }

        size_t frameOffset = currentFrameOffset();
        return mImpl->hardwareMgr->mapBuffer(mImpl->buffer, frameOffset+offset, size, BufferMapFlags::MapWrite|BufferMapFlags::Persistent);
    }

    PISCES_API void StreamingBufferBase::unmapBuffer()
    {
        mImpl->hardwareMgr->unmapBuffer(mImpl->buffer, true);
    }

    PISCES_API void StreamingBufferBase::resize( size_t newSize, StreamingBufferResizeFlags flags )
    {
        BufferResizeFlags resizeFlags = BufferResizeFlags::None;
        if (all(flags, StreamingBufferResizeFlags::KeepCurrentFrame)) {
            resizeFlags = set(resizeFlags, BufferResizeFlags::KeepRange);   
        }

        size_t currentFrame = mImpl->context->currentFrame();
        size_t currentOffset = offsetForFrame(mImpl->size, currentFrame);
        size_t newOffset = offsetForFrame(newSize, currentFrame);

        size_t copySize = mImpl->size < newSize ? mImpl->size : newSize;


        mImpl->hardwareMgr->resizeBuffer(mImpl->buffer, newSize*FRAMES_IN_FLIGHT, resizeFlags, currentOffset, newOffset, copySize);
        mImpl->size = newSize;
    }

    PISCES_API size_t StreamingBufferBase::currentFrameOffset()
    {
        size_t currentFrame = mImpl->context->currentFrame();
        return offsetForFrame(mImpl->size, currentFrame);
    }

    PISCES_API BufferHandle StreamingBufferBase::handle()
    {
        return mImpl->buffer;
    }

    PISCES_API StreamingUniformBuffer::StreamingUniformBuffer( Context *context, size_t size, bool autoResize ) :
        StreamingBufferBase(context, BufferType::Uniform, size)
    {
        HardwareResourceManager *hardwareMgr = context->getHardwareResourceManager();
        mAlignment = hardwareMgr->getUniformAlignment();
    }

    PISCES_API UniformBufferHandle StreamingUniformBuffer::allocate( const void *data, size_t size )
    {
        if (!mCurrentMapping) {
            LOG_ERROR("StreamingUniformBuffer::allocate must be called between beginAllocation() and endAllocation()");
            return {};
        }

        size_t requiredSize = mCurrentOffset + size;
        if (requiredSize > mImpl->size) {
            size_t newSize = requiredSize * 2;

            size_t currentFrame = mImpl->context->currentFrame();
            size_t currentOffset = offsetForFrame(mImpl->size, currentFrame),
                   newOffest = offsetForFrame(newSize, currentFrame);

            mImpl->hardwareMgr->resizeBuffer(mImpl->buffer, newSize, BufferResizeFlags::KeepRange, currentOffset, newOffest, mCurrentOffset);
        }

        UniformBufferHandle handle;
            handle.buffer = mImpl->buffer;
            handle.offset = mCurrentOffset;
            handle.size = size;

        // Adjust size to the next multiple of aligment
        size_t alignedSize = ((size + mAlignment - 1) / mAlignment)  * mAlignment;
        memcpy(Common::advance(mCurrentMapping, mCurrentOffset), data, size);
        mCurrentOffset += alignedSize;

        return handle;
    }

    PISCES_API void StreamingUniformBuffer::beginAllocation()
    {
        mCurrentOffset = 0;
        mCurrentMapping = mapBuffer(0, mImpl->size);
    }

    PISCES_API void StreamingUniformBuffer::endAllocation()
    {
        unmapBuffer();
    }
}
