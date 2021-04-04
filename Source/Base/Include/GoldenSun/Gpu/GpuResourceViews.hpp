#pragma once

#include <GoldenSun/ImplPtr.hpp>

#include <string_view>

namespace GoldenSun
{
    class GpuSystemInternalD3D12;

    class GpuShaderResourceView final
    {
        friend class GpuSystemInternalD3D12;

        DISALLOW_COPY_AND_ASSIGN(GpuShaderResourceView)

    public:
        GpuShaderResourceView();
        ~GpuShaderResourceView() noexcept;

        GpuShaderResourceView(GpuShaderResourceView&& other) noexcept;
        GpuShaderResourceView& operator=(GpuShaderResourceView&& other) noexcept;

        explicit operator bool() const noexcept;

        void Reset() noexcept;

    private:
        class Impl;
        ImplPtr<Impl> impl_;
    };

    class GpuRenderTargetView final
    {
        friend class GpuSystemInternalD3D12;

        DISALLOW_COPY_AND_ASSIGN(GpuRenderTargetView)

    public:
        GpuRenderTargetView();
        ~GpuRenderTargetView() noexcept;

        GpuRenderTargetView(GpuRenderTargetView&& other) noexcept;
        GpuRenderTargetView& operator=(GpuRenderTargetView&& other) noexcept;

        explicit operator bool() const noexcept;

        void Reset() noexcept;

    private:
        class Impl;
        ImplPtr<Impl> impl_;
    };

    class GpuUnorderedAccessView final
    {
        friend class GpuSystemInternalD3D12;

        DISALLOW_COPY_AND_ASSIGN(GpuUnorderedAccessView)

    public:
        GpuUnorderedAccessView();
        ~GpuUnorderedAccessView() noexcept;

        GpuUnorderedAccessView(GpuUnorderedAccessView&& other) noexcept;
        GpuUnorderedAccessView& operator=(GpuUnorderedAccessView&& other) noexcept;

        explicit operator bool() const noexcept;

        void Reset() noexcept;

    private:
        class Impl;
        ImplPtr<Impl> impl_;
    };
} // namespace GoldenSun
