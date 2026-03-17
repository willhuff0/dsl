#include "Provider.hpp"

#include <utility>

static Platform* platformBackend;
static dvk::Device* graphicsBackend;

static vk::Format graphicsSurfaceFormat;
static vk::SampleCountFlagBits sampleCount;

static dvk::CommandEncoder* graphicsCommandEncoder;
static dvk::BindPool::Ref bindPool;

static dvk::BindSetLayout::Ref globalBindSetLayout;
static dvk::Buffer::Refs<FRAMES_IN_FLIGHT> globalUniformsBuffer;
static std::array<dvk::BindSet::Ref, FRAMES_IN_FLIGHT> globalBindSet;
static dvk::Sampler::Ref environmentSampler;
static dvk::TextureView::Ref irradianceView;
static dvk::TextureView::Ref radianceView;
static dvk::TextureView::Ref brdfLutView;

static constexpr auto renderTextureFormat = vk::Format::eR16G16B16A16Sfloat;

static dvk::BindSetLayout::Ref debugTextureBindSetLayout;
static dvk::BindSet::Ref debugTextureBindSet;
static dvk::TextureView::Ref debugTextureView;

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

void SetupProvider(Platform* platform, dvk::Device* device, dvk::CommandEncoder* commandEncoder, vk::Format surfaceFormat) {
    platformBackend = platform;
    graphicsBackend = device;
    graphicsCommandEncoder = commandEncoder;
    graphicsSurfaceFormat = surfaceFormat;

    bindPool = dvk::BindPool::Create(GetDevice())
        .SetLabel("Global")
        .SetMaxSets(32)
        .AddEntry(vk::DescriptorType::eUniformBuffer, 128)
        .AddEntry(vk::DescriptorType::eSampledImage, 64)
        .AddEntry(vk::DescriptorType::eSampler, 32)
        .AddEntry(vk::DescriptorType::eStorageImage, 2)
        .Build();

    globalBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("Global")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eUniformBuffer)
            .SetStage(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eGeometry | vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(1)
            .SetType(vk::DescriptorType::eSampler)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(2)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(3)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(4)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .Build();

    globalUniformsBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel("Global Uniform Buffer")
        .SetSize(sizeof(GlobalUniforms))
        .SetUsage(vk::BufferUsageFlagBits::eUniformBuffer)
        .AllowRandomAccess()
        .EnablePersistentMapping()
        .Build<FRAMES_IN_FLIGHT>();

    environmentSampler = dvk::Sampler::Create(GetDevice())
        .SetLabel("Global Environment Sample")
        .SetAddressMode(vk::SamplerAddressMode::eClampToEdge)
        // .SetAnisotropy(false, 1.0f)
        .SetLodBounds(0.0f, 32.0f)
        // .SetBorderColor(vk::BorderColor::eFloatTransparentBlack)
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

void FreeProvider() {
    globalBindSetLayout = nullptr;
    environmentSampler = nullptr;
    debugTextureBindSetLayout = nullptr;
    debugTextureBindSet = nullptr;
    debugTextureView = nullptr;

    irradianceView = nullptr;
    radianceView = nullptr;
    brdfLutView = nullptr;

    globalUniformsBuffer.fill(nullptr);
    globalBindSet.fill(nullptr);

    bindPool = nullptr;
}

void SetGlobalUniforms(GlobalUniforms* uniforms, uint32_t frameIndex) {
    globalUniformsBuffer[frameIndex]->CopyToAndFlush(uniforms);
}

void SetSkybox(const dvk::Texture::Ref& irradianceMap,
               const dvk::Texture::Ref& radianceMap,
               const dvk::Texture::Ref& brdfLut) {
    irradianceView = dvk::TextureView::Create(GetDevice(), irradianceMap)
        .SetLabel("Global Irradiance CubeMap")
        .SetType(vk::ImageViewType::eCube)
        .Build();

    radianceView = dvk::TextureView::Create(GetDevice(), radianceMap)
        .SetLabel("Global Radiance CubeMap")
        .SetType(vk::ImageViewType::eCube)
        .Build();

    brdfLutView = dvk::TextureView::Create(GetDevice(), brdfLut)
        .SetLabel("Global Environment BRDF LUT")
        .Build();

    globalBindSet = dvk::BindSet::Create<FRAMES_IN_FLIGHT>(GetDevice(), GetBindPool(), globalBindSetLayout);
    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        dvk::Binder()
            .BindBuffer(0)
                .SetBufferType(vk::DescriptorType::eUniformBuffer)
                .SetBuffer(globalUniformsBuffer[i])
                .FinishBuffer()
            .BindSampler(1)
                .SetSampler(environmentSampler)
                .FinishSampler()
            .BindTexture(2)
                .SetTextureView(irradianceView)
                .FinishTexture()
            .BindTexture(3)
                .SetTextureView(radianceView)
                .FinishTexture()
            .BindTexture(4)
                .SetTextureView(brdfLutView)
                .FinishTexture()
            .Update(globalBindSet[i]);
    }
}

void BindGlobals(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) {
    commandBuffer.SetBindSet(0, globalBindSet[frameIndex]);
}

dvk::BindSetLayout::Ref GetGlobalBindSetLayout() { return globalBindSetLayout; }

Platform* GetPlatform() { return platformBackend; }
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