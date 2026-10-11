#pragma once

#include "ShaderHotReload.hpp"
#include "Renderer.hpp"

#include <dvk/dvk.hpp>

#include <memory>

class CascadedShadowMap {
public:
    CascadedShadowMap(uint32_t cascadeCount, uint32_t size);

    [[nodiscard]] uint32_t GetCascadeCount() const;
    [[nodiscard]] uint32_t GetSize() const;

    [[nodiscard]] dvk::Texture::Ref GetDepthTexture(uint32_t frameIndex) const;
    [[nodiscard]] dvk::TextureView::Ref GetDepthTextureView(uint32_t frameIndex, uint32_t cascade) const;
    [[nodiscard]] dvk::TextureView::Ref GetDepthTextureArrayView(uint32_t frameIndex) const;
    [[nodiscard]] dvk::Sampler::Ref GetDepthTextureSampler() const;

private:
    uint32_t _cascadeCount;
    uint32_t _size;

    dvk::Texture::Refs<FRAMES_IN_FLIGHT> _depthTexture;
    std::vector<dvk::TextureView::Refs<FRAMES_IN_FLIGHT>> _cascadeToDepthTextureView;
    dvk::TextureView::Refs<FRAMES_IN_FLIGHT> _depthTextureArrayView;
    dvk::Sampler::Ref _depthTextureSampler;
};