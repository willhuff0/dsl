#include <Skybox.hpp>

#include <Provider.hpp>
#include <utils/Paths.hpp>

#include <string>
#include <utility>

#include "dvk/CommandBuffer.hpp"

static constexpr int IRRADIANCE_MAP_SIZE = 64;
static constexpr int RADIANCE_MAP_SIZE = 512;
static constexpr int BRDF_LUT_SIZE = 512;
static const uint32_t RADIANCE_MIPMAP_COUNT = static_cast<uint32_t>(std::floor(std::log2(RADIANCE_MAP_SIZE))) + 1;

static constexpr float cubeVertexPositions[] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
    1.0f,  1.0f, -1.0f,
    1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
    1.0f, -1.0f, -1.0f,
    1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
    1.0f, -1.0f,  1.0f
};

static std::vector<vk::VertexInputBindingDescription> getCubeVertexBindingDescriptions() {
    return {
        vk::VertexInputBindingDescription {
            .binding = 0,
            .stride = sizeof(float) * 3,
            .inputRate = vk::VertexInputRate::eVertex,
        },
    };
}

static std::vector<vk::VertexInputAttributeDescription> getCubeVertexAttributeDescriptions() {
    return {
        vk::VertexInputAttributeDescription {
            .location = 0,
            .binding = 0,
            .format = vk::Format::eR32G32B32Sfloat,
            .offset = 0,
        },
    };
}

