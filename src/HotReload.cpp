#include <HotReload.hpp>

#include <thomasmonkman-filewatch/FileWatch.hpp>
#include <iostream>

WatcherInst::WatcherInst(const std::string& path, std::function<void(const std::string& path, const filewatch::Event event)> callback)
    : fileWatch(path, std::move(callback)) { }

// void HotReload::Trigger(const dvk::Device& device, const std::function<void()>& updator) {
//     std::cout << "Hot Reloading!" << std::endl;
//     device.GetDevice().waitIdle();
//     updator();
//     device.GetDevice().waitIdle();
//     std::cout << "Hot Reload Complete!" << std::endl;
// }

void HotReload::Attempt(const dvk::Device& device) {
    if (!hotReloadRequested) return;
    std::cout << "Hot Reloading!" << std::endl;
    std::lock_guard lock(updateQueueMutex);
    device.GetDevice().waitIdle();
    for (uint64_t watcherId : updateQueue) {
        updateQueueMap[watcherId]();
    }
    device.GetDevice().waitIdle();
    updateQueue.clear();
    updateQueueMap.clear();
    hotReloadRequested = false;
    std::cout << "Hot Reload Complete!" << std::endl;
}

Watcher HotReload::WatchCustom(const std::string& path, std::function<void(const std::string& path, const filewatch::Event event)> callback) {
    return std::make_unique<WatcherInst>(path, std::move(callback));
}

// Watcher HotReload::WatchExisting(const std::string& path, std::function<void()> updator) {
//     return std::make_unique<WatcherInst>(path, [updator = std::move(updator)](const std::string& path, const filewatch::Event event) {
//         HotReload::Trigger(device, updator);
//     });
// }
Watcher HotReload::CreateAndWatch(const std::string& path, std::function<void()> updator) {
    updator();
    return std::make_unique<WatcherInst>(path, [id = nextWatcherId++, updator = std::move(updator)](const std::string& path, const filewatch::Event event) {
        enqueue(id, updator);
    });
}

void HotReload::enqueue(uint64_t watcherId, std::function<void()> updator) {
    std::lock_guard lock(updateQueueMutex);

    auto iter = updateQueueMap.find(watcherId);
    if (iter != updateQueueMap.end()) {
        std::erase(updateQueue, watcherId);
    }

    updateQueue.push_back(watcherId);
    updateQueueMap[watcherId] = std::move(updator);

    hotReloadRequested = true;
}

std::atomic<uint64_t> HotReload::nextWatcherId;
std::atomic<bool> HotReload::hotReloadRequested;
std::mutex HotReload::updateQueueMutex;
std::vector<uint64_t> HotReload::updateQueue;
std::unordered_map<uint64_t, std::function<void()>> HotReload::updateQueueMap;
