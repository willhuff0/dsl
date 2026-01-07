#include <Skybox.hpp>

#include <Provider.hpp>
#include <utils/Paths.hpp>

#include <string>

static constexpr int IRRADIANCE_MAP_SIZE = 64;
static constexpr int RADIANCE_MAP_SIZE = 512;
static constexpr int BRDF_LUT_SIZE = 512;
static const int RADIANCE_MIPMAP_COUNT = static_cast<int>(std::floor(std::log2(RADIANCE_MAP_SIZE))) + 1;

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
        .SetAddressMode(vk::SamplerAddressMode::eClampToBorder)
        .SetBorderColor(vk::BorderColor::eFloatOpaqueBlack)
        .Build();

    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[] = {
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, 1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, 1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f, 0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, 1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, 1.0f,  0.0f))
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
        .SetSize(4)
        .EnableCopyToAndFlush()
        .Build();

    equirectangularToCubeMapPerMapBindSet = dvk::BindSet::Create(GetDevice(), GetBindPool(), equirectangularToCubeMapPerMapBindSetLayout, "CubeMapConverter ConvertEquirectangularToCubeMap Per Map");
    convolutionPerMapBindSet = dvk::BindSet::Create(GetDevice(), GetBindPool(), convolutionPerMapBindSetLayout, "CubeMapConverter Irradiance/Radiance Per Map");
}

dvk::Texture::Ref CubeMapConverter::ConvertEquirectangularToCubeMap(const dvk::Texture::Ref& equirectangularMap, const std::string& label) {
    uint32_t cubeMapFaceSize = equirectangularMap->GetExtent().height;
    auto cubeMapTexture = dvk::Texture::Create(GetDevice())
        .SetLabel(label)
        .SetUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment)
        .SetFormat(vk::Format::eR16G16B16A16Sfloat)
        .SetExtent({ cubeMapFaceSize, cubeMapFaceSize, 1 })
        .AllowCubeViews()
        .Build();

    auto equirectangularMapView = dvk::TextureView::Create(GetDevice(), equirectangularMap).Build();

    dvk::Binder()
        .BindTexture(0)
            .SetTextureView(equirectangularMapView)
            .FinishImage()
        .BindSampler(1)
            .SetSampler(sampler)
            .FinishSampler()
        .Update(equirectangularToCubeMapPerMapBindSet);

    capture(equirectangularToCubeMapPerMapBindSet, cubeMapTexture, equirectangularToCubeMapShaderPipeline);

    return cubeMapTexture;
}

dvk::Texture::Ref CubeMapConverter::GenerateIrradianceMap(const dvk::Texture::Ref& cubeMap, const std::string& label) {
    auto irradianceMap = dvk::Texture::Create(GetDevice())
        .SetLabel(label)
        .SetUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment)
        .SetFormat(cubeMap->GetFormat())
        .SetExtent({ IRRADIANCE_MAP_SIZE, IRRADIANCE_MAP_SIZE, 1 })
        .AllowCubeViews()
        .Build();

    auto cubeMapView = dvk::TextureView::Create(GetDevice(), cubeMap)
        .SetType(vk::ImageViewType::eCube)
        .SetArrayLayer(0, 6)
        .Build();

    dvk::Binder()
        .BindTexture(0)
            .SetTextureView(cubeMapView)
            .FinishImage()
        .BindSampler(1)
            .SetSampler(sampler)
            .FinishSampler()
        .BindBuffer(2)
            .SetBuffer(convolutionUniformBuffer)
            .SetRange(0, 4)
            .FinishBuffer()
        .Update(convolutionPerMapBindSet);

    capture(convolutionPerMapBindSet, irradianceMap, irradianceShaderPipeline);

    return irradianceMap;
}

dvk::Texture::Ref CubeMapConverter::GenerateRadianceMap(const dvk::Texture::Ref& cubeMap, const std::string& label) {
    auto radianceMap = dvk::Texture::Create(GetDevice())
        .SetLabel(label)
        .SetUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment)
        .SetFormat(cubeMap->GetFormat())
        .SetExtent({ RADIANCE_MAP_SIZE, RADIANCE_MAP_SIZE, 1 })
        .SetMipLevelCount(RADIANCE_MIPMAP_COUNT)
        .AllowCubeViews()
        .Build();

    auto cubeMapView = dvk::TextureView::Create(GetDevice(), cubeMap)
        .SetType(vk::ImageViewType::eCube)
        .SetArrayLayer(0, 6)
        .Build();

    dvk::Binder()
        .BindTexture(0)
            .SetTextureView(cubeMapView)
            .FinishImage()
        .BindSampler(1)
            .SetSampler(sampler)
            .FinishSampler()
        .BindBuffer(2)
            .SetBuffer(convolutionUniformBuffer)
            .SetRange(0, 4)
            .FinishBuffer()
        .Update(convolutionPerMapBindSet);

    return radianceMap;

    for (int i = 0; i < RADIANCE_MIPMAP_COUNT; i++) {
        ConvolutionUniforms uniforms {
            .roughness = static_cast<float>(i) / static_cast<float>(RADIANCE_MIPMAP_COUNT - 1),
        };
        convolutionUniformBuffer->CopyToAndFlush(&uniforms);
        capture(convolutionPerMapBindSet, radianceMap, radianceShaderPipeline, i, i == 0, i == 6);
    }

    return radianceMap;
}

