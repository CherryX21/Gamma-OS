#include "gdt.hpp"

#include <stddef.h>
#include <stdint.h>

namespace gamma {

namespace {

struct [[gnu::packed]] Tss {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t io_map_base;
};

struct [[gnu::packed]] DescriptorPointer {
    uint16_t limit;
    uint64_t base;
};

constexpr size_t kDoubleFaultStackSize = 16384;

alignas(16) uint8_t g_double_fault_stack[kDoubleFaultStackSize];
alignas(16) Tss g_tss;
alignas(16) uint64_t g_gdt[5];

}  // namespace

void gdt_init() {
    g_tss.ist[0] = reinterpret_cast<uint64_t>(g_double_fault_stack) + kDoubleFaultStackSize;
    g_tss.io_map_base = sizeof(Tss);

    const uint64_t tss_base = reinterpret_cast<uint64_t>(&g_tss);
    const uint64_t tss_limit = sizeof(Tss) - 1;

    g_gdt[0] = 0;
    g_gdt[1] = 0x00AF9A000000FFFFULL;  // 64-бітний сегмент коду, DPL 0
    g_gdt[2] = 0x00CF92000000FFFFULL;  // сегмент даних, DPL 0
    g_gdt[3] = (tss_limit & 0xFFFF) | ((tss_base & 0xFFFFFF) << 16) | (0x89ULL << 40) |
               (((tss_limit >> 16) & 0xF) << 48) | (((tss_base >> 24) & 0xFF) << 56);
    g_gdt[4] = tss_base >> 32;

    DescriptorPointer ptr{static_cast<uint16_t>(sizeof(g_gdt) - 1),
                          reinterpret_cast<uint64_t>(g_gdt)};
    asm volatile("lgdt %0" : : "m"(ptr) : "memory");

    asm volatile(
        "pushq %[code]\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw %[data], %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "xorl %%eax, %%eax\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        :
        : [code] "i"(kKernelCodeSelector), [data] "i"(kKernelDataSelector)
        : "rax", "memory");

    asm volatile("ltr %0" : : "r"(static_cast<uint16_t>(kTssSelector)) : "memory");
}

}  // namespace gamma
