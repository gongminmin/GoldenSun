#include "pch.hpp"

#include "GoldenSunTest.hpp"

#include <GoldenSun/ErrorHandling.hpp>
#include <GoldenSun/Util.hpp>
#include <GoldenSun/Uuid.hpp>

#include <filesystem>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "CompiledShaders/Comparator.h"

using namespace DirectX;
using namespace testing;

DEFINE_UUID_OF(ID3D12PipelineState);
DEFINE_UUID_OF(ID3D12RootSignature);

namespace
{
    GoldenSun::TestEnvironment* test_env;
}

namespace GoldenSun
{
    TestEnvironment& TestEnv()
    {
        return *test_env;
    }

    void TestEnvironment::SetUp()
    {
        {
            char exe_file[MAX_PATH];
            uint32_t size = ::GetModuleFileNameA(nullptr, exe_file, static_cast<uint32_t>(std::size(exe_file)));
            Verify((size != 0) && (size != std::size(exe_file)));

            std::filesystem::path exe_path = exe_file;
            expected_dir_ = (exe_path.parent_path() / "Test/Expected/").string();
            result_dir_ = (exe_path.parent_path() / "Test/Result/").string();
            if (!std::filesystem::exists(result_dir_))
            {
                std::filesystem::create_directory(result_dir_);
            }
        }
    }

    void TestEnvironment::TearDown()
    {
        gpu_system_.WaitForGpu();
    }

    GpuTexture2D TestEnvironment::LoadTexture(std::string const& file_name)
    {
        GpuTexture2D ret;

        int width, height;
        uint8_t* data = stbi_load(std::string(file_name).c_str(), &width, &height, nullptr, 4);
        if (data != nullptr)
        {
            ret = gpu_system_.CreateTexture2D(width, height, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_GENERIC_READ);
            auto cmd_list = gpu_system_.CreateCommandList();
            ret.Upload(gpu_system_, cmd_list, 0, data);
            gpu_system_.Execute(std::move(cmd_list));

            stbi_image_free(data);
        }

        return ret;
    }

    void TestEnvironment::SaveTexture(GpuTexture2D const& texture, std::string const& file_name)
    {
        assert(texture);

        uint32_t const width = texture.Width(0);
        uint32_t const height = texture.Height(0);
        uint32_t const format_size = FormatSize(texture.Format());

        std::vector<uint8_t> data(width * height * format_size);
        auto cmd_list = gpu_system_.CreateCommandList();
        texture.Readback(gpu_system_, cmd_list, 0, data.data());
        gpu_system_.Execute(std::move(cmd_list));

        stbi_write_png(
            file_name.c_str(), static_cast<int>(width), static_cast<int>(height), 4, data.data(), static_cast<int>(width * format_size));
    }

