#pragma once

#include <stdint.h>

namespace gamma
{

    struct InterruptFrame
    {
        uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
        uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
        uint64_t vector;
        uint64_t error_code;
        uint64_t rip, cs, rflags, rsp, ss;
    };

    constexpr unsigned kIdtEntries = 48; // 32 виключення процесора та 16 ліній IRQ
    constexpr unsigned kIrqBase = 32;

    using IrqHandler = void (*)(InterruptFrame *frame);

    void idt_init();
    void irq_register(uint8_t irq, IrqHandler handler);

    void pic_init_masked();
    void pic_unmask(uint8_t irq);
    void pic_send_eoi(uint8_t irq);

    [[noreturn]] void panic(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

} // namespace gamma
