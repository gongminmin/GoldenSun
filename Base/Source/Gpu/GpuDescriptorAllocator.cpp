#include "../pch.hpp"

#include <GoldenSun/Gpu/GpuDescriptorAllocator.hpp>
#include <GoldenSun/Gpu/GpuSystem.hpp>

#include <cassert>

namespace
{
    uint32_t descriptor_size[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]{};

    uint16_t const default_descriptor_page_size[] = {32 * 1024, 1 * 1024, 8 * 1024, 4 * 1024};

    void UpdateDescriptorSize(GoldenSun::GpuSystem& gpu_system, D3D12_DESCRIPTOR_HEAP_TYPE type)
    {
        if (descriptor_size[type] == 0)
        {
            descriptor_size[type] = reinterpret_cast<ID3D12Device5*>(gpu_system.NativeDevice())->GetDescriptorHandleIncrementSize(type);
        }
    }
} // namespace

namespace GoldenSun
{
    GpuDescriptorPage::GpuDescriptorPage(
        GpuSystem& gpu_system, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags, uint32_t size)
    {
        heap_ = gpu_system.CreateDescriptorHeap(size, type, flags, L"GpuDescriptorPage");
        cpu_handle_ = heap_.CpuHandleStart();
        gpu_handle_ = heap_.GpuHandleStart();
    }

    GpuDescriptorPage::~GpuDescriptorPage() noexcept = default;
    GpuDescriptorPage::GpuDescriptorPage(GpuDescriptorPage&& other) noexcept = default;
    GpuDescriptorPage& GpuDescriptorPage::operator=(GpuDescriptorPage&& other) noexcept = default;


    GpuDescriptorBlock::GpuDescriptorBlock() noexcept = default;
    GpuDescriptorBlock::GpuDescriptorBlock(GpuDescriptorBlock&& other) noexcept = default;
    GpuDescriptorBlock& GpuDescriptorBlock::operator=(GpuDescriptorBlock&& other) noexcept = default;

    void GpuDescriptorBlock::Reset() noexcept
    {
        native_heap_ = nullptr;
        offset_ = 0;
        size_ = 0;
        cpu_handle_ = {};
        gpu_handle_ = {};
    }

    void GpuDescriptorBlock::Reset(GpuDescriptorPage const& page, uint32_t offset, uint32_t size) noexcept
    {
        native_heap_ = page.Heap().NativeDescriptorHeap();
        offset_ = offset;
        size_ = size;

        uint32_t const desc_size = descriptor_size[reinterpret_cast<ID3D12DescriptorHeap*>(native_heap_)->GetDesc().Type];

        std::tie(cpu_handle_, gpu_handle_) = OffsetHandle(page.CpuHandleStart(), page.GpuHandleStart(), offset, desc_size);
    }


    GpuDescriptorAllocator::GpuDescriptorAllocator(
        GpuSystem& gpu_system, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags) noexcept
        : gpu_system_(&gpu_system), type_(type), flags_(flags)
    {
    }

    GpuDescriptorAllocator::GpuDescriptorAllocator(GpuDescriptorAllocator&& other) noexcept
        : gpu_system_(std::move(other.gpu_system_)), type_(other.type_), flags_(other.flags_), pages_(std::move(other.pages_))
    {
    }

    GpuDescriptorAllocator& GpuDescriptorAllocator::operator=(GpuDescriptorAllocator&& other) noexcept
    {
        if (this != &other)
        {
            assert(type_ == other.type_);
            assert(flags_ == other.flags_);

            gpu_system_ = std::move(other.gpu_system_);
            pages_ = std::move(other.pages_);
        }
        return *this;
    }

    uint32_t GpuDescriptorAllocator::DescriptorSize() const
    {
        UpdateDescriptorSize(*gpu_system_, type_);
        return descriptor_size[type_];
    }

    GpuDescriptorBlock GpuDescriptorAllocator::Allocate(uint32_t size)
    {
        std::lock_guard<std::mutex> lock(allocation_mutex_);

        GpuDescriptorBlock desc_block;
        this->Allocate(lock, desc_block, size);
        return desc_block;
    }

    void GpuDescriptorAllocator::Allocate(
        [[maybe_unused]] std::lock_guard<std::mutex>& proof_of_lock, GpuDescriptorBlock& desc_block, uint32_t size)
    {
        UpdateDescriptorSize(*gpu_system_, type_);

        for (auto& page_info : pages_)
        {
            auto const iter = std::lower_bound(page_info.free_list.begin(), page_info.free_list.end(), size,
                [](PageInfo::FreeRange const& free_range, uint32_t s) { return free_range.first_offset + s > free_range.last_offset; });
            if (iter != page_info.free_list.end())
            {
                desc_block.Reset(page_info.page, iter->first_offset, size);
                iter->first_offset += static_cast<uint16_t>(size);
                if (iter->first_offset == iter->last_offset)
                {
                    page_info.free_list.erase(iter);
                }

                return;
            }
        }

        uint16_t const default_page_size = default_descriptor_page_size[type_];
        assert(size <= default_page_size);

        GpuDescriptorPage new_page(*gpu_system_, type_, flags_, default_page_size);
        desc_block.Reset(new_page, 0, size);
        pages_.emplace_back(PageInfo{std::move(new_page), {{static_cast<uint16_t>(size), default_page_size}}, {}});
    }

    void GpuDescriptorAllocator::Deallocate(GpuDescriptorBlock&& desc_block, uint64_t fence_value)
    {
        if (desc_block)
        {
            std::lock_guard<std::mutex> lock(allocation_mutex_);
            this->Deallocate(lock, desc_block, fence_value);
        }
    }

    void GpuDescriptorAllocator::Deallocate(
        [[maybe_unused]] std::lock_guard<std::mutex>& proof_of_lock, GpuDescriptorBlock& desc_block, uint64_t fence_value)
    {
        assert(desc_block);

        uint16_t const default_page_size = default_descriptor_page_size[type_];

        if (desc_block.Size() <= default_page_size)
        {
            for (auto& page : pages_)
            {
                if (page.page.Heap().NativeDescriptorHeap() == desc_block.NativeDescriptorHeap())
                {
                    page.stall_list.push_back(
                        {{static_cast<uint16_t>(desc_block.Offset()), static_cast<uint16_t>(desc_block.Offset() + desc_block.Size())},
                            fence_value});
                    return;
                }
            }

            GOLDEN_SUN_UNREACHABLE("This descriptor block is not allocated by this allocator");
        }
    }

    void GpuDescriptorAllocator::Reallocate(GpuDescriptorBlock& desc_block, uint64_t fence_value, uint32_t size)
    {
        std::lock_guard<std::mutex> lock(allocation_mutex_);

        if (desc_block)
        {
            this->Deallocate(lock, desc_block, fence_value);
        }
        this->Allocate(lock, desc_block, size);
    }

    void GpuDescriptorAllocator::ClearStallPages(uint64_t fence_value)
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
    }

    void GpuDescriptorAllocator::Clear()
    {
        std::lock_guard<std::mutex> lock(allocation_mutex_);

        pages_.clear();
    }
} // namespace GoldenSun
