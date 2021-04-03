#include "../pch.hpp"

#include <GoldenSun/Gpu/GpuSystem.hpp>
#include <GoldenSun/Util.hpp>

#include "../ImplPtrImpl.hpp"
#include "GpuSystemInternal.hpp"

namespace GoldenSun
{
    class GpuTexture2D::Impl final
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        Impl(ID3D12Device5* device, uint32_t width, uint32_t height, uint32_t mip_levels, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
            D3D12_RESOURCE_STATES init_state, std::wstring_view name)
            : curr_states_(mip_levels, init_state)
        {
            D3D12_HEAP_PROPERTIES const default_heap_prop = {
                D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};

            desc_ = {D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, static_cast<uint64_t>(width), height, 1, static_cast<uint16_t>(mip_levels),
                format, {1, 0}, D3D12_TEXTURE_LAYOUT_UNKNOWN, flags};
            TIFHR(device->CreateCommittedResource(
                &default_heap_prop, D3D12_HEAP_FLAG_NONE, &desc_, init_state, nullptr, UuidOf<ID3D12Resource>(), resource_.PutVoid()));
            if (!name.empty())
            {
                resource_->SetName(std::wstring(name).c_str());
            }
        }

        Impl(void* resource, D3D12_RESOURCE_STATES curr_state, std::wstring_view name)
            : resource_(reinterpret_cast<ID3D12Resource*>(resource))
        {
            if (resource_)
            {
                desc_ = resource_->GetDesc();
                if (!name.empty())
                {
                    resource_->SetName(std::wstring(name).c_str());
                }

                curr_states_.assign(desc_.MipLevels, curr_state);
            }
        }

        void Share(Impl& target) const
        {
            target.resource_ = resource_;
            target.desc_ = desc_;
            target.curr_states_ = curr_states_;
        }

        explicit operator bool() const noexcept
        {
            return resource_ ? true : false;
        }

        ID3D12Resource* Resource() const noexcept
        {
            return resource_.Get();
        }

        uint32_t Width(uint32_t mip) const noexcept
        {
            return std::max(static_cast<uint32_t>(desc_.Width >> mip), 1U);
        }

        uint32_t Height(uint32_t mip) const noexcept
        {
            return std::max(desc_.Height >> mip, 1U);
        }

        uint32_t MipLevels() const noexcept
        {
            return desc_.MipLevels;
        }

        DXGI_FORMAT Format() const noexcept
        {
            return desc_.Format;
        }

        D3D12_RESOURCE_FLAGS Flags() const noexcept
        {
            return desc_.Flags;
        }

        void Reset() noexcept
        {
            resource_.Reset();
            desc_ = {};
            curr_states_.clear();
        }

        D3D12_RESOURCE_STATES State(uint32_t mip) const noexcept
        {
            return curr_states_[mip];
        }

        void Transition(GpuCommandList& cmd_list, uint32_t mip, D3D12_RESOURCE_STATES target_state) const
        {
            if (curr_states_[mip] != target_state)
            {
                auto* d3d12_cmd_list = cmd_list.NativeHandle<D3D12Traits>();

                D3D12_RESOURCE_BARRIER barrier;
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource = resource_.Get();
                barrier.Transition.StateBefore = curr_states_[mip];
                barrier.Transition.StateAfter = target_state;
                barrier.Transition.Subresource = mip;
                d3d12_cmd_list->ResourceBarrier(1, &barrier);

                curr_states_[mip] = target_state;
            }
        }

