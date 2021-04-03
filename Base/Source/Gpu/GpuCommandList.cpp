#include "../pch.hpp"

#include <GoldenSun/Gpu/GpuSystem.hpp>

#include "../ImplPtrImpl.hpp"
#include "GpuSystemInternal.hpp"

namespace GoldenSun
{
    class GpuCommandList::Impl final
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        Impl(ID3D12Device5* device, ID3D12CommandAllocator* cmd_allocator)
        {
            TIFHR(device->CreateCommandList(
                0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_allocator, nullptr, UuidOf<ID3D12GraphicsCommandList4>(), cmd_list_.PutVoid()));
        }

        explicit Impl(ID3D12GraphicsCommandList4* cmd_list) : cmd_list_(cmd_list)
        {
        }

        explicit operator bool() const noexcept
        {
            return cmd_list_ ? true : false;
        }

        ID3D12GraphicsCommandList4* CommandList() const noexcept
        {
            return cmd_list_.Get();
        }

        void Copy(ID3D12Resource* dest, ID3D12Resource* src)
        {
            cmd_list_->CopyResource(dest, src);
        }

        void BuildRaytracingAccelerationStructure(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC const& desc,
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC const* postbuild_info_descs, uint32_t num_postbuild_info_descs)
        {
            cmd_list_->BuildRaytracingAccelerationStructure(&desc, num_postbuild_info_descs, postbuild_info_descs);
        }

        void Close()
        {
            TIFHR(cmd_list_->Close());
        }

        void Reset(ID3D12CommandAllocator* cmd_allocator)
        {
            TIFHR(cmd_list_->Reset(cmd_allocator, nullptr));
        }

    private:
        ComPtr<ID3D12GraphicsCommandList4> cmd_list_;
    };


    GpuCommandList::GpuCommandList() = default;
    GpuCommandList::~GpuCommandList() noexcept = default;
    GpuCommandList::GpuCommandList(GpuCommandList&& other) noexcept = default;
    GpuCommandList& GpuCommandList::operator=(GpuCommandList&& other) noexcept = default;

    GpuCommandList::GpuCommandList(void* native_cmd_list) : impl_(reinterpret_cast<ID3D12GraphicsCommandList4*>(native_cmd_list))
    {
    }

    GpuCommandList::operator bool() const noexcept
    {
        return impl_ && impl_->operator bool();
    }

    void* GpuCommandList::NativeHandle() const noexcept
    {
        return impl_->CommandList();
    }

    void GpuCommandList::Copy(GpuBuffer const& dest, GpuBuffer const& src)
    {
        impl_->Copy(dest.NativeHandle<D3D12Traits>(), src.NativeHandle<D3D12Traits>());
    }

    void GpuCommandList::Copy(GpuTexture2D const& dest, GpuTexture2D const& src)
    {
        impl_->Copy(dest.NativeHandle<D3D12Traits>(), src.NativeHandle<D3D12Traits>());
    }

    void GpuCommandList::BuildRaytracingAccelerationStructure(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC const& desc,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC const* postbuild_info_descs, uint32_t num_postbuild_info_descs)
    {
        impl_->BuildRaytracingAccelerationStructure(desc, postbuild_info_descs, num_postbuild_info_descs);
    }

    void GpuCommandList::BuildRaytracingAccelerationStructure(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC const& desc)
    {
        this->BuildRaytracingAccelerationStructure(desc, nullptr, 0);
    }

    void GpuCommandList::Close()
    {
        impl_->Close();
    }

    GpuCommandList GpuSystemInternalD3D12::CreateCommandList(ID3D12Device5* device, ID3D12CommandAllocator* cmd_allocator)
    {
        GpuCommandList cmd_list;
        cmd_list.impl_ = ImplPtr<GpuCommandList::Impl>(device, cmd_allocator);
        return cmd_list;
    }

    void GpuSystemInternalD3D12::ResetCommandList(GpuCommandList& cmd_list, ID3D12CommandAllocator* cmd_allocator)
    {
        cmd_list.impl_->Reset(cmd_allocator);
    }
} // namespace GoldenSun
