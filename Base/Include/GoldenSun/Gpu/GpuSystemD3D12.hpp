#pragma once

namespace GoldenSun
{
    class GpuSystemInternalD3D12;

    struct D3D12Traits
    {
        using DeviceType = ID3D12Device5*;
        using CommandQueueType = ID3D12CommandQueue*;
        using CommandListType = ID3D12GraphicsCommandList4*;
        using BufferType = ID3D12Resource*;
        using Texture2DType = ID3D12Resource*;
        using DescriptorHeapType = ID3D12DescriptorHeap*;
    };
} // namespace GoldenSun
