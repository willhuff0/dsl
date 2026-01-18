#include "Platform.hpp"
#include "Provider.hpp"
#include "Game.hpp"

#include <dvk/dvk.hpp>

int main() {
    Platform platform;
    auto extensions = platform.GetVulkanRequiredExtensions();

    dvk::Instance instance(extensions);

    auto window = platform.CreateWindow(1920, 1080, "Hello, SLGL!");
    dvk::Surface surface(instance, window->GetVulkanSurface(instance.GetInstance()));

    dvk::Device device(instance, surface);
    dvk::Presenter presenter(device, surface, vk::Extent2D { window->GetWidth(), window->GetHeight() });
    dvk::CommandEncoder commandEncoder(device);

    SetupProvider(&platform, &device, &commandEncoder, presenter.GetFormat());
    {
        Game game(&platform, window, surface, presenter);
        while (!window->ShouldClose()) {
            game.MainLoop();
        }
        device.GetQueue().waitIdle();
    }
    FreeProvider();

    return 0;
}
