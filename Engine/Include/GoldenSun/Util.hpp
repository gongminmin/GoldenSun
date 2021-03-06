#pragma once

#include <d3d12.h>

#include <GoldenSun/Base.hpp>

namespace GoldenSun
{
    static constexpr uint32_t FrameCount = 3;

    template <uint32_t Alignment>
    constexpr uint32_t Align(uint32_t size) noexcept
    {
        static_assert((Alignment & (Alignment - 1)) == 0);
        return (size + (Alignment - 1)) & ~(Alignment - 1);
    }

    template <typename T>
    struct ConstantBufferWrapper : T
    {
        uint8_t padding[Align<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(sizeof(T)) - sizeof(T)];
    };

    DXGI_FORMAT GOLDEN_SUN_API LinearFormatOf(DXGI_FORMAT fmt) noexcept;
    DXGI_FORMAT GOLDEN_SUN_API SRGBFormatOf(DXGI_FORMAT fmt) noexcept;

    D3D12_ROOT_PARAMETER GOLDEN_SUN_API CreateRootParameterAsDescriptorTable(const D3D12_DESCRIPTOR_RANGE* descriptor_ranges,
        uint32_t num_descriptor_ranges, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept;
    D3D12_ROOT_PARAMETER GOLDEN_SUN_API CreateRootParameterAsShaderResourceView(
        uint32_t shader_register, uint32_t register_space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept;
    D3D12_ROOT_PARAMETER GOLDEN_SUN_API CreateRootParameterAsConstantBufferView(
        uint32_t shader_register, uint32_t register_space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept;
    D3D12_ROOT_PARAMETER GOLDEN_SUN_API CreateRootParameterAsConstants(uint32_t num_32bit_values, uint32_t shader_register,
        uint32_t register_space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept;
} // namespace GoldenSun
