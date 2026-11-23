#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "syscall.h"

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/random.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

/* Linux RISC-V (asm-generic) syscall numbers. */
enum {
    RV_SYS_getcwd = 17,
    RV_SYS_dup = 23,
    RV_SYS_dup3 = 24,
    RV_SYS_fcntl = 25,
    RV_SYS_ioctl = 29,
    RV_SYS_mkdirat = 34,
    RV_SYS_unlinkat = 35,
    RV_SYS_symlinkat = 36,
    RV_SYS_linkat = 37,
    RV_SYS_renameat = 38,
    RV_SYS_faccessat = 48,
    RV_SYS_chdir = 49,
    RV_SYS_openat = 56,
    RV_SYS_close = 57,
    RV_SYS_pipe2 = 59,
    RV_SYS_getdents64 = 61,
    RV_SYS_lseek = 62,
    RV_SYS_read = 63,
    RV_SYS_write = 64,
    RV_SYS_readv = 65,
    RV_SYS_writev = 66,
    RV_SYS_pread64 = 67,
    RV_SYS_pwrite64 = 68,
    RV_SYS_newfstatat = 79,
    RV_SYS_fstat = 80,
    RV_SYS_exit = 93,
    RV_SYS_exit_group = 94,
    RV_SYS_set_tid_address = 96,
    RV_SYS_futex = 98,
    RV_SYS_set_robust_list = 99,
    RV_SYS_nanosleep = 101,
    RV_SYS_clock_gettime = 113,
    RV_SYS_clock_getres = 114,
    RV_SYS_sched_yield = 124,
    RV_SYS_uname = 160,
    RV_SYS_rt_sigaction = 134,
    RV_SYS_rt_sigprocmask = 135,
    RV_SYS_getrlimit = 163,
    RV_SYS_setrlimit = 164,
    RV_SYS_gettimeofday = 169,
    RV_SYS_getpid = 172,
    RV_SYS_getppid = 173,
    RV_SYS_getuid = 174,
    RV_SYS_geteuid = 175,
    RV_SYS_getgid = 176,
    RV_SYS_getegid = 177,
    RV_SYS_gettid = 178,
    RV_SYS_socket = 198,
    RV_SYS_connect = 203,
    RV_SYS_sendto = 206,
    RV_SYS_recvfrom = 207,
    RV_SYS_setsockopt = 208,
    RV_SYS_getsockopt = 209,
    RV_SYS_shutdown = 210,
    RV_SYS_brk = 214,
    RV_SYS_munmap = 215,
    RV_SYS_clone = 220,
    RV_SYS_execve = 221,
    RV_SYS_mmap = 222,
    RV_SYS_mprotect = 226,
    RV_SYS_madvise = 233,
    RV_SYS_prlimit64 = 261,
    RV_SYS_getrandom = 278,
    RV_SYS_membarrier = 283,
};

enum {
    RV_MAP_PRIVATE = 0x02,
    RV_MAP_FIXED = 0x10,
    RV_MAP_ANONYMOUS = 0x20,
    RV_PAGE_SIZE = 4096,
    RV_MAX_IOV = 1024,
    RV_MAX_SOCKADDR = 128,
    RV_MAX_SOCKET_OPTION = 128,
};

struct rv_timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

struct rv_timeval {
    int64_t tv_sec;
    int64_t tv_usec;
};

struct rv_rlimit {
    uint64_t rlim_cur;
    uint64_t rlim_max;
};

struct rv_iovec {
    uint64_t iov_base;
    uint64_t iov_len;
};

struct rv_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

/* Linux asm-generic struct stat, as used by RISC-V. */
struct rv_stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint32_t st_mode;
    uint32_t st_nlink;
    uint32_t st_uid;
    uint32_t st_gid;
    uint64_t st_rdev;
    uint64_t __pad1;
    int64_t st_size;
    int32_t st_blksize;
    int32_t __pad2;
    int64_t st_blocks;
    int64_t st_atime_sec;
    uint64_t st_atime_nsec;
    int64_t st_mtime_sec;
    uint64_t st_mtime_nsec;
    int64_t st_ctime_sec;
    uint64_t st_ctime_nsec;
    uint32_t __unused4;
    uint32_t __unused5;
};

typedef int (*syscall_fn)(cpu_t *cpu, const uint64_t args[6]);

static int finish(cpu_t *cpu, int64_t value) {
    cpu->regs[10] = (uint64_t)value;
    return 0;
}

static int fail(cpu_t *cpu, int error) { return finish(cpu, -error); }

static int host_result(cpu_t *cpu, long result) {
    if (result < 0) {
        return fail(cpu, errno);
    }
    return finish(cpu, result);
}

