// Юніт-тести бітової карти. Збираються звичайним компілятором хоста: make test.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bitmap.hpp"

using gamma::Bitmap;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_checks++;                                                          \
        if (!(cond)) {                                                       \
            g_failures++;                                                    \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                    \
    } while (0)

static void test_init_all_used() {
    uint64_t storage[Bitmap::words_for(100)];
    Bitmap b;
    b.init(storage, 100);
    CHECK(b.size() == 100);
    CHECK(b.count_clear() == 0);
    CHECK(b.find_first_clear() == Bitmap::npos);
    CHECK(b.test(0) && b.test(99));
}

static void test_set_clear_test() {
    uint64_t storage[Bitmap::words_for(130)];
    Bitmap b;
    b.init(storage, 130);
    b.clear(0);
    b.clear(63);
    b.clear(64);
    b.clear(129);
    CHECK(!b.test(0) && !b.test(63) && !b.test(64) && !b.test(129));
    CHECK(b.test(1) && b.test(65));
    CHECK(b.count_clear() == 4);
    b.set(64);
    CHECK(b.test(64));
    CHECK(b.count_clear() == 3);
}

static void test_out_of_range() {
    uint64_t storage[Bitmap::words_for(70)];
    Bitmap b;
    b.init(storage, 70);
    CHECK(b.test(70));
    CHECK(b.test(1000));
    b.clear(70);
    b.clear(1000);
    CHECK(b.count_clear() == 0);
    b.clear_range(60, 100);
    CHECK(b.count_clear() == 10);
    CHECK(b.find_first_clear() == 60);
    b.clear_range(500, 5);
    CHECK(b.count_clear() == 10);
}

static void test_padding_bits_never_free() {
    uint64_t storage[Bitmap::words_for(65)];
    Bitmap b;
    b.init(storage, 65);
    b.clear_range(0, 65);
    CHECK(b.count_clear() == 65);
    for (size_t i = 0; i < 65; i++) {
        b.set(i);
    }
    CHECK(b.find_first_clear() == Bitmap::npos);
}

static void test_find_first_clear_order() {
    uint64_t storage[Bitmap::words_for(256)];
    Bitmap b;
    b.init(storage, 256);
    b.clear_range(0, 256);
    b.set_range(0, 70);
    CHECK(b.find_first_clear() == 70);
    b.clear(5);
    CHECK(b.find_first_clear() == 5);
}

static void test_ranges_across_words() {
    uint64_t storage[Bitmap::words_for(200)];
    Bitmap b;
    b.init(storage, 200);
    b.clear_range(60, 80);
    CHECK(b.count_clear() == 80);
    CHECK(b.test(59) && !b.test(60) && !b.test(139) && b.test(140));
    b.set_range(100, 10);
    CHECK(b.count_clear() == 70);
}

static void test_run_basic() {
    uint64_t storage[Bitmap::words_for(128)];
    Bitmap b;
    b.init(storage, 128);
    b.clear_range(10, 20);
    CHECK(b.find_clear_run(20) == 10);
    CHECK(b.find_clear_run(21) == Bitmap::npos);
    CHECK(b.find_clear_run(1) == 10);
    CHECK(b.find_clear_run(0) == Bitmap::npos);
}

static void test_run_skips_fragments() {
    uint64_t storage[Bitmap::words_for(128)];
    Bitmap b;
    b.init(storage, 128);
    b.clear_range(0, 3);
    b.clear_range(10, 4);
    b.clear_range(50, 8);
    CHECK(b.find_clear_run(4) == 10);
    CHECK(b.find_clear_run(5) == 50);
    CHECK(b.find_clear_run(8) == 50);
    CHECK(b.find_clear_run(9) == Bitmap::npos);
}

static void test_run_alignment() {
    uint64_t storage[Bitmap::words_for(128)];
    Bitmap b;
    b.init(storage, 128);
    b.clear_range(3, 40);
    CHECK(b.find_clear_run(16, 16) == 16);
    CHECK(b.find_clear_run(8, 8) == 8);
    CHECK(b.find_clear_run(30, 16) == Bitmap::npos);
    CHECK(b.find_clear_run(4, 0) == 3);
}

static void test_run_at_end() {
    uint64_t storage[Bitmap::words_for(100)];
    Bitmap b;
    b.init(storage, 100);
    b.clear_range(90, 10);
    CHECK(b.find_clear_run(10) == 90);
    CHECK(b.find_clear_run(11) == Bitmap::npos);
    CHECK(b.find_clear_run(100) == Bitmap::npos);
}

static void test_alloc_free_cycle() {
    uint64_t storage[Bitmap::words_for(64)];
    Bitmap b;
    b.init(storage, 64);
    b.clear_range(0, 64);
    for (int round = 0; round < 3; round++) {
        for (size_t i = 0; i < 64; i++) {
            size_t idx = b.find_first_clear();
            CHECK(idx == i);
            b.set(idx);
        }
        CHECK(b.find_first_clear() == Bitmap::npos);
        b.clear_range(0, 64);
        CHECK(b.count_clear() == 64);
    }
}

int main() {
    test_init_all_used();
    test_set_clear_test();
    test_out_of_range();
    test_padding_bits_never_free();
    test_find_first_clear_order();
    test_ranges_across_words();
    test_run_basic();
    test_run_skips_fragments();
    test_run_alignment();
    test_run_at_end();
    test_alloc_free_cycle();
    printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
