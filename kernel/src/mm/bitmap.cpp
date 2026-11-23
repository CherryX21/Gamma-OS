#include "bitmap.hpp"

namespace gamma {

void Bitmap::init(uint64_t* storage, size_t bit_count) {
    words_ = storage;
    bit_count_ = bit_count;
    const size_t words = words_for(bit_count);
    for (size_t i = 0; i < words; i++) {
        words_[i] = ~static_cast<uint64_t>(0);
    }
}

bool Bitmap::test(size_t index) const {
    if (index >= bit_count_) {
        return true;
    }
    return (words_[index / 64] >> (index % 64)) & 1;
}

void Bitmap::set(size_t index) {
    if (index < bit_count_) {
        words_[index / 64] |= static_cast<uint64_t>(1) << (index % 64);
    }
}

void Bitmap::clear(size_t index) {
    if (index < bit_count_) {
        words_[index / 64] &= ~(static_cast<uint64_t>(1) << (index % 64));
    }
}

void Bitmap::set_range(size_t first, size_t count) {
    if (first >= bit_count_) {
        return;
    }
    const size_t end = (count > bit_count_ - first) ? bit_count_ : first + count;
    for (size_t i = first; i < end; i++) {
        set(i);
    }
}

void Bitmap::clear_range(size_t first, size_t count) {
    if (first >= bit_count_) {
        return;
    }
    const size_t end = (count > bit_count_ - first) ? bit_count_ : first + count;
    for (size_t i = first; i < end; i++) {
        clear(i);
    }
}

size_t Bitmap::find_first_clear() const {
    const size_t words = words_for(bit_count_);
    for (size_t w = 0; w < words; w++) {
        if (words_[w] != ~static_cast<uint64_t>(0)) {
            const size_t index = w * 64 + static_cast<size_t>(__builtin_ctzll(~words_[w]));
            return index < bit_count_ ? index : npos;
        }
    }
    return npos;
}

size_t Bitmap::find_clear_run(size_t count, size_t alignment) const {
    if (count == 0 || count > bit_count_) {
        return npos;
    }
    if (alignment == 0) {
        alignment = 1;
    }
    size_t i = 0;
    while (i + count <= bit_count_) {
        i = (i + alignment - 1) / alignment * alignment;
        if (i + count > bit_count_) {
            break;
        }
        size_t run = 0;
        while (run < count && !test(i + run)) {
            run++;
        }
        if (run == count) {
            return i;
        }
        i += run + 1;
    }
    return npos;
}

size_t Bitmap::count_clear() const {
    size_t clear_bits = 0;
    const size_t words = words_for(bit_count_);
    for (size_t w = 0; w < words; w++) {
        clear_bits += static_cast<size_t>(__builtin_popcountll(~words_[w]));
    }
    return clear_bits;
}

}  // namespace gamma
