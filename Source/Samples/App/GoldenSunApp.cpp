#include "pch.hpp"

#include "GoldenSunApp.hpp"

#include <GoldenSun/MeshHelper.hpp>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

using namespace std;
using namespace DirectX;

namespace
{
    std::wstring const WindowTitle = L"Golden Sun";

    XMFLOAT3 constexpr init_eye = {0.0f, 1.0f, -3.0f};
    XMFLOAT3 constexpr init_look_at = {0.0f, 0.0f, 0.0f};
    XMFLOAT3 constexpr init_up = {0.0f, 1.0f, 0.0f};

    float constexpr init_fov = XMConvertToRadians(45);
    float constexpr init_near_plane = 0.1f;
    float constexpr init_far_plane = 20;

    XMFLOAT3 constexpr init_light_pos = {0.0f, 1.8f, -3.0f};
    XMFLOAT3 constexpr init_light_color = {20.0f, 20.0f, 20.0f};
    XMFLOAT3 constexpr init_light_falloff = {1, 0, 1};
    bool constexpr init_light_shadowing = true;
} // namespace

namespace GoldenSun
{
    GoldenSunApp::GoldenSunApp(uint32_t width, uint32_t height)
        : asset_dir_(ExeDirectory() + "Assets/"), width_(width), height_(height), window_(*this, WindowTitle)
    {
        this->CreateDeviceResources();
        this->CreateWindowSizeDependentResources();

        auto* d3d12_device = gpu_system_.NativeDeviceHandle<D3D12Traits>();
        auto* d3d12_cmd_queue = gpu_system_.NativeCommandQueueHandle<D3D12Traits>();

        golden_sun_engine_ = Engine(d3d12_device, d3d12_cmd_queue);
        golden_sun_engine_.RenderTarget(width_, height_, back_buffer_fmt_);

        this->InitializeScene();

        window_.ShowWindow(SW_SHOWNORMAL);

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
            camera_.Eye() = init_eye;
            camera_.LookAt() = init_look_at;
            camera_.Up() = init_up;

            camera_.Fov() = init_fov;
            camera_.NearPlane() = init_near_plane;
            camera_.FarPlane() = init_far_plane;

            golden_sun_engine_.Camera(camera_);
        }
        {
            light_.Position() = init_light_pos;
            light_.Color() = init_light_color;
            light_.Falloff() = init_light_falloff;
            light_.Shadowing() = init_light_shadowing;

            golden_sun_engine_.Lights(&light_, 1);
        }

        meshes_ = LoadMesh(gpu_system_, asset_dir_ + "DamagedHelmet/DamagedHelmet.gltf");
        golden_sun_engine_.Meshes(meshes_.data(), static_cast<uint32_t>(meshes_.size()));
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
            float const rotate = XMConvertToRadians(360.0f * (app_time_ / seconds_to_rotate_around));
            XMMATRIX const rotate_mat = XMMatrixRotationY(rotate);
            XMStoreFloat3(&camera_.Eye(), XMVector3Transform(XMLoadFloat3(&init_eye), rotate_mat));

            golden_sun_engine_.Camera(camera_);
        }
        {
            float const seconds_to_rotate_around = 8.0f;
            float const rotate = XMConvertToRadians(-360.0f * (app_time_ / seconds_to_rotate_around));
            XMMATRIX const rotate_mat = XMMatrixRotationZ(rotate) * XMMatrixRotationY(rotate);
            XMStoreFloat3(&light_.Position(), XMVector3Transform(XMLoadFloat3(&init_light_pos), rotate_mat));

            golden_sun_engine_.Lights(&light_, 1);
        }

        if (window_visible_)
        {
            auto cmd_list = gpu_system_.CreateCommandList();

            auto* d3d12_cmd_list = cmd_list.NativeHandle<D3D12Traits>();
            golden_sun_engine_.Render(d3d12_cmd_list);

            auto& render_target = render_targets_[gpu_system_.FrameIndex()];
            GpuTexture2D ray_tracing_output(golden_sun_engine_.Output(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

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

            golden_sun_engine_.RenderTarget(width_, height_, back_buffer_fmt_);
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
            swap_chain_ = gpu_system_.CreateSwapChain(window_.Hwnd(), back_buffer_width, back_buffer_height, back_buffer_fmt);
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
        golden_sun_engine_.RenderTarget(0, 0, back_buffer_fmt_);

        for (auto& render_target : render_targets_)
        {
            render_target.Reset();
        }

        rtv_desc_block_.Reset();
        swap_chain_.Reset();

        gpu_system_.HandleDeviceLost();

        this->CreateDeviceResources();
        this->CreateWindowSizeDependentResources();

        golden_sun_engine_.RenderTarget(width_, height_, back_buffer_fmt_);
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
            window_.WindowText(ss.str());
        }

        timer_.Restart();
    }
} // namespace GoldenSun
