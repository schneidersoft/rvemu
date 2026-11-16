/*
 * Minimal C library support for the freestanding RVEMU guest: the errno
 * variable, process exit, diagnostics and the time/descriptor wrappers that
 * Linux system calls are exposed through.
 */

#include <errno.h>
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

int getpid(void) { return (int)linux_getpid(); }

int getuid(void) { return (int)linux_getuid(); }
