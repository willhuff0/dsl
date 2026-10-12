#include <DirectionalLight.hpp>

#include "FlyCamera.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <algorithm>
#include <cmath>

namespace {
    struct alignas(64) DirectionalLightCameraUniforms : public CameraUniforms { };

    struct alignas(16) DirectionalLightUniforms {
        glm::mat4 lightSpaceViewProjMats[4];
        float cascadeDistances[4];
        alignas(16) glm::vec3 direction;
        alignas(16) glm::vec3 color;
        float cameraZFar;
        uint32_t cascadeCount;
        uint32_t shadowMapSize;
    };
}

static std::array<glm::vec4, 8> getFrustumCornersWorldSpace(const glm::mat4& viewProjMat) {
    auto invMat = inverse(viewProjMat);

    std::array<glm::vec4, 8> corners{};
    for (int x = 0; x < 2; x++) {
        for (int y = 0; y < 2; y++) {
            for (int z = 0; z < 2; z++) {
                glm::vec4 point(
                    invMat *
                    glm::vec4(
                      2.0f * static_cast<float>(x) - 1.0f,
                      2.0f * static_cast<float>(y) - 1.0f,
                      static_cast<float>(z),
                      1.0f
                    ));

                corners[x * 4 + y * 2 + z] = point / point.w;
            }
        }
    }

    return corners;
}

static glm::mat4 getCascadeViewProjMat(
    const glm::mat4& cameraViewMat,
    float fovRad,
    float aspect,
    float zNear,
    float zFar,
    glm::vec3 lightDir,
    uint32_t shadowMapSize
) {
    auto projMat = glm::perspectiveRH_ZO(fovRad, aspect, zNear, zFar);
    auto frustumCorners = getFrustumCornersWorldSpace(projMat * cameraViewMat);

    auto center = glm::zero<glm::vec3>();
    for (const auto& corner : frustumCorners) {
        center += xyz(corner);
    }
    center /= 8;

    auto radius = 0.0f;
    for (const auto& corner : frustumCorners) {
        radius = std::max(radius, distance(xyz(corner), center));
    }
    radius = std::ceil(radius * 16.0f) / 16.0f;

    auto up = glm::vec3(0.0f, 1.0f, 0.0f);
    if (std::abs(normalize(lightDir).y) > 0.999f) {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    auto lightViewMat = glm::lookAt(center + lightDir, center, up);

    float minX = -radius;
    float maxX = radius;
    float minY = -radius;
    float maxY = radius;
    float minZ = std::numeric_limits<float>::infinity();
    float maxZ = -std::numeric_limits<float>::infinity();
    for (const auto& corner : frustumCorners) {
        auto cornerLightViewSpace = lightViewMat * corner;
        minZ = std::min(minZ, cornerLightViewSpace.z);
        maxZ = std::max(maxZ, cornerLightViewSpace.z);
    }

    float zMult = 5.0;
    if (minZ < 0.0f) {
        minZ *= zMult;
    } else {
        minZ /= zMult;
    }
    if (maxZ < 0.0f) {
        maxZ /= zMult;
    } else {
        maxZ *= zMult;
    }

    auto lightProjMat = glm::orthoRH_ZO(minX, maxX, minY, maxY, -maxZ, -minZ);

    auto shadowMat = lightProjMat * lightViewMat;
    auto shadowOrigin = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    auto shadowOriginNdc = shadowMat * shadowOrigin;

    auto shadowMapSizeFloat = static_cast<float>(shadowMapSize);
    auto shadowOriginScaled = shadowOriginNdc * (shadowMapSizeFloat / 2.0f);

    auto roundedOrigin = glm::vec4(
        std::round(shadowOriginScaled.x),
        std::round(shadowOriginScaled.y),
        std::round(shadowOriginScaled.z),
        shadowOriginScaled.w
    );

    auto roundedOffset = roundedOrigin - shadowOriginScaled;
    roundedOffset *= 2.0f / shadowMapSizeFloat;
    roundedOffset.z = 0.0f;
    roundedOffset.w = 0.0f;

    lightProjMat[3] = lightProjMat[3] + roundedOffset;

    return lightProjMat * lightViewMat;
}

DirectionalLight::DirectionalLight(
    BindingProvider* cameraProvider,
    glm::vec3 direction,
    glm::vec3 color,
    uint32_t shadowMapSize,
    uint32_t cascadeCount,
    std::vector<float> cascadePortions
) : _direction(direction),
    _color(color),
    _shadowMap(cascadeCount, shadowMapSize),
    _cascadePortions(std::move(cascadePortions))
{
    _cameraUniformBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Directional Light] Depth Camera Uniform Buffer")
        .SetSize(sizeof(DirectionalLightCameraUniforms) * cascadeCount)
        .SetUsage(vk::BufferUsageFlagBits::eUniformBuffer)
        .EnableCopyToAndFlush()
        .Build<FRAMES_IN_FLIGHT>();

    for (uint32_t cascade = 0; cascade < cascadeCount; cascade++) {
        auto& bindSet = _cascadeToCameraBindSet.emplace_back(dvk::BindSet::Create<FRAMES_IN_FLIGHT>(
            GetDevice(),
            GetBindPool(),
            cameraProvider->GetLayout(),
            "[Directional Light] Depth Camera Bind Set for Cascade " + std::to_string(cascade)));

        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
            dvk::Binder()
                .BindBuffer(0)
                    .SetBuffer(_cameraUniformBuffer[i])
                    .SetBufferType(vk::DescriptorType::eUniformBuffer)
                    .SetRange(sizeof(DirectionalLightCameraUniforms) * cascade, sizeof(CameraUniforms))
                    .FinishBuffer()
                .Update(bindSet[i]);
        }
    }

    _bindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("[Directional Light] Bind Set Layout")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eUniformBuffer)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
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
    _uniformBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel("[Directional Light] Uniform Buffer")
        .SetUsage(vk::BufferUsageFlagBits::eUniformBuffer)
        .SetSize(sizeof(DirectionalLightUniforms))
        .EnableCopyToAndFlush()
        .Build<FRAMES_IN_FLIGHT>();
    _bindSet = dvk::BindSet::Create<FRAMES_IN_FLIGHT>(GetDevice(), GetBindPool(), _bindSetLayout, "[Directional Light] Bind Set");
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        dvk::Binder()
            .BindBuffer(0)
                .SetBuffer(_uniformBuffer[i])
                .SetBufferType(vk::DescriptorType::eUniformBuffer)
                .FinishBuffer()
            .BindTexture(1)
                .SetTextureView(_shadowMap.GetDepthTextureArrayView(i))
                .FinishTexture()
            .BindSampler(2)
                .SetSampler(_shadowMap.GetDepthTextureSampler())
                .FinishSampler()
            .Update(_bindSet[i]);
    }
}

