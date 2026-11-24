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

static inline long linux_recvfrom(int fd, void *buffer, unsigned long length, int flags, void *source, long source_length) {
    return linux_syscall(SYS_recvfrom, fd, (long)(uintptr_t)buffer, (long)length, flags, (long)(uintptr_t)source, source_length);
}

static inline long linux_setsockopt(int fd, int level, int option, const void *value, long length) {
    return linux_syscall(SYS_setsockopt, fd, level, option, (long)(uintptr_t)value, length, 0);
}

static inline long linux_getsockopt(int fd, int level, int option, void *value, void *length) {
    return linux_syscall(SYS_getsockopt, fd, level, option, (long)(uintptr_t)value, (long)(uintptr_t)length, 0);
}

static inline long linux_shutdown(int fd, int how) { return linux_syscall(SYS_shutdown, fd, how, 0, 0, 0, 0); }

static inline void linux_exit_group(int status) {
    linux_syscall(SYS_exit_group, status, 0, 0, 0, 0, 0);
    while (1) {
    }
}

#endif
