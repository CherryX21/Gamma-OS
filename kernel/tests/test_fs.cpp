#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "fs.hpp"

namespace {

int g_checks = 0;

void check(bool cond, const char* expr, int line) {
    g_checks++;
    if (!cond) {
        std::fprintf(stderr, "FAIL at line %d: %s\n", line, expr);
        std::exit(1);
    }
}

#define CHECK(expr) check((expr), #expr, __LINE__)

uint8_t g_disk_mem[256 * gamma::kBlockSize];

}  // namespace

int main() {
    gamma::RamDisk disk(g_disk_mem, 256);
    gamma::GammaFS fs(&disk);

    CHECK(fs.format());
    CHECK(fs.mount());

    CHECK(fs.mkdir("/docs") == gamma::FsStatus::Ok);
    CHECK(fs.mkdir("/docs") == gamma::FsStatus::AlreadyExists);
    CHECK(fs.touch("/docs/note.txt") == gamma::FsStatus::Ok);

    const char* text = "GammaFS inode filesystem test payload!";
    CHECK(fs.write_file("/docs/note.txt", text, std::strlen(text)) == gamma::FsStatus::Ok);

    char read_buf[128] = {};
    size_t out_len = 0;
    CHECK(fs.read_file("/docs/note.txt", read_buf, sizeof(read_buf), &out_len) == gamma::FsStatus::Ok);
    CHECK(out_len == std::strlen(text));
    CHECK(std::strcmp(read_buf, text) == 0);

    gamma::DirEntryInfo entries[8];
    size_t count = 0;
    CHECK(fs.list_dir("/docs", entries, 8, &count) == gamma::FsStatus::Ok);
    CHECK(count == 1);
    CHECK(std::strcmp(entries[0].name, "note.txt") == 0);
    CHECK(!entries[0].is_dir);

    // Непорожній каталог видаляти заборонено.
    CHECK(fs.remove("/docs") == gamma::FsStatus::NotEmpty);
    CHECK(fs.remove("/docs/note.txt") == gamma::FsStatus::Ok);
    CHECK(fs.remove("/docs") == gamma::FsStatus::Ok);
    CHECK(fs.read_file("/docs/note.txt", read_buf, sizeof(read_buf), &out_len) == gamma::FsStatus::NotFound);

    std::printf("GammaFS unit tests: %d checks, 0 failures\n", g_checks);
    return 0;
}
