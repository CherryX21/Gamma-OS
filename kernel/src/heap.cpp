#include "heap.hpp"

#include <stdint.h>

#ifndef GAMMA_HOST_TEST
#include "interrupts.hpp"
#include "kstring.hpp"
#include "log.hpp"
#include "pmm.hpp"
#else
#include <cstring>
#endif

namespace gamma
{

    namespace
    {

        constexpr size_t kAlign = 16;
        constexpr size_t kMinSplitRemainder = 32;

        size_t align_up(size_t val, size_t align)
        {
            return (val + align - 1) & ~(align - 1);
        }

        HeapAllocator g_kernel_heap;

#ifndef GAMMA_HOST_TEST
        void *kernel_page_provider(size_t pages)
        {
            const uint64_t phys = g_pmm.alloc_contiguous(pages);
            if (phys == 0)
            {
                return nullptr;
            }
            return reinterpret_cast<void *>(g_pmm.hhdm_offset() + phys);
        }
#endif

    } // namespace

    void HeapAllocator::init(PageProvider provider, size_t page_size)
    {
        provider_ = provider;
        page_size_ = page_size;
        head_ = nullptr;
    }

    bool HeapAllocator::expand(size_t min_bytes)
    {
        if (provider_ == nullptr)
        {
            return false;
        }
        const size_t header_size = align_up(sizeof(BlockHeader), kAlign);
        const size_t total_needed = header_size + min_bytes;
        const size_t pages = (total_needed + page_size_ - 1) / page_size_;
        const size_t alloc_pages = pages < 4 ? 4 : pages; // виділяємо мінімум 16 КіБ за раз

        void *mem = provider_(alloc_pages);
        if (mem == nullptr)
        {
            return false;
        }

        auto *block = reinterpret_cast<BlockHeader *>(mem);
        block->size = alloc_pages * page_size_ - header_size;
        block->free = true;
        block->prev = nullptr;
        block->next = nullptr;

        if (head_ == nullptr)
        {
            head_ = block;
            return true;
        }

        BlockHeader *tail = head_;
        while (tail->next != nullptr)
        {
            tail = tail->next;
        }
        tail->next = block;
        block->prev = tail;
        coalesce(block);
        return true;
    }

    void HeapAllocator::split_block(BlockHeader *block, size_t size)
    {
        const size_t header_size = align_up(sizeof(BlockHeader), kAlign);
        if (block->size < size + header_size + kMinSplitRemainder)
        {
            return;
        }
        auto *base = reinterpret_cast<uint8_t *>(block);
        auto *next_block = reinterpret_cast<BlockHeader *>(base + header_size + size);
        next_block->size = block->size - size - header_size;
        next_block->free = true;
        next_block->prev = block;
        next_block->next = block->next;

        if (block->next != nullptr)
        {
            block->next->prev = next_block;
        }
        block->next = next_block;
        block->size = size;
    }

    void HeapAllocator::coalesce(BlockHeader *block)
    {
        const size_t header_size = align_up(sizeof(BlockHeader), kAlign);

        // Об'єднання з наступним блоком, якщо він фізично суміжний і вільний.
        if (block->next != nullptr && block->next->free)
        {
            auto *expected_next = reinterpret_cast<uint8_t *>(block) + header_size + block->size;
            if (reinterpret_cast<uint8_t *>(block->next) == expected_next)
            {
                BlockHeader *victim = block->next;
                block->size += header_size + victim->size;
                block->next = victim->next;
                if (victim->next != nullptr)
                {
                    victim->next->prev = block;
                }
            }
        }

        // Об'єднання з попереднім блоком, якщо він фізично суміжний і вільний.
        if (block->prev != nullptr && block->prev->free)
        {
            BlockHeader *prev = block->prev;
            auto *expected_curr = reinterpret_cast<uint8_t *>(prev) + header_size + prev->size;
            if (reinterpret_cast<uint8_t *>(block) == expected_curr)
            {
                prev->size += header_size + block->size;
                prev->next = block->next;
                if (block->next != nullptr)
                {
                    block->next->prev = prev;
                }
            }
        }
    }

    void *HeapAllocator::alloc(size_t size)
    {
        if (size == 0)
        {
            size = 1;
        }
        const size_t aligned_size = align_up(size, kAlign);
        const size_t header_size = align_up(sizeof(BlockHeader), kAlign);

        for (int attempt = 0; attempt < 2; attempt++)
        {
            for (BlockHeader *curr = head_; curr != nullptr; curr = curr->next)
            {
                if (curr->free && curr->size >= aligned_size)
                {
                    split_block(curr, aligned_size);
                    curr->free = false;
                    return reinterpret_cast<uint8_t *>(curr) + header_size;
                }
            }
            if (!expand(aligned_size))
            {
                return nullptr;
            }
        }
        return nullptr;
    }

    void HeapAllocator::free(void *ptr)
    {
        if (ptr == nullptr)
        {
            return;
        }
        const size_t header_size = align_up(sizeof(BlockHeader), kAlign);
        auto *block = reinterpret_cast<BlockHeader *>(reinterpret_cast<uint8_t *>(ptr) - header_size);
        block->free = true;
        coalesce(block);
    }

    HeapStats HeapAllocator::stats() const
    {
        HeapStats st{0, 0, 0, 0};
        for (const BlockHeader *curr = head_; curr != nullptr; curr = curr->next)
        {
            st.total_bytes += curr->size;
            st.block_count++;
            if (curr->free)
            {
                st.free_bytes += curr->size;
            }
            else
            {
                st.used_bytes += curr->size;
            }
        }
        return st;
    }

#ifndef GAMMA_HOST_TEST
    void heap_init()
    {
        g_kernel_heap.init(kernel_page_provider, kPageSize);
    }

    void *kmalloc(size_t size)
    {
        return g_kernel_heap.alloc(size);
    }

    void *kzalloc(size_t size)
    {
        void *ptr = kmalloc(size);
        if (ptr != nullptr)
        {
            memset(ptr, 0, size);
        }
        return ptr;
    }

    void kfree(void *ptr)
    {
        g_kernel_heap.free(ptr);
    }

    HeapStats heap_stats()
    {
        return g_kernel_heap.stats();
    }

    void heap_self_test()
    {
        void *p1 = kmalloc(64);
        void *p2 = kmalloc(256);
        void *p3 = kmalloc(128);
        if (p1 == nullptr || p2 == nullptr || p3 == nullptr)
        {
            panic("heap self-test: allocation returned nullptr");
        }
        memset(p1, 0xAA, 64);
        memset(p2, 0xBB, 256);
        kfree(p2);
        kfree(p1);
        void *p4 = kmalloc(300); // має перевикористати об'єднаний блок p1+p2
        if (p4 != p1)
        {
            panic("heap self-test: coalescing failed");
        }
        kfree(p4);
        kfree(p3);
        const HeapStats st = heap_stats();
        if (st.used_bytes != 0)
        {
            panic("heap self-test: memory leak detected (%lu bytes)", static_cast<uint64_t>(st.used_bytes));
        }
        kprintf("[heap] self-test passed (%lu KiB pool, %lu blocks)\n",
                static_cast<uint64_t>(st.total_bytes / 1024), static_cast<uint64_t>(st.block_count));
    }
#endif

} // namespace gamma
