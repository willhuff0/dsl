#include "Application.hpp"

int main() {
    Application app;
    while (app.IsRunning()) {
        app.MainLoop();
    }
    return 0;
}