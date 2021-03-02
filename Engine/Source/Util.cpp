#include "pch.hpp"

#include <GoldenSun/Util.hpp>

namespace GoldenSun
{
    DXGI_FORMAT LinearFormatOf(DXGI_FORMAT fmt) noexcept
    {
        switch (fmt)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
            return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
            return DXGI_FORMAT_B8G8R8X8_UNORM;
        case DXGI_FORMAT_BC1_UNORM_SRGB:
            return DXGI_FORMAT_BC1_UNORM;
        case DXGI_FORMAT_BC2_UNORM_SRGB:
            return DXGI_FORMAT_BC2_UNORM;
        case DXGI_FORMAT_BC3_UNORM_SRGB:
            return DXGI_FORMAT_BC3_UNORM;
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            return DXGI_FORMAT_BC7_UNORM;
        default:
            return fmt;
        }
    }

    DXGI_FORMAT SRGBFormatOf(DXGI_FORMAT fmt) noexcept
    {
        switch (fmt)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case DXGI_FORMAT_B8G8R8A8_UNORM:
            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case DXGI_FORMAT_B8G8R8X8_UNORM:
            return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
        case DXGI_FORMAT_BC1_UNORM:
            return DXGI_FORMAT_BC1_UNORM_SRGB;
        case DXGI_FORMAT_BC2_UNORM:
            return DXGI_FORMAT_BC2_UNORM_SRGB;
        case DXGI_FORMAT_BC3_UNORM:
            return DXGI_FORMAT_BC3_UNORM_SRGB;
        case DXGI_FORMAT_BC7_UNORM:
            return DXGI_FORMAT_BC7_UNORM_SRGB;
        default:
            return fmt;
        }
    }
} // namespace GoldenSun
