#include <Gui.hpp>

#include "Provider.hpp"
#include "utils/Paths.hpp"

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_glfw.h>
#include <iostream>

Gui::Gui(GLFWwindow* window, const dvk::Presenter& presenter) {
    _bindPool = dvk::BindPool::Create(GetDevice())
        .SetFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
        .SetMaxSets(IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE + IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE)
        .AddEntry(vk::DescriptorType::eSampledImage, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE)
        .AddEntry(vk::DescriptorType::eSampler, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE)
        .Build();

    float displayScaling = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.FontDefault = io.Fonts->AddFontFromFileTTF(GUI_FONT_PATH, 18.0f);

    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(displayScaling);
    style.FontScaleDpi = displayScaling;

    ImGui_ImplGlfw_InitForVulkan(window, true);
    auto swapChainImageFormat = static_cast<VkFormat>(presenter.GetFormat());
    ImGui_ImplVulkan_InitInfo initInfo {
        .ApiVersion = dvk::API_VERSION,
        .Instance = *GetInstance().GetInstance(),
        .PhysicalDevice = *GetDevice().GetPhysicalDevice(),
        .Device = *GetDevice().GetDevice(),
        .QueueFamily = GetDevice().GetQueueFamilyIndex(),
        .Queue = *GetDevice().GetQueue(),
        .DescriptorPool = *_bindPool->GetDescriptorPool(),
        .MinImageCount = presenter.GetMinImageCount(),
        .ImageCount = presenter.GetSwapChainImageCount(),
        .PipelineInfoMain = {
            .PipelineRenderingCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &swapChainImageFormat,
            },
        },
        .UseDynamicRendering = true,
    };
    ImGui_ImplVulkan_Init(&initInfo);
}

Gui::~Gui() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Gui::Resize(const dvk::Presenter& presenter) {
    ImGui_ImplVulkan_SetMinImageCount(presenter.GetMinImageCount());
}

void Gui::BeginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Gui::GuiRenderPass(dvk::CommandBuffer& commandBuffer, const vk::raii::ImageView& surfaceImageView, vk::Extent2D surfaceExtent) {
    ImGui::Render();

    commandBuffer.BeginRendering()
        .SetRenderArea({0, 0}, surfaceExtent)
        .AddColorAttachment()
            .SetView(surfaceImageView)
            .SetOps(vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore)
            .FinishAttachment()
        .FinishBeginRendering();
    commandBuffer.SetViewport(
        {0.0f, 0.0f},
        {static_cast<float>(surfaceExtent.width), static_cast<float>(surfaceExtent.height)});
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *commandBuffer.GetCommandBuffer());
    commandBuffer.EndRendering();
}

void Gui::DrawFpsOverlay() {
    // 1 = Top-right, 2 = Bottom-left, 3 = Bottom-right, 0 = Top-left
    static int corner = 0;
    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDecoration |
                                    ImGuiWindowFlags_AlwaysAutoResize |
                                    ImGuiWindowFlags_NoSavedSettings |
                                    ImGuiWindowFlags_NoFocusOnAppearing |
                                    ImGuiWindowFlags_NoNav;

    if (corner != -1) {
        const float PAD = 10.0f;
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 work_pos = viewport->WorkPos;
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;

        window_pos.x = (corner & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
        window_pos.y = (corner & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
        window_pos_pivot.x = (corner & 1) ? 1.0f : 0.0f;
        window_pos_pivot.y = (corner & 2) ? 1.0f : 0.0f;

        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        windowFlags |= ImGuiWindowFlags_NoMove;
    }

    ImGui::SetNextWindowBgAlpha(0.35f);
    if (ImGui::Begin("FPS Overlay", nullptr, windowFlags)) {
        ImGui::Text("%.1f FPS", io.Framerate);
    }
    ImGui::End();
}
