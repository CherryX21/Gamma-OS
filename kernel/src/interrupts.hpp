#pragma once

#include <stdint.h>

namespace gamma {

// Стан процесора, збережений при вході в переривання. Порядок полів відповідає
// послідовності push у isr_common (isr_stubs.cpp).
struct InterruptFrame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t vector;
    uint64_t error_code;
    uint64_t rip, cs, rflags, rsp, ss;
};

constexpr unsigned kIdtEntries = 48;  // 32 виключення процесора та 16 ліній IRQ

// Створює IDT, вказує обробники та завантажує її. Апаратні переривання залишаються вимкненими.
void idt_init();

// Перепризначає PIC на вектори 0x20-0x2F і маскує всі лінії.
void pic_init_masked();

// Повідомлення про фатальну помилку: дамп регістрів та зупинка системи.
[[noreturn]] void panic(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

}  // namespace gamma
