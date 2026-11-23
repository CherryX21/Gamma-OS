#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "heap.hpp"

namespace
{

    int g_checks = 0;

    void check(bool cond, const char *expr, int line)
    {
        g_checks++;
        if (!cond)
        {
            std::fprintf(stderr, "FAIL at line %d: %s\n", line, expr);
            std::exit(1);
        }
    }

#define CHECK(expr) check((expr), #expr, __LINE__)

    // Імітуємо суцільний пул пам'яті хоста для перевірки злиття блоків.
    alignas(4096) uint8_t g_backing_pool[256 * 1024];
    size_t g_pool_offset = 0;

    void *host_page_provider(size_t pages)
    {
        const size_t bytes = pages * 4096;
        if (g_pool_offset + bytes > sizeof(g_backing_pool))
        {
            return nullptr;
        }
        void *ptr = g_backing_pool + g_pool_offset;
        g_pool_offset += bytes;
        return ptr;
    }

} // namespace

int main()
{
    gamma::HeapAllocator heap;
    heap.init(host_page_provider, 4096);

    void *a = heap.alloc(32);
    void *b = heap.alloc(64);
    void *c = heap.alloc(128);
    CHECK(a != nullptr && b != nullptr && c != nullptr);
    CHECK(reinterpret_cast<uintptr_t>(a) % 16 == 0);
    CHECK(reinterpret_cast<uintptr_t>(b) % 16 == 0);
    CHECK(reinterpret_cast<uintptr_t>(c) % 16 == 0);

    std::memset(a, 0x11, 32);
    std::memset(b, 0x22, 64);
    std::memset(c, 0x33, 128);

    // Звільняємо середній і лівий блоки — вони мають злитися в один великий блок.
    heap.free(b);
    heap.free(a);
    void *merged = heap.alloc(96);
    CHECK(merged == a);

    heap.free(merged);
    heap.free(c);

    // Перевірка великого виділення (розширення купи на декілька сторінок).
    void *big = heap.alloc(32 * 1024);
    CHECK(big != nullptr);
    std::memset(big, 0x5A, 32 * 1024);
    heap.free(big);

    const gamma::HeapStats st = heap.stats();
    CHECK(st.used_bytes == 0);
    CHECK(st.free_bytes == st.total_bytes);

    std::printf("heap unit tests: %d checks, 0 failures\n", g_checks);
    return 0;
}
