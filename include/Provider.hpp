#pragma once

#include "Platform.hpp"

#include <dvk/dvk.hpp>
#include <glm/glm.hpp>

class Renderer;

constexpr uint32_t FRAMES_IN_FLIGHT = 2;

struct alignas(16) GlobalUniforms {
    glm::mat4 viewProjMat;
    glm::mat4 projMat;
    glm::mat4 viewMat;
    glm::vec3 viewPos;
    uint32_t radianceMipMapCount;
};

void SetupProvider(Platform* platform, dvk::Device* device, dvk::CommandEncoder* commandEncoder, vk::Format surfaceFormat);
void FreeProvider();

void SetGlobalUniforms(GlobalUniforms* uniforms, uint32_t frameIndex);
void SetSkybox(const dvk::Texture::Ref& irradianceMap,
               const dvk::Texture::Ref& radianceMap,
               const dvk::Texture::Ref& brdfLuts);

void BindGlobals(dvk::CommandBuffer& commandBuffer, uint32_t frameIndex, uint32_t binding = 0);

dvk::BindSetLayout::Ref GetGlobalBindSetLayout();

Platform* GetPlatform();
dvk::Device& GetDevice();
dvk::CommandEncoder& GetCommandEncoder();
dvk::BindPool::Ref& GetBindPool();
vk::Format GetRenderTextureFormat();
vk::Format GetDisplaySurfaceFormat();

vk::Format GetDepthFormat();
vk::SampleCountFlagBits GetMultisampleCount();

void SetDebugTextureView(dvk::TextureView::Ref debugTextureView);
const dvk::TextureView::Ref& GetDebugTextureView();
const dvk::BindSetLayout::Ref& GetDebugTextureBindSetLayout();
const dvk::BindSet::Ref& GetDebugTextureBindSet();

Renderer& GetRenderer();