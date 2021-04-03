#pragma once

#include <d3d12.h>

#include <type_traits>
#include <vector>

#include <GoldenSun/Base.hpp>
#include <GoldenSun/ComPtr.hpp>

#define GOLDEN_SUN_UNREACHABLE(msg) __assume(false)

#if __cpp_lib_to_underlying < 202102L
namespace std
{
    template <typename Enum>
    constexpr std::underlying_type_t<Enum> to_underlying(Enum e) noexcept
    {
        return static_cast<std::underlying_type_t<Enum>>(e);
    }
} // namespace std
#else
#include <utility>
#endif

namespace GoldenSun
{
    template <uint32_t Alignment>
    constexpr uint32_t Align(uint32_t size) noexcept
    {
        static_assert((Alignment & (Alignment - 1)) == 0);
        return (size + (Alignment - 1)) & ~(Alignment - 1);
    }

    std::string ExeDirectory();

    DXGI_FORMAT LinearFormatOf(DXGI_FORMAT fmt) noexcept;
    DXGI_FORMAT SrgbFormatOf(DXGI_FORMAT fmt) noexcept;
    bool IsSrgbFormat(DXGI_FORMAT fmt) noexcept;
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
