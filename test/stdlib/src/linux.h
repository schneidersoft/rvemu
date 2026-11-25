#ifndef LINUX_H
#define LINUX_H

#include <stdint.h>

#include "guest.h"

struct linux_timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct linux_timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};

struct linux_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

/* Linux asm-generic struct stat, as passed to guest code. */
struct linux_stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    uint64_t pad1;
    int64_t st_size;
    int32_t st_blksize;
    int32_t pad2;
    int64_t st_blocks;
    int64_t st_atime_sec;
    uint64_t st_atime_nsec;
    int64_t st_mtime_sec;
    uint64_t st_mtime_nsec;
    int64_t st_ctime_sec;
    uint64_t st_ctime_nsec;
    uint32_t unused4;
    uint32_t unused5;
};

static inline long linux_syscall(long number, long arg0, long arg1, long arg2, long arg3, long arg4, long arg5) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a7 __asm__("a7") = number;
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a7) : "memory");
    return a0;
}

static inline long linux_write(int fd, const void *buffer, unsigned long length) { return linux_syscall(SYS_write, fd, (long)(uintptr_t)buffer, (long)length, 0, 0, 0); }

static inline long linux_read(int fd, void *buffer, unsigned long length) { return linux_syscall(SYS_read, fd, (long)(uintptr_t)buffer, (long)length, 0, 0, 0); }

static inline long linux_openat(int dirfd, const char *path, int flags, int mode) { return linux_syscall(SYS_openat, dirfd, (long)(uintptr_t)path, flags, mode, 0, 0); }

static inline long linux_close(int fd) { return linux_syscall(SYS_close, fd, 0, 0, 0, 0, 0); }

static inline long linux_lseek(int fd, long offset, int whence) { return linux_syscall(SYS_lseek, fd, offset, whence, 0, 0, 0); }

static inline long linux_unlinkat(int dirfd, const char *path, int flags) { return linux_syscall(SYS_unlinkat, dirfd, (long)(uintptr_t)path, flags, 0, 0, 0); }

static inline long linux_fstat(int fd, void *stat_buffer) { return linux_syscall(SYS_fstat, fd, (long)(uintptr_t)stat_buffer, 0, 0, 0, 0); }

static inline long linux_getpid(void) { return linux_syscall(SYS_getpid, 0, 0, 0, 0, 0, 0); }

static inline long linux_getuid(void) { return linux_syscall(SYS_getuid, 0, 0, 0, 0, 0, 0); }

static inline long linux_uname(struct linux_utsname *name) { return linux_syscall(SYS_uname, (long)(uintptr_t)name, 0, 0, 0, 0, 0); }

static inline long linux_clock_gettime(int clock, struct linux_timespec *time) { return linux_syscall(SYS_clock_gettime, clock, (long)(uintptr_t)time, 0, 0, 0, 0); }

static inline long linux_gettimeofday(struct linux_timeval *time) { return linux_syscall(SYS_gettimeofday, (long)(uintptr_t)time, 0, 0, 0, 0, 0); }

static inline long linux_brk(void *address) { return linux_syscall(SYS_brk, (long)(uintptr_t)address, 0, 0, 0, 0, 0); }

static inline long linux_mmap(void *address, unsigned long length, int prot, int flags, int fd, long offset) {
    return linux_syscall(SYS_mmap, (long)(uintptr_t)address, (long)length, prot, flags, fd, offset);
}

static inline long linux_getrandom(void *buffer, unsigned long length, unsigned int flags) {
    return linux_syscall(SYS_getrandom, (long)(uintptr_t)buffer, (long)length, (long)flags, 0, 0, 0);
}

static inline long linux_socket(int domain, int type, int protocol) { return linux_syscall(SYS_socket, domain, type, protocol, 0, 0, 0); }

static inline long linux_connect(int fd, const void *address, long length) {
    return linux_syscall(SYS_connect, fd, (long)(uintptr_t)address, length, 0, 0, 0);
}

static inline long linux_sendto(int fd, const void *buffer, unsigned long length, int flags, const void *destination, long destination_length) {
    return linux_syscall(SYS_sendto, fd, (long)(uintptr_t)buffer, (long)length, flags, (long)(uintptr_t)destination, destination_length);
}

static inline long linux_recvfrom(int fd, void *buffer, unsigned long length, int flags, void *source, void *source_length) {
    return linux_syscall(SYS_recvfrom, fd, (long)(uintptr_t)buffer, (long)length, flags, (long)(uintptr_t)source, (long)(uintptr_t)source_length);
}

static inline long linux_setsockopt(int fd, int level, int option, const void *value, long length) {
    return linux_syscall(SYS_setsockopt, fd, level, option, (long)(uintptr_t)value, length, 0);
}

static inline long linux_getsockopt(int fd, int level, int option, void *value, void *length) {
    return linux_syscall(SYS_getsockopt, fd, level, option, (long)(uintptr_t)value, (long)(uintptr_t)length, 0);
}

static inline long linux_shutdown(int fd, int how) { return linux_syscall(SYS_shutdown, fd, how, 0, 0, 0, 0); }

