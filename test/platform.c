#include <stdio.h>

#include "linux.h"

long _read(int fd, void *buffer, size_t length) {
    long result;
    do {
        result = linux_read(fd, buffer, length);
    } while (result == -EINTR);
    return result;
}

long _write(int fd, const void *buffer, size_t length) {
    long result;
    do {
        result = linux_write(fd, buffer, length);
    } while (result == -EINTR);
    return result;
}
