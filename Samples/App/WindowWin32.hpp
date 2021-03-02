#pragma once

#include <string>

namespace GoldenSun
{
    class GoldenSunApp;

    class WindowWin32 final
    {
    public:
        WindowWin32(GoldenSunApp& app, std::wstring const& title);

        void ShowWindow(int cmd_show);
        void WindowText(std::wstring const& text);

        HWND Hwnd() const noexcept
        {
            return hwnd_;
        }

    private:
        static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    private:
        GoldenSunApp* app_ = nullptr;
        HWND hwnd_ = nullptr;
    };
} // namespace GoldenSun
