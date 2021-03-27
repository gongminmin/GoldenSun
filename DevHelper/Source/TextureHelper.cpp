#include "pch.hpp"

#include <GoldenSun/TextureHelper.hpp>

#include <GoldenSun/Util.hpp>

#include <iostream>

#include <d3d12.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

using namespace GoldenSun;
using namespace std;

namespace GoldenSun
{
    GpuTexture2D LoadTexture(GpuSystem& gpu_system, std::string_view file_name, DXGI_FORMAT format)
    {
        GpuTexture2D ret;

        int width, height;
        uint8_t* data = stbi_load(std::string(file_name).c_str(), &width, &height, nullptr, 4);
        if (data != nullptr)
        {
            ret = gpu_system.CreateTexture2D(
                width, height, 1, format, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
            auto cmd_list = gpu_system.CreateCommandList();
            ret.Upload(gpu_system, cmd_list, 0, data);
            gpu_system.Execute(std::move(cmd_list));

            stbi_image_free(data);
        }

        return ret;
    }

    void SaveTexture(GpuSystem& gpu_system, GpuTexture2D const& texture, std::string_view file_name)
    {
        assert(texture);

        uint32_t const width = texture.Width(0);
        uint32_t const height = texture.Height(0);
        uint32_t const format_size = FormatSize(texture.Format());

        std::vector<uint8_t> data(width * height * format_size);
        auto cmd_list = gpu_system.CreateCommandList();
        texture.Readback(gpu_system, cmd_list, 0, data.data());
        gpu_system.Execute(std::move(cmd_list));

        stbi_write_png(std::string(file_name).c_str(), static_cast<int>(width), static_cast<int>(height), 4, data.data(),
            static_cast<int>(width * format_size));
    }
} // namespace GoldenSun
