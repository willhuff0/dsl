#pragma once

#include "utils/UploadQueue.hpp"
#include "Provider.hpp"

#include <dvk/dvk.hpp>
#include <vk_mem_alloc.hpp>

#include <array>
#include <cstdint>
#include <mutex>
#include <string>
#include <map>
#include <utility>
#include <vector>

// Device-local storage for mostly static data, such as vertices, indices, materials and instances.
//
// Storage is a list of fixed-size chunks. Each chunk is one VkBuffer with a VMA virtual block suballocating
// it. Allocations are counted in elements, so an allocation's offset is always a multiple of elementSize.
// That makes firstElement directly usable as vertexOffset, firstIndex or a StructuredBuffer index, even for
// strides that aren't a power of two, such as a 56-byte vertex.
//
// Chunks never move or grow, so existing allocations and descriptors stay valid when a chunk is added.
// With maxChunks = 1 this is a single buffer, which the current renderer can bind as-is. With more chunks
// you'd bind or draw per chunk, or use buffer device addresses.
//
// Thread safety: Allocate, Upload and Free may be called from any thread. BeginFrame is render-thread only.
class ChunkedBuffer {
public:
    struct Desc {
        std::string label;
        vk::BufferUsageFlags usage;
        vk::DeviceSize elementSize = 1;
        uint32_t elementsPerChunk = 0;
        uint32_t maxChunks = 1;
    };

    struct Allocation {
        uint32_t chunk = ~0u;
        uint32_t firstElement = 0;
        uint32_t elementCount = 0;
        VmaVirtualAllocation handle = VK_NULL_HANDLE;

        explicit operator bool() const { return handle != VK_NULL_HANDLE; }
    };

    ChunkedBuffer(UploadQueue& uploadQueue, Desc desc);
    ~ChunkedBuffer();

    ChunkedBuffer(const ChunkedBuffer&) = delete;
    ChunkedBuffer& operator=(const ChunkedBuffer&) = delete;

    // Returns an empty Allocation when every chunk is full and maxChunks has been reached.
    [[nodiscard]] Allocation Allocate(uint32_t elementCount);

    // Writes elementCount elements starting elementOffset elements into the allocation.
    UploadTicket Upload(const Allocation& allocation, const void* src, uint32_t elementCount, uint32_t elementOffset = 0);

    // Empty Allocation and an empty ticket on failure.
    std::pair<Allocation, UploadTicket> AllocateAndUpload(const void* src, uint32_t elementCount);

    // Deferred: the range becomes reusable once no frame in flight can read it and every upload into it has
    // completed. The caller must stop recording commands that reference the allocation before calling this.
    void Free(const Allocation& allocation);

    // Call on the render thread after the frame fence for frameIndex has been waited on, i.e. after
    // dvk::CommandEncoder::WaitForNextFrame(), and before recording.
    void BeginFrame(uint32_t frameIndex);

    // Can increase when Allocate adds a chunk. Rebind descriptors or vertex buffers when it does.
    [[nodiscard]] uint32_t GetChunkCount() const;
    [[nodiscard]] dvk::Buffer::Ref GetChunk(uint32_t chunk) const;
    [[nodiscard]] vk::DeviceSize GetElementSize() const;
    [[nodiscard]] vk::DeviceSize GetByteOffset(const Allocation& allocation) const;

private:
    struct Chunk {
        dvk::Buffer::Ref buffer;
        VmaVirtualBlock block = VK_NULL_HANDLE;
    };

    struct PendingFree {
        Allocation allocation;
        uint64_t lastUploadValue;
    };

    Chunk createChunkLocked();
    void releaseLocked(const Allocation& allocation);

    UploadQueue& uploadQueue;
    Desc desc;

    mutable std::mutex mutex;
    std::vector<Chunk> chunks;

    // Last upload ticket written into each live allocation, so Free can't release a range the transfer queue
    // is still writing. Keyed by chunk too, since virtual allocation handles are only unique within a block.
    using AllocationKey = std::pair<uint32_t, VmaVirtualAllocation>;
    std::map<AllocationKey, uint64_t> lastUploadValues;

    // Frees requested since the last BeginFrame. They're tagged with the frame slot that begins next and
    // released when that slot comes around again, after its fence has been waited on. By then every frame
    // recorded before the Free has finished.
    std::vector<PendingFree> requestedFrees;
    std::array<std::vector<PendingFree>, FRAMES_IN_FLIGHT> retiringFrees;
    // Frame-safe, but an upload into the range is still in flight.
    std::vector<PendingFree> waitingOnUpload;
};
