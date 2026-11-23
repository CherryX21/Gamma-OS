#pragma once

#include <stdint.h>

namespace gamma {

inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

inline void outl(uint16_t port, uint32_t value) {
    asm volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

// Завершує роботу QEMU через пристрій isa-debug-exit (iobase=0xf4). Код виходу процесу
// QEMU дорівнює (code << 1) | 1. Використовується лише в автоматичних тестах.
inline void qemu_exit(uint32_t code) {
    outl(0xF4, code);
}

[[noreturn]] inline void halt_forever() {
    for (;;) {
        asm volatile("cli; hlt");
    }
}

}  // namespace gamma
