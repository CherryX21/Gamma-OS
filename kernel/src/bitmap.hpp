#pragma once

#include <stddef.h>
#include <stdint.h>

namespace gamma {

// Бітова карта з пошуком вільних бітів і неперервних серій. Не залежить від ядра,
// тому перевіряється юніт-тестами на хост-системі (tests/test_bitmap.cpp).
// Значення біта: 1 — зайнято, 0 — вільно.
class Bitmap {
public:
    static constexpr size_t npos = ~static_cast<size_t>(0);

    constexpr Bitmap() = default;

    // storage має вміщувати words_for(bit_count) слів. Усі біти встановлюються в 1.
    void init(uint64_t* storage, size_t bit_count);

    static constexpr size_t words_for(size_t bits) { return (bits + 63) / 64; }

    size_t size() const { return bit_count_; }

    // Індекс поза межами вважається зайнятим.
    bool test(size_t index) const;
    void set(size_t index);
    void clear(size_t index);

    // Діапазони обмежуються розміром карти.
    void set_range(size_t first, size_t count);
    void clear_range(size_t first, size_t count);

    // Повертає найменший вільний індекс або npos.
    size_t find_first_clear() const;

    // Повертає початок першої серії з count вільних бітів, вирівняної на alignment, або npos.
    size_t find_clear_run(size_t count, size_t alignment = 1) const;

    size_t count_clear() const;

private:
    uint64_t* words_ = nullptr;
    size_t bit_count_ = 0;
};

}  // namespace gamma
