#include "../pch.hpp"

#include <GoldenSun/Gpu/GpuSystem.hpp>
#include <GoldenSun/Util.hpp>

#include <string>

#include "../ImplPtrImpl.hpp"
#include "GpuSystemInternal.hpp"

namespace GoldenSun
{
    class GpuSwapChain::Impl final
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

    public:
        Impl() noexcept = default;

        Impl(IDXGIFactory4* dxgi_factory, ID3D12CommandQueue* cmd_queue, HWND hwnd, uint32_t width, uint32_t height, DXGI_FORMAT format,
            bool tearing_supported)
            : width_(width), height_(height), format_(format), tearing_supported_(tearing_supported)
        {
            DXGI_SWAP_CHAIN_DESC1 const swap_chain_desc{width, height, format, FALSE, {1, 0}, DXGI_USAGE_RENDER_TARGET_OUTPUT,
                GpuSystem::FrameCount(), DXGI_SCALING_STRETCH, DXGI_SWAP_EFFECT_FLIP_DISCARD, DXGI_ALPHA_MODE_IGNORE,
                tearing_supported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0U};

            DXGI_SWAP_CHAIN_FULLSCREEN_DESC const fs_swap_chain_desc{
                {0, 0}, DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED, DXGI_MODE_SCALING_UNSPECIFIED, TRUE};

            ComPtr<IDXGISwapChain1> swap_chain;
            TIFHR(dxgi_factory->CreateSwapChainForHwnd(cmd_queue, hwnd, &swap_chain_desc, &fs_swap_chain_desc, nullptr, swap_chain.Put()));
            swap_chain_ = swap_chain.As<IDXGISwapChain3>();

            dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
        }

        explicit operator bool() const noexcept
        {
            return swap_chain_ ? true : false;
        }

        uint32_t Width() const noexcept
        {
            return width_;
        }

        uint32_t Height() const noexcept
        {
            return height_;
        }

        DXGI_FORMAT Format() const noexcept
        {
            return format_;
        }

        bool Resize(uint32_t width, uint32_t height, DXGI_FORMAT format)
        {
            width_ = width;
            height_ = height;
            format_ = format;

            HRESULT const hr = swap_chain_->ResizeBuffers(
                GpuSystem::FrameCount(), width, height, format, tearing_supported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
            if ((hr == DXGI_ERROR_DEVICE_REMOVED) || (hr == DXGI_ERROR_DEVICE_RESET))
            {
                return false;
            }
            else
            {
                TIFHR(hr);
                return true;
            }
        }

        bool Present(uint32_t sync_interval)
        {
            HRESULT const hr = swap_chain_->Present(sync_interval, tearing_supported_ ? DXGI_PRESENT_ALLOW_TEARING : 0);
            if ((hr == DXGI_ERROR_DEVICE_REMOVED) || (hr == DXGI_ERROR_DEVICE_RESET))
            {
                return false;
            }
            else
            {
                TIFHR(hr);
                return true;
            }
        }

        GpuTexture2D Buffer(uint32_t index) const
        {
            ComPtr<ID3D12Resource> render_target;
            TIFHR(swap_chain_->GetBuffer(index, UuidOf<ID3D12Resource>(), render_target.PutVoid()));

            return GpuTexture2D(render_target.Get(), D3D12_RESOURCE_STATE_PRESENT, L"Render target " + std::to_wstring(index));
        }

        void Reset() noexcept
        {
            swap_chain_.Reset();
            width_ = 0;
            height_ = 0;
            format_ = DXGI_FORMAT_UNKNOWN;
        }

    private:
        ComPtr<IDXGISwapChain3> swap_chain_;
        uint32_t width_{};
        uint32_t height_{};
        DXGI_FORMAT format_{};
        bool const tearing_supported_{};
    };


    GpuSwapChain::GpuSwapChain() = default;
    GpuSwapChain::~GpuSwapChain() noexcept = default;
    GpuSwapChain::GpuSwapChain(GpuSwapChain&& other) noexcept = default;
    GpuSwapChain& GpuSwapChain::operator=(GpuSwapChain&& other) noexcept = default;

    GpuSwapChain::operator bool() const noexcept
    {
        return impl_ && impl_->operator bool();
    }

    uint32_t GpuSwapChain::Width() const noexcept
    {
        return impl_->Width();
    }

    uint32_t GpuSwapChain::Height() const noexcept
    {
        return impl_->Height();
    }

    DXGI_FORMAT GpuSwapChain::Format() const noexcept
    {
        return impl_->Format();
    }

    bool GpuSwapChain::Resize(uint32_t width, uint32_t height, DXGI_FORMAT format)
    {
        return impl_->Resize(width, height, format);
    }

    bool GpuSwapChain::Present(uint32_t sync_interval)
    {
        return impl_->Present(sync_interval);
    }

    GpuTexture2D GpuSwapChain::Buffer(uint32_t index) const
    {
        return impl_->Buffer(index);
    }

    void GpuSwapChain::Reset() noexcept
    {
        impl_->Reset();
    }


    GpuSwapChain GpuSystemInternalD3D12::CreateSwapChain(IDXGIFactory4* dxgi_factory, ID3D12CommandQueue* cmd_queue, HWND hwnd,
        uint32_t width, uint32_t height, DXGI_FORMAT format, bool tearing_supported)
    {
        GpuSwapChain swap_chain;
        swap_chain.impl_ = ImplPtr<GpuSwapChain::Impl>(dxgi_factory, cmd_queue, hwnd, width, height, format, tearing_supported);
        return swap_chain;
    }
} // namespace GoldenSun
