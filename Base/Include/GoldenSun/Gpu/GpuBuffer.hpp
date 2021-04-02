#pragma once

#include <GoldenSun/ImplPtr.hpp>

#include <string_view>

namespace GoldenSun
{
    class GpuCommandList;
    class GpuSystemInternalD3D12;

    class GpuBuffer
    {
        friend class GpuSystemInternalD3D12;

        DISALLOW_COPY_AND_ASSIGN(GpuBuffer)

    public:
        GpuBuffer();
        GpuBuffer(void* native_resource, D3D12_RESOURCE_STATES curr_state, std::wstring_view name = L"");
        ~GpuBuffer() noexcept;

        GpuBuffer(GpuBuffer&& other) noexcept;
        GpuBuffer& operator=(GpuBuffer&& other) noexcept;

        GpuBuffer Share() const;

        explicit operator bool() const noexcept;

        // TODO: Remove it after finishing the GPU system
        void* NativeResource() const noexcept;

        D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress() const noexcept;
        uint32_t Size() const noexcept;

        void* Map(D3D12_RANGE const& read_range);
        void* Map();
        void Unmap(D3D12_RANGE const& write_range);
        void Unmap();

        void Reset() noexcept;

        D3D12_RESOURCE_STATES State() const noexcept;
        void Transition(GpuCommandList& cmd_list, D3D12_RESOURCE_STATES target_state);

    protected:
        class Impl;
        ImplPtr<Impl> impl_;
    };

    class GpuDefaultBuffer final : public GpuBuffer
    {
        friend class GpuSystemInternalD3D12;

    public:
        GpuDefaultBuffer();
        ~GpuDefaultBuffer() noexcept;

        GpuDefaultBuffer(GpuDefaultBuffer&& other) noexcept;
        GpuDefaultBuffer& operator=(GpuDefaultBuffer&& other) noexcept;

    private:
        class Impl;
    };

    class GpuUploadBuffer final : public GpuBuffer
    {
        friend class GpuSystemInternalD3D12;

    public:
        GpuUploadBuffer();
        ~GpuUploadBuffer() noexcept;

        GpuUploadBuffer(GpuUploadBuffer&& other) noexcept;
        GpuUploadBuffer& operator=(GpuUploadBuffer&& other) noexcept;

        GpuUploadBuffer Share() const;

        void Reset() noexcept;

        void* MappedData() noexcept;

        template <typename T>
        T* MappedData() noexcept
        {
            return reinterpret_cast<T*>(this->MappedData());
        }

    private:
        class Impl;
    };

    class GpuReadbackBuffer final : public GpuBuffer
    {
        friend class GpuSystemInternalD3D12;

    public:
        GpuReadbackBuffer();
        ~GpuReadbackBuffer() noexcept;

        GpuReadbackBuffer(GpuReadbackBuffer&& other) noexcept;
        GpuReadbackBuffer& operator=(GpuReadbackBuffer&& other) noexcept;

        GpuReadbackBuffer Share() const;

        void Reset() noexcept;

        void* MappedData() noexcept;

        template <typename T>
        T* MappedData() noexcept
        {
            return reinterpret_cast<T*>(this->MappedData());
        }

    private:
        class Impl;
    };
} // namespace GoldenSun
