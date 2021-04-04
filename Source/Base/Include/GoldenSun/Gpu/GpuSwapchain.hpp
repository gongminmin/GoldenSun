#pragma once

#include <GoldenSun/Gpu/GpuTexture2D.hpp>
#include <GoldenSun/ImplPtr.hpp>

namespace GoldenSun
{
    class GpuSystemInternalD3D12;

    class GpuSwapChain final
    {
        friend class GpuSystemInternalD3D12;

        DISALLOW_COPY_AND_ASSIGN(GpuSwapChain)

    public:
        GpuSwapChain();
        ~GpuSwapChain() noexcept;

        GpuSwapChain(GpuSwapChain&& other) noexcept;
        GpuSwapChain& operator=(GpuSwapChain&& other) noexcept;

        explicit operator bool() const noexcept;

        uint32_t Width() const noexcept;
        uint32_t Height() const noexcept;
        DXGI_FORMAT Format() const noexcept;

        bool Resize(uint32_t width, uint32_t height, DXGI_FORMAT format);
        bool Present(uint32_t sync_interval);

        GpuTexture2D Buffer(uint32_t index) const;

        void Reset() noexcept;

    private:
        class Impl;
        ImplPtr<Impl> impl_;
    };
} // namespace GoldenSun
