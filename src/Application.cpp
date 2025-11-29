#include "Application.hpp"

#include <raylib.h>

Application::Application() : terrain(1337) {
    InitWindow(800, 450, "raylib [core] example - basic window");

    camera.fovy = 90.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    camera.position = { 0.0f, 5.0f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.target = { 0.0f, 0.0f, 15.0f };
}

Application::~Application() {
    CloseWindow();
}

bool Application::IsRunning() {
    return !WindowShouldClose();
}

void Application::MainLoop() {
    // Tick

    terrain.Tick({0.0f, 0.0f, 0.0f});


    // Draw

    BeginDrawing();
    ClearBackground(RAYWHITE);
    BeginMode3D(camera);

    terrain.Draw();

    EndMode3D();
    EndDrawing();
}