#pragma once

#include <stdint.h>

namespace gamma
{

    constexpr uint64_t kPagePresent = 1ULL << 0;
    constexpr uint64_t kPageWrite = 1ULL << 1;
    constexpr uint64_t kPageUser = 1ULL << 2;
    constexpr uint64_t kPageNoExec = 1ULL << 63;

    // Ініціалізує VMM поверх поточної 4-рівневої таблиці сторінок (PML4) від Limine.
    void vmm_init();

    // Відображає віртуальну сторінку virt на фізичну сторінку phys із заданими прапорцями.
    bool vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags = kPagePresent | kPageWrite);

    // Знімає відображення віртуальної сторінки та скидає запис у TLB.
    bool vmm_unmap_page(uint64_t virt);

    // Повертає фізичну адресу для віртуальної або 0, якщо сторінка не замаплена.
    uint64_t vmm_virt_to_phys(uint64_t virt);

    // Самоперевірка мапінгу, запису та розмапінгу сторінок.
    void vmm_self_test();

} // namespace gamma
