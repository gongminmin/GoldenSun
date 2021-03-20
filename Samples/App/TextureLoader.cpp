#include "pch.hpp"

#include "TextureLoader.hpp"

#include <iostream>

#include <d3d12.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <GoldenSun/Util.hpp>

using namespace GoldenSun;
using namespace std;

namespace
{
    void UploadTexture(ID3D12Device5* device, ID3D12GraphicsCommandList4* cmd_list, ID3D12Resource* texture, void const* data)
    {
        auto const tex_desc = texture->GetDesc();
        uint32_t const width = static_cast<uint32_t>(tex_desc.Width);
        uint32_t const height = tex_desc.Height;

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        uint32_t num_row = 0;
        uint64_t row_size_in_bytes = 0;
        uint64_t required_size = 0;
        device->GetCopyableFootprints(&tex_desc, 0, 1, 0, &layout, &num_row, &row_size_in_bytes, &required_size);

        GpuUploadBuffer upload_buffer(device, required_size, L"UploadBuffer");

        assert(row_size_in_bytes >= width * 4);

        uint8_t* tex_data = upload_buffer.MappedData<uint8_t>();
        for (uint32_t y = 0; y < height; ++y)
        {
            memcpy(tex_data + y * row_size_in_bytes, static_cast<uint8_t const*>(data) + y * width * 4, width * 4);
        }

        D3D12_TEXTURE_COPY_LOCATION src;
        src.pResource = upload_buffer.Resource();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layout;

        D3D12_TEXTURE_COPY_LOCATION dst;
        dst.pResource = texture;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;

        D3D12_BOX src_box;
        src_box.left = 0;
        src_box.top = 0;
        src_box.front = 0;
        src_box.right = width;
        src_box.bottom = height;
        src_box.back = 1;

        D3D12_RESOURCE_BARRIER barrier;
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = texture;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd_list->ResourceBarrier(1, &barrier);

        cmd_list->CopyTextureRegion(&dst, 0, 0, 0, &src, &src_box);

        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        cmd_list->ResourceBarrier(1, &barrier);

        // TODO: LEAK!! Cache the resource until the command list is executed
        upload_buffer.Resource()->AddRef();
    }
} // namespace

namespace GoldenSun
{
    ComPtr<ID3D12Resource> LoadTexture(
        ID3D12Device5* device, ID3D12GraphicsCommandList4* cmd_list, std::string_view file_name, DXGI_FORMAT format)
    {
        ComPtr<ID3D12Resource> ret;

        int width, height;
        uint8_t* data = stbi_load(std::string(file_name).c_str(), &width, &height, nullptr, 4);
        if (data != nullptr)
        {
            D3D12_HEAP_PROPERTIES const default_heap_prop = {
                D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};

            D3D12_RESOURCE_DESC const tex_desc = {D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, static_cast<uint64_t>(width),
                static_cast<uint32_t>(height), 1, 1, format, {1, 0}, D3D12_TEXTURE_LAYOUT_UNKNOWN,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};
            TIFHR(device->CreateCommittedResource(&default_heap_prop, D3D12_HEAP_FLAG_NONE, &tex_desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, UuidOf<ID3D12Resource>(), ret.PutVoid()));

            UploadTexture(device, cmd_list, ret.Get(), data);

            stbi_image_free(data);
        }

        return ret;
    }
} // namespace GoldenSun
