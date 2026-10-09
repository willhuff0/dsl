#pragma once

#include <dvk/dvk.hpp>

constexpr uint64_t BUFFER_START_SIZE = 1024;

class GrowingBuffer {
public:
    explicit GrowingBuffer(std::string label, vk::BufferUsageFlags usage, vk::PipelineStageFlags2 stages, uint64_t capacity = BUFFER_START_SIZE);

    bool Push(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, const void* src, uint64_t dsize);
    void Pop(uint64_t dsize);

    bool Reserve(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, uint64_t capacity);
    void Recreate(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex);

    [[nodiscard]] const dvk::Buffer::Ref& GetBuffer() const;
    [[nodiscard]] dvk::Buffer::Ref& GetBuffer();
    [[nodiscard]] uint64_t GetSize() const;
    [[nodiscard]] uint64_t GetCapacity() const;

private:
    void recreate(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex);

    std::string label;
    vk::BufferUsageFlags usage;
    vk::PipelineStageFlags2 stages;

    dvk::Buffer::Ref buffer;
    uint64_t size = 0;
    uint64_t capacity;
};