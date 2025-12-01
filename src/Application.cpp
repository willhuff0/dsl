#include "Application.hpp"

#include <raylib-cpp/raylib.hpp>

Application::Application()
    : window(1920, 1080, "Zombies"),
      camera(Vector3(0.0f, 5.0f, -15.0f)),
      terrain(6767) {
    camera.fovy = 90.0f;
    camera.target = Vector3(0.0f, 0.0f, 0.0f);

    DisableCursor();
}

Application::~Application() {

}

bool Application::IsRunning() {
    return !window.ShouldClose();
}

void Application::MainLoop() {
    // Tick

    terrain.Tick(camera.position);
    camera.Update(CAMERA_FREE);

    // Draw

    BeginDrawing();
    ClearBackground(raylib::Color::Black());
    DrawFPS(10, 10);

    BeginMode3D(camera);

//    DrawGrid(10, 10);
    {
        raylib::Vector3 origin = { 0.0f, 20.0f, 0.0f };
        raylib::Vector3 axisX = { 10.0f, 0.0f, 0.0f };
        raylib::Vector3 axisY = { 0.0f, 10.0f, 0.0f };
        raylib::Vector3 axisZ = { 0.0f, 0.0f, 10.0f };

        DrawLine3D(origin, origin + axisX, RED);   // X-axis (Red)
        DrawLine3D(origin, origin + axisY, GREEN); // Y-axis (Green)
        DrawLine3D(origin, origin + axisZ, BLUE);  // Z-axis (Blue)
    }

    terrain.Draw();

    EndMode3D();
    EndDrawing();
}
