#include <PostProcessing.hpp>

#include <Provider.hpp>
#include <utils/Paths.hpp>

#include <string>

constexpr int numBlurIterations = 5;
constexpr float bloomThreshold = 1.0f;
constexpr float acesExposure = 1.2f;
constexpr auto toneMappingMode = ToneMappingMode::ACES;

PostProcessing::PostProcessing() {
    bindSetLayout = dvk::BindSetLayout::Create(GetDevice())
        .SetLabel("Post Processing")
        .AddEntry(0)
            .SetType(vk::DescriptorType::eUniformBuffer)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(1)
            .SetType(vk::DescriptorType::eSampler)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .AddEntry(2)
            .SetType(vk::DescriptorType::eSampledImage)
            .SetStage(vk::ShaderStageFlagBits::eFragment)
            .FinishEntry()
        .Build();

    uniformBuffer = dvk::Buffer::Create(GetDevice())
        .SetLabel("Post Processing Uniform Buffer")
        .SetUsage(vk::BufferUsageFlagBits::eUniformBuffer)
        .SetSize(sizeof(PostProcessingUniforms))
        .AllowRandomAccess()
        .EnablePersistentMapping()
        .Build();

    PostProcessingUniforms uniforms {
        .bloomThreshold = bloomThreshold,
        .acesExposure = acesExposure,
        .toneMappingMode = toneMappingMode,
    };
    SetUniforms(&uniforms);

    singlePipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("Post Processing")
        .AddBindSetLayout(bindSetLayout)
        .Build();

    doublePipelineLayout = dvk::PipelineLayout::Create(GetDevice())
        .SetLabel("Post Processing - Combine / Color Correction")
        .AddBindSetLayout(bindSetLayout)
        .AddBindSetLayout(bindSetLayout)
        .Build();

    auto fullscreenVertexShader = dvk::Shader::LoadFile(GetDevice(), PP_FULLSCREEN_VERTEX_SHADER_PATH, "Post Processing - Vertex");
    auto splitBrightnessShader = dvk::Shader::LoadFile(GetDevice(), PP_SPLIT_BRIGHTNESS_SHADER_PATH, "Post Processing - Split Brightness");
    auto blurShader = dvk::Shader::LoadFile(GetDevice(), PP_BLUR_SHADER_PATH, "Post Processing - Blur");
    auto colorCorrectionShader = dvk::Shader::LoadFile(GetDevice(), PP_COLOR_CORRECTION_SHADER_PATH, "Post Processing - Color Correction");

    splitBrightnessPipeline = dvk::GraphicsPipeline::Create(GetDevice(), singlePipelineLayout)
        .SetLabel("Post Processing - Split Brightness")
        .AddStage(vk::ShaderStageFlagBits::eVertex, fullscreenVertexShader, "vs_main").FinishStage()
        .AddStage(vk::ShaderStageFlagBits::eFragment, splitBrightnessShader, "fs_main").FinishStage()
        .SetSingleAttachment(GetRenderTextureFormat())
        .Build();

    blurHorizontalPipeline = dvk::GraphicsPipeline::Create(GetDevice(), singlePipelineLayout)
        .SetLabel("Post Processing - Blur Horizontal")
        .AddStage(vk::ShaderStageFlagBits::eVertex, fullscreenVertexShader, "vs_main").FinishStage()
        .AddStage(vk::ShaderStageFlagBits::eFragment, blurShader, "fs_main")
            .SetConstant(0, 1)
            .FinishStage()
        .SetSingleAttachment(GetRenderTextureFormat())
        .Build();

    blurVerticalPipeline = dvk::GraphicsPipeline::Create(GetDevice(), singlePipelineLayout)
        .SetLabel("Post Processing - Blur Vertical")
        .AddStage(vk::ShaderStageFlagBits::eVertex, fullscreenVertexShader, "vs_main").FinishStage()
        .AddStage(vk::ShaderStageFlagBits::eFragment, blurShader, "fs_main")
            .SetConstant(0, 0)
            .FinishStage()
        .SetSingleAttachment(GetRenderTextureFormat())
        .Build();

    colorCorrectionPipeline = dvk::GraphicsPipeline::Create(GetDevice(), doublePipelineLayout)
        .SetLabel("Post Processing - Combine / Color Correction")
        .AddStage(vk::ShaderStageFlagBits::eVertex, fullscreenVertexShader, "vs_main").FinishStage()
        .AddStage(vk::ShaderStageFlagBits::eFragment, colorCorrectionShader, "fs_main").FinishStage()
        .SetSingleAttachment(GetDisplaySurfaceFormat())
        .Build();

    sampler = dvk::Sampler::Create(GetDevice())
        .SetLabel("Post Processing")
        .Build();

    inputBindSet = dvk::BindSet::Create(GetDevice(), GetBindPool(), bindSetLayout);
    bindSet1 = dvk::BindSet::Create(GetDevice(), GetBindPool(), bindSetLayout);
    bindSet2 = dvk::BindSet::Create(GetDevice(), GetBindPool(), bindSetLayout);
}

