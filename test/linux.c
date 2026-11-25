#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* Platform-specific syscall probes (uname, clock_gettime, getrandom, brk,
 * mmap, raw syscall, invalid pointer) that have no portable stdlib wrapper
 * in this minimal guest libc. */
#include "linux.h"

static int failures;

static int cmp_int_test(const void *a, const void *b){ return *(const int*)a - *(const int*)b; }

static void check(const char *name, int condition) {
    if (condition) {
        printf("PASS: %s\n", name);
    } else {
        printf("FAIL: %s\n", name);
        failures++;
    }
}

struct cookie_buf {
    const char *data;
    size_t pos;
    size_t len;
};

static ssize_t cookie_read(void *cookie, char *buf, size_t size) {
    struct cookie_buf *c = (struct cookie_buf *)cookie;
    size_t n = c->len - c->pos;
    if (n > size) {
        n = size;
    }
    if (n > 0) {
        memcpy(buf, c->data + c->pos, n);
        c->pos += n;
    }
    return (ssize_t)n;
}

static ssize_t cookie_write(void *cookie, const char *buf, size_t size) {
    (void)cookie;
    (void)buf;
    return (ssize_t)size;
}

static int cookie_close(void *cookie) {
    (void)cookie;
    return 0;
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

    // In-memory stdio streams.
    {
        char mem[64] = "hello";
        char out[64] = {0};
        FILE *f = fmemopen(mem, sizeof(mem), "r+");
        check("fmemopen open", f != NULL);
        if (f != NULL) {
            check("fmemopen read", fread(out, 1, 5, f) == 5 && memcmp(out, "hello", 5) == 0);
            check("fmemopen seek", fseek(f, 0, SEEK_SET) == 0);
            check("fmemopen write", fprintf(f, "world") == 5);
            check("fmemopen fflush", fflush(f) == 0);
            check("fmemopen content", memcmp(mem, "world", 5) == 0);
            check("fmemopen close", fclose(f) == 0);
        }

        FILE *f2 = fmemopen(NULL, 32, "w+");
        check("fmemopen alloc open", f2 != NULL);
        if (f2 != NULL) {
            check("fmemopen alloc write", fputs("abc", f2) >= 0);
            check("fmemopen alloc seek", fseek(f2, 0, SEEK_SET) == 0);
            memset(out, 0, sizeof(out));
            check("fmemopen alloc read", fgets(out, sizeof(out), f2) != NULL && strcmp(out, "abc") == 0);
            check("fmemopen alloc close", fclose(f2) == 0);
        }
    }

    {
        char *buf = NULL;
        size_t size = 0;
        FILE *f = open_memstream(&buf, &size);
        check("open_memstream open", f != NULL && buf != NULL);
        if (f != NULL) {
            check("open_memstream write", fprintf(f, "hello %d", 42) == 8);
            check("open_memstream fflush", fflush(f) == 0);
            check("open_memstream size", size == 8);
            check("open_memstream content", strcmp(buf, "hello 42") == 0);
            check("open_memstream ftell", ftell(f) == 8);
            check("open_memstream close", fclose(f) == 0);
            check("open_memstream final", size == 8 && strcmp(buf, "hello 42") == 0);
            free(buf);
        }
    }

    {
        struct cookie_buf cb = {"abc", 0, 3};
        cookie_io_functions_t io = {cookie_read, cookie_write, NULL, cookie_close};
        char out[8] = {0};
        FILE *f = fopencookie(&cb, "r", io);
        check("fopencookie open", f != NULL);
        if (f != NULL) {
            check("fopencookie read", fread(out, 1, 3, f) == 3 && memcmp(out, "abc", 3) == 0);
            check("fopencookie eof", fgetc(f) == EOF && feof(f) != 0);
            check("fopencookie close", fclose(f) == 0);
        }
    }

    // string additions
    {
        char buf[32];
        check("memchr", memchr("abcde", 'c', 5) == (void *)("abcde" + 2));
        check("strnlen", strnlen("hello", 3) == 3 && strnlen("hi", 10) == 2);
        char *d = strdup("test"); check("strdup", d && strcmp(d, "test") == 0); free(d);
        char *nd = strndup("abcdef", 3); check("strndup", nd && strcmp(nd, "abc") == 0); free(nd);
        char *save=NULL;
        strcpy(buf, "a,b,c");
        check("strtok_r", strcmp(strtok_r(buf, ",", &save), "a") == 0 && strcmp(strtok_r(NULL, ",", &save), "b") == 0 && strcmp(strtok_r(NULL, ",", &save), "c") == 0 && strtok_r(NULL, ",", &save) == NULL);
        check("strcasecmp", strcasecmp("AbC", "aBC") == 0 && strcasecmp("a", "B") < 0);
        check("strncasecmp", strncasecmp("AbCdef", "aBCxyz", 3) == 0);
        char sb[16]; check("stpcpy", stpcpy(sb, "hi") == sb+2 && strcmp(sb,"hi")==0);
        check("stpncpy", stpncpy(sb, "hello", 3) == sb+3 && memcmp(sb,"hel",3)==0);
    }

    // stdlib additions
    {
        char *end;
        check("strtol", strtol(" -42x", &end, 10) == -42 && *end == 'x');
        check("strtoul", strtoul("0xff", &end, 16) == 255 && *end == '\0');
        check("strtoll", strtoll("-1234567890123", NULL, 10) == -1234567890123LL);
        check("strtoull", strtoull("18446744073709551615", NULL, 10) == ~0ULL);
        check("strtod", strtod("1.5e2", &end) == 150.0 && *end == '\0');
        check("atol/atoll/atof", atol("123") == 123 && atoll("-99") == -99 && atof("2.5") == 2.5);
        check("abs/labs/llabs", abs(-5)==5 && labs(-5L)==5L && llabs(-5LL)==5LL);
        div_t dv = div(7,3); ldiv_t ld = ldiv(-7L,3L);
        check("div/ldiv", dv.quot==2 && dv.rem==1 && ld.quot==-2 && ld.rem==-1);
        int arr[5] = {5,1,4,2,3};
        qsort(arr, 5, sizeof(int), cmp_int_test);
        check("qsort", arr[0]==1 && arr[1]==2 && arr[2]==3 && arr[3]==4 && arr[4]==5);
        int key = 4;
        int *found = (int *)bsearch(&key, arr, 5, sizeof(int), cmp_int_test);
        check("bsearch", found && *found == 4);
        srand(1); int r1 = rand(); srand(1); int r2 = rand();
        check("rand/srand", r1 == r2);
    }

    // time additions
    {
        struct tm tm; time_t t = 0;
        check("gmtime_r epoch", gmtime_r(&t, &tm) && tm.tm_year==70 && tm.tm_mon==0 && tm.tm_mday==1 && tm.tm_hour==0 && tm.tm_min==0 && tm.tm_sec==0 && tm.tm_wday==4);
        time_t t2 = 1609459200; /* 2021-01-01T00:00:00Z */
        check("gmtime_r 2021", gmtime_r(&t2, &tm) && tm.tm_year==121 && tm.tm_mon==0 && tm.tm_mday==1);
        struct tm tm3; tm3.tm_year=121; tm3.tm_mon=0; tm3.tm_mday=1; tm3.tm_hour=0; tm3.tm_min=0; tm3.tm_sec=0; tm3.tm_isdst=0;
        check("mktime", mktime(&tm3) == 1609459200);
        char tb[32];
        struct tm tm2; time_t t3v = 1609459200; gmtime_r(&t3v, &tm2);
        check("strftime", strftime(tb, sizeof(tb), "%Y-%m-%dT%H:%M:%S", &tm2) == 19 && strcmp(tb, "2021-01-01T00:00:00")==0);
        check("difftime", difftime(10, 3) == 7.0);
        check("time", time(NULL) > 0);
    }

    // stdio file positioning
    {
        char *buf = NULL; size_t sz = 0;
        FILE *f = open_memstream(&buf, &sz);
        check("pos open_memstream", f != NULL);
        if (f) {
            fputs("abcdef", f); fflush(f);
            check("ftello", ftello(f) == 6);
            check("fseeko", fseeko(f, 2, SEEK_SET) == 0 && ftello(f) == 2);
            rewind(f);
            check("rewind", ftello(f) == 0 && ferror(f)==0);
            fpos_t pos;
            fseeko(f, 3, SEEK_SET);
            check("fgetpos", fgetpos(f, &pos) == 0);
            fseeko(f, 0, SEEK_SET);
            check("fsetpos", fsetpos(f, &pos) == 0 && ftello(f) == 3);
            fclose(f);
            free(buf);
        }
    }

    // networking: byte order and inet
    {
        check("htons/ntohs", htons(0x1234) == 0x3412 && ntohs(0x3412) == 0x1234);
        check("htonl/ntohl", htonl(0x01020304) == 0x04030201 && ntohl(0x04030201) == 0x01020304);
        struct in_addr a;
        check("inet_pton", inet_pton(AF_INET, "127.0.0.1", &a) == 1);
        char ip[32];
        check("inet_ntop", inet_ntop(AF_INET, &a, ip, sizeof(ip)) != NULL && strcmp(ip, "127.0.0.1") == 0);
    }

    // Unsupported and invalid calls report Linux-style errors.
    check("unknown syscall", linux_syscall(9999, 0, 0, 0, 0, 0, 0) == -ENOSYS);
    check("invalid pointer", write(1, (void *)(uintptr_t)0x7fffffff, 1) == -1 && errno == EFAULT);

    printf("SYSCALL TESTS: %d failure(s)\n", failures);
    fflush(NULL);
    exit(failures ? 1 : 0);
    return 0;
}