CubeMapConverter::CubeMapConverter() {
    perFaceBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("CubeMapConverter Per Face")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eUniformBuffer)
            .SetStage(vk::ShaderStageFlagBits::eVertex)
            .FinishEntry()
        .Build();

    equirectangularToCubeMapPerMapBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("CubeMapConverter EquirectangularToCubeMap Per Map")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(1)
            .SetType(vk::DescriptorType::eSampler)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .Build();

    convolutionPerMapBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("CubeMapConverter Convolution Per Map")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(1)
            .SetType(vk::DescriptorType::eSampler)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(2)
            .SetType(vk::DescriptorType::eUniformBuffer)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .Build();

    auto equirectangularToCubeMapShaderPipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("CubeMapConverter EquirectangularToCubeMap")
        .AddBindSetLayout(perFaceBindSetLayout)
        .AddBindSetLayout(equirectangularToCubeMapPerMapBindSetLayout)
        .Build();

    auto convolutionShaderPipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("CubeMapConverter Convolution")
        .AddBindSetLayout(perFaceBindSetLayout)
        .AddBindSetLayout(convolutionPerMapBindSetLayout)
        .Build();

    auto vertexShader = dvk::Shader::LoadFile(GetDevice(), SB_CUBE_MAP_CONVERTER_VERTEX_SHADER_PATH, "CubeMapConverter Vertex");
    auto equirectangularToCubeMapShader = dvk::Shader::LoadFile(GetDevice(), SB_EQUIRECTANGULAR_TO_CUBE_MAP_SHADER_PATH, "CubeMapConverter EquirectangularToCubeMap Fragment");
    auto irradianceShader = dvk::Shader::LoadFile(GetDevice(), SB_IRRADIANCE_SHADER_PATH, "CubeMapConverter Irradiance Fragment");
    auto radianceShader = dvk::Shader::LoadFile(GetDevice(), SB_RADIANCE_SHADER_PATH, "CubeMapConverter Radiance Fragment");

    equirectangularToCubeMapShaderPipeline = dvk::GraphicsPipeline::Create(GetDevice(), equirectangularToCubeMapShaderPipelineLayout)
        .SetLabel("CubeMapConverter EquirectangularToCubeMap")
        .SetVertexInput(getCubeVertexBindingDescriptions(), getCubeVertexAttributeDescriptions())
        .AddStage(vk::ShaderStageFlagBits::eVertex, vertexShader, "vs_main").FinishStage()
        .AddStage(vk::ShaderStageFlagBits::eFragment, equirectangularToCubeMapShader, "fs_main").FinishStage()
        .SetSingleAttachment(vk::Format::eR16G16B16A16Sfloat)
        .Build();

    irradianceShaderPipeline = dvk::GraphicsPipeline::Create(GetDevice(), convolutionShaderPipelineLayout)
        .SetLabel("CubeMapConverter EquirectangularToCubeMap")
        .SetVertexInput(getCubeVertexBindingDescriptions(), getCubeVertexAttributeDescriptions())
        .AddStage(vk::ShaderStageFlagBits::eVertex, vertexShader, "vs_main").FinishStage()
        .AddStage(vk::ShaderStageFlagBits::eFragment, irradianceShader, "fs_main").FinishStage()
        .SetSingleAttachment(vk::Format::eR16G16B16A16Sfloat)
        .Build();

    radianceShaderPipeline = dvk::GraphicsPipeline::Create(GetDevice(), convolutionShaderPipelineLayout)
        .SetLabel("CubeMapConverter EquirectangularToCubeMap")
        .SetVertexInput(getCubeVertexBindingDescriptions(), getCubeVertexAttributeDescriptions())
        .AddStage(vk::ShaderStageFlagBits::eVertex, vertexShader, "vs_main").FinishStage()
        .AddStage(vk::ShaderStageFlagBits::eFragment, radianceShader, "fs_main").FinishStage()
        .SetSingleAttachment(vk::Format::eR16G16B16A16Sfloat)
        .Build();

    positionBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel("CubeMapConverter Position Buffer")
        .SetUsage(vk::BufferUsageFlagBits::eVertexBuffer)
        .SetSize(sizeof(cubeVertexPositions))
        .EnableWriteWithStaging()
        .Build();
    positionBuffer->WriteWithStaging(GetCommandEncoder(), cubeVertexPositions);

    sampler = dvk::Sampler::Create(GetDevice())
        .SetLabel("CubeMapConverter")
        .SetAddressMode(vk::SamplerAddressMode::eClampToEdge)
        .Build();

    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    captureProjection[1][1] *= -1;
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };
    CubeMapCaptureUniforms captureUniforms[] {
        { .viewProjMatrix = captureProjection * captureViews[0] },
        { .viewProjMatrix = captureProjection * captureViews[1] },
        { .viewProjMatrix = captureProjection * captureViews[2] },
        { .viewProjMatrix = captureProjection * captureViews[3] },
        { .viewProjMatrix = captureProjection * captureViews[4] },
        { .viewProjMatrix = captureProjection * captureViews[5] },
    };

    faceUniformBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel("CubeMapConverter Uniform Buffer")
        .SetUsage(vk::BufferUsageFlagBits::eUniformBuffer)
        .SetSize(sizeof(captureUniforms))
        .EnableWriteWithStaging()
        .Build();
    faceUniformBuffer->WriteWithStaging(GetCommandEncoder(), captureUniforms);

    faceBindSets = dvk::BindSet::Create<6>(GetDevice(), GetBindPool(), perFaceBindSetLayout, "CubeMapConverter Face");

    for (int i = 0; i < 6; i++) {
        dvk::Binder()
            .BindBuffer(0)
                .SetBufferType(vk::DescriptorType::eUniformBuffer)
                .SetBuffer(faceUniformBuffer)
                .SetRange(i * sizeof(CubeMapCaptureUniforms), sizeof(CubeMapCaptureUniforms))
                .FinishBuffer()
            .Update(faceBindSets[i]);
    }

    convolutionUniformBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel("CubeMapConverter Convolution Uniform Buffer")
        .SetUsage(vk::BufferUsageFlagBits::eUniformBuffer)
        .SetSize(sizeof(ConvolutionUniforms))
        .EnableCopyToAndFlush()
        .Build();

    equirectangularToCubeMapPerMapBindSet = dvk::BindSet::Create(GetDevice(), GetBindPool(), equirectangularToCubeMapPerMapBindSetLayout, "CubeMapConverter ConvertEquirectangularToCubeMap Per Map");
    convolutionPerMapBindSet = dvk::BindSet::Create(GetDevice(), GetBindPool(), convolutionPerMapBindSetLayout, "CubeMapConverter Irradiance/Radiance Per Map");
}