        void Transition(GpuCommandList& cmd_list, D3D12_RESOURCE_STATES target_state) const
        {
            auto* d3d12_cmd_list = cmd_list.NativeHandle<D3D12Traits>();

            if ((curr_states_[0] == target_state) && ((target_state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) ||
                                                         (target_state == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE)))
            {
                D3D12_RESOURCE_BARRIER barrier;
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.UAV.pResource = resource_.Get();
                d3d12_cmd_list->ResourceBarrier(1, &barrier);
            }
            else
            {
                bool same_state = true;
                for (size_t i = 1; i < curr_states_.size(); ++i)
                {
                    if (curr_states_[i] != curr_states_[0])
                    {
                        same_state = false;
                        break;
                    }
                }

                if (same_state)
                {
                    if (curr_states_[0] != target_state)
                    {
                        D3D12_RESOURCE_BARRIER barrier;
                        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                        barrier.Transition.pResource = resource_.Get();
                        barrier.Transition.StateBefore = curr_states_[0];
                        barrier.Transition.StateAfter = target_state;
                        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                        d3d12_cmd_list->ResourceBarrier(1, &barrier);
                    }
                }
                else
                {
                    std::vector<D3D12_RESOURCE_BARRIER> barriers;
                    for (size_t i = 0; i < curr_states_.size(); ++i)
                    {
                        if (curr_states_[i] != target_state)
                        {
                            auto& barrier = barriers.emplace_back();
                            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                            barrier.Transition.pResource = resource_.Get();
                            barrier.Transition.StateBefore = curr_states_[i];
                            barrier.Transition.StateAfter = target_state;
                            barrier.Transition.Subresource = static_cast<uint32_t>(i);
                        }
                    }
                    d3d12_cmd_list->ResourceBarrier(static_cast<uint32_t>(barriers.size()), barriers.data());
                }
            }

            curr_states_.assign(desc_.MipLevels, target_state);
        }

        void Upload(GpuSystem& gpu_system, GpuCommandList& cmd_list, uint32_t mip, void const* data)
        {
            uint32_t const width = this->Width(mip);
            uint32_t const height = this->Height(mip);
            uint32_t const format_size = FormatSize(this->Format());

            auto* d3d12_device = gpu_system.NativeDeviceHandle<D3D12Traits>();

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
            uint32_t num_row = 0;
            uint64_t row_size_in_bytes = 0;
            uint64_t required_size = 0;
            d3d12_device->GetCopyableFootprints(&desc_, mip, 1, 0, &layout, &num_row, &row_size_in_bytes, &required_size);

            auto upload_mem_block = gpu_system.AllocUploadMemBlock(static_cast<uint32_t>(required_size));

            assert(row_size_in_bytes >= width * format_size);

            uint8_t* tex_data = upload_mem_block.CpuAddress<uint8_t>();
            for (uint32_t y = 0; y < height; ++y)
            {
                memcpy(tex_data + y * row_size_in_bytes, static_cast<uint8_t const*>(data) + y * width * format_size, width * format_size);
            }

            layout.Offset += upload_mem_block.Offset();
            D3D12_TEXTURE_COPY_LOCATION src;
            src.pResource = upload_mem_block.NativeBufferHandle<D3D12Traits>();
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint = layout;

            D3D12_TEXTURE_COPY_LOCATION dst;
            dst.pResource = resource_.Get();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = mip;

            D3D12_BOX src_box;
            src_box.left = 0;
            src_box.top = 0;
            src_box.front = 0;
            src_box.right = width;
            src_box.bottom = height;
            src_box.back = 1;

            auto* d3d12_cmd_list = cmd_list.NativeHandle<D3D12Traits>();

            auto src_old_state = this->State(0);
            this->Transition(cmd_list, D3D12_RESOURCE_STATE_COPY_DEST);

            d3d12_cmd_list->CopyTextureRegion(&dst, 0, 0, 0, &src, &src_box);

            this->Transition(cmd_list, src_old_state);

            gpu_system.DeallocUploadMemBlock(std::move(upload_mem_block));
        }

        void Readback(GpuSystem& gpu_system, GpuCommandList& cmd_list, uint32_t mip, void* data) const
        {
            uint32_t const width = this->Width(mip);
            uint32_t const height = this->Height(mip);
            uint32_t const format_size = FormatSize(this->Format());

            auto* d3d12_device = gpu_system.NativeDeviceHandle<D3D12Traits>();

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
            uint32_t num_row = 0;
            uint64_t row_size_in_bytes = 0;
            uint64_t required_size = 0;
            d3d12_device->GetCopyableFootprints(&desc_, mip, 1, 0, &layout, &num_row, &row_size_in_bytes, &required_size);

            auto readback_mem_block = gpu_system.AllocReadbackMemBlock(static_cast<uint32_t>(required_size));

            D3D12_TEXTURE_COPY_LOCATION src;
            src.pResource = resource_.Get();
            src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src.SubresourceIndex = mip;

            layout.Offset = readback_mem_block.Offset();
            D3D12_TEXTURE_COPY_LOCATION dst;
            dst.pResource = readback_mem_block.NativeBufferHandle<D3D12Traits>();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst.PlacedFootprint = layout;

            D3D12_BOX src_box;
            src_box.left = 0;
            src_box.top = 0;
            src_box.front = 0;
            src_box.right = width;
            src_box.bottom = height;
            src_box.back = 1;

            auto* d3d12_cmd_list = cmd_list.NativeHandle<D3D12Traits>();

            auto src_old_state = this->State(0);
            this->Transition(cmd_list, D3D12_RESOURCE_STATE_GENERIC_READ);

            d3d12_cmd_list->CopyTextureRegion(&dst, 0, 0, 0, &src, &src_box);

            this->Transition(cmd_list, src_old_state);

            gpu_system.ExecuteAndReset(cmd_list);
            gpu_system.WaitForGpu();

            assert(row_size_in_bytes >= width * format_size);

            uint8_t* u8_data = reinterpret_cast<uint8_t*>(data);
            uint8_t const* tex_data = readback_mem_block.CpuAddress<uint8_t>();
            for (uint32_t y = 0; y < height; ++y)
            {
                memcpy(&u8_data[y * width * format_size], tex_data + y * row_size_in_bytes, width * format_size);
            }

            gpu_system.DeallocReadbackMemBlock(std::move(readback_mem_block));
        }

