#include "../pch.hpp"

#include <GoldenSun/Gpu/GpuSystem.hpp>

#include <GoldenSun/SmartPtrHelper.hpp>
#include <GoldenSun/Util.hpp>
#include <GoldenSun/Uuid.hpp>

#include <list>

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#include "../ImplPtrImpl.hpp"
#include "GpuSystemInternal.hpp"

DEFINE_UUID_OF(ID3D12CommandAllocator);
DEFINE_UUID_OF(ID3D12CommandQueue);
DEFINE_UUID_OF(ID3D12DescriptorHeap);
DEFINE_UUID_OF(ID3D12Device5);
DEFINE_UUID_OF(ID3D12GraphicsCommandList4);
DEFINE_UUID_OF(ID3D12Fence);
DEFINE_UUID_OF(ID3D12Resource);
DEFINE_UUID_OF(ID3D12PipelineState);
DEFINE_UUID_OF(ID3D12RootSignature);
DEFINE_UUID_OF(ID3D12StateObject);
DEFINE_UUID_OF(ID3D12StateObjectProperties);
DEFINE_UUID_OF(IDXGIAdapter1);
DEFINE_UUID_OF(IDXGIFactory4);
DEFINE_UUID_OF(IDXGIFactory5);
DEFINE_UUID_OF(IDXGIFactory6);
DEFINE_UUID_OF(IDXGISwapChain3);

#ifdef _DEBUG
DEFINE_UUID_OF(ID3D12Debug);
DEFINE_UUID_OF(ID3D12InfoQueue);
DEFINE_UUID_OF(IDXGIInfoQueue);
#endif

namespace GoldenSun
{
    class GpuSystem::Impl final
    {
        DISALLOW_COPY_AND_ASSIGN(Impl)
        DISALLOW_COPY_MOVE_AND_ASSIGN(Impl)

        friend class GpuSystem;

    public:
        Impl(GpuSystem& owner, ID3D12Device5* device, ID3D12CommandQueue* cmd_queue)
            : upload_mem_allocator_(owner, true), readback_mem_allocator_(owner, false),
              rtv_desc_allocator_(owner, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE),
              dsv_desc_allocator_(owner, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE),
              cbv_srv_uav_desc_allocator_(owner, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE),
              sampler_desc_allocator_(owner, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
        {
            bool debug_dxgi = false;

#ifdef _DEBUG
            if (device == nullptr)
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

            {
                tearing_supported_ = false;

                if (ComPtr<IDXGIFactory5> factory5 = dxgi_factory_.TryAs<IDXGIFactory5>())
                {
                    BOOL allow_tearing = FALSE;
                    HRESULT hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing));
                    if (SUCCEEDED(hr) && allow_tearing)
                    {
                        tearing_supported_ = true;
                    }
                }
            }

            if (device != nullptr)
            {
                device_ = device;
            }
            else
            {
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

                        if (SUCCEEDED(
                                ::D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, UuidOf<ID3D12Device5>(), device_.PutVoid())))
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
            }

            if (cmd_queue != nullptr)
            {
                cmd_queue_ = cmd_queue;
            }
            else
            {
                D3D12_COMMAND_QUEUE_DESC const queue_qesc{D3D12_COMMAND_LIST_TYPE_DIRECT, 0, D3D12_COMMAND_QUEUE_FLAG_NONE, 0};
                TIFHR(device_->CreateCommandQueue(&queue_qesc, UuidOf<ID3D12CommandQueue>(), cmd_queue_.PutVoid()));
            }

