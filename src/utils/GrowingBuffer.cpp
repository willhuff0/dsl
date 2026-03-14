#include <utils/GrowingBuffer.hpp>

#include "Provider.hpp"
#include "dvk/CommandBuffer.hpp"

constexpr uint64_t GROW_FACTOR = 2;

GrowingBuffer::GrowingBuffer(std::string label, vk::BufferUsageFlags usage, vk::PipelineStageFlags2 stages, uint64_t capacity)
    : label(std::move(label)), usage(usage), stages(stages), capacity(capacity) {
    if (capacity != 0) {
        recreate();
    }
}

void GrowingBuffer::recreate() {
    auto newBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel(label)
        .SetUsage(usage)
        .SetSize(capacity)
        .EnableCopyToAndFlush()
        .Build();

    GetCommandEncoder().Execute([&](dvk::CommandBuffer& commandBuffer) {
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

    buffer = newBuffer;
}

void GrowingBuffer::Push(uint64_t dsize) {
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
        recreate();
    }
    size += dsize;
}

void GrowingBuffer::Pop(uint64_t dsize) {
    size -= dsize;
}

void GrowingBuffer::Reserve(uint64_t size) {
    if (size > capacity) {
        capacity = size;
        recreate();
    }
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
