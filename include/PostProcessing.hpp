#pragma once

#include <dvk/dvk.hpp>
#include <glm/glm.hpp>

#include "Provider.hpp"

enum class ToneMappingMode : uint32_t {
    None = 0,
    Reinhard = 1,
    ACES = 2,
};

struct PostProcessingUniforms {
    float bloomThreshold;
    float acesExposure;
    ToneMappingMode toneMappingMode;
};

class PostProcessing {
public:
    explicit PostProcessing();

    void ResizeRenderTexture(glm::uvec2 newSize);
    void SetUniforms(PostProcessingUniforms* uniforms);

    dvk::TextureView::Ref GetPostProcessingInputTextureView();

    void PostProcessingRenderPass(const vk::raii::CommandBuffer& commandBuffer, const vk::raii::ImageView& surfaceImageView, vk::Extent2D surfaceExtent);

private:
    dvk::BindSetLayout::Ref bindSetLayout;
    dvk::Buffer::Ref uniformBuffer;

    dvk::PipelineLayout::Ref singlePipelineLayout;
    dvk::PipelineLayout::Ref doublePipelineLayout;

    dvk::GraphicsPipeline::Ref splitBrightnessPipeline;
    dvk::GraphicsPipeline::Ref blurHorizontalPipeline;
    dvk::GraphicsPipeline::Ref blurVerticalPipeline;
    dvk::GraphicsPipeline::Ref colorCorrectionPipeline;

    dvk::BindSet::Ref inputBindSet;
    dvk::BindSet::Ref bindSet1;
    dvk::BindSet::Ref bindSet2;

    dvk::Sampler::Ref sampler;
    dvk::TextureView::Ref inputRenderTextureView;
    dvk::TextureView::Ref renderTextureView1;
    dvk::TextureView::Ref renderTextureView2;
};