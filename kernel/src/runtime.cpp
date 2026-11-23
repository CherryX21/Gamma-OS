// Мінімальна підтримка виконання для freestanding-ядра: функції, які компілятор
// може викликати неявно, та заглушка для чисто віртуальних викликів.
#include <stddef.h>
#include <stdint.h>

extern "C" {

void* memcpy(void* dst, const void* src, size_t n) {
    void* ret = dst;
    asm volatile("rep movsb" : "+D"(dst), "+S"(src), "+c"(n) : : "memory");
    return ret;
}

void* memmove(void* dst, const void* src, size_t n) {
    uint8_t* d = static_cast<uint8_t*>(dst);
    const uint8_t* s = static_cast<const uint8_t*>(src);
    if (d == s || n == 0) {
        return dst;
    }
    if (d < s || d >= s + n) {
        return memcpy(dst, src, n);
    }
    // Області перекриваються і dst > src: копіюємо з кінця.
    while (n--) {
        d[n] = s[n];
    }
    return dst;
}

void* memset(void* dst, int value, size_t n) {
    void* ret = dst;
    asm volatile("rep stosb" : "+D"(dst), "+c"(n) : "a"(value) : "memory");
    return ret;
}

int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* x = static_cast<const uint8_t*>(a);
    const uint8_t* y = static_cast<const uint8_t*>(b);
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]) {
            return x[i] < y[i] ? -1 : 1;
        }
    }
    return 0;
}

void __cxa_pure_virtual() {
    for (;;) {
        asm volatile("cli; hlt");
    }
}

}  // extern "C"

void operator delete(void*) noexcept {}
void operator delete(void*, size_t) noexcept {}
void operator delete[](void*) noexcept {}
void operator delete[](void*, size_t) noexcept {}
