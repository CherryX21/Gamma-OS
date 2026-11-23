#include "vmm.hpp"

#include "interrupts.hpp"
#include "kstring.hpp"
#include "log.hpp"
#include "pmm.hpp"

namespace gamma {

namespace {

constexpr uint64_t kAddrMask = 0x000FFFFFFFFFF000ULL;
uint64_t g_pml4_phys = 0;

uint64_t* phys_to_virt(uint64_t phys) {
    return reinterpret_cast<uint64_t*>(g_pmm.hhdm_offset() + phys);
}

void invlpg(uint64_t virt) {
    asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

uint64_t* get_or_create_table(uint64_t* table, size_t index, uint64_t flags) {
    if ((table[index] & kPagePresent) != 0) {
        return phys_to_virt(table[index] & kAddrMask);
    }
    const uint64_t new_table_phys = g_pmm.alloc_page();
    if (new_table_phys == 0) {
        return nullptr;
    }
    uint64_t* new_table = phys_to_virt(new_table_phys);
    memset(new_table, 0, kPageSize);
    const uint64_t entry_flags = kPagePresent | kPageWrite | (flags & kPageUser);
    table[index] = (new_table_phys & kAddrMask) | entry_flags;
    return new_table;
}

}  // namespace

void vmm_init() {
    uint64_t cr3 = 0;
    asm volatile("movq %%cr3, %0" : "=r"(cr3));
    g_pml4_phys = cr3 & kAddrMask;
}

bool vmm_map_page(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (g_pml4_phys == 0 || (virt % kPageSize) != 0 || (phys % kPageSize) != 0) {
        return false;
    }
    const size_t pml4_i = (virt >> 39) & 0x1FF;
    const size_t pdpt_i = (virt >> 30) & 0x1FF;
    const size_t pd_i   = (virt >> 21) & 0x1FF;
    const size_t pt_i   = (virt >> 12) & 0x1FF;

    uint64_t* pml4 = phys_to_virt(g_pml4_phys);
    uint64_t* pdpt = get_or_create_table(pml4, pml4_i, flags);
    if (pdpt == nullptr) return false;

    uint64_t* pd = get_or_create_table(pdpt, pdpt_i, flags);
    if (pd == nullptr) return false;

    uint64_t* pt = get_or_create_table(pd, pd_i, flags);
    if (pt == nullptr) return false;

    pt[pt_i] = (phys & kAddrMask) | flags | kPagePresent;
    invlpg(virt);
    return true;
}

bool vmm_unmap_page(uint64_t virt) {
    if (g_pml4_phys == 0 || (virt % kPageSize) != 0) {
        return false;
    }
    const size_t pml4_i = (virt >> 39) & 0x1FF;
    const size_t pdpt_i = (virt >> 30) & 0x1FF;
    const size_t pd_i   = (virt >> 21) & 0x1FF;
    const size_t pt_i   = (virt >> 12) & 0x1FF;

    uint64_t* pml4 = phys_to_virt(g_pml4_phys);
    if ((pml4[pml4_i] & kPagePresent) == 0) return false;

    uint64_t* pdpt = phys_to_virt(pml4[pml4_i] & kAddrMask);
    if ((pdpt[pdpt_i] & kPagePresent) == 0) return false;

    uint64_t* pd = phys_to_virt(pdpt[pdpt_i] & kAddrMask);
    if ((pd[pd_i] & kPagePresent) == 0) return false;

    uint64_t* pt = phys_to_virt(pd[pd_i] & kAddrMask);
    if ((pt[pt_i] & kPagePresent) == 0) return false;

    pt[pt_i] = 0;
    invlpg(virt);
    return true;
}

uint64_t vmm_virt_to_phys(uint64_t virt) {
    if (g_pml4_phys == 0) return 0;

    const size_t pml4_i = (virt >> 39) & 0x1FF;
    const size_t pdpt_i = (virt >> 30) & 0x1FF;
    const size_t pd_i   = (virt >> 21) & 0x1FF;
    const size_t pt_i   = (virt >> 12) & 0x1FF;

    uint64_t* pml4 = phys_to_virt(g_pml4_phys);
    if ((pml4[pml4_i] & kPagePresent) == 0) return 0;

    uint64_t* pdpt = phys_to_virt(pml4[pml4_i] & kAddrMask);
    if ((pdpt[pdpt_i] & kPagePresent) == 0) return 0;

    uint64_t* pd = phys_to_virt(pdpt[pdpt_i] & kAddrMask);
    if ((pd[pd_i] & kPagePresent) == 0) return 0;

    uint64_t* pt = phys_to_virt(pd[pd_i] & kAddrMask);
    if ((pt[pt_i] & kPagePresent) == 0) return 0;

    return (pt[pt_i] & kAddrMask) + (virt & 0xFFF);
}

void vmm_self_test() {
    constexpr uint64_t kTestVirt = 0xFFFF900000000000ULL;
    const uint64_t phys = g_pmm.alloc_page();
    if (phys == 0) {
        panic("vmm self-test: out of physical memory");
    }
    if (!vmm_map_page(kTestVirt, phys, kPagePresent | kPageWrite)) {
        panic("vmm self-test: vmm_map_page failed");
    }
    if (vmm_virt_to_phys(kTestVirt + 0x123) != phys + 0x123) {
        panic("vmm self-test: virt_to_phys mismatch");
    }
    volatile uint64_t* ptr = reinterpret_cast<volatile uint64_t*>(kTestVirt);
    *ptr = 0xCAFEBABE12345678ULL;
    if (*ptr != 0xCAFEBABE12345678ULL) {
        panic("vmm self-test: mapped page read/write failed");
    }
    if (!vmm_unmap_page(kTestVirt) || vmm_virt_to_phys(kTestVirt) != 0) {
        panic("vmm self-test: unmap failed");
    }
    g_pmm.free_page(phys);
    kprintf("[vmm] 4-level paging self-test passed (PML4=%p)\n", reinterpret_cast<void*>(g_pml4_phys));
}

}  // namespace gamma
