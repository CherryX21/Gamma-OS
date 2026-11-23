#include "fs.hpp"

#ifndef GAMMA_HOST_TEST
#include "heap.hpp"
#include "interrupts.hpp"
#include "kstring.hpp"
#include "log.hpp"
#else
#include <cstring>
#endif

inline void* operator new(size_t, void* ptr) noexcept { return ptr; }

namespace gamma {

namespace {

constexpr size_t kSuperblockLba = 0;
constexpr size_t kBlockBitmapLba = 1;
constexpr size_t kInodeBitmapLba = 2;
constexpr size_t kInodeTableStartLba = 3;
constexpr size_t kInodeTableBlocks = (GammaFS::kMaxInodes * sizeof(GammaFS::Inode)) / kBlockSize;
constexpr size_t kFirstDataLba = kInodeTableStartLba + kInodeTableBlocks;  // 7
constexpr uint16_t kRootInode = 1;

size_t str_len(const char* s) {
    size_t n = 0;
    while (s[n] != '\0') n++;
    return n;
}

bool str_eq(const char* a, const char* b) {
    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

void str_copy(char* dst, const char* src, size_t max_len) {
    size_t i = 0;
    while (i + 1 < max_len && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

bool bitmap_get(const uint8_t* map, size_t bit) {
    return (map[bit / 8] & (1u << (bit % 8))) != 0;
}

void bitmap_set(uint8_t* map, size_t bit) {
    map[bit / 8] |= static_cast<uint8_t>(1u << (bit % 8));
}

void bitmap_clear(uint8_t* map, size_t bit) {
    map[bit / 8] &= static_cast<uint8_t>(~(1u << (bit % 8)));
}

}  // namespace

const char* fs_status_str(FsStatus st) {
    switch (st) {
        case FsStatus::Ok: return "ok";
        case FsStatus::NotFound: return "not found";
        case FsStatus::AlreadyExists: return "already exists";
        case FsStatus::NotADirectory: return "not a directory";
        case FsStatus::IsADirectory: return "is a directory";
        case FsStatus::NotEmpty: return "directory not empty";
        case FsStatus::NoSpace: return "no space left on device";
        case FsStatus::InvalidPath: return "invalid path";
        case FsStatus::IoError: return "I/O error";
    }
    return "unknown error";
}

RamDisk::RamDisk(uint8_t* storage, size_t blocks) : storage_(storage), blocks_(blocks) {
    memset(storage_, 0, blocks_ * kBlockSize);
}

bool RamDisk::read_block(size_t lba, void* buf) {
    if (storage_ == nullptr || buf == nullptr || lba >= blocks_) {
        return false;
    }
    memcpy(buf, storage_ + lba * kBlockSize, kBlockSize);
    return true;
}

bool RamDisk::write_block(size_t lba, const void* buf) {
    if (storage_ == nullptr || buf == nullptr || lba >= blocks_) {
        return false;
    }
    memcpy(storage_ + lba * kBlockSize, buf, kBlockSize);
    return true;
}

bool GammaFS::sync_superblock() {
    return dev_->write_block(kSuperblockLba, &sb_);
}

bool GammaFS::read_inode(uint16_t idx, Inode* out) {
    if (idx == 0 || idx >= kMaxInodes) return false;
    const size_t per_block = kBlockSize / sizeof(Inode);
    const size_t lba = kInodeTableStartLba + (idx / per_block);
    const size_t offset = (idx % per_block) * sizeof(Inode);
    uint8_t sec[kBlockSize];
    if (!dev_->read_block(lba, sec)) return false;
    memcpy(out, sec + offset, sizeof(Inode));
    return true;
}

bool GammaFS::write_inode(uint16_t idx, const Inode& in) {
    if (idx == 0 || idx >= kMaxInodes) return false;
    const size_t per_block = kBlockSize / sizeof(Inode);
    const size_t lba = kInodeTableStartLba + (idx / per_block);
    const size_t offset = (idx % per_block) * sizeof(Inode);
    uint8_t sec[kBlockSize];
    if (!dev_->read_block(lba, sec)) return false;
    memcpy(sec + offset, &in, sizeof(Inode));
    return dev_->write_block(lba, sec);
}

int GammaFS::alloc_inode() {
    uint8_t map[kBlockSize];
    if (!dev_->read_block(kInodeBitmapLba, map)) return -1;
    for (size_t i = kRootInode; i < kMaxInodes; i++) {
        if (!bitmap_get(map, i)) {
            bitmap_set(map, i);
            if (!dev_->write_block(kInodeBitmapLba, map)) return -1;
            sb_.free_inodes--;
            sync_superblock();
            return static_cast<int>(i);
        }
    }
    return -1;
}

void GammaFS::free_inode(uint16_t idx) {
    if (idx <= kRootInode || idx >= kMaxInodes) return;
    Inode empty{};
    write_inode(idx, empty);
    uint8_t map[kBlockSize];
    if (dev_->read_block(kInodeBitmapLba, map)) {
        bitmap_clear(map, idx);
        dev_->write_block(kInodeBitmapLba, map);
        sb_.free_inodes++;
        sync_superblock();
    }
}

int GammaFS::alloc_block() {
    uint8_t map[kBlockSize];
    if (!dev_->read_block(kBlockBitmapLba, map)) return -1;
    const size_t max_blocks = sb_.total_blocks < kBlockSize * 8 ? sb_.total_blocks : kBlockSize * 8;
    for (size_t b = kFirstDataLba; b < max_blocks; b++) {
        if (!bitmap_get(map, b)) {
            bitmap_set(map, b);
            if (!dev_->write_block(kBlockBitmapLba, map)) return -1;
            uint8_t zero[kBlockSize] = {};
            dev_->write_block(b, zero);
            sb_.free_blocks--;
            sync_superblock();
            return static_cast<int>(b);
        }
    }
    return -1;
}

void GammaFS::free_block(uint32_t lba) {
    if (lba < kFirstDataLba || lba >= sb_.total_blocks) return;
    uint8_t map[kBlockSize];
    if (dev_->read_block(kBlockBitmapLba, map)) {
        bitmap_clear(map, lba);
        dev_->write_block(kBlockBitmapLba, map);
        sb_.free_blocks++;
        sync_superblock();
    }
}

bool GammaFS::format() {
    if (dev_ == nullptr || dev_->block_count() <= kFirstDataLba + 4) {
        return false;
    }
    uint8_t zero[kBlockSize] = {};
    for (size_t i = 0; i <= kFirstDataLba; i++) {
        if (!dev_->write_block(i, zero)) return false;
    }

    memset(&sb_, 0, sizeof(sb_));
    sb_.magic = kMagic;
    sb_.block_size = kBlockSize;
    sb_.total_blocks = static_cast<uint32_t>(dev_->block_count());
    sb_.inode_count = kMaxInodes;
    sb_.first_data_block = kFirstDataLba;
    sb_.free_blocks = sb_.total_blocks - kFirstDataLba;
    sb_.free_inodes = kMaxInodes - 1;  // inode 0 зарезервовано

    uint8_t bmap[kBlockSize] = {};
    for (size_t b = 0; b < kFirstDataLba; b++) {
        bitmap_set(bmap, b);
    }
    if (!dev_->write_block(kBlockBitmapLba, bmap)) return false;

    uint8_t imap[kBlockSize] = {};
    bitmap_set(imap, 0);
    if (!dev_->write_block(kInodeBitmapLba, imap)) return false;
    if (!sync_superblock()) return false;

    const int root_idx = alloc_inode();
    if (root_idx != kRootInode) return false;

    Inode root{};
    root.type = kInodeDir;
    root.links = 1;
    root.size = 0;
    if (!write_inode(kRootInode, root)) return false;

    mounted_ = true;
    return true;
}

bool GammaFS::mount() {
    if (dev_ == nullptr) return false;
    if (!dev_->read_block(kSuperblockLba, &sb_)) return false;
    if (sb_.magic != kMagic || sb_.block_size != kBlockSize) {
        return false;
    }
    mounted_ = true;
    return true;
}

bool GammaFS::dir_lookup(const Inode& dir, const char* name, uint16_t* out_inode) {
    if (dir.type != kInodeDir) return false;
    const size_t entries_per_block = kBlockSize / sizeof(DirEntry);
    DirEntry entries[entries_per_block];

    for (size_t i = 0; i < kDirectBlocks; i++) {
        if (dir.direct[i] == 0) continue;
        if (!dev_->read_block(dir.direct[i], entries)) return false;
        for (size_t j = 0; j < entries_per_block; j++) {
            if (entries[j].inode != 0 && str_eq(entries[j].name, name)) {
                if (out_inode != nullptr) *out_inode = entries[j].inode;
                return true;
            }
        }
    }
    return false;
}

FsStatus GammaFS::dir_add_entry(uint16_t dir_idx, Inode& dir, const char* name, uint16_t target_idx) {
    const size_t entries_per_block = kBlockSize / sizeof(DirEntry);
    DirEntry entries[entries_per_block];

    for (size_t i = 0; i < kDirectBlocks; i++) {
        if (dir.direct[i] == 0) {
            const int new_blk = alloc_block();
            if (new_blk < 0) return FsStatus::NoSpace;
            dir.direct[i] = static_cast<uint32_t>(new_blk);
            dir.size += kBlockSize;
            write_inode(dir_idx, dir);
        }
        if (!dev_->read_block(dir.direct[i], entries)) return FsStatus::IoError;
        for (size_t j = 0; j < entries_per_block; j++) {
            if (entries[j].inode == 0) {
                entries[j].inode = target_idx;
                str_copy(entries[j].name, name, sizeof(entries[j].name));
                if (!dev_->write_block(dir.direct[i], entries)) return FsStatus::IoError;
                return FsStatus::Ok;
            }
        }
    }
    return FsStatus::NoSpace;
}

bool GammaFS::dir_remove_entry(uint16_t dir_idx, Inode& dir, const char* name) {
    (void)dir_idx;
    const size_t entries_per_block = kBlockSize / sizeof(DirEntry);
    DirEntry entries[entries_per_block];

    for (size_t i = 0; i < kDirectBlocks; i++) {
        if (dir.direct[i] == 0) continue;
        if (!dev_->read_block(dir.direct[i], entries)) return false;
        for (size_t j = 0; j < entries_per_block; j++) {
            if (entries[j].inode != 0 && str_eq(entries[j].name, name)) {
                memset(&entries[j], 0, sizeof(DirEntry));
                return dev_->write_block(dir.direct[i], entries);
            }
        }
    }
    return false;
}

bool GammaFS::dir_is_empty(const Inode& dir) {
    const size_t entries_per_block = kBlockSize / sizeof(DirEntry);
    DirEntry entries[entries_per_block];

    for (size_t i = 0; i < kDirectBlocks; i++) {
        if (dir.direct[i] == 0) continue;
        if (!dev_->read_block(dir.direct[i], entries)) return false;
        for (size_t j = 0; j < entries_per_block; j++) {
            if (entries[j].inode != 0) return false;
        }
    }
    return true;
}

FsStatus GammaFS::resolve_path(const char* path, uint16_t* out_inode) {
    if (!mounted_ || path == nullptr || path[0] == '\0') {
        return FsStatus::InvalidPath;
    }
    uint16_t curr = kRootInode;
    size_t pos = 0;
    while (path[pos] == '/') pos++;
    if (path[pos] == '\0') {
        *out_inode = curr;
        return FsStatus::Ok;
    }

    char token[kMaxNameLen + 1];
    while (path[pos] != '\0') {
        size_t len = 0;
        while (path[pos] != '\0' && path[pos] != '/') {
            if (len >= kMaxNameLen) return FsStatus::InvalidPath;
            token[len++] = path[pos++];
        }
        token[len] = '\0';
        while (path[pos] == '/') pos++;

        Inode in{};
        if (!read_inode(curr, &in)) return FsStatus::IoError;
        if (in.type != kInodeDir) return FsStatus::NotADirectory;

        if (str_eq(token, ".")) {
            continue;
        }
        uint16_t next = 0;
        if (!dir_lookup(in, token, &next)) {
            return FsStatus::NotFound;
        }
        curr = next;
    }
    *out_inode = curr;
    return FsStatus::Ok;
}

FsStatus GammaFS::resolve_parent(const char* path, uint16_t* out_parent, char* out_name) {
    if (!mounted_ || path == nullptr || path[0] == '\0') {
        return FsStatus::InvalidPath;
    }
    char clean[kMaxPathLen];
    str_copy(clean, path, sizeof(clean));
    size_t len = str_len(clean);
    while (len > 1 && clean[len - 1] == '/') {
        clean[--len] = '\0';
    }
    if (str_eq(clean, "/")) {
        return FsStatus::InvalidPath;
    }

    int last_slash = -1;
    for (size_t i = 0; i < len; i++) {
        if (clean[i] == '/') last_slash = static_cast<int>(i);
    }

    const char* name_part = nullptr;
    if (last_slash < 0) {
        *out_parent = kRootInode;
        name_part = clean;
    } else if (last_slash == 0) {
        *out_parent = kRootInode;
        name_part = clean + 1;
    } else {
        clean[last_slash] = '\0';
        name_part = clean + last_slash + 1;
        const FsStatus st = resolve_path(clean, out_parent);
        if (st != FsStatus::Ok) return st;
    }

    if (name_part[0] == '\0' || str_len(name_part) > kMaxNameLen ||
        str_eq(name_part, ".") || str_eq(name_part, "..")) {
        return FsStatus::InvalidPath;
    }
    str_copy(out_name, name_part, kMaxNameLen + 1);
    return FsStatus::Ok;
}

FsStatus GammaFS::list_dir(const char* path, DirEntryInfo* out, size_t max_entries, size_t* out_count) {
    uint16_t idx = 0;
    const FsStatus st = resolve_path(path, &idx);
    if (st != FsStatus::Ok) return st;

    Inode dir{};
    if (!read_inode(idx, &dir)) return FsStatus::IoError;
    if (dir.type != kInodeDir) return FsStatus::NotADirectory;

    const size_t entries_per_block = kBlockSize / sizeof(DirEntry);
    DirEntry entries[entries_per_block];
    size_t count = 0;

    for (size_t i = 0; i < kDirectBlocks; i++) {
        if (dir.direct[i] == 0) continue;
        if (!dev_->read_block(dir.direct[i], entries)) return FsStatus::IoError;
        for (size_t j = 0; j < entries_per_block; j++) {
            if (entries[j].inode != 0 && count < max_entries) {
                Inode child{};
                if (read_inode(entries[j].inode, &child)) {
                    str_copy(out[count].name, entries[j].name, sizeof(out[count].name));
                    out[count].is_dir = (child.type == kInodeDir);
                    out[count].size = child.size;
                    count++;
                }
            }
        }
    }
    if (out_count != nullptr) *out_count = count;
    return FsStatus::Ok;
}

FsStatus GammaFS::mkdir(const char* path) {
    uint16_t parent_idx = 0;
    char name[kMaxNameLen + 1];
    const FsStatus st = resolve_parent(path, &parent_idx, name);
    if (st != FsStatus::Ok) return st;

    Inode parent{};
    if (!read_inode(parent_idx, &parent)) return FsStatus::IoError;
    if (parent.type != kInodeDir) return FsStatus::NotADirectory;
    if (dir_lookup(parent, name, nullptr)) return FsStatus::AlreadyExists;

    const int new_idx = alloc_inode();
    if (new_idx < 0) return FsStatus::NoSpace;

    Inode dir{};
    dir.type = kInodeDir;
    dir.links = 1;
    dir.size = 0;
    if (!write_inode(static_cast<uint16_t>(new_idx), dir)) {
        free_inode(static_cast<uint16_t>(new_idx));
        return FsStatus::IoError;
    }

    const FsStatus add_st = dir_add_entry(parent_idx, parent, name, static_cast<uint16_t>(new_idx));
    if (add_st != FsStatus::Ok) {
        free_inode(static_cast<uint16_t>(new_idx));
        return add_st;
    }
    return FsStatus::Ok;
}

FsStatus GammaFS::touch(const char* path) {
    uint16_t existing = 0;
    if (resolve_path(path, &existing) == FsStatus::Ok) {
        return FsStatus::Ok;
    }
    uint16_t parent_idx = 0;
    char name[kMaxNameLen + 1];
    const FsStatus st = resolve_parent(path, &parent_idx, name);
    if (st != FsStatus::Ok) return st;

    Inode parent{};
    if (!read_inode(parent_idx, &parent)) return FsStatus::IoError;
    if (parent.type != kInodeDir) return FsStatus::NotADirectory;

    const int new_idx = alloc_inode();
    if (new_idx < 0) return FsStatus::NoSpace;

    Inode file{};
    file.type = kInodeFile;
    file.links = 1;
    file.size = 0;
    if (!write_inode(static_cast<uint16_t>(new_idx), file)) {
        free_inode(static_cast<uint16_t>(new_idx));
        return FsStatus::IoError;
    }

    const FsStatus add_st = dir_add_entry(parent_idx, parent, name, static_cast<uint16_t>(new_idx));
    if (add_st != FsStatus::Ok) {
        free_inode(static_cast<uint16_t>(new_idx));
        return add_st;
    }
    return FsStatus::Ok;
}

FsStatus GammaFS::write_file(const char* path, const void* data, size_t len) {
    if (len > kDirectBlocks * kBlockSize) {
        return FsStatus::NoSpace;
    }
    const FsStatus touch_st = touch(path);
    if (touch_st != FsStatus::Ok) return touch_st;

    uint16_t idx = 0;
    const FsStatus st = resolve_path(path, &idx);
    if (st != FsStatus::Ok) return st;

    Inode file{};
    if (!read_inode(idx, &file)) return FsStatus::IoError;
    if (file.type != kInodeFile) return FsStatus::IsADirectory;

    for (size_t i = 0; i < kDirectBlocks; i++) {
        if (file.direct[i] != 0) {
            free_block(file.direct[i]);
            file.direct[i] = 0;
        }
    }
    file.size = 0;

    const auto* bytes = reinterpret_cast<const uint8_t*>(data);
    size_t written = 0;
    for (size_t b = 0; b < kDirectBlocks && written < len; b++) {
        const int blk = alloc_block();
        if (blk < 0) {
            write_inode(idx, file);
            return FsStatus::NoSpace;
        }
        file.direct[b] = static_cast<uint32_t>(blk);
        uint8_t sec[kBlockSize] = {};
        const size_t chunk = (len - written) > kBlockSize ? kBlockSize : (len - written);
        memcpy(sec, bytes + written, chunk);
        if (!dev_->write_block(static_cast<size_t>(blk), sec)) {
            write_inode(idx, file);
            return FsStatus::IoError;
        }
        written += chunk;
    }
    file.size = static_cast<uint32_t>(written);
    return write_inode(idx, file) ? FsStatus::Ok : FsStatus::IoError;
}

FsStatus GammaFS::read_file(const char* path, void* buf, size_t max_len, size_t* out_len) {
    uint16_t idx = 0;
    const FsStatus st = resolve_path(path, &idx);
    if (st != FsStatus::Ok) return st;

    Inode file{};
    if (!read_inode(idx, &file)) return FsStatus::IoError;
    if (file.type != kInodeFile) return FsStatus::IsADirectory;

    const size_t to_read = file.size < max_len ? file.size : max_len;
    auto* out = reinterpret_cast<uint8_t*>(buf);
    size_t read_total = 0;

    for (size_t b = 0; b < kDirectBlocks && read_total < to_read; b++) {
        if (file.direct[b] == 0) break;
        uint8_t sec[kBlockSize];
        if (!dev_->read_block(file.direct[b], sec)) return FsStatus::IoError;
        const size_t chunk = (to_read - read_total) > kBlockSize ? kBlockSize : (to_read - read_total);
        memcpy(out + read_total, sec, chunk);
        read_total += chunk;
    }
    if (out_len != nullptr) *out_len = read_total;
    return FsStatus::Ok;
}

FsStatus GammaFS::remove(const char* path) {
    uint16_t parent_idx = 0;
    char name[kMaxNameLen + 1];
    const FsStatus st = resolve_parent(path, &parent_idx, name);
    if (st != FsStatus::Ok) return st;

    Inode parent{};
    if (!read_inode(parent_idx, &parent)) return FsStatus::IoError;

    uint16_t target_idx = 0;
    if (!dir_lookup(parent, name, &target_idx)) return FsStatus::NotFound;

    Inode target{};
    if (!read_inode(target_idx, &target)) return FsStatus::IoError;

    if (target.type == kInodeDir && !dir_is_empty(target)) {
        return FsStatus::NotEmpty;
    }
    for (size_t i = 0; i < kDirectBlocks; i++) {
        if (target.direct[i] != 0) {
            free_block(target.direct[i]);
        }
    }
    dir_remove_entry(parent_idx, parent, name);
    free_inode(target_idx);
    return FsStatus::Ok;
}

FsStatus GammaFS::stat(const char* path, bool* is_dir, uint32_t* size) {
    uint16_t idx = 0;
    const FsStatus st = resolve_path(path, &idx);
    if (st != FsStatus::Ok) return st;

    Inode in{};
    if (!read_inode(idx, &in)) return FsStatus::IoError;
    if (is_dir != nullptr) *is_dir = (in.type == kInodeDir);
    if (size != nullptr) *size = in.size;
    return FsStatus::Ok;
}

#ifndef GAMMA_HOST_TEST
namespace {
constexpr size_t kRamDiskBlocks = 256;  // 128 KiB
RamDisk* g_ramdisk = nullptr;
GammaFS* g_rootfs = nullptr;
}  // namespace

void vfs_init() {
    auto* storage = reinterpret_cast<uint8_t*>(kmalloc(kRamDiskBlocks * kBlockSize));
    auto* rd_mem = kmalloc(sizeof(RamDisk));
    auto* fs_mem = kmalloc(sizeof(GammaFS));
    if (storage == nullptr || rd_mem == nullptr || fs_mem == nullptr) {
        panic("vfs: failed to allocate RAM disk");
    }
    g_ramdisk = new (rd_mem) RamDisk(storage, kRamDiskBlocks);
    g_rootfs = new (fs_mem) GammaFS(g_ramdisk);
    if (!g_rootfs->format()) {
        panic("vfs: failed to format GammaFS");
    }

    // Початкове наповнення кореневого каталогу згідно прототипу ТЗ (рис. 4.1).
    g_rootfs->mkdir("/bin");
    g_rootfs->mkdir("/docs");
    const char* welcome = "Welcome to Gamma OS!\n";
    g_rootfs->write_file("/readme.txt", welcome, str_len(welcome));
    kprintf("[vfs] GammaFS mounted on 128 KiB RAM disk\n");
}

FileSystem* vfs_root() {
    return g_rootfs;
}

void vfs_self_test() {
    FileSystem* fs = vfs_root();
    if (fs == nullptr || fs->mkdir("/tmp") != FsStatus::Ok) {
        panic("vfs self-test: mkdir failed");
    }
    const char* msg = "hello";
    if (fs->write_file("/tmp/test.txt", msg, 5) != FsStatus::Ok) {
        panic("vfs self-test: write failed");
    }
    char buf[16] = {};
    size_t n = 0;
    if (fs->read_file("/tmp/test.txt", buf, sizeof(buf), &n) != FsStatus::Ok || n != 5 || !str_eq(buf, "hello")) {
        panic("vfs self-test: read mismatch");
    }
    if (fs->remove("/tmp/test.txt") != FsStatus::Ok || fs->remove("/tmp") != FsStatus::Ok) {
        panic("vfs self-test: cleanup failed");
    }
    kprintf("[vfs] self-test passed\n");
}
#endif

}  // namespace gamma
