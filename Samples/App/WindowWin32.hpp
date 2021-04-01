#pragma once

#include <string>

namespace GoldenSun
{
    class GoldenSunApp;

    class WindowWin32 final
    {
        DISALLOW_COPY_AND_ASSIGN(WindowWin32)

    public:
        WindowWin32() noexcept;
        WindowWin32(GoldenSunApp& app, std::wstring const& title);

        WindowWin32(WindowWin32&& other) noexcept;
        WindowWin32& operator=(WindowWin32&& other) noexcept;

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
