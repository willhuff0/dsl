#pragma once

#include "Terrain.hpp"

#include <raylib-cpp.hpp>

class Application {
public:
    Application();
    ~Application();

    bool IsRunning();
    void MainLoop();

private:
    raylib::Window window;
    raylib::Camera camera;
    Terrain terrain;
};