#pragma once

#include <stdarg.h>

#include "text_sink.hpp"

namespace gamma {

// Реєструє приймач виводу (до 4 штук). Усі повідомлення kprintf надходять у кожен з них.
bool log_add_sink(TextSink* sink);

void kvprintf(const char* fmt, va_list ap);
void kprintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// Підтримувані специфікатори: %c %s %d %i %u %x %X %p %%, модифікатори l/ll/z, ширина та прапорці 0 і -.
typedef void (*EmitFn)(char c, void* ctx);
void vformat(EmitFn emit, void* ctx, const char* fmt, va_list ap);

}  // namespace gamma