void CubeMapConverter::capture(const dvk::BindSet::Ref& perMapBindSet, const dvk::Texture::Ref& target, dvk::GraphicsPipeline::Ref shaderPipeline, int mipMap, bool transitionLayoutStart , bool transitionLayoutEnd) {
    vk::Extent2D targetExtent = { target->GetExtent().width, target->GetExtent().height };

    std::array<dvk::TextureView::Ref, 6> cubeMapTextureViews;
    for (int i = 0; i < 6; i++) {
        cubeMapTextureViews[i] = dvk::TextureView::Create(GetDevice(), target)
            .SetLabel("CubeMapConverter ConvertEquirectangularToCubeMap Result View (" + std::to_string(i) + ")")
            .SetArrayLayer(i)
            .SetMipLevel(mipMap)
            .Build();
    }

    GetCommandEncoder().ExecuteOneTime([&](const vk::raii::CommandBuffer& commandBuffer) {
        if (transitionLayoutStart) {
            target->TransitionLayout(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
        }

        for (int i = 0; i < 6; i++) {
            vk::RenderingAttachmentInfo attachmentInfo = {
                .imageView   = cubeMapTextureViews[i]->GetImageView(),
                .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                .loadOp      = vk::AttachmentLoadOp::eClear,
                .storeOp     = vk::AttachmentStoreOp::eStore,
                .clearValue  = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
            };
            commandBuffer.beginRendering({
                .renderArea           = { .offset = {0, 0}, .extent = targetExtent },
                .layerCount           = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments    = &attachmentInfo,
            });
            commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(targetExtent.width), static_cast<float>(targetExtent.height), 0.0f, 1.0f));
            commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), targetExtent));
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, shaderPipeline->GetPipeline());
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shaderPipeline->GetLayout()->GetPipelineLayout(), 0, *faceBindSets.at(i)->GetDescriptorSet(), nullptr);
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shaderPipeline->GetLayout()->GetPipelineLayout(), 1, *perMapBindSet->GetDescriptorSet(), nullptr);
            commandBuffer.bindVertexBuffers(0, *positionBuffer->GetBuffer(), { 0 });
            commandBuffer.draw(36, 1, 0, 0);
            commandBuffer.endRendering();
        }

        if (transitionLayoutEnd) {
            target->TransitionLayout(commandBuffer, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        }
    });
}

EnvBrdfLutGenerator::EnvBrdfLutGenerator() {
    auto pipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("Environment BRDF LUT Generator")
        .Build();

    auto shader = dvk::Shader::LoadFile(GetDevice(), SB_ENV_BRDF_LUT_GENERATOR_SHADER_PATH, "Environment BRDF LUT Generator");

    shaderPipeline = dvk::GraphicsPipeline::Create(GetDevice(), pipelineLayout)
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

    GetCommandEncoder().ExecuteOneTime([&](const vk::raii::CommandBuffer& commandBuffer) {
        texture->TransitionLayout(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

        vk::RenderingAttachmentInfo attachmentInfo = {
            .imageView   = textureView->GetImageView(),
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
        };
        commandBuffer.beginRendering({
            .renderArea           = { .offset = {0, 0}, .extent = { BRDF_LUT_SIZE, BRDF_LUT_SIZE } },
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &attachmentInfo,
        });
        commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(BRDF_LUT_SIZE), static_cast<float>(BRDF_LUT_SIZE), 0.0f, 1.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), { BRDF_LUT_SIZE, BRDF_LUT_SIZE }));
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, shaderPipeline->GetPipeline());
        commandBuffer.draw(6, 1, 0, 0);
        commandBuffer.endRendering();

        texture->TransitionLayout(commandBuffer, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    });

    return texture;
}