CubeMap CubeMapConverter::ConvertEquirectangularToCubeMap(const dvk::Texture::Ref& equirectangularMap, const std::string& label) {
    uint32_t cubeMapFaceSize = equirectangularMap->GetExtent().height;
    // Mirrors the Dart `CubeMap.fromHdri`, which calls `generateMipmap` on the
    // environment cube map. Those mips are required for the radiance prefilter to
    // sample lower-resolution levels and avoid firefly/bright-dot artifacts.
    uint32_t mipLevelCount = static_cast<uint32_t>(std::floor(std::log2(cubeMapFaceSize))) + 1;

    auto cubeMapTexture = dvk::Texture::Create(GetDevice())
        .SetLabel(label)
        .SetUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst)
        .SetFormat(vk::Format::eR16G16B16A16Sfloat)
        .SetExtent({ cubeMapFaceSize, cubeMapFaceSize, 1 })
        .SetMipLevelCount(mipLevelCount)
        .AllowCubeViews()
        .Build();

    auto equirectangularMapView = dvk::TextureView::Create(GetDevice(), equirectangularMap).Build();

    dvk::Binder()
        .BindTexture(0)
            .SetTextureView(equirectangularMapView)
            .FinishTexture()
        .BindSampler(1)
            .SetSampler(sampler)
            .FinishSampler()
        .Update(equirectangularToCubeMapPerMapBindSet);

    // Render into mip 0 only; leave the image in a color-attachment layout so the
    // mipmap generator can take over the layout transitions.
    capture(equirectangularToCubeMapPerMapBindSet, cubeMapTexture, equirectangularToCubeMapShaderPipeline, 0, true, false);
    generateCubeMipmaps(cubeMapTexture, cubeMapFaceSize, mipLevelCount);

    return CubeMap(cubeMapTexture, cubeMapFaceSize, mipLevelCount);
}

CubeMap CubeMapConverter::GenerateIrradianceMap(const CubeMap& cubeMap, const std::string& label) {
    auto irradianceMap = dvk::Texture::Create(GetDevice())
        .SetLabel(label)
        .SetUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment)
        .SetFormat(cubeMap.GetTexture()->GetFormat())
        .SetExtent({ IRRADIANCE_MAP_SIZE, IRRADIANCE_MAP_SIZE, 1 })
        .AllowCubeViews()
        .Build();

    auto cubeMapView = cubeMap.CreateCubeView("Irradiance Source CubeMap View");

    dvk::Binder()
        .BindTexture(0)
            .SetTextureView(cubeMapView)
            .FinishTexture()
        .BindSampler(1)
            .SetSampler(sampler)
            .FinishSampler()
        .BindBuffer(2)
            .SetBuffer(convolutionUniformBuffer)
            .SetRange(0, sizeof(ConvolutionUniforms))
            .FinishBuffer()
        .Update(convolutionPerMapBindSet);

    capture(convolutionPerMapBindSet, irradianceMap, irradianceShaderPipeline);

    return CubeMap(irradianceMap, IRRADIANCE_MAP_SIZE, 1);
}

CubeMap CubeMapConverter::GenerateRadianceMap(const CubeMap& cubeMap, const std::string& label) {
    auto radianceMap = dvk::Texture::Create(GetDevice())
        .SetLabel(label)
        .SetUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment)
        .SetFormat(cubeMap.GetTexture()->GetFormat())
        .SetExtent({ RADIANCE_MAP_SIZE, RADIANCE_MAP_SIZE, 1 })
        .SetMipLevelCount(RADIANCE_MIPMAP_COUNT)
        .AllowCubeViews()
        .Build();

    auto cubeMapView = cubeMap.CreateCubeView("Radiance Source CubeMap View");

    dvk::Binder()
        .BindTexture(0)
            .SetTextureView(cubeMapView)
            .FinishTexture()
        .BindSampler(1)
            .SetSampler(sampler)
            .FinishSampler()
        .BindBuffer(2)
            .SetBuffer(convolutionUniformBuffer)
            .SetRange(0, sizeof(ConvolutionUniforms))
            .FinishBuffer()
        .Update(convolutionPerMapBindSet);

    for (int i = 0; i < RADIANCE_MIPMAP_COUNT; i++) {
        ConvolutionUniforms uniforms {
            .roughness = static_cast<float>(i) / static_cast<float>(RADIANCE_MIPMAP_COUNT - 1),
            .resolution = static_cast<float>(cubeMap.GetFaceSize()),
        };
        convolutionUniformBuffer->CopyToAndFlush(&uniforms);
        capture(convolutionPerMapBindSet, radianceMap, radianceShaderPipeline, i, i == 0, i == RADIANCE_MIPMAP_COUNT - 1);
    }

    return CubeMap(radianceMap, RADIANCE_MAP_SIZE, RADIANCE_MIPMAP_COUNT);
}