    private:
        ComPtr<ID3D12Resource> resource_;
        D3D12_RESOURCE_DESC desc_{};
        mutable std::vector<D3D12_RESOURCE_STATES> curr_states_;
    };


    GpuTexture2D::GpuTexture2D() = default;
    GpuTexture2D::~GpuTexture2D() noexcept = default;
    GpuTexture2D::GpuTexture2D(GpuTexture2D&& other) noexcept = default;
    GpuTexture2D& GpuTexture2D::operator=(GpuTexture2D&& other) noexcept = default;

    GpuTexture2D::GpuTexture2D(void* native_resource, D3D12_RESOURCE_STATES curr_state, std::wstring_view name) noexcept
        : impl_(native_resource, curr_state, std::move(name))
    {
    }

    GpuTexture2D GpuTexture2D::Share() const
    {
        GpuTexture2D texture;
        impl_->Share(*texture.impl_);
        return texture;
    }

    GpuTexture2D::operator bool() const noexcept
    {
        return impl_ && impl_->operator bool();
    }

    void* GpuTexture2D::NativeHandle() const noexcept
    {
        return impl_->Resource();
    }

    uint32_t GpuTexture2D::Width(uint32_t mip) const noexcept
    {
        return impl_->Width(mip);
    }

    uint32_t GpuTexture2D::Height(uint32_t mip) const noexcept
    {
        return impl_->Height(mip);
    }

    uint32_t GpuTexture2D::MipLevels() const noexcept
    {
        return impl_->MipLevels();
    }

    DXGI_FORMAT GpuTexture2D::Format() const noexcept
    {
        return impl_->Format();
    }

    D3D12_RESOURCE_FLAGS GpuTexture2D::Flags() const noexcept
    {
        return impl_->Flags();
    }

    void GpuTexture2D::Reset() noexcept
    {
        impl_->Reset();
    }

    D3D12_RESOURCE_STATES GpuTexture2D::State(uint32_t mip) const noexcept
    {
        return impl_->State(mip);
    }

    void GpuTexture2D::Transition(GpuCommandList& cmd_list, uint32_t mip, D3D12_RESOURCE_STATES target_state)
    {
        impl_->Transition(cmd_list, mip, target_state);
    }

    void GpuTexture2D::Transition(GpuCommandList& cmd_list, D3D12_RESOURCE_STATES target_state)
    {
        impl_->Transition(cmd_list, target_state);
    }

    void GpuTexture2D::Upload(GpuSystem& gpu_system, GpuCommandList& cmd_list, uint32_t mip, void const* data)
    {
        impl_->Upload(gpu_system, cmd_list, mip, data);
    }

    void GpuTexture2D::Readback(GpuSystem& gpu_system, GpuCommandList& cmd_list, uint32_t mip, void* data) const
    {
        impl_->Readback(gpu_system, cmd_list, mip, data);
    }


    GpuTexture2D GpuSystemInternalD3D12::CreateTexture2D(ID3D12Device5* device, uint32_t width, uint32_t height, uint32_t mip_levels,
        DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, std::wstring_view name)
    {
        GpuTexture2D texture;
        texture.impl_ = ImplPtr<GpuTexture2D::Impl>(device, width, height, mip_levels, format, flags, init_state, name);
        return texture;
    }
} // namespace GoldenSun
