#include "pch.hpp"

#include <GoldenSun/ErrorHandling.hpp>
#include <GoldenSun/Util.hpp>
#include <GoldenSun/Uuid.hpp>

#include <GoldenSun/GoldenSun.hpp>

#include "AccelerationStructure.hpp"

using namespace DirectX;
using namespace GoldenSun;

namespace GoldenSun
{
    AccelerationStructure::AccelerationStructure(
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags, bool allow_update, bool update_on_build) noexcept
        : build_flags_(build_flags | (allow_update ? D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE
                                                   : D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE)),
          update_on_build_(update_on_build), allow_update_(allow_update)
    {
    }

    AccelerationStructure::AccelerationStructure(AccelerationStructure&& other) noexcept
        : acceleration_structure_(std::move(other.acceleration_structure_)), build_flags_(std::move(other.build_flags_)),
          prebuild_info_(std::move(other.prebuild_info_)), update_on_build_(std::move(other.update_on_build_)),
          allow_update_(std::move(other.allow_update_)), is_built_(std::move(other.is_built_)), dirty_(std::move(other.dirty_))
    {
    }

    AccelerationStructure& AccelerationStructure::operator=(AccelerationStructure&& other) noexcept
    {
        if (this != &other)
        {
            acceleration_structure_ = std::move(other.acceleration_structure_);
            build_flags_ = std::move(other.build_flags_);
            prebuild_info_ = std::move(other.prebuild_info_);
            update_on_build_ = std::move(other.update_on_build_);
            allow_update_ = std::move(other.allow_update_);
            is_built_ = std::move(other.is_built_);
            dirty_ = std::move(other.dirty_);
        }
        return *this;
    }

    void AccelerationStructure::AddBarrier(ID3D12GraphicsCommandList4* cmd_list) noexcept
    {
        D3D12_RESOURCE_BARRIER barrier;
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.UAV.pResource = acceleration_structure_.Resource();
        cmd_list->ResourceBarrier(1, &barrier);
    }

    void AccelerationStructure::CreateResource(ID3D12Device5* device, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE type,
        D3D12_RAYTRACING_GEOMETRY_DESC const* descs, uint32_t num_descs, wchar_t const* name)
    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build_desc{};
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& inputs = build_desc.Inputs;
        inputs.Type = type;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.Flags = build_flags_;
        inputs.NumDescs = num_descs;
        inputs.pGeometryDescs = descs;

