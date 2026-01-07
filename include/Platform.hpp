#pragma once

#include "Window.hpp"

#include <string>
#include <vector>

class Platform {
public:
    Platform();
    ~Platform();

    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;
    Platform(Platform&&) = delete;
    Platform& operator=(Platform&&) = delete;

    [[nodiscard]] std::vector<const char*> GetVulkanRequiredExtensions() const;

    [[nodiscard]] bool ShouldQuit();

    [[nodiscard]] Window::Ref CreateWindow(int width, int height, const std::string& title);

    void ProcessEvents();
};
