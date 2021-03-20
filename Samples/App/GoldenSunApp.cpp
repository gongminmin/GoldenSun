#include "pch.hpp"

#include "GoldenSunApp.hpp"
#include "MeshLoader.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <string>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

using namespace std;
using namespace DirectX;

DEFINE_UUID_OF(ID3D12CommandAllocator);
DEFINE_UUID_OF(ID3D12CommandQueue);
DEFINE_UUID_OF(ID3D12DescriptorHeap);
DEFINE_UUID_OF(ID3D12Device5);
DEFINE_UUID_OF(ID3D12GraphicsCommandList4);
DEFINE_UUID_OF(ID3D12Fence);
DEFINE_UUID_OF(ID3D12Resource);
DEFINE_UUID_OF(IDXGIAdapter1);
DEFINE_UUID_OF(IDXGIFactory4);
DEFINE_UUID_OF(IDXGIFactory5);
DEFINE_UUID_OF(IDXGIFactory6);
DEFINE_UUID_OF(IDXGISwapChain3);

#ifdef _DEBUG
DEFINE_UUID_OF(ID3D12Debug);
DEFINE_UUID_OF(ID3D12InfoQueue);
DEFINE_UUID_OF(IDXGIInfoQueue);
#endif

namespace
{
    const std::wstring WindowTitle = L"Golden Sun";
}

namespace GoldenSun
{
    GoldenSunApp::GoldenSunApp(uint32_t width, uint32_t height) : width_(width), height_(height)
    {
        {
            char exe_file[MAX_PATH];
            uint32_t size = ::GetModuleFileNameA(nullptr, exe_file, static_cast<uint32_t>(std::size(exe_file)));
            Verify((size != 0) && (size != std::size(exe_file)));

            std::filesystem::path exe_path = exe_file;
            asset_dir_ = (exe_path.parent_path() / "Assets/").string();
        }

        window_ = std::make_unique<WindowWin32>(*this, WindowTitle);

        this->CreateDeviceResources();
        this->CreateWindowSizeDependentResources();

        golden_sun_engine_ = std::make_unique<Engine>(device_.Get(), cmd_queue_.Get());
        golden_sun_engine_->RenderTarget(width_, height_, back_buffer_fmt_);

        this->InitializeScene();

        window_->ShowWindow(SW_SHOWNORMAL);

        (void)::DXGIDeclareAdapterRemovalSupport();
    }

    GoldenSunApp::~GoldenSunApp()
    {
        this->WaitForGpu();
    }

    int GoldenSunApp::Run()
    {
        bool got_msg;
        MSG msg{};
        while (msg.message != WM_QUIT)
        {
            if (active_)
            {
                got_msg = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) != 0;
            }
            else
            {
                got_msg = GetMessage(&msg, nullptr, 0, 0) != 0;
            }

            if (got_msg)
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
            {
                this->Refresh();
            }
        }

