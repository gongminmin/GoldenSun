#include "pch.hpp"

#include "WindowWin32.hpp"

#include "GoldenSunApp.hpp"

namespace GoldenSun
{
    WindowWin32::WindowWin32(GoldenSunApp& app, std::wstring const& title) : app_(&app)
    {
        HINSTANCE instance = ::GetModuleHandle(nullptr);

        WNDCLASSEX wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = instance;
        wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
        wc.lpszClassName = L"GoldenSunClass";
        ::RegisterClassEx(&wc);

        RECT rect = {0, 0, static_cast<LONG>(app.Width()), static_cast<LONG>(app.Height())};
        ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

        hwnd_ = ::CreateWindowW(wc.lpszClassName, title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left,
            rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);

        ::SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }

    void WindowWin32::ShowWindow(int cmd_show)
    {
        ::ShowWindow(hwnd_, cmd_show);
    }

    void WindowWin32::WindowText(std::wstring const& text)
    {
        ::SetWindowText(hwnd_, text.c_str());
    }

    LRESULT CALLBACK WindowWin32::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
    {
        WindowWin32* window = reinterpret_cast<WindowWin32*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));

        switch (message)
        {
        case WM_ACTIVATE:
            if (window)
            {
                window->app_->Active(LOWORD(wparam) != WA_INACTIVE);
            }
            return 0;

        case WM_PAINT:
            if (window)
            {
                window->app_->Refresh();
            }
            return 0;

        case WM_SIZE:
            if (window)
            {
                RECT rect{};
                ::GetClientRect(hwnd, &rect);
                window->app_->SizeChanged(rect.right - rect.left, rect.bottom - rect.top, wparam == SIZE_MINIMIZED);
            }
            return 0;

        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        }

        return ::DefWindowProc(hwnd, message, wparam, lparam);
    }
} // namespace GoldenSun
