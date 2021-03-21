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


    GpuBuffer::GpuBuffer(ID3D12Device5* device, uint64_t buffer_size, D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_FLAGS flags,
        D3D12_RESOURCE_STATES init_state, wchar_t const* name)
    {
        D3D12_HEAP_PROPERTIES const upload_heap_prop = {heap_type, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};

        D3D12_RESOURCE_DESC const buffer_desc = {
            D3D12_RESOURCE_DIMENSION_BUFFER, 0, buffer_size, 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0}, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags};
        TIFHR(device->CreateCommittedResource(
            &upload_heap_prop, D3D12_HEAP_FLAG_NONE, &buffer_desc, init_state, nullptr, UuidOf<ID3D12Resource>(), resource_.PutVoid()));
        if (name != nullptr)
        {
            resource_->SetName(name);
        }
    }

    GpuBuffer::GpuBuffer() noexcept = default;

    GpuBuffer::GpuBuffer(GpuBuffer&& other) noexcept : resource_(std::move(other.resource_))
    {
    }

    GpuBuffer& GpuBuffer::operator=(GpuBuffer&& other) noexcept
    {
        if (this != &other)
        {
            resource_ = std::move(other.resource_);
        }
        return *this;
    }

    GpuBuffer::~GpuBuffer() noexcept = default;

    ID3D12Resource* GpuBuffer::Resource() const noexcept
    {
        return resource_.Get();
    }

    uint64_t GpuBuffer::Size() const noexcept
    {
        return resource_ ? resource_->GetDesc().Width : 0;
    }


    GpuDefaultBuffer::GpuDefaultBuffer() noexcept = default;
    GpuDefaultBuffer::GpuDefaultBuffer(GpuDefaultBuffer&& other) noexcept = default;
    GpuDefaultBuffer& GpuDefaultBuffer::operator=(GpuDefaultBuffer&& other) noexcept = default;

    GpuDefaultBuffer::GpuDefaultBuffer(
        ID3D12Device5* device, uint64_t buffer_size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, wchar_t const* name)
        : GpuBuffer(device, buffer_size, D3D12_HEAP_TYPE_DEFAULT, flags, init_state, name)
    {
    }


    GpuUploadBuffer::GpuUploadBuffer() noexcept = default;

    GpuUploadBuffer::GpuUploadBuffer(GpuUploadBuffer&& other) noexcept
        : GpuBuffer(std::move(other)), mapped_data_(std::move(other.mapped_data_))
    {
    }

    GpuUploadBuffer& GpuUploadBuffer::operator=(GpuUploadBuffer&& other) noexcept
    {
        if (this != &other)
        {
            GpuBuffer::operator=(std::move(other));
            mapped_data_ = std::move(other.mapped_data_);
        }
        return *this;
    }

    GpuUploadBuffer::GpuUploadBuffer(ID3D12Device5* device, uint64_t buffer_size, wchar_t const* name)
        : GpuBuffer(device, buffer_size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, name)
    {
        D3D12_RANGE const read_range{0, 0};
        TIFHR(resource_->Map(0, &read_range, &mapped_data_));
    }

    GpuUploadBuffer::~GpuUploadBuffer() noexcept
    {
        if (resource_.Get())
        {
            resource_->Unmap(0, nullptr);
        }
    }


    GpuReadbackBuffer::GpuReadbackBuffer() noexcept = default;

    GpuReadbackBuffer::GpuReadbackBuffer(GpuReadbackBuffer&& other) noexcept
        : GpuBuffer(std::move(other)), mapped_data_(std::move(other.mapped_data_))
    {
    }

    GpuReadbackBuffer& GpuReadbackBuffer::operator=(GpuReadbackBuffer&& other) noexcept
    {
        if (this != &other)
        {
            GpuBuffer::operator=(std::move(other));
            mapped_data_ = std::move(other.mapped_data_);
        }
        return *this;
    }

    GpuReadbackBuffer::GpuReadbackBuffer(ID3D12Device5* device, uint64_t buffer_size, wchar_t const* name)
        : GpuBuffer(device, buffer_size, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, name)
    {
        TIFHR(resource_->Map(0, nullptr, &mapped_data_));
    }

    GpuReadbackBuffer::~GpuReadbackBuffer() noexcept
    {
        if (resource_.Get())
        {
            D3D12_RANGE const write_range{0, 0};
            resource_->Unmap(0, &write_range);
        }
    }
} // namespace GoldenSun
