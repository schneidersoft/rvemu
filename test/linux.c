#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

/* Platform-specific syscall probes (uname, clock_gettime, getrandom, brk,
 * mmap, raw syscall, invalid pointer) that have no portable stdlib wrapper
 * in this minimal guest libc. */
#include "linux.h"

static int failures;

static void check(const char *name, int condition) {
    if (condition) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        failures++;
    }
}

int main(void) {
    printf("STARTING SYSCALL TESTS\n");

    // Standard streams are real FILE objects with buffered output.
    check("fileno", fileno(stdin) == 0 && fileno(stdout) == 1 && fileno(stderr) == 2);
    check("fprintf", fprintf(stdout, "fprintf works\n") > 0);
    check("fprintf stderr", fprintf(stderr, "stderr works\n") > 0);
    check("fputc", fputc('F', stdout) != EOF && fputc('\n', stdout) != EOF);
    check("fputs", fputs("fputs works\n", stdout) >= 0);
    check("fwrite", fwrite("fwrite works\n", 1, 13, stdout) == 13);
    check("puts", puts("puts works") >= 0);
    check("putchar", putchar('P') != EOF && putchar('\n') != EOF);
    check("fflush", fflush(stdout) == 0);
    check("stream state", feof(stdout) == 0 && ferror(stdout) == 0);
    check("ungetc", ungetc('Z', stdin) == 'Z' && fgetc(stdin) == 'Z');

    char empty[1];
    check("fgets size 1", fgets(empty, 1, stdin) == empty && empty[0] == '\0');

    // File I/O through the portable stdlib/unistd interface.
    // CWD-relative on purpose: the harness runs the emulator from the repo
    // root (and the matrix container from any directory), so no subdir
    // assumption is allowed here.
    const char *path = "syscall-test.tmp";
    const char content[] = "hello from riscv\n";
    const long content_length = (long)(sizeof(content) - 1);
    int fd = open(path, O_CREAT | O_TRUNC | O_RDWR, 0644);
    check("open", fd >= 0);
    if (fd >= 0) {
        check("write", write(fd, content, (size_t)content_length) == content_length);
        check("lseek", lseek(fd, 0, SEEK_SET) == 0);

        char buffer[sizeof(content)] = {0};
        ssize_t received = read(fd, buffer, sizeof(buffer) - 1);
        check("read", received == content_length && memcmp(buffer, content, (size_t)content_length) == 0);

        check("lseek end", lseek(fd, 0, SEEK_END) == content_length);
        check("close", close(fd) == 0);
    }
    check("unlink", unlink(path) == 0);
    errno = 0;
    check("open missing", open(path, O_RDONLY, 0) == -1 && errno == ENOENT);

    // Process and system information.
    check("getpid", getpid() > 0);
    check("getuid", getuid() >= 0);

    struct linux_utsname name;
    check("uname", linux_uname(&name) == 0 && name.sysname[0] != 0);

    struct linux_timespec timespec;
    check("clock_gettime", linux_clock_gettime(CLOCK_REALTIME, &timespec) == 0 && timespec.tv_sec > 0);

    struct timeval timeval_value;
    check("gettimeofday", gettimeofday(&timeval_value, 0) == 0 && timeval_value.tv_sec > 0);

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
    check("invalid pointer", write(1, (void *)(uintptr_t)0x7fffffff, 1) == -1 && errno == EFAULT);

    printf("SYSCALL TESTS: %d failure(s)\n", failures);
    fflush(NULL);
    exit(failures ? 1 : 0);
    return 0;
}
