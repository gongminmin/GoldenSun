#pragma once

#include <d3d12.h>

#include <type_traits>
#include <vector>

#include <GoldenSun/Base.hpp>
#include <GoldenSun/ComPtr.hpp>

namespace GoldenSun
{
    DXGI_FORMAT LinearFormatOf(DXGI_FORMAT fmt) noexcept;
    DXGI_FORMAT SRGBFormatOf(DXGI_FORMAT fmt) noexcept;
    uint32_t FormatSize(DXGI_FORMAT fmt) noexcept;

    D3D12_ROOT_PARAMETER CreateRootParameterAsDescriptorTable(const D3D12_DESCRIPTOR_RANGE* descriptor_ranges,
        uint32_t num_descriptor_ranges, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept;
    D3D12_ROOT_PARAMETER CreateRootParameterAsShaderResourceView(
        uint32_t shader_register, uint32_t register_space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept;
    D3D12_ROOT_PARAMETER CreateRootParameterAsConstantBufferView(
        uint32_t shader_register, uint32_t register_space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept;
    D3D12_ROOT_PARAMETER CreateRootParameterAsConstants(uint32_t num_32bit_values, uint32_t shader_register, uint32_t register_space = 0,
        D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept;
} // namespace GoldenSun
