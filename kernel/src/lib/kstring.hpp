#pragma once

#include <stddef.h>

// Прототипи функцій з runtime.cpp (стандартна бібліотека недоступна в freestanding-режимі).
extern "C" {
void* memcpy(void* dst, const void* src, size_t n);
void* memmove(void* dst, const void* src, size_t n);
void* memset(void* dst, int value, size_t n);
int memcmp(const void* a, const void* b, size_t n);
}