void DirectionalLight::RegenerateLightSpaceMatrices(
    uint32_t frameIndex,
    const glm::mat4& cameraViewMat,
    float cameraNear,
    float cameraFar,
    float fovRad,
    float aspect
) {
    float cascadeDistances[4] = {};

    for (auto& lightSpaceViewProjMat : _lightSpaceViewProjMats) {
        lightSpaceViewProjMat = glm::identity<glm::mat4>();
    }
    for (uint32_t cascade = 0; cascade < _shadowMap.GetCascadeCount(); cascade++) {
        auto zNear = cameraNear + (cameraFar - cameraNear) * _cascadePortions[cascade];
        auto zFar = cameraNear + (cameraFar - cameraNear) * _cascadePortions[cascade + 1];

        cascadeDistances[cascade] = zFar;
        _lightSpaceViewProjMats[cascade] = getCascadeViewProjMat(
            cameraViewMat,
            fovRad,
            aspect,
            zNear,
            zFar,
            _direction,
            _shadowMap.GetSize()
        );
    }

    DirectionalLightCameraUniforms cameraUniforms[4];
    cameraUniforms[0].viewProjMat = _lightSpaceViewProjMats[0];
    cameraUniforms[1].viewProjMat = _lightSpaceViewProjMats[1];
    cameraUniforms[2].viewProjMat = _lightSpaceViewProjMats[2];
    cameraUniforms[3].viewProjMat = _lightSpaceViewProjMats[3];
    _cameraUniformBuffer[frameIndex]->CopyToAndFlush(&cameraUniforms);

    DirectionalLightUniforms uniforms {
        .lightSpaceViewProjMats = {
            _lightSpaceViewProjMats[0],
            _lightSpaceViewProjMats[1],
            _lightSpaceViewProjMats[2],
            _lightSpaceViewProjMats[3],
        },
        .cascadeDistances = {
            cascadeDistances[0],
            cascadeDistances[1],
            cascadeDistances[2],
            cascadeDistances[3],
        },
        .direction = _direction,
        .color = _color,
        .cameraZFar = cameraFar,
        .cascadeCount = _shadowMap.GetCascadeCount(),
        .shadowMapSize = _shadowMap.GetSize(),
    };
    _uniformBuffer[frameIndex]->CopyToAndFlush(&uniforms);
}