    GpuTexture2D TestEnvironment::CloneTexture(GpuTexture2D& texture)
    {
        GpuTexture2D ret = gpu_system_.CreateTexture2D(
            texture.Width(0), texture.Height(0), texture.MipLevels(), texture.Format(), texture.Flags(), D3D12_RESOURCE_STATE_COPY_DEST);

        auto cmd_list = gpu_system_.CreateCommandList();

        auto src_old_state = texture.State(0);
        texture.Transition(cmd_list, D3D12_RESOURCE_STATE_GENERIC_READ);

        cmd_list.Copy(ret, texture);

        texture.Transition(cmd_list, src_old_state);
        ret.Transition(cmd_list, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        gpu_system_.Execute(std::move(cmd_list));

        return ret;
    }

    TestEnvironment::CompareResult TestEnvironment::CompareImages(
        GpuTexture2D& expected_image, GpuTexture2D& actual_image, float channel_tolerance)
    {
        TestEnvironment::CompareResult ret{};

        if ((expected_image.Width(0) != actual_image.Width(0)) || (expected_image.Height(0) != actual_image.Height(0)))
        {
            ret.size_unmatch = true;
        }
        if (expected_image.Format() != actual_image.Format())
        {
            ret.format_unmatch = true;
        }

        if (!ret.size_unmatch && !ret.format_unmatch)
        {
            ret.error_image = gpu_system_.CreateTexture2D(expected_image.Width(0), expected_image.Height(0), 1, DXGI_FORMAT_R8G8B8A8_UNORM,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ErrorImage");

            auto channel_error_buff = gpu_system_.CreateDefaultBuffer(
                sizeof(XMUINT4), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ChannelErrorBuffer");
            auto channel_error_cpu_buff = gpu_system_.CreateReadbackBuffer(sizeof(XMUINT4), L"ChannelErrorCpuBuffer");

            // expected_image, actual_image, result_image, error_buffer
            auto desc_block = gpu_system_.AllocCbvSrvUavDescBlock(4);
            uint32_t const descriptor_size = gpu_system_.CbvSrvUavDescSize();

            auto const cpu_desc_handle_base = desc_block.CpuHandle();
            auto const gpu_desc_handle_base = desc_block.GpuHandle();

            auto const expected_srv_gpu_desc_handle = OffsetHandle(gpu_desc_handle_base, 0, descriptor_size);
            auto const result_uav_cpu_desc_handle = OffsetHandle(gpu_desc_handle_base, 2, descriptor_size);

            gpu_system_.CreateShaderResourceView(expected_image, OffsetHandle(cpu_desc_handle_base, 0, descriptor_size));
            gpu_system_.CreateShaderResourceView(actual_image, OffsetHandle(cpu_desc_handle_base, 1, descriptor_size));
            gpu_system_.CreateUnorderedAccessView(ret.error_image, OffsetHandle(cpu_desc_handle_base, 2, descriptor_size));
            gpu_system_.CreateUnorderedAccessView(
                channel_error_buff, 0, 1, sizeof(XMUINT4), OffsetHandle(cpu_desc_handle_base, 3, descriptor_size));

            struct ComparatorConstantBuffer
            {
                XMUINT2 width_height;
                float channel_tolerance;
            };

            ConstantBuffer<ComparatorConstantBuffer> comparator_cb(gpu_system_, 1, L"ComparatorCb");
            {
                comparator_cb->width_height = {expected_image.Width(0), expected_image.Height(0)};
                comparator_cb->channel_tolerance = channel_tolerance;
                comparator_cb.UploadToGpu();
            }

            auto* d3d12_device = reinterpret_cast<ID3D12Device*>(gpu_system_.NativeDevice());

            ComPtr<ID3D12RootSignature> root_sig;
            {
                D3D12_DESCRIPTOR_RANGE const ranges[] = {
                    {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},
                    {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND},
                };

                D3D12_ROOT_PARAMETER const root_params[] = {
                    CreateRootParameterAsDescriptorTable(&ranges[0], 1),
                    CreateRootParameterAsDescriptorTable(&ranges[1], 1),
                    CreateRootParameterAsConstantBufferView(0),
                };

                D3D12_ROOT_SIGNATURE_DESC const root_signature_desc = {
                    static_cast<uint32_t>(std::size(root_params)), root_params, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};

                ComPtr<ID3DBlob> blob;
                ComPtr<ID3DBlob> error;
                HRESULT hr = ::D3D12SerializeRootSignature(&root_signature_desc, D3D_ROOT_SIGNATURE_VERSION_1, blob.Put(), error.Put());
                if (FAILED(hr))
                {
                    ::OutputDebugStringW(
                        (std::wstring(L"D3D12SerializeRootSignature failed: ") + static_cast<wchar_t*>(error->GetBufferPointer()) + L"\n")
                            .c_str());
                    TIFHR(hr);
                }

                TIFHR(d3d12_device->CreateRootSignature(
                    1, blob->GetBufferPointer(), blob->GetBufferSize(), UuidOf<ID3D12RootSignature>(), root_sig.PutVoid()));
            }

            ComPtr<ID3D12PipelineState> pso;
            {
                D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc;
                pso_desc.pRootSignature = root_sig.Get();
                pso_desc.CS.pShaderBytecode = Comparator_shader;
                pso_desc.CS.BytecodeLength = sizeof(Comparator_shader);
                pso_desc.NodeMask = 0;
                pso_desc.CachedPSO.pCachedBlob = nullptr;
                pso_desc.CachedPSO.CachedBlobSizeInBytes = 0;
                pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

                TIFHR(d3d12_device->CreateComputePipelineState(&pso_desc, UuidOf<ID3D12PipelineState>(), pso.PutVoid()));
            }

            auto cmd_list = gpu_system_.CreateCommandList();
            auto* d3d12_cmd_list = reinterpret_cast<ID3D12GraphicsCommandList4*>(cmd_list.NativeCommandList());

            d3d12_cmd_list->SetComputeRootSignature(root_sig.Get());
            d3d12_cmd_list->SetPipelineState(pso.Get());

            ID3D12DescriptorHeap* heaps[] = {reinterpret_cast<ID3D12DescriptorHeap*>(desc_block.NativeDescriptorHeap())};
            d3d12_cmd_list->SetDescriptorHeaps(static_cast<uint32_t>(std::size(heaps)), heaps);
            d3d12_cmd_list->SetComputeRootDescriptorTable(0, expected_srv_gpu_desc_handle);
            d3d12_cmd_list->SetComputeRootDescriptorTable(1, result_uav_cpu_desc_handle);
            d3d12_cmd_list->SetComputeRootConstantBufferView(2, comparator_cb.GpuVirtualAddress());

            auto old_state = actual_image.State(0);
            actual_image.Transition(cmd_list, D3D12_RESOURCE_STATE_GENERIC_READ);

            uint32_t constexpr TILE_DIM = 16;
            d3d12_cmd_list->Dispatch(
                (expected_image.Width(0) + (TILE_DIM - 1)) / TILE_DIM, (expected_image.Height(0) + (TILE_DIM - 1)) / TILE_DIM, 1);

            actual_image.Transition(cmd_list, old_state);
            channel_error_buff.Transition(cmd_list, D3D12_RESOURCE_STATE_GENERIC_READ);

            cmd_list.Copy(channel_error_cpu_buff, channel_error_buff);

            gpu_system_.Execute(std::move(cmd_list));
            gpu_system_.WaitForGpu();

            uint32_t channel_errors[4];
            memcpy(channel_errors, channel_error_cpu_buff.MappedData<uint32_t>(), sizeof(XMUINT4));

            bool match = true;
            float const scale = 1.0f / 255.0f;
            for (size_t i = 0; i < std::size(channel_errors); ++i)
            {
                if (channel_errors[i] > 0)
                {
                    match = false;
                }

                ret.channel_errors[i] = channel_errors[i] * scale;
            }

            if (match)
            {
                ret.error_image.Reset();
            }

            gpu_system_.DeallocCbvSrvUavDescBlock(std::move(desc_block));
        }

        return ret;
    }

    void TestEnvironment::CompareWithExpected(std::string const& expected_name, GpuTexture2D& actual_image, float channel_tolerance)
    {
        auto expected_image = this->LoadTexture(expected_dir_ + expected_name + ".png");

        {
            std::filesystem::path leaf_dir = result_dir_ + expected_name;
            leaf_dir = leaf_dir.parent_path();
            if (!std::filesystem::exists(leaf_dir))
            {
                std::filesystem::create_directory(leaf_dir);
            }
        }

        if (expected_image)
        {
            auto result = this->CompareImages(expected_image, actual_image, channel_tolerance);
            if (result.error_image)
            {
                this->SaveTexture(expected_image, result_dir_ + expected_name + "_expected.png");
                this->SaveTexture(actual_image, result_dir_ + expected_name + "_actual.png");
                this->SaveTexture(result.error_image, result_dir_ + expected_name + "_diff.png");
            }

            EXPECT_FALSE(result.format_unmatch);
            EXPECT_FALSE(result.size_unmatch);

            EXPECT_LT(result.channel_errors[0], 1e-6f);
            EXPECT_LT(result.channel_errors[1], 1e-6f);
            EXPECT_LT(result.channel_errors[2], 1e-6f);
            EXPECT_LT(result.channel_errors[3], 1e-6f);

            EXPECT_FALSE(result.error_image);
        }
        else
        {
            std::string const expected_file = result_dir_ + expected_name + ".png";
            std::cout << "Saving expected image to " << expected_file << '\n';
            this->SaveTexture(actual_image, expected_file.c_str());
        }
    }
} // namespace GoldenSun

int main(int argc, char** argv)
{
    InitGoogleTest(&argc, argv);
    test_env = static_cast<GoldenSun::TestEnvironment*>(AddGlobalTestEnvironment(new GoldenSun::TestEnvironment));

    int ret_val = RUN_ALL_TESTS();
    if (ret_val != 0)
    {
        [[maybe_unused]] int ch = getchar();
    }

    return ret_val;
}
