#pragma once

#include "utils/ObserverPattern.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_raii.hpp>

#include <string>
#include <memory>

OBSERVER_PATTERN(WindowSize, uint32_t, uint32_t)

class Window : public WindowSizeSubject {
public:
    using Ref = std::shared_ptr<Window>;

    Window(uint32_t width, uint32_t height, const std::string& title);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] bool ShouldClose() const;

    [[nodiscard]] uint32_t GetWidth() const;
    [[nodiscard]] uint32_t GetHeight() const;
    [[nodiscard]] void* GetNativeHandle() const;

    void SetTitle(const std::string& title);

    [[nodiscard]] GLFWwindow* getHandle() const;

    [[nodiscard]] VkSurfaceKHR GetVulkanSurface(const vk::raii::Instance& instance) const;

private:
    GLFWwindow* handle;

    uint32_t width;
    uint32_t height;

    void onResize(uint32_t width, uint32_t height);
};