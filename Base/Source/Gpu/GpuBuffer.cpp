#include "../pch.hpp"

#include <GoldenSun/Gpu/GpuSystem.hpp>

#include "../ImplPtrImpl.hpp"
#include "GpuSystemInternal.hpp"

namespace GoldenSun
{
    class GpuBuffer::Impl
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        Impl(ID3D12Device5* device, uint32_t size, D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state,
            std::wstring_view name)
            : heap_type_(heap_type), curr_state_(init_state)
        {
            D3D12_HEAP_PROPERTIES const heap_prop = {heap_type, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};

            desc_ = {D3D12_RESOURCE_DIMENSION_BUFFER, 0, static_cast<uint64_t>(size), 1, 1, 1, DXGI_FORMAT_UNKNOWN, {1, 0},
                D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags};
            TIFHR(device->CreateCommittedResource(
                &heap_prop, D3D12_HEAP_FLAG_NONE, &desc_, init_state, nullptr, UuidOf<ID3D12Resource>(), resource_.PutVoid()));
            if (!name.empty())
            {
                resource_->SetName(std::wstring(std::move(name)).c_str());
            }
        }

        explicit Impl(void* resource, D3D12_RESOURCE_STATES curr_state, std::wstring_view name)
            : resource_(reinterpret_cast<ID3D12Resource*>(resource)), curr_state_(curr_state)
        {
            if (resource_)
            {
                D3D12_HEAP_PROPERTIES heap_prop;
                resource_->GetHeapProperties(&heap_prop, nullptr);
                heap_type_ = heap_prop.Type;

                desc_ = resource_->GetDesc();
                if (!name.empty())
                {
                    resource_->SetName(std::wstring(std::move(name)).c_str());
                }
            }
        }

        virtual ~Impl() = default;

        explicit operator bool() const noexcept
        {
            return resource_ ? true : false;
        }

        ID3D12Resource* Resource() const noexcept
        {
            return resource_.Get();
        }

        D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress() const noexcept
        {
            return resource_->GetGPUVirtualAddress();
        }

        uint32_t Size() const noexcept
        {
            return static_cast<uint32_t>(desc_.Width);
        }

        void* Map(D3D12_RANGE const& read_range)
        {
            void* addr;
            TIFHR(resource_->Map(0, &read_range, &addr));
            return addr;
        }

        void* Map()
        {
            void* addr;
            D3D12_RANGE const read_range{0, 0};
            TIFHR(resource_->Map(0, (heap_type_ == D3D12_HEAP_TYPE_READBACK) ? nullptr : &read_range, &addr));
            return addr;
        }

        void Unmap(D3D12_RANGE const& write_range)
        {
            resource_->Unmap(0, &write_range);
        }

        void Unmap()
        {
            D3D12_RANGE const write_range{0, 0};
            resource_->Unmap(0, (heap_type_ == D3D12_HEAP_TYPE_UPLOAD) ? nullptr : &write_range);
        }

        virtual void Reset() noexcept
        {
            resource_.Reset();
            desc_ = {};
            heap_type_ = {};
            curr_state_ = {};
        }

        D3D12_RESOURCE_STATES State() const noexcept
        {
            return curr_state_;
        }

        void Transition(GpuCommandList& cmd_list, D3D12_RESOURCE_STATES target_state) const
        {
            auto* d3d12_cmd_list = reinterpret_cast<ID3D12GraphicsCommandList4*>(cmd_list.NativeCommandList());

            D3D12_RESOURCE_BARRIER barrier;
            if (curr_state_ != target_state)
            {
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource = resource_.Get();
                barrier.Transition.StateBefore = curr_state_;
                barrier.Transition.StateAfter = target_state;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                d3d12_cmd_list->ResourceBarrier(1, &barrier);
            }
            else if ((target_state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS) ||
                     (target_state == D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE))
            {
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.UAV.pResource = resource_.Get();
                d3d12_cmd_list->ResourceBarrier(1, &barrier);
            }

            curr_state_ = target_state;
        }

