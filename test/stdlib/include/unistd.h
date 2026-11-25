#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>
#include <sys/types.h>

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

int close(int fd);
ssize_t read(int fd, void *buffer, size_t length);
ssize_t write(int fd, const void *buffer, size_t length);
off_t lseek(int fd, off_t offset, int whence);
int unlink(const char *path);
int rmdir(const char *path);
int access(const char *path, int mode);
int faccessat(int dirfd, const char *path, int mode, int flags);
int dup(int fd);
int dup2(int oldfd, int newfd);
int dup3(int oldfd, int newfd, int flags);
int pipe(int pipefd[2]);
int pipe2(int pipefd[2], int flags);
int chdir(const char *path);
char *getcwd(char *buf, size_t size);
int isatty(int fd);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
int fsync(int fd);
int ftruncate(int fd, off_t length);
int truncate(const char *path, off_t length);
int link(const char *oldpath, const char *newpath);
int symlink(const char *target, const char *linkpath);
ssize_t readlink(const char *path, char *buf, size_t bufsiz);
int getpid(void);
int getuid(void);
int getgid(void);
int geteuid(void);
int getegid(void);

#endif
