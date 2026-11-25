/*
 * Minimal C library support for the freestanding RVEMU guest: the errno
 * variable, process exit, diagnostics and the time/descriptor wrappers that
 * Linux system calls are exposed through.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "linux.h"

int errno;

static void (*atexit_funcs[32])(void);
static int atexit_count;

int atexit(void (*func)(void)) {
    if (func == NULL || atexit_count >= (int)(sizeof(atexit_funcs)/sizeof(atexit_funcs[0]))) return -1;
    atexit_funcs[atexit_count++] = func;
    return 0;
}

static void run_atexit(void) {
    while (atexit_count > 0) {
        atexit_count--;
        atexit_funcs[atexit_count]();
    }
}

void exit(int status) {
    run_atexit();
    linux_exit_group(status);
    while (1) {
    }
}

void _Exit(int status) {
    linux_exit_group(status);
    while (1) {
    }
}

void abort(void) {
    linux_exit_group(134);
    while (1) {
    }
}

int atoi(const char *string) {
    int sign = 1;
    int result = 0;

    while (*string == ' ' || *string == '\t' || *string == '\n') {
        string++;
    }
    if (*string == '-') {
        sign = -1;
        string++;
    } else if (*string == '+') {
        string++;
    }
    while (*string >= '0' && *string <= '9') {
        result = result * 10 + (*string - '0');
        string++;
    }
    return sign * result;
}

static const struct {
    int number;
    const char *message;
} error_messages[] = {
    {EPERM, "Operation not permitted"},
    {ENOENT, "No such file or directory"},
    {EIO, "Input/output error"},
    {EBADF, "Bad file descriptor"},
    {EAGAIN, "Resource temporarily unavailable"},
    {ENOMEM, "Cannot allocate memory"},
    {EACCES, "Permission denied"},
    {EFAULT, "Bad address"},
    {EINVAL, "Invalid argument"},
    {ENFILE, "Too many open files in system"},
    {EMFILE, "Too many open files"},
    {EPIPE, "Broken pipe"},
    {ENAMETOOLONG, "File name too long"},
    {ENOSYS, "Function not implemented"},
    {ENOTSOCK, "Socket operation on non-socket"},
    {EPROTONOSUPPORT, "Protocol not supported"},
    {EOPNOTSUPP, "Operation not supported"},
    {EAFNOSUPPORT, "Address family not supported by protocol"},
    {EADDRINUSE, "Address already in use"},
    {EADDRNOTAVAIL, "Cannot assign requested address"},
    {ENETDOWN, "Network is down"},
    {ENETUNREACH, "Network is unreachable"},
    {ECONNRESET, "Connection reset by peer"},
    {ENOBUFS, "No buffer space available"},
    {EISCONN, "Transport endpoint is already connected"},
    {ENOTCONN, "Transport endpoint is not connected"},
    {ETIMEDOUT, "Connection timed out"},
    {ECONNREFUSED, "Connection refused"},
    {EHOSTDOWN, "Host is down"},
    {EHOSTUNREACH, "No route to host"},
    {EALREADY, "Operation already in progress"},
    {EINPROGRESS, "Operation now in progress"},
};

char *strerror(int number) {
    for (size_t i = 0; i < sizeof(error_messages) / sizeof(error_messages[0]); i++) {
        if (error_messages[i].number == number) {
            return (char *)error_messages[i].message;
        }
    }
    return (char *)"Unknown error";
}

void perror(const char *string) {
    int saved = errno;
    if (string != NULL && string[0] != '\0') {
        fprintf(stderr, "%s: %s\n", string, strerror(saved));
    } else {
        fprintf(stderr, "%s\n", strerror(saved));
    }
}

int gettimeofday(struct timeval *time, struct timezone *zone) {
    (void)zone;
    if (time == NULL) {
        errno = EFAULT;
        return -1;
    }

    struct linux_timeval value;
    long result = linux_gettimeofday(&value);
    if (result < 0) {
        errno = -(int)result;
        return -1;
    }
    time->tv_sec = (time_t)value.tv_sec;
    time->tv_usec = (suseconds_t)value.tv_usec;
    return 0;
}

int settimeofday(const struct timeval *tv, const struct timezone *tz) {
    (void)tv; (void)tz;
    errno = EPERM;
    return -1;
}

ssize_t read(int fd, void *buffer, size_t length) {
    long result = linux_read(fd, buffer, (unsigned long)length);
    if (result < 0) {
        errno = -(int)result;
        return -1;
    }
    return (ssize_t)result;
}

ssize_t write(int fd, const void *buffer, size_t length) {
    long result = linux_write(fd, buffer, (unsigned long)length);
    if (result < 0) {
        errno = -(int)result;
        return -1;
    }
    return (ssize_t)result;
}

int close(int fd) {
    long result = linux_close(fd);
    if (result < 0) {
        errno = -(int)result;
        return -1;
    }
    return 0;
}

off_t lseek(int fd, off_t offset, int whence) {
    long result = linux_lseek(fd, (long)offset, whence);
    if (result < 0) {
        errno = -(int)result;
        return (off_t)-1;
    }
    return (off_t)result;
}

int open(const char *path, int flags, ...) {
    int mode = 0;
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, int);
        va_end(args);
    }
    long result = linux_openat(AT_FDCWD, path, flags, mode);
    if (result < 0) {
        errno = -(int)result;
        return -1;
    }
    return (int)result;
}

int unlink(const char *path) {
    long result = linux_unlinkat(AT_FDCWD, path, 0);
    if (result < 0) {
        errno = -(int)result;
        return -1;
    }
    return 0;
}

int getpid(void) { return (int)linux_getpid(); }

int getuid(void) { return (int)linux_getuid(); }

int getgid(void) { return 0; }
int geteuid(void) { return (int)linux_getuid(); }
int getegid(void) { return 0; }

int rmdir(const char *path) {
    long r = linux_unlinkat(AT_FDCWD, path, AT_REMOVEDIR);
    if (r < 0) { errno = -(int)r; return -1; }
    return 0;
}

int access(const char *path, int mode) { return faccessat(AT_FDCWD, path, mode, 0); }

int faccessat(int dirfd, const char *path, int mode, int flags) {
    long r = linux_faccessat(dirfd, path, mode, flags);
    if (r < 0) { errno = -(int)r; return -1; }
    return 0;
}

int dup(int fd) {
    long r = linux_dup(fd);
    if (r < 0) { errno = -(int)r; return -1; }
    return (int)r;
}

int dup2(int oldfd, int newfd) {
    if (oldfd == newfd) return newfd;
    long r = linux_dup3(oldfd, newfd, 0);
    if (r < 0) { errno = -(int)r; return -1; }
    return (int)r;
}

int dup3(int oldfd, int newfd, int flags) {
    long r = linux_dup3(oldfd, newfd, flags);
    if (r < 0) { errno = -(int)r; return -1; }
    return (int)r;
}

int pipe(int pipefd[2]) { return pipe2(pipefd, 0); }

int pipe2(int pipefd[2], int flags) {
    long r = linux_pipe2(pipefd, flags);
    if (r < 0) { errno = -(int)r; return -1; }
    return 0;
}

int chdir(const char *path) {
    long r = linux_chdir(path);
    if (r < 0) { errno = -(int)r; return -1; }
    return 0;
}

char *getcwd(char *buf, size_t size) {
    char tmp[256];
    char *use = buf ? buf : tmp;
    size_t usz = buf ? size : sizeof(tmp);
    long r = linux_getcwd(use, usz);
    if (r < 0) { errno = -(int)r; return NULL; }
    if (buf) return buf;
    size_t len = strlen(tmp) + 1;
    char *out = (char *)malloc(len);
    if (out) memcpy(out, tmp, len);
    return out;
}

int isatty(int fd) {
    (void)fd;
    errno = ENOTTY;
    return 0;
}

unsigned int sleep(unsigned int seconds) {
    struct linux_timespec req, rem;
    req.tv_sec = (long)seconds; req.tv_nsec = 0;
    rem.tv_sec = 0; rem.tv_nsec = 0;
    if (linux_nanosleep(&req, &rem) < 0) return (unsigned int)rem.tv_sec;
    return 0;
}

int usleep(unsigned int usec) {
    struct linux_timespec req, rem;
    req.tv_sec = usec / 1000000u;
    req.tv_nsec = (long)(usec % 1000000u) * 1000L;
    if (linux_nanosleep(&req, &rem) < 0) { errno = EINTR; return -1; }
    return 0;
}

ssize_t pread(int fd, void *buf, size_t count, off_t offset) {
    long r = linux_pread64(fd, buf, count, (long)offset);
    if (r < 0) { errno = -(int)r; return -1; }
    return (ssize_t)r;
}

ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset) {
    long r = linux_pwrite64(fd, buf, count, (long)offset);
    if (r < 0) { errno = -(int)r; return -1; }
    return (ssize_t)r;
}

int fsync(int fd) {
    long r = linux_fsync(fd);
    if (r < 0) { errno = -(int)r; return -1; }
    return 0;
}

int ftruncate(int fd, off_t length) {
    long r = linux_ftruncate(fd, (long)length);
    if (r < 0) { errno = -(int)r; return -1; }
    return 0;
}

int truncate(const char *path, off_t length) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    int r = ftruncate(fd, length);
    int saved = errno;
    close(fd);
    if (r < 0) errno = saved;
    return r;
}

int link(const char *oldpath, const char *newpath) {
    long r = linux_linkat(AT_FDCWD, oldpath, AT_FDCWD, newpath, 0);
    if (r < 0) { errno = -(int)r; return -1; }
    return 0;
}

int symlink(const char *target, const char *linkpath) {
    long r = linux_symlinkat(target, AT_FDCWD, linkpath);
    if (r < 0) { errno = -(int)r; return -1; }
    return 0;
}

ssize_t readlink(const char *path, char *buf, size_t bufsiz) {
    long r = linux_readlinkat(AT_FDCWD, path, buf, bufsiz);
    if (r < 0) { errno = -(int)r; return -1; }
    return (ssize_t)r;
}


static int is_space_c(int c) { return c==' '||c=='\f'||c=='\n'||c=='\r'||c=='\t'||c=='\v'; }
static int digit_val(int c) {
    if (c>='0'&&c<='9') return c-'0';
    if (c>='a'&&c<='z') return c-'a'+10;
    if (c>='A'&&c<='Z') return c-'A'+10;
    return -1;
}

unsigned long long strtoull(const char *nptr, char **endptr, int base) {
    const char *p = nptr;
    int neg = 0;
    unsigned long long acc = 0;
    int any = 0;
    while (is_space_c((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) { base = 16; p += 2; }
    else if ((base == 0 || base == 2) && p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) { base = 2; p += 2; }
    else if (base == 0) { base = (p[0] == '0') ? 8 : 10; }
    if (base < 2 || base > 36) { if (endptr) *endptr = (char *)nptr; return 0; }
    for (;;) {
        int d = digit_val((unsigned char)*p);
        if (d < 0 || d >= base) break;
        if (acc > (~(unsigned long long)0 - (unsigned long long)d) / (unsigned long long)base) {
            acc = ~(unsigned long long)0;
            errno = ERANGE;
        } else {
            acc = acc * (unsigned long long)base + (unsigned long long)d;
        }
        any = 1; p++;
    }
    if (endptr) *endptr = (char *)(any ? p : nptr);
    return neg ? -acc : acc;
}

long long strtoll(const char *nptr, char **endptr, int base) {
    const char *p = nptr;
    int neg = 0;
    while (is_space_c((unsigned char)*p)) p++;
    if (*p == '-') neg = 1;
    else if (*p == '+') neg = 0;
    unsigned long long v = strtoull(nptr, endptr, base);
    if (neg) {
        unsigned long long mag = (unsigned long long)(-(long long)v);
        unsigned long long limit = (unsigned long long)1 << 63;
        if (mag > limit) { errno = ERANGE; return (long long)(~limit); }
        return -(long long)mag;
    }
    if (v > (unsigned long long)(((unsigned long long)1 << 63) - 1)) { errno = ERANGE; return (long long)(((unsigned long long)1 << 63) - 1); }
    return (long long)v;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) { return (unsigned long)strtoull(nptr, endptr, base); }
long strtol(const char *nptr, char **endptr, int base) { return (long)strtoll(nptr, endptr, base); }

double strtod(const char *nptr, char **endptr) {
    const char *p = nptr;
    int neg = 0;
    double val = 0.0;
    int any = 0;
    while (is_space_c((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
    while (*p >= '0' && *p <= '9') { any = 1; val = val * 10.0 + (*p - '0'); p++; }
    if (*p == '.') {
        double frac = 1.0;
        p++;
        while (*p >= '0' && *p <= '9') { any = 1; frac /= 10.0; val += (*p - '0') * frac; p++; }
    }
    if (any && (*p == 'e' || *p == 'E')) {
        const char *e = p + 1;
        int eneg = 0; int exp = 0; int eany = 0;
        if (*e == '+' || *e == '-') { eneg = (*e == '-'); e++; }
        while (*e >= '0' && *e <= '9') { eany = 1; exp = exp * 10 + (*e - '0'); e++; }
        if (eany) {
            double pow10 = 1.0;
            while (exp-- > 0) pow10 *= 10.0;
            val = eneg ? val / pow10 : val * pow10;
            p = e;
        }
    }
    if (endptr) *endptr = (char *)(any ? p : nptr);
    return neg ? -val : val;
}

float strtof(const char *nptr, char **endptr) { return (float)strtod(nptr, endptr); }
long double strtold(const char *nptr, char **endptr) { return (long double)strtod(nptr, endptr); }
long atol(const char *s) { return strtol(s, NULL, 10); }
long long atoll(const char *s) { return strtoll(s, NULL, 10); }
double atof(const char *s) { return strtod(s, NULL); }

int abs(int x) { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }
long long llabs(long long x) { return x < 0 ? -x : x; }
div_t div(int numer, int denom) { div_t r; r.quot = numer / denom; r.rem = numer % denom; return r; }
ldiv_t ldiv(long numer, long denom) { ldiv_t r; r.quot = numer / denom; r.rem = numer % denom; return r; }
lldiv_t lldiv(long long numer, long long denom) { lldiv_t r; r.quot = numer / denom; r.rem = numer % denom; return r; }

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    unsigned char *b = (unsigned char *)base;
    unsigned char *tmp = (unsigned char *)malloc(size);
    if (tmp == NULL) return;
    /* simple shell sort for small libc */
    for (size_t gap = nmemb / 2; gap > 0; gap /= 2) {
        for (size_t i = gap; i < nmemb; i++) {
            memcpy(tmp, b + i * size, size);
            size_t j = i;
            while (j >= gap && compar(b + (j - gap) * size, tmp) > 0) {
                memcpy(b + j * size, b + (j - gap) * size, size);
                j -= gap;
            }
            memcpy(b + j * size, tmp, size);
        }
    }
    free(tmp);
}

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    const unsigned char *b = (const unsigned char *)base;
    size_t lo = 0, hi = nmemb;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = compar(key, b + mid * size);
        if (c == 0) return (void *)(b + mid * size);
        if (c > 0) lo = mid + 1; else hi = mid;
    }
    return NULL;
}

