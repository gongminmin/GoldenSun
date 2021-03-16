#include "GoldenSunTest.hpp"

#include <gtest/gtest.h>

using namespace GoldenSun;

TEST(TestFrameworkTest, ExactMatch)
{
    auto& test_env = TestEnv();

    auto image = test_env.LoadTexture(EXPECTED_DIR "TestFrameworkTest/SIPI_Jelly_Beans_4.1.07.jpg");

    auto result = test_env.CompareImages(image.Get(), image.Get(), 0.0f);

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

    auto expected_image = test_env.LoadTexture(EXPECTED_DIR "TestFrameworkTest/SIPI_Jelly_Beans_4.1.07.jpg");
    auto actual_image = test_env.CloneTexture(expected_image.Get());

    uint32_t constexpr sabotage_x = 53;
    uint32_t constexpr sabotage_y = 37;
    uint32_t constexpr sabotage_ch = 0;
    uint8_t constexpr delta = 4;

    {
        std::vector<uint8_t> tex_data = test_env.ReadBackTexture(actual_image.Get());
        tex_data[(sabotage_y * actual_image->GetDesc().Width + sabotage_x) * 4 + sabotage_ch] += delta;
        test_env.UploadTexture(actual_image.Get(), tex_data.data());
    }

    auto result = test_env.CompareImages(expected_image.Get(), actual_image.Get(), delta / 255.0f);

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

    auto expected_image = test_env.LoadTexture(EXPECTED_DIR "TestFrameworkTest/SIPI_Jelly_Beans_4.1.07.jpg");
    auto actual_image = test_env.CloneTexture(expected_image.Get());

    uint32_t constexpr sabotage_x = 53;
    uint32_t constexpr sabotage_y = 37;
    uint32_t constexpr sabotage_ch = 1;
    uint8_t constexpr delta = 4;

    {
        std::vector<uint8_t> tex_data = test_env.ReadBackTexture(actual_image.Get());
        tex_data[(sabotage_y * actual_image->GetDesc().Width + sabotage_x) * 4 + sabotage_ch] += delta;
        test_env.UploadTexture(actual_image.Get(), tex_data.data());
    }

    auto result = test_env.CompareImages(expected_image.Get(), actual_image.Get(), (delta - 1) / 255.0f);

    EXPECT_FALSE(result.format_unmatch);
    EXPECT_FALSE(result.size_unmatch);

    EXPECT_LT(result.channel_errors[0], 1e-6f);
    EXPECT_LT(abs(result.channel_errors[1] - delta / 255.0f), 1e-6f);
    EXPECT_LT(result.channel_errors[2], 1e-6f);
    EXPECT_LT(result.channel_errors[3], 1e-6f);

    EXPECT_TRUE(result.error_image);

    {
        std::vector<uint8_t> error_data = test_env.ReadBackTexture(result.error_image.Get());
        auto const error_desc = result.error_image->GetDesc();
        for (size_t i = 0; i < error_data.size(); ++i)
        {
            if (i == (sabotage_y * result.error_image->GetDesc().Width + sabotage_x) * 4 + sabotage_ch)
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
