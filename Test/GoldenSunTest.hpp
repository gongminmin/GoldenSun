#pragma once

#define INITGUID

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include <GoldenSun/ComPtr.hpp>
#include <GoldenSun/SmartPtrHelper.hpp>
#include <GoldenSun/Util.hpp>

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

        ID3D12Device5* Device() const noexcept
        {
            return device_.Get();
        }
        ID3D12CommandQueue* CommandQueue() const noexcept
        {
            return cmd_queue_.Get();
        }
        ID3D12CommandAllocator* CommandAllocator() const noexcept
        {
            return cmd_allocators_[frame_index_].Get();
        }
        ID3D12GraphicsCommandList4* CommandList() const noexcept
        {
            return cmd_list_.Get();
        }

        void BeginFrame();
        void EndFrame();

        void ExecuteCommandList();
        void WaitForGpu();

        ComPtr<ID3D12Resource> LoadTexture(std::string const& file_name);
        void SaveTexture(ID3D12Resource* texture, std::string const& file_name);
        ComPtr<ID3D12Resource> CloneTexture(ID3D12Resource* texture);
        void CopyTexture(ID3D12Resource* src_texture, ID3D12Resource* dst_texture);

        void UploadTexture(ID3D12Resource* texture, uint8_t const* data);
        std::vector<uint8_t> ReadBackTexture(ID3D12Resource* texture);

        struct CompareResult
        {
            bool size_unmatch;
            bool format_unmatch;
            float channel_errors[4];

            ComPtr<ID3D12Resource> error_image;
        };
        CompareResult CompareImages(ID3D12Resource* expected_image, ID3D12Resource* actual_image, float channel_tolerance = 1 / 255.0f);

        void CompareWithExpected(std::string const& expected_name, ID3D12Resource* actual_image, float channel_tolerance = 1 / 255.0f);

    private:
        std::string expected_dir_;
        std::string result_dir_;

        ComPtr<IDXGIFactory4> dxgi_factory_;
        ComPtr<ID3D12Device5> device_;
        ComPtr<ID3D12CommandQueue> cmd_queue_;
        ComPtr<ID3D12GraphicsCommandList4> cmd_list_;
        ComPtr<ID3D12CommandAllocator> cmd_allocators_[FrameCount];

        ComPtr<ID3D12Fence> fence_;
        uint64_t fence_vals_[FrameCount]{};
        Win32UniqueHandle fence_event_;

        uint32_t frame_index_ = 0;
    };

    TestEnvironment& TestEnv();
} // namespace GoldenSun
