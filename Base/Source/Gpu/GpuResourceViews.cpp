#include "../pch.hpp"

#include <GoldenSun/Gpu/GpuSystem.hpp>
#include <GoldenSun/Util.hpp>

#include <string>

#include "../ImplPtrImpl.hpp"
#include "GpuSystemInternal.hpp"

namespace GoldenSun
{
    class GpuShaderResourceView::Impl final
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        Impl(ID3D12Device5* device, GpuBuffer const& buffer, uint32_t num_elements, uint32_t element_size,
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
            : cpu_handle_(cpu_handle)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Buffer.NumElements = num_elements;
            srv_desc.Buffer.StructureByteStride = element_size;
            if (element_size == 0)
            {
                srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
                srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
            }
            else
            {
                srv_desc.Format = DXGI_FORMAT_UNKNOWN;
                srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
            }
            device->CreateShaderResourceView(reinterpret_cast<ID3D12Resource*>(buffer.NativeResource()), &srv_desc, cpu_handle);
        }

        Impl(ID3D12Device5* device, GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
            : cpu_handle_(cpu_handle)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.Format = (format == DXGI_FORMAT_UNKNOWN) ? texture.Format() : format;
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Texture2D.MostDetailedMip = 0;
            srv_desc.Texture2D.MipLevels = texture.MipLevels();
            srv_desc.Texture2D.PlaneSlice = 0;
            srv_desc.Texture2D.ResourceMinLODClamp = 0;
            device->CreateShaderResourceView(reinterpret_cast<ID3D12Resource*>(texture.NativeResource()), &srv_desc, cpu_handle);
        }

        explicit operator bool() const noexcept
        {
            return (cpu_handle_.ptr != 0);
        }

        void Reset() noexcept
        {
            cpu_handle_ = {};
        }

    private:
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_{};
    };


    GpuShaderResourceView::GpuShaderResourceView() = default;
    GpuShaderResourceView::~GpuShaderResourceView() noexcept = default;
    GpuShaderResourceView::GpuShaderResourceView(GpuShaderResourceView&& other) noexcept = default;
    GpuShaderResourceView& GpuShaderResourceView::operator=(GpuShaderResourceView&& other) noexcept = default;

    GpuShaderResourceView::operator bool() const noexcept
    {
        return impl_ && impl_->operator bool();
    }

    void GpuShaderResourceView::Reset() noexcept
    {
        impl_->Reset();
    }


    class GpuRenderTargetView::Impl final
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        Impl(ID3D12Device5* device, GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
            : cpu_handle_(cpu_handle)
        {
            D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
            rtv_desc.Format = (format == DXGI_FORMAT_UNKNOWN) ? texture.Format() : format;
            rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            device->CreateRenderTargetView(reinterpret_cast<ID3D12Resource*>(texture.NativeResource()), &rtv_desc, cpu_handle);
        }

        explicit operator bool() const noexcept
        {
            return (cpu_handle_.ptr != 0);
        }

        void Reset() noexcept
        {
            cpu_handle_ = {};
        }

    private:
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_{};
    };


    GpuRenderTargetView::GpuRenderTargetView() = default;
    GpuRenderTargetView::~GpuRenderTargetView() noexcept = default;
    GpuRenderTargetView::GpuRenderTargetView(GpuRenderTargetView&& other) noexcept = default;
    GpuRenderTargetView& GpuRenderTargetView::operator=(GpuRenderTargetView&& other) noexcept = default;

    GpuRenderTargetView::operator bool() const noexcept
    {
        return impl_ && impl_->operator bool();
    }

    void GpuRenderTargetView::Reset() noexcept
    {
        impl_->Reset();
    }


    class GpuUnorderedAccessView::Impl final
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        Impl(ID3D12Device5* device, GpuBuffer const& buffer, uint32_t first_element, uint32_t num_elements, uint32_t element_size,
            D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
            : cpu_handle_(cpu_handle)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uav_desc.Buffer.FirstElement = first_element;
            uav_desc.Buffer.NumElements = num_elements;
            uav_desc.Buffer.StructureByteStride = element_size;
            device->CreateUnorderedAccessView(reinterpret_cast<ID3D12Resource*>(buffer.NativeResource()), nullptr, &uav_desc, cpu_handle);
        }

        Impl(ID3D12Device5* device, GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
            : cpu_handle_(cpu_handle)
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
            uav_desc.Format = format;
            uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            device->CreateUnorderedAccessView(reinterpret_cast<ID3D12Resource*>(texture.NativeResource()), nullptr, &uav_desc, cpu_handle);
        }

        explicit operator bool() const noexcept
        {
            return (cpu_handle_.ptr != 0);
        }

        void Reset() noexcept
        {
            cpu_handle_ = {};
        }

    private:
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle_{};
    };


    GpuUnorderedAccessView::GpuUnorderedAccessView() = default;
    GpuUnorderedAccessView::~GpuUnorderedAccessView() noexcept = default;
    GpuUnorderedAccessView::GpuUnorderedAccessView(GpuUnorderedAccessView&& other) noexcept = default;
    GpuUnorderedAccessView& GpuUnorderedAccessView::operator=(GpuUnorderedAccessView&& other) noexcept = default;

    GpuUnorderedAccessView::operator bool() const noexcept
    {
        return impl_ && impl_->operator bool();
    }

    void GpuUnorderedAccessView::Reset() noexcept
    {
        impl_->Reset();
    }


    GpuShaderResourceView GpuSystemInternalD3D12::CreateShaderResourceView(ID3D12Device5* device, GpuBuffer const& buffer,
        uint32_t num_elements, uint32_t element_size, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        GpuShaderResourceView srv;
        srv.impl_ = ImplPtr<GpuShaderResourceView::Impl>(device, buffer, num_elements, element_size, cpu_handle);
        return srv;
    }

    GpuShaderResourceView GpuSystemInternalD3D12::CreateShaderResourceView(
        ID3D12Device5* device, GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        GpuShaderResourceView srv;
        srv.impl_ = ImplPtr<GpuShaderResourceView::Impl>(device, texture, format, cpu_handle);
        return srv;
    }

    GpuRenderTargetView GpuSystemInternalD3D12::CreateRenderTargetView(
        ID3D12Device5* device, GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        GpuRenderTargetView rtv;
        rtv.impl_ = ImplPtr<GpuRenderTargetView::Impl>(device, texture, format, cpu_handle);
        return rtv;
    }

    GpuUnorderedAccessView GpuSystemInternalD3D12::CreateUnorderedAccessView(ID3D12Device5* device, GpuBuffer const& buffer,
        uint32_t first_element, uint32_t num_elements, uint32_t element_size, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        GpuUnorderedAccessView uav;
        uav.impl_ = ImplPtr<GpuUnorderedAccessView::Impl>(device, buffer, first_element, num_elements, element_size, cpu_handle);
        return uav;
    }

    GpuUnorderedAccessView GpuSystemInternalD3D12::CreateUnorderedAccessView(
        ID3D12Device5* device, GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        GpuUnorderedAccessView uav;
        uav.impl_ = ImplPtr<GpuUnorderedAccessView::Impl>(device, texture, format, cpu_handle);
        return uav;
    }
} // namespace GoldenSun