Skybox::Skybox(dvk::Texture::Ref cubeMap, dvk::Texture::Ref irradianceMap, dvk::Texture::Ref radianceMap, dvk::Texture::Ref brdfLut)
    : irradianceMap(std::move(irradianceMap)),
      radianceMap(std::move(radianceMap)),
      brdfLut(std::move(brdfLut)) {
    cubeMapView = dvk::TextureView::Create(GetDevice(), cubeMap)
        .SetType(vk::ImageViewType::eCube)
        .SetArrayLayer(0, 6)
        .Build();

    bindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("Skybox")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eUniformBuffer)
            .SetStage(vk::ShaderStageFlagBits::eVertex)
            .FinishEntry()
        .AddEntry(1)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(2)
            .SetType(vk::DescriptorType::eSampler)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .Build();

    uniformBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel("Skybox Uniform Buffer")
        .SetUsage(vk::BufferUsageFlagBits::eUniformBuffer)
        .SetSize(sizeof(SkyboxUniforms))
        .EnableCopyToAndFlush()
        .Build<FRAMES_IN_FLIGHT>();

    bindSet = dvk::BindSet::Create<FRAMES_IN_FLIGHT>(GetDevice(), GetBindPool(), bindSetLayout, "Skybox");

    sampler = dvk::Sampler::Create(GetDevice())
        .SetLabel("Skybox")
        .SetAddressMode(vk::SamplerAddressMode::eClampToBorder)
        .SetBorderColor(vk::BorderColor::eFloatOpaqueBlack)
        .Build();

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        dvk::Binder()
            .BindBuffer(0)
                .SetBufferType(vk::DescriptorType::eUniformBuffer)
                .SetBuffer(uniformBuffer[i])
                .SetRange(0, sizeof(SkyboxUniforms))
                .FinishBuffer()
            .BindTexture(1)
                .SetTextureView(cubeMapView)
                .FinishImage()
            .BindSampler(2)
                .SetSampler(sampler)
                .FinishSampler()
            .Update(bindSet[i]);
    }

    auto pipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("Skybox")
        .AddBindSetLayout(bindSetLayout)
        .Build();

    auto shader = dvk::Shader::LoadFile(GetDevice(), SB_SKYBOX_SHADER_PATH, "Skybox");

    shaderPipeline = dvk::GraphicsPipeline::Create(GetDevice(), pipelineLayout)
        .SetLabel("Skybox")
        .SetVertexInput(getCubeVertexBindingDescriptions(), getCubeVertexAttributeDescriptions())
        .AddStage(vk::ShaderStageFlagBits::eVertex, shader, "vs_main").FinishStage()
        .AddStage(vk::ShaderStageFlagBits::eFragment, shader, "fs_main").FinishStage()
        .SetSingleAttachment(GetRenderTextureFormat())
        .EnableDepth(GetDepthFormat())
        .EnableMultisample(GetMultisampleCount())
        .Build();

    positionBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel("Skybox Position Buffer")
        .SetUsage(vk::BufferUsageFlagBits::eVertexBuffer)
        .SetSize(sizeof(cubeVertexPositions))
        .EnableWriteWithStaging()
        .Build();
    positionBuffer->WriteWithStaging(GetCommandEncoder(), cubeVertexPositions);
}

Skybox Skybox::FromEquirectangularMap(const dvk::Texture::Ref& equirectangularMap) {
    CubeMapConverter converter;
    auto cubeMap = converter.ConvertEquirectangularToCubeMap(equirectangularMap);
    auto irradianceMap = converter.GenerateIrradianceMap(cubeMap);
    auto radianceMap = converter.GenerateRadianceMap(cubeMap);

    EnvBrdfLutGenerator brdfLutGenerator;
    auto brdfLut = brdfLutGenerator.GenerateBrdfLut();

    return Skybox(cubeMap, irradianceMap, radianceMap, brdfLut);
}

Skybox Skybox::FromEquirectangularMap(const Data::Image& image) {
    auto texture = dvk::Texture::Create(GetDevice())
        .SetLabel("Skybox Equirectangular Map")
        .SetFormat(vk::Format::eR16G16B16A16Sfloat)
        .SetUsage(vk::ImageUsageFlagBits::eSampled)
        .SetExtent({ static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height), 1 })
        .EnableWriteWithStaging()
        .Build();
    texture->WriteWithStaging(GetCommandEncoder(), image.data.get(), image.size);
    return FromEquirectangularMap(texture);
}

void Skybox::Draw(glm::mat4 projMatrix, glm::mat4 viewMatrix, const vk::raii::CommandBuffer& commandBuffer, uint32_t frameIndex) {
    SkyboxUniforms uniforms {
        .projMatrix = projMatrix,
        .viewMatrix = viewMatrix,
    };
    uniformBuffer[frameIndex]->CopyToAndFlush(&uniforms);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, shaderPipeline->GetPipeline());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, shaderPipeline->GetLayout()->GetPipelineLayout(), 0, *bindSet[frameIndex]->GetDescriptorSet(), nullptr);
    commandBuffer.bindVertexBuffers(0, *positionBuffer->GetBuffer(), { 0 });
    commandBuffer.draw(36, 1, 0, 0);
}

dvk::Texture::Ref Skybox::GetIrradianceMap() const { return irradianceMap; }
dvk::Texture::Ref Skybox::GetRadianceMap() const { return radianceMap; }
dvk::Texture::Ref Skybox::GetBrdfLut() const { return brdfLut; }

int Skybox::GetRadianceMipMapCount() {
    return RADIANCE_MIPMAP_COUNT;
}
