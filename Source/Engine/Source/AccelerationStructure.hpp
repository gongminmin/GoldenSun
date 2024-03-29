#pragma once

#include <GoldenSun/ComPtr.hpp>
#include <GoldenSun/Gpu/GpuSystem.hpp>
#include <GoldenSun/Util.hpp>

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace GoldenSun
{
    class Mesh;

    class AccelerationStructure
    {
        DISALLOW_COPY_AND_ASSIGN(AccelerationStructure)

    public:
        virtual ~AccelerationStructure() noexcept;

        AccelerationStructure(AccelerationStructure&& other) noexcept;
        AccelerationStructure& operator=(AccelerationStructure&& other) noexcept;

        void AddBarrier(GpuCommandList& cmd_list) noexcept;

        uint32_t RequiredScratchSize() const noexcept
        {
            return static_cast<uint32_t>(std::max(prebuild_info_.ScratchDataSizeInBytes, prebuild_info_.UpdateScratchDataSizeInBytes));
        }
        uint32_t RequiredResultDataSizeInBytes() const noexcept
        {
            return static_cast<uint32_t>(prebuild_info_.ResultDataMaxSizeInBytes);
        }

        GpuDefaultBuffer const& Buffer() const noexcept
        {
            return acceleration_structure_;
        }

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO const& PrebuildInfo() const noexcept
        {
            return prebuild_info_;
        }

        void Dirty(bool dirty)
        {
            dirty_ = dirty;
        }
        bool Dirty() const noexcept
        {
            return dirty_;
        }

    protected:
        AccelerationStructure() noexcept;
        AccelerationStructure(
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags, bool allow_update, bool update_on_build) noexcept;

        void CreateResource(GpuSystem& gpu_system, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE type,
            D3D12_RAYTRACING_GEOMETRY_DESC const* descs, uint32_t num_descs, std::wstring_view name);

    protected:
        GpuDefaultBuffer acceleration_structure_;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags_;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild_info_{};

        bool update_on_build_;
        bool allow_update_;

        bool is_built_ = false; // whether the AS has been built at least once.
        bool dirty_ = true;     // whether the AS has been modified and needs to be rebuilt.
    };

    class BottomLevelAccelerationStructure : public AccelerationStructure
    {
        DISALLOW_COPY_AND_ASSIGN(BottomLevelAccelerationStructure)

    public:
        BottomLevelAccelerationStructure(GpuSystem& gpu_system, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags,
            Mesh const& mesh, bool allow_update = false, bool update_on_build = false, std::wstring_view name = L"");
        ~BottomLevelAccelerationStructure() noexcept override;

        BottomLevelAccelerationStructure(BottomLevelAccelerationStructure&& other) noexcept;
        BottomLevelAccelerationStructure& operator=(BottomLevelAccelerationStructure&& other) noexcept;

        void Build(GpuCommandList& cmd_list, GpuBuffer const& scratch, D3D12_GPU_VIRTUAL_ADDRESS base_geometry_transform_gpu_addr = 0);

        void UpdateGeometryDescsTransform(D3D12_GPU_VIRTUAL_ADDRESS base_geometry_transform_gpu_addr) noexcept;

        uint32_t InstanceContributionToHitGroupIndex() const noexcept
        {
            return instance_contribution_to_hit_group_index_;
        }
        void InstanceContributionToHitGroupIndex(uint32_t index) noexcept
        {
            instance_contribution_to_hit_group_index_ = index;
        }

        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> const& GeometryDescs() const noexcept
        {
            return geometry_descs_;
        }

    private:
        GpuSystem& gpu_system_;

        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry_descs_;
        std::array<std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>, GpuSystem::FrameCount()> cache_geometry_descs_;

        uint32_t instance_contribution_to_hit_group_index_ = 0;
    };

    class TopLevelAccelerationStructure : public AccelerationStructure
    {
        DISALLOW_COPY_AND_ASSIGN(TopLevelAccelerationStructure)

    public:
        TopLevelAccelerationStructure() noexcept;
        TopLevelAccelerationStructure(GpuSystem& gpu_system, uint32_t num_bottom_level_as_instance_descs,
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags, bool allow_update = false, bool update_on_build = false,
            std::wstring_view name = L"");
        ~TopLevelAccelerationStructure() noexcept override;

        TopLevelAccelerationStructure(TopLevelAccelerationStructure&& other) noexcept;
        TopLevelAccelerationStructure& operator=(TopLevelAccelerationStructure&& other) noexcept;

        void Build(GpuCommandList& cmd_list, uint32_t num_instance_descs, D3D12_GPU_VIRTUAL_ADDRESS instance_descs,
            GpuBuffer const& scratch, bool update = false);
    };

    class RaytracingAccelerationStructureManager
    {
        DISALLOW_COPY_AND_ASSIGN(RaytracingAccelerationStructureManager);

    public:
        RaytracingAccelerationStructureManager(GpuSystem& gpu_system, uint32_t max_num_bottom_level_instances);

        RaytracingAccelerationStructureManager(RaytracingAccelerationStructureManager&& other) noexcept;
        RaytracingAccelerationStructureManager& operator=(RaytracingAccelerationStructureManager&& other) noexcept;

        uint32_t AddBottomLevelAS(GpuSystem& gpu_system, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags, Mesh const& mesh,
            uint32_t instance_contribution_to_hit_group_index, bool allow_update = false, bool perform_update_on_build = false);
        uint32_t AddBottomLevelASInstance(
            uint32_t index, DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity(), uint8_t instance_mask = 1);

        void AssignTopLevelAS(GpuSystem& gpu_system, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags,
            bool allow_update = false, bool perform_update_on_build = false, std::wstring_view resource_name = L"");

        void Build(GpuCommandList& cmd_list, uint32_t frame_index, bool force_build = false);

        uint32_t NumBottomLevelsASs() const noexcept
        {
            return static_cast<uint32_t>(bottom_level_as_.size());
        }

        BottomLevelAccelerationStructure& BottomLevelAS(uint32_t index) noexcept
        {
            return bottom_level_as_[index];
        }
        BottomLevelAccelerationStructure const& BottomLevelAS(uint32_t index) const noexcept
        {
            return bottom_level_as_[index];
        }

        GpuDefaultBuffer const& TopLevelASBuffer() const noexcept
        {
            return top_level_as_.Buffer();
        }

        uint32_t NumBottomLevelASInstances() const noexcept
        {
            return static_cast<uint32_t>(bottom_level_as_instance_descs_.NumElements());
        }

        uint32_t MaxInstanceContributionToHitGroupIndex() const noexcept;

    private:
        std::vector<BottomLevelAccelerationStructure> bottom_level_as_;
        StructuredBuffer<D3D12_RAYTRACING_INSTANCE_DESC> bottom_level_as_instance_descs_;
        uint32_t num_bottom_level_as_instances_ = 0;

        TopLevelAccelerationStructure top_level_as_;

        GpuDefaultBuffer scratch_buffer_;
        uint32_t scratch_buffer_size_ = 0;
    };
} // namespace GoldenSun