static uint8_t *guest_bytes(cpu_t *cpu, uint64_t address, uint64_t length) {
    if (length == 0) {
        return cpu->bus.dram.mem;
    }
    uint64_t offset = address - DRAM_BASE;
    if (offset > DRAM_SIZE || length > DRAM_SIZE - offset) {
        return NULL;
    }
    return cpu->bus.dram.mem + offset;
}

static const char *guest_string(cpu_t *cpu, uint64_t address) {
    uint64_t offset = address - DRAM_BASE;
    if (offset >= DRAM_SIZE) {
        return NULL;
    }
    char *string = (char *)cpu->bus.dram.mem + offset;
    if (memchr(string, 0, DRAM_SIZE - offset) == NULL) {
        return NULL;
    }
    return string;
}

static int guest_read(cpu_t *cpu, uint64_t address, void *destination, uint64_t length) {
    uint8_t *source = guest_bytes(cpu, address, length);
    if (source == NULL) {
        return -1;
    }
    memcpy(destination, source, length);
    return 0;
}

static int guest_write(cpu_t *cpu, uint64_t address, const void *source, uint64_t length) {
    uint8_t *destination = guest_bytes(cpu, address, length);
    if (destination == NULL) {
        return -1;
    }
    memcpy(destination, source, length);
    return 0;
}

static uint64_t align_up(uint64_t value, uint64_t alignment) { return (value + alignment - 1) & ~(alignment - 1); }

static long retry_read(int fd, void *buffer, size_t length) {
    long result;
    do {
        result = (long)read(fd, buffer, length);
    } while (result < 0 && errno == EINTR);
    return result;
}

static long retry_write(int fd, const void *buffer, size_t length) {
    long result;
    do {
        result = (long)write(fd, buffer, length);
    } while (result < 0 && errno == EINTR);
    return result;
}

static void stat_from_host(struct rv_stat *destination, const struct stat *source) {
    memset(destination, 0, sizeof(*destination));
    destination->st_dev = (uint64_t)source->st_dev;
    destination->st_ino = (uint64_t)source->st_ino;
    destination->st_mode = (uint32_t)source->st_mode;
    destination->st_nlink = (uint32_t)source->st_nlink;
    destination->st_uid = (uint32_t)source->st_uid;
    destination->st_gid = (uint32_t)source->st_gid;
    destination->st_rdev = (uint64_t)source->st_rdev;
    destination->st_size = (int64_t)source->st_size;
    destination->st_blksize = (int32_t)source->st_blksize;
    destination->st_blocks = (int64_t)source->st_blocks;
    destination->st_atime_sec = (int64_t)source->st_atim.tv_sec;
    destination->st_atime_nsec = (uint64_t)source->st_atim.tv_nsec;
    destination->st_mtime_sec = (int64_t)source->st_mtim.tv_sec;
    destination->st_mtime_nsec = (uint64_t)source->st_mtim.tv_nsec;
    destination->st_ctime_sec = (int64_t)source->st_ctim.tv_sec;
    destination->st_ctime_nsec = (uint64_t)source->st_ctim.tv_nsec;
}

static void timespec_from_host(struct rv_timespec *destination, const struct timespec *source) {
    destination->tv_sec = (int64_t)source->tv_sec;
    destination->tv_nsec = (int64_t)source->tv_nsec;
}

static int sys_read(cpu_t *cpu, const uint64_t args[6]) {
    uint8_t *buffer = guest_bytes(cpu, args[1], args[2]);
    if (buffer == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, retry_read((int)args[0], buffer, (size_t)args[2]));
}

static int sys_write(cpu_t *cpu, const uint64_t args[6]) {
    uint8_t *buffer = guest_bytes(cpu, args[1], args[2]);
    if (buffer == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, retry_write((int)args[0], buffer, (size_t)args[2]));
}

static int sys_readv(cpu_t *cpu, const uint64_t args[6]) {
    if (args[2] > RV_MAX_IOV) {
        return fail(cpu, EINVAL);
    }
    if (args[2] == 0) {
        return finish(cpu, 0);
    }

    struct rv_iovec *guest_iov = (struct rv_iovec *)guest_bytes(cpu, args[1], args[2] * sizeof(struct rv_iovec));
    struct iovec *host_iov = malloc((size_t)args[2] * sizeof(struct iovec));
    if (guest_iov == NULL || host_iov == NULL) {
        free(host_iov);
        return fail(cpu, guest_iov == NULL ? EFAULT : ENOMEM);
    }

    for (uint64_t i = 0; i < args[2]; i++) {
        uint8_t *buffer = guest_bytes(cpu, guest_iov[i].iov_base, guest_iov[i].iov_len);
        if (buffer == NULL) {
            free(host_iov);
            return fail(cpu, EFAULT);
        }
        host_iov[i].iov_base = buffer;
        host_iov[i].iov_len = (size_t)guest_iov[i].iov_len;
    }

    long result = (long)readv((int)args[0], host_iov, (int)args[2]);
    free(host_iov);
    return host_result(cpu, result);
}

