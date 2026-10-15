#pragma once

#include "CascadedShadowMap.hpp"

#include <dvk/dvk.hpp>

class DirectionalLight : public BindingProvider {
public:
    DirectionalLight(
        BindingProvider* cameraProvider,
        glm::vec3 direction = normalize(glm::vec3(-1.0f, 15.0f, -10.0f)),
        glm::vec3 color = glm::vec3(1.0f, 1.0f, 0.98f) * 8.0f,
        uint32_t shadowMapSize = 4096,
        uint32_t cascadeCount = 4,
        std::vector<float> cascadePortions = {0.0, 0.005, 0.025, 0.125, 0.625}
    );

    void RegenerateLightSpaceMatrices(
        uint32_t frameIndex,
        const glm::mat4& cameraViewMat,
        float cameraNear,
        float cameraFar,
        float fovRad,
        float aspect
    );
    void DrawShadowMap(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex);

    dvk::BindSetLayout::Ref GetLayout() override;
    void Bind(uint32_t binding, dvk::CommandBuffer& commandBuffer, uint32_t frameIndex) override;

private:
    glm::vec3 _direction;
    glm::vec3 _color;

    CascadedShadowMap _shadowMap;
    std::vector<float> _cascadePortions;

    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _cameraUniformBuffer;
    std::vector<dvk::BindSet::Refs<FRAMES_IN_FLIGHT>> _cascadeToCameraBindSet;

    dvk::BindSetLayout::Ref _bindSetLayout;
    dvk::Buffer::Refs<FRAMES_IN_FLIGHT> _uniformBuffer;
    dvk::BindSet::Refs<FRAMES_IN_FLIGHT> _bindSet;

    glm::mat4 _lightSpaceViewProjMats[4]{};
};