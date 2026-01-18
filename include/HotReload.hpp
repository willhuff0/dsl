#pragma once

#include <dvk/dvk.hpp>
#include <thomasmonkman-filewatch/FileWatch.hpp>
#undef CreateWindow
#undef LoadImage

#include <functional>
#include <string>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_map>

class WatcherInst {
public:
    WatcherInst(const std::string& path, std::function<void(const std::string& path, const filewatch::Event event)> callback);

private:
    filewatch::FileWatch<std::string> fileWatch;
};
using Watcher = std::shared_ptr<WatcherInst>;

class HotReload {
public:
    // static void Trigger(const dvk::Device& device, const std::function<void()>& updator);
    static void Attempt(const dvk::Device& device);

    static Watcher WatchCustom(const std::string& path, std::function<void(const std::string& path, const filewatch::Event event)> callback);
    // static Watcher WatchExisting(const std::string& path, std::function<void()> updator);
    static Watcher CreateAndWatch(const std::string& path, std::function<void()> updator);

private:
    static void enqueue(uint64_t watcherId, std::function<void()> updator);

    static std::atomic<uint64_t> nextWatcherId;
    static std::atomic<bool> hotReloadRequested;
    static std::mutex updateQueueMutex;
    static std::vector<uint64_t> updateQueue;
    static std::unordered_map<uint64_t, std::function<void()>> updateQueueMap;
};
