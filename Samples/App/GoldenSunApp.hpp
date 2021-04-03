#pragma once

#include <GoldenSun/GoldenSun.hpp>
#include <GoldenSun/Gpu/GpuSystem.hpp>
#include <GoldenSun/SmartPtrHelper.hpp>
#include <GoldenSun/Util.hpp>

#include <string>

#include "Timer.hpp"
#include "WindowWin32.hpp"

namespace GoldenSun
{
    class GoldenSunApp
    {
    public:
        GoldenSunApp(uint32_t width, uint32_t height);
        ~GoldenSunApp();

        int Run();

        void Active(bool active);
        void Refresh();
        void SizeChanged(uint32_t width, uint32_t height, bool minimized);

        uint32_t Width() const noexcept
        {
            return width_;
        }
        uint32_t Height() const noexcept
        {
            return height_;
        }

    private:
        void InitializeScene();

        void CreateDeviceResources();
        void CreateWindowSizeDependentResources();
        void HandleDeviceLost();

        void FrameStats();

    private:
        std::string asset_dir_;

        uint32_t width_;
        uint32_t height_;
        bool active_ = false;
        bool window_visible_ = true;
        WindowWin32 window_;

        GpuSystem gpu_system_;
        GpuSwapChain swap_chain_;
        GpuTexture2D render_targets_[GpuSystem::FrameCount()];

        GpuDescriptorBlock rtv_desc_block_;
        uint32_t rtv_descriptor_size_;

        DXGI_FORMAT back_buffer_fmt_ = DXGI_FORMAT_R8G8B8A8_UNORM;
        bool tearing_supported_;

        uint32_t total_num_frames_ = 0;
        float accumulate_time_ = 0;
        uint32_t num_frames_ = 0;

        Timer timer_;
        float app_time_ = 0;
        float frame_time_ = 0;

        std::vector<Mesh> meshes_;

        Camera camera_;

        PointLight light_;

        Engine golden_sun_engine_;
    };
} // namespace GoldenSun
