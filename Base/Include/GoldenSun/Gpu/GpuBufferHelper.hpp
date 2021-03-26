#pragma once

#include <string_view>
#include <vector>

namespace GoldenSun
{
    template <typename T>
    class ConstantBuffer final
    {
        static_assert(std::is_pod<T>::value);

    public:
        using value_type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;

    public:
        ConstantBuffer() noexcept = default;

        ConstantBuffer(GpuSystem& gpu_system, uint32_t num_frames = 1, std::wstring_view name = L"")
            : buffer_(gpu_system.CreateUploadBuffer(
                  num_frames * Align<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(sizeof(value_type)), name)),
              aligned_size_(Align<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(sizeof(value_type))), num_frames_(num_frames)
        {
        }

        ConstantBuffer(ConstantBuffer&& other) noexcept
            : buffer_(std::move(other.buffer_)), staging_(std::move(other.staging_)), aligned_size_(std::move(other.aligned_size_)),
              num_frames_(std::move(other.num_frames_))
        {
        }

        ConstantBuffer& operator=(ConstantBuffer&& other) noexcept
        {
            if (this != &other)
            {
                buffer_ = std::move(other.buffer_);
                staging_ = std::move(other.staging_);
                aligned_size_ = std::move(other.aligned_size_);
                num_frames_ = std::move(other.num_frames_);
            }
            return *this;
        }

        explicit operator bool() const noexcept
        {
            return buffer_ ? true : false;
        }

        GpuUploadBuffer const& Buffer() const noexcept
        {
            return buffer_;
        }

        value_type* MappedData(uint32_t frame_index = 0) noexcept
        {
            return reinterpret_cast<value_type*>(buffer_.MappedData<uint8_t>() + frame_index * aligned_size_);
        }
        value_type const* MappedData(uint32_t frame_index = 0) const noexcept
        {
            return reinterpret_cast<value_type const*>(buffer_.MappedData<uint8_t>() + frame_index * aligned_size_);
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

        void* NativeResource() const noexcept
        {
            return buffer_.NativeResource();
        }

        D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress(uint32_t frame_index = 0) const noexcept
        {
            return buffer_.GpuVirtualAddress() + frame_index * aligned_size_;
        }

    private:
        GpuUploadBuffer buffer_;

        value_type staging_;

        uint32_t aligned_size_;
        uint32_t num_frames_;
    };

    template <typename T>
    class StructuredBuffer final
    {
    public:
        using value_type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
        static_assert(std::is_standard_layout<value_type>::value);

    public:
        // Performance tip: Align structures on 16-byte boundary
        // Ref: https://developer.nvidia.com/content/understanding-structured-buffer-performance
        static_assert(sizeof(value_type) % 16 == 0, "Align structure buffers on 16-byte boundary for performance reasons.");

        StructuredBuffer() noexcept = default;

        StructuredBuffer(GpuSystem& gpu_system, uint32_t num_elements, uint32_t num_frames = 1, std::wstring_view name = L"")
            : buffer_(gpu_system.CreateUploadBuffer(num_frames * num_elements * sizeof(value_type), name)), num_frames_(num_frames),
              staging_(num_elements)
        {
        }

        StructuredBuffer(StructuredBuffer&& other) noexcept
            : buffer_(std::move(other.buffer_)), staging_(std::move(other.staging_)), num_frames_(std::move(other.num_frames_))
        {
        }

        StructuredBuffer& operator=(StructuredBuffer&& other) noexcept
        {
            if (this != &other)
            {
                buffer_ = std::move(other.buffer_);
                staging_ = std::move(other.staging_);
                num_frames_ = std::move(other.num_frames_);
            }
            return *this;
        }

        explicit operator bool() const noexcept
        {
            return buffer_ ? true : false;
        }

        GpuUploadBuffer const& Buffer() const noexcept
        {
            return buffer_;
        }

        value_type* MappedData(uint32_t frame_index = 0) noexcept
        {
            return buffer_.MappedData<value_type>() + frame_index * this->NumElements();
        }
        value_type const* MappedData(uint32_t frame_index = 0) const noexcept
        {
            return buffer_.MappedData<value_type const>() + frame_index * this->NumElements();
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

        void* NativeResource() const noexcept
        {
            return buffer_.NativeResource();
        }

        D3D12_GPU_VIRTUAL_ADDRESS GpuVirtualAddress(uint32_t frame_index = 0, uint32_t element_index = 0) const noexcept
        {
            return buffer_.GpuVirtualAddress() + frame_index * this->FrameSize() + element_index * this->ElementSize();
        }

    private:
        GpuUploadBuffer buffer_;

        std::vector<value_type> staging_;
        uint32_t num_frames_{};
    };
} // namespace GoldenSun
