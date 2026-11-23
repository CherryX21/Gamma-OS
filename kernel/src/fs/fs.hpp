#pragma once

#include <stddef.h>
#include <stdint.h>

namespace gamma {

constexpr size_t kBlockSize = 512;
constexpr size_t kMaxNameLen = 29;
constexpr size_t kMaxPathLen = 128;

// Абстрактний інтерфейс блочного пристрою (ФВ-5).
class BlockDevice {
public:
    virtual ~BlockDevice() = default;
    virtual size_t block_size() const = 0;
    virtual size_t block_count() const = 0;
    virtual bool read_block(size_t lba, void* buf) = 0;
    virtual bool write_block(size_t lba, const void* buf) = 0;
};

// Реалізація блочного пристрою в оперативній пам'яті (RAM-диск).
class RamDisk final : public BlockDevice {
public:
    RamDisk(uint8_t* storage, size_t blocks);
    size_t block_size() const override { return kBlockSize; }
    size_t block_count() const override { return blocks_; }
    bool read_block(size_t lba, void* buf) override;
    bool write_block(size_t lba, const void* buf) override;

private:
    uint8_t* storage_;
    size_t blocks_;
};

enum class FsStatus : int {
    Ok = 0,
    NotFound,
    AlreadyExists,
    NotADirectory,
    IsADirectory,
    NotEmpty,
    NoSpace,
    InvalidPath,
    IoError,
};

const char* fs_status_str(FsStatus st);

struct DirEntryInfo {
    char name[kMaxNameLen + 1];
    bool is_dir;
    uint32_t size;
};

// Абстрактний інтерфейс файлової системи для шару VFS.
class FileSystem {
public:
    virtual ~FileSystem() = default;
    virtual FsStatus list_dir(const char* path, DirEntryInfo* out, size_t max_entries, size_t* out_count) = 0;
    virtual FsStatus mkdir(const char* path) = 0;
    virtual FsStatus touch(const char* path) = 0;
    virtual FsStatus write_file(const char* path, const void* data, size_t len) = 0;
    virtual FsStatus read_file(const char* path, void* buf, size_t max_len, size_t* out_len) = 0;
    virtual FsStatus remove(const char* path) = 0;
    virtual FsStatus stat(const char* path, bool* is_dir, uint32_t* size) = 0;
};

// Власна індексна файлова система GammaFS поверх блочного пристрою.
class GammaFS final : public FileSystem {
public:
    static constexpr uint64_t kMagic = 0x47414D4D41465331ULL;  // "GAMMAFS1"
    static constexpr uint16_t kInodeFree = 0;
    static constexpr uint16_t kInodeFile = 1;
    static constexpr uint16_t kInodeDir  = 2;
    static constexpr size_t kMaxInodes = 32;
    static constexpr size_t kDirectBlocks = 8;

    struct [[gnu::packed]] Superblock {
        uint64_t magic;
        uint32_t block_size;
        uint32_t total_blocks;
        uint32_t inode_count;
        uint32_t free_blocks;
        uint32_t free_inodes;
        uint32_t first_data_block;
        uint8_t reserved[480];
    };

    struct [[gnu::packed]] Inode {
        uint16_t type;
        uint16_t links;
        uint32_t size;
        uint32_t direct[kDirectBlocks];
        uint8_t reserved[24];
    };

    struct [[gnu::packed]] DirEntry {
        uint16_t inode;
        char name[kMaxNameLen + 1];
    };

    static_assert(sizeof(Superblock) == kBlockSize);
    static_assert(sizeof(Inode) == 64);
    static_assert(sizeof(DirEntry) == 32);

    explicit GammaFS(BlockDevice* dev) : dev_(dev) {}

    bool format();
    bool mount();

    FsStatus list_dir(const char* path, DirEntryInfo* out, size_t max_entries, size_t* out_count) override;
    FsStatus mkdir(const char* path) override;
    FsStatus touch(const char* path) override;
    FsStatus write_file(const char* path, const void* data, size_t len) override;
    FsStatus read_file(const char* path, void* buf, size_t max_len, size_t* out_len) override;
    FsStatus remove(const char* path) override;
    FsStatus stat(const char* path, bool* is_dir, uint32_t* size) override;

private:
    bool sync_superblock();
    bool read_inode(uint16_t idx, Inode* out);
    bool write_inode(uint16_t idx, const Inode& in);
    int alloc_inode();
    void free_inode(uint16_t idx);
    int alloc_block();
    void free_block(uint32_t lba);

    bool dir_lookup(const Inode& dir, const char* name, uint16_t* out_inode);
    FsStatus dir_add_entry(uint16_t dir_idx, Inode& dir, const char* name, uint16_t target_idx);
    bool dir_remove_entry(uint16_t dir_idx, Inode& dir, const char* name);
    bool dir_is_empty(const Inode& dir);

    FsStatus resolve_path(const char* path, uint16_t* out_inode);
    FsStatus resolve_parent(const char* path, uint16_t* out_parent, char* out_name);

    BlockDevice* dev_ = nullptr;
    Superblock sb_{};
    bool mounted_ = false;
};

// Глобальний інтерфейс VFS ядра.
void vfs_init();
FileSystem* vfs_root();
void vfs_self_test();

}  // namespace gamma
