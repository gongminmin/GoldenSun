#pragma once

#include <GoldenSun/ImplPtr.hpp>

namespace GoldenSun
{
    class GpuTexture2D;
    class GpuSystemInternalD3D12;

    class GpuCommandList final
    {
        friend class GpuSystemInternalD3D12;

        DISALLOW_COPY_AND_ASSIGN(GpuCommandList)

    public:
        GpuCommandList();
        explicit GpuCommandList(void* native_cmd_list);
        ~GpuCommandList() noexcept;

        GpuCommandList(GpuCommandList&& other) noexcept;
        GpuCommandList& operator=(GpuCommandList&& other) noexcept;

        void* NativeHandle() const noexcept;
        template <typename ApiTraits>
        typename ApiTraits::CommandListType NativeHandle() const noexcept
        {
            return reinterpret_cast<typename ApiTraits::CommandListType>(this->NativeHandle());
        }

        explicit operator bool() const noexcept;

        void Copy(GpuBuffer const& dest, GpuBuffer const& src);
        void Copy(GpuTexture2D const& dest, GpuTexture2D const& src);

        void BuildRaytracingAccelerationStructure(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC const& desc,
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC const* postbuild_info_descs, uint32_t num_postbuild_info_descs);
        void BuildRaytracingAccelerationStructure(D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC const& desc);

        void Close();

    private:
        class Impl;
        ImplPtr<Impl> impl_;
    };
} // namespace GoldenSun
