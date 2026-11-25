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

void exit(int status) {
    linux_exit_group(status);
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

const char *strerror(int number) {
    for (size_t i = 0; i < sizeof(error_messages) / sizeof(error_messages[0]); i++) {
        if (error_messages[i].number == number) {
            return error_messages[i].message;
        }
    }
    return "Unknown error";
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

long lseek(int fd, long offset, int whence) {
    long result = linux_lseek(fd, offset, whence);
    if (result < 0) {
        errno = -(int)result;
        return -1;
    }
    return result;
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
