#pragma once

#include <GoldenSun/Base.hpp>

namespace GoldenSun
{
    static constexpr uint32_t FrameCount = 3;

    DXGI_FORMAT GOLDEN_SUN_API LinearFormatOf(DXGI_FORMAT fmt) noexcept;
    DXGI_FORMAT GOLDEN_SUN_API SRGBFormatOf(DXGI_FORMAT fmt) noexcept;
} // namespace GoldenSun
