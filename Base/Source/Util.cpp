#include "pch.hpp"

#include <GoldenSun/Util.hpp>

#include <filesystem>

namespace GoldenSun
{
    std::string ExeDirectory()
    {
        char exe_file[MAX_PATH];
        uint32_t size = ::GetModuleFileNameA(nullptr, exe_file, static_cast<uint32_t>(std::size(exe_file)));
        Verify((size != 0) && (size != std::size(exe_file)));

        std::filesystem::path exe_path = exe_file;
        return exe_path.parent_path().string() + '/';
    }

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

    uint32_t FormatSize(DXGI_FORMAT fmt) noexcept
    {
        switch (fmt)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
            return 4;

        default:
            // TODO: Support more formats
            assert(false);
            return 0;
        }
    }

    D3D12_ROOT_PARAMETER CreateRootParameterAsDescriptorTable(
        const D3D12_DESCRIPTOR_RANGE* descriptor_ranges, uint32_t num_descriptor_ranges, D3D12_SHADER_VISIBILITY visibility) noexcept
    {
        D3D12_ROOT_PARAMETER ret;
        ret.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        ret.DescriptorTable.NumDescriptorRanges = num_descriptor_ranges;
        ret.DescriptorTable.pDescriptorRanges = descriptor_ranges;
        ret.ShaderVisibility = visibility;
        return ret;
    }

    D3D12_ROOT_PARAMETER CreateRootParameterAsShaderResourceView(
        uint32_t shader_register, uint32_t register_space, D3D12_SHADER_VISIBILITY visibility) noexcept
    {
        D3D12_ROOT_PARAMETER ret;
        ret.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        ret.Descriptor.ShaderRegister = shader_register;
        ret.Descriptor.RegisterSpace = register_space;
        ret.ShaderVisibility = visibility;
        return ret;
    }

    D3D12_ROOT_PARAMETER CreateRootParameterAsConstantBufferView(
        uint32_t shader_register, uint32_t register_space, D3D12_SHADER_VISIBILITY visibility) noexcept
    {
        D3D12_ROOT_PARAMETER ret;
        ret.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        ret.Descriptor.ShaderRegister = shader_register;
        ret.Descriptor.RegisterSpace = register_space;
        ret.ShaderVisibility = visibility;
        return ret;
    }

    D3D12_ROOT_PARAMETER CreateRootParameterAsConstants(
        uint32_t num_32bit_values, uint32_t shader_register, uint32_t register_space, D3D12_SHADER_VISIBILITY visibility) noexcept
    {
        D3D12_ROOT_PARAMETER ret;
        ret.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        ret.Constants.Num32BitValues = num_32bit_values;
        ret.Constants.ShaderRegister = shader_register;
        ret.Constants.RegisterSpace = register_space;
        ret.ShaderVisibility = visibility;
        return ret;
    }
} // namespace GoldenSun
