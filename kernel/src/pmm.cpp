#include "pmm.hpp"

#include "interrupts.hpp"
#include "log.hpp"

namespace gamma {

PhysicalMemoryManager g_pmm;

namespace {

constexpr uint64_t kReservedLowBytes = 1024 * 1024;

uint64_t align_up(uint64_t value, uint64_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

}  // namespace

bool PhysicalMemoryManager::init(const limine_memmap_response* map, uint64_t hhdm_offset) {
    if (map == nullptr) {
        return false;
    }
    hhdm_offset_ = hhdm_offset;

    uint64_t highest = 0;
    for (uint64_t i = 0; i < map->entry_count; i++) {
        const limine_memmap_entry* e = map->entries[i];
        if (e->type == LIMINE_MEMMAP_USABLE && e->base + e->length > highest) {
            highest = e->base + e->length;
        }
    }
    const size_t frames = static_cast<size_t>(highest / kPageSize);
    if (frames == 0) {
        return false;
    }

    const size_t bitmap_bytes = Bitmap::words_for(frames) * sizeof(uint64_t);
    const size_t pages_needed = static_cast<size_t>(align_up(bitmap_bytes, kPageSize) / kPageSize);

    uint64_t bitmap_phys = 0;
    for (uint64_t i = 0; i < map->entry_count && bitmap_phys == 0; i++) {
        const limine_memmap_entry* e = map->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }
        uint64_t start = align_up(e->base, kPageSize);
        if (start < kReservedLowBytes) {
            start = kReservedLowBytes;
        }
        const uint64_t end = e->base + e->length;
        if (start < end && end - start >= pages_needed * kPageSize) {
            bitmap_phys = start;
        }
    }
    if (bitmap_phys == 0) {
        return false;
    }

    bitmap_.init(reinterpret_cast<uint64_t*>(hhdm_offset + bitmap_phys), frames);

    for (uint64_t i = 0; i < map->entry_count; i++) {
        const limine_memmap_entry* e = map->entries[i];
        if (e->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }
        uint64_t start = align_up(e->base, kPageSize);
        if (start < kReservedLowBytes) {
            start = kReservedLowBytes;
        }
        const uint64_t end = (e->base + e->length) / kPageSize * kPageSize;
        if (start < end) {
            bitmap_.clear_range(static_cast<size_t>(start / kPageSize),
                                static_cast<size_t>((end - start) / kPageSize));
        }
    }
    bitmap_.set_range(static_cast<size_t>(bitmap_phys / kPageSize), pages_needed);

    total_pages_ = frames;
    bitmap_pages_ = pages_needed;
    free_pages_ = bitmap_.count_clear();
    return true;
}

uint64_t PhysicalMemoryManager::alloc_page() {
    const size_t index = bitmap_.find_first_clear();
    if (index == Bitmap::npos) {
        return 0;
    }
    bitmap_.set(index);
    free_pages_--;
    return static_cast<uint64_t>(index) * kPageSize;
}

uint64_t PhysicalMemoryManager::alloc_contiguous(size_t pages, size_t align_pages) {
    const size_t index = bitmap_.find_clear_run(pages, align_pages);
    if (index == Bitmap::npos) {
        return 0;
    }
    bitmap_.set_range(index, pages);
    free_pages_ -= pages;
    return static_cast<uint64_t>(index) * kPageSize;
}

void PhysicalMemoryManager::free_page(uint64_t phys) {
    free_contiguous(phys, 1);
}

void PhysicalMemoryManager::free_contiguous(uint64_t phys, size_t pages) {
    if (phys % kPageSize != 0 || pages == 0) {
        panic("pmm: invalid free of %p (%lu pages)", reinterpret_cast<void*>(phys),
              static_cast<uint64_t>(pages));
    }
    const size_t first = static_cast<size_t>(phys / kPageSize);
    if (first >= total_pages_ || pages > total_pages_ - first) {
        panic("pmm: free outside managed memory: %p", reinterpret_cast<void*>(phys));
    }
    for (size_t i = 0; i < pages; i++) {
        if (!bitmap_.test(first + i)) {
            panic("pmm: double free of page %p", reinterpret_cast<void*>(phys + i * kPageSize));
        }
    }
    bitmap_.clear_range(first, pages);
    free_pages_ += pages;
}

void pmm_self_test() {
    const size_t initial_free = g_pmm.free_pages();

    const uint64_t a = g_pmm.alloc_page();
    const uint64_t b = g_pmm.alloc_page();
    if (a == 0 || b == 0 || a == b) {
        panic("pmm self-test: allocation failed (a=%p, b=%p)", reinterpret_cast<void*>(a),
              reinterpret_cast<void*>(b));
    }

    volatile uint64_t* mapped = reinterpret_cast<volatile uint64_t*>(g_pmm.hhdm_offset() + a);
    *mapped = 0x47616D6D614F5321ULL;
    if (*mapped != 0x47616D6D614F5321ULL) {
        panic("pmm self-test: page %p is not writable through HHDM", reinterpret_cast<void*>(a));
    }

    g_pmm.free_page(a);
    g_pmm.free_page(b);
    if (g_pmm.free_pages() != initial_free) {
        panic("pmm self-test: free page counter mismatch");
    }

    const uint64_t c = g_pmm.alloc_page();
    if (c != a) {
        panic("pmm self-test: lowest free page was not reused (%p != %p)",
              reinterpret_cast<void*>(c), reinterpret_cast<void*>(a));
    }

    const uint64_t run = g_pmm.alloc_contiguous(16, 16);
    if (run == 0 || run % (16 * kPageSize) != 0) {
        panic("pmm self-test: contiguous allocation failed (%p)", reinterpret_cast<void*>(run));
    }
    g_pmm.free_contiguous(run, 16);
    g_pmm.free_page(c);

    if (g_pmm.free_pages() != initial_free) {
        panic("pmm self-test: pages leaked (%lu != %lu)", static_cast<uint64_t>(g_pmm.free_pages()),
              static_cast<uint64_t>(initial_free));
    }
    kprintf("[pmm] self-test passed\n");
}

}  // namespace gamma
