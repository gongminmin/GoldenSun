#pragma once

#include <string_view>
#include <vector>

#include <d3d12.h>

#include <GoldenSun/Mesh.hpp>

namespace GoldenSun
{
    class GpuSystem;

    std::vector<Mesh> LoadMesh(GpuSystem& gpu_system, std::string_view file_name);
} // namespace GoldenSun
