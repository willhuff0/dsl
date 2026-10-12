#include <StaticBufferGroup.hpp>

#include <optional>

SbAddr StaticBuffer::QueueUpload(const void* src, uint64_t size) {
    std::optional<FreeSpace> dstSpace;
    for (const auto& freeSpace : _freeSpaces) {
        if (freeSpace.size < size) continue;
        dstSpace = freeSpace;
        break;
    }
    if (dstSpace == std::nullopt) {
        // get last allocated space

        vk::DeviceSize lastAllocationEnd =
    }
}

void StaticBuffer::QueueFree(SbAddr addr, uint32_t frameIndex) {
    if (std::erase_if(_uploadQueue, [&](const UploadOp& uploadOp) {
        return uploadOp.targetAddr == addr;
    }) > 0) return;

    _freeQueue[frameIndex].push_back(addr);
}
