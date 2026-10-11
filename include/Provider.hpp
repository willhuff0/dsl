#pragma once

#include "Platform.hpp"

#include <dvk/dvk.hpp>
#include <glm/glm.hpp>

#include "BindingProvider.hpp"

class Renderer;
class BindingProvider;

constexpr uint32_t FRAMES_IN_FLIGHT = 2;
constexpr vk::Format SHADOW_DEPTH_FORMAT = vk::Format::eD32Sfloat;

void SetupProvider(
    Platform* platform,
    dvk::Instance* instance,
    dvk::Device* device,
    dvk::CommandEncoder* commandEncoder,
    vk::Format surfaceFormat);
void SetupRenderer(BindingProvider* cameraProvider);
void FreeProvider();

Platform* GetPlatform();
dvk::Instance& GetInstance();
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