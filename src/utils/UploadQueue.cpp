#include <utils/UploadQueue.hpp>

#include <dvk/Config.hpp>

#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_map>

static constexpr vk::DeviceSize STAGING_ALIGNMENT = 16;

static vk::DeviceSize alignUp(vk::DeviceSize value, vk::DeviceSize alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

UploadQueue::UploadQueue(const dvk::Device& device, vk::DeviceSize stagingSize)
    : device(device),
      stagingSize(stagingSize),
      maxPieceSize(std::max<vk::DeviceSize>(alignUp(stagingSize / 4, STAGING_ALIGNMENT), STAGING_ALIGNMENT)) {
    sharingQueueFamilies[0] = device.GetQueueFamilyIndex();
    if (device.GetTransferQueueFamilyIndex() != device.GetQueueFamilyIndex()) {
        sharingQueueFamilies[1] = device.GetTransferQueueFamilyIndex();
        sharingQueueFamilyCount = 2;
    }

    vk::SemaphoreTypeCreateInfo timelineInfo {
        .semaphoreType = vk::SemaphoreType::eTimeline,
        .initialValue = 0,
    };
    timeline = vk::raii::Semaphore(device.GetDevice(), vk::SemaphoreCreateInfo {
        .pNext = &timelineInfo,
    });

    commandPool = vk::raii::CommandPool(device.GetDevice(), {
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient,
        .queueFamilyIndex = device.GetTransferQueueFamilyIndex(),
    });

    staging = dvk::Buffer::CreateStagingBuffer(device, stagingSize);
    stagingData = static_cast<std::byte*>(staging->GetMappedData());

#if defined(DVK_ENABLE_DEBUG_LABELS)
    device.GetDevice().setDebugUtilsObjectNameEXT(*timeline, "[Upload Queue] Timeline");
    device.GetDevice().setDebugUtilsObjectNameEXT(*commandPool, "[Upload Queue]");
    device.GetDevice().setDebugUtilsObjectNameEXT(*staging->GetBuffer(), "[Upload Queue] Staging Ring");
#endif
}

UploadQueue::~UploadQueue() {
    // Anything never submitted is dropped. Wait for everything that was.
    uint64_t value;
    {
        std::lock_guard lock(mutex);
        pending.clear();
        value = lastSubmittedValue;
    }
    waitValue(value);
}

UploadTicket UploadQueue::Enqueue(const dvk::Buffer::Ref& dst, vk::DeviceSize dstOffset, const void* src, vk::DeviceSize size) {
    UploadTicket ticket;
    auto* bytes = static_cast<const std::byte*>(src);

    while (size > 0) {
        vk::DeviceSize pieceSize = std::min(size, maxPieceSize);

        Reservation* reservation;
        {
            std::unique_lock lock(mutex);
            reservation = reserveLocked(lock, pieceSize);
        }

        // Fill outside the lock so other threads can reserve and fill concurrently. The ring can't reclaim
        // this range until its copy has been submitted and completed.
        std::memcpy(stagingData + reservation->begin, bytes, pieceSize);
        staging->Flush(reservation->begin, pieceSize);

        {
            std::lock_guard lock(mutex);
            pending.push_back(CopyRecord {
                .dst = dst,
                .srcOffset = reservation->begin,
                .dstOffset = dstOffset,
                .size = pieceSize,
                .reservation = reservation,
            });
            // Pending copies always go into the next batch submitted.
            ticket |= UploadTicket { lastSubmittedValue + 1 };
        }
        recordPushed.notify_all();

        bytes += pieceSize;
        dstOffset += pieceSize;
        size -= pieceSize;
    }

    return ticket;
}

UploadTicket UploadQueue::Submit() {
    std::lock_guard lock(mutex);
    return UploadTicket { submitLocked() };
}

bool UploadQueue::IsComplete(UploadTicket ticket) const {
    return ticket.value <= GetCompletedValue();
}

uint64_t UploadQueue::GetCompletedValue() const {
    return timeline.getCounterValue();
}

void UploadQueue::Wait(UploadTicket ticket) {
    {
        std::lock_guard lock(mutex);
        if (ticket.value > lastSubmittedValue) submitLocked();
    }
    waitValue(ticket.value);
}

vk::SemaphoreSubmitInfo UploadQueue::GetWaitInfo(UploadTicket ticket, vk::PipelineStageFlags2 stages) const {
    return {
        .semaphore = *timeline,
        .value = ticket.value,
        .stageMask = stages,
    };
}

std::span<const uint32_t> UploadQueue::GetSharingQueueFamilies() const {
    return { sharingQueueFamilies.data(), sharingQueueFamilyCount };
}

const dvk::Device& UploadQueue::GetDevice() const {
    return device;
}

UploadQueue::Reservation* UploadQueue::reserveLocked(std::unique_lock<std::mutex>& lock, vk::DeviceSize size) {
    for (;;) {
        reclaimLocked();

        vk::DeviceSize begin;
        if (tryAllocateLocked(size, begin)) {
            head = begin + size;
            return &reservations.emplace_back(Reservation { .begin = begin, .end = begin + size });
        }

        // Full. Get whatever is filled but unsubmitted moving so it can eventually retire.
        if (!pending.empty()) {
            submitLocked();
            continue;
        }

        // The ring is non-empty here, otherwise any size <= maxPieceSize would have fit.
        const auto& oldest = reservations.front();
        if (oldest.batchValue != 0) {
            uint64_t value = oldest.batchValue;
            lock.unlock();
            waitValue(value);
            lock.lock();
        } else {
            // Another thread is still copying into the oldest reservation.
            recordPushed.wait(lock);
        }
    }
}

bool UploadQueue::tryAllocateLocked(vk::DeviceSize size, vk::DeviceSize& begin) {
    if (reservations.empty()) {
        // Reset so the whole ring is one contiguous range.
        head = 0;
        begin = 0;
        return size <= stagingSize;
    }

    vk::DeviceSize tail = reservations.front().begin;
    vk::DeviceSize alignedHead = alignUp(head, STAGING_ALIGNMENT);

    if (tail < head) {
        // Live range is [tail, head). Free space is [head, end) and, wrapping, [0, tail).
        if (alignedHead + size <= stagingSize) {
            begin = alignedHead;
            return true;
        }
        if (size <= tail) {
            begin = 0;
            return true;
        }
        return false;
    }

    // Wrapped: live ranges are [tail, end) and [0, head). Free space is [head, tail).
    if (alignedHead + size <= tail) {
        begin = alignedHead;
        return true;
    }
    return false;
}

void UploadQueue::reclaimLocked() {
    uint64_t completed = GetCompletedValue();

    while (!reservations.empty()) {
        const auto& oldest = reservations.front();
        if (oldest.batchValue == 0 || oldest.batchValue > completed) break;
        reservations.pop_front();
    }

    while (!inFlight.empty() && inFlight.front().value <= completed) {
        freeCommandBuffers.push_back(std::move(inFlight.front().commandBuffer));
        inFlight.pop_front();
    }
}

uint64_t UploadQueue::submitLocked() {
    if (pending.empty()) return lastSubmittedValue;

    reclaimLocked();
    uint64_t value = lastSubmittedValue + 1;

    auto commandBuffer = acquireCommandBufferLocked();
    commandBuffer.begin({ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit });

    // Two copies writing the same bytes in one batch is a write-after-write hazard, e.g. re-uploading part
    // of an allocation before the first upload was submitted. Insert a barrier only when that happens.
    std::unordered_map<VkBuffer, std::vector<std::pair<vk::DeviceSize, vk::DeviceSize>>> writtenSinceBarrier;

    std::vector<dvk::Buffer::Ref> keepAlive;
    keepAlive.reserve(pending.size());

    vk::Buffer stagingBuffer = *staging->GetBuffer();
    for (auto& record : pending) {
        vk::Buffer dstBuffer = *record.dst->GetBuffer();
        vk::DeviceSize dstBegin = record.dstOffset;
        vk::DeviceSize dstEnd = record.dstOffset + record.size;

        auto& written = writtenSinceBarrier[static_cast<VkBuffer>(dstBuffer)];
        bool overlaps = std::ranges::any_of(written, [&](const auto& range) {
            return dstBegin < range.second && range.first < dstEnd;
        });
        if (overlaps) {
            vk::MemoryBarrier2 barrier {
                .srcStageMask = vk::PipelineStageFlagBits2::eCopy,
                .srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
                .dstStageMask = vk::PipelineStageFlagBits2::eCopy,
                .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
            };
            commandBuffer.pipelineBarrier2(vk::DependencyInfo {
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &barrier,
            });
            writtenSinceBarrier.clear();
        }
        writtenSinceBarrier[static_cast<VkBuffer>(dstBuffer)].emplace_back(dstBegin, dstEnd);

        commandBuffer.copyBuffer(stagingBuffer, dstBuffer, vk::BufferCopy {
            .srcOffset = record.srcOffset,
            .dstOffset = record.dstOffset,
            .size = record.size,
        });

        record.reservation->batchValue = value;
        keepAlive.push_back(std::move(record.dst));
    }

    commandBuffer.end();

    // The signal is the release half of the dependency: with stageMask = all commands it covers every copy
    // above and makes their writes available. The graphics queue's wait on this value is the acquire half.
    // With CONCURRENT sharing there's no queue family ownership transfer to pair with it.
    vk::CommandBufferSubmitInfo commandBufferInfo {
        .commandBuffer = *commandBuffer,
    };
    vk::SemaphoreSubmitInfo signal {
        .semaphore = *timeline,
        .value = value,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
    };
    device.GetTransferQueue().submit2(vk::SubmitInfo2 {
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &commandBufferInfo,
        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signal,
    });

    inFlight.push_back(InFlightBatch {
        .value = value,
        .commandBuffer = std::move(commandBuffer),
        .keepAlive = std::move(keepAlive),
    });
    pending.clear();
    lastSubmittedValue = value;
    return value;
}

vk::raii::CommandBuffer UploadQueue::acquireCommandBufferLocked() {
    if (!freeCommandBuffers.empty()) {
        auto commandBuffer = std::move(freeCommandBuffers.back());
        freeCommandBuffers.pop_back();
        commandBuffer.reset();
        return commandBuffer;
    }

    auto commandBuffers = vk::raii::CommandBuffers(device.GetDevice(), {
        .commandPool = *commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    });
    return std::move(commandBuffers.front());
}

void UploadQueue::waitValue(uint64_t value) const {
    if (value == 0) return;

    vk::Semaphore semaphore = *timeline;
    auto result = device.GetDevice().waitSemaphores(vk::SemaphoreWaitInfo {
        .semaphoreCount = 1,
        .pSemaphores = &semaphore,
        .pValues = &value,
    }, std::numeric_limits<uint64_t>::max());
    if (result != vk::Result::eSuccess) throw std::runtime_error("Failed to wait for upload timeline!");
}