    protected:
        ComPtr<ID3D12Resource> resource_;
        D3D12_RESOURCE_DESC desc_{};
        D3D12_HEAP_TYPE heap_type_{};
        mutable D3D12_RESOURCE_STATES curr_state_{};
    };


    GpuBuffer::GpuBuffer() = default;
    GpuBuffer::~GpuBuffer() noexcept = default;
    GpuBuffer::GpuBuffer(GpuBuffer&& other) noexcept = default;
    GpuBuffer& GpuBuffer::operator=(GpuBuffer&& other) noexcept = default;

    GpuBuffer::GpuBuffer(void* native_resource, D3D12_RESOURCE_STATES curr_state, std::wstring_view name)
        : impl_(native_resource, curr_state, std::move(name))
    {
    }

    GpuBuffer::operator bool() const noexcept
    {
        return impl_ && impl_->operator bool();
    }

    void* GpuBuffer::NativeResource() const noexcept
    {
        return impl_->Resource();
    }

    D3D12_GPU_VIRTUAL_ADDRESS GpuBuffer::GpuVirtualAddress() const noexcept
    {
        return impl_->GpuVirtualAddress();
    }

    uint32_t GpuBuffer::Size() const noexcept
    {
        return impl_->Size();
    }

    void* GpuBuffer::Map(D3D12_RANGE const& read_range)
    {
        return impl_->Map(read_range);
    }

    void* GpuBuffer::Map()
    {
        return impl_->Map();
    }

    void GpuBuffer::Unmap(D3D12_RANGE const& write_range)
    {
        impl_->Unmap(write_range);
    }

    void GpuBuffer::Unmap()
    {
        return impl_->Unmap();
    }

    void GpuBuffer::Reset() noexcept
    {
        impl_->Reset();
    }

    D3D12_RESOURCE_STATES GpuBuffer::State() const noexcept
    {
        return impl_->State();
    }

    void GpuBuffer::Transition(GpuCommandList& cmd_list, D3D12_RESOURCE_STATES target_state)
    {
        impl_->Transition(cmd_list, target_state);
    }


    class GpuDefaultBuffer::Impl final : public GpuBuffer::Impl
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        Impl(ID3D12Device5* device, uint32_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, std::wstring_view name)
            : GpuBuffer::Impl(device, size, D3D12_HEAP_TYPE_DEFAULT, flags, init_state, std::move(name))
        {
        }
    };


    GpuDefaultBuffer::GpuDefaultBuffer() = default;
    GpuDefaultBuffer::~GpuDefaultBuffer() noexcept = default;
    GpuDefaultBuffer::GpuDefaultBuffer(GpuDefaultBuffer&& other) noexcept = default;
    GpuDefaultBuffer& GpuDefaultBuffer::operator=(GpuDefaultBuffer&& other) noexcept = default;


    class GpuUploadBuffer::Impl final : public GpuBuffer::Impl
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        Impl(ID3D12Device5* device, uint32_t size, std::wstring_view name)
            : GpuBuffer::Impl(
                  device, size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, std::move(name)),
              mapped_data_(this->Map())
        {
        }

        ~Impl() noexcept override
        {
            this->Reset();
        }

        void Reset() noexcept override
        {
            if (resource_)
            {
                this->Unmap();
            }

            GpuBuffer::Impl::Reset();
        }

        void* MappedData() noexcept
        {
            return mapped_data_;
        }

    private:
        void* mapped_data_ = nullptr;
    };


    GpuUploadBuffer::GpuUploadBuffer() = default;
    GpuUploadBuffer::~GpuUploadBuffer() noexcept = default;
    GpuUploadBuffer::GpuUploadBuffer(GpuUploadBuffer&& other) noexcept = default;
    GpuUploadBuffer& GpuUploadBuffer::operator=(GpuUploadBuffer&& other) noexcept = default;

    void GpuUploadBuffer::Reset() noexcept
    {
        impl_->Reset();
    }

    void* GpuUploadBuffer::MappedData() noexcept
    {
        return static_cast<GpuUploadBuffer::Impl&>(*impl_).MappedData();
    }


    class GpuReadbackBuffer::Impl final : public GpuBuffer::Impl
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        Impl(ID3D12Device5* device, uint32_t size, std::wstring_view name)
            : GpuBuffer::Impl(
                  device, size, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST, std::move(name)),
              mapped_data_(this->Map())
        {
        }

        ~Impl() noexcept override
        {
            this->Reset();
        }

        void Reset() noexcept override
        {
            if (resource_)
            {
                this->Unmap();
            }

            GpuBuffer::Impl::Reset();
        }

        void* MappedData() noexcept
        {
            return mapped_data_;
        }

    private:
        void* mapped_data_ = nullptr;
    };


    GpuReadbackBuffer::GpuReadbackBuffer() = default;
    GpuReadbackBuffer::~GpuReadbackBuffer() noexcept = default;
    GpuReadbackBuffer::GpuReadbackBuffer(GpuReadbackBuffer&& other) noexcept = default;
    GpuReadbackBuffer& GpuReadbackBuffer::operator=(GpuReadbackBuffer&& other) noexcept = default;

    void GpuReadbackBuffer::Reset() noexcept
    {
        impl_->Reset();
    }

    void* GpuReadbackBuffer::MappedData() noexcept
    {
        return static_cast<GpuReadbackBuffer::Impl&>(*impl_).MappedData();
    }


    GpuBuffer GpuSystemInternalD3D12::CreateBuffer(ID3D12Device5* device, uint32_t size, D3D12_HEAP_TYPE heap_type,
        D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, std::wstring_view name)
    {
        GpuBuffer buffer;
        buffer.impl_ = ImplPtr<GpuBuffer::Impl>(device, size, heap_type, flags, init_state, name);
        return buffer;
    }

    GpuDefaultBuffer GpuSystemInternalD3D12::CreateDefaultBuffer(
        ID3D12Device5* device, uint32_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, std::wstring_view name)
    {
        GpuDefaultBuffer buffer;
        buffer.impl_ = ImplPtr<GpuDefaultBuffer::Impl>(device, size, flags, init_state, name);
        return buffer;
    }

    GpuUploadBuffer GpuSystemInternalD3D12::CreateUploadBuffer(ID3D12Device5* device, uint32_t size, std::wstring_view name)
    {
        GpuUploadBuffer buffer;
        buffer.impl_ = ImplPtr<GpuUploadBuffer::Impl>(device, size, name);
        return buffer;
    }

    GpuReadbackBuffer GpuSystemInternalD3D12::CreateReadbackBuffer(ID3D12Device5* device, uint32_t size, std::wstring_view name)
    {
        GpuReadbackBuffer buffer;
        buffer.impl_ = ImplPtr<GpuReadbackBuffer::Impl>(device, size, name);
        return buffer;
    }
} // namespace GoldenSun
