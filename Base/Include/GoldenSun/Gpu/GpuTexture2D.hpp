#pragma once

#include <GoldenSun/ImplPtr.hpp>

#include <string_view>

namespace GoldenSun
{
    class GpuSystemInternalD3D12;

    class GpuTexture2D final
    {
        friend class GpuSystemInternalD3D12;

        DISALLOW_COPY_AND_ASSIGN(GpuTexture2D)

    public:
        GpuTexture2D();
        GpuTexture2D(void* native_resource, D3D12_RESOURCE_STATES curr_state, std::wstring_view name = L"") noexcept;
        ~GpuTexture2D() noexcept;

        GpuTexture2D(GpuTexture2D&& other) noexcept;
        GpuTexture2D& operator=(GpuTexture2D&& other) noexcept;

        GpuTexture2D Share() const;

        explicit operator bool() const noexcept;

        // TODO: Remove it after finishing the GPU system
        void* NativeResource() const noexcept;

        uint32_t Width(uint32_t mip) const noexcept;
        uint32_t Height(uint32_t mip) const noexcept;
        uint32_t MipLevels() const noexcept;
        DXGI_FORMAT Format() const noexcept;
        D3D12_RESOURCE_FLAGS Flags() const noexcept;

        void Reset() noexcept;

        D3D12_RESOURCE_STATES State(uint32_t mip) const noexcept;
        void Transition(GpuCommandList& cmd_list, uint32_t mip, D3D12_RESOURCE_STATES target_state);
        void Transition(GpuCommandList& cmd_list, D3D12_RESOURCE_STATES target_state);

        void Upload(GpuSystem& gpu_system, GpuCommandList& cmd_list, uint32_t mip, void const* data);
        void Readback(GpuSystem& gpu_system, GpuCommandList& cmd_list, uint32_t mip, void* data) const;

    private:
        class Impl;
        ImplPtr<Impl> impl_;
    };
} // namespace GoldenSun