static int sys_writev(cpu_t *cpu, const uint64_t args[6]) {
    if (args[2] > RV_MAX_IOV) {
        return fail(cpu, EINVAL);
    }
    if (args[2] == 0) {
        return finish(cpu, 0);
    }

    struct rv_iovec *guest_iov = (struct rv_iovec *)guest_bytes(cpu, args[1], args[2] * sizeof(struct rv_iovec));
    struct iovec *host_iov = malloc((size_t)args[2] * sizeof(struct iovec));
    if (guest_iov == NULL || host_iov == NULL) {
        free(host_iov);
        return fail(cpu, guest_iov == NULL ? EFAULT : ENOMEM);
    }

    for (uint64_t i = 0; i < args[2]; i++) {
        uint8_t *buffer = guest_bytes(cpu, guest_iov[i].iov_base, guest_iov[i].iov_len);
        if (buffer == NULL) {
            free(host_iov);
            return fail(cpu, EFAULT);
        }
        host_iov[i].iov_base = buffer;
        host_iov[i].iov_len = (size_t)guest_iov[i].iov_len;
    }

    long result = (long)writev((int)args[0], host_iov, (int)args[2]);
    free(host_iov);
    return host_result(cpu, result);
}

static int sys_pread64(cpu_t *cpu, const uint64_t args[6]) {
    uint8_t *buffer = guest_bytes(cpu, args[1], args[2]);
    if (buffer == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, (long)pread((int)args[0], buffer, (size_t)args[2], (off_t)args[3]));
}

static int sys_pwrite64(cpu_t *cpu, const uint64_t args[6]) {
    uint8_t *buffer = guest_bytes(cpu, args[1], args[2]);
    if (buffer == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, (long)pwrite((int)args[0], buffer, (size_t)args[2], (off_t)args[3]));
}

static int sys_openat(cpu_t *cpu, const uint64_t args[6]) {
    const char *path = guest_string(cpu, args[1]);
    if (path == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, openat((int)args[0], path, (int)args[2], (mode_t)args[3]));
}

static int sys_close(cpu_t *cpu, const uint64_t args[6]) { return host_result(cpu, close((int)args[0])); }

static int sys_lseek(cpu_t *cpu, const uint64_t args[6]) { return host_result(cpu, (long)lseek((int)args[0], (off_t)args[1], (int)args[2])); }

static int sys_dup(cpu_t *cpu, const uint64_t args[6]) { return host_result(cpu, dup((int)args[0])); }

static int sys_dup3(cpu_t *cpu, const uint64_t args[6]) { return host_result(cpu, dup3((int)args[0], (int)args[1], (int)args[2])); }

static int sys_fcntl(cpu_t *cpu, const uint64_t args[6]) {
    int command = (int)args[1];
    switch (command) {
    case F_DUPFD:
    case F_DUPFD_CLOEXEC:
        return host_result(cpu, fcntl((int)args[0], command, (int)args[2]));
    case F_SETFD:
    case F_SETFL:
    case F_SETOWN:
        return host_result(cpu, fcntl((int)args[0], command, (int)args[2]));
    case F_GETFD:
    case F_GETFL:
    case F_GETOWN:
        return host_result(cpu, fcntl((int)args[0], command));
    default:
        return fail(cpu, ENOSYS);
    }
}

