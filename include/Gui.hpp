#pragma once

#include <dvk/dvk.hpp>

#include "Window.hpp"

class Gui {
public:
    Gui(GLFWwindow* window, const dvk::Presenter& presenter);
    ~Gui();

    void Resize(const dvk::Presenter& presenter);

    void BeginFrame();
    void GuiRenderPass(dvk::CommandBuffer& commandBuffer, const vk::raii::ImageView& surfaceImageView, vk::Extent2D surfaceExtent);

    void DrawFpsOverlay();

private:
    dvk::BindPool::Ref _bindPool;
};
