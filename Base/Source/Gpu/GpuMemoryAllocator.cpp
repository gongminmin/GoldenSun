#include "../pch.hpp"

#include <GoldenSun/Gpu/GpuMemoryAllocator.hpp>
#include <GoldenSun/Gpu/GpuSystem.hpp>

#include <cassert>

namespace
{
    static constexpr uint32_t DefaultPageSize = 2 * 1024 * 1024;
}

namespace GoldenSun
{
    GpuMemoryPage::GpuMemoryPage(GpuSystem& gpu_system, bool is_upload, uint32_t size_in_bytes) : is_upload_(is_upload)
    {
        D3D12_RESOURCE_STATES init_state;
        D3D12_HEAP_TYPE heap_type;
        if (is_upload_)
        {
            heap_type = D3D12_HEAP_TYPE_UPLOAD;
            init_state = D3D12_RESOURCE_STATE_GENERIC_READ;
        }
        else
        {
            heap_type = D3D12_HEAP_TYPE_READBACK;
            init_state = D3D12_RESOURCE_STATE_COPY_DEST;
        }

        buffer_ = gpu_system.CreateBuffer(size_in_bytes, heap_type, D3D12_RESOURCE_FLAG_NONE, init_state, L"GpuMemoryPage");
        cpu_addr_ = buffer_.Map();
        gpu_addr_ = buffer_.GpuVirtualAddress();
    }

    GpuMemoryPage::~GpuMemoryPage() noexcept
    {
        if (buffer_)
        {
            buffer_.Unmap();
        }
    }

    GpuMemoryPage::GpuMemoryPage(GpuMemoryPage&& other) noexcept
        : is_upload_(other.is_upload_), buffer_(std::move(other.buffer_)), cpu_addr_(std::move(other.cpu_addr_)),
          gpu_addr_(std::move(other.gpu_addr_))
    {
    }

    GpuMemoryPage& GpuMemoryPage::operator=(GpuMemoryPage&& other) noexcept
    {
        if (this != &other)
        {
            assert(is_upload_ == other.is_upload_);

            buffer_ = std::move(other.buffer_);
            cpu_addr_ = std::move(other.cpu_addr_);
            gpu_addr_ = std::move(other.gpu_addr_);
        }
        return *this;
    }


    GpuMemoryBlock::GpuMemoryBlock() noexcept = default;
    GpuMemoryBlock::GpuMemoryBlock(GpuMemoryBlock&& other) noexcept = default;
    GpuMemoryBlock& GpuMemoryBlock::operator=(GpuMemoryBlock&& other) noexcept = default;

    void GpuMemoryBlock::Reset() noexcept
    {
        native_resource_ = nullptr;
        offset_ = 0;
        size_ = 0;
        cpu_addr_ = nullptr;
        gpu_addr_ = {};
    }

    void GpuMemoryBlock::Reset(GpuMemoryPage const& page, uint32_t offset, uint32_t size) noexcept
    {
        native_resource_ = page.Buffer().NativeResource();
        offset_ = offset;
        size_ = size;
        cpu_addr_ = page.CpuAddress<uint8_t>() + offset;
        gpu_addr_ = page.GpuAddress() + offset;
    }


    GpuMemoryAllocator::GpuMemoryAllocator(GpuSystem& gpu_system, bool is_upload) noexcept : gpu_system_(&gpu_system), is_upload_(is_upload)
    {
    }

    GpuMemoryAllocator::GpuMemoryAllocator(GpuMemoryAllocator&& other) noexcept
        : gpu_system_(std::move(other.gpu_system_)), is_upload_(other.is_upload_), pages_(std::move(other.pages_)),
          large_pages_(std::move(other.large_pages_))
    {
    }

    GpuMemoryAllocator& GpuMemoryAllocator::operator=(GpuMemoryAllocator&& other) noexcept
    {
        if (this != &other)
        {
            assert(is_upload_ == other.is_upload_);

            gpu_system_ = std::move(other.gpu_system_);
            pages_ = std::move(other.pages_);
            large_pages_ = std::move(other.large_pages_);
        }
        return *this;
    }

    GpuMemoryBlock GpuMemoryAllocator::Allocate(uint32_t size_in_bytes, uint32_t alignment)
    {
        std::lock_guard<std::mutex> lock(allocation_mutex_);

        GpuMemoryBlock mem_block;
        this->Allocate(lock, mem_block, size_in_bytes, alignment);
        return mem_block;
    }

