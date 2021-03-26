#pragma once

#include <GoldenSun/ComPtr.hpp>
#include <GoldenSun/Gpu/GpuSystem.hpp>
#include <GoldenSun/SmartPtrHelper.hpp>
#include <GoldenSun/Util.hpp>

#include <d3d12.h>
#include <dxgi1_6.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include <gtest/gtest.h>

namespace GoldenSun
{
    class TestEnvironment : public testing::Environment
    {
    public:
        void SetUp() override;
        void TearDown() override;

        std::string const& ExpectedDir() const noexcept
        {
            return expected_dir_;
        }

        GpuSystem& GpuSystem() noexcept
        {
            return gpu_system_;
        }

        GpuTexture2D LoadTexture(std::string const& file_name);
        void SaveTexture(GpuTexture2D const& texture, std::string const& file_name);
        GpuTexture2D CloneTexture(GpuTexture2D& texture);

        struct CompareResult
        {
            bool size_unmatch;
            bool format_unmatch;
            float channel_errors[4];

            GpuTexture2D error_image;
        };
        CompareResult CompareImages(GpuTexture2D& expected_image, GpuTexture2D& actual_image, float channel_tolerance = 1 / 255.0f);

        void CompareWithExpected(std::string const& expected_name, GpuTexture2D& actual_image, float channel_tolerance = 1 / 255.0f);

    private:
        std::string expected_dir_;
        std::string result_dir_;

        GoldenSun::GpuSystem gpu_system_;
    };

    TestEnvironment& TestEnv();
} // namespace GoldenSun
