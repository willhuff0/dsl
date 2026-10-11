#include "Provider.hpp"

#include "Renderer.hpp"
#include "BindingProvider.hpp"

#include <utility>
#include <optional>

static Platform* platformBackend;
static dvk::Instance* graphicsInstance;
static dvk::Device* graphicsBackend;

static vk::Format graphicsSurfaceFormat;
static vk::SampleCountFlagBits sampleCount;

static dvk::CommandEncoder* graphicsCommandEncoder;
static dvk::BindPool::Ref bindPool;

static constexpr auto renderTextureFormat = vk::Format::eR16G16B16A16Sfloat;

static dvk::BindSetLayout::Ref debugTextureBindSetLayout;
static dvk::BindSet::Ref debugTextureBindSet;
static dvk::TextureView::Ref debugTextureView;

static std::optional<Renderer> renderer;

static vk::SampleCountFlagBits getMaxUsableSampleCount() {
    vk::PhysicalDeviceProperties physicalDeviceProperties = GetDevice().GetPhysicalDevice().getProperties();

    vk::SampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & vk::SampleCountFlagBits::e64) { return vk::SampleCountFlagBits::e64; }
    if (counts & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
    if (counts & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
    if (counts & vk::SampleCountFlagBits::e8) { return vk::SampleCountFlagBits::e8; }
    if (counts & vk::SampleCountFlagBits::e4) { return vk::SampleCountFlagBits::e4; }
    if (counts & vk::SampleCountFlagBits::e2) { return vk::SampleCountFlagBits::e2; }

    return vk::SampleCountFlagBits::e1;
}

void SetupProvider(Platform* platform, dvk::Instance* instance, dvk::Device* device, dvk::CommandEncoder* commandEncoder, vk::Format surfaceFormat) {
    platformBackend = platform;
    graphicsInstance = instance;
    graphicsBackend = device;
    graphicsCommandEncoder = commandEncoder;
    graphicsSurfaceFormat = surfaceFormat;

    bindPool = dvk::BindPool::Create(GetDevice())
        .SetLabel("Global")
        .SetMaxSets(32)
        .AddEntry(vk::DescriptorType::eStorageBuffer, 128)
        .AddEntry(vk::DescriptorType::eUniformBuffer, 128)
        .AddEntry(vk::DescriptorType::eSampledImage, 64)
        .AddEntry(vk::DescriptorType::eSampler, 32)
        .AddEntry(vk::DescriptorType::eStorageImage, 2)
        .Build();

    sampleCount = getMaxUsableSampleCount();

    debugTextureBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("Debug Texture Blit")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .Build();
    debugTextureBindSet = dvk::BindSet::Create(GetDevice(), GetBindPool(), debugTextureBindSetLayout, "Debug Texture Blit");
}

void SetupRenderer(BindingProvider* cameraProvider) {
    renderer.emplace(cameraProvider);
}

void FreeProvider() {
    renderer = std::nullopt;

    debugTextureBindSetLayout = nullptr;
    debugTextureBindSet = nullptr;
    debugTextureView = nullptr;

    bindPool = nullptr;
}

Platform* GetPlatform() { return platformBackend; }
dvk::Instance& GetInstance() { return *graphicsInstance; }
dvk::Device& GetDevice() { return *graphicsBackend; }
dvk::CommandEncoder& GetCommandEncoder() { return *graphicsCommandEncoder; }
dvk::BindPool::Ref& GetBindPool() { return bindPool; }
vk::Format GetRenderTextureFormat() { return renderTextureFormat; }
vk::Format GetDisplaySurfaceFormat() { return graphicsSurfaceFormat; }

vk::Format GetDepthFormat() { return vk::Format::eD32Sfloat; }
vk::SampleCountFlagBits GetMultisampleCount() { return sampleCount; }

void SetDebugTextureView(dvk::TextureView::Ref debugTextureView_) {
    debugTextureView = std::move(debugTextureView_);
    dvk::Binder()
        .BindTexture(0)
            .SetImageLayout(vk::ImageLayout::eGeneral)
            .SetTextureView(debugTextureView)
            .FinishTexture()
        .Update(debugTextureBindSet);
}
const dvk::TextureView::Ref& GetDebugTextureView() { return debugTextureView; }
const dvk::BindSetLayout::Ref& GetDebugTextureBindSetLayout() { return debugTextureBindSetLayout; }
const dvk::BindSet::Ref& GetDebugTextureBindSet() { return debugTextureBindSet; }

Renderer& GetRenderer() {
    return renderer.value();
}

