#include "pch.hpp"

#include "GoldenSunApp.hpp"

#include <GoldenSun/MeshHelper.hpp>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <string>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

using namespace std;
using namespace DirectX;

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

        auto* d3d12_device = reinterpret_cast<ID3D12Device5*>(gpu_system_.NativeDevice());
        auto* d3d12_cmd_queue = reinterpret_cast<ID3D12CommandQueue*>(gpu_system_.NativeCommandQueue());

        golden_sun_engine_ = std::make_unique<Engine>(d3d12_device, d3d12_cmd_queue);
        golden_sun_engine_->RenderTarget(width_, height_, back_buffer_fmt_);

        this->InitializeScene();

        window_->ShowWindow(SW_SHOWNORMAL);

        (void)::DXGIDeclareAdapterRemovalSupport();
    }

    GoldenSunApp::~GoldenSunApp()
    {
        gpu_system_.WaitForGpu();
    }

    int GoldenSunApp::Run()
    {
        bool got_msg;
        MSG msg{};
        while (msg.message != WM_QUIT)
        {
            if (active_)
            {
                got_msg = ::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE) != 0;
            }
            else
            {
                got_msg = ::GetMessage(&msg, nullptr, 0, 0) != 0;
            }

            if (got_msg)
            {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
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

        meshes_ = LoadMesh(gpu_system_, asset_dir_ + "DamagedHelmet/DamagedHelmet.gltf");
        golden_sun_engine_->Geometries(meshes_.data(), static_cast<uint32_t>(meshes_.size()));
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
            auto cmd_list = gpu_system_.CreateCommandList();

            auto* d3d12_cmd_list = reinterpret_cast<ID3D12GraphicsCommandList4*>(cmd_list.NativeCommandList());
            golden_sun_engine_->Render(d3d12_cmd_list);

            auto& render_target = render_targets_[gpu_system_.FrameIndex()];
            GpuTexture2D ray_tracing_output(golden_sun_engine_->Output(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            render_target.Transition(cmd_list, D3D12_RESOURCE_STATE_COPY_DEST);
            ray_tracing_output.Transition(cmd_list, D3D12_RESOURCE_STATE_GENERIC_READ);

            cmd_list.Copy(render_target, ray_tracing_output);

            render_target.Transition(cmd_list, D3D12_RESOURCE_STATE_PRESENT);
            ray_tracing_output.Transition(cmd_list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            gpu_system_.Execute(std::move(cmd_list));

            if (!swap_chain_.Present(0))
            {
                this->HandleDeviceLost();
            }
            else
            {
                gpu_system_.MoveToNextFrame();
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
        gpu_system_.ReallocRtvDescBlock(rtv_desc_block_, GpuSystem::FrameCount());
        rtv_descriptor_size_ = gpu_system_.RtvDescSize();
    }

    void GoldenSunApp::CreateWindowSizeDependentResources()
    {
        gpu_system_.WaitForGpu();

        for (auto& render_target : render_targets_)
        {
            render_target.Reset();
        }

        gpu_system_.ResetFenceValues();

        uint32_t const back_buffer_width = std::max(width_, 1U);
        uint32_t const back_buffer_height = std::max(height_, 1U);
        DXGI_FORMAT const back_buffer_fmt = LinearFormatOf(back_buffer_fmt_);

        if (swap_chain_)
        {
            if (!swap_chain_.Resize(back_buffer_width, back_buffer_height, back_buffer_fmt))
            {
                this->HandleDeviceLost();
                return;
            }
        }
        else
        {
            swap_chain_ = gpu_system_.CreateSwapChain(window_->Hwnd(), back_buffer_width, back_buffer_height, back_buffer_fmt);
        }

        for (uint32_t i = 0; i < GpuSystem::FrameCount(); ++i)
        {
            render_targets_[i] = swap_chain_.Buffer(i);

            gpu_system_.CreateRenderTargetView(
                render_targets_[i], back_buffer_fmt_, OffsetHandle(rtv_desc_block_.CpuHandle(), i, rtv_descriptor_size_));
        }
    }

    void GoldenSunApp::HandleDeviceLost()
    {
        golden_sun_engine_->RenderTarget(0, 0, back_buffer_fmt_);

        for (auto& render_target : render_targets_)
        {
            render_target.Reset();
        }

        rtv_desc_block_.Reset();
        swap_chain_.Reset();

        gpu_system_.HandleDeviceLost();

        this->CreateDeviceResources();
        this->CreateWindowSizeDependentResources();

        golden_sun_engine_->RenderTarget(width_, height_, back_buffer_fmt_);
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
