#include "interrupts.hpp"

#include <stdarg.h>
#include <stddef.h>

#include "console.hpp"
#include "gdt.hpp"
#include "io.hpp"
#include "log.hpp"

extern "C" const uint64_t isr_stub_table[gamma::kIdtEntries];

namespace gamma {

namespace {

struct [[gnu::packed]] IdtEntry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
};

struct [[gnu::packed]] DescriptorPointer {
    uint16_t limit;
    uint64_t base;
};

constexpr uint8_t kGateInterrupt = 0x8E;  // present, DPL 0, 64-bit interrupt gate
constexpr unsigned kDoubleFault = 8;
constexpr unsigned kBreakpoint = 3;
constexpr unsigned kPageFault = 14;

alignas(16) IdtEntry g_idt[256];

const char* const kExceptionNames[32] = {
    "Divide Error (#DE)",
    "Debug (#DB)",
    "Non-Maskable Interrupt",
    "Breakpoint (#BP)",
    "Overflow (#OF)",
    "BOUND Range Exceeded (#BR)",
    "Invalid Opcode (#UD)",
    "Device Not Available (#NM)",
    "Double Fault (#DF)",
    "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)",
    "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)",
    "General Protection Fault (#GP)",
    "Page Fault (#PF)",
    "Reserved",
    "x87 Floating-Point Error (#MF)",
    "Alignment Check (#AC)",
    "Machine Check (#MC)",
    "SIMD Floating-Point Exception (#XM)",
    "Virtualization Exception (#VE)",
    "Control Protection Exception (#CP)",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Hypervisor Injection Exception (#HV)",
    "VMM Communication Exception (#VC)",
    "Security Exception (#SX)",
    "Reserved",
};

void set_gate(unsigned vector, uint64_t handler, uint8_t ist) {
    IdtEntry& e = g_idt[vector];
    e.offset_low = static_cast<uint16_t>(handler & 0xFFFF);
    e.selector = kKernelCodeSelector;
    e.ist = ist;
    e.type_attr = kGateInterrupt;
    e.offset_mid = static_cast<uint16_t>((handler >> 16) & 0xFFFF);
    e.offset_high = static_cast<uint32_t>(handler >> 32);
    e.reserved = 0;
}

uint64_t read_cr2() {
    uint64_t v;
    asm volatile("movq %%cr2, %0" : "=r"(v));
    return v;
}

uint64_t read_cr3() {
    uint64_t v;
    asm volatile("movq %%cr3, %0" : "=r"(v));
    return v;
}

void enter_panic_mode() {
    g_console.set_colors(0xFFFFFF, 0xA00000);
}

void dump_frame(const InterruptFrame& f) {
    kprintf("  RIP=%016lx  CS=%04lx  RFLAGS=%016lx\n", f.rip, f.cs, f.rflags);
    kprintf("  RSP=%016lx  SS=%04lx  ERR=%016lx\n", f.rsp, f.ss, f.error_code);
    kprintf("  RAX=%016lx  RBX=%016lx  RCX=%016lx\n", f.rax, f.rbx, f.rcx);
    kprintf("  RDX=%016lx  RSI=%016lx  RDI=%016lx\n", f.rdx, f.rsi, f.rdi);
    kprintf("  RBP=%016lx  R8 =%016lx  R9 =%016lx\n", f.rbp, f.r8, f.r9);
    kprintf("  R10=%016lx  R11=%016lx  R12=%016lx\n", f.r10, f.r11, f.r12);
    kprintf("  R13=%016lx  R14=%016lx  R15=%016lx\n", f.r13, f.r14, f.r15);
    kprintf("  CR3=%016lx\n", read_cr3());
}

[[noreturn]] void fatal_exception(const InterruptFrame& f) {
    enter_panic_mode();
    const char* name = f.vector < 32 ? kExceptionNames[f.vector] : "Unexpected interrupt";
    kprintf("\n*** KERNEL PANIC: %s, vector %lu ***\n", name, f.vector);
    if (f.vector == kPageFault) {
        kprintf("  CR2=%016lx (faulting address)\n", read_cr2());
    }
    dump_frame(f);
    kprintf("System halted.\n");
#ifdef GAMMA_AUTOTEST
    qemu_exit(0x10);
#endif
    halt_forever();
}

}  // namespace

extern "C" void isr_dispatch(InterruptFrame* frame) {
    if (frame->vector == kBreakpoint) {
        kprintf("[idt] breakpoint at %016lx (handled)\n", frame->rip);
        return;
    }
    fatal_exception(*frame);
}

void idt_init() {
    for (unsigned v = 0; v < kIdtEntries; v++) {
        set_gate(v, isr_stub_table[v], v == kDoubleFault ? 1 : 0);
    }
    DescriptorPointer ptr{static_cast<uint16_t>(sizeof(g_idt) - 1), reinterpret_cast<uint64_t>(g_idt)};
    asm volatile("lidt %0" : : "m"(ptr) : "memory");
}

[[noreturn]] void panic(const char* fmt, ...) {
    enter_panic_mode();
    kprintf("\n*** KERNEL PANIC: ");
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    kprintf(" ***\nSystem halted.\n");
#ifdef GAMMA_AUTOTEST
    qemu_exit(0x10);
#endif
    halt_forever();
}

}  // namespace gamma
