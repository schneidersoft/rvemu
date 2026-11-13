#include "linux.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check(const char *name, int condition) {
    if (condition) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        failures++;
    }
}

/* Keep the test binary freestanding: the guest has no C library. */
void *memcpy(void *destination, const void *source, size_t length) {
    uint8_t *to = destination;
    const uint8_t *from = source;
    while (length--) {
        *to++ = *from++;
    }
    return destination;
}

void *memset(void *destination, int value, size_t length) {
    uint8_t *to = destination;
    while (length--) {
        *to++ = (uint8_t)value;
    }
    return destination;
}

int memcmp(const void *left, const void *right, size_t length) {
    const uint8_t *a = left;
    const uint8_t *b = right;
    while (length--) {
        if (*a != *b) {
            return *a < *b ? -1 : 1;
        }
        a++;
        b++;
    }
    return 0;
}

void _putchar(char character) { linux_write(1, &character, 1); }

int main(void) {
    printf("STARTING SYSCALL TESTS\n");

    // File I/O is forwarded to the host filesystem.
    const char *path = "bin/syscall-test.tmp";
    const char content[] = "hello from riscv\n";
    const long content_length = (long)(sizeof(content) - 1);
    long fd = linux_openat(AT_FDCWD, path, O_CREAT | O_TRUNC | O_RDWR, 0644);
    check("openat", fd >= 0);
    if (fd >= 0) {
        check("write", linux_write((int)fd, content, (unsigned long)content_length) == content_length);
        check("lseek", linux_lseek((int)fd, 0, SEEK_SET) == 0);

        char buffer[sizeof(content)] = {0};
        long received = linux_read((int)fd, buffer, sizeof(buffer) - 1);
        check("read", received == content_length && memcmp(buffer, content, (size_t)content_length) == 0);

        struct linux_stat file_stat;
        check("fstat", linux_fstat((int)fd, &file_stat) == 0 && file_stat.st_size == content_length);
        check("close", linux_close((int)fd) == 0);
    }
    check("unlinkat", linux_unlinkat(AT_FDCWD, path, 0) == 0);
    check("open missing", linux_openat(AT_FDCWD, path, O_RDONLY, 0) == -ENOENT);

    // Process and system information.
    check("getpid", linux_getpid() > 0);
    check("getuid", linux_getuid() >= 0);

    struct linux_utsname name;
    check("uname", linux_uname(&name) == 0 && name.sysname[0] != 0);

    struct linux_timespec timespec;
    check("clock_gettime", linux_clock_gettime(CLOCK_REALTIME, &timespec) == 0 && timespec.tv_sec > 0);

    struct linux_timeval timeval;
    check("gettimeofday", linux_gettimeofday(&timeval) == 0 && timeval.tv_sec > 0);

    uint8_t random[16];
    check("getrandom", linux_getrandom(random, sizeof(random), 0) == (long)sizeof(random));

    // The heap grows from the end of the loaded image.
    char *heap = (char *)linux_brk(0);
    check("brk query", (long)heap > 0);
    char *new_heap = (char *)linux_brk(heap + 4096);
    check("brk grow", new_heap == heap + 4096);
    if (new_heap == heap + 4096) {
        volatile char *memory = (volatile char *)heap;
        check("brk zeroed", memory[0] == 0 && memory[4095] == 0);
        memory[0] = 42;
        memory[4095] = 43;
        check("brk writable", memory[0] == 42 && memory[4095] == 43);
    }

    // Anonymous mappings come from a separate region below the stack.
    char *mapping = (char *)linux_mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    check("mmap", (long)mapping > 0);
    if ((long)mapping > 0) {
        volatile char *memory = (volatile char *)mapping;
        check("mmap zeroed", memory[0] == 0 && memory[4095] == 0);
        memory[0] = 7;
        memory[4095] = 9;
        check("mmap writable", memory[0] == 7 && memory[4095] == 9);
    }

    // Unsupported and invalid calls report Linux-style errors.
    check("unknown syscall", linux_syscall(9999, 0, 0, 0, 0, 0, 0) == -ENOSYS);
    check("invalid pointer", linux_write(1, (void *)(uintptr_t)0x7fffffff, 1) == -EFAULT);

    printf("SYSCALL TESTS: %d failure(s)\n", failures);
    linux_exit_group(failures ? 1 : 0);
    return 0;
}
