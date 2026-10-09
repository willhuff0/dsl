#pragma once

#include "HotReload.hpp"

#include <string>
#include <functional>

using ShaderHotReloadCallback = std::function<void(uint32_t frameIndex, const std::string& compiledPath)>;
using ShaderHotReloadCallbackNoFrameIndex = std::function<void(const std::string& compiledPath)>;

class ShaderHotReload {
public:
    static Watcher CreateAndWatch(uint32_t frameIndex, const std::string& relativePath, const ShaderHotReloadCallback& callback);
    static Watcher CreateAndWatch(const std::string& relativePath, const ShaderHotReloadCallbackNoFrameIndex& callback);
};