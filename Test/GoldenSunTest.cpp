#define INITGUID

#include "GoldenSunTest.hpp"

#include <DirectXMath.h>

#include <GoldenSun/ErrorHandling.hpp>
#include <GoldenSun/Util.hpp>
#include <GoldenSun/Uuid.hpp>

#include <iostream>

#include <gtest/gtest.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "CompiledShaders/Comparator.h"

using namespace DirectX;
using namespace testing;

DEFINE_UUID_OF(ID3D12CommandAllocator);
DEFINE_UUID_OF(ID3D12CommandQueue);
DEFINE_UUID_OF(ID3D12DescriptorHeap);
DEFINE_UUID_OF(ID3D12Device5);
DEFINE_UUID_OF(ID3D12GraphicsCommandList4);
DEFINE_UUID_OF(ID3D12Fence);
DEFINE_UUID_OF(ID3D12PipelineState);
DEFINE_UUID_OF(ID3D12Resource);
DEFINE_UUID_OF(ID3D12RootSignature);
DEFINE_UUID_OF(IDXGIAdapter1);
DEFINE_UUID_OF(IDXGIFactory4);
DEFINE_UUID_OF(IDXGIFactory6);

#ifdef _DEBUG
DEFINE_UUID_OF(ID3D12Debug);
DEFINE_UUID_OF(ID3D12InfoQueue);
DEFINE_UUID_OF(IDXGIInfoQueue);
#endif

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
        bool debug_dxgi = false;

#ifdef _DEBUG
        {
            ComPtr<ID3D12Debug> debug_ctrl;
            if (SUCCEEDED(::D3D12GetDebugInterface(UuidOf<ID3D12Debug>(), debug_ctrl.PutVoid())))
            {
                debug_ctrl->EnableDebugLayer();
            }
            else
            {
                ::OutputDebugStringW(L"WARNING: Direct3D Debug Device is not available\n");
            }

            ComPtr<IDXGIInfoQueue> dxgi_info_queue;
            if (SUCCEEDED(::DXGIGetDebugInterface1(0, UuidOf<IDXGIInfoQueue>(), dxgi_info_queue.PutVoid())))
            {
                debug_dxgi = true;

                TIFHR(::CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, UuidOf<IDXGIFactory4>(), dxgi_factory_.PutVoid()));

                dxgi_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
                dxgi_info_queue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
            }
        }
#endif

        if (!debug_dxgi)
        {
            TIFHR(::CreateDXGIFactory2(0, UuidOf<IDXGIFactory4>(), dxgi_factory_.PutVoid()));
        }

        ComPtr<IDXGIAdapter1> adapter;
        {
            ComPtr<IDXGIFactory6> factory6 = dxgi_factory_.As<IDXGIFactory6>();

            uint32_t adapter_id = 0;
            while (factory6->EnumAdapterByGpuPreference(adapter_id, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, UuidOf<IDXGIAdapter1>(),
                       adapter.PutVoid()) != DXGI_ERROR_NOT_FOUND)
            {
                DXGI_ADAPTER_DESC1 desc;
                TIFHR(adapter->GetDesc1(&desc));

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    continue;
                }

                if (SUCCEEDED(::D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, UuidOf<ID3D12Device5>(), device_.PutVoid())))
                {
                    break;
                }

                ++adapter_id;
            }
        }

#ifdef _DEBUG
        if (!adapter)
        {
            TIFHR(dxgi_factory_->EnumWarpAdapter(UuidOf<IDXGIAdapter1>(), adapter.PutVoid()));

            TIFHR(::D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, UuidOf<ID3D12Device5>(), device_.PutVoid()));
        }
#endif

        Verify(adapter && device_);

#ifdef _DEBUG
        if (ComPtr<ID3D12InfoQueue> d3d_info_queue = device_.TryAs<ID3D12InfoQueue>())
        {
            d3d_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
            d3d_info_queue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
        }
