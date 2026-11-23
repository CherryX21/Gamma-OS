#include "log.hpp"

#include <stdint.h>

namespace gamma {

namespace {

constexpr int kMaxSinks = 4;
TextSink* g_sinks[kMaxSinks];
int g_sink_count = 0;

void emit_to_sinks(char c, void*) {
    for (int i = 0; i < g_sink_count; i++) {
        g_sinks[i]->put_char(c);
    }
}

void emit_padding(EmitFn emit, void* ctx, char pad, int count) {
    for (int i = 0; i < count; i++) {
        emit(pad, ctx);
    }
}

int string_length(const char* s) {
    int n = 0;
    while (s[n]) {
        n++;
    }
    return n;
}

// Виводить беззнакове число в заданій системі числення з урахуванням ширини та прапорців.
void emit_number(EmitFn emit, void* ctx, uint64_t value, unsigned base, bool upper, bool negative,
                 int width, bool zero, bool left) {
    char digits[64];
    int count = 0;
    const char* alphabet = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    do {
        digits[count++] = alphabet[value % base];
        value /= base;
    } while (value != 0);

    int total = count + (negative ? 1 : 0);
    int pad = width > total ? width - total : 0;

    if (!left && !zero) {
        emit_padding(emit, ctx, ' ', pad);
    }
    if (negative) {
        emit('-', ctx);
    }
    if (!left && zero) {
        emit_padding(emit, ctx, '0', pad);
    }
    while (count > 0) {
        emit(digits[--count], ctx);
    }
    if (left) {
        emit_padding(emit, ctx, ' ', pad);
    }
}

}  // namespace

bool log_add_sink(TextSink* sink) {
    if (sink == nullptr || g_sink_count >= kMaxSinks) {
        return false;
    }
    g_sinks[g_sink_count++] = sink;
    return true;
}

void vformat(EmitFn emit, void* ctx, const char* fmt, va_list ap) {
    for (; *fmt; ++fmt) {
        if (*fmt != '%') {
            emit(*fmt, ctx);
            continue;
        }
        ++fmt;

        bool zero = false;
        bool left = false;
        int width = 0;
        int long_count = 0;

        while (*fmt == '0' || *fmt == '-') {
            if (*fmt == '0') {
                zero = true;
            } else {
                left = true;
            }
            ++fmt;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            ++fmt;
        }
        while (*fmt == 'l' || *fmt == 'z') {
            long_count++;
            ++fmt;
        }

        switch (*fmt) {
            case 'd':
            case 'i': {
                int64_t v = long_count ? va_arg(ap, int64_t) : va_arg(ap, int);
                bool neg = v < 0;
                uint64_t mag = neg ? static_cast<uint64_t>(0) - static_cast<uint64_t>(v)
                                   : static_cast<uint64_t>(v);
                emit_number(emit, ctx, mag, 10, false, neg, width, zero, left);
                break;
            }
            case 'u':
            case 'x':
            case 'X': {
                uint64_t v = long_count ? va_arg(ap, uint64_t) : va_arg(ap, unsigned int);
                unsigned base = (*fmt == 'u') ? 10 : 16;
                emit_number(emit, ctx, v, base, *fmt == 'X', false, width, zero, left);
                break;
            }
            case 'p': {
                uint64_t v = reinterpret_cast<uintptr_t>(va_arg(ap, void*));
                emit('0', ctx);
                emit('x', ctx);
                emit_number(emit, ctx, v, 16, false, false, 16, true, false);
                break;
            }
            case 'c': {
                emit(static_cast<char>(va_arg(ap, int)), ctx);
                break;
            }
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (s == nullptr) {
                    s = "(null)";
                }
                int len = string_length(s);
                int pad = width > len ? width - len : 0;
                if (!left) {
                    emit_padding(emit, ctx, ' ', pad);
                }
                for (int i = 0; i < len; i++) {
                    emit(s[i], ctx);
                }
                if (left) {
                    emit_padding(emit, ctx, ' ', pad);
                }
                break;
            }
            case '%':
                emit('%', ctx);
                break;
            case '\0':
                return;
            default:
                emit('%', ctx);
                emit(*fmt, ctx);
                break;
        }
    }
}

void kvprintf(const char* fmt, va_list ap) {
    vformat(emit_to_sinks, nullptr, fmt, ap);
}

void kprintf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

}  // namespace gamma
