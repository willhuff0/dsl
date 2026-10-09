#include <Game.hpp>

#include <Provider.hpp>
#include <utils/Paths.hpp>
#include <TempImageLoader.hpp>

#include <utility>
#include <filesystem>

constexpr std::chrono::milliseconds MIN_SHADER_RECOMPILE_INTERVAL(500);

Game::Game(Platform* platform, Window::Ref window, dvk::Surface& surface, dvk::Presenter& presenter)
    : platform(platform),
      window(std::move(window)),
      surface(surface),
      presenter(presenter),
      flyCamera(static_cast<GLFWwindow*>(this->window->GetNativeHandle()), glm::vec3(0.0f, 0.5f, 10.0f)) {
    postProcessing = std::make_unique<PostProcessing>();
    skybox = std::make_unique<Skybox>(Skybox::FromEquirectangularMap(LoadHDRI(SKYBOX_HDRI_PATH)));
    terrain = std::make_unique<Terrain>();
    GetCommandEncoder().ExecuteAndWait([&](dvk::CommandBuffer& commandBuffer) {
        model = std::make_unique<Model>(Model::LoadFile(commandBuffer, 0, RESOURCE_BASEPATH "/models/FlightHelmet.glb"));
    });
    model->GetRoot()->SetLScale(glm::vec3(50.0f, 50.0f, 50.0f));
    model->GetRoot()->SetLPos(glm::vec3(0.0f, -50.0f, 0.0f));

    SetSkybox(skybox->GetIrradianceMap(), skybox->GetRadianceMap(), skybox->GetBrdfLut());

    GetDevice().GetQueue().waitIdle();
    regenerateRenderTextures();
}

void Game::MainLoop() {
    platform->ProcessEvents();

    // Tick
    flyCamera.Tick();

    // Draw
    auto& onSurfaceReadySemaphore = GetCommandEncoder().WaitForNextFrame();
    auto [result, imageIndex, surfaceImageView, onRenderFinishedSemaphore] = presenter.GetNextImage(onSurfaceReadySemaphore);
    switch (result) {
        case dvk::PresenterResult::Fail_MustResize:
            resizeSurface();
            return;
        default:
            break;
    }
    auto [frameIndex, commandBuffer] = GetCommandEncoder().GetNextFrame();

    HotReload::Attempt(frameIndex, GetDevice());

    // Render
    GlobalUniforms globalUniforms {
        .viewProjMat = flyCamera.GetProjMat() * flyCamera.GetViewMat(),
        .projMat = flyCamera.GetProjMat(),
        .viewMat = flyCamera.GetViewMat(),
        .viewPos = flyCamera.GetPos(),
        .radianceMipMapCount = static_cast<uint32_t>(Skybox::GetRadianceMipMapCount()),
    };
    SetGlobalUniforms(&globalUniforms, frameIndex);

    // Stream terrain heightmap around the camera before entering the render pass.
    terrain->Update(flyCamera.GetPos(), commandBuffer);

    GetRenderer().Update(commandBuffer, frameIndex);

    // Main Render Pass
    auto postProcessingInputRenderTextureView = postProcessing->GetPostProcessingInputTextureView();

    commandBuffer.InsertBarriers()
        .AddImageBarrier()
            .SetImage(multisampleTextureView->GetTexture())
            .SetLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal)
            .InferStagesAndAccess()
            .FinishImageBarrier()
        .AddImageBarrier()
            .SetImage(postProcessingInputRenderTextureView->GetTexture())
            .SetLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal)
            .InferStagesAndAccess()
            .FinishImageBarrier()
        .AddImageBarrier()
            .SetImage(depthTextureView->GetTexture())
            .SetAspect(vk::ImageAspectFlagBits::eDepth)
            .SetLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal)
            .InferStagesAndAccess()
            .FinishImageBarrier()
        .FinishBarriers();

    commandBuffer.BeginRendering()
        .SetRenderArea({0, 0}, presenter.GetExtent())
        .AddColorAttachment()
            .SetView(multisampleTextureView)
            .EnableMultisample(postProcessingInputRenderTextureView)
            .FinishAttachment()
        .SetDepthAttachmentSimple(depthTextureView)
        .FinishBeginRendering();

    commandBuffer.SetViewport(
        {0.0f, static_cast<float>(presenter.GetExtent().height)},
        {static_cast<float>(presenter.GetExtent().width), -static_cast<float>(presenter.GetExtent().height)});
    commandBuffer.SetScissor({0, 0}, presenter.GetExtent());

    terrain->Draw(window->getHandle(), commandBuffer, frameIndex);
    skybox->Draw(flyCamera.GetProjMat(), flyCamera.GetViewMat(), commandBuffer, frameIndex);
    GetRenderer().Draw(commandBuffer, frameIndex);

    commandBuffer.EndRendering();

    // Post Processing Render Pass
    presenter.PrepareImageForRendering(commandBuffer, imageIndex);
    postProcessing->PostProcessingRenderPass(commandBuffer, surfaceImageView, presenter.GetExtent());
    presenter.PrepareImageForPresentation(commandBuffer, imageIndex);

    GetCommandEncoder().SubmitFrame(frameIndex, onRenderFinishedSemaphore);
    result = presenter.Present(imageIndex);
    switch (result) {
        case dvk::PresenterResult::Fail_MustResize:
            resizeSurface();
            return;
        default:
            break;
    }
}

void Game::resizeSurface() {
    GetDevice().GetQueue().waitIdle();

    presenter.Resize(surface, { window->GetWidth(), window->GetHeight() });

    regenerateRenderTextures();
}

void Game::regenerateRenderTextures() {
    vk::Extent3D extent = { presenter.GetExtent().width, presenter.GetExtent().height, 1 };

    depthTextureView = dvk::TextureView::Create(GetDevice(), dvk::Texture::Create(GetDevice())
        .SetLabel("Depth Texture")
        .SetUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment)
        .SetFormat(GetDepthFormat())
        .SetSampleCount(GetMultisampleCount())
        .SetExtent(extent)
        .Build())
        .SetAspect(vk::ImageAspectFlagBits::eDepth)
        .Build();

    multisampleTextureView = dvk::TextureView::Create(GetDevice(), dvk::Texture::Create(GetDevice())
        .SetLabel("Pre Post Processing Multisample Render Texture")
        .SetUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .SetFormat(GetRenderTextureFormat())
        .SetSampleCount(GetMultisampleCount())
        .SetExtent(extent)
        .Build())
        .SetAspect(vk::ImageAspectFlagBits::eColor)
        .Build();

    postProcessing->ResizeRenderTexture(glm::ivec2(presenter.GetExtent().width, presenter.GetExtent().height));
}