void PostProcessing::ResizeRenderTexture(glm::uvec2 newSize) {
    auto renderTextureBuilder = dvk::Texture::Create(GetDevice())
        .SetUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled)
        .SetFormat(GetRenderTextureFormat())
        .SetExtent({ newSize.x, newSize.y, 1 });

    inputRenderTextureView = dvk::TextureView::Create(GetDevice(), renderTextureBuilder
        .SetLabel("Post Processing Input Render Texture")
        .Build()).Build();
    renderTextureView1 = dvk::TextureView::Create(GetDevice(), renderTextureBuilder
        .SetLabel("Post Processing Intermediate Render Texture 1")
        .Build()).Build();
    renderTextureView2 = dvk::TextureView::Create(GetDevice(), renderTextureBuilder
        .SetLabel("Post Processing Intermediate Render Texture 2")
        .Build()).Build();

    auto binder = dvk::Binder()
        .BindBuffer(0)
            .SetBufferType(vk::DescriptorType::eUniformBuffer)
            .SetBuffer(uniformBuffer)
            .SetRange(0, uniformBuffer->GetSize())
            .FinishBuffer()
        .BindSampler(1)
            .SetSampler(sampler)
            .FinishSampler();

    binder.BindTexture(2)
        .SetTextureView(inputRenderTextureView)
        .FinishTexture()
        .Update(inputBindSet);
    binder.BindTexture(2)
        .SetTextureView(renderTextureView1)
        .FinishTexture()
        .Update(bindSet1);
    binder.BindTexture(2)
        .SetTextureView(renderTextureView2)
        .FinishTexture()
        .Update(bindSet2);
}

void PostProcessing::SetUniforms(PostProcessingUniforms* uniforms) {
    uniformBuffer->CopyToAndFlush(uniforms);
}

dvk::TextureView::Ref PostProcessing::GetPostProcessingInputTextureView() { return inputRenderTextureView; }

void PostProcessing::PostProcessingRenderPass(const vk::raii::CommandBuffer& commandBuffer, const vk::raii::ImageView& surfaceImageView, vk::Extent2D surfaceExtent) {
    inputRenderTextureView->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    renderTextureView1->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

    {
        vk::RenderingAttachmentInfo attachmentInfo = {
            .imageView   = renderTextureView1->GetImageView(),
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
        };
        commandBuffer.beginRendering({
            .renderArea           = { .offset = { 0, 0 }, .extent = surfaceExtent },
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &attachmentInfo,
        });
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, splitBrightnessPipeline->GetPipeline());
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, singlePipelineLayout->GetPipelineLayout(), 0, *inputBindSet->GetDescriptorSet(), nullptr);
        commandBuffer.draw(6, 1, 0, 0);
        commandBuffer.endRendering();
    }

    renderTextureView1->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    renderTextureView2->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

    bool horizontal = true;
    std::string pingPongLabels[2] = { "Horizontal (1 -> 2)", "Vertical (2 -> 1)" };
    dvk::TextureView::Ref pingPongTargets[2] = { renderTextureView1, renderTextureView2 };
    dvk::BindSet::Ref pingPongBindSets[2] = { bindSet2, bindSet1 };
    dvk::GraphicsPipeline::Ref pingPongPipelines[2] = { blurVerticalPipeline, blurHorizontalPipeline };

    for (int i = 0; i < numBlurIterations * 2; i++) {
        vk::RenderingAttachmentInfo attachmentInfo = {
            .imageView   = pingPongTargets[horizontal]->GetImageView(),
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
        };
        commandBuffer.beginRendering({
            .renderArea           = { .offset = { 0, 0 }, .extent = surfaceExtent },
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &attachmentInfo,
        });
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pingPongPipelines[horizontal]->GetPipeline());
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, singlePipelineLayout->GetPipelineLayout(), 0, *pingPongBindSets[horizontal]->GetDescriptorSet(), nullptr);
        commandBuffer.draw(6, 1, 0, 0);
        commandBuffer.endRendering();

        if (horizontal) {
            renderTextureView1->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
            renderTextureView2->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        } else {
            renderTextureView1->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
            renderTextureView2->GetTexture()->TransitionLayout(commandBuffer, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
        }

        horizontal = !horizontal;
    }

    {
        vk::RenderingAttachmentInfo attachmentInfo = {
            .imageView   = surfaceImageView,
            .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .loadOp      = vk::AttachmentLoadOp::eClear,
            .storeOp     = vk::AttachmentStoreOp::eStore,
            .clearValue  = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
        };
        commandBuffer.beginRendering({
            .renderArea           = { .offset = { 0, 0 }, .extent = surfaceExtent },
            .layerCount           = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments    = &attachmentInfo,
        });
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, colorCorrectionPipeline->GetPipeline());
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, doublePipelineLayout->GetPipelineLayout(), 0, *inputBindSet->GetDescriptorSet(), nullptr);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, doublePipelineLayout->GetPipelineLayout(), 1, *bindSet1->GetDescriptorSet(), nullptr);
        commandBuffer.draw(6, 1, 0, 0);
        commandBuffer.endRendering();
    }
}