            for (auto& allocator : cmd_allocators_)
            {
                TIFHR(
                    device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, UuidOf<ID3D12CommandAllocator>(), allocator.PutVoid()));
            }

            TIFHR(device_->CreateFence(fence_vals_[frame_index_], D3D12_FENCE_FLAG_NONE, UuidOf<ID3D12Fence>(), fence_.PutVoid()));
            ++fence_vals_[frame_index_];

            fence_event_ = MakeWin32UniqueHandle(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
            Verify(fence_event_.get() != INVALID_HANDLE_VALUE);
        }

        bool TearingSupported() const noexcept
        {
            return tearing_supported_;
        }

        ID3D12Device5* Device() const noexcept
        {
            return device_.Get();
        }

        ID3D12CommandQueue* CommandQueue() const noexcept
        {
            return cmd_queue_.Get();
        }

        uint32_t FrameIndex() const noexcept
        {
            return frame_index_;
        }

        void MoveToNextFrame()
        {
            uint64_t const curr_fence_value = fence_vals_[frame_index_];
            TIFHR(cmd_queue_->Signal(fence_.Get(), curr_fence_value));

            frame_index_ = (frame_index_ + 1) % FrameCount();

            if (fence_->GetCompletedValue() < fence_vals_[frame_index_])
            {
                TIFHR(fence_->SetEventOnCompletion(fence_vals_[frame_index_], fence_event_.get()));
                ::WaitForSingleObjectEx(fence_event_.get(), INFINITE, FALSE);
            }

            fence_vals_[frame_index_] = curr_fence_value + 1;

            TIFHR(this->CurrentCommandAllocator()->Reset());
        }

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO RayTracingAccelerationStructurePrebuildInfo(
            D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS const& desc)
        {
            D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
            device_->GetRaytracingAccelerationStructurePrebuildInfo(&desc, &info);
            return info;
        }

        GpuBuffer CreateBuffer(
            uint32_t size, D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, std::wstring_view name)
        {
            return GpuSystemInternalD3D12::CreateBuffer(device_.Get(), size, heap_type, flags, init_state, std::move(name));
        }

        GpuDefaultBuffer CreateDefaultBuffer(
            uint32_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, std::wstring_view name)
        {
            return GpuSystemInternalD3D12::CreateDefaultBuffer(device_.Get(), size, flags, init_state, std::move(name));
        }

        GpuUploadBuffer CreateUploadBuffer(uint32_t size, std::wstring_view name)
        {
            return GpuSystemInternalD3D12::CreateUploadBuffer(device_.Get(), size, std::move(name));
        }

        GpuReadbackBuffer CreateReadbackBuffer(uint32_t size, std::wstring_view name)
        {
            return GpuSystemInternalD3D12::CreateReadbackBuffer(device_.Get(), size, std::move(name));
        }

        GpuTexture2D CreateTexture2D(uint32_t width, uint32_t height, uint32_t mip_levels, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
            D3D12_RESOURCE_STATES init_state, std::wstring_view name)
        {
            return GpuSystemInternalD3D12::CreateTexture2D(
                device_.Get(), width, height, mip_levels, format, flags, init_state, std::move(name));
        }

        GpuDescriptorHeap CreateDescriptorHeap(
            uint32_t size, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, std::wstring_view name)
        {
            return GpuSystemInternalD3D12::CreateDescriptorHeap(device_.Get(), size, type, flags, std::move(name));
        }

        GpuShaderResourceView CreateShaderResourceView(GpuBuffer const& buffer, uint32_t first_element, uint32_t num_elements,
            uint32_t element_size, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
        {
            return GpuSystemInternalD3D12::CreateShaderResourceView(
                device_.Get(), buffer, first_element, num_elements, element_size, cpu_handle);
        }

        GpuShaderResourceView CreateShaderResourceView(
            GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
        {
            return GpuSystemInternalD3D12::CreateShaderResourceView(device_.Get(), texture, format, cpu_handle);
        }

        GpuRenderTargetView CreateRenderTargetView(GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
        {
            return GpuSystemInternalD3D12::CreateRenderTargetView(device_.Get(), texture, format, cpu_handle);
        }

        GpuUnorderedAccessView CreateUnorderedAccessView(GpuBuffer const& buffer, uint32_t first_element, uint32_t num_elements,
            uint32_t element_size, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
        {
            return GpuSystemInternalD3D12::CreateUnorderedAccessView(
                device_.Get(), buffer, first_element, num_elements, element_size, cpu_handle);
        }

        GpuUnorderedAccessView CreateUnorderedAccessView(
            GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
        {
            return GpuSystemInternalD3D12::CreateUnorderedAccessView(device_.Get(), texture, format, cpu_handle);
        }

        GpuSwapChain CreateSwapChain(HWND window, uint32_t width, uint32_t height, DXGI_FORMAT format)
        {
            return GpuSystemInternalD3D12::CreateSwapChain(
                dxgi_factory_.Get(), cmd_queue_.Get(), window, width, height, format, tearing_supported_);
        }

        GpuCommandList CreateCommandList()
        {
            auto* cmd_allocator = this->CurrentCommandAllocator();
            if (cmd_list_pool_.empty())
            {
                return GpuSystemInternalD3D12::CreateCommandList(device_.Get(), cmd_allocator);
            }
            else
            {
                auto cmd_list = std::move(cmd_list_pool_.front());
                cmd_list_pool_.pop_front();
                GpuSystemInternalD3D12::ResetCommandList(cmd_list, cmd_allocator);
                return cmd_list;
            }
        }

        void Execute(GpuCommandList&& cmd_list)
        {
            this->ExecuteOnly(cmd_list);
            cmd_list_pool_.emplace_back(std::move(cmd_list));
        }

        void ExecuteAndReset(GpuCommandList& cmd_list)
        {
            this->ExecuteOnly(cmd_list);
            GpuSystemInternalD3D12::ResetCommandList(cmd_list, this->CurrentCommandAllocator());
        }

        uint32_t RtvDescSize() const noexcept
        {
            return rtv_desc_allocator_.DescriptorSize();
        }

        uint32_t DsvDescSize() const noexcept
        {
            return dsv_desc_allocator_.DescriptorSize();
        }

        uint32_t CbvSrvUavDescSize() const noexcept
        {
            return cbv_srv_uav_desc_allocator_.DescriptorSize();
        }

        uint32_t SamplerDescSize() const noexcept
        {
            return sampler_desc_allocator_.DescriptorSize();
        }

        GpuDescriptorBlock AllocRtvDescBlock(uint32_t size)
        {
            return rtv_desc_allocator_.Allocate(size);
        }

        void DeallocRtvDescBlock(GpuDescriptorBlock&& desc_block)
        {
            return rtv_desc_allocator_.Deallocate(std::move(desc_block), fence_vals_[frame_index_]);
        }

        void ReallocRtvDescBlock(GpuDescriptorBlock& desc_block, uint32_t size)
        {
            return rtv_desc_allocator_.Reallocate(desc_block, fence_vals_[frame_index_], size);
        }

        GpuDescriptorBlock AllocDsvDescBlock(uint32_t size)
        {
            return dsv_desc_allocator_.Allocate(size);
        }

        void DeallocDsvDescBlock(GpuDescriptorBlock&& desc_block)
        {
            return dsv_desc_allocator_.Deallocate(std::move(desc_block), fence_vals_[frame_index_]);
        }

        void ReallocDsvDescBlock(GpuDescriptorBlock& desc_block, uint32_t size)
        {
            return dsv_desc_allocator_.Reallocate(desc_block, fence_vals_[frame_index_], size);
        }

        GpuDescriptorBlock AllocCbvSrvUavDescBlock(uint32_t size)
        {
            return cbv_srv_uav_desc_allocator_.Allocate(size);
        }

        void DeallocCbvSrvUavDescBlock(GpuDescriptorBlock&& desc_block)
        {
            return cbv_srv_uav_desc_allocator_.Deallocate(std::move(desc_block), fence_vals_[frame_index_]);
        }

        void ReallocCbvSrvUavDescBlock(GpuDescriptorBlock& desc_block, uint32_t size)
        {
            return cbv_srv_uav_desc_allocator_.Reallocate(desc_block, fence_vals_[frame_index_], size);
        }

        GpuDescriptorBlock AllocSamplerDescBlock(uint32_t size)
        {
            return sampler_desc_allocator_.Allocate(size);
        }

        void DeallocSamplerDescBlock(GpuDescriptorBlock&& desc_block)
        {
            return sampler_desc_allocator_.Deallocate(std::move(desc_block), fence_vals_[frame_index_]);
        }

        void ReallocSamplerDescBlock(GpuDescriptorBlock& desc_block, uint32_t size)
        {
            return sampler_desc_allocator_.Reallocate(desc_block, fence_vals_[frame_index_], size);
        }

        GpuMemoryBlock AllocUploadMemBlock(uint32_t size_in_bytes, uint32_t alignment)
        {
            return upload_mem_allocator_.Allocate(size_in_bytes, alignment);
        }

        void DeallocUploadMemBlock(GpuMemoryBlock&& mem_block)
        {
            return upload_mem_allocator_.Deallocate(std::move(mem_block), fence_vals_[frame_index_]);
        }

        void ReallocUploadMemBlock(GpuMemoryBlock& mem_block, uint32_t size_in_bytes, uint32_t alignment)
        {
            return upload_mem_allocator_.Reallocate(mem_block, fence_vals_[frame_index_], size_in_bytes, alignment);
        }

        GpuMemoryBlock AllocReadbackMemBlock(uint32_t size_in_bytes, uint32_t alignment)
        {
            return readback_mem_allocator_.Allocate(size_in_bytes, alignment);
        }

        void DeallocReadbackMemBlock(GpuMemoryBlock&& mem_block)
        {
            return readback_mem_allocator_.Deallocate(std::move(mem_block), fence_vals_[frame_index_]);
        }

        void ReallocReadbackMemBlock(GpuMemoryBlock& mem_block, uint32_t size_in_bytes, uint32_t alignment)
        {
            return readback_mem_allocator_.Reallocate(mem_block, fence_vals_[frame_index_], size_in_bytes, alignment);
        }

        void WaitForGpu()
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

        void ResetFenceValues()
        {
            for (auto& value : fence_vals_)
            {
                value = fence_vals_[frame_index_];
            }

            frame_index_ = 0;
        }

        void HandleDeviceLost()
        {
            upload_mem_allocator_.Clear();
            readback_mem_allocator_.Clear();

            rtv_desc_allocator_.Clear();
            dsv_desc_allocator_.Clear();
            cbv_srv_uav_desc_allocator_.Clear();
            sampler_desc_allocator_.Clear();

            cmd_list_pool_.clear();
            for (auto& cmd_allocator : cmd_allocators_)
            {
                cmd_allocator.Reset();
            }

            fence_.Reset();
            cmd_queue_.Reset();
            device_.Reset();
            dxgi_factory_.Reset();

            frame_index_ = 0;
        }

    private:
        ID3D12CommandAllocator* CurrentCommandAllocator() const noexcept
        {
            return cmd_allocators_[frame_index_].Get();
        }

        void ExecuteOnly(GpuCommandList& cmd_list)
        {
            cmd_list.Close();

            ID3D12CommandList* cmd_lists[] = {cmd_list.NativeHandle<D3D12Traits>()};
            cmd_queue_->ExecuteCommandLists(static_cast<uint32_t>(std::size(cmd_lists)), cmd_lists);

            uint64_t const curr_fence_value = fence_vals_[frame_index_];
            TIFHR(cmd_queue_->Signal(fence_.Get(), curr_fence_value));
            fence_vals_[frame_index_] = curr_fence_value + 1;
        }

    private:
        ComPtr<IDXGIFactory4> dxgi_factory_;
        ComPtr<ID3D12Device5> device_;
        ComPtr<ID3D12CommandQueue> cmd_queue_;
        ComPtr<ID3D12CommandAllocator> cmd_allocators_[FrameCount()];
        std::list<GpuCommandList> cmd_list_pool_;

        ComPtr<ID3D12Fence> fence_;
        uint64_t fence_vals_[FrameCount()]{};
        Win32UniqueHandle fence_event_;

        uint32_t frame_index_{0};

        bool tearing_supported_;

        GpuMemoryAllocator upload_mem_allocator_;
        GpuMemoryAllocator readback_mem_allocator_;

        GpuDescriptorAllocator rtv_desc_allocator_;
        GpuDescriptorAllocator dsv_desc_allocator_;
        GpuDescriptorAllocator cbv_srv_uav_desc_allocator_;
        GpuDescriptorAllocator sampler_desc_allocator_;
    };


    GpuSystem::GpuSystem() : GpuSystem(nullptr, nullptr)
    {
    }

    GpuSystem::GpuSystem(void* native_device, void* native_cmd_queue)
        : impl_(*this, reinterpret_cast<ID3D12Device5*>(native_device), reinterpret_cast<ID3D12CommandQueue*>(native_cmd_queue))
    {
    }

    GpuSystem::~GpuSystem() noexcept = default;
    GpuSystem::GpuSystem(GpuSystem&& other) noexcept = default;
    GpuSystem& GpuSystem::operator=(GpuSystem&& other) noexcept = default;

    bool GpuSystem::TearingSupported() const noexcept
    {
        return impl_->TearingSupported();
    }

    void* GpuSystem::NativeDeviceHandle() const noexcept
    {
        return impl_->Device();
    }

    void* GpuSystem::NativeCommandQueueHandle() const noexcept
    {
        return impl_->CommandQueue();
    }

    uint32_t GpuSystem::FrameIndex() const noexcept
    {
        return impl_->FrameIndex();
    }

    void GpuSystem::MoveToNextFrame()
    {
        impl_->MoveToNextFrame();
    }

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO GpuSystem::RayTracingAccelerationStructurePrebuildInfo(
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS const& desc)
    {
        return impl_->RayTracingAccelerationStructurePrebuildInfo(desc);
    }

    GpuBuffer GpuSystem::CreateBuffer(
        uint32_t size, D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, std::wstring_view name)
    {
        return impl_->CreateBuffer(size, heap_type, flags, init_state, std::move(name));
    }

    GpuDefaultBuffer GpuSystem::CreateDefaultBuffer(
        uint32_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, std::wstring_view name)
    {
        return impl_->CreateDefaultBuffer(size, flags, init_state, std::move(name));
    }

    GpuUploadBuffer GpuSystem::CreateUploadBuffer(uint32_t size, std::wstring_view name)
    {
        return impl_->CreateUploadBuffer(size, std::move(name));
    }

    GpuUploadBuffer GpuSystem::CreateUploadBuffer(void const* data, uint32_t size, std::wstring_view name)
    {
        auto buffer = impl_->CreateUploadBuffer(size, std::move(name));
        memcpy(buffer.MappedData<void>(), data, size);
        return buffer;
    }

    GpuReadbackBuffer GpuSystem::CreateReadbackBuffer(uint32_t size, std::wstring_view name)
    {
        return impl_->CreateReadbackBuffer(size, std::move(name));
    }

    GpuTexture2D GpuSystem::CreateTexture2D(uint32_t width, uint32_t height, uint32_t mip_levels, DXGI_FORMAT format,
        D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES init_state, std::wstring_view name)
    {
        return impl_->CreateTexture2D(width, height, mip_levels, format, flags, init_state, std::move(name));
    }

    GpuDescriptorHeap GpuSystem::CreateDescriptorHeap(
        uint32_t size, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, std::wstring_view name)
    {
        return impl_->CreateDescriptorHeap(size, type, flags, std::move(name));
    }

    GpuShaderResourceView GpuSystem::CreateShaderResourceView(GpuBuffer const& buffer, uint32_t first_element, uint32_t num_elements,
        uint32_t element_size, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        return impl_->CreateShaderResourceView(buffer, first_element, num_elements, element_size, cpu_handle);
    }

    GpuShaderResourceView GpuSystem::CreateShaderResourceView(
        GpuMemoryBlock const& mem_block, uint32_t element_size, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        assert(mem_block.Offset() / element_size * element_size == mem_block.Offset());
        assert(mem_block.Size() / element_size * element_size == mem_block.Size());
        return this->CreateShaderResourceView(GpuBuffer(mem_block.NativeBufferHandle<D3D12Traits>(), D3D12_RESOURCE_STATE_GENERIC_READ),
            mem_block.Offset() / element_size, mem_block.Size() / element_size, element_size, cpu_handle);
    }

    GpuShaderResourceView GpuSystem::CreateShaderResourceView(
        GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        return impl_->CreateShaderResourceView(texture, format, cpu_handle);
    }

    GpuShaderResourceView GpuSystem::CreateShaderResourceView(GpuTexture2D const& texture, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        return this->CreateShaderResourceView(texture, DXGI_FORMAT_UNKNOWN, cpu_handle);
    }

    GpuRenderTargetView GpuSystem::CreateRenderTargetView(
        GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        return impl_->CreateRenderTargetView(texture, format, cpu_handle);
    }

    GpuRenderTargetView GpuSystem::CreateRenderTargetView(GpuTexture2D const& texture, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        return this->CreateRenderTargetView(texture, DXGI_FORMAT_UNKNOWN, cpu_handle);
    }

    GpuUnorderedAccessView GpuSystem::CreateUnorderedAccessView(GpuBuffer const& buffer, uint32_t first_element, uint32_t num_elements,
        uint32_t element_size, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        return impl_->CreateUnorderedAccessView(buffer, first_element, num_elements, element_size, cpu_handle);
    }

    GpuUnorderedAccessView GpuSystem::CreateUnorderedAccessView(
        GpuTexture2D const& texture, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        return impl_->CreateUnorderedAccessView(texture, format, cpu_handle);
    }

    GpuUnorderedAccessView GpuSystem::CreateUnorderedAccessView(GpuTexture2D const& texture, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
    {
        return this->CreateUnorderedAccessView(texture, DXGI_FORMAT_UNKNOWN, cpu_handle);
    }

    GpuSwapChain GpuSystem::CreateSwapChain(void* native_window, uint32_t width, uint32_t height, DXGI_FORMAT format)
    {
        return impl_->CreateSwapChain(static_cast<HWND>(native_window), width, height, format);
    }

    GpuCommandList GpuSystem::CreateCommandList()
    {
        return impl_->CreateCommandList();
    }

    void GpuSystem::Execute(GpuCommandList&& cmd_list)
    {
        impl_->Execute(std::move(cmd_list));
    }

    void GpuSystem::ExecuteAndReset(GpuCommandList& cmd_list)
    {
        impl_->ExecuteAndReset(cmd_list);
    }

    uint32_t GpuSystem::RtvDescSize() const noexcept
    {
        return impl_->RtvDescSize();
    }

    uint32_t GpuSystem::DsvDescSize() const noexcept
    {
        return impl_->DsvDescSize();
    }

    uint32_t GpuSystem::CbvSrvUavDescSize() const noexcept
    {
        return impl_->CbvSrvUavDescSize();
    }

    uint32_t GpuSystem::SamplerDescSize() const noexcept
    {
        return impl_->SamplerDescSize();
    }

    GpuDescriptorBlock GpuSystem::AllocRtvDescBlock(uint32_t size)
    {
        return impl_->AllocRtvDescBlock(size);
    }

    void GpuSystem::DeallocRtvDescBlock(GpuDescriptorBlock&& desc_block)
    {
        return impl_->DeallocRtvDescBlock(std::move(desc_block));
    }

    void GpuSystem::ReallocRtvDescBlock(GpuDescriptorBlock& desc_block, uint32_t size)
    {
        return impl_->ReallocRtvDescBlock(desc_block, size);
    }

    GpuDescriptorBlock GpuSystem::AllocDsvDescBlock(uint32_t size)
    {
        return impl_->AllocDsvDescBlock(size);
    }

    void GpuSystem::DeallocDsvDescBlock(GpuDescriptorBlock&& desc_block)
    {
        return impl_->DeallocDsvDescBlock(std::move(desc_block));
    }

    void GpuSystem::ReallocDsvDescBlock(GpuDescriptorBlock& desc_block, uint32_t size)
    {
        return impl_->ReallocDsvDescBlock(desc_block, size);
    }

    GpuDescriptorBlock GpuSystem::AllocCbvSrvUavDescBlock(uint32_t size)
    {
        return impl_->AllocCbvSrvUavDescBlock(size);
    }

    void GpuSystem::DeallocCbvSrvUavDescBlock(GpuDescriptorBlock&& desc_block)
    {
        return impl_->DeallocCbvSrvUavDescBlock(std::move(desc_block));
    }

    void GpuSystem::ReallocCbvSrvUavDescBlock(GpuDescriptorBlock& desc_block, uint32_t size)
    {
        return impl_->ReallocCbvSrvUavDescBlock(desc_block, size);
    }

    GpuDescriptorBlock GpuSystem::AllocSamplerDescBlock(uint32_t size)
    {
        return impl_->AllocSamplerDescBlock(size);
    }

    void GpuSystem::DeallocSamplerDescBlock(GpuDescriptorBlock&& desc_block)
    {
        return impl_->DeallocSamplerDescBlock(std::move(desc_block));
    }

    void GpuSystem::ReallocSamplerDescBlock(GpuDescriptorBlock& desc_block, uint32_t size)
    {
        return impl_->ReallocSamplerDescBlock(desc_block, size);
    }

    GpuMemoryBlock GpuSystem::AllocUploadMemBlock(uint32_t size_in_bytes, uint32_t alignment)
    {
        return impl_->AllocUploadMemBlock(size_in_bytes, alignment);
    }

    void GpuSystem::DeallocUploadMemBlock(GpuMemoryBlock&& mem_block)
    {
        return impl_->DeallocUploadMemBlock(std::move(mem_block));
    }

    void GpuSystem::ReallocUploadMemBlock(GpuMemoryBlock& mem_block, uint32_t size_in_bytes, uint32_t alignment)
    {
        return impl_->ReallocUploadMemBlock(mem_block, size_in_bytes, alignment);
    }

    GpuMemoryBlock GpuSystem::AllocReadbackMemBlock(uint32_t size_in_bytes, uint32_t alignment)
    {
        return impl_->AllocReadbackMemBlock(size_in_bytes, alignment);
    }

    void GpuSystem::DeallocReadbackMemBlock(GpuMemoryBlock&& mem_block)
    {
        return impl_->DeallocReadbackMemBlock(std::move(mem_block));
    }

    void GpuSystem::ReallocReadbackMemBlock(GpuMemoryBlock& mem_block, uint32_t size_in_bytes, uint32_t alignment)
    {
        return impl_->ReallocReadbackMemBlock(mem_block, size_in_bytes, alignment);
    }

    void GpuSystem::WaitForGpu()
    {
        impl_->WaitForGpu();
    }

    void GpuSystem::ResetFenceValues()
    {
        impl_->ResetFenceValues();
    }

    void GpuSystem::HandleDeviceLost()
    {
        impl_->HandleDeviceLost();
    }
} // namespace GoldenSun