        return static_cast<char>(msg.wParam);
    }

    void GoldenSunApp::InitializeScene()
    {
        {
            eye_ = {0.0f, 1.0f, -3.0f};
            look_at_ = {0.0f, 0.0f, 0.0f};
            up_ = {0.0f, 1.0f, 0.0f};

            fov_ = XMConvertToRadians(45);
            near_plane_ = 0.1f;
            far_plane_ = 20;

            golden_sun_engine_->Camera(eye_, look_at_, up_, fov_, near_plane_, far_plane_);
        }
        {
            light_pos_ = {0.0f, 1.8f, -3.0f};
            light_color_ = {20.0f, 20.0f, 20.0f};
            light_falloff_ = {1, 0, 1};

            golden_sun_engine_->Light(light_pos_, light_color_, light_falloff_);
        }

        TIFHR(cmd_allocators_[frame_index_]->Reset());
        TIFHR(cmd_list_->Reset(cmd_allocators_[frame_index_].Get(), nullptr));

        meshes_ = LoadMesh(device_.Get(), cmd_list_.Get(), asset_dir_ + "DamagedHelmet/DamagedHelmet.gltf");
        golden_sun_engine_->Geometries(cmd_list_.Get(), meshes_.data(), static_cast<uint32_t>(meshes_.size()));

        this->ExecuteCommandList();
        this->WaitForGpu();
    }

    void GoldenSunApp::Active(bool active)
    {
        active_ = active;
    }

    void GoldenSunApp::Refresh()
    {
        this->FrameStats();

        {
            float const seconds_to_rotate_around = 12.0f;
            float const rotate = XMConvertToRadians(360.0f * (frame_time_ / seconds_to_rotate_around));
            XMMATRIX const rotate_mat = XMMatrixRotationY(rotate);
            XMVECTOR eye = XMVector3Transform(XMLoadFloat3(&eye_), rotate_mat);
            XMVECTOR look_at = XMVector3Transform(XMLoadFloat3(&look_at_), rotate_mat);
            XMVECTOR up = XMVector3Transform(XMLoadFloat3(&up_), rotate_mat);

            XMStoreFloat3(&eye_, eye);
            XMStoreFloat3(&look_at_, look_at);
            XMStoreFloat3(&up_, up);

            golden_sun_engine_->Camera(eye_, look_at_, up_, fov_, near_plane_, far_plane_);
        }
        {
            float const seconds_to_rotate_around = 8.0f;
            float const rotate = XMConvertToRadians(-360.0f * (frame_time_ / seconds_to_rotate_around));
            XMMATRIX const rotate_mat = XMMatrixRotationZ(rotate) * XMMatrixRotationY(rotate);
            XMVECTOR light_pos = XMVector3Transform(XMLoadFloat3(&light_pos_), rotate_mat);

            XMStoreFloat3(&light_pos_, light_pos);

            golden_sun_engine_->Light(light_pos_, light_color_, light_falloff_);
        }

        if (window_visible_)
        {
            TIFHR(cmd_allocators_[frame_index_]->Reset());
            TIFHR(cmd_list_->Reset(cmd_allocators_[frame_index_].Get(), nullptr));

            golden_sun_engine_->Render(cmd_list_.Get());

            auto* render_rarget = render_targets_[frame_index_].Get();
            auto* ray_tracing_output = golden_sun_engine_->Output();

            D3D12_RESOURCE_BARRIER barriers[2];
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barriers[0].Transition.pResource = render_rarget;
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barriers[1].Transition.pResource = ray_tracing_output;
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            cmd_list_->ResourceBarrier(static_cast<uint32_t>(std::size(barriers)), barriers);

            cmd_list_->CopyResource(render_rarget, ray_tracing_output);

            for (auto& barrier : barriers)
            {
                std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
            }
            cmd_list_->ResourceBarrier(static_cast<uint32_t>(std::size(barriers)), barriers);

            this->ExecuteCommandList();

            HRESULT hr = swap_chain_->Present(0, tearing_supported_ ? DXGI_PRESENT_ALLOW_TEARING : 0);
            if ((hr == DXGI_ERROR_DEVICE_REMOVED) || (hr == DXGI_ERROR_DEVICE_RESET))
            {
                this->HandleDeviceLost();
            }
            else
            {
                TIFHR(hr);

                uint64_t const curr_fence_value = fence_vals_[frame_index_];
                TIFHR(cmd_queue_->Signal(fence_.Get(), curr_fence_value));

                frame_index_ = swap_chain_->GetCurrentBackBufferIndex();

                if (fence_->GetCompletedValue() < fence_vals_[frame_index_])
                {
                    TIFHR(fence_->SetEventOnCompletion(fence_vals_[frame_index_], fence_event_.get()));
                    ::WaitForSingleObjectEx(fence_event_.get(), INFINITE, FALSE);
                }

                fence_vals_[frame_index_] = curr_fence_value + 1;
            }
        }
    }

    void GoldenSunApp::SizeChanged(uint32_t width, uint32_t height, bool minimized)
    {
        window_visible_ = !minimized;

        if (minimized || (width == 0) || (height == 0))
        {
            return;
        }

        if ((width != width_) || (height != height_))
        {
            width_ = width;
            height_ = height;
            this->CreateWindowSizeDependentResources();

            golden_sun_engine_->RenderTarget(width_, height_, back_buffer_fmt_);
        }
    }

    void GoldenSunApp::CreateDeviceResources()
    {
        dxgi_factory_.Reset();
        device_.Reset();

        bool debug_dxgi = false;

#ifdef _DEBUG
        {
            ComPtr<ID3D12Debug> debug_ctrl;
            if (SUCCEEDED(::D3D12GetDebugInterface(UuidOf<ID3D12Debug>(), debug_ctrl.PutVoid())))
            {
                debug_ctrl->EnableDebugLayer();
            }
            else
            {
                ::OutputDebugStringW(L"WARNING: Direct3D Debug Device is not available\n");
            }

            ComPtr<IDXGIInfoQueue> dxgi_info_queue;
            if (SUCCEEDED(::DXGIGetDebugInterface1(0, UuidOf<IDXGIInfoQueue>(), dxgi_info_queue.PutVoid())))
            {
                debug_dxgi = true;

                TIFHR(::CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, UuidOf<IDXGIFactory4>(), dxgi_factory_.PutVoid()));

                dxgi_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
                dxgi_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
            }
        }
