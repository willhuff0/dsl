#include <utils/GrowingBuffer.hpp>

#include "Provider.hpp"
#include "dvk/CommandBuffer.hpp"

constexpr uint64_t GROW_FACTOR = 2;

GrowingBuffer::GrowingBuffer(std::string label, vk::BufferUsageFlags usage, vk::PipelineStageFlags2 stages, uint64_t capacity)
    : label(std::move(label)), usage(usage), stages(stages), capacity(capacity) { }

bool GrowingBuffer::Push(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, const void* src, uint64_t dsize) {
    if (dsize == 0) return false;

    bool didResize = false;
    if (size + dsize > capacity) {
        if (capacity == 0) {
            capacity = 1;
            while (size + dsize > capacity) {
                capacity *= GROW_FACTOR;
            }
        } else {
            do {
                capacity *= GROW_FACTOR;
            } while (size + dsize > capacity);
        }
        recreate(commandBuffer, frameIndex);
        didResize = true;
    } else if (buffer == nullptr) {
        recreate(commandBuffer, frameIndex);
        didResize = true;
    }
    buffer->CopyToAndFlush(src, size, dsize);
    size += dsize;
    return didResize;
}

void GrowingBuffer::Pop(uint64_t dsize) {
    size -= dsize;
}

bool GrowingBuffer::Reserve(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, uint64_t newCapacity) {
    if (buffer == nullptr || newCapacity > capacity) {
        capacity = newCapacity;
        recreate(commandBuffer, frameIndex);
        return true;
    }
    return false;
}

void GrowingBuffer::Recreate(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) {
    recreate(commandBuffer, frameIndex);
}

const dvk::Buffer::Ref& GrowingBuffer::GetBuffer() const {
    return buffer;
}

dvk::Buffer::Ref& GrowingBuffer::GetBuffer() {
    return buffer;
}

uint64_t GrowingBuffer::GetSize() const {
    return size;
}

uint64_t GrowingBuffer::GetCapacity() const {
    return capacity;
}

void GrowingBuffer::recreate(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) {
    auto newBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel(label)
        .SetUsage(usage | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst)
        .SetSize(capacity)
        .EnableCopyToAndFlush()
        .Build();

    if (buffer != nullptr) {
        if (size > 0) {
            commandBuffer
                .InsertBarriers()
                    .AddBufferBarrier()
                        .SetBuffer(buffer)
                        .SetStages(stages, vk::PipelineStageFlagBits2::eTransfer)
                        .SetAccess(vk::AccessFlagBits2::eMemoryWrite, vk::AccessFlagBits2::eTransferRead)
                        .FinishBufferBarrier()
                    .FinishBarriers()
                .CopyBuffer(buffer, newBuffer)
                    .AddRegion(0, 0, size)
                    .FinishBufferCopy()
                .InsertBarriers()
                    .AddBufferBarrier()
                        .SetBuffer(newBuffer)
                        .SetStages(vk::PipelineStageFlagBits2::eTransfer, stages)
                        .SetAccess(vk::AccessFlagBits2::eTransferWrite, vk::AccessFlagBits2::eMemoryRead)
                        .FinishBufferBarrier()
                    .FinishBarriers();
        }

        GetCommandEncoder().DropRefOnceNotInUse(frameIndex, buffer);
    }

    buffer = newBuffer;
}