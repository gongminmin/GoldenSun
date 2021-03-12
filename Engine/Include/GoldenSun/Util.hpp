#pragma once

#include <d3d12.h>

#include <type_traits>
#include <vector>

#include <GoldenSun/Base.hpp>
#include <GoldenSun/ComPtr.hpp>

#pragma warning(disable : 4251)

#define DISALLOW_COPY_AND_ASSIGN(ClassName)     \
    ClassName(ClassName const& other) = delete; \
    ClassName& operator=(ClassName const& other) = delete;

namespace GoldenSun
{
    static constexpr uint32_t FrameCount = 3;

    template <uint32_t Alignment>
    constexpr uint32_t Align(uint32_t size) noexcept
    {
        static_assert((Alignment & (Alignment - 1)) == 0);
        return (size + (Alignment - 1)) & ~(Alignment - 1);
    }

    DXGI_FORMAT GOLDEN_SUN_API LinearFormatOf(DXGI_FORMAT fmt) noexcept;
    DXGI_FORMAT GOLDEN_SUN_API SRGBFormatOf(DXGI_FORMAT fmt) noexcept;

    D3D12_ROOT_PARAMETER GOLDEN_SUN_API CreateRootParameterAsDescriptorTable(const D3D12_DESCRIPTOR_RANGE* descriptor_ranges,
        uint32_t num_descriptor_ranges, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept;
    D3D12_ROOT_PARAMETER GOLDEN_SUN_API CreateRootParameterAsShaderResourceView(
        uint32_t shader_register, uint32_t register_space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept;
    D3D12_ROOT_PARAMETER GOLDEN_SUN_API CreateRootParameterAsConstantBufferView(
        uint32_t shader_register, uint32_t register_space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept;
    D3D12_ROOT_PARAMETER GOLDEN_SUN_API CreateRootParameterAsConstants(uint32_t num_32bit_values, uint32_t shader_register,
        uint32_t register_space = 0, D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept;

    class GOLDEN_SUN_API GpuBuffer
    {
        DISALLOW_COPY_AND_ASSIGN(GpuBuffer);

    public:
        ID3D12Resource* Resource() const noexcept;
        uint64_t Size() const noexcept;

    protected:
        GpuBuffer() noexcept;
        GpuBuffer(ID3D12Device5* device, uint64_t buffer_size, D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_FLAGS flags,
            D3D12_RESOURCE_STATES init_state, wchar_t const* name);
        ~GpuBuffer() noexcept;

        GpuBuffer(GpuBuffer&& other) noexcept;
        GpuBuffer& operator=(GpuBuffer&& other) noexcept;

    protected:
        ComPtr<ID3D12Resource> resource_;
    };

    class GOLDEN_SUN_API GpuDefaultBuffer : public GpuBuffer
    {
    public:
        GpuDefaultBuffer() noexcept;
        GpuDefaultBuffer(
            ID3D12Device5* device, uint64_t buffer_size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, wchar_t const* name);

        GpuDefaultBuffer(GpuDefaultBuffer&& other) noexcept;
        GpuDefaultBuffer& operator=(GpuDefaultBuffer&& other) noexcept;
    };

    class GOLDEN_SUN_API GpuUploadBuffer : public GpuBuffer
    {
    public:
        GpuUploadBuffer() noexcept;
        GpuUploadBuffer(ID3D12Device5* device, uint64_t buffer_size, wchar_t const* name);
        ~GpuUploadBuffer() noexcept;

        GpuUploadBuffer(GpuUploadBuffer&& other) noexcept;
        GpuUploadBuffer& operator=(GpuUploadBuffer&& other) noexcept;

        template <typename T>
        T* MappedData() noexcept
        {
            return reinterpret_cast<T*>(mapped_data_);
        }

    private:
        void* mapped_data_ = nullptr;
    };

    class GOLDEN_SUN_API GpuReadbackBuffer : public GpuBuffer
    {
    public:
        GpuReadbackBuffer() noexcept;
        GpuReadbackBuffer(ID3D12Device5* device, uint64_t buffer_size, wchar_t const* name);
        ~GpuReadbackBuffer() noexcept;

        GpuReadbackBuffer(GpuReadbackBuffer&& other) noexcept;
        GpuReadbackBuffer& operator=(GpuReadbackBuffer&& other) noexcept;

        template <typename T>
        T const* MappedData() const noexcept
        {
            return reinterpret_cast<T*>(mapped_data_);
        }

    private:
        void* mapped_data_ = nullptr;
    };

    template <typename T>
    class ConstantBuffer : public GpuUploadBuffer
    {
        static_assert(std::is_pod<T>::value);

    public:
        using value_type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    public:
        ConstantBuffer(ID3D12Device5* device, uint32_t num_frames = 1, wchar_t const* name = nullptr)
            : GpuUploadBuffer(device, num_frames * Align<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(sizeof(value_type)), name),
              aligned_size_(Align<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(sizeof(value_type))), num_frames_(num_frames)
        {
        }

        ConstantBuffer(ConstantBuffer&& other) noexcept
            : GpuUploadBuffer(other), staging_(std::move(other.staging_)), aligned_size_(std::move(other.aligned_size_)),
              num_frames_(std::move(other.num_frames_))
        {
        }

        ConstantBuffer& operator=(ConstantBuffer&& other) noexcept
        {
            if (this != &other)
            {
                GpuUploadBuffer::operator=(std::move(other));

                staging_ = std::move(other.staging_);
                aligned_size_ = std::move(other.aligned_size_);
                num_frames_ = std::move(other.num_frames_);
            }
            return *this;
        }

        value_type* MappedData(uint32_t frame_index = 0) noexcept
        {
            return reinterpret_cast<value_type*>(GpuUploadBuffer::MappedData<uint8_t>() + frame_index * aligned_size_);
        }
        value_type const* MappedData(uint32_t frame_index = 0) const noexcept
        {
            return reinterpret_cast<value_type const*>(GpuUploadBuffer::MappedData<uint8_t>() + frame_index * aligned_size_);
        }

        void UploadToGpu(uint32_t frame_index = 0) noexcept
        {
            *this->MappedData(frame_index) = staging_;
        }

        value_type* operator->() noexcept
        {
            return &staging_;
        }
        value_type const* operator->() const noexcept
        {
            return &staging_;
        }

        uint32_t NumFrames() const noexcept
        {
            return num_frames_;
        }

        D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress(uint32_t frame_index = 0) const noexcept
        {
            return resource_->GetGPUVirtualAddress() + frame_index * aligned_size_;
        }

    private:
        value_type staging_;

        uint32_t aligned_size_;
        uint32_t num_frames_;
    };

    template <typename T>
    class StructuredBuffer : public GpuUploadBuffer
    {
        static_assert(std::is_pod<T>::value);

    public:
        using value_type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    public:
        // Performance tip: Align structures on 16-byte boundary
        // Ref: https://developer.nvidia.com/content/understanding-structured-buffer-performance
        static_assert(sizeof(value_type) % 16 == 0, "Align structure buffers on 16-byte boundary for performance reasons.");

        StructuredBuffer(ID3D12Device5* device, uint32_t num_elements, uint32_t num_frames = 1, wchar_t const* name = nullptr)
            : GpuUploadBuffer(device, num_frames * num_elements * sizeof(value_type), name), num_frames_(num_frames), staging_(num_elements)
        {
        }

        StructuredBuffer(StructuredBuffer&& other) noexcept
            : GpuUploadBuffer(other), staging_(std::move(other.staging_)), num_frames_(std::move(other.num_frames_))
        {
        }

        StructuredBuffer& operator=(StructuredBuffer&& other) noexcept
        {
            if (this != &other)
            {
                GpuUploadBuffer::operator=(std::move(other));

                staging_ = std::move(other.staging_);
                num_frames_ = std::move(other.num_frames_);
            }
            return *this;
        }

        value_type* MappedData(uint32_t frame_index = 0) noexcept
        {
            return GpuUploadBuffer::MappedData<value_type>() + frame_index * this->NumElements();
        }
        value_type const* MappedData(uint32_t frame_index = 0) const noexcept
        {
            return GpuUploadBuffer::MappedData<value_type const>() + frame_index * this->NumElements();
        }

        void UploadToGpu(uint32_t frame_index = 0) noexcept
        {
            memcpy(this->MappedData(frame_index), staging_.data(), this->FrameSize());
        }

        auto begin() noexcept
        {
            return staging_.begin();
        }
        auto end() noexcept
        {
            return staging_.end();
        }

        auto begin() const noexcept
        {
            return staging_.begin();
        }
        auto end() const noexcept
        {
            return staging_.end();
        }

        value_type& operator[](uint32_t element_index) noexcept
        {
            return staging_[element_index];
        }
        value_type const& operator[](uint32_t element_index) const noexcept
        {
            return staging_[element_index];
        }

        uint32_t NumElements() const noexcept
        {
            return static_cast<uint32_t>(staging_.size());
        }

        uint32_t ElementSize() const noexcept
        {
            return sizeof(value_type);
        }

        uint32_t NumFrames() const noexcept
        {
            return num_frames_;
        }

        uint32_t FrameSize() const noexcept
        {
            return this->NumElements() * this->ElementSize();
        }

        D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress(uint32_t frame_index = 0, uint32_t element_index = 0) const noexcept
        {
            return resource_->GetGPUVirtualAddress() + frame_index * this->FrameSize() + element_index * this->ElementSize();
        }

    private:
        std::vector<value_type> staging_;
        uint32_t num_frames_;
    };
} // namespace GoldenSun
