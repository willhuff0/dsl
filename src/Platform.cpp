#include "Platform.hpp"

#include "Window.hpp"

#include <GLFW/glfw3.h>

#include <stdexcept>
#include <memory>

Platform::Platform() {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("Failed to initialize GLFW!");
    }
}
Platform::~Platform() {
    glfwTerminate();
}

std::vector<const char*> Platform::GetVulkanRequiredExtensions() const {
    uint32_t count;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);

    std::vector<const char*> result;
    result.reserve(count);
    for(uint32_t i = 0; i < count; i++) {
        result.emplace_back(extensions[i]);
    }

    return result;
}

bool Platform::ShouldQuit() {
    return false;
}

Window::Ref Platform::CreateWindow(int width, int height, const std::string& title) {
    return std::make_shared<Window>(width, height, title);
}

void Platform::ProcessEvents() {
    glfwPollEvents();
}