static unsigned int rand_state = 1;
int rand(void) { rand_state = rand_state * 1103515245u + 12345u; return (int)((rand_state >> 16) & 0x7fffffff); }
void srand(unsigned int seed) { rand_state = seed ? seed : 1; }

char *getenv(const char *name) { (void)name; return NULL; }
int system(const char *command) { if (command == NULL) return 0; errno = ENOSYS; return -1; }

typedef struct malloc_block {
    size_t size;
    int free;
    struct malloc_block *next;
} malloc_block;

static malloc_block *malloc_head;

static size_t malloc_align(size_t size) {
    const size_t align = sizeof(long);
    return (size + align - 1) & ~(align - 1);
}

static malloc_block *malloc_find(size_t size) {
    malloc_block *block = malloc_head;
    while (block != NULL) {
        if (block->free && block->size >= size) {
            return block;
        }
        block = block->next;
    }
    return NULL;
}

static malloc_block *malloc_grow(size_t size) {
    malloc_block *block = (malloc_block *)linux_brk(0);
    void *request = (char *)block + sizeof(malloc_block) + size;
    if ((long)request <= 0) {
        return NULL;
    }
    if (linux_brk(request) != (long)request) {
        return NULL;
    }
    block->size = size;
    block->free = 0;
    block->next = NULL;
    return block;
}

