#include <Game.hpp>

#include <Provider.hpp>
#include <utils/Paths.hpp>
#include <TempImageLoader.hpp>

#include <utility>
#include <cstdlib>
#include <iostream>
#include <filesystem>

constexpr std::chrono::milliseconds MIN_SHADER_RECOMPILE_INTERVAL(500);

Game::Game(Platform* platform, Window::Ref window, dvk::Surface& surface, dvk::Presenter& presenter)
    : platform(platform),
      window(std::move(window)),
      surface(surface),
      presenter(presenter),
      flyCamera(static_cast<GLFWwindow*>(this->window->GetNativeHandle()), glm::vec3(0.01f, 26.0f, 0.01f)) {
    postProcessing = std::make_unique<PostProcessing>();
    skybox = std::make_unique<Skybox>(Skybox::FromEquirectangularMap(LoadHDRI(SKYBOX_HDRI_PATH)));
    terrain = std::make_unique<Terrain>();

    SetSkybox(skybox->GetIrradianceMap(), skybox->GetRadianceMap(), skybox->GetBrdfLut());

    // meshRenderer = std::make_unique<MeshRenderer>();

    GetDevice().GetQueue().waitIdle();
    regenerateRenderTextures();

#if !defined(NDEBUG)
    lastShaderRecompile = std::chrono::high_resolution_clock::now();
    shaderSrcWatcher = HotReload::WatchCustom(SHADER_SRC_BASEPATH, [this](const std::string& path, const filewatch::Event event) {
        std::lock_guard lock (shaderRecompileMutex);
        if (std::chrono::high_resolution_clock::now() - lastShaderRecompile < MIN_SHADER_RECOMPILE_INTERVAL) return;
        lastShaderRecompile = std::chrono::high_resolution_clock::now();
        switch (event) {
            case filewatch::Event::added:
            case filewatch::Event::modified:
            case filewatch::Event::renamed_new: {
                std::cout << "Recompiling shader (" << path << ")!" << std::endl;
                std::filesystem::path srcPath = SHADER_SRC_BASEPATH / std::filesystem::path(path);
                std::filesystem::path dstPath = SHADER_RES_BASEPATH / std::filesystem::path(path).parent_path() / (std::filesystem::path(path).stem().string() + ".spv");
                std::string command("slangc " + srcPath.string() + " -target spirv -emit-spirv-directly -fvk-use-entrypoint-name -o " + dstPath.string());
                int result = std::system(command.c_str());
                if (result == 0) {
                    std::cout << "Shader recompiled (" << path << ")!"<< std::endl;
                } else {
                    std::cout << "Failed to recompile shader (" << path << ")!"<< std::endl;
                }
                break;
            }
            default:
                break;
        }
    });
#endif
}

void Game::MainLoop() {
    HotReload::Attempt(GetDevice());

    platform->ProcessEvents();

    // Tick

    flyCamera.Tick();
    // terrain->Tick(flyCamera.GetPos());

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

    // Render

    GlobalUniforms globalUniforms {
        .viewProjMatrix = flyCamera.GetProjMat() * flyCamera.GetViewMat(),
        .viewPosition = glm::vec4(flyCamera.GetPos(), 1.0f),
        .radianceMipMapCount = static_cast<uint32_t>(Skybox::GetRadianceMipMapCount()),
    };
    SetGlobalUniforms(&globalUniforms, frameIndex);

    // Main Render Pass
    auto postProcessingInputRenderTextureView = postProcessing->GetPostProcessingInputTextureView();

    commandBuffer.InsertBarriers()
        .AddImageBarrier()
            .SetTexture(multisampleTextureView->GetTexture())
            .SetLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal)
            .InferStagesAndAccess(true)
            .FinishImageBarrier()
        .AddImageBarrier()
            .SetTexture(postProcessingInputRenderTextureView->GetTexture())
            .SetLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal)
            .InferStagesAndAccess(true)
            .FinishImageBarrier()
        .AddImageBarrier()
            .SetTexture(depthTextureView->GetTexture())
            .SetAspect(vk::ImageAspectFlagBits::eDepth)
            .SetLayout(vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal)
            .InferStagesAndAccess(true)
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

    commandBuffer.SetViewport({0.0f, 0.0f}, {static_cast<float>(presenter.GetExtent().width), static_cast<float>(presenter.GetExtent().height)});
    commandBuffer.SetScissor({0, 0}, presenter.GetExtent());

    terrain->Draw(window->getHandle(), commandBuffer, frameIndex);
    skybox->Draw(flyCamera.GetProjMat(), flyCamera.GetViewMat(), commandBuffer, frameIndex);

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