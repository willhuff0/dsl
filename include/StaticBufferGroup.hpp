#pragma once

#include "Provider.hpp"
#include "utils/GrowingBuffer.hpp"

#include <dvk/dvk.hpp>

#include <vector>
#include <unordered_map>
#include <memory>

class StaticBufferGroup;

struct UploadOp {
    vk::DeviceSize stagingBufferAddr;
    vk::DeviceSize targetAddr;
    vk::DeviceSize size;
};

struct FreeSpace {
    vk::DeviceSize addr;
    vk::DeviceSize size;
};

using SbAddr = vk::DeviceSize;

class StaticBuffer {
    friend class StaticBufferGroup;
public:
    SbAddr QueueUpload(const void* src, uint64_t size);
    void QueueFree(SbAddr addr, uint32_t frameIndex);

private:
    std::shared_ptr<StaticBufferGroup> _group;
    vk::DeviceSize _alignment;

    dvk::Buffer::Ref _buffer;
    std::unordered_map<SbAddr, vk::DeviceSize> _allocations;
    std::vector<FreeSpace> _freeSpaces;

    std::vector<UploadOp> _uploadQueue;
    std::array<std::vector<SbAddr>, FRAMES_IN_FLIGHT> _freeQueue;

    void update(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex);
};

class StaticBufferGroup {
    friend class StaticBuffer;
public:
    StaticBuffer Create(vk::DeviceSize alignment);

    void Update(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex);

private:
    std::vector<StaticBuffer> _buffers;
    GrowingBuffer _stagingBuffer;
};