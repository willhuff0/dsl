#include "Window.hpp"

#include <stdexcept>
#include <string>

Window::Window(uint32_t _width, uint32_t _height, const std::string& title) {
    width = _width;
    height = _height;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    handle = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (handle == nullptr) {
        throw std::runtime_error("Failed to create GLFW window!");
    }

    glfwSetWindowUserPointer(handle, this);
    glfwSetFramebufferSizeCallback(handle, [](GLFWwindow* window, int width, int height) {
        const auto slglWindow = static_cast<Window*>(glfwGetWindowUserPointer(window));
        if (slglWindow == nullptr) return;
        slglWindow->onResize(width, height);
    });
}

Window::~Window() {
    glfwDestroyWindow(handle);
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(handle);
}

uint32_t Window::GetWidth() const {
    return width;
}

uint32_t Window::GetHeight() const {
    return height;
}

void* Window::GetNativeHandle() const {
    return handle;
}

void Window::SetTitle(const std::string& title) {
    glfwSetWindowTitle(handle, title.c_str());
}

GLFWwindow* Window::getHandle() const {
    return handle;
}

VkSurfaceKHR Window::GetVulkanSurface(const vk::raii::Instance& instance) const {
    VkSurfaceKHR surface;
    auto result = glfwCreateWindowSurface(*instance, handle, nullptr, &surface);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create window surface: " + std::to_string(result));
    }
    return surface;
}

void Window::onResize(uint32_t _width, uint32_t _height) {
    width = _width;
    height = _height;
    notifyWindowSizeChanged(width, height);
}