    void GpuMemoryAllocator::Allocate(
        [[maybe_unused]] std::lock_guard<std::mutex>& proof_of_lock, GpuMemoryBlock& mem_block, uint32_t size_in_bytes, uint32_t alignment)
    {
        uint32_t const alignment_mask = alignment - 1;
        assert((alignment & alignment_mask) == 0);

        uint32_t const aligned_size = (size_in_bytes + alignment_mask) & ~alignment_mask;

        if (aligned_size > DefaultPageSize)
        {
            auto& large_page = large_pages_.emplace_back(GpuMemoryPage(*gpu_system_, is_upload_, aligned_size));
            mem_block.Reset(large_page, 0, size_in_bytes);
            return;
        }

        for (auto& page_info : pages_)
        {
            auto const iter = std::lower_bound(page_info.free_list.begin(), page_info.free_list.end(), aligned_size,
                [](PageInfo::FreeRange const& free_range, uint32_t s) { return free_range.first_offset + s > free_range.last_offset; });
            if (iter != page_info.free_list.end())
            {
                mem_block.Reset(page_info.page, iter->first_offset, aligned_size);
                iter->first_offset += aligned_size;
                if (iter->first_offset == iter->last_offset)
                {
                    page_info.free_list.erase(iter);
                }

                return;
            }
        }

        GpuMemoryPage new_page(*gpu_system_, is_upload_, DefaultPageSize);
        mem_block.Reset(new_page, 0, aligned_size);
        pages_.emplace_back(PageInfo{std::move(new_page), {{aligned_size, DefaultPageSize}}, {}});
    }

    void GpuMemoryAllocator::Deallocate(GpuMemoryBlock&& mem_block, uint64_t fence_value)
    {
        if (mem_block)
        {
            std::lock_guard<std::mutex> lock(allocation_mutex_);
            this->Deallocate(lock, mem_block, fence_value);
        }
    }

    void GpuMemoryAllocator::Deallocate(
        [[maybe_unused]] std::lock_guard<std::mutex>& proof_of_lock, GpuMemoryBlock& mem_block, uint64_t fence_value)
    {
        assert(mem_block);

        if (mem_block.Size() <= DefaultPageSize)
        {
            for (auto& page : pages_)
            {
                if (page.page.Buffer().NativeResource() == mem_block.NativeResource())
                {
                    page.stall_list.push_back({{mem_block.Offset(), mem_block.Offset() + mem_block.Size()}, fence_value});
                    return;
                }
            }

            GOLDEN_SUN_UNREACHABLE("This memory block is not allocated by this allocator");
        }
    }

    void GpuMemoryAllocator::Reallocate(GpuMemoryBlock& mem_block, uint64_t fence_value, uint32_t size_in_bytes, uint32_t alignment)
    {
        std::lock_guard<std::mutex> lock(allocation_mutex_);

        if (mem_block)
        {
            this->Deallocate(lock, mem_block, fence_value);
        }
        this->Allocate(lock, mem_block, size_in_bytes, alignment);
    }

    void GpuMemoryAllocator::ClearStallPages(uint64_t fence_value)
    {
        std::lock_guard<std::mutex> lock(allocation_mutex_);

        for (auto& page : pages_)
        {
            for (auto stall_iter = page.stall_list.begin(); stall_iter != page.stall_list.end();)
            {
                if (stall_iter->fence_value <= fence_value)
                {
                    auto const free_iter = std::lower_bound(page.free_list.begin(), page.free_list.end(),
                        stall_iter->free_range.first_offset, [](PageInfo::FreeRange const& free_range, uint32_t first_offset) {
                            return free_range.first_offset < first_offset;
                        });
                    if (free_iter == page.free_list.end())
                    {
                        if (page.free_list.empty() || (page.free_list.back().last_offset != stall_iter->free_range.first_offset))
                        {
                            page.free_list.emplace_back(std::move(stall_iter->free_range));
                        }
                        else
                        {
                            page.free_list.back().last_offset = stall_iter->free_range.last_offset;
                        }
                    }
                    else if (free_iter->first_offset != stall_iter->free_range.last_offset)
                    {
                        bool merge_with_prev = false;
                        if (free_iter != page.free_list.begin())
                        {
                            auto const prev_free_iter = std::prev(free_iter);
                            if (prev_free_iter->last_offset == stall_iter->free_range.first_offset)
                            {
                                prev_free_iter->last_offset = stall_iter->free_range.last_offset;
                                merge_with_prev = true;
                            }
                        }

                        if (!merge_with_prev)
                        {
                            page.free_list.emplace(free_iter, std::move(stall_iter->free_range));
                        }
                    }
                    else
                    {
                        free_iter->first_offset = stall_iter->free_range.first_offset;
                        if (free_iter != page.free_list.begin())
                        {
                            auto const prev_free_iter = std::prev(free_iter);
                            if (prev_free_iter->last_offset == free_iter->first_offset)
                            {
                                prev_free_iter->last_offset = free_iter->last_offset;
                                page.free_list.erase(free_iter);
                            }
                        }
                    }

                    stall_iter = page.stall_list.erase(stall_iter);
                }
                else
                {
                    ++stall_iter;
                }
            }
        }

        large_pages_.clear();
    }

    void GpuMemoryAllocator::Clear()
    {
        std::lock_guard<std::mutex> lock(allocation_mutex_);

        pages_.clear();
        large_pages_.clear();
    }
} // namespace GoldenSun
