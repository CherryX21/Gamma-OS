#include "shell.hpp"

#include <stddef.h>
#include <stdint.h>

#include "console.hpp"
#include "fs.hpp"
#include "heap.hpp"
#include "interrupts.hpp"
#include "io.hpp"
#include "keyboard.hpp"
#include "kstring.hpp"
#include "log.hpp"
#include "pmm.hpp"
#include "timer.hpp"

namespace gamma {

namespace {

constexpr size_t kMaxLineLen = 255;
constexpr size_t kMaxArgs = 16;

char g_cwd[kMaxPathLen] = "/";

struct Command {
    const char* name;
    const char* usage;
    const char* description;
    void (*handler)(int argc, char** argv, const char* raw_args);
};

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

void build_path(const char* input, char* out, size_t out_size) {
    if (input == nullptr || input[0] == '\0') {
        str_copy(out, g_cwd, out_size);
        return;
    }
    if (input[0] == '/') {
        str_copy(out, input, out_size);
        return;
    }
    size_t pos = 0;
    while (g_cwd[pos] != '\0' && pos + 1 < out_size) {
        out[pos] = g_cwd[pos];
        pos++;
    }
    if (pos > 0 && out[pos - 1] != '/' && pos + 1 < out_size) {
        out[pos++] = '/';
    }
    size_t i = 0;
    while (input[i] != '\0' && pos + 1 < out_size) {
        out[pos++] = input[i++];
    }
    out[pos] = '\0';
}

void cmd_help(int, char**, const char*);

void cmd_clear(int, char**, const char*) {
    g_console.clear();
}

void cmd_echo(int, char**, const char* raw_args) {
    kprintf("%s\n", raw_args != nullptr ? raw_args : "");
}

void cmd_version(int, char**, const char*) {
    kprintf("Gamma OS 0.1 (x86_64, UEFI, Limine protocol)\n");
    kprintf("Kernel: freestanding C++20, GammaFS v1\n");
}

void cmd_mem(int, char**, const char*) {
    const uint64_t total_mib = (g_pmm.total_pages() * kPageSize) / (1024 * 1024);
    const uint64_t free_mib = (g_pmm.free_pages() * kPageSize) / (1024 * 1024);
    const HeapStats hs = heap_stats();
    kprintf("Physical RAM: Total %lu MiB | Free %lu MiB (%lu pages)\n",
            total_mib, free_mib, static_cast<uint64_t>(g_pmm.free_pages()));
    kprintf("Kernel Heap:  Total %lu KiB | Used %lu B | Free %lu B\n",
            static_cast<uint64_t>(hs.total_bytes / 1024),
            static_cast<uint64_t>(hs.used_bytes),
            static_cast<uint64_t>(hs.free_bytes));
}

void cmd_uptime(int, char**, const char*) {
    const uint64_t sec = timer_uptime_seconds();
    const uint64_t h = sec / 3600;
    const uint64_t m = (sec % 3600) / 60;
    const uint64_t s = sec % 60;
    kprintf("Up %02lu:%02lu:%02lu (%lu ticks)\n", h, m, s, timer_ticks());
}

void cmd_pwd(int, char**, const char*) {
    kprintf("%s\n", g_cwd);
}

void cmd_cd(int argc, char** argv, const char*) {
    if (argc < 2 || str_eq(argv[1], "/") || str_eq(argv[1], "~")) {
        str_copy(g_cwd, "/", sizeof(g_cwd));
        return;
    }
    if (str_eq(argv[1], "..")) {
        size_t len = str_len(g_cwd);
        while (len > 1 && g_cwd[len - 1] == '/') g_cwd[--len] = '\0';
        while (len > 1 && g_cwd[len - 1] != '/') g_cwd[--len] = '\0';
        if (len > 1 && g_cwd[len - 1] == '/') g_cwd[--len] = '\0';
        return;
    }
    char target[kMaxPathLen];
    build_path(argv[1], target, sizeof(target));
    bool is_dir = false;
    const FsStatus st = vfs_root()->stat(target, &is_dir, nullptr);
    if (st != FsStatus::Ok) {
        kprintf("cd: %s: %s\n", target, fs_status_str(st));
        return;
    }
    if (!is_dir) {
        kprintf("cd: %s: not a directory\n", target);
        return;
    }
    str_copy(g_cwd, target, sizeof(g_cwd));
}

void cmd_ls(int argc, char** argv, const char*) {
    char target[kMaxPathLen];
    build_path(argc >= 2 ? argv[1] : "", target, sizeof(target));

    DirEntryInfo entries[32];
    size_t count = 0;
    const FsStatus st = vfs_root()->list_dir(target, entries, 32, &count);
    if (st != FsStatus::Ok) {
        kprintf("ls: %s: %s\n", target, fs_status_str(st));
        return;
    }
    if (count == 0) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        if (entries[i].is_dir) {
            kprintf("%s/   ", entries[i].name);
        } else {
            kprintf("%s (%u B)   ", entries[i].name, entries[i].size);
        }
    }
    kprintf("\n");
}

void cmd_cat(int argc, char** argv, const char*) {
    if (argc < 2) {
        kprintf("Usage: cat <file>\n");
        return;
    }
    char target[kMaxPathLen];
    build_path(argv[1], target, sizeof(target));

    char buf[1024];
    size_t out_len = 0;
    const FsStatus st = vfs_root()->read_file(target, buf, sizeof(buf) - 1, &out_len);
    if (st != FsStatus::Ok) {
        kprintf("cat: %s: %s\n", target, fs_status_str(st));
        return;
    }
    buf[out_len] = '\0';
    kprintf("%s", buf);
    if (out_len > 0 && buf[out_len - 1] != '\n') {
        kprintf("\n");
    }
}

void cmd_mkdir(int argc, char** argv, const char*) {
    if (argc < 2) {
        kprintf("Usage: mkdir <dir>\n");
        return;
    }
    char target[kMaxPathLen];
    build_path(argv[1], target, sizeof(target));
    const FsStatus st = vfs_root()->mkdir(target);
    if (st != FsStatus::Ok) {
        kprintf("mkdir: %s: %s\n", target, fs_status_str(st));
    }
}

void cmd_touch(int argc, char** argv, const char*) {
    if (argc < 2) {
        kprintf("Usage: touch <file>\n");
        return;
    }
    char target[kMaxPathLen];
    build_path(argv[1], target, sizeof(target));
    const FsStatus st = vfs_root()->touch(target);
    if (st != FsStatus::Ok) {
        kprintf("touch: %s: %s\n", target, fs_status_str(st));
    }
}

void cmd_write(int argc, char** argv, const char* raw_args) {
    if (argc < 3 || raw_args == nullptr) {
        kprintf("Usage: write <file> <text>\n");
        return;
    }
    char target[kMaxPathLen];
    build_path(argv[1], target, sizeof(target));

    const char* p = raw_args;
    while (*p == ' ') p++;
    while (*p != '\0' && *p != ' ') p++;
    while (*p == ' ') p++;

    char payload[kMaxLineLen + 2];
    size_t len = str_len(p);
    if (len > kMaxLineLen) len = kMaxLineLen;
    memcpy(payload, p, len);
    payload[len++] = '\n';
    payload[len] = '\0';

    const FsStatus st = vfs_root()->write_file(target, payload, len);
    if (st != FsStatus::Ok) {
        kprintf("write: %s: %s\n", target, fs_status_str(st));
    }
}

void cmd_rm(int argc, char** argv, const char*) {
    if (argc < 2) {
        kprintf("Usage: rm <path>\n");
        return;
    }
    char target[kMaxPathLen];
    build_path(argv[1], target, sizeof(target));
    const FsStatus st = vfs_root()->remove(target);
    if (st != FsStatus::Ok) {
        kprintf("rm: %s: %s\n", target, fs_status_str(st));
    }
}

void cmd_reboot(int, char**, const char*) {
    kprintf("Rebooting system...\n");
    asm volatile("cli");
    while ((inb(0x64) & 0x02) != 0) {}
    outb(0x64, 0xFE);
    halt_forever();
}

constexpr Command kCommands[] = {
    {"help",    "help",               "Show available commands",         cmd_help},
    {"clear",   "clear",              "Clear terminal screen",           cmd_clear},
    {"echo",    "echo <text>",        "Print text to console",           cmd_echo},
    {"version", "version",            "Show OS version info",            cmd_version},
    {"mem",     "mem",                "Show RAM and heap statistics",    cmd_mem},
    {"uptime",  "uptime",             "Show system uptime",              cmd_uptime},
    {"pwd",     "pwd",                "Print working directory",         cmd_pwd},
    {"cd",      "cd <dir>",           "Change current directory",        cmd_cd},
    {"ls",      "ls [dir]",           "List directory contents",         cmd_ls},
    {"cat",     "cat <file>",         "Display file contents",           cmd_cat},
    {"mkdir",   "mkdir <dir>",        "Create directory",                cmd_mkdir},
    {"touch",   "touch <file>",       "Create empty file",               cmd_touch},
    {"write",   "write <file> <txt>", "Write text into file",            cmd_write},
    {"rm",      "rm <path>",          "Remove file or empty directory",  cmd_rm},
    {"reboot",  "reboot",             "Restart virtual machine",         cmd_reboot},
};

constexpr size_t kCommandCount = sizeof(kCommands) / sizeof(kCommands[0]);

void cmd_help(int, char**, const char*) {
    kprintf("Available commands:\n");
    for (size_t i = 0; i < kCommandCount; i++) {
        kprintf("  %-18s - %s\n", kCommands[i].usage, kCommands[i].description);
    }
}

void execute_line(char* line) {
    char* p = line;
    while (*p == ' ') p++;
    if (*p == '\0') {
        return;
    }

    // Копіюємо сирі аргументи в окремий буфер до того, як розіб'ємо line на argv.
    char raw_args[kMaxLineLen + 1] = {};
    char* after_cmd = p;
    while (*after_cmd != '\0' && *after_cmd != ' ') after_cmd++;
    if (*after_cmd == ' ') {
        const char* src = after_cmd + 1;
        while (*src == ' ') src++;
        str_copy(raw_args, src, sizeof(raw_args));
    }

    char* argv[kMaxArgs];
    int argc = 0;
    while (*p != '\0' && argc < static_cast<int>(kMaxArgs)) {
        argv[argc++] = p;
        while (*p != '\0' && *p != ' ') p++;
        if (*p == ' ') {
            *p++ = '\0';
            while (*p == ' ') p++;
        }
    }

    for (size_t i = 0; i < kCommandCount; i++) {
        if (str_eq(argv[0], kCommands[i].name)) {
            kCommands[i].handler(argc, argv, raw_args);
            return;
        }
    }
    kprintf("Unknown command: '%s'. Type 'help' for a list of commands.\n", argv[0]);
}

}  // namespace

[[noreturn]] void shell_run() {
    char line[kMaxLineLen + 1];
    while (true) {
        kprintf("gamma:%s> ", g_cwd);
        size_t len = 0;
        while (true) {
            const char c = keyboard_getchar();
            if (c == '\n') {
                kprintf("\n");
                line[len] = '\0';
                break;
            }
            if (c == '\b') {
                if (len > 0) {
                    len--;
                    kprintf("\b");
                }
                continue;
            }
            if (c >= 32 && c < 127 && len < kMaxLineLen) {
                line[len++] = c;
                kprintf("%c", c);
            }
        }
        execute_line(line);
    }
}

}  // namespace gamma
