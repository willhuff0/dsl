#include <utils/GrowingBuffer.hpp>

#include "Provider.hpp"
#include "dvk/CommandBuffer.hpp"

constexpr uint64_t GROW_FACTOR = 2;

GrowingBuffer::GrowingBuffer(std::string label, vk::BufferUsageFlags usage, vk::PipelineStageFlags2 stages, uint64_t capacity)
    : label(std::move(label)), usage(usage), stages(stages), capacity(capacity) {
    if (capacity != 0) {
        recreate(0);
    }
}

void GrowingBuffer::recreate(uint32_t frameIndex) {
    auto newBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel(label)
        .SetUsage(usage | vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst)
        .SetSize(capacity)
        .EnableCopyToAndFlush()
        .Build();

    if (buffer != nullptr && size > 0) {
        GetCommandEncoder().Execute(frameIndex, [&](dvk::CommandBuffer& commandBuffer) {
            vk::BufferMemoryBarrier2 srcBarrier {
                .srcStageMask = stages,
                .srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
                .dstAccessMask = vk::AccessFlagBits2::eTransferRead,
                .buffer = buffer->GetBuffer(),
                .offset = 0,
                .size = vk::WholeSize,
            };
            commandBuffer.GetCommandBuffer().pipelineBarrier2({
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers = &srcBarrier,
            });
            commandBuffer.GetCommandBuffer().copyBuffer(buffer->GetBuffer(), newBuffer->GetBuffer(), { vk::BufferCopy {
                .srcOffset = 0,
                .dstOffset = 0,
                .size = size,
            }});
            vk::BufferMemoryBarrier2 dstBarrier {
                .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
                .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                .dstStageMask = stages,
                .dstAccessMask = vk::AccessFlagBits2::eMemoryRead,
                .buffer = buffer->GetBuffer(),
                .offset = 0,
                .size = vk::WholeSize,
            };
            commandBuffer.GetCommandBuffer().pipelineBarrier2({
                .bufferMemoryBarrierCount = 1,
                .pBufferMemoryBarriers = &dstBarrier,
            });
        });

        GetCommandEncoder().DropRefOnceNotInUse(frameIndex, buffer);
    }

    buffer = newBuffer;
}

bool GrowingBuffer::Push(uint32_t frameIndex, const void* src, uint64_t dsize) {
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
        recreate(frameIndex);
        didResize = true;
    }
    buffer->CopyToAndFlush(src, size, dsize);
    size += dsize;
    return didResize;
}

void GrowingBuffer::Pop(uint64_t dsize) {
    size -= dsize;
}

bool GrowingBuffer::Reserve(uint32_t frameIndex, uint64_t size) {
    if (size > capacity) {
        capacity = size;
        recreate(frameIndex);
        return true;
    }
    return false;
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
