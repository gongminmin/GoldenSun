#include "../pch.hpp"

#include <GoldenSun/Gpu/GpuSystem.hpp>

#include "../ImplPtrImpl.hpp"
#include "GpuSystemInternal.hpp"

namespace GoldenSun
{
    D3D12_CPU_DESCRIPTOR_HANDLE OffsetHandle(D3D12_CPU_DESCRIPTOR_HANDLE const& handle, int32_t offset, uint32_t desc_size)
    {
        return {handle.ptr + offset * desc_size};
    }

    D3D12_GPU_DESCRIPTOR_HANDLE OffsetHandle(D3D12_GPU_DESCRIPTOR_HANDLE const& handle, int32_t offset, uint32_t desc_size)
    {
        return {handle.ptr + offset * desc_size};
    }

    std::tuple<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> OffsetHandle(
        D3D12_CPU_DESCRIPTOR_HANDLE const& cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE const& gpu_handle, int32_t offset, uint32_t desc_size)
    {
        int32_t const offset_in_bytes = offset * desc_size;
        return {{cpu_handle.ptr + offset_in_bytes}, {gpu_handle.ptr + offset_in_bytes}};
    }

    class GpuDescriptorHeap::Impl final
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        Impl(ID3D12Device5* device, uint32_t size, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags,
            std::wstring_view name)
        {
            desc_.Type = type;
            desc_.NumDescriptors = size;
            desc_.Flags = flags;
            desc_.NodeMask = 0;
            TIFHR(device->CreateDescriptorHeap(&desc_, UuidOf<ID3D12DescriptorHeap>(), heap_.PutVoid()));
            if (!name.empty())
            {
                heap_->SetName(std::wstring(std::move(name)).c_str());
            }
        }

        explicit operator bool() const noexcept
        {
            return heap_ ? true : false;
        }

        ID3D12DescriptorHeap* DescriptorHeap() const noexcept
        {
            return heap_.Get();
        }

        D3D12_CPU_DESCRIPTOR_HANDLE CpuHandleStart() const noexcept
        {
            return heap_->GetCPUDescriptorHandleForHeapStart();
        }

        D3D12_GPU_DESCRIPTOR_HANDLE GpuHandleStart() const noexcept
        {
            return heap_->GetGPUDescriptorHandleForHeapStart();
        }

        uint32_t Size() const noexcept
        {
            return static_cast<uint32_t>(desc_.NumDescriptors);
        }

        void Reset() noexcept
        {
            heap_.Reset();
            desc_ = {};
        }

    private:
        ComPtr<ID3D12DescriptorHeap> heap_;
        D3D12_DESCRIPTOR_HEAP_DESC desc_{};
    };


    GpuDescriptorHeap::GpuDescriptorHeap() = default;
    GpuDescriptorHeap::~GpuDescriptorHeap() noexcept = default;
    GpuDescriptorHeap::GpuDescriptorHeap(GpuDescriptorHeap&& other) noexcept = default;
    GpuDescriptorHeap& GpuDescriptorHeap::operator=(GpuDescriptorHeap&& other) noexcept = default;

    GpuDescriptorHeap::operator bool() const noexcept
    {
        return impl_ && impl_->operator bool();
    }

    void* GpuDescriptorHeap::NativeDescriptorHeap() const noexcept
    {
        return impl_->DescriptorHeap();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GpuDescriptorHeap::CpuHandleStart() const noexcept
    {
        return impl_->CpuHandleStart();
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GpuDescriptorHeap::GpuHandleStart() const noexcept
    {
        return impl_->GpuHandleStart();
    }

    uint32_t GpuDescriptorHeap::Size() const noexcept
    {
        return impl_->Size();
    }

    void GpuDescriptorHeap::Reset() noexcept
    {
        impl_->Reset();
    }


    GpuDescriptorHeap GpuSystemInternalD3D12::CreateDescriptorHeap(
        ID3D12Device5* device, uint32_t size, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, std::wstring_view name)
    {
        GpuDescriptorHeap heap;
        heap.impl_ = ImplPtr<GpuDescriptorHeap::Impl>(device, size, type, flags, name);
        return heap;
    }
} // namespace GoldenSun