static int sys_fstat(cpu_t *cpu, const uint64_t args[6]) {
    struct stat host_stat;
    if (fstat((int)args[0], &host_stat) < 0) {
        return fail(cpu, errno);
    }
    struct rv_stat guest_stat;
    stat_from_host(&guest_stat, &host_stat);
    if (guest_write(cpu, args[1], &guest_stat, sizeof(guest_stat)) < 0) {
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_newfstatat(cpu_t *cpu, const uint64_t args[6]) {
    const char *path = guest_string(cpu, args[1]);
    if (path == NULL) {
        return fail(cpu, EFAULT);
    }
    struct stat host_stat;
    if (fstatat((int)args[0], path, &host_stat, (int)args[3]) < 0) {
        return fail(cpu, errno);
    }
    struct rv_stat guest_stat;
    stat_from_host(&guest_stat, &host_stat);
    if (guest_write(cpu, args[2], &guest_stat, sizeof(guest_stat)) < 0) {
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_getcwd(cpu_t *cpu, const uint64_t args[6]) {
    char *cwd = getcwd(NULL, 0);
    if (cwd == NULL) {
        return fail(cpu, errno);
    }
    size_t length = strlen(cwd) + 1;
    if (length > args[1] || guest_write(cpu, args[0], cwd, length) < 0) {
        free(cwd);
        return fail(cpu, length > args[1] ? ERANGE : EFAULT);
    }
    free(cwd);
    return finish(cpu, (int64_t)length);
}

static int sys_chdir(cpu_t *cpu, const uint64_t args[6]) {
    const char *path = guest_string(cpu, args[0]);
    if (path == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, chdir(path));
}

static int sys_mkdirat(cpu_t *cpu, const uint64_t args[6]) {
    const char *path = guest_string(cpu, args[1]);
    if (path == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, mkdirat((int)args[0], path, (mode_t)args[2]));
}

static int sys_unlinkat(cpu_t *cpu, const uint64_t args[6]) {
    const char *path = guest_string(cpu, args[1]);
    if (path == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, unlinkat((int)args[0], path, (int)args[2]));
}

static int sys_symlinkat(cpu_t *cpu, const uint64_t args[6]) {
    const char *target = guest_string(cpu, args[0]);
    const char *linkpath = guest_string(cpu, args[2]);
    if (target == NULL || linkpath == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, symlinkat(target, (int)args[1], linkpath));
}

static int sys_linkat(cpu_t *cpu, const uint64_t args[6]) {
    const char *oldpath = guest_string(cpu, args[1]);
    const char *newpath = guest_string(cpu, args[3]);
    if (oldpath == NULL || newpath == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, linkat((int)args[0], oldpath, (int)args[2], newpath, (int)args[4]));
}

static int sys_renameat(cpu_t *cpu, const uint64_t args[6]) {
    const char *oldpath = guest_string(cpu, args[1]);
    const char *newpath = guest_string(cpu, args[3]);
    if (oldpath == NULL || newpath == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, renameat((int)args[0], oldpath, (int)args[2], newpath));
}

static int sys_faccessat(cpu_t *cpu, const uint64_t args[6]) {
    const char *path = guest_string(cpu, args[1]);
    if (path == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, faccessat((int)args[0], path, (int)args[2], (int)args[3]));
}

static int sys_pipe2(cpu_t *cpu, const uint64_t args[6]) {
    int host_fds[2];
    if (pipe2(host_fds, (int)args[1]) < 0) {
        return fail(cpu, errno);
    }
    int32_t guest_fds[2] = {host_fds[0], host_fds[1]};
    if (guest_write(cpu, args[0], guest_fds, sizeof(guest_fds)) < 0) {
        close(host_fds[0]);
        close(host_fds[1]);
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_getdents64(cpu_t *cpu, const uint64_t args[6]) {
    uint8_t *buffer = guest_bytes(cpu, args[1], args[2]);
    if (buffer == NULL) {
        return fail(cpu, EFAULT);
    }
#ifdef SYS_getdents64
    long result = syscall(SYS_getdents64, (int)args[0], buffer, (size_t)args[2]);
    return host_result(cpu, result);
#else
    return fail(cpu, ENOSYS);
#endif
}

static int sys_ioctl(cpu_t *cpu, const uint64_t args[6]) {
    unsigned long request = (unsigned long)args[1];
    if (request == TIOCGWINSZ) {
        struct winsize window;
        if (ioctl((int)args[0], request, &window) < 0) {
            return fail(cpu, errno);
        }
        if (guest_write(cpu, args[2], &window, sizeof(window)) < 0) {
            return fail(cpu, EFAULT);
        }
        return finish(cpu, 0);
    }
    if (request == FIONREAD) {
        int available;
        if (ioctl((int)args[0], request, &available) < 0) {
            return fail(cpu, errno);
        }
        if (guest_write(cpu, args[2], &available, sizeof(available)) < 0) {
            return fail(cpu, EFAULT);
        }
        return finish(cpu, 0);
    }
    return fail(cpu, ENOSYS);
}

static int sys_clock_gettime(cpu_t *cpu, const uint64_t args[6]) {
    struct timespec host_time;
    if (clock_gettime((clockid_t)args[0], &host_time) < 0) {
        return fail(cpu, errno);
    }
    struct rv_timespec guest_time;
    timespec_from_host(&guest_time, &host_time);
    if (guest_write(cpu, args[1], &guest_time, sizeof(guest_time)) < 0) {
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_clock_getres(cpu_t *cpu, const uint64_t args[6]) {
    struct timespec host_time;
    if (clock_getres((clockid_t)args[0], &host_time) < 0) {
        return fail(cpu, errno);
    }
    if (args[1] != 0) {
        struct rv_timespec guest_time;
        timespec_from_host(&guest_time, &host_time);
        if (guest_write(cpu, args[1], &guest_time, sizeof(guest_time)) < 0) {
            return fail(cpu, EFAULT);
        }
    }
    return finish(cpu, 0);
}

static int sys_gettimeofday(cpu_t *cpu, const uint64_t args[6]) {
    struct timeval host_time;
    if (gettimeofday(&host_time, NULL) < 0) {
        return fail(cpu, errno);
    }
    if (args[0] != 0) {
        struct rv_timeval guest_time = {(int64_t)host_time.tv_sec, (int64_t)host_time.tv_usec};
        if (guest_write(cpu, args[0], &guest_time, sizeof(guest_time)) < 0) {
            return fail(cpu, EFAULT);
        }
    }
    return finish(cpu, 0);
}

static int sys_nanosleep(cpu_t *cpu, const uint64_t args[6]) {
    struct rv_timespec guest_request;
    if (guest_read(cpu, args[0], &guest_request, sizeof(guest_request)) < 0) {
        return fail(cpu, EFAULT);
    }
    struct timespec host_request = {(time_t)guest_request.tv_sec, (long)guest_request.tv_nsec};
    struct timespec host_remaining;
    if (nanosleep(&host_request, &host_remaining) < 0) {
        if (errno == EINTR && args[1] != 0) {
            struct rv_timespec guest_remaining;
            timespec_from_host(&guest_remaining, &host_remaining);
            if (guest_write(cpu, args[1], &guest_remaining, sizeof(guest_remaining)) < 0) {
                return fail(cpu, EFAULT);
            }
        }
        return fail(cpu, errno);
    }
    return finish(cpu, 0);
}

static int sys_getrlimit(cpu_t *cpu, const uint64_t args[6]) {
    struct rlimit host_limit;
    if (getrlimit((int)args[0], &host_limit) < 0) {
        return fail(cpu, errno);
    }
    struct rv_rlimit guest_limit = {(uint64_t)host_limit.rlim_cur, (uint64_t)host_limit.rlim_max};
    if (guest_write(cpu, args[1], &guest_limit, sizeof(guest_limit)) < 0) {
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_setrlimit(cpu_t *cpu, const uint64_t args[6]) {
    struct rv_rlimit guest_limit;
    if (guest_read(cpu, args[1], &guest_limit, sizeof(guest_limit)) < 0) {
        return fail(cpu, EFAULT);
    }
    struct rlimit host_limit = {(rlim_t)guest_limit.rlim_cur, (rlim_t)guest_limit.rlim_max};
    return host_result(cpu, setrlimit((int)args[0], &host_limit));
}

static int sys_prlimit64(cpu_t *cpu, const uint64_t args[6]) {
    struct rlimit host_new;
    struct rlimit host_old;
    struct rlimit *new_limit = NULL;
    struct rlimit *old_limit = NULL;

    if (args[0] != 0) {
        return fail(cpu, ENOSYS);
    }
    if (args[2] != 0) {
        struct rv_rlimit guest_limit;
        if (guest_read(cpu, args[2], &guest_limit, sizeof(guest_limit)) < 0) {
            return fail(cpu, EFAULT);
        }
        host_new.rlim_cur = (rlim_t)guest_limit.rlim_cur;
        host_new.rlim_max = (rlim_t)guest_limit.rlim_max;
        new_limit = &host_new;
    }
    if (args[3] != 0) {
        old_limit = &host_old;
    }
    if (prlimit(0, (int)args[1], new_limit, old_limit) < 0) {
        return fail(cpu, errno);
    }
    if (old_limit != NULL) {
        struct rv_rlimit guest_limit = {(uint64_t)host_old.rlim_cur, (uint64_t)host_old.rlim_max};
        if (guest_write(cpu, args[3], &guest_limit, sizeof(guest_limit)) < 0) {
            return fail(cpu, EFAULT);
        }
    }
    return finish(cpu, 0);
}

static int sys_brk(cpu_t *cpu, const uint64_t args[6]) {
    uint64_t requested = args[0];
    if (requested == 0) {
        return finish(cpu, (int64_t)cpu->sys.brk);
    }
    if (requested < cpu->sys.brk_base) {
        return finish(cpu, (int64_t)cpu->sys.brk);
    }
    if (requested > cpu->sys.mmap_top) {
        return fail(cpu, ENOMEM);
    }
    if (requested > cpu->sys.brk) {
        uint8_t *start = guest_bytes(cpu, cpu->sys.brk, requested - cpu->sys.brk);
        if (start == NULL) {
            return fail(cpu, ENOMEM);
        }
        memset(start, 0, requested - cpu->sys.brk);
    }
    cpu->sys.brk = requested;
    return finish(cpu, (int64_t)cpu->sys.brk);
}

static int sys_mmap(cpu_t *cpu, const uint64_t args[6]) {
    uint64_t length = align_up(args[1], RV_PAGE_SIZE);
    if (length == 0) {
        return fail(cpu, EINVAL);
    }
    if ((args[3] & RV_MAP_ANONYMOUS) == 0) {
        return fail(cpu, ENODEV);
    }
    if ((args[3] & RV_MAP_FIXED) != 0) {
        return fail(cpu, ENOSYS);
    }
    if (length > cpu->sys.mmap_top - cpu->sys.brk) {
        return fail(cpu, ENOMEM);
    }
    cpu->sys.mmap_top -= length;
    uint8_t *memory = guest_bytes(cpu, cpu->sys.mmap_top, length);
    if (memory == NULL) {
        cpu->sys.mmap_top += length;
        return fail(cpu, ENOMEM);
    }
    memset(memory, 0, length);
    return finish(cpu, (int64_t)cpu->sys.mmap_top);
}

static int sys_munmap(cpu_t *cpu, const uint64_t args[6]) {
    if (guest_bytes(cpu, args[0], args[1]) == NULL) {
        return fail(cpu, EINVAL);
    }
    return finish(cpu, 0);
}

static int sys_mprotect(cpu_t *cpu, const uint64_t args[6]) {
    if (guest_bytes(cpu, args[0], args[1]) == NULL) {
        return fail(cpu, EINVAL);
    }
    return finish(cpu, 0);
}

static int sys_madvise(cpu_t *cpu, const uint64_t args[6]) {
    if (guest_bytes(cpu, args[0], args[1]) == NULL) {
        return fail(cpu, EINVAL);
    }
    return finish(cpu, 0);
}

static int sys_uname(cpu_t *cpu, const uint64_t args[6]) {
    struct utsname host_name;
    if (uname(&host_name) < 0) {
        return fail(cpu, errno);
    }
    struct rv_utsname guest_name;
    memset(&guest_name, 0, sizeof(guest_name));
    strncpy(guest_name.sysname, host_name.sysname, sizeof(guest_name.sysname) - 1);
    strncpy(guest_name.nodename, host_name.nodename, sizeof(guest_name.nodename) - 1);
    strncpy(guest_name.release, host_name.release, sizeof(guest_name.release) - 1);
    strncpy(guest_name.version, host_name.version, sizeof(guest_name.version) - 1);
    strncpy(guest_name.machine, host_name.machine, sizeof(guest_name.machine) - 1);
#ifdef _GNU_SOURCE
    strncpy(guest_name.domainname, host_name.domainname, sizeof(guest_name.domainname) - 1);
#endif
    if (guest_write(cpu, args[0], &guest_name, sizeof(guest_name)) < 0) {
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_getrandom(cpu_t *cpu, const uint64_t args[6]) {
    uint8_t *buffer = guest_bytes(cpu, args[0], args[1]);
    if (buffer == NULL) {
        return fail(cpu, EFAULT);
    }
    long result;
    do {
        result = (long)getrandom(buffer, (size_t)args[1], (unsigned int)args[2]);
    } while (result < 0 && errno == EINTR);
    return host_result(cpu, result);
}

static int sys_socket(cpu_t *cpu, const uint64_t args[6]) {
    return host_result(cpu, socket((int)args[0], (int)args[1], (int)args[2]));
}

static int sys_connect(cpu_t *cpu, const uint64_t args[6]) {
    uint8_t address[RV_MAX_SOCKADDR];
    uint64_t length = args[2] > RV_MAX_SOCKADDR ? RV_MAX_SOCKADDR : args[2];

    if (args[1] == 0) {
        return fail(cpu, EFAULT);
    }
    if (guest_read(cpu, args[1], address, length) < 0) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, connect((int)args[0], (const struct sockaddr *)address, (socklen_t)length));
}

static int sys_sendto(cpu_t *cpu, const uint64_t args[6]) {
    uint8_t *buffer = guest_bytes(cpu, args[1], args[2]);
    uint8_t address[RV_MAX_SOCKADDR];
    const struct sockaddr *destination = NULL;
    uint64_t length = 0;

    if (buffer == NULL) {
        return fail(cpu, EFAULT);
    }
    if (args[4] != 0) {
        length = args[5] > RV_MAX_SOCKADDR ? RV_MAX_SOCKADDR : args[5];
        if (guest_read(cpu, args[4], address, length) < 0) {
            return fail(cpu, EFAULT);
        }
        destination = (const struct sockaddr *)address;
    }
    return host_result(cpu, sendto((int)args[0], buffer, (size_t)args[2], (int)args[3], destination, (socklen_t)length));
}

static int sys_recvfrom(cpu_t *cpu, const uint64_t args[6]) {
    uint8_t *buffer = guest_bytes(cpu, args[1], args[2]);
    uint8_t address[RV_MAX_SOCKADDR];
    socklen_t address_length;
    long result;

    if (buffer == NULL) {
        return fail(cpu, EFAULT);
    }
    address_length = args[5] > RV_MAX_SOCKADDR ? (socklen_t)RV_MAX_SOCKADDR : (socklen_t)args[5];
    result = (long)recvfrom((int)args[0], buffer, (size_t)args[2], (int)args[3], (struct sockaddr *)address, &address_length);
    if (result >= 0 && args[4] != 0 && address_length > 0) {
        if (guest_write(cpu, args[4], address, address_length) < 0) {
            return fail(cpu, EFAULT);
        }
    }
    return host_result(cpu, result);
}

static int sys_setsockopt(cpu_t *cpu, const uint64_t args[6]) {
    uint8_t value[RV_MAX_SOCKET_OPTION];
    uint64_t length = args[4] > RV_MAX_SOCKET_OPTION ? RV_MAX_SOCKET_OPTION : args[4];

    if (length > 0) {
        if (args[3] == 0 || guest_read(cpu, args[3], value, length) < 0) {
            return fail(cpu, EFAULT);
        }
    }
    return host_result(cpu, setsockopt((int)args[0], (int)args[1], (int)args[2], value, (socklen_t)length));
}

static int sys_getsockopt(cpu_t *cpu, const uint64_t args[6]) {
    uint8_t value[RV_MAX_SOCKET_OPTION];
    int32_t guest_length;
    socklen_t length;

    if (args[3] == 0 || args[4] == 0) {
        return fail(cpu, EFAULT);
    }
    if (guest_read(cpu, args[4], &guest_length, sizeof(guest_length)) < 0) {
        return fail(cpu, EFAULT);
    }
    if (guest_length < 0) {
        return fail(cpu, EINVAL);
    }
    length = (socklen_t)guest_length;
    if (length > RV_MAX_SOCKET_OPTION) {
        length = RV_MAX_SOCKET_OPTION;
    }
    if (getsockopt((int)args[0], (int)args[1], (int)args[2], value, &length) < 0) {
        return fail(cpu, errno);
    }
    if (length > 0 && guest_write(cpu, args[3], value, length) < 0) {
        return fail(cpu, EFAULT);
    }
    guest_length = (int32_t)length;
    if (guest_write(cpu, args[4], &guest_length, sizeof(guest_length)) < 0) {
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_shutdown(cpu_t *cpu, const uint64_t args[6]) { return host_result(cpu, shutdown((int)args[0], (int)args[1])); }

static int sys_set_tid_address(cpu_t *cpu, const uint64_t args[6]) {
    (void)args;
#ifdef SYS_gettid
    return finish(cpu, (int64_t)syscall(SYS_gettid));
#else
    return finish(cpu, (int64_t)getpid());
#endif
}

static int sys_exit(cpu_t *cpu, const uint64_t args[6]) {
    exit((int)args[0]);
    (void)cpu;
    return 0;
}

/* clang-format off */
static const struct {
    uint64_t number;
    syscall_fn function;
} syscall_table[] = {
    {RV_SYS_getcwd, sys_getcwd},
    {RV_SYS_dup, sys_dup},
    {RV_SYS_dup3, sys_dup3},
    {RV_SYS_fcntl, sys_fcntl},
    {RV_SYS_ioctl, sys_ioctl},
    {RV_SYS_mkdirat, sys_mkdirat},
    {RV_SYS_unlinkat, sys_unlinkat},
    {RV_SYS_symlinkat, sys_symlinkat},
    {RV_SYS_linkat, sys_linkat},
    {RV_SYS_renameat, sys_renameat},
    {RV_SYS_faccessat, sys_faccessat},
    {RV_SYS_chdir, sys_chdir},
    {RV_SYS_openat, sys_openat},
    {RV_SYS_close, sys_close},
    {RV_SYS_pipe2, sys_pipe2},
    {RV_SYS_getdents64, sys_getdents64},
    {RV_SYS_lseek, sys_lseek},
    {RV_SYS_read, sys_read},
    {RV_SYS_write, sys_write},
    {RV_SYS_readv, sys_readv},
    {RV_SYS_writev, sys_writev},
    {RV_SYS_pread64, sys_pread64},
    {RV_SYS_pwrite64, sys_pwrite64},
    {RV_SYS_newfstatat, sys_newfstatat},
    {RV_SYS_fstat, sys_fstat},
    {RV_SYS_set_tid_address, sys_set_tid_address},
    {RV_SYS_nanosleep, sys_nanosleep},
    {RV_SYS_clock_gettime, sys_clock_gettime},
    {RV_SYS_clock_getres, sys_clock_getres},
    {RV_SYS_getrlimit, sys_getrlimit},
    {RV_SYS_setrlimit, sys_setrlimit},
    {RV_SYS_gettimeofday, sys_gettimeofday},
    {RV_SYS_uname, sys_uname},
    {RV_SYS_brk, sys_brk},
    {RV_SYS_munmap, sys_munmap},
    {RV_SYS_mmap, sys_mmap},
    {RV_SYS_mprotect, sys_mprotect},
    {RV_SYS_madvise, sys_madvise},
    {RV_SYS_prlimit64, sys_prlimit64},
    {RV_SYS_getrandom, sys_getrandom},
    {RV_SYS_socket, sys_socket},
    {RV_SYS_connect, sys_connect},
    {RV_SYS_sendto, sys_sendto},
    {RV_SYS_recvfrom, sys_recvfrom},
    {RV_SYS_setsockopt, sys_setsockopt},
    {RV_SYS_getsockopt, sys_getsockopt},
    {RV_SYS_shutdown, sys_shutdown},
};
/* clang-format on */

void syscall_init(cpu_t *cpu, uint64_t image_size) {
    memset(&cpu->sys, 0, sizeof(cpu->sys));

    uint64_t base = align_up(DRAM_BASE + image_size, RV_PAGE_SIZE);
    if (base < DRAM_BASE + DRAM_SIZE / 2) {
        base = DRAM_BASE + DRAM_SIZE / 2;
    }

    cpu->sys.brk_base = base;
    cpu->sys.brk = base;
    cpu->sys.mmap_top = DRAM_BASE + DRAM_SIZE - 64 * 1024;
    if (cpu->sys.mmap_top < cpu->sys.brk) {
        cpu->sys.mmap_top = cpu->sys.brk;
    }
    cpu->sys.trace = getenv("RVEMU_TRACE_SYSCALLS") != NULL;
}

int syscall_dispatch(cpu_t *cpu) {
    uint64_t number = cpu->regs[17];
    uint64_t args[6] = {cpu->regs[10], cpu->regs[11], cpu->regs[12], cpu->regs[13], cpu->regs[14], cpu->regs[15]};

    if (cpu->sys.trace) {
        fprintf(stderr, "rvemu: syscall %lu(%#lx, %#lx, %#lx, %#lx)\n", (unsigned long)number, (unsigned long)args[0], (unsigned long)args[1], (unsigned long)args[2],
                (unsigned long)args[3]);
    }

    switch (number) {
    case RV_SYS_exit:
    case RV_SYS_exit_group:
        return sys_exit(cpu, args);
    case RV_SYS_sched_yield:
        return host_result(cpu, sched_yield());
    case RV_SYS_getpid:
        return finish(cpu, getpid());
    case RV_SYS_getppid:
        return finish(cpu, getppid());
    case RV_SYS_getuid:
        return finish(cpu, getuid());
    case RV_SYS_geteuid:
        return finish(cpu, geteuid());
    case RV_SYS_getgid:
        return finish(cpu, getgid());
    case RV_SYS_getegid:
        return finish(cpu, getegid());
    case RV_SYS_gettid:
#ifdef SYS_gettid
        return finish(cpu, (int64_t)syscall(SYS_gettid));
#else
        return finish(cpu, getpid());
#endif
    case RV_SYS_rt_sigaction:
    case RV_SYS_rt_sigprocmask:
    case RV_SYS_set_robust_list:
    case RV_SYS_membarrier:
        return finish(cpu, 0);
    case RV_SYS_futex:
    case RV_SYS_clone:
    case RV_SYS_execve:
        return fail(cpu, ENOSYS);
    }

    for (size_t i = 0; i < sizeof(syscall_table) / sizeof(syscall_table[0]); i++) {
        if (syscall_table[i].number == number) {
            int result = syscall_table[i].function(cpu, args);
            if (cpu->sys.trace) {
                fprintf(stderr, "rvemu: syscall %lu -> %ld\n", (unsigned long)number, (long)(int64_t)cpu->regs[10]);
            }
            return result;
        }
    }

    return fail(cpu, ENOSYS);
}
