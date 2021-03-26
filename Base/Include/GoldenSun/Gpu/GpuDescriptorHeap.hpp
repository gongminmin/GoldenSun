#pragma once

#include <GoldenSun/ImplPtr.hpp>

#include <string_view>
#include <tuple>

namespace GoldenSun
{
    class GpuSystemInternalD3D12;

    D3D12_CPU_DESCRIPTOR_HANDLE OffsetHandle(D3D12_CPU_DESCRIPTOR_HANDLE const& handle, int32_t offset, uint32_t desc_size);
    D3D12_GPU_DESCRIPTOR_HANDLE OffsetHandle(D3D12_GPU_DESCRIPTOR_HANDLE const& handle, int32_t offset, uint32_t desc_size);
    std::tuple<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> OffsetHandle(
        D3D12_CPU_DESCRIPTOR_HANDLE const& cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE const& gpu_handle, int32_t offset, uint32_t desc_size);

    class GpuDescriptorHeap final
    {
        friend class GpuSystemInternalD3D12;

        DISALLOW_COPY_AND_ASSIGN(GpuDescriptorHeap)

    public:
        GpuDescriptorHeap();
        ~GpuDescriptorHeap() noexcept;

        GpuDescriptorHeap(GpuDescriptorHeap&& other) noexcept;
        GpuDescriptorHeap& operator=(GpuDescriptorHeap&& other) noexcept;

        explicit operator bool() const noexcept;

        // TODO: Remove it after finishing the GPU system
        void* NativeDescriptorHeap() const noexcept;

        D3D12_CPU_DESCRIPTOR_HANDLE CpuHandleStart() const noexcept;
        D3D12_GPU_DESCRIPTOR_HANDLE GpuHandleStart() const noexcept;

        uint32_t Size() const noexcept;

        void Reset() noexcept;

    protected:
        class Impl;
        ImplPtr<Impl> impl_;
    };
} // namespace GoldenSun
