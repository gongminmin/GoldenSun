#pragma once

#include <string_view>

#include <d3d12.h>

#include <GoldenSun/Gpu/GpuSystem.hpp>

namespace GoldenSun
{
    GpuTexture2D LoadTexture(GpuSystem& gpu_system, std::string_view file_name, DXGI_FORMAT format);
    void SaveTexture(GpuSystem& gpu_system, GpuTexture2D const& texture, std::string_view file_name);
} // namespace GoldenSun
