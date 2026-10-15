#include <utils/ChunkedBuffer.hpp>

#include <dvk/Config.hpp>

#include <cassert>
#include <stdexcept>

ChunkedBuffer::ChunkedBuffer(UploadQueue& uploadQueue, Desc desc)
    : uploadQueue(uploadQueue), desc(std::move(desc)) {
    assert(this->desc.elementSize > 0);
    assert(this->desc.elementsPerChunk > 0);
    assert(this->desc.maxChunks > 0);

    // Create the first chunk eagerly so there's always something to bind.
    std::lock_guard lock(mutex);
    chunks.push_back(createChunkLocked());
}

ChunkedBuffer::~ChunkedBuffer() {
    // Assumes the GPU is idle, which holds at shutdown after the queues have been waited on.
    for (auto& chunk : chunks) {
        vmaClearVirtualBlock(chunk.block);
        vmaDestroyVirtualBlock(chunk.block);
    }
}

ChunkedBuffer::Allocation ChunkedBuffer::Allocate(uint32_t elementCount) {
    if (elementCount == 0 || elementCount > desc.elementsPerChunk) return {};

    VmaVirtualAllocationCreateInfo createInfo {
        .size = elementCount,
        .alignment = 1,
    };

    std::lock_guard lock(mutex);

    for (uint32_t chunkIndex = 0;; chunkIndex++) {
        if (chunkIndex == chunks.size()) {
            if (chunks.size() >= desc.maxChunks) return {};
            chunks.push_back(createChunkLocked());
        }

        VmaVirtualAllocation handle;
        VkDeviceSize offset;
        if (vmaVirtualAllocate(chunks[chunkIndex].block, &createInfo, &handle, &offset) != VK_SUCCESS) continue;

        lastUploadValues.emplace(AllocationKey { chunkIndex, handle }, 0);
        return Allocation {
            .chunk = chunkIndex,
            .firstElement = static_cast<uint32_t>(offset),
            .elementCount = elementCount,
            .handle = handle,
        };
    }
}

UploadTicket ChunkedBuffer::Upload(const Allocation& allocation, const void* src, uint32_t elementCount, uint32_t elementOffset) {
    assert(allocation);
    assert(elementOffset + elementCount <= allocation.elementCount);

    dvk::Buffer::Ref buffer;
    {
        std::lock_guard lock(mutex);
        buffer = chunks[allocation.chunk].buffer;
    }

    auto ticket = uploadQueue.Enqueue(
        buffer,
        (allocation.firstElement + elementOffset) * desc.elementSize,
        src,
        elementCount * desc.elementSize);

    {
        std::lock_guard lock(mutex);
        auto& lastUploadValue = lastUploadValues.at(AllocationKey { allocation.chunk, allocation.handle });
        lastUploadValue = std::max(lastUploadValue, ticket.value);
    }

    return ticket;
}

std::pair<ChunkedBuffer::Allocation, UploadTicket> ChunkedBuffer::AllocateAndUpload(const void* src, uint32_t elementCount) {
    auto allocation = Allocate(elementCount);
    if (!allocation) return { allocation, {} };
    return { allocation, Upload(allocation, src, elementCount) };
}

void ChunkedBuffer::Free(const Allocation& allocation) {
    if (!allocation) return;

    std::lock_guard lock(mutex);
    auto it = lastUploadValues.find(AllocationKey { allocation.chunk, allocation.handle });
    assert(it != lastUploadValues.end() && "Double free or foreign allocation");

    requestedFrees.push_back(PendingFree {
        .allocation = allocation,
        .lastUploadValue = it->second,
    });
    lastUploadValues.erase(it);
}

void ChunkedBuffer::BeginFrame(uint32_t frameIndex) {
    uint64_t completedUploadValue = uploadQueue.GetCompletedValue();

    std::lock_guard lock(mutex);

    auto releaseOrWait = [&](PendingFree& pendingFree) {
        if (pendingFree.lastUploadValue <= completedUploadValue) {
            releaseLocked(pendingFree.allocation);
        } else {
            waitingOnUpload.push_back(pendingFree);
        }
    };

    std::vector<PendingFree> stillWaiting;
    stillWaiting.swap(waitingOnUpload);
    for (auto& pendingFree : stillWaiting) releaseOrWait(pendingFree);

    // Slot frameIndex's previous frame has finished (its fence was waited on), and so has every frame
    // before it, which covers every frame that could have been recorded before these frees were requested.
    for (auto& pendingFree : retiringFrees[frameIndex]) releaseOrWait(pendingFree);
    retiringFrees[frameIndex].clear();

    retiringFrees[frameIndex].swap(requestedFrees);
}

uint32_t ChunkedBuffer::GetChunkCount() const {
    std::lock_guard lock(mutex);
    return static_cast<uint32_t>(chunks.size());
}

dvk::Buffer::Ref ChunkedBuffer::GetChunk(uint32_t chunk) const {
    std::lock_guard lock(mutex);
    return chunks.at(chunk).buffer;
}

vk::DeviceSize ChunkedBuffer::GetElementSize() const {
    return desc.elementSize;
}

vk::DeviceSize ChunkedBuffer::GetByteOffset(const Allocation& allocation) const {
    return allocation.firstElement * desc.elementSize;
}

ChunkedBuffer::Chunk ChunkedBuffer::createChunkLocked() {
    const auto& device = uploadQueue.GetDevice();
    auto sharingQueueFamilies = uploadQueue.GetSharingQueueFamilies();
    vk::DeviceSize size = desc.elementsPerChunk * desc.elementSize;

    // No host access flags, so VMA places this in DEVICE_LOCAL memory rather than system RAM or the BAR.
    auto vmaBuffer = vma::raii::Buffer(device.GetAllocator(),
        vk::BufferCreateInfo {
            .size = size,
            .usage = desc.usage | vk::BufferUsageFlagBits::eTransferDst,
            // CONCURRENT across graphics and transfer families removes queue family ownership transfers.
            // For buffers this has no meaningful cost on desktop GPUs; it matters for image compression.
            .sharingMode = sharingQueueFamilies.size() > 1 ? vk::SharingMode::eConcurrent : vk::SharingMode::eExclusive,
            .queueFamilyIndexCount = sharingQueueFamilies.size() > 1 ? static_cast<uint32_t>(sharingQueueFamilies.size()) : 0u,
            .pQueueFamilyIndices = sharingQueueFamilies.size() > 1 ? sharingQueueFamilies.data() : nullptr,
        },
        vma::AllocationCreateInfo {
            .usage = vma::MemoryUsage::eAutoPreferDevice,
        });

#if defined(DVK_ENABLE_DEBUG_LABELS)
    device.GetDevice().setDebugUtilsObjectNameEXT(*vmaBuffer, desc.label + " Chunk " + std::to_string(chunks.size()));
#endif

    Chunk chunk;
    chunk.buffer = std::make_shared<dvk::Buffer>(std::move(vmaBuffer), size);

    VmaVirtualBlockCreateInfo blockInfo {
        .size = desc.elementsPerChunk,
    };
    if (vmaCreateVirtualBlock(&blockInfo, &chunk.block) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create virtual block for " + desc.label);
    }

    return chunk;
}

void ChunkedBuffer::releaseLocked(const Allocation& allocation) {
    vmaVirtualFree(chunks[allocation.chunk].block, allocation.handle);
}
