#include "pch.hpp"

#include <GoldenSun/TextureHelper.hpp>

#include "GoldenSunTest.hpp"

using namespace GoldenSun;

TEST(TestFrameworkTest, ExactMatch)
{
    auto& test_env = TestEnv();

    auto image = LoadTexture(
        test_env.GpuSystem(), test_env.ExpectedDir() + "TestFrameworkTest/SIPI_Jelly_Beans_4.1.07.jpg", DXGI_FORMAT_R8G8B8A8_UNORM);

    auto result = test_env.CompareImages(image, image, 0.0f);

    EXPECT_FALSE(result.format_unmatch);
    EXPECT_FALSE(result.size_unmatch);

    EXPECT_LT(result.channel_errors[0], 1e-6f);
    EXPECT_LT(result.channel_errors[1], 1e-6f);
    EXPECT_LT(result.channel_errors[2], 1e-6f);
    EXPECT_LT(result.channel_errors[3], 1e-6f);

    EXPECT_FALSE(result.error_image);
}

TEST(TestFrameworkTest, TolerantMatch)
{
    auto& test_env = TestEnv();

    auto expected_image = LoadTexture(
        test_env.GpuSystem(), test_env.ExpectedDir() + "TestFrameworkTest/SIPI_Jelly_Beans_4.1.07.jpg", DXGI_FORMAT_R8G8B8A8_UNORM);
    auto actual_image = test_env.CloneTexture(expected_image);

    uint32_t constexpr sabotage_x = 53;
    uint32_t constexpr sabotage_y = 37;
    uint32_t constexpr sabotage_ch = 0;
    uint8_t constexpr delta = 4;

    {
        auto& gpu_system = test_env.GpuSystem();
        auto cmd_list = gpu_system.CreateCommandList();
        std::vector<uint8_t> tex_data(actual_image.Width(0) * actual_image.Height(0) * FormatSize(actual_image.Format()));
        actual_image.Readback(gpu_system, cmd_list, 0, tex_data.data());
        tex_data[(sabotage_y * actual_image.Width(0) + sabotage_x) * 4 + sabotage_ch] += delta;
        actual_image.Upload(gpu_system, cmd_list, 0, tex_data.data());
        gpu_system.Execute(std::move(cmd_list));
    }

    auto result = test_env.CompareImages(expected_image, actual_image, delta / 255.0f);

    EXPECT_FALSE(result.format_unmatch);
    EXPECT_FALSE(result.size_unmatch);

    EXPECT_LT(result.channel_errors[0], 1e-6f);
    EXPECT_LT(result.channel_errors[1], 1e-6f);
    EXPECT_LT(result.channel_errors[2], 1e-6f);
    EXPECT_LT(result.channel_errors[3], 1e-6f);

    EXPECT_FALSE(result.error_image);
}

TEST(TestFrameworkTest, Unmatch)
{
    auto& test_env = TestEnv();

    auto expected_image = LoadTexture(
        test_env.GpuSystem(), test_env.ExpectedDir() + "TestFrameworkTest/SIPI_Jelly_Beans_4.1.07.jpg", DXGI_FORMAT_R8G8B8A8_UNORM);
    auto actual_image = test_env.CloneTexture(expected_image);

    uint32_t constexpr sabotage_x = 53;
    uint32_t constexpr sabotage_y = 37;
    uint32_t constexpr sabotage_ch = 1;
    uint8_t constexpr delta = 4;

    auto& gpu_system = test_env.GpuSystem();

    {
        auto cmd_list = gpu_system.CreateCommandList();
        std::vector<uint8_t> tex_data(actual_image.Width(0) * actual_image.Height(0) * FormatSize(actual_image.Format()));
        actual_image.Readback(gpu_system, cmd_list, 0, tex_data.data());
        tex_data[(sabotage_y * actual_image.Width(0) + sabotage_x) * 4 + sabotage_ch] += delta;
        actual_image.Upload(gpu_system, cmd_list, 0, tex_data.data());
        gpu_system.Execute(std::move(cmd_list));
    }

    auto result = test_env.CompareImages(expected_image, actual_image, (delta - 1) / 255.0f);

    EXPECT_FALSE(result.format_unmatch);
    EXPECT_FALSE(result.size_unmatch);

    EXPECT_LT(result.channel_errors[0], 1e-6f);
    EXPECT_LT(abs(result.channel_errors[1] - delta / 255.0f), 1e-6f);
    EXPECT_LT(result.channel_errors[2], 1e-6f);
    EXPECT_LT(result.channel_errors[3], 1e-6f);

    EXPECT_TRUE(result.error_image);

    {
        std::vector<uint8_t> error_data(
            result.error_image.Width(0) * result.error_image.Height(0) * FormatSize(result.error_image.Format()));
        auto cmd_list = gpu_system.CreateCommandList();
        result.error_image.Readback(gpu_system, cmd_list, 0, error_data.data());
        gpu_system.Execute(std::move(cmd_list));
        for (size_t i = 0; i < error_data.size(); ++i)
        {
            if (i == (sabotage_y * result.error_image.Width(0) + sabotage_x) * 4 + sabotage_ch)
            {
                EXPECT_LT(abs(error_data[i] - delta), 1e-6f);
            }
            else
            {
                EXPECT_LT(error_data[i], 1e-6f);
            }
        }
    }
}
