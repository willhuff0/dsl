#include <ShaderHotReload.hpp>

#include "utils/Paths.hpp"

#include <iostream>
#include <filesystem>

Watcher ShaderHotReload::CreateAndWatch(uint32_t frameIndex, const std::string& relativePath, const ShaderHotReloadCallback& callback) {
    std::string sourcePath = SHADER_SRC_BASEPATH + relativePath + ".slang";
    std::string compiledPath = SHADER_RES_BASEPATH + relativePath + ".spv";

    auto compileThenCallback = [sourcePath, compiledPath, callback](uint32_t frameIndex) {
        std::cout << "Compiling Shader '" + sourcePath + "'!" << std::endl;
        std::string command("slangc " + sourcePath + " -target spirv -matrix-layout-column-major -emit-spirv-directly -g2 -fvk-use-entrypoint-name -o " + compiledPath);
        int result = std::system(command.c_str());
        if (result == 0) {
            std::cout << "Shader Compiled '" << sourcePath << "'!"<< std::endl;
            callback(frameIndex, compiledPath);
        } else {
            std::cout << "Failed to Compiled Shader '" << sourcePath << "'!"<< std::endl;
        }
    };

    if (!std::filesystem::exists(compiledPath) ||
        std::filesystem::last_write_time(compiledPath) < std::filesystem::last_write_time(sourcePath)
    ) {
        std::filesystem::create_directories(std::filesystem::path(compiledPath).parent_path());
        compileThenCallback(frameIndex);
    } else {
        callback(frameIndex, compiledPath);
    }

    return HotReload::Watch(sourcePath, compileThenCallback);
}

Watcher ShaderHotReload::CreateAndWatch(const std::string& relativePath, const ShaderHotReloadCallbackNoFrameIndex& callback) {
    return CreateAndWatch(0, relativePath, [callback](auto _, const auto& path) {
       callback(path);
    });
}
