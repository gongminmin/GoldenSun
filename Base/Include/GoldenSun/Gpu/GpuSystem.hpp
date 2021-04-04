#pragma once

#include <GoldenSun/Gpu/GpuBuffer.hpp>
#include <GoldenSun/Gpu/GpuCommandList.hpp>
#include <GoldenSun/Gpu/GpuDescriptorAllocator.hpp>
#include <GoldenSun/Gpu/GpuDescriptorHeap.hpp>
#include <GoldenSun/Gpu/GpuMemoryAllocator.hpp>
#include <GoldenSun/Gpu/GpuResourceViews.hpp>
#include <GoldenSun/Gpu/GpuSwapChain.hpp>
#include <GoldenSun/Gpu/GpuTexture2D.hpp>
#include <GoldenSun/ImplPtr.hpp>

#include <GoldenSun/Gpu/GpuSystemD3D12.hpp>

#include <string_view>

namespace GoldenSun
{
    class GpuSystem final
    {
        DISALLOW_COPY_AND_ASSIGN(GpuSystem)

    public:
        GpuSystem();
        GpuSystem(void* native_device, void* native_cmd_queue);
        ~GpuSystem() noexcept;

        GpuSystem(GpuSystem&& other) noexcept;
        GpuSystem& operator=(GpuSystem&& other) noexcept;

        bool TearingSupported() const noexcept;

        void* NativeDeviceHandle() const noexcept;
        template <typename ApiTraits>
        typename ApiTraits::DeviceType NativeDeviceHandle() const noexcept
        {
            return reinterpret_cast<typename ApiTraits::DeviceType>(this->NativeDeviceHandle());
        }

        void* NativeCommandQueueHandle() const noexcept;
        template <typename ApiTraits>
        typename ApiTraits::CommandQueueType NativeCommandQueueHandle() const noexcept
        {
            return reinterpret_cast<typename ApiTraits::CommandQueueType>(this->NativeCommandQueueHandle());
        }

        uint32_t FrameIndex() const noexcept;
        static uint32_t constexpr FrameCount() noexcept
        {
            return 3;
        }

        void MoveToNextFrame();

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO RayTracingAccelerationStructurePrebuildInfo(
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS const& desc);

        GpuBuffer CreateBuffer(uint32_t size, D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state,
            std::wstring_view name = L"");
        GpuDefaultBuffer CreateDefaultBuffer(
            uint32_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, std::wstring_view name = L"");
        GpuUploadBuffer CreateUploadBuffer(uint32_t size, std::wstring_view name = L"");
        GpuUploadBuffer CreateUploadBuffer(void const* data, uint32_t size, std::wstring_view name = L"");
        GpuReadbackBuffer CreateReadbackBuffer(uint32_t size, std::wstring_view name = L"");

        GpuTexture2D CreateTexture2D(uint32_t width, uint32_t height, uint32_t mip_levels, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
            D3D12_RESOURCE_STATES init_state, std::wstring_view name = L"");

        GpuDescriptorHeap CreateDescriptorHeap(
            uint32_t size, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, std::wstring_view name);

        GpuShaderResourceView CreateShaderResourceView(GpuBuffer const& buffer, uint32_t first_element, uint32_t num_elements,
            uint32_t element_size, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);
        GpuShaderResourceView CreateShaderResourceView(
            GpuMemoryBlock const& mem_block, uint32_t element_size, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);
        GpuShaderResourceView CreateShaderResourceView(
            GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);
        GpuShaderResourceView CreateShaderResourceView(GpuTexture2D const& texture, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);
        GpuRenderTargetView CreateRenderTargetView(GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);
        GpuRenderTargetView CreateRenderTargetView(GpuTexture2D const& texture, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);
        GpuUnorderedAccessView CreateUnorderedAccessView(GpuBuffer const& buffer, uint32_t first_element, uint32_t num_elements,
            uint32_t element_size, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);
        GpuUnorderedAccessView CreateUnorderedAccessView(
            GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);
        GpuUnorderedAccessView CreateUnorderedAccessView(GpuTexture2D const& texture, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);

        GpuSwapChain CreateSwapChain(void* native_window, uint32_t width, uint32_t height, DXGI_FORMAT format);

        [[nodiscard]] GpuCommandList CreateCommandList();
        void Execute(GpuCommandList&& cmd_list);
        void ExecuteAndReset(GpuCommandList& cmd_list);

        uint32_t RtvDescSize() const noexcept;
        uint32_t DsvDescSize() const noexcept;
        uint32_t CbvSrvUavDescSize() const noexcept;
        uint32_t SamplerDescSize() const noexcept;

        GpuDescriptorBlock AllocRtvDescBlock(uint32_t size);
        void DeallocRtvDescBlock(GpuDescriptorBlock&& desc_block);
        void ReallocRtvDescBlock(GpuDescriptorBlock& desc_block, uint32_t size);
        GpuDescriptorBlock AllocDsvDescBlock(uint32_t size);
        void DeallocDsvDescBlock(GpuDescriptorBlock&& desc_block);
        void ReallocDsvDescBlock(GpuDescriptorBlock& desc_block, uint32_t size);
        GpuDescriptorBlock AllocCbvSrvUavDescBlock(uint32_t size);
        void DeallocCbvSrvUavDescBlock(GpuDescriptorBlock&& desc_block);
        void ReallocCbvSrvUavDescBlock(GpuDescriptorBlock& desc_block, uint32_t size);
        GpuDescriptorBlock AllocSamplerDescBlock(uint32_t size);
        void DeallocSamplerDescBlock(GpuDescriptorBlock&& desc_block);
        void ReallocSamplerDescBlock(GpuDescriptorBlock& desc_block, uint32_t size);

        GpuMemoryBlock AllocUploadMemBlock(uint32_t size_in_bytes, uint32_t alignment);
        void DeallocUploadMemBlock(GpuMemoryBlock&& mem_block);
        void ReallocUploadMemBlock(GpuMemoryBlock& mem_block, uint32_t size_in_bytes, uint32_t alignment);
        GpuMemoryBlock AllocReadbackMemBlock(uint32_t size_in_bytes, uint32_t alignment);
        void DeallocReadbackMemBlock(GpuMemoryBlock&& mem_block);
        void ReallocReadbackMemBlock(GpuMemoryBlock& mem_block, uint32_t size_in_bytes, uint32_t alignment);

        void WaitForGpu();
        void ResetFenceValues();

        void HandleDeviceLost();

    private:
        class Impl;
        ImplPtr<Impl> impl_;
    };
} // namespace GoldenSun

#include <GoldenSun/Gpu/GpuBufferHelper.hpp>
