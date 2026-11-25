#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>
#include <sys/types.h>

int close(int fd);
ssize_t read(int fd, void *buffer, size_t length);
ssize_t write(int fd, const void *buffer, size_t length);
long lseek(int fd, long offset, int whence);
int unlink(const char *path);
int getpid(void);
int getuid(void);

#endif
