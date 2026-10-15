#include <Game.hpp>

#include <Provider.hpp>
#include <utils/Paths.hpp>
#include <TempImageLoader.hpp>
#include <utils/UploadQueue.hpp>

#include <imgui.h>

#include <utility>
#include <filesystem>

#include "Physics.hpp"
#include "components/PhysicsComponent.hpp"

constexpr std::chrono::milliseconds MIN_SHADER_RECOMPILE_INTERVAL(500);

Game::Game(Platform* platform, Window::Ref window, dvk::Surface& surface, dvk::Presenter& presenter)
    : platform(platform),
      window(std::move(window)),
      surface(surface),
      presenter(presenter),
      flyCamera(static_cast<GLFWwindow*>(this->window->GetNativeHandle()), glm::vec3(0.0f, 0.5f, 10.0f)) {
    gui = std::make_unique<Gui>(static_cast<GLFWwindow*>(this->window->GetNativeHandle()), presenter);
    postProcessing = std::make_unique<PostProcessing>();
    skybox = std::make_unique<Skybox>(Skybox::FromEquirectangularMap(&flyCamera, LoadHDRI(SKYBOX_HDRI_PATH)));
    directionalLight = std::make_unique<DirectionalLight>(&flyCamera);
    SetupRenderer(&flyCamera);
    terrain = std::make_unique<Terrain>(
        &flyCamera,
        skybox.get(),
        directionalLight.get()
    );
    modelLoader = std::make_unique<ModelLoader>(
        &flyCamera,
        skybox.get(),
        directionalLight.get()
    );
    GetCommandEncoder().ExecuteAndWait([&](dvk::CommandBuffer& commandBuffer) {
        // models.emplace_back(std::make_unique<Model>(Model::LoadFile(commandBuffer, 0, RESOURCE_BASEPATH "/models/AlphaBlendModeTest.glb")));
        // models.emplace_back(std::make_unique<Model>(modelLoader->LoadFile(commandBuffer, 0, RESOURCE_BASEPATH "/Bistro_Godot.glb")));
        auto& model = models.emplace_back(std::make_unique<Model>(modelLoader->LoadFile(commandBuffer, 0, RESOURCE_BASEPATH "/models/DamagedHelmet.glb", true)));
        model->GetRoot()->AddComponent(std::make_shared<PhysicsComponent>());
        model->GetRoot()->EnterScene();
    });
    // models[0]->GetRoot()->SetLScale(glm::vec3(5.0f, 5.0f, 5.0f));
    // models[0]->GetRoot()->SetLPos(glm::vec3(0.0f, -5.0f, -100.0f));

    GetDevice().GetQueue().waitIdle();
    regenerateRenderTextures();
}

void Game::MainLoop() {
    platform->ProcessEvents();

    // Tick
    flyCamera.Tick();
    GetPhysics().Tick();

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

    // Kick off anything queued since last frame.
    GetUploadQueue().Submit();

    HotReload::Attempt(frameIndex, GetDevice());
    gui->BeginFrame();
    gui->DrawFpsOverlay();

    terrain->Update(flyCamera.GetPos(), commandBuffer);
    GetRenderer().UploadWorldState(frameIndex);

    commandBuffer.BeginRegion("CSM Depth");
    directionalLight->RegenerateLightSpaceMatrices(
        frameIndex,
        flyCamera.GetViewMat(),
        flyCamera.GetZNear(),
        flyCamera.GetZFar(),
        flyCamera.GetFovRad(),
        flyCamera.GetAspectRatio()
    );
    directionalLight->DrawShadowMap(commandBuffer, frameIndex);
    commandBuffer.EndRegion();

    commandBuffer.BeginRegion("Main");
    flyCamera.UpdateUniforms(frameIndex);
    GetRenderer().ComputeDrawCommands(commandBuffer, frameIndex, &flyCamera);

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

    GetRenderer().DrawOpaque(commandBuffer, frameIndex,
        &flyCamera, skybox.get(), directionalLight.get());
    terrain->Draw(window->getHandle(), commandBuffer, frameIndex,
        &flyCamera, skybox.get(), directionalLight.get());
    skybox->Draw(   commandBuffer, frameIndex,
        &flyCamera);
    GetRenderer().DrawTransparent(commandBuffer, frameIndex,
        &flyCamera, skybox.get(), directionalLight.get());

    commandBuffer.EndRendering();
    commandBuffer.EndRegion();

    commandBuffer.BeginRegion("Post Processing");
    presenter.PrepareImageForRendering(commandBuffer, imageIndex);
    postProcessing->PostProcessingRenderPass(commandBuffer, surfaceImageView, presenter.GetExtent());
    commandBuffer.EndRegion();
    commandBuffer.BeginRegion("GUI");
    gui->GuiRenderPass(commandBuffer, surfaceImageView, presenter.GetExtent());
    commandBuffer.EndRegion();
    presenter.PrepareImageForPresentation(commandBuffer, imageIndex);

    // The renderer only draws uploads that had completed when UploadWorldState ran, so this wait is already
    // satisfied, but it is what makes the transfer queue's writes visible to this submit.
    auto uploadWait = GetUploadQueue().GetWaitInfo(GetRenderer().GetPublishedUploads());
    GetCommandEncoder().SubmitFrame(frameIndex, onRenderFinishedSemaphore, { &uploadWait, 1 });
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
    gui->Resize(presenter);

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