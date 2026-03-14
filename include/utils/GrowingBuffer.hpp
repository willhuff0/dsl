#pragma once

#include <dvk/dvk.hpp>

class GrowingBuffer {
public:
    explicit GrowingBuffer(std::string label, vk::BufferUsageFlags usage, vk::PipelineStageFlags2 stages, uint64_t capacity = 0);

    void Push(uint64_t dsize);
    void Pop(uint64_t dsize);

    void Reserve(uint64_t size);

    [[nodiscard]] const dvk::Buffer::Ref& GetBuffer() const;
    [[nodiscard]] dvk::Buffer::Ref& GetBuffer();
    [[nodiscard]] uint64_t GetSize() const;
    [[nodiscard]] uint64_t GetCapacity() const;

private:
    void recreate();

    std::string label;
    vk::BufferUsageFlags usage;
    vk::PipelineStageFlags2 stages;

    dvk::Buffer::Ref buffer;
    uint64_t size = 0;
    uint64_t capacity;
};