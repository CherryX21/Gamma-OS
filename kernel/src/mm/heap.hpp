#pragma once

#include <stddef.h>
#include <stdint.h>

namespace gamma
{

    struct HeapStats
    {
        size_t total_bytes;
        size_t used_bytes;
        size_t free_bytes;
        size_t block_count;
    };

    // Абстрактний аллокатор кучи (first-fit з об'єднанням суміжних блоків).
    // Не залежить від заліза та тестується як у ядрі, так і на хост-системі.
    class HeapAllocator
    {
    public:
        using PageProvider = void *(*)(size_t pages);

        void init(PageProvider provider, size_t page_size = 4096);
        void *alloc(size_t size);
        void free(void *ptr);
        HeapStats stats() const;

    private:
        struct BlockHeader
        {
            size_t size; // корисний розмір блоку в байтах (без заголовка)
            bool free;
            BlockHeader *prev;
            BlockHeader *next;
        };

        bool expand(size_t min_bytes);
        void split_block(BlockHeader *block, size_t size);
        void coalesce(BlockHeader *block);

        PageProvider provider_ = nullptr;
        size_t page_size_ = 4096;
        BlockHeader *head_ = nullptr;
    };

    // Глобальні функції динамічної пам'яті ядра (ФВ-2).
    void heap_init();
    void *kmalloc(size_t size);
    void *kzalloc(size_t size);
    void kfree(void *ptr);
    HeapStats heap_stats();
    void heap_self_test();

} // namespace gamma
