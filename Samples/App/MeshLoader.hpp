#pragma once

#include <string_view>
#include <vector>

#include <d3d12.h>

#include <GoldenSun/Mesh.hpp>

namespace GoldenSun
{
    std::vector<Mesh> LoadMesh(ID3D12Device5* device, std::string_view const input_name);
}