void DirectionalLight::DrawShadowMap(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) {
    commandBuffer.InsertBarriers()
        .AddImageBarrier()
            .SetImage(_shadowMap.GetDepthTexture(frameIndex))
            .SetAccess(vk::AccessFlagBits2::eShaderSampledRead, vk::AccessFlagBits2::eDepthStencilAttachmentWrite | vk::AccessFlagBits2::eDepthStencilAttachmentRead)
            .SetStages(vk::PipelineStageFlagBits2::eFragmentShader, vk::PipelineStageFlagBits2::eEarlyFragmentTests)
            .SetLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal)
            .SetAspect(vk::ImageAspectFlagBits::eDepth)
            .FinishImageBarrier()
        .FinishBarriers();

    for (uint32_t cascade = 0; cascade < _shadowMap.GetCascadeCount(); cascade++) {
        class DirectionalLightCamera : public BindingProvider {
        public:
            explicit DirectionalLightCamera(const dvk::BindSet::Refs<FRAMES_IN_FLIGHT>& cameraBindSet)
                : _cameraBindSet(cameraBindSet) { }

            dvk::BindSetLayout::Ref GetLayout() override { return nullptr; }
            void Bind(uint32_t binding, dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) override {
                commandBuffer.SetBindSet(binding, _cameraBindSet[frameIndex]);
            }
        private:
            const dvk::BindSet::Refs<FRAMES_IN_FLIGHT>& _cameraBindSet;
        } camera(_cascadeToCameraBindSet[cascade]);

        GetRenderer().ComputeDrawCommands(commandBuffer, frameIndex, &camera);

        commandBuffer.BeginRegion("[Directional Light] Draw Shadow Map", glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        commandBuffer.BeginRendering()
            .SetDepthAttachment()
                .SetView(_shadowMap.GetDepthTextureView(frameIndex, cascade))
                .SetImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
                .SetOps(vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore)
                .SetClearValue(vk::ClearDepthStencilValue(1.0f, 0))
                .FinishAttachment()
            .FinishBeginRendering();

        commandBuffer.SetViewport(
            {0.0f, static_cast<float>(_shadowMap.GetSize())},
            {static_cast<float>(_shadowMap.GetSize()), -static_cast<float>(_shadowMap.GetSize())});
        // commandBuffer.SetViewport(
        //     {0.0f, 0.0f},
        //     {static_cast<float>(_shadowMap.GetSize()), static_cast<float>(_shadowMap.GetSize())});
        commandBuffer.SetScissor({0, 0}, {_shadowMap.GetSize(), _shadowMap.GetSize()});

        GetRenderer().DrawDepthOnly(commandBuffer, frameIndex, &camera, true);

        commandBuffer.EndRendering();
        commandBuffer.EndRegion();
    }

    commandBuffer.InsertBarriers()
        .AddImageBarrier()
            .SetImage(_shadowMap.GetDepthTexture(frameIndex))
            .SetAccess(vk::AccessFlagBits2::eDepthStencilAttachmentWrite, vk::AccessFlagBits2::eShaderSampledRead)
            .SetStages(vk::PipelineStageFlagBits2::eLateFragmentTests, vk::PipelineStageFlagBits2::eFragmentShader)
            .SetLayout(vk::ImageLayout::eDepthAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal)
            .SetAspect(vk::ImageAspectFlagBits::eDepth)
            .FinishImageBarrier()
        .FinishBarriers();
}

dvk::BindSetLayout::Ref DirectionalLight::GetLayout() {
    return _bindSetLayout;
}

void DirectionalLight::Bind(uint32_t binding, dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) {
    commandBuffer.SetBindSet(binding, _bindSet[frameIndex]);
}
