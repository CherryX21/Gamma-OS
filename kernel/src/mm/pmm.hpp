#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bitmap.hpp"
#include "limine.h"

namespace gamma {

constexpr size_t kPageSize = 4096;

// Менеджер фізичних сторінок на основі бітової карти. Перший мегабайт пам'яті
// не видається. Фізична адреса 0 використовується як ознака помилки.
class PhysicalMemoryManager {
public:
    constexpr PhysicalMemoryManager() = default;

    // Будує карту за даними завантажувача. Повертає false, якщо для карти немає місця.
    bool init(const limine_memmap_response* map, uint64_t hhdm_offset);

    // Видає одну сторінку; повертає фізичну адресу або 0.
    uint64_t alloc_page();
    // Видає неперервну серію сторінок, початок вирівняний на align_pages сторінок.
    uint64_t alloc_contiguous(size_t pages, size_t align_pages = 1);

    void free_page(uint64_t phys);
    void free_contiguous(uint64_t phys, size_t pages);

    size_t total_pages() const { return total_pages_; }
    size_t free_pages() const { return free_pages_; }
    size_t bitmap_pages() const { return bitmap_pages_; }
    uint64_t hhdm_offset() const { return hhdm_offset_; }

private:
    Bitmap bitmap_;
    size_t total_pages_ = 0;
    size_t free_pages_ = 0;
    size_t bitmap_pages_ = 0;
    uint64_t hhdm_offset_ = 0;
};

extern PhysicalMemoryManager g_pmm;

// Перевіряє видачу/повернення сторінок; у разі помилки викликає panic.
void pmm_self_test();

}  // namespace gamma