#endif

        D3D12_COMMAND_QUEUE_DESC const queue_qesc{D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0};
        TIFHR(device_->CreateCommandQueue(&queue_qesc, UuidOf<ID3D12CommandQueue>(), cmd_queue_.PutVoid()));

        for (auto& allocator : cmd_allocators_)
        {
            TIFHR(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, UuidOf<ID3D12CommandAllocator>(), allocator.PutVoid()));
        }

        TIFHR(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmd_allocators_[0].Get(), nullptr,
            UuidOf<ID3D12GraphicsCommandList4>(), cmd_list_.PutVoid()));
        TIFHR(cmd_list_->Close());

        TIFHR(device_->CreateFence(fence_vals_[frame_index_], D3D12_FENCE_FLAG_NONE, UuidOf<ID3D12Fence>(), fence_.PutVoid()));
        ++fence_vals_[frame_index_];

        fence_event_ = MakeWin32UniqueHandle(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
        Verify(fence_event_.get() != INVALID_HANDLE_VALUE);
    }

    void TestEnvironment::TearDown()
    {
        this->WaitForGpu();
    }

    void TestEnvironment::BeginFrame()
    {
        TIFHR(cmd_allocators_[frame_index_]->Reset());
    }

    void TestEnvironment::EndFrame()
    {
        uint64_t const curr_fence_value = fence_vals_[frame_index_];
        TIFHR(cmd_queue_->Signal(fence_.Get(), curr_fence_value));

        frame_index_ = (frame_index_ + 1) % FrameCount;

        if (fence_->GetCompletedValue() < fence_vals_[frame_index_])
        {
            TIFHR(fence_->SetEventOnCompletion(fence_vals_[frame_index_], fence_event_.get()));
            ::WaitForSingleObjectEx(fence_event_.get(), INFINITE, FALSE);
        }

        fence_vals_[frame_index_] = curr_fence_value + 1;
    }

    void TestEnvironment::ExecuteCommandList()
    {
        TIFHR(cmd_list_->Close());

        ID3D12CommandList* cmd_lists[] = {cmd_list_.Get()};
        cmd_queue_->ExecuteCommandLists(static_cast<uint32_t>(std::size(cmd_lists)), cmd_lists);
    }

    void TestEnvironment::WaitForGpu()
    {
        if (cmd_queue_ && fence_ && (fence_event_.get() != INVALID_HANDLE_VALUE))
        {
            uint64_t const fence_value = fence_vals_[frame_index_];
            if (SUCCEEDED(cmd_queue_->Signal(fence_.Get(), fence_value)))
            {
                if (SUCCEEDED(fence_->SetEventOnCompletion(fence_value, fence_event_.get())))
                {
                    ::WaitForSingleObjectEx(fence_event_.get(), INFINITE, FALSE);
                    ++fence_vals_[frame_index_];
                }
            }
        }
    }

    ComPtr<ID3D12Resource> TestEnvironment::LoadTexture(std::string const& file_name)
    {
        ComPtr<ID3D12Resource> ret;

        int width, height;
        uint8_t* data = stbi_load(file_name.c_str(), &width, &height, nullptr, 4);
        if (data != nullptr)
        {
            D3D12_HEAP_PROPERTIES const default_heap_prop = {
                D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};

            D3D12_RESOURCE_DESC const tex_desc = {D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, static_cast<uint64_t>(width),
                static_cast<uint32_t>(height), 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D12_TEXTURE_LAYOUT_UNKNOWN,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};
            TIFHR(device_->CreateCommittedResource(&default_heap_prop, D3D12_HEAP_FLAG_NONE, &tex_desc,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, UuidOf<ID3D12Resource>(), ret.PutVoid()));

            this->UploadTexture(ret.Get(), data);

            stbi_image_free(data);
        }

        return ret;
    }

    void TestEnvironment::SaveTexture(ID3D12Resource* texture, std::string const& file_name)
    {
        assert(texture != nullptr);

        std::vector<uint8_t> data = this->ReadBackTexture(texture);

        auto const tex_desc = texture->GetDesc();
        stbi_write_png(file_name.c_str(), static_cast<int>(tex_desc.Width), static_cast<int>(tex_desc.Height), 4, data.data(),
            static_cast<int>(tex_desc.Width * 4));
    }

    ComPtr<ID3D12Resource> TestEnvironment::CloneTexture(ID3D12Resource* texture)
    {
        ComPtr<ID3D12Resource> ret;

        auto const tex_desc = texture->GetDesc();

        D3D12_HEAP_PROPERTIES heap_prop;
        D3D12_HEAP_FLAGS heap_flags;
        TIFHR(texture->GetHeapProperties(&heap_prop, &heap_flags));

        TIFHR(device_->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &tex_desc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
            UuidOf<ID3D12Resource>(), ret.PutVoid()));

        this->CopyTexture(texture, ret.Get());

        return ret;
    }

    void TestEnvironment::CopyTexture(ID3D12Resource* src_texture, ID3D12Resource* dst_texture)
    {
        D3D12_TEXTURE_COPY_LOCATION src;
        src.pResource = src_texture;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION dst;
        dst.pResource = dst_texture;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;

        cmd_list_->Reset(cmd_allocators_[frame_index_].Get(), nullptr);

        D3D12_RESOURCE_BARRIER barriers[2];
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[0].Transition.pResource = src_texture;
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barriers[1].Transition.pResource = dst_texture;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd_list_->ResourceBarrier(static_cast<uint32_t>(std::size(barriers)), barriers);

        cmd_list_->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
        std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);
        cmd_list_->ResourceBarrier(static_cast<uint32_t>(std::size(barriers)), barriers);

        this->ExecuteCommandList();
        this->WaitForGpu();
    }

    void TestEnvironment::UploadTexture(ID3D12Resource* texture, uint8_t const* data)
    {
        auto const tex_desc = texture->GetDesc();
        uint32_t const width = static_cast<uint32_t>(tex_desc.Width);
        uint32_t const height = tex_desc.Height;

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        uint32_t num_row = 0;
        uint64_t row_size_in_bytes = 0;
        uint64_t required_size = 0;
        device_->GetCopyableFootprints(&tex_desc, 0, 1, 0, &layout, &num_row, &row_size_in_bytes, &required_size);

        GpuUploadBuffer upload_buffer(device_.Get(), required_size, L"UploadBuffer");

        assert(row_size_in_bytes >= width * 4);

        uint8_t* tex_data = upload_buffer.MappedData<uint8_t>();
        for (uint32_t y = 0; y < height; ++y)
        {
            memcpy(tex_data + y * row_size_in_bytes, data + y * width * 4, width * 4);
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

        cmd_list_->Reset(cmd_allocators_[frame_index_].Get(), nullptr);

        D3D12_RESOURCE_BARRIER barrier;
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = texture;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd_list_->ResourceBarrier(1, &barrier);

        cmd_list_->CopyTextureRegion(&dst, 0, 0, 0, &src, &src_box);

        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        cmd_list_->ResourceBarrier(1, &barrier);

        this->ExecuteCommandList();
        this->WaitForGpu();
    }

    std::vector<uint8_t> TestEnvironment::ReadBackTexture(ID3D12Resource* texture)
    {
        auto const tex_desc = texture->GetDesc();

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        uint32_t num_row = 0;
        uint64_t row_size_in_bytes = 0;
        uint64_t required_size = 0;
        device_->GetCopyableFootprints(&tex_desc, 0, 1, 0, &layout, &num_row, &row_size_in_bytes, &required_size);

        GpuReadbackBuffer readback_buffer(device_.Get(), required_size, L"ReadbackBuffer");

        D3D12_TEXTURE_COPY_LOCATION src;
        src.pResource = texture;
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION dst;
        dst.pResource = readback_buffer.Resource();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint = layout;

        D3D12_BOX src_box;
        src_box.left = 0;
        src_box.top = 0;
        src_box.front = 0;
        src_box.right = static_cast<uint32_t>(tex_desc.Width);
        src_box.bottom = tex_desc.Height;
        src_box.back = 1;

        cmd_list_->Reset(cmd_allocators_[frame_index_].Get(), nullptr);

        D3D12_RESOURCE_BARRIER barrier;
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = texture;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmd_list_->ResourceBarrier(1, &barrier);

        cmd_list_->CopyTextureRegion(&dst, 0, 0, 0, &src, &src_box);

        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        cmd_list_->ResourceBarrier(1, &barrier);

        this->ExecuteCommandList();
        this->WaitForGpu();

        assert(row_size_in_bytes >= tex_desc.Width * 4);

        std::vector<uint8_t> data(tex_desc.Width * tex_desc.Height * 4);
        uint8_t const* tex_data = readback_buffer.MappedData<uint8_t>();
        for (uint32_t y = 0; y < tex_desc.Height; ++y)
        {
            memcpy(&data[y * tex_desc.Width * 4], tex_data + y * row_size_in_bytes, tex_desc.Width * 4);
        }

        return data;
    }

    TestEnvironment::CompareResult TestEnvironment::CompareImages(
        ID3D12Resource* expected_image, ID3D12Resource* actual_image, float channel_tolerance)
    {
        TestEnvironment::CompareResult ret{};

        auto const expected_desc = expected_image->GetDesc();
        auto const actual_desc = actual_image->GetDesc();

        if ((expected_desc.Width != actual_desc.Width) || (expected_desc.Height != actual_desc.Height) ||
            (expected_desc.DepthOrArraySize != actual_desc.DepthOrArraySize))
        {
            ret.size_unmatch = true;
        }
        if (expected_desc.Format != actual_desc.Format)
        {
            ret.format_unmatch = true;
        }

        if (!ret.size_unmatch && !ret.format_unmatch)
        {
            D3D12_HEAP_PROPERTIES const default_heap_prop = {
                D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1};

            {
                D3D12_RESOURCE_DESC const tex_desc = {D3D12_RESOURCE_DIMENSION_TEXTURE2D, 0, expected_desc.Width, expected_desc.Height, 1,
                    1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D12_TEXTURE_LAYOUT_UNKNOWN, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS};
                TIFHR(device_->CreateCommittedResource(&default_heap_prop, D3D12_HEAP_FLAG_NONE, &tex_desc,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, UuidOf<ID3D12Resource>(), ret.error_image.PutVoid()));
            }

            GpuDefaultBuffer channel_error_buff(device_.Get(), sizeof(XMUINT4), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ChannelErrorBuffer");
            GpuReadbackBuffer channel_error_cpu_buff(device_.Get(), sizeof(XMUINT4), L"ChannelErrorCpuBuffer");

            D3D12_DESCRIPTOR_HEAP_DESC desc_heap_desc{};
            desc_heap_desc.NumDescriptors = 4; // expected_image, actual_image, result_image, error_buffer
            desc_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            desc_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            desc_heap_desc.NodeMask = 0;

            ComPtr<ID3D12DescriptorHeap> descriptor_heap;
            TIFHR(device_->CreateDescriptorHeap(&desc_heap_desc, UuidOf<ID3D12DescriptorHeap>(), descriptor_heap.PutVoid()));
            descriptor_heap->SetName(L"Comparator Descriptor Heap");

            uint32_t const descriptor_size = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            auto const cpu_desc_handle_base = descriptor_heap->GetCPUDescriptorHandleForHeapStart();
            auto const gpu_desc_handle_base = descriptor_heap->GetGPUDescriptorHandleForHeapStart();

            auto CreateSrv = [&](ID3D12Resource* resource, uint32_t index, uint32_t structure_stride = 0) {
                D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu_desc_handle = {cpu_desc_handle_base.ptr + index * descriptor_size};

                auto const desc = resource->GetDesc();
                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
                if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
                {
                    srv_desc.Format = desc.Format;
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    srv_desc.Texture2D.MipLevels = 1;
                    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                }
                else
                {
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
                    srv_desc.Buffer.FirstElement = 0;
                    srv_desc.Buffer.NumElements = 1;
                    srv_desc.Buffer.StructureByteStride = structure_stride;
                }
                device_->CreateShaderResourceView(resource, &srv_desc, srv_cpu_desc_handle);
                D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu_desc_handle = {gpu_desc_handle_base.ptr + index * descriptor_size};
                return srv_gpu_desc_handle;
            };

            auto CreateUav = [&](ID3D12Resource* resource, uint32_t index, uint32_t structure_stride = 0) {
                D3D12_CPU_DESCRIPTOR_HANDLE uav_cpu_desc_handle = {cpu_desc_handle_base.ptr + index * descriptor_size};

                auto const desc = resource->GetDesc();
                D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
                if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
                {
                    uav_desc.Format = LinearFormatOf(desc.Format);
                    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                }
                else
                {
                    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
                    uav_desc.Buffer.FirstElement = 0;
                    uav_desc.Buffer.NumElements = 1;
                    uav_desc.Buffer.StructureByteStride = structure_stride;
                }
                device_->CreateUnorderedAccessView(resource, nullptr, &uav_desc, uav_cpu_desc_handle);
                D3D12_GPU_DESCRIPTOR_HANDLE uav_gpu_desc_handle = {gpu_desc_handle_base.ptr + index * descriptor_size};
                return uav_gpu_desc_handle;
            };

            D3D12_GPU_DESCRIPTOR_HANDLE expected_srv_gpu_desc_handle = CreateSrv(expected_image, 0);
            D3D12_GPU_DESCRIPTOR_HANDLE actual_srv_cpu_desc_handle = CreateSrv(actual_image, 1);
            D3D12_GPU_DESCRIPTOR_HANDLE result_uav_cpu_desc_handle = CreateUav(ret.error_image.Get(), 2);
            D3D12_GPU_DESCRIPTOR_HANDLE channel_error_uav_cpu_desc_handle = CreateUav(channel_error_buff.Resource(), 3, sizeof(XMUINT4));

            struct ComparatorConstantBuffer
            {
                XMUINT2 width_height;
                float channel_tolerance;
            };

            ConstantBuffer<ComparatorConstantBuffer> comparator_cb(device_.Get(), 1, L"ComparatorCb");
            {
                comparator_cb->width_height = {static_cast<uint32_t>(expected_desc.Width), expected_desc.Height};
                comparator_cb->channel_tolerance = channel_tolerance;
                comparator_cb.UploadToGpu();
            }

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

                TIFHR(device_->CreateRootSignature(
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

                TIFHR(device_->CreateComputePipelineState(&pso_desc, UuidOf<ID3D12PipelineState>(), pso.PutVoid()));
            }

            cmd_list_->Reset(cmd_allocators_[frame_index_].Get(), nullptr);

            cmd_list_->SetComputeRootSignature(root_sig.Get());
            cmd_list_->SetPipelineState(pso.Get());

            ID3D12DescriptorHeap* heaps[] = {descriptor_heap.Get()};
            cmd_list_->SetDescriptorHeaps(static_cast<uint32_t>(std::size(heaps)), heaps);
            cmd_list_->SetComputeRootDescriptorTable(0, expected_srv_gpu_desc_handle);
            cmd_list_->SetComputeRootDescriptorTable(1, result_uav_cpu_desc_handle);
            cmd_list_->SetComputeRootConstantBufferView(2, comparator_cb.Resource()->GetGPUVirtualAddress());

            uint32_t constexpr TILE_DIM = 16;
            cmd_list_->Dispatch((static_cast<uint32_t>(expected_desc.Width) + (TILE_DIM - 1)) / TILE_DIM,
                (expected_desc.Height + (TILE_DIM - 1)) / TILE_DIM, 1);

            D3D12_RESOURCE_BARRIER barrier;
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = channel_error_buff.Resource();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            cmd_list_->ResourceBarrier(1, &barrier);

            cmd_list_->CopyBufferRegion(channel_error_cpu_buff.Resource(), 0, channel_error_buff.Resource(), 0, sizeof(XMFLOAT4));

            this->ExecuteCommandList();
            this->WaitForGpu();

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
        }

        return ret;
    }

    void TestEnvironment::CompareWithExpected(std::string const& expected_name, ID3D12Resource* actual_image, float channel_tolerance)
    {
        auto expected_image = this->LoadTexture(EXPECTED_DIR + expected_name + ".png");

        std::string const result_dir = EXPECTED_DIR "../Result/";
        if (!expected_image)
        {
            std::string const expected_file = result_dir + expected_name + ".png";
            std::cout << "Saving expected image to " << expected_file;
            this->SaveTexture(actual_image, expected_file.c_str());
        }
        else
        {
            auto result = this->CompareImages(expected_image.Get(), actual_image, channel_tolerance);
            if (result.error_image)
            {
                this->SaveTexture(result.error_image.Get(), result_dir + expected_name + "_diff.png");
            }

            EXPECT_FALSE(result.format_unmatch);
            EXPECT_FALSE(result.size_unmatch);

            EXPECT_LT(result.channel_errors[0], 1e-6f);
            EXPECT_LT(result.channel_errors[1], 1e-6f);
            EXPECT_LT(result.channel_errors[2], 1e-6f);
            EXPECT_LT(result.channel_errors[3], 1e-6f);

            EXPECT_FALSE(result.error_image);
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
