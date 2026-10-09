#include <HotReload.hpp>

#include <iostream>

void HotReload::Attempt(uint32_t frameIndex, const dvk::Device& device) {
    std::vector<std::string> changedPaths;
    for (const auto& path : _paths) {
        auto lastWriteTime = std::filesystem::last_write_time(path);

        auto& fileWatch = _pathToFileWatch[path];
        if (fileWatch.lastWriteTime != lastWriteTime) {
            changedPaths.emplace_back(path);
        }
        fileWatch.lastWriteTime = lastWriteTime;
    }

    if (changedPaths.empty()) return;

    std::cout << "Hot Reloading!" << std::endl;
    device.GetDevice().waitIdle();
    for (const auto& path : changedPaths) {
        const auto& fileWatch = _pathToFileWatch[path];
        for (const auto& callback : fileWatch.callbacks) {
            callback.second(frameIndex);
        }
    }
    device.GetDevice().waitIdle();
    std::cout << "Hot Reload Complete!" << std::endl;
}

Watcher HotReload::Watch(const std::string& path, const HotReloadCallback& callback) {
    auto lastWriteTime = std::filesystem::last_write_time(path);

    _paths.emplace_back(path);
    auto iter = _pathToFileWatch.emplace(path, FileWatch {
        .lastWriteTime = lastWriteTime,
    });
    uint64_t callbackId = iter.first->second.nextCallbackId++;
    iter.first->second.callbacks.emplace_back(callbackId, callback);

    return std::shared_ptr<void>(nullptr, [path, callbackId](void* _) {
        std::erase_if(_paths, [&](const auto& otherPath) {
            return path == otherPath;
        });
        std::erase_if(_pathToFileWatch[path].callbacks, [&](const std::pair<uint64_t, HotReloadCallback>& otherCallback) {
            return callbackId == otherCallback.first;
        });
        if (_pathToFileWatch[path].callbacks.empty()) {
            _pathToFileWatch.erase(path);
        }
    });
}

Watcher HotReload::CreateAndWatch(uint32_t frameIndex, const std::string& path, const HotReloadCallback& callback) {
    callback(frameIndex);
    return Watch(path, callback);
}

std::vector<std::string> HotReload::_paths;
std::unordered_map<std::string, HotReload::FileWatch> HotReload::_pathToFileWatch;