#endif

        if (!debug_dxgi)
        {
            TIFHR(::CreateDXGIFactory2(0, UuidOf<IDXGIFactory4>(), dxgi_factory_.PutVoid()));
        }

        {
            tearing_supported_ = false;

            if (ComPtr<IDXGIFactory5> factory5 = dxgi_factory_.TryAs<IDXGIFactory5>())
            {
                BOOL allow_tearing = FALSE;
                HRESULT hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));
                if (SUCCEEDED(hr) && allow_tearing)
                {
                    tearing_supported_ = true;
                }
            }
        }

        ComPtr<IDXGIAdapter1> adapter;
        {
            ComPtr<IDXGIFactory6> factory6 = dxgi_factory_.As<IDXGIFactory6>();

            uint32_t adapter_id = 0;
            while (factory6->EnumAdapterByGpuPreference(adapter_id, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, UuidOf<IDXGIAdapter1>(),
                       adapter.PutVoid()) != DXGI_ERROR_NOT_FOUND)
            {
                DXGI_ADAPTER_DESC1 desc;
                TIFHR(adapter->GetDesc1(&desc));

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    continue;
                }

                if (SUCCEEDED(::D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, UuidOf<ID3D12Device5>(), device_.PutVoid())))
                {
                    break;
                }

                ++adapter_id;
            }
        }

#ifdef _DEBUG
        if (!adapter)
        {
            TIFHR(dxgi_factory_->EnumWarpAdapter(UuidOf<IDXGIAdapter1>(), adapter.PutVoid()));

            TIFHR(::D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, UuidOf<ID3D12Device5>(), device_.PutVoid()));
        }
#endif

        Verify(adapter && device_);

#ifdef _DEBUG
        if (ComPtr<ID3D12InfoQueue> d3d_info_queue = device_.TryAs<ID3D12InfoQueue>())
        {
            d3d_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            d3d_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        }
