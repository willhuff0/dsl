#pragma once

#include <Platform.hpp>
#include <dvk/dvk.hpp>

#include "HotReload.hpp"
#include "FlyCamera.hpp"
#include "PostProcessing.hpp"
#include "terrain/Terrain.hpp"
#include "Skybox.hpp"
#include "DirectionalLight.hpp"
#include "Model.hpp"
#include "Gui.hpp"

#include <memory>
#include <chrono>
#include <mutex>

class Game {
public:
    Game(Platform* platform, Window::Ref window, dvk::Surface& surface, dvk::Presenter& presenter);

    void MainLoop();

private:
    void resizeSurface();
    void regenerateRenderTextures();

#if !defined(NDEBUG)
    Watcher shaderSrcWatcher;
    std::mutex shaderRecompileMutex;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastShaderRecompile;
#endif

    Platform* platform;
    Window::Ref window;

    dvk::Surface& surface;
    dvk::Presenter& presenter;
    dvk::TextureView::Ref depthTextureView;
    dvk::TextureView::Ref multisampleTextureView;

    FlyCamera flyCamera;
    std::unique_ptr<Gui> gui = nullptr;
    std::unique_ptr<PostProcessing> postProcessing = nullptr;
    std::unique_ptr<Skybox> skybox = nullptr;
    std::unique_ptr<DirectionalLight> directionalLight = nullptr;
    std::unique_ptr<Terrain> terrain = nullptr;

    std::unique_ptr<ModelLoader> modelLoader = nullptr;
    std::vector<std::unique_ptr<Model>> models;
};