void CubeMapConverter::generateCubeMipmaps(const dvk::Texture::Ref& target, uint32_t faceSize, uint32_t mipLevelCount) {
    if (mipLevelCount <= 1) {
        GetCommandEncoder().ExecuteAndWait([&](dvk::CommandBuffer& commandBuffer) {
            target->SimpleLayoutBarrier(commandBuffer, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        }, "CubeMapConverter Generate Mipmaps");
        return;
    }

    GetCommandEncoder().ExecuteAndWait([&](dvk::CommandBuffer& commandBuffer) {
        // mip 0 (all faces) was produced as a color attachment; make it a blit source.
        commandBuffer.InsertBarriers()
            .AddImageBarrier()
                .SetImage(target)
                .SetMipLevels(0, 1)
                .SetArrayLayers(0, 6)
                .SetLayout(vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eTransferSrcOptimal)
                .SetStages(vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::PipelineStageFlagBits2::eTransfer)
                .SetAccess(vk::AccessFlagBits2::eColorAttachmentWrite, vk::AccessFlagBits2::eTransferRead)
                .FinishImageBarrier()
            .FinishBarriers();

        // The remaining mips are undefined; make them blit destinations.
        commandBuffer.InsertBarriers()
            .AddImageBarrier()
                .SetImage(target)
                .SetMipLevels(1, mipLevelCount - 1)
                .SetArrayLayers(0, 6)
                .SetLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal)
                .SetStages(vk::PipelineStageFlagBits2::eTopOfPipe, vk::PipelineStageFlagBits2::eTransfer)
                .SetAccess({}, vk::AccessFlagBits2::eTransferWrite)
                .FinishImageBarrier()
            .FinishBarriers();

        for (uint32_t mip = 1; mip < mipLevelCount; mip++) {
            int32_t srcSize = static_cast<int32_t>(std::max(1u, faceSize >> (mip - 1)));
            int32_t dstSize = static_cast<int32_t>(std::max(1u, faceSize >> mip));

            vk::ImageBlit blit {
                .srcSubresource = { vk::ImageAspectFlagBits::eColor, mip - 1, 0, 6 },
                .srcOffsets = std::array<vk::Offset3D, 2>{ vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ srcSize, srcSize, 1 } },
                .dstSubresource = { vk::ImageAspectFlagBits::eColor, mip, 0, 6 },
                .dstOffsets = std::array<vk::Offset3D, 2>{ vk::Offset3D{ 0, 0, 0 }, vk::Offset3D{ dstSize, dstSize, 1 } },
            };

            commandBuffer.GetCommandBuffer().blitImage(
                target->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
                target->GetImage(), vk::ImageLayout::eTransferDstOptimal,
                { blit }, vk::Filter::eLinear);

            // This mip becomes the source for the next iteration.
            commandBuffer.InsertBarriers()
                .AddImageBarrier()
                    .SetImage(target)
                    .SetMipLevels(mip, 1)
                    .SetArrayLayers(0, 6)
                    .SetLayout(vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal)
                    .SetStages(vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eTransfer)
                    .SetAccess(vk::AccessFlagBits2::eTransferWrite, vk::AccessFlagBits2::eTransferRead)
                    .FinishImageBarrier()
                .FinishBarriers();
        }

        // Every mip is now a transfer source; move the whole chain to shader-read.
        commandBuffer.InsertBarriers()
            .AddImageBarrier()
                .SetImage(target)
                .SetMipLevels(0, mipLevelCount)
                .SetArrayLayers(0, 6)
                .SetLayout(vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::eShaderReadOnlyOptimal)
                .SetStages(vk::PipelineStageFlagBits2::eTransfer, vk::PipelineStageFlagBits2::eFragmentShader)
                .SetAccess(vk::AccessFlagBits2::eTransferRead, vk::AccessFlagBits2::eShaderSampledRead)
                .FinishImageBarrier()
            .FinishBarriers();
    }, "CubeMapConverter Generate Mipmaps");
}