        device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild_info_);
        Verify(prebuild_info_.ResultDataMaxSizeInBytes > 0);

        acceleration_structure_ = GpuDefaultBuffer(device, prebuild_info_.ResultDataMaxSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, name);
    }


    BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(ID3D12Device5* device,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags, Mesh const& mesh, bool allow_update, bool update_on_build,
        wchar_t const* name)
        : AccelerationStructure(build_flags, allow_update, update_on_build), geometry_descs_(mesh.GeometryDescs())
    {
        this->CreateResource(device, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL, geometry_descs_.data(),
            static_cast<uint32_t>(geometry_descs_.size()), name);
    }

    BottomLevelAccelerationStructure::BottomLevelAccelerationStructure(BottomLevelAccelerationStructure&& other) noexcept
        : AccelerationStructure(std::move(other)), geometry_descs_(std::move(other.geometry_descs_)),
          cache_geometry_descs_(std::move(other.cache_geometry_descs_)), frame_index_(std::move(other.frame_index_)),
          instance_contribution_to_hit_group_index_(std::move(other.instance_contribution_to_hit_group_index_))
    {
    }

    BottomLevelAccelerationStructure& BottomLevelAccelerationStructure::operator=(BottomLevelAccelerationStructure&& other) noexcept
    {
        if (this != &other)
        {
            AccelerationStructure::operator=(std::move(other));

            geometry_descs_ = std::move(other.geometry_descs_);
            cache_geometry_descs_ = std::move(other.cache_geometry_descs_);
            frame_index_ = std::move(other.frame_index_);
            instance_contribution_to_hit_group_index_ = std::move(other.instance_contribution_to_hit_group_index_);
        }
        return *this;
    }

    void BottomLevelAccelerationStructure::UpdateGeometryDescsTransform(D3D12_GPU_VIRTUAL_ADDRESS base_geometry_transform_gpu_addr) noexcept
    {
        for (size_t i = 0; i < geometry_descs_.size(); ++i)
        {
            auto& geometry_desc = geometry_descs_[i];
            geometry_desc.Triangles.Transform3x4 = base_geometry_transform_gpu_addr + i * sizeof(XMFLOAT3X4);
        }
    }

    void BottomLevelAccelerationStructure::Build(
        ID3D12GraphicsCommandList4* cmd_list, ID3D12Resource* scratch, D3D12_GPU_VIRTUAL_ADDRESS base_geometry_transform_gpu_addr)
    {
        Verify(prebuild_info_.ScratchDataSizeInBytes <= scratch->GetDesc().Width);

        if (base_geometry_transform_gpu_addr > 0)
        {
            this->UpdateGeometryDescsTransform(base_geometry_transform_gpu_addr);
        }

        frame_index_ = (frame_index_ + 1) % FrameCount;
        cache_geometry_descs_[frame_index_] = geometry_descs_;

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottom_level_build_desc{};
        auto& bottom_level_inputs = bottom_level_build_desc.Inputs;
        {
            bottom_level_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
            bottom_level_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            bottom_level_inputs.Flags = build_flags_;
            if (is_built_ && allow_update_ && update_on_build_)
            {
                bottom_level_inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
                bottom_level_build_desc.SourceAccelerationStructureData = acceleration_structure_.Resource()->GetGPUVirtualAddress();
            }
            bottom_level_inputs.NumDescs = static_cast<uint32_t>(cache_geometry_descs_[frame_index_].size());
            bottom_level_inputs.pGeometryDescs = cache_geometry_descs_[frame_index_].data();

            bottom_level_build_desc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
            bottom_level_build_desc.DestAccelerationStructureData = acceleration_structure_.Resource()->GetGPUVirtualAddress();
        }

        cmd_list->BuildRaytracingAccelerationStructure(&bottom_level_build_desc, 0, nullptr);
        this->AddBarrier(cmd_list);

        dirty_ = false;
        is_built_ = true;
    }


    TopLevelAccelerationStructure::TopLevelAccelerationStructure(ID3D12Device5* device, uint32_t num_bottom_level_as_instance_descs,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags, bool allow_update, bool update_on_build, wchar_t const* name)
        : AccelerationStructure(build_flags, allow_update, update_on_build)
    {
        this->CreateResource(
            device, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL, nullptr, num_bottom_level_as_instance_descs, name);
    }

    TopLevelAccelerationStructure::TopLevelAccelerationStructure(TopLevelAccelerationStructure&& other) noexcept = default;
    TopLevelAccelerationStructure& TopLevelAccelerationStructure::operator=(TopLevelAccelerationStructure&& other) noexcept = default;

    void TopLevelAccelerationStructure::Build(ID3D12GraphicsCommandList4* cmd_list, uint32_t num_bottom_level_as_instance_descs,
        D3D12_GPU_VIRTUAL_ADDRESS bottom_level_as_instance_descs, ID3D12Resource* scratch, [[maybe_unused]] bool update)
    {
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC top_level_build_desc{};
        auto& top_level_inputs = top_level_build_desc.Inputs;
        {
            top_level_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
            top_level_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
            top_level_inputs.Flags = build_flags_;
            if (is_built_ && allow_update_ && update_on_build_)
            {
                top_level_inputs.Flags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
            }
            top_level_inputs.NumDescs = num_bottom_level_as_instance_descs;

            top_level_build_desc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
            top_level_build_desc.DestAccelerationStructureData = acceleration_structure_.Resource()->GetGPUVirtualAddress();
        }
        top_level_inputs.InstanceDescs = bottom_level_as_instance_descs;

        cmd_list->BuildRaytracingAccelerationStructure(&top_level_build_desc, 0, nullptr);
        this->AddBarrier(cmd_list);

        dirty_ = false;
        is_built_ = true;
    }

    RaytracingAccelerationStructureManager::RaytracingAccelerationStructureManager(
        ID3D12Device5* device, uint32_t max_num_bottom_level_instances, uint32_t frame_count)
        : bottom_level_as_instance_descs_(device, max_num_bottom_level_instances, frame_count, L"Bottom-Level AS Instance descs")
    {
    }

    uint32_t RaytracingAccelerationStructureManager::AddBottomLevelAS(ID3D12Device5* device,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags, Mesh const& mesh, bool allow_update,
        [[maybe_unused]] bool perform_update_on_build)
    {
        uint32_t const as_id = static_cast<uint32_t>(bottom_level_as_.size());
        auto const& bottom_level_as =
            bottom_level_as_.emplace_back(BottomLevelAccelerationStructure(device, build_flags, mesh, allow_update));
        scratch_buffer_size_ = std::max(scratch_buffer_size_, bottom_level_as.RequiredScratchSize());
        return as_id;
    }

    uint32_t RaytracingAccelerationStructureManager::AddBottomLevelASInstance(
        uint32_t index, uint32_t instance_contribution_to_hit_group_index, XMMATRIX transform, uint8_t instance_mask)
    {
        Verify(num_bottom_level_as_instances_ < bottom_level_as_instance_descs_.NumElements());

        uint32_t const instance_index = num_bottom_level_as_instances_;
        ++num_bottom_level_as_instances_;

        auto& bottom_level_as = bottom_level_as_[index];

        auto& instance_desc = bottom_level_as_instance_descs_[instance_index];
        instance_desc.InstanceMask = instance_mask;
        instance_desc.InstanceContributionToHitGroupIndex = instance_contribution_to_hit_group_index != 0xFFFFFFFFU
                                                                ? instance_contribution_to_hit_group_index
                                                                : bottom_level_as.InstanceContributionToHitGroupIndex();
        instance_desc.AccelerationStructure = bottom_level_as.Resource()->GetGPUVirtualAddress();
        XMStoreFloat3x4(reinterpret_cast<XMFLOAT3X4*>(instance_desc.Transform), transform);

        return instance_index;
    };

    uint32_t RaytracingAccelerationStructureManager::MaxInstanceContributionToHitGroupIndex() const noexcept
    {
        uint32_t max_instance_contribution_to_hit_group_index = 0;
        for (uint32_t i = 0; i < num_bottom_level_as_instances_; i++)
        {
            auto const& instance_desc = bottom_level_as_instance_descs_[i];
            max_instance_contribution_to_hit_group_index =
                std::max(max_instance_contribution_to_hit_group_index, instance_desc.InstanceContributionToHitGroupIndex);
        }
        return max_instance_contribution_to_hit_group_index;
    };

    void RaytracingAccelerationStructureManager::ResetTopLevelAS(ID3D12Device5* device,
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS build_flags, bool allow_update, bool perform_update_on_build,
        wchar_t const* resource_name)
    {
        top_level_as_ = std::make_unique<TopLevelAccelerationStructure>(
            device, this->NumBottomLevelASInstances(), build_flags, allow_update, perform_update_on_build, resource_name);

        scratch_buffer_size_ = std::max(scratch_buffer_size_, top_level_as_->RequiredScratchSize());
        scratch_buffer_ = GpuDefaultBuffer(device, scratch_buffer_size_, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchBuffer");
    }

    void RaytracingAccelerationStructureManager::Build(ID3D12GraphicsCommandList4* cmd_list, uint32_t frame_index, bool force_build)
    {
        bottom_level_as_instance_descs_.UploadToGpu(frame_index);

        {
            for (auto& bottom_level_as : bottom_level_as_)
            {
                if (force_build || bottom_level_as.Dirty())
                {
                    D3D12_GPU_VIRTUAL_ADDRESS base_geometry_transform_gpu_addr{};
                    bottom_level_as.Build(cmd_list, scratch_buffer_.Resource(), base_geometry_transform_gpu_addr);
                }
            }
        }

        {
            D3D12_GPU_VIRTUAL_ADDRESS instance_descs = bottom_level_as_instance_descs_.GpuVirtualAddress(frame_index);
            top_level_as_->Build(cmd_list, this->NumBottomLevelASInstances(), instance_descs, scratch_buffer_.Resource());
        }
    }
} // namespace GoldenSun
