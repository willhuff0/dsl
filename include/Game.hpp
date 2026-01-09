#pragma once

#include <Platform.hpp>
#include <dvk/dvk.hpp>

#include "FlyCamera.hpp"
#include "PostProcessing.hpp"
#include "Terrain.hpp"
#include "Skybox.hpp"
// #include "MeshRenderer.hpp"

#include <memory>

class Game {
public:
    Game(Platform* platform, Window::Ref window, dvk::Surface& surface, dvk::Presenter& presenter);

    void MainLoop();

private:
    void resizeSurface();
    void regenerateRenderTextures();

    Platform* platform;
    Window::Ref window;

    dvk::Surface& surface;
    dvk::Presenter& presenter;
    dvk::TextureView::Ref depthTextureView;
    dvk::TextureView::Ref multisampleTextureView;

    FlyCamera flyCamera;
    std::unique_ptr<PostProcessing> postProcessing = nullptr;
    std::unique_ptr<Skybox> skybox = nullptr;
    std::unique_ptr<Terrain> terrain = nullptr;

    // std::unique_ptr<MeshRenderer> meshRenderer = nullptr;
};
