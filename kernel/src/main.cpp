#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "console.hpp"
#include "gdt.hpp"
#include "interrupts.hpp"
#include "io.hpp"
#include "limine.h"
#include "log.hpp"
#include "pmm.hpp"
#include "serial.hpp"

__attribute__((used, section(".limine_requests_start")))
static volatile uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests")))
static volatile uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests")))
static volatile limine_framebuffer_request framebuffer_request = {
    LIMINE_FRAMEBUFFER_REQUEST_ID, 0, nullptr};

__attribute__((used, section(".limine_requests")))
static volatile limine_memmap_request memmap_request = {LIMINE_MEMMAP_REQUEST_ID, 0, nullptr};

__attribute__((used, section(".limine_requests")))
static volatile limine_hhdm_request hhdm_request = {LIMINE_HHDM_REQUEST_ID, 0, nullptr};

__attribute__((used, section(".limine_requests_end")))
static volatile uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

namespace {

const char* memmap_type_name(uint64_t type) {
    switch (type) {
        case LIMINE_MEMMAP_USABLE: return "usable";
        case LIMINE_MEMMAP_RESERVED: return "reserved";
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE: return "acpi reclaimable";
        case LIMINE_MEMMAP_ACPI_NVS: return "acpi nvs";
        case LIMINE_MEMMAP_BAD_MEMORY: return "bad memory";
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE: return "bootloader reclaimable";
        case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES: return "kernel and modules";
        case LIMINE_MEMMAP_FRAMEBUFFER: return "framebuffer";
        default: return "unknown";
    }
}

void serial_emit(char c, void*) {
    gamma::g_serial.put_char(c);
}

// Виводить лише в послідовний порт: для докладних даних, що не мають займати екран.
void serial_printf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    gamma::vformat(serial_emit, nullptr, fmt, ap);
    va_end(ap);
}

void print_memory_map() {
    limine_memmap_response* resp = memmap_request.response;
    if (resp == nullptr) {
        gamma::kprintf("[mem] no memory map from bootloader\n");
        return;
    }
    uint64_t usable = 0;
    serial_printf("[mem] memory map (%lu entries):\n", resp->entry_count);
    for (uint64_t i = 0; i < resp->entry_count; i++) {
        const limine_memmap_entry* e = resp->entries[i];
        serial_printf("  %016lx - %016lx  %s\n", e->base, e->base + e->length,
                      memmap_type_name(e->type));
        if (e->type == LIMINE_MEMMAP_USABLE) {
            usable += e->length;
        }
    }
    gamma::kprintf("[mem] %lu entries, usable: %lu KiB (%lu MiB), full map in COM1 log\n",
                   resp->entry_count, usable / 1024, usable / (1024 * 1024));
}

void run_fault_test() {
#if defined(GAMMA_FAULT_div0)
    // Інструкція div виконується явно: компілятор може замінити ділення на нуль (UB) іншим кодом.
    asm volatile("movl $1, %%eax; xorl %%edx, %%edx; xorl %%ecx, %%ecx; divl %%ecx"
                 : : : "eax", "ecx", "edx");
#elif defined(GAMMA_FAULT_ud)
    asm volatile("ud2");
#elif defined(GAMMA_FAULT_pf)
    *reinterpret_cast<volatile int*>(0xDEAD0000ULL) = 1;
#elif defined(GAMMA_FAULT_gp)
    *reinterpret_cast<volatile int*>(0x8000000000000000ULL) = 1;
#elif defined(GAMMA_FAULT_df)
    asm volatile("movq $0, %rsp; pushq $0");
#elif defined(GAMMA_FAULT_panic)
    gamma::panic("test panic, code %d", 42);
#endif
}

}  // namespace

extern "C" void kmain() {
    gamma::g_serial.init();
    gamma::log_add_sink(&gamma::g_serial);

    if (!LIMINE_BASE_REVISION_SUPPORTED(limine_base_revision)) {
        gamma::kprintf("Bootloader does not support the requested base revision\n");
        gamma::halt_forever();
    }

    limine_framebuffer_response* fb_resp = framebuffer_request.response;
    if (fb_resp != nullptr && fb_resp->framebuffer_count > 0 &&
        gamma::g_console.init(fb_resp->framebuffers[0], 2)) {
        gamma::log_add_sink(&gamma::g_console);
    } else {
        gamma::kprintf("[warn] framebuffer console unavailable, using serial only\n");
    }

    gamma::kprintf("Gamma OS 0.1 (x86_64, UEFI)\n");
    gamma::kprintf("Kernel started.\n");
    if (gamma::g_console.columns() != 0) {
        gamma::kprintf("[fb] console %ux%u characters\n", gamma::g_console.columns(),
                       gamma::g_console.rows());
    }

    gamma::gdt_init();
    gamma::kprintf("[gdt] loaded (code=0x08, data=0x10, tss=0x18)\n");
    gamma::pic_init_masked();
    gamma::idt_init();
    gamma::kprintf("[idt] loaded, %u vectors, PIC remapped and masked\n", gamma::kIdtEntries);

    limine_hhdm_response* hhdm = hhdm_request.response;
    if (hhdm != nullptr) {
        gamma::kprintf("[mem] HHDM offset: %p\n", reinterpret_cast<void*>(hhdm->offset));
    }
    print_memory_map();

    if (hhdm == nullptr || !gamma::g_pmm.init(memmap_request.response, hhdm->offset)) {
        gamma::panic("physical memory manager initialization failed");
    }
    gamma::kprintf("[pmm] %lu pages tracked, %lu free (%lu MiB), bitmap %lu KiB\n",
                   static_cast<uint64_t>(gamma::g_pmm.total_pages()),
                   static_cast<uint64_t>(gamma::g_pmm.free_pages()),
                   static_cast<uint64_t>(gamma::g_pmm.free_pages() * gamma::kPageSize / (1024 * 1024)),
                   static_cast<uint64_t>(gamma::g_pmm.bitmap_pages() * gamma::kPageSize / 1024));
    gamma::pmm_self_test();

    asm volatile("int3");
    gamma::kprintf("[idt] execution resumed after breakpoint\n");

    run_fault_test();

    gamma::kprintf("Initialization complete. Halting.\n");
#ifdef GAMMA_AUTOTEST
    gamma::kprintf("[test] all checks passed\n");
    gamma::qemu_exit(0);
#endif
    gamma::halt_forever();
}