void *malloc(size_t size) {
    malloc_block *block;
    size_t aligned;

    if (size == 0) {
        size = 1;
    }
    aligned = malloc_align(size);
    block = malloc_find(aligned);
    if (block != NULL) {
        block->free = 0;
        return (char *)block + sizeof(malloc_block);
    }

    block = malloc_grow(aligned);
    if (block == NULL) {
        errno = ENOMEM;
        return NULL;
    }
    if (malloc_head == NULL) {
        malloc_head = block;
    } else {
        malloc_block *tail = malloc_head;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = block;
    }
    return (char *)block + sizeof(malloc_block);
}

void free(void *ptr) {
    malloc_block *block;
    if (ptr == NULL) {
        return;
    }
    block = (malloc_block *)((char *)ptr - sizeof(malloc_block));
    block->free = 1;
}

void *calloc(size_t count, size_t size) {
    size_t total = count * size;
    void *ptr;
    if (count != 0 && total / count != size) {
        errno = ENOMEM;
        return NULL;
    }
    ptr = malloc(total);
    if (ptr != NULL) {
        memset(ptr, 0, total);
    }
    return ptr;
}

void *realloc(void *ptr, size_t size) {
    malloc_block *block;
    void *next;
    if (ptr == NULL) {
        return malloc(size);
    }
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    block = (malloc_block *)((char *)ptr - sizeof(malloc_block));
    if (block->size >= malloc_align(size)) {
        return ptr;
    }
    next = malloc(size);
    if (next == NULL) {
        return NULL;
    }
    memcpy(next, ptr, block->size);
    free(ptr);
    return next;
}
