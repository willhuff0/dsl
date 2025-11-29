#pragma once

#include "Terrain.hpp"

#include <raylib.h>

class Application {
public:
    Application();
    ~Application();

    bool IsRunning();
    void MainLoop();

private:
    Camera camera { 0 };
    Terrain terrain;
};