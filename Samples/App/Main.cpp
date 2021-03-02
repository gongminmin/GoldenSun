#include "pch.hpp"

#include "GoldenSunApp.hpp"

int WINAPI WinMain([[maybe_unused]] HINSTANCE instance, [[maybe_unused]] HINSTANCE prev_instance, [[maybe_unused]] LPSTR cmd_line,
    [[maybe_unused]] int cmd_show)
{
    GoldenSun::GoldenSunApp app(1280, 720);
    return app.Run();
}
