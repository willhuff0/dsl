#pragma once

#include <dvk/dvk.hpp>

#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <span>
#include <vector>

// Value the upload timeline semaphore reaches once the batch containing an upload has finished on the
// transfer queue. 0 means "nothing to wait for". Tickets are monotonic, so several can be combined with |=.
struct UploadTicket {
    uint64_t value = 0;

    UploadTicket& operator|=(UploadTicket other) {
        value = std::max(value, other.value);
        return *this;
    }
};

// Asynchronous buffer uploads on the transfer queue.
//
// CPU side:
//   - Enqueue() is thread-safe. It copies the source data into a single shared, persistently mapped staging
//     ring and queues a copy command. It only blocks when the ring is full, in which case it submits what is
//     pending and waits for the oldest batch to retire.
//   - Submit() is thread-safe. It records every pending copy into one command buffer and submits it to the
//     transfer queue, signalling the timeline semaphore with the batch's ticket value.
//
// GPU side:
//   - Copies go straight from staging into the final device-local buffer on the transfer queue. Nothing is
//     copied again on the graphics queue.
//   - Destination buffers must be created with GetSharingQueueFamilies(): CONCURRENT sharing when the
//     transfer family differs from graphics. That removes the need for queue family ownership transfers.
//   - A graphics submit that reads uploaded data waits on the timeline with GetWaitInfo(). If the CPU only
//     publishes data whose ticket IsComplete(), that wait is already satisfied and costs nothing, but it is
//     still what makes the transfer queue's writes visible to the graphics queue.
//
// If the device has no separate transfer queue (dvk::Device::TransferQueueAliasesGraphics()), the transfer
// queue is the graphics queue, and Submit() (including the implicit submit in Enqueue() when the ring is
// full) must only run on the render thread.
class UploadQueue {
public:
    UploadQueue(const dvk::Device& device, vk::DeviceSize stagingSize);
    ~UploadQueue();

    UploadQueue(const UploadQueue&) = delete;
    UploadQueue& operator=(const UploadQueue&) = delete;

    // Uploads larger than a quarter of the staging ring are split into several copies.
    UploadTicket Enqueue(const dvk::Buffer::Ref& dst, vk::DeviceSize dstOffset, const void* src, vk::DeviceSize size);

    // Returns the ticket of the batch just submitted, or of the last submitted batch if nothing was pending.
    UploadTicket Submit();

    [[nodiscard]] bool IsComplete(UploadTicket ticket) const;
    [[nodiscard]] uint64_t GetCompletedValue() const;
    // Submits first if the ticket's batch is still pending.
    void Wait(UploadTicket ticket);

    [[nodiscard]] vk::SemaphoreSubmitInfo GetWaitInfo(
        UploadTicket ticket,
        vk::PipelineStageFlags2 stages = vk::PipelineStageFlagBits2::eAllCommands) const;

    // One family: graphics and transfer share a family, use EXCLUSIVE sharing.
    // Two families: create the destination with CONCURRENT sharing over both.
    [[nodiscard]] std::span<const uint32_t> GetSharingQueueFamilies() const;

    [[nodiscard]] const dvk::Device& GetDevice() const;

private:
    struct Reservation {
        vk::DeviceSize begin;
        vk::DeviceSize end;
        // 0 until the copy reading this range is submitted.
        uint64_t batchValue = 0;
    };

    struct CopyRecord {
        dvk::Buffer::Ref dst;
        vk::DeviceSize srcOffset;
        vk::DeviceSize dstOffset;
        vk::DeviceSize size;
        Reservation* reservation;
    };

    struct InFlightBatch {
        uint64_t value;
        vk::raii::CommandBuffer commandBuffer;
        // Destination buffers are kept alive until the copies into them have finished.
        std::vector<dvk::Buffer::Ref> keepAlive;
    };

    Reservation* reserveLocked(std::unique_lock<std::mutex>& lock, vk::DeviceSize size);
    bool tryAllocateLocked(vk::DeviceSize size, vk::DeviceSize& begin);
    void reclaimLocked();
    uint64_t submitLocked();
    vk::raii::CommandBuffer acquireCommandBufferLocked();
    void waitValue(uint64_t value) const;

    const dvk::Device& device;

    std::array<uint32_t, 2> sharingQueueFamilies{};
    uint32_t sharingQueueFamilyCount = 1;

    vk::raii::Semaphore timeline = nullptr;
    vk::raii::CommandPool commandPool = nullptr;

    dvk::Buffer::Ref staging;
    std::byte* stagingData = nullptr;
    vk::DeviceSize stagingSize;
    vk::DeviceSize maxPieceSize;

    mutable std::mutex mutex;
    std::condition_variable recordPushed;

    // Ring state. Live reservations are kept in allocation order, so the front is always the tail.
    // std::deque keeps references stable across push_back/pop_front, which CopyRecord::reservation relies on.
    std::deque<Reservation> reservations;
    vk::DeviceSize head = 0;

    std::vector<CopyRecord> pending;
    uint64_t lastSubmittedValue = 0;

    std::deque<InFlightBatch> inFlight;
    std::vector<vk::raii::CommandBuffer> freeCommandBuffers;
};
