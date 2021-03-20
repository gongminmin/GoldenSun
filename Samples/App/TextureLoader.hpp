#pragma once

#include <string_view>

#include <d3d12.h>

#include <GoldenSun/ComPtr.hpp>

namespace GoldenSun
{
    ComPtr<ID3D12Resource> LoadTexture(ID3D12Device5* device, ID3D12GraphicsCommandList4* cmd_list, std::string_view file_name);
}