#endif

        D3D12_COMMAND_QUEUE_DESC const queue_qesc{D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0};
        TIFHR(device_->CreateCommandQueue(&queue_qesc, UuidOf<ID3D12CommandQueue>(), cmd_queue_.PutVoid()));

        D3D12_DESCRIPTOR_HEAP_DESC const rtv_descriptor_heap_desc{
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV, FrameCount, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
        TIFHR(device_->CreateDescriptorHeap(&rtv_descriptor_heap_desc, UuidOf<ID3D12DescriptorHeap>(), rtv_descriptor_heap_.PutVoid()));

        rtv_descriptor_size_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        for (auto& allocator : cmd_allocators_)
        {
            TIFHR(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, UuidOf<ID3D12CommandAllocator>(), allocator.PutVoid()));
        }

        TIFHR(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_allocators_[0].Get(), nullptr,
            UuidOf<ID3D12GraphicsCommandList4>(), cmd_list_.PutVoid()));
        TIFHR(cmd_list_->Close());

        TIFHR(device_->CreateFence(fence_vals_[frame_index_], D3D12_FENCE_FLAG_NONE, UuidOf<ID3D12Fence>(), fence_.PutVoid()));
        ++fence_vals_[frame_index_];

        fence_event_ = MakeWin32UniqueHandle(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
        Verify(fence_event_.get() != INVALID_HANDLE_VALUE);
    }

    void GoldenSunApp::CreateWindowSizeDependentResources()
    {
        this->WaitForGpu();

        for (uint32_t n = 0; n < FrameCount; ++n)
        {
            render_targets_[n].Reset();
            fence_vals_[n] = fence_vals_[frame_index_];
        }

        uint32_t const back_buffer_width = std::max(width_, 1U);
        uint32_t const back_buffer_height = std::max(height_, 1U);
        DXGI_FORMAT const back_buffer_fmt = LinearFormatOf(back_buffer_fmt_);

        if (swap_chain_)
        {
            HRESULT hr = swap_chain_->ResizeBuffers(FrameCount, back_buffer_width, back_buffer_height, back_buffer_fmt,
                tearing_supported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
            if ((hr == DXGI_ERROR_DEVICE_REMOVED) || (hr == DXGI_ERROR_DEVICE_RESET))
            {
                this->HandleDeviceLost();
                return;
            }
            else
            {
                TIFHR(hr);
            }
        }
        else
        {
            DXGI_SWAP_CHAIN_DESC1 const swap_chain_desc{
                back_buffer_width,
                back_buffer_height,
                back_buffer_fmt,
                FALSE,
                {1, 0},
                DXGI_USAGE_RENDER_TARGET_OUTPUT,
                FrameCount,
                DXGI_SCALING_STRETCH,
                DXGI_SWAP_EFFECT_FLIP_DISCARD,
                DXGI_ALPHA_MODE_IGNORE,
                tearing_supported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0U,
            };

            DXGI_SWAP_CHAIN_FULLSCREEN_DESC const fs_swap_chain_desc{
                {0, 0},
                DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED,
                DXGI_MODE_SCALING_UNSPECIFIED,
                TRUE,
            };

            ComPtr<IDXGISwapChain1> swap_chain;
            TIFHR(dxgi_factory_->CreateSwapChainForHwnd(
                cmd_queue_.Get(), window_->Hwnd(), &swap_chain_desc, &fs_swap_chain_desc, nullptr, swap_chain.Put()));
            swap_chain_ = swap_chain.As<IDXGISwapChain3>();

            dxgi_factory_->MakeWindowAssociation(window_->Hwnd(), DXGI_MWA_NO_ALT_ENTER);
        }

        for (uint32_t n = 0; n < FrameCount; ++n)
        {
            TIFHR(swap_chain_->GetBuffer(n, UuidOf<ID3D12Resource>(), render_targets_[n].PutVoid()));

            std::wostringstream ss;
            ss << L"Render target " << n;
            render_targets_[n]->SetName(ss.str().c_str());

            D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
            rtv_desc.Format = back_buffer_fmt_;
            rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

            D3D12_CPU_DESCRIPTOR_HANDLE const rtv_descriptor{
                rtv_descriptor_heap_->GetCPUDescriptorHandleForHeapStart().ptr + n * rtv_descriptor_size_};
            device_->CreateRenderTargetView(render_targets_[n].Get(), &rtv_desc, rtv_descriptor);
        }

        frame_index_ = swap_chain_->GetCurrentBackBufferIndex();
    }

    void GoldenSunApp::HandleDeviceLost()
    {
        golden_sun_engine_->RenderTarget(0, 0, back_buffer_fmt_);

        for (uint32_t n = 0; n < FrameCount; ++n)
        {
            cmd_allocators_[n].Reset();
            render_targets_[n].Reset();
        }

        cmd_queue_.Reset();
        cmd_list_.Reset();
        fence_.Reset();
        rtv_descriptor_heap_.Reset();
        swap_chain_.Reset();
        device_.Reset();
        dxgi_factory_.Reset();

        this->CreateDeviceResources();
        this->CreateWindowSizeDependentResources();

        golden_sun_engine_->RenderTarget(width_, height_, back_buffer_fmt_);
    }

    void GoldenSunApp::ExecuteCommandList()
    {
        TIFHR(cmd_list_->Close());

        ID3D12CommandList* cmd_lists[] = {cmd_list_.Get()};
        cmd_queue_->ExecuteCommandLists(static_cast<uint32_t>(std::size(cmd_lists)), cmd_lists);
    }

    void GoldenSunApp::WaitForGpu() noexcept
    {
        if (cmd_queue_ && fence_ && (fence_event_.get() != INVALID_HANDLE_VALUE))
        {
            uint64_t const fence_value = fence_vals_[frame_index_];
            if (SUCCEEDED(cmd_queue_->Signal(fence_.Get(), fence_value)))
            {
                if (SUCCEEDED(fence_->SetEventOnCompletion(fence_value, fence_event_.get())))
                {
                    ::WaitForSingleObjectEx(fence_event_.get(), INFINITE, FALSE);
                    ++fence_vals_[frame_index_];
                }
            }
        }
    }

    void GoldenSunApp::FrameStats()
    {
        ++total_num_frames_;

        frame_time_ = static_cast<float>(timer_.Elapsed());
        ++num_frames_;
        accumulate_time_ += frame_time_;
        app_time_ += frame_time_;

        if (accumulate_time_ >= 1)
        {
            float const fps = num_frames_ / accumulate_time_;

            accumulate_time_ = 0;
            num_frames_ = 0;

            float const rays_per_second = width_ * height_ * fps;

            std::wostringstream ss;
            ss << std::setprecision(2) << fixed << WindowTitle << L": " << fps << L"Hz, " << rays_per_second / 1e6f << L" M Primary Rays/s";
            window_->WindowText(ss.str());
        }

        timer_.Restart();
    }
} // namespace GoldenSun
