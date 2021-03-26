#pragma once

namespace GoldenSun
{
    class GpuSystemInternalD3D12 final
    {
    public:
        static GpuCommandList CreateCommandList(ID3D12Device5* device, ID3D12CommandAllocator* cmd_allocator);
        static void ResetCommandList(GpuCommandList& cmd_list, ID3D12CommandAllocator* cmd_allocator);

        static GpuBuffer CreateBuffer(ID3D12Device5* device, uint32_t size, D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_FLAGS flags,
            D3D12_RESOURCE_STATES init_state, std::wstring_view name);
        static GpuDefaultBuffer CreateDefaultBuffer(
            ID3D12Device5* device, uint32_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, std::wstring_view name);
        static GpuUploadBuffer CreateUploadBuffer(ID3D12Device5* device, uint32_t size, std::wstring_view name);
        static GpuReadbackBuffer CreateReadbackBuffer(ID3D12Device5* device, uint32_t size, std::wstring_view name);

        static GpuTexture2D CreateTexture2D(ID3D12Device5* device, uint32_t width, uint32_t height, uint32_t mip_levels, DXGI_FORMAT format,
            D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, std::wstring_view name);

        static GpuDescriptorHeap CreateDescriptorHeap(ID3D12Device5* device, uint32_t size, D3D12_DESCRIPTOR_HEAP_TYPE type,
            D3D12_DESCRIPTOR_HEAP_FLAGS flags, std::wstring_view name);

        static GpuShaderResourceView CreateShaderResourceView(ID3D12Device5* device, GpuBuffer const& buffer, uint32_t num_elements,
            uint32_t element_size, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);
        static GpuShaderResourceView CreateShaderResourceView(
            ID3D12Device5* device, GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);

        static GpuRenderTargetView CreateRenderTargetView(
            ID3D12Device5* device, GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);

        static GpuUnorderedAccessView CreateUnorderedAccessView(ID3D12Device5* device, GpuBuffer const& buffer, uint32_t first_element,
            uint32_t num_elements, uint32_t element_size, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);
        static GpuUnorderedAccessView CreateUnorderedAccessView(
            ID3D12Device5* device, GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle);

        static GpuSwapChain CreateSwapChain(IDXGIFactory4* dxgi_factory, ID3D12CommandQueue* cmd_queue, HWND hwnd, uint32_t width,
            uint32_t height, DXGI_FORMAT format, bool tearing_supported);
    };
} // namespace GoldenSun
