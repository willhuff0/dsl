#pragma once

#include <dvk/dvk.hpp>

#include <functional>
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <filesystem>

using HotReloadCallback = std::function<void(uint32_t frameIndex)>;
using Watcher = std::shared_ptr<void>;

class HotReload {
public:
    static void Attempt(uint32_t frameIndex, const dvk::Device& device);

    static Watcher Watch(const std::string& path, const HotReloadCallback& callback);
    static Watcher CreateAndWatch(uint32_t frameIndex, const std::string& path, const HotReloadCallback& callback);

private:
    struct FileWatch {
        std::filesystem::file_time_type lastWriteTime;
        std::vector<std::pair<uint64_t, HotReloadCallback>> callbacks;
        uint64_t nextCallbackId = 0;
    };

    static std::vector<std::string> _paths;
    static std::unordered_map<std::string, FileWatch> _pathToFileWatch;
};