void CubeMapConverter::capture(const dvk::BindSet::Ref& perMapBindSet, const dvk::Texture::Ref& target, dvk::GraphicsPipeline::Ref graphicsPipeline, int mipMap, bool transitionLayoutStart , bool transitionLayoutEnd) {
    vk::Extent2D targetExtent = { std::max(1u, target->GetExtent().width >> mipMap), std::max(1u, target->GetExtent().height >> mipMap) };

    std::array<dvk::TextureView::Ref, 6> cubeMapTextureViews;
    for (int i = 0; i < 6; i++) {
        cubeMapTextureViews[i] = dvk::TextureView::Create(GetDevice(), target)
            .SetLabel("CubeMapConverter Capture Result View (" + std::to_string(i) + ")")
            .SetArrayLayer(i)
            .SetMipLevel(mipMap)
            .Build();
    }

    GetCommandEncoder().ExecuteAndWait([&](dvk::CommandBuffer& commandBuffer) {
        if (transitionLayoutStart) {
            target->SimpleLayoutBarrier(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
        }

        for (int i = 0; i < 6; i++) {
            commandBuffer.BeginRendering()
                .SetColorAttachmentSimple(cubeMapTextureViews[i])
                .SetRenderArea({0, 0}, targetExtent)
                .FinishBeginRendering();

            commandBuffer.SetViewport({0.0, 0.0}, {static_cast<float>(targetExtent.width), static_cast<float>(targetExtent.height)});
            commandBuffer.SetScissor({0, 0}, targetExtent);
            commandBuffer.SetGraphicsPipeline(graphicsPipeline);
            commandBuffer.SetBindSet(0, faceBindSets[i]);
            commandBuffer.SetBindSet(1, perMapBindSet);
            commandBuffer.SetVertexBuffer(0, positionBuffer);
            commandBuffer.Draw(36);

            commandBuffer.EndRendering();
        }

        if (transitionLayoutEnd) {
            target->SimpleLayoutBarrier(commandBuffer, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    }, "CubeMapConverter Capture");
}

CubeMap::CubeMap(dvk::Texture::Ref texture, uint32_t faceSize, uint32_t mipLevelCount)
    : texture(std::move(texture)), faceSize(faceSize), mipLevelCount(mipLevelCount) { }

CubeMap CubeMap::FromEquirectangular(CubeMapConverter& converter, const dvk::Texture::Ref& equirectangularMap, const std::string& label) {
    return converter.ConvertEquirectangularToCubeMap(equirectangularMap, label);
}

CubeMap CubeMap::GenerateIrradianceMap(CubeMapConverter& converter, const std::string& label) const {
    return converter.GenerateIrradianceMap(*this, label);
}

CubeMap CubeMap::GenerateRadianceMap(CubeMapConverter& converter, const std::string& label) const {
    return converter.GenerateRadianceMap(*this, label);
}

dvk::TextureView::Ref CubeMap::CreateCubeView(const std::string& label) const {
    return dvk::TextureView::Create(GetDevice(), texture)
        .SetLabel(label)
        .SetType(vk::ImageViewType::eCube)
        .SetArrayLayer(0, 6)
        .SetMipLevel(0, mipLevelCount)
        .Build();
}

EnvBrdfLutGenerator::EnvBrdfLutGenerator() {
    auto pipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("Environment BRDF LUT Generator")
        .Build();

    auto shader = dvk::Shader::LoadFile(GetDevice(), SB_ENV_BRDF_LUT_GENERATOR_SHADER_PATH, "Environment BRDF LUT Generator");

    graphicsPipeline = dvk::GraphicsPipeline::Create(GetDevice(), pipelineLayout)
        .AddStage(vk::ShaderStageFlagBits::eVertex, shader, "vs_main").FinishStage()
        .AddStage(vk::ShaderStageFlagBits::eFragment, shader, "fs_main").FinishStage()
        .SetSingleAttachment(vk::Format::eR16G16Sfloat)
        .Build();
}

dvk::Texture::Ref EnvBrdfLutGenerator::GenerateBrdfLut(const std::string& label) {
    auto texture = dvk::Texture::Create(GetDevice())
        .SetLabel(label)
        .SetUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment)
        .SetFormat(vk::Format::eR16G16Sfloat)
        .SetExtent({ BRDF_LUT_SIZE, BRDF_LUT_SIZE, 1 })
        .Build();

    auto textureView = dvk::TextureView::Create(GetDevice(), texture).Build();

    GetCommandEncoder().ExecuteAndWait([&](dvk::CommandBuffer& commandBuffer) {
        texture->SimpleLayoutBarrier(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

        commandBuffer.BeginRendering()
            .SetColorAttachmentSimple(textureView)
            .SetRenderArea({0, 0}, {BRDF_LUT_SIZE, BRDF_LUT_SIZE})
            .FinishBeginRendering();

        commandBuffer.SetViewport({0.0, 0.0}, {static_cast<float>(BRDF_LUT_SIZE), static_cast<float>(BRDF_LUT_SIZE)});
        commandBuffer.SetScissor({0, 0}, {BRDF_LUT_SIZE, BRDF_LUT_SIZE});
        commandBuffer.SetGraphicsPipeline(graphicsPipeline);
        commandBuffer.Draw(6);

        commandBuffer.EndRendering();

        texture->SimpleLayoutBarrier(commandBuffer, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    }, "EnvBrdfLutGenerator " + label);

    return texture;
}

Skybox::Skybox(
    const CubeMap& environmentMap,
    const CubeMap& irradianceMap,
    const CubeMap& radianceMap,
    const dvk::Texture::Ref& brdfLut,
    BindingProvider* cameraProvider
) {
    positionBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel("Skybox Position Buffer")
        .SetUsage(vk::BufferUsageFlagBits::eVertexBuffer)
        .SetSize(sizeof(cubeVertexPositions))
        .EnableWriteWithStaging()
        .Build();
    positionBuffer->WriteWithStaging(GetCommandEncoder(), cubeVertexPositions);

    environmentBindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("Environment Bind Set Layout")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eUniformBuffer)
            .SetStage(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(1)
            .SetType(vk::DescriptorType::eSampler)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(2)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry(4)
        .Build();

    environmentUniformsBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel("Environment Uniform Buffer")
        .SetSize(sizeof(EnvironmentUniforms))
        .SetUsage(vk::BufferUsageFlagBits::eUniformBuffer)
        .EnableWriteWithStaging()
        .Build();
    EnvironmentUniforms uniforms {
        .radianceMipMapCount = RADIANCE_MIPMAP_COUNT,
    };
    environmentUniformsBuffer->WriteWithStaging(GetCommandEncoder(), &uniforms);

    environmentSampler = dvk::Sampler::Create(GetDevice())
        .SetLabel("Environment Sampler")
        .SetAddressMode(vk::SamplerAddressMode::eClampToEdge)
        .SetLodBounds(0.0f, 32.0f)
        .Build();
    environmentView = dvk::TextureView::Create(GetDevice(), environmentMap.GetTexture())
        .SetLabel("Global Environment CubeMap")
        .SetType(vk::ImageViewType::eCube)
        .Build();
    irradianceView = dvk::TextureView::Create(GetDevice(), irradianceMap.GetTexture())
        .SetLabel("Global Irradiance CubeMap")
        .SetType(vk::ImageViewType::eCube)
        .Build();
    radianceView = dvk::TextureView::Create(GetDevice(), radianceMap.GetTexture())
        .SetLabel("Global Radiance CubeMap")
        .SetType(vk::ImageViewType::eCube)
        .Build();
    brdfLutView = dvk::TextureView::Create(GetDevice(), brdfLut)
        .SetLabel("Global Environment BRDF LUT")
        .Build();

    environmentBindSet = dvk::BindSet::Create(GetDevice(), GetBindPool(), environmentBindSetLayout);
    dvk::Binder()
        .BindBuffer(0)
            .SetBufferType(vk::DescriptorType::eUniformBuffer)
            .SetBuffer(environmentUniformsBuffer)
            .FinishBuffer()
        .BindSampler(1)
            .SetSampler(environmentSampler)
            .FinishSampler()
        .BindTexture(2)
            .SetTextureView(environmentView)
            .FinishTexture()
        .BindTexture(3)
            .SetTextureView(irradianceView)
            .FinishTexture()
        .BindTexture(4)
            .SetTextureView(radianceView)
            .FinishTexture()
        .BindTexture(5)
            .SetTextureView(brdfLutView)
            .FinishTexture()
        .Update(environmentBindSet);

    auto pipelineLayout = dvk::PipelineLayout::Create(GetDevice())
    .SetLabel("Skybox Layout")
    .AddBindSetLayout(cameraProvider->GetLayout())
    .AddBindSetLayout(environmentBindSetLayout)
    .Build();
    graphicsPipelineWatcher = ShaderHotReload::CreateAndWatch(SB_SKYBOX_SHADER_PATH, [this, pipelineLayout](const auto& path) {
        auto shader = dvk::Shader::LoadFile(GetDevice(), path, "Skybox Module");
        graphicsPipeline = dvk::GraphicsPipeline::Create(GetDevice(), pipelineLayout)
            .SetLabel("Skybox Pipeline")
            .SetVertexInput(getCubeVertexBindingDescriptions(), getCubeVertexAttributeDescriptions())
            .AddStage(vk::ShaderStageFlagBits::eVertex, shader, "vs_main").FinishStage()
            .AddStage(vk::ShaderStageFlagBits::eFragment, shader, "fs_main").FinishStage()
            .SetSingleAttachment(GetRenderTextureFormat())
            .SetDepth()
                .SetFormat(GetDepthFormat())
                .EnableDepthTest(true)
                .EnableDepthWrite(false)
                .SetDepthCompare(vk::CompareOp::eLessOrEqual)
                .FinishDepth()
            .EnableMultisample(GetMultisampleCount())
            .Build();
    });
}

Skybox Skybox::FromEquirectangularMap(
    BindingProvider* cameraProvider,
    const dvk::Texture::Ref& equirectangularMap
) {
    CubeMapConverter converter;
    CubeMap environmentMap = CubeMap::FromEquirectangular(converter, equirectangularMap);
    CubeMap irradianceMap = environmentMap.GenerateIrradianceMap(converter);
    CubeMap radianceMap = environmentMap.GenerateRadianceMap(converter);

    EnvBrdfLutGenerator brdfLutGenerator;
    auto brdfLut = brdfLutGenerator.GenerateBrdfLut();

    return Skybox(
        environmentMap,
        irradianceMap,
        radianceMap,
        brdfLut,
        cameraProvider);
}

Skybox Skybox::FromEquirectangularMap(
    BindingProvider* cameraProvider,
    const Data::Image& image
) {
    auto texture = dvk::Texture::Create(GetDevice())
        .SetLabel("Skybox Equirectangular Map")
        .SetFormat(vk::Format::eR16G16B16A16Sfloat)
        .SetUsage(vk::ImageUsageFlagBits::eSampled)
        .SetExtent({ static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height), 1 })
        .EnableWriteWithStaging()
        .Build();
    texture->WriteWithStaging(GetCommandEncoder(), image.data.get(), image.size);
    return FromEquirectangularMap(cameraProvider, texture);
}

void Skybox::Draw(
    dvk::CommandBuffer& commandBuffer,
    uint32_t frameIndex,
    BindingProvider* cameraProvider
) {
    commandBuffer.SetGraphicsPipeline(graphicsPipeline);
    commandBuffer.SetVertexBuffer(0, positionBuffer);
    cameraProvider->Bind(0, commandBuffer, frameIndex);
    Bind(1, commandBuffer, frameIndex);
    commandBuffer.Draw(36);
}

dvk::BindSetLayout::Ref Skybox::GetLayout() {
    return environmentBindSetLayout;
}

void Skybox::Bind(uint32_t binding, dvk::CommandBuffer& commandBuffer, uint32_t _) {
    commandBuffer.SetBindSet(binding, environmentBindSet);
}
