#pragma once

#ifdef _WINDOWS
#include <windows.h>
#endif

#include <memory>

namespace GoldenSun
{
#ifdef _WINDOWS
    class Win32HandleDeleter final
    {
    public:
        void operator()(HANDLE handle)
        {
            if (handle != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(handle);
            }
        }
    };
    using Win32UniqueHandle = std::unique_ptr<void, Win32HandleDeleter>;
    using Win32SharedHandle = std::shared_ptr<void>;

    inline Win32UniqueHandle MakeWin32UniqueHandle(HANDLE handle)
    {
        return Win32UniqueHandle(handle);
    }
    inline Win32SharedHandle MakeWin32SharedHandle(HANDLE handle)
    {
        return Win32SharedHandle(handle, Win32HandleDeleter());
    }
#endif
} // namespace GoldenSun
