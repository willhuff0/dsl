#pragma once

#include "Platform.hpp"

#include <dvk/dvk.hpp>
#include <glm/glm.hpp>

constexpr uint32_t FRAMES_IN_FLIGHT = 2;

struct alignas(16) GlobalUniforms {
    glm::mat4 viewProjMatrix;
    glm::vec4 viewPosition;
    uint32_t radianceMipMapCount;
};

void SetupProvider(Platform* platform, dvk::Device* device, dvk::CommandEncoder* commandEncoder, vk::Format surfaceFormat);

void SetGlobalUniforms(GlobalUniforms* uniforms, uint32_t frameIndex);
void SetSkybox(const dvk::Texture::Ref& irradianceMap,
               const dvk::Texture::Ref& radianceMap,
               const dvk::Texture::Ref& brdfLuts);

void BindGlobals(const vk::raii::CommandBuffer& commandBuffer, vk::PipelineLayout pipelineLayout, uint32_t frameIndex);

dvk::BindSetLayout::Ref GetGlobalBindSetLayout();

Platform* GetPlatform();
dvk::Device& GetDevice();
dvk::CommandEncoder& GetCommandEncoder();
dvk::BindPool::Ref& GetBindPool();
vk::Format GetRenderTextureFormat();
vk::Format GetDisplaySurfaceFormat();

vk::Format GetDepthFormat();
vk::SampleCountFlagBits GetMultisampleCount();
