#pragma once

#include <GoldenSun/ComPtr.hpp>
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
        DISALLOW_COPY_AND_ASSIGN(AccelerationStructure);

    public:
        virtual ~AccelerationStructure() noexcept = default;

        AccelerationStructure(AccelerationStructure&& other) noexcept;
        AccelerationStructure& operator=(AccelerationStructure&& other) noexcept;

        void AddBarrier(ID3D12GraphicsCommandList4* cmd_list) noexcept;

        uint64_t RequiredScratchSize() const noexcept
        {
            return std::max(prebuild_info_.ScratchDataSizeInBytes, prebuild_info_.UpdateScratchDataSizeInBytes);
        }
        uint64_t RequiredResultDataSizeInBytes() const noexcept
        {
            return prebuild_info_.ResultDataMaxSizeInBytes;
        }

        ID3D12Resource* Resource() const noexcept
        {
            return acceleration_structure_.Resource();
        }
        uint64_t ResourceSize() const noexcept
        {
            return this->Resource()->GetDesc().Width;
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
        AccelerationStructure(
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags, bool allow_update, bool update_on_build) noexcept;

        void CreateResource(ID3D12Device5* device, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE type,
            D3D12_RAYTRACING_GEOMETRY_DESC const* descs, uint32_t num_descs, wchar_t const* name);

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
    public:
        BottomLevelAccelerationStructure(ID3D12Device5* device, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags,
            Mesh const& mesh, bool allow_update = false, bool update_on_build = false, wchar_t const* name = nullptr);
        ~BottomLevelAccelerationStructure() override = default;

        BottomLevelAccelerationStructure(BottomLevelAccelerationStructure&& other) noexcept;
        BottomLevelAccelerationStructure& operator=(BottomLevelAccelerationStructure&& other) noexcept;

        void Build(
            ID3D12GraphicsCommandList4* cmd_list, ID3D12Resource* scratch, D3D12_GPU_VIRTUAL_ADDRESS base_geometry_transform_gpu_addr = 0);

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
        std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometry_descs_;
        std::array<std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>, FrameCount> cache_geometry_descs_;
        uint32_t frame_index_ = 0;

        uint32_t instance_contribution_to_hit_group_index_ = 0;
    };

    class TopLevelAccelerationStructure : public AccelerationStructure
    {
    public:
        TopLevelAccelerationStructure(ID3D12Device5* device, uint32_t num_bottom_level_as_instance_descs,
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags, bool allow_update = false, bool update_on_build = false,
            wchar_t const* name = nullptr);
        ~TopLevelAccelerationStructure() override = default;

        TopLevelAccelerationStructure(TopLevelAccelerationStructure&& other) noexcept;
        TopLevelAccelerationStructure& operator=(TopLevelAccelerationStructure&& other) noexcept;

        void Build(ID3D12GraphicsCommandList4* cmd_list, uint32_t num_instance_descs, D3D12_GPU_VIRTUAL_ADDRESS instance_descs,
            ID3D12Resource* scratch, bool update = false);
    };

    class RaytracingAccelerationStructureManager
    {
    public:
        RaytracingAccelerationStructureManager(ID3D12Device5* device, uint32_t max_num_bottom_level_instances, uint32_t frame_count);

        uint32_t AddBottomLevelAS(ID3D12Device5* device, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags, Mesh const& mesh,
            bool allow_update = false, bool perform_update_on_build = false);
        uint32_t AddBottomLevelASInstance(uint32_t index, uint32_t instance_contribution_to_hit_group_index = 0xFFFFFFFFU,
            DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity(), uint8_t instance_mask = 1);

        void ResetTopLevelAS(ID3D12Device5* device, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags,
            bool allow_update = false, bool perform_update_on_build = false, wchar_t const* resource_name = nullptr);

        void Build(ID3D12GraphicsCommandList4* cmd_list, uint32_t frame_index, bool force_build = false);

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

        ID3D12Resource* TopLevelASResource() const noexcept
        {
            return top_level_as_->Resource();
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

        std::unique_ptr<TopLevelAccelerationStructure> top_level_as_;

        GpuDefaultBuffer scratch_buffer_;
        uint64_t scratch_buffer_size_ = 0;
    };
} // namespace GoldenSun
