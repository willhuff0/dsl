#include <CascadedShadowMap.hpp>

#include "utils/Paths.hpp"
#include "Provider.hpp"

CascadedShadowMap::CascadedShadowMap(uint32_t cascadeCount, uint32_t size)
    : _cascadeCount(cascadeCount), _size(size) {
    _depthTexture = dvk::Texture::Create(GetDevice())
        .SetLabel("[CSM] Depth Texture")
        .SetFormat(SHADOW_DEPTH_FORMAT)
        .SetExtent({ size, size, 1 })
        .SetArrayLayerCount(cascadeCount)
        .SetUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled)
        .SetType(vk::ImageType::e2D)
        .Build<FRAMES_IN_FLIGHT>();
    for (uint32_t cascade = 0; cascade < cascadeCount; cascade++) {
        _cascadeToDepthTextureView.emplace_back();
        for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
            _cascadeToDepthTextureView[cascade][i] = dvk::TextureView::Create(GetDevice(), _depthTexture[i])
                .SetArrayLayer(cascade)
                .SetAspect(vk::ImageAspectFlagBits::eDepth)
                .Build();
        }
    }
    for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        _depthTextureArrayView[i] = dvk::TextureView::Create(GetDevice(), _depthTexture[i])
            .SetAspect(vk::ImageAspectFlagBits::eDepth)
            .SetType(vk::ImageViewType::e2DArray)
            .Build();
    }

    _depthTextureSampler = dvk::Sampler::Create(GetDevice())
        .SetLabel("[CSM] Depth Texture Sampler")
        .SetAddressMode(vk::SamplerAddressMode::eClampToBorder)
        .SetBorderColor(vk::BorderColor::eFloatOpaqueWhite)
        .SetCompare(true, vk::CompareOp::eLessOrEqual)
        .Build();
}

uint32_t CascadedShadowMap::GetCascadeCount() const {
    return _cascadeCount;
}

uint32_t CascadedShadowMap::GetSize() const {
    return _size;
}

dvk::Texture::Ref CascadedShadowMap::GetDepthTexture(uint32_t frameIndex) const {
    return _depthTexture[frameIndex];
}

dvk::TextureView::Ref CascadedShadowMap::GetDepthTextureView(uint32_t frameIndex, uint32_t cascade) const {
    return _cascadeToDepthTextureView[cascade][frameIndex];
}

dvk::TextureView::Ref CascadedShadowMap::GetDepthTextureArrayView(uint32_t frameIndex) const {
    return _depthTextureArrayView[frameIndex];
}

dvk::Sampler::Ref CascadedShadowMap::GetDepthTextureSampler() const {
    return _depthTextureSampler;
}
