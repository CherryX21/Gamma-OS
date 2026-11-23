#pragma once

namespace gamma {

constexpr unsigned kKernelCodeSelector = 0x08;
constexpr unsigned kKernelDataSelector = 0x10;
constexpr unsigned kTssSelector = 0x18;

// Встановлює власну GDT (код, дані, TSS) і перезавантажує сегментні регістри.
void gdt_init();

}  // namespace gamma