static inline long linux_bind(int fd, const void *address, long length) { return linux_syscall(SYS_bind, fd, (long)(uintptr_t)address, length, 0, 0, 0); }

static inline long linux_listen(int fd, int backlog) { return linux_syscall(SYS_listen, fd, backlog, 0, 0, 0, 0); }

static inline long linux_accept(int fd, void *address, void *length) { return linux_syscall(SYS_accept, fd, (long)(uintptr_t)address, (long)(uintptr_t)length, 0, 0, 0); }

static inline long linux_getsockname(int fd, void *address, void *length) { return linux_syscall(SYS_getsockname, fd, (long)(uintptr_t)address, (long)(uintptr_t)length, 0, 0, 0); }

static inline long linux_getpeername(int fd, void *address, void *length) { return linux_syscall(SYS_getpeername, fd, (long)(uintptr_t)address, (long)(uintptr_t)length, 0, 0, 0); }

static inline long linux_dup(int fd) { return linux_syscall(SYS_dup, fd, 0, 0, 0, 0, 0); }

static inline long linux_dup3(int oldfd, int newfd, int flags) { return linux_syscall(SYS_dup3, oldfd, newfd, flags, 0, 0, 0); }

static inline long linux_pipe2(int fds[2], int flags) { return linux_syscall(SYS_pipe2, (long)(uintptr_t)fds, flags, 0, 0, 0, 0); }

static inline long linux_mkdirat(int dirfd, const char *path, int mode) { return linux_syscall(SYS_mkdirat, dirfd, (long)(uintptr_t)path, mode, 0, 0, 0); }

static inline long linux_renameat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath) { return linux_syscall(SYS_renameat, olddirfd, (long)(uintptr_t)oldpath, newdirfd, (long)(uintptr_t)newpath, 0, 0); }

static inline long linux_linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags) { return linux_syscall(SYS_linkat, olddirfd, (long)(uintptr_t)oldpath, newdirfd, (long)(uintptr_t)newpath, flags, 0); }

static inline long linux_faccessat(int dirfd, const char *path, int mode, int flags) { return linux_syscall(SYS_faccessat, dirfd, (long)(uintptr_t)path, mode, flags, 0, 0); }

static inline long linux_getcwd(char *buffer, unsigned long size) { return linux_syscall(SYS_getcwd, (long)(uintptr_t)buffer, (long)size, 0, 0, 0, 0); }

static inline long linux_chdir(const char *path) { return linux_syscall(SYS_chdir, (long)(uintptr_t)path, 0, 0, 0, 0, 0); }

static inline long linux_newfstatat(int dirfd, const char *path, void *stat_buffer, int flags) { return linux_syscall(SYS_newfstatat, dirfd, (long)(uintptr_t)path, (long)(uintptr_t)stat_buffer, flags, 0, 0); }

static inline long linux_nanosleep(const struct linux_timespec *request, struct linux_timespec *remaining) { return linux_syscall(SYS_nanosleep, (long)(uintptr_t)request, (long)(uintptr_t)remaining, 0, 0, 0, 0); }

static inline long linux_clock_getres(int clock, struct linux_timespec *result) { return linux_syscall(SYS_clock_getres, clock, (long)(uintptr_t)result, 0, 0, 0, 0); }

static inline long linux_ftruncate(int fd, long length) { return linux_syscall(SYS_ftruncate, fd, length, 0, 0, 0, 0); }
static inline long linux_fsync(int fd) { return linux_syscall(SYS_fsync, fd, 0, 0, 0, 0, 0); }
static inline long linux_readlinkat(int dirfd, const char *path, char *buf, unsigned long bufsiz) { return linux_syscall(SYS_readlinkat, dirfd, (long)(uintptr_t)path, (long)(uintptr_t)buf, (long)bufsiz, 0, 0); }
static inline long linux_symlinkat(const char *target, int newdirfd, const char *linkpath) { return linux_syscall(SYS_symlinkat, (long)(uintptr_t)target, newdirfd, (long)(uintptr_t)linkpath, 0, 0, 0); }
static inline long linux_fcntl(int fd, int cmd, long arg) { return linux_syscall(SYS_fcntl, fd, cmd, arg, 0, 0, 0); }
static inline long linux_ioctl(int fd, unsigned long request, void *arg) { return linux_syscall(SYS_ioctl, fd, (long)request, (long)(uintptr_t)arg, 0, 0, 0); }
static inline long linux_pread64(int fd, void *buf, unsigned long count, long offset) { return linux_syscall(SYS_pread64, fd, (long)(uintptr_t)buf, (long)count, offset, 0, 0); }
static inline long linux_pwrite64(int fd, const void *buf, unsigned long count, long offset) { return linux_syscall(SYS_pwrite64, fd, (long)(uintptr_t)buf, (long)count, offset, 0, 0); }

static inline void linux_exit_group(int status) {
    linux_syscall(SYS_exit_group, status, 0, 0, 0, 0, 0);
    while (1) {
    }
}

#endif
