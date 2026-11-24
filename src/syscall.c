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
#include <netinet/in.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

enum {
    RV_MAP_PRIVATE = 0x02,
    RV_MAP_FIXED = 0x10,
    RV_MAP_ANONYMOUS = 0x20,
    RV_PAGE_SIZE = 4096,
    RV_MAX_IOV = 1024,
    RV_MAX_SOCKADDR = 128,
    RV_MAX_SOCKET_OPTION = 128,
    RV_DIRENT64_HEADER = 19, /* kernel linux_dirent64 header: d_ino 8 + d_off 8 + d_reclen 2 + d_name[0] 1 */
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

/* Linux kernel struct linux_dirent64 layout (identical on every Linux arch);
 * d_name is open-ended, entries are stepped by d_reclen. */
struct rv_dirent64 {
    uint64_t d_ino;
    uint64_t d_off;
    uint16_t d_reclen;
    char d_name[1];
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

static int finish(cpu_t *cpu, int64_t value) {
    cpu->regs[10] = (uint64_t)value;
    return 0;
}

/* A few host ABIs number errnos differently -- in the host matrix, MIPS is
 * the concrete counterexample (ENOSYS is 89 there versus the guest's 38,
 * ENOTSOCK 95 versus 88, EADDRINUSE 125 versus 98, ...).  Error values that
 * reach the guest must therefore be the asm-generic (RISC-V) numbers, not the
 * host's: map every host errno symbol to the guest value it corresponds to,
 * guarded with #ifdef so the table survives host ABIs that rename or drop a
 * symbol.  Host values with no guest counterpart pass through unchanged. */
static int guest_errno(int host_errno) {
#ifdef EPERM
    if (host_errno == EPERM)
        return 1;
#endif
#ifdef ENOENT
    if (host_errno == ENOENT)
        return 2;
#endif
#ifdef ESRCH
    if (host_errno == ESRCH)
        return 3;
#endif
#ifdef EINTR
    if (host_errno == EINTR)
        return 4;
#endif
#ifdef EIO
    if (host_errno == EIO)
        return 5;
#endif
#ifdef ENXIO
    if (host_errno == ENXIO)
        return 6;
#endif
#ifdef E2BIG
    if (host_errno == E2BIG)
        return 7;
#endif
#ifdef ENOEXEC
    if (host_errno == ENOEXEC)
        return 8;
#endif
#ifdef EBADF
    if (host_errno == EBADF)
        return 9;
#endif
#ifdef ECHILD
    if (host_errno == ECHILD)
        return 10;
#endif
#ifdef EAGAIN
    if (host_errno == EAGAIN)
        return 11;
#endif
#ifdef ENOMEM
    if (host_errno == ENOMEM)
        return 12;
#endif
#ifdef EACCES
    if (host_errno == EACCES)
        return 13;
#endif
#ifdef EFAULT
    if (host_errno == EFAULT)
        return 14;
#endif
#ifdef ENOTBLK
    if (host_errno == ENOTBLK)
        return 15;
#endif
#ifdef EBUSY
    if (host_errno == EBUSY)
        return 16;
#endif
#ifdef EEXIST
    if (host_errno == EEXIST)
        return 17;
#endif
#ifdef EXDEV
    if (host_errno == EXDEV)
        return 18;
#endif
#ifdef ENODEV
    if (host_errno == ENODEV)
        return 19;
#endif
#ifdef ENOTDIR
    if (host_errno == ENOTDIR)
        return 20;
#endif
#ifdef EISDIR
    if (host_errno == EISDIR)
        return 21;
#endif
#ifdef EINVAL
    if (host_errno == EINVAL)
        return 22;
#endif
#ifdef ENFILE
    if (host_errno == ENFILE)
        return 23;
#endif
#ifdef EMFILE
    if (host_errno == EMFILE)
        return 24;
#endif
#ifdef ENOSR
    if (host_errno == ENOSR)
        return 63;
#endif
#ifdef ENOSPC
    if (host_errno == ENOSPC)
        return 28;
#endif
#ifdef ESPIPE
    if (host_errno == ESPIPE)
        return 29;
#endif
#ifdef EROFS
    if (host_errno == EROFS)
        return 30;
#endif
#ifdef EMLINK
    if (host_errno == EMLINK)
        return 31;
#endif
#ifdef EPIPE
    if (host_errno == EPIPE)
        return 32;
#endif
#ifdef EDOM
    if (host_errno == EDOM)
        return 33;
#endif
#ifdef ERANGE
    if (host_errno == ERANGE)
        return 34;
#endif
#ifdef EDEADLK
    if (host_errno == EDEADLK)
        return 35;
#endif
#ifdef ENAMETOOLONG
    if (host_errno == ENAMETOOLONG)
        return 36;
#endif
#ifdef ENOLCK
    if (host_errno == ENOLCK)
        return 37;
#endif
#ifdef ENOSYS
    if (host_errno == ENOSYS)
        return 38;
#endif
#ifdef ENOTEMPTY
    if (host_errno == ENOTEMPTY)
        return 39;
#endif
#ifdef ELOOP
    if (host_errno == ELOOP)
        return 40;
#endif
#ifdef ENOMSG
    if (host_errno == ENOMSG)
        return 42;
#endif
#ifdef ENODATA
    if (host_errno == ENODATA)
        return 61;
#endif
#ifdef ENOTSOCK
    if (host_errno == ENOTSOCK)
        return 88;
#endif
#ifdef EDESTADDRREQ
    if (host_errno == EDESTADDRREQ)
        return 89;
#endif
#ifdef EMSGSIZE
    if (host_errno == EMSGSIZE)
        return 90;
#endif
#ifdef EPROTOTYPE
    if (host_errno == EPROTOTYPE)
        return 91;
#endif
#ifdef ENOPROTOOPT
    if (host_errno == ENOPROTOOPT)
        return 92;
#endif
#ifdef EPROTONOSUPPORT
    if (host_errno == EPROTONOSUPPORT)
        return 93;
#endif
#ifdef EOPNOTSUPP
    if (host_errno == EOPNOTSUPP)
        return 95;
#endif
#ifdef EAFNOSUPPORT
    if (host_errno == EAFNOSUPPORT)
        return 97;
#endif
#ifdef EADDRINUSE
    if (host_errno == EADDRINUSE)
        return 98;
#endif
#ifdef EADDRNOTAVAIL
    if (host_errno == EADDRNOTAVAIL)
        return 99;
#endif
#ifdef ENETDOWN
    if (host_errno == ENETDOWN)
        return 100;
#endif
#ifdef ENETUNREACH
    if (host_errno == ENETUNREACH)
        return 101;
#endif
#ifdef ENETRESET
    if (host_errno == ENETRESET)
        return 102;
#endif
#ifdef ECONNABORTED
    if (host_errno == ECONNABORTED)
        return 103;
#endif
#ifdef ECONNRESET
    if (host_errno == ECONNRESET)
        return 104;
#endif
#ifdef ENOBUFS
    if (host_errno == ENOBUFS)
        return 105;
#endif
#ifdef EISCONN
    if (host_errno == EISCONN)
        return 106;
#endif
#ifdef ENOTCONN
    if (host_errno == ENOTCONN)
        return 107;
#endif
#ifdef ESHUTDOWN
    if (host_errno == ESHUTDOWN)
        return 108;
#endif
#ifdef ETOOMANYREFS
    if (host_errno == ETOOMANYREFS)
        return 109;
#endif
#ifdef ETIMEDOUT
    if (host_errno == ETIMEDOUT)
        return 110;
#endif
#ifdef ECONNREFUSED
    if (host_errno == ECONNREFUSED)
        return 111;
#endif
#ifdef EHOSTDOWN
    if (host_errno == EHOSTDOWN)
        return 112;
#endif
#ifdef EHOSTUNREACH
    if (host_errno == EHOSTUNREACH)
        return 113;
#endif
#ifdef EALREADY
    if (host_errno == EALREADY)
        return 114;
#endif
#ifdef EINPROGRESS
    if (host_errno == EINPROGRESS)
        return 115;
#endif
#ifdef EREMOTEIO
    if (host_errno == EREMOTEIO)
        return 121;
#endif
#ifdef EDQUOT
    if (host_errno == EDQUOT)
        return 122;
#endif
    return host_errno;
}

static int fail(cpu_t *cpu, int error)
{
    return finish(cpu, -guest_errno(error));
}

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

/* Guest DRAM holds a fixed little-endian rv64 machine, so multi-byte values are
 * packed byte-wise (LSB first) and the emulator runs identically on little- and
 * big-endian hosts. Each byte is bounds-checked via guest_bytes. */
static int guest_load_u32(cpu_t *cpu, uint64_t address, uint32_t *value) {
    uint32_t result = 0;
    for (unsigned i = 0; i < 4; i++) {
        uint8_t *slot = guest_bytes(cpu, address + i, 1);
        if (slot == NULL) {
            return -1;
        }
        result |= (uint32_t)*slot << (8u * i);
    }
    *value = result;
    return 0;
}

static int guest_load_u64(cpu_t *cpu, uint64_t address, uint64_t *value) {
    uint64_t result = 0;
    for (unsigned i = 0; i < 8; i++) {
        uint8_t *slot = guest_bytes(cpu, address + i, 1);
        if (slot == NULL) {
            return -1;
        }
        result |= (uint64_t)*slot << (8u * i);
    }
    *value = result;
    return 0;
}

static int guest_store_u16(cpu_t *cpu, uint64_t address, uint16_t value) {
    uint8_t *low = guest_bytes(cpu, address, 1);
    uint8_t *high = guest_bytes(cpu, address + 1, 1);
    if (low == NULL || high == NULL) {
        return -1;
    }
    *low = (uint8_t)(value & 0xffu);
    *high = (uint8_t)(value >> 8);
    return 0;
}

static int guest_store_u32(cpu_t *cpu, uint64_t address, uint32_t value) {
    for (unsigned i = 0; i < 4; i++) {
        uint8_t *slot = guest_bytes(cpu, address + i, 1);
        if (slot == NULL) {
            return -1;
        }
        *slot = (uint8_t)(value >> (8u * i));
    }
    return 0;
}

static int guest_store_u64(cpu_t *cpu, uint64_t address, uint64_t value) {
    for (unsigned i = 0; i < 8; i++) {
        uint8_t *slot = guest_bytes(cpu, address + i, 1);
        if (slot == NULL) {
            return -1;
        }
        *slot = (uint8_t)(value >> (8u * i));
    }
    return 0;
}

/* Guest open/pipe flags use the asm-generic (RISC-V) bit layout; host fcntl
 * constants are ABI-specific and are NOT a reliable passthrough. MIPS is the
 * concrete counterexample: guest O_CREAT|O_TRUNC (0x240) reads as host
 * O_EXCL|O_TRUNC without O_CREAT, so openat deterministically fails with
 * ENOENT while the same bits succeed on arches whose layout matches asm-generic.
 * Interpret the guest bits against the asm-generic layout, then re-emit them
 * with the host's own O_* constants. O_ACCMODE (0/1/2) is portable across the
 * arches in the host matrix and passes through unchanged. */
enum {
    RVOF_ACCMODE   = 0x0003,
    RVOF_CREAT     = 0x0040,
    RVOF_EXCL      = 0x0080,
    RVOF_NOCTTY    = 0x0100,
    RVOF_TRUNC     = 0x0200,
    RVOF_APPEND    = 0x0400,
    RVOF_NONBLOCK  = 0x0800,
    RVOF_DSYNC     = 0x1000,
    RVOF_DIRECT    = 0x4000,
    RVOF_DIRECTORY = 0x10000,
    RVOF_NOFOLLOW  = 0x20000,
    RVOF_NOATIME   = 0x40000,
    RVOF_CLOEXEC   = 0x80000,
};

static int guest_open_to_host_flags(int guest_flags) {
    int host_flags = guest_flags & O_ACCMODE;
#ifdef O_CREAT
    if (guest_flags & RVOF_CREAT) {
        host_flags |= O_CREAT;
    }
#endif
#ifdef O_EXCL
    if (guest_flags & RVOF_EXCL) {
        host_flags |= O_EXCL;
    }
#endif
#ifdef O_NOCTTY
    if (guest_flags & RVOF_NOCTTY) {
        host_flags |= O_NOCTTY;
    }
#endif
#ifdef O_TRUNC
    if (guest_flags & RVOF_TRUNC) {
        host_flags |= O_TRUNC;
    }
#endif
#ifdef O_APPEND
    if (guest_flags & RVOF_APPEND) {
        host_flags |= O_APPEND;
    }
#endif
#ifdef O_NONBLOCK
    if (guest_flags & RVOF_NONBLOCK) {
        host_flags |= O_NONBLOCK;
    }
#endif
#ifdef O_DSYNC
    if (guest_flags & RVOF_DSYNC) {
        host_flags |= O_DSYNC;
    }
#endif
#ifdef O_DIRECT
    if (guest_flags & RVOF_DIRECT) {
        host_flags |= O_DIRECT;
    }
#endif
#ifdef O_NOFOLLOW
    if (guest_flags & RVOF_NOFOLLOW) {
        host_flags |= O_NOFOLLOW;
    }
#endif
#ifdef O_CLOEXEC
    if (guest_flags & RVOF_CLOEXEC) {
        host_flags |= O_CLOEXEC;
    }
#endif
    /* RVOF_LARGEFILE (always on for this 64-bit guest), RVOF_DIRECTORY, and
     * RVOF_NOATIME are ABI-advisory; no host bit is required for them here. */
    return host_flags;
}

/* Copy at most dest_len-1 bytes from src and NUL-terminate dest (dest_len >= 1).
 * strnlen+memcpy is the portable replacement for the strncpy(..., sizeof-1)
 * pattern, which gcc warns about with -Wstringop-truncation. */
static void copy_field(char *dest, size_t dest_len, const char *src) {
    size_t length = strnlen(src, dest_len - 1);
    memcpy(dest, src, length);
    dest[length] = 0;
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

/* Copy a stat image into guest DRAM field by field (little-endian, LSB first). */
static int stat_store(cpu_t *cpu, uint64_t address, const struct rv_stat *value) {
    if (guest_store_u64(cpu, address + 0, value->st_dev) < 0 || guest_store_u64(cpu, address + 8, value->st_ino) < 0 ||
        guest_store_u32(cpu, address + 16, value->st_mode) < 0 || guest_store_u32(cpu, address + 20, value->st_nlink) < 0 ||
        guest_store_u32(cpu, address + 24, value->st_uid) < 0 || guest_store_u32(cpu, address + 28, value->st_gid) < 0 ||
        guest_store_u64(cpu, address + 32, value->st_rdev) < 0 || guest_store_u64(cpu, address + 40, 0) < 0 ||
        guest_store_u64(cpu, address + 48, (uint64_t)value->st_size) < 0 || guest_store_u32(cpu, address + 56, (uint32_t)value->st_blksize) < 0 ||
        guest_store_u32(cpu, address + 60, 0) < 0 || guest_store_u64(cpu, address + 64, (uint64_t)value->st_blocks) < 0 ||
        guest_store_u64(cpu, address + 72, (uint64_t)value->st_atime_sec) < 0 || guest_store_u64(cpu, address + 80, value->st_atime_nsec) < 0 ||
        guest_store_u64(cpu, address + 88, (uint64_t)value->st_mtime_sec) < 0 || guest_store_u64(cpu, address + 96, value->st_mtime_nsec) < 0 ||
        guest_store_u64(cpu, address + 104, (uint64_t)value->st_ctime_sec) < 0 || guest_store_u64(cpu, address + 112, value->st_ctime_nsec) < 0 ||
        guest_store_u32(cpu, address + 120, 0) < 0 || guest_store_u32(cpu, address + 124, 0) < 0) {
        return -1;
    }
    return 0;
}

static void timespec_from_host(struct rv_timespec *destination, const struct timespec *source) {
    destination->tv_sec = (int64_t)source->tv_sec;
    destination->tv_nsec = (int64_t)source->tv_nsec;
}

static int sys_read(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    uint8_t *buffer = guest_bytes(cpu, args[1], args[2]);
    if (buffer == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, retry_read((int)args[0], buffer, (size_t)args[2]));
}

static int sys_write(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    uint8_t *buffer = guest_bytes(cpu, args[1], args[2]);
    if (buffer == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, retry_write((int)args[0], buffer, (size_t)args[2]));
}

static int sys_readv(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    if (args[2] > RV_MAX_IOV) {
        return fail(cpu, EINVAL);
    }
    if (args[2] == 0) {
        return finish(cpu, 0);
    }

    struct iovec *host_iov = malloc((size_t)args[2] * sizeof(struct iovec));
    if (host_iov == NULL) {
        return fail(cpu, ENOMEM);
    }

    for (uint64_t i = 0; i < args[2]; i++) {
        uint64_t iov_base = 0;
        uint64_t iov_len = 0;
        if (guest_load_u64(cpu, args[1] + i * 16, &iov_base) < 0 || guest_load_u64(cpu, args[1] + i * 16 + 8, &iov_len) < 0) {
            free(host_iov);
            return fail(cpu, EFAULT);
        }
        uint8_t *buffer = guest_bytes(cpu, iov_base, iov_len);
        if (buffer == NULL) {
            free(host_iov);
            return fail(cpu, EFAULT);
        }
        host_iov[i].iov_base = buffer;
        host_iov[i].iov_len = (size_t)iov_len;
    }

    long result = (long)readv((int)args[0], host_iov, (int)args[2]);
    free(host_iov);
    return host_result(cpu, result);
}

static int sys_writev(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    if (args[2] > RV_MAX_IOV) {
        return fail(cpu, EINVAL);
    }
    if (args[2] == 0) {
        return finish(cpu, 0);
    }

    struct iovec *host_iov = malloc((size_t)args[2] * sizeof(struct iovec));
    if (host_iov == NULL) {
        return fail(cpu, ENOMEM);
    }

    for (uint64_t i = 0; i < args[2]; i++) {
        uint64_t iov_base = 0;
        uint64_t iov_len = 0;
        if (guest_load_u64(cpu, args[1] + i * 16, &iov_base) < 0 || guest_load_u64(cpu, args[1] + i * 16 + 8, &iov_len) < 0) {
            free(host_iov);
            return fail(cpu, EFAULT);
        }
        uint8_t *buffer = guest_bytes(cpu, iov_base, iov_len);
        if (buffer == NULL) {
            free(host_iov);
            return fail(cpu, EFAULT);
        }
        host_iov[i].iov_base = buffer;
        host_iov[i].iov_len = (size_t)iov_len;
    }

    long result = (long)writev((int)args[0], host_iov, (int)args[2]);
    free(host_iov);
    return host_result(cpu, result);
}

static int sys_pread64(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    uint8_t *buffer = guest_bytes(cpu, args[1], args[2]);
    if (buffer == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, (long)pread((int)args[0], buffer, (size_t)args[2], (off_t)args[3]));
}

static int sys_pwrite64(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    uint8_t *buffer = guest_bytes(cpu, args[1], args[2]);
    if (buffer == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, (long)pwrite((int)args[0], buffer, (size_t)args[2], (off_t)args[3]));
}

static int sys_openat(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    const char *path = guest_string(cpu, args[1]);
    if (path == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, openat((int)args[0], path, guest_open_to_host_flags((int)args[2]), (mode_t)args[3]));
}

static int sys_close(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) { return host_result(cpu, close((int)args[0])); }

static int sys_lseek(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) { return host_result(cpu, (long)lseek((int)args[0], (off_t)args[1], (int)args[2])); }

static int sys_dup(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) { return host_result(cpu, dup((int)args[0])); }

static int sys_dup3(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) { return host_result(cpu, dup3((int)args[0], (int)args[1], (int)args[2])); }

static int sys_fcntl(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
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

static int sys_fstat(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    struct stat host_stat;
    if (fstat((int)args[0], &host_stat) < 0) {
        return fail(cpu, errno);
    }
    struct rv_stat guest_stat;
    stat_from_host(&guest_stat, &host_stat);
    if (stat_store(cpu, args[1], &guest_stat) < 0) {
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_newfstatat(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
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
    if (stat_store(cpu, args[2], &guest_stat) < 0) {
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_getcwd(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
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

static int sys_chdir(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    const char *path = guest_string(cpu, args[0]);
    if (path == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, chdir(path));
}

static int sys_mkdirat(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    const char *path = guest_string(cpu, args[1]);
    if (path == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, mkdirat((int)args[0], path, (mode_t)args[2]));
}

static int sys_unlinkat(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    const char *path = guest_string(cpu, args[1]);
    if (path == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, unlinkat((int)args[0], path, (int)args[2]));
}

static int sys_symlinkat(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    const char *target = guest_string(cpu, args[0]);
    const char *linkpath = guest_string(cpu, args[2]);
    if (target == NULL || linkpath == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, symlinkat(target, (int)args[1], linkpath));
}

static int sys_linkat(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    const char *oldpath = guest_string(cpu, args[1]);
    const char *newpath = guest_string(cpu, args[3]);
    if (oldpath == NULL || newpath == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, linkat((int)args[0], oldpath, (int)args[2], newpath, (int)args[4]));
}

static int sys_renameat(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    const char *oldpath = guest_string(cpu, args[1]);
    const char *newpath = guest_string(cpu, args[3]);
    if (oldpath == NULL || newpath == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, renameat((int)args[0], oldpath, (int)args[2], newpath));
}

static int sys_faccessat(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    const char *path = guest_string(cpu, args[1]);
    if (path == NULL) {
        return fail(cpu, EFAULT);
    }
    return host_result(cpu, faccessat((int)args[0], path, (int)args[2], (int)args[3]));
}

static int sys_pipe2(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    int host_fds[2];
    if (pipe2(host_fds, guest_open_to_host_flags((int)args[1])) < 0) {
        return fail(cpu, errno);
    }
    int32_t guest_fds[2] = {host_fds[0], host_fds[1]};
    if (guest_store_u32(cpu, args[0], (uint32_t)guest_fds[0]) < 0 || guest_store_u32(cpu, args[0] + 4, (uint32_t)guest_fds[1]) < 0) {
        close(host_fds[0]);
        close(host_fds[1]);
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_getdents64(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
#ifdef SYS_getdents64
    size_t capacity = (size_t)args[2];
    uint8_t *entries = malloc(capacity == 0 ? 1 : capacity);
    if (entries == NULL) {
        return fail(cpu, ENOMEM);
    }
    /* The kernel fills the buffer in host layout; re-serialize each entry
     * byte-wise (LSB first) into guest DRAM. */
    long result = syscall(SYS_getdents64, (int)args[0], entries, capacity);
    if (result < 0) {
        int error = errno;
        free(entries);
        return fail(cpu, error);
    }
    const char *cursor = (const char *)entries;
    const char *end = (const char *)entries + (size_t)result;
    uint64_t slot = args[1];
    while (cursor < end) {
        const struct rv_dirent64 *entry = (const struct rv_dirent64 *)cursor;
        if (entry->d_reclen < RV_DIRENT64_HEADER || entry->d_reclen > (size_t)(end - cursor)) {
            free(entries);
            return fail(cpu, EINVAL);
        }
        size_t name_length = entry->d_reclen - RV_DIRENT64_HEADER;
        if (guest_store_u64(cpu, slot, entry->d_ino) < 0 || guest_store_u64(cpu, slot + 8, entry->d_off) < 0 ||
            guest_store_u16(cpu, slot + 16, entry->d_reclen) < 0) {
            free(entries);
            return fail(cpu, EFAULT);
        }
        if (name_length > 0) {
            uint8_t *name = guest_bytes(cpu, slot + 18, name_length);
            if (name == NULL) {
                free(entries);
                return fail(cpu, EFAULT);
            }
            memcpy(name, entry->d_name, name_length);
        }
        cursor += entry->d_reclen;
        slot += entry->d_reclen;
    }
    free(entries);
    return finish(cpu, (int64_t)result);
#else
    return fail(cpu, ENOSYS);
#endif
}

static int sys_ioctl(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    unsigned long request = (unsigned long)args[1];
    if (request == TIOCGWINSZ) {
        struct winsize window;
        if (ioctl((int)args[0], request, &window) < 0) {
            return fail(cpu, errno);
        }
        if (guest_store_u16(cpu, args[2], (uint16_t)window.ws_row) < 0 || guest_store_u16(cpu, args[2] + 2, (uint16_t)window.ws_col) < 0 ||
            guest_store_u16(cpu, args[2] + 4, (uint16_t)window.ws_xpixel) < 0 || guest_store_u16(cpu, args[2] + 6, (uint16_t)window.ws_ypixel) < 0) {
            return fail(cpu, EFAULT);
        }
        return finish(cpu, 0);
    }
    if (request == FIONREAD) {
        int available;
        if (ioctl((int)args[0], request, &available) < 0) {
            return fail(cpu, errno);
        }
        if (guest_store_u32(cpu, args[2], (uint32_t)available) < 0) {
            return fail(cpu, EFAULT);
        }
        return finish(cpu, 0);
    }
    return fail(cpu, ENOSYS);
}

static int sys_clock_gettime(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    struct timespec host_time;
    if (clock_gettime((clockid_t)args[0], &host_time) < 0) {
        return fail(cpu, errno);
    }
    struct rv_timespec guest_time;
    timespec_from_host(&guest_time, &host_time);
    if (guest_store_u64(cpu, args[1], (uint64_t)guest_time.tv_sec) < 0 || guest_store_u64(cpu, args[1] + 8, (uint64_t)guest_time.tv_nsec) < 0) {
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_clock_getres(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    struct timespec host_time;
    if (clock_getres((clockid_t)args[0], &host_time) < 0) {
        return fail(cpu, errno);
    }
    if (args[1] != 0) {
        struct rv_timespec guest_time;
        timespec_from_host(&guest_time, &host_time);
        if (guest_store_u64(cpu, args[1], (uint64_t)guest_time.tv_sec) < 0 || guest_store_u64(cpu, args[1] + 8, (uint64_t)guest_time.tv_nsec) < 0) {
            return fail(cpu, EFAULT);
        }
    }
    return finish(cpu, 0);
}

static int sys_gettimeofday(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    struct timeval host_time;
    if (gettimeofday(&host_time, NULL) < 0) {
        return fail(cpu, errno);
    }
    if (args[0] != 0) {
        struct rv_timeval guest_time = {(int64_t)host_time.tv_sec, (int64_t)host_time.tv_usec};
        if (guest_store_u64(cpu, args[0], (uint64_t)guest_time.tv_sec) < 0 || guest_store_u64(cpu, args[0] + 8, (uint64_t)guest_time.tv_usec) < 0) {
            return fail(cpu, EFAULT);
        }
    }
    return finish(cpu, 0);
}

static int sys_nanosleep(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    struct rv_timespec guest_request;
    uint64_t raw = 0;
    if (guest_load_u64(cpu, args[0], &raw) < 0) {
        return fail(cpu, EFAULT);
    }
    guest_request.tv_sec = (int64_t)raw;
    if (guest_load_u64(cpu, args[0] + 8, &raw) < 0) {
        return fail(cpu, EFAULT);
    }
    guest_request.tv_nsec = (int64_t)raw;
    struct timespec host_request = {(time_t)guest_request.tv_sec, (long)guest_request.tv_nsec};
    struct timespec host_remaining;
    if (nanosleep(&host_request, &host_remaining) < 0) {
        if (errno == EINTR && args[1] != 0) {
            struct rv_timespec guest_remaining;
            timespec_from_host(&guest_remaining, &host_remaining);
            if (guest_store_u64(cpu, args[1], (uint64_t)guest_remaining.tv_sec) < 0 || guest_store_u64(cpu, args[1] + 8, (uint64_t)guest_remaining.tv_nsec) < 0) {
                return fail(cpu, EFAULT);
            }
        }
        return fail(cpu, errno);
    }
    return finish(cpu, 0);
}

static int sys_getrlimit(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    struct rlimit host_limit;
    if (getrlimit((int)args[0], &host_limit) < 0) {
        return fail(cpu, errno);
    }
    struct rv_rlimit guest_limit = {(uint64_t)host_limit.rlim_cur, (uint64_t)host_limit.rlim_max};
    if (guest_store_u64(cpu, args[1], guest_limit.rlim_cur) < 0 || guest_store_u64(cpu, args[1] + 8, guest_limit.rlim_max) < 0) {
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_setrlimit(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    struct rv_rlimit guest_limit;
    uint64_t raw = 0;
    if (guest_load_u64(cpu, args[1], &raw) < 0) {
        return fail(cpu, EFAULT);
    }
    guest_limit.rlim_cur = raw;
    if (guest_load_u64(cpu, args[1] + 8, &raw) < 0) {
        return fail(cpu, EFAULT);
    }
    guest_limit.rlim_max = raw;
    struct rlimit host_limit = {(rlim_t)guest_limit.rlim_cur, (rlim_t)guest_limit.rlim_max};
    return host_result(cpu, setrlimit((int)args[0], &host_limit));
}

static int sys_prlimit64(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    struct rlimit host_new;
    struct rlimit host_old;
    struct rlimit *new_limit = NULL;
    struct rlimit *old_limit = NULL;

    if (args[0] != 0) {
        return fail(cpu, ENOSYS);
    }
    if (args[2] != 0) {
        struct rv_rlimit guest_limit;
        uint64_t raw = 0;
        if (guest_load_u64(cpu, args[2], &raw) < 0) {
            return fail(cpu, EFAULT);
        }
        guest_limit.rlim_cur = raw;
        if (guest_load_u64(cpu, args[2] + 8, &raw) < 0) {
            return fail(cpu, EFAULT);
        }
        guest_limit.rlim_max = raw;
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
        if (guest_store_u64(cpu, args[3], guest_limit.rlim_cur) < 0 || guest_store_u64(cpu, args[3] + 8, guest_limit.rlim_max) < 0) {
            return fail(cpu, EFAULT);
        }
    }
    return finish(cpu, 0);
}

static int sys_brk(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    uint64_t requested = args[0];
    if (requested == 0) {
        return finish(cpu, (int64_t)sys->brk);
    }
    if (requested < sys->brk_base) {
        return finish(cpu, (int64_t)sys->brk);
    }
    if (requested > sys->mmap_top) {
        return fail(cpu, ENOMEM);
    }
    if (requested > sys->brk) {
        uint8_t *start = guest_bytes(cpu, sys->brk, requested - sys->brk);
        if (start == NULL) {
            return fail(cpu, ENOMEM);
        }
        memset(start, 0, requested - sys->brk);
    }
    sys->brk = requested;
    return finish(cpu, (int64_t)sys->brk);
}

static int sys_mmap(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
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
    if (length > sys->mmap_top - sys->brk) {
        return fail(cpu, ENOMEM);
    }
    sys->mmap_top -= length;
    uint8_t *memory = guest_bytes(cpu, sys->mmap_top, length);
    if (memory == NULL) {
        sys->mmap_top += length;
        return fail(cpu, ENOMEM);
    }
    memset(memory, 0, length);
    return finish(cpu, (int64_t)sys->mmap_top);
}

static int sys_munmap(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    if (guest_bytes(cpu, args[0], args[1]) == NULL) {
        return fail(cpu, EINVAL);
    }
    return finish(cpu, 0);
}

static int sys_mprotect(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    if (guest_bytes(cpu, args[0], args[1]) == NULL) {
        return fail(cpu, EINVAL);
    }
    return finish(cpu, 0);
}

static int sys_madvise(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    if (guest_bytes(cpu, args[0], args[1]) == NULL) {
        return fail(cpu, EINVAL);
    }
    return finish(cpu, 0);
}

static int sys_uname(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    struct utsname host_name;
    if (uname(&host_name) < 0) {
        return fail(cpu, errno);
    }
    struct rv_utsname guest_name;
    memset(&guest_name, 0, sizeof(guest_name));
    copy_field(guest_name.sysname, sizeof(guest_name.sysname), host_name.sysname);
    copy_field(guest_name.nodename, sizeof(guest_name.nodename), host_name.nodename);
    copy_field(guest_name.release, sizeof(guest_name.release), host_name.release);
    copy_field(guest_name.version, sizeof(guest_name.version), host_name.version);
    copy_field(guest_name.machine, sizeof(guest_name.machine), host_name.machine);
#ifdef _GNU_SOURCE
    copy_field(guest_name.domainname, sizeof(guest_name.domainname), host_name.domainname);
#endif
    if (guest_write(cpu, args[0], &guest_name, sizeof(guest_name)) < 0) {
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_getrandom(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
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

/* Guest socket ABI is Linux-generic (asm-generic / RISC-V), fixed by the
 * guest headers.  rvemu forwards socket syscalls to the host, but when the
 * emulator itself is cross-compiled for a target ABI and run under qemu-user,
 * qemu-user translates those host syscalls according to the target ABI.  MIPS
 * numbers SOCK_STREAM/SOCK_DGRAM and SOL_SOCKET/optname values differently,
 * and BE targets read sockaddr fields in host byte order; forwarding the
 * guest's Linux-generic values verbatim therefore mis-translates on those
 * arches.  Map guest -> host here (mirrors guest_errno/guest_open_flags). */
enum {
    GUEST_AF_INET      = 2,
    GUEST_AF_INET6     = 10,
    GUEST_SOCK_STREAM  = 1,
    GUEST_SOCK_DGRAM   = 2,
    GUEST_SOCK_RAW     = 3,
    GUEST_SOCK_RDM     = 4,
    GUEST_SOCK_SEQPACKET = 5,
    GUEST_SOCK_DCCP    = 6,
    GUEST_SOCK_PACKET  = 10,
    GUEST_SOCK_NONBLOCK = 00004000,
    GUEST_SOCK_CLOEXEC  = 02000000,
    GUEST_SOCK_TYPE_MASK = 0xf,
};

static int guest_sock_domain_to_host(int domain) {
    switch (domain) {
    case GUEST_AF_INET:
        return AF_INET;
#ifdef AF_INET6
    case GUEST_AF_INET6:
        return AF_INET6;
#endif
#ifdef AF_UNIX
    case 1:
        return AF_UNIX;
#endif
    default:
        return domain;
    }
}

static int guest_sock_type_to_host(int type) {
    int flags = 0;
    int base = type & ~GUEST_SOCK_NONBLOCK & ~GUEST_SOCK_CLOEXEC;
    int host_type;

    if (type & GUEST_SOCK_NONBLOCK) {
#ifdef SOCK_NONBLOCK
        flags |= SOCK_NONBLOCK;
#endif
    }
    if (type & GUEST_SOCK_CLOEXEC) {
#ifdef SOCK_CLOEXEC
        flags |= SOCK_CLOEXEC;
#endif
    }

    switch (base) {
    case GUEST_SOCK_STREAM:
        host_type = SOCK_STREAM;
        break;
    case GUEST_SOCK_DGRAM:
        host_type = SOCK_DGRAM;
        break;
#ifdef SOCK_RAW
    case GUEST_SOCK_RAW:
        host_type = SOCK_RAW;
        break;
#endif
#ifdef SOCK_RDM
    case GUEST_SOCK_RDM:
        host_type = SOCK_RDM;
        break;
#endif
#ifdef SOCK_SEQPACKET
    case GUEST_SOCK_SEQPACKET:
        host_type = SOCK_SEQPACKET;
        break;
#endif
#ifdef SOCK_DCCP
    case GUEST_SOCK_DCCP:
        host_type = SOCK_DCCP;
        break;
#endif
#ifdef SOCK_PACKET
    case GUEST_SOCK_PACKET:
        host_type = SOCK_PACKET;
        break;
#endif
    default:
        host_type = base;
        break;
    }
    return host_type | flags;
}

enum {
    GUEST_SOL_SOCKET = 1,
};

static int guest_sockopt_level_to_host(int level) {
    if (level == GUEST_SOL_SOCKET)
        return SOL_SOCKET;
#ifdef IPPROTO_IP
    if (level == 0)
        return IPPROTO_IP;
#endif
#ifdef IPPROTO_TCP
    if (level == 6)
        return IPPROTO_TCP;
#endif
#ifdef IPPROTO_UDP
    if (level == 17)
        return IPPROTO_UDP;
#endif
#ifdef IPPROTO_IPV6
    if (level == 41)
        return IPPROTO_IPV6;
#endif
    return level;
}

static int guest_sockopt_optname_to_host(int level, int optname) {
    if (level != GUEST_SOL_SOCKET)
        return optname;
    switch (optname) {
#ifdef SO_DEBUG
    case 1: return SO_DEBUG;
#endif
#ifdef SO_REUSEADDR
    case 2: return SO_REUSEADDR;
#endif
#ifdef SO_TYPE
    case 3: return SO_TYPE;
#endif
#ifdef SO_ERROR
    case 4: return SO_ERROR;
#endif
#ifdef SO_DONTROUTE
    case 5: return SO_DONTROUTE;
#endif
#ifdef SO_BROADCAST
    case 6: return SO_BROADCAST;
#endif
#ifdef SO_SNDBUF
    case 7: return SO_SNDBUF;
#endif
#ifdef SO_RCVBUF
    case 8: return SO_RCVBUF;
#endif
#ifdef SO_KEEPALIVE
    case 9: return SO_KEEPALIVE;
#endif
#ifdef SO_OOBINLINE
    case 10: return SO_OOBINLINE;
#endif
#ifdef SO_LINGER
    case 13: return SO_LINGER;
#endif
#ifdef SO_REUSEPORT
    case 15: return SO_REUSEPORT;
#endif
#ifdef SO_RCVTIMEO
    case 20: return SO_RCVTIMEO;
#endif
#ifdef SO_SNDTIMEO
    case 21: return SO_SNDTIMEO;
#endif
#ifdef SO_ACCEPTCONN
    case 30: return SO_ACCEPTCONN;
#endif
    default:
        return optname;
    }
}

/* Copy a guest sockaddr into a host sockaddr buffer, fixing sa_family to the
 * host representation.  sin_port/sin_addr are network byte order (byte-identical).
 * Returns host sockaddr length, or -1 if the guest address is invalid. */
static int guest_sockaddr_to_host(const uint8_t *guest, uint64_t guest_len,
                                  uint8_t *host, uint64_t host_cap) {
    uint16_t family;
    if (guest_len < 2 || host_cap < 2)
        return -1;
    family = (uint16_t)guest[0] | ((uint16_t)guest[1] << 8); /* guest is LE */
    if (guest_len > host_cap)
        guest_len = host_cap;
    memcpy(host, guest, (size_t)guest_len);

    /* Write sa_family in host byte order. */
    {
        uint16_t hf = 0;
        if (family == GUEST_AF_INET)
            hf = (uint16_t)AF_INET;
#ifdef AF_INET6
        else if (family == GUEST_AF_INET6)
            hf = (uint16_t)AF_INET6;
#endif
#ifdef AF_UNIX
        else if (family == 1)
            hf = (uint16_t)AF_UNIX;
#endif
        else
            hf = family;
        memcpy(host, &hf, sizeof(hf));
    }

#ifdef AF_INET6
    if (family == GUEST_AF_INET6 && guest_len >= (uint64_t)sizeof(struct sockaddr_in6)) {
        /* sin6_flowinfo and sin6_scope_id are uint32_t host-order fields. */
        struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)host;
        uint32_t flowinfo = (uint32_t)guest[4] | ((uint32_t)guest[5] << 8) |
                            ((uint32_t)guest[6] << 16) | ((uint32_t)guest[7] << 24);
        uint32_t scope_id = (uint32_t)guest[24] | ((uint32_t)guest[25] << 8) |
                            ((uint32_t)guest[26] << 16) | ((uint32_t)guest[27] << 24);
        in6->sin6_flowinfo = flowinfo;
        in6->sin6_scope_id = scope_id;
    }
#endif
    return (int)guest_len;
}

/* Convert a host sockaddr in-place buffer to guest/Linux-generic layout. */
static void host_sockaddr_to_guest(uint8_t *addr, uint64_t len) {
    uint16_t hf;
    uint16_t gf;
    if (len < 2)
        return;
    memcpy(&hf, addr, sizeof(hf));
    if (hf == AF_INET)
        gf = GUEST_AF_INET;
#ifdef AF_INET6
    else if (hf == AF_INET6)
        gf = GUEST_AF_INET6;
#endif
#ifdef AF_UNIX
    else if (hf == AF_UNIX)
        gf = 1;
#endif
    else
        gf = hf;
    addr[0] = (uint8_t)(gf & 0xff);
    addr[1] = (uint8_t)((gf >> 8) & 0xff);

#ifdef AF_INET6
    if (gf == GUEST_AF_INET6 && len >= (uint64_t)sizeof(struct sockaddr_in6)) {
        struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)addr;
        uint32_t flowinfo = in6->sin6_flowinfo;
        uint32_t scope_id = in6->sin6_scope_id;
        /* write guest LE uint32 */
        addr[4] = (uint8_t)(flowinfo & 0xff);
        addr[5] = (uint8_t)((flowinfo >> 8) & 0xff);
        addr[6] = (uint8_t)((flowinfo >> 16) & 0xff);
        addr[7] = (uint8_t)((flowinfo >> 24) & 0xff);
        addr[24] = (uint8_t)(scope_id & 0xff);
        addr[25] = (uint8_t)((scope_id >> 8) & 0xff);
        addr[26] = (uint8_t)((scope_id >> 16) & 0xff);
        addr[27] = (uint8_t)((scope_id >> 24) & 0xff);
    }
#endif
}

static int sys_socket(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    return host_result(cpu, socket(guest_sock_domain_to_host((int)args[0]),
                                   guest_sock_type_to_host((int)args[1]),
                                   (int)args[2]));
}

static int sys_connect(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    uint8_t address[RV_MAX_SOCKADDR];
    uint64_t length = args[2] > RV_MAX_SOCKADDR ? RV_MAX_SOCKADDR : args[2];
    int host_len;

    if (args[1] == 0) {
        return fail(cpu, EFAULT);
    }
    if (guest_read(cpu, args[1], address, length) < 0) {
        return fail(cpu, EFAULT);
    }
    host_len = guest_sockaddr_to_host(address, length, address, sizeof(address));
    if (host_len < 0) {
        return fail(cpu, EINVAL);
    }
    return host_result(cpu, connect((int)args[0], (const struct sockaddr *)address, (socklen_t)host_len));
}

static int sys_sendto(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    uint8_t *buffer = guest_bytes(cpu, args[1], args[2]);
    uint8_t address[RV_MAX_SOCKADDR];
    const struct sockaddr *destination = NULL;
    socklen_t length = 0;

    if (buffer == NULL) {
        return fail(cpu, EFAULT);
    }
    if (args[4] != 0) {
        uint64_t glen = args[5] > RV_MAX_SOCKADDR ? RV_MAX_SOCKADDR : args[5];
        int host_len;
        if (guest_read(cpu, args[4], address, glen) < 0) {
            return fail(cpu, EFAULT);
        }
        host_len = guest_sockaddr_to_host(address, glen, address, sizeof(address));
        if (host_len < 0) {
            return fail(cpu, EINVAL);
        }
        destination = (const struct sockaddr *)address;
        length = (socklen_t)host_len;
    }
    return host_result(cpu, sendto((int)args[0], buffer, (size_t)args[2], (int)args[3], destination, length));
}

static int sys_recvfrom(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
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
        host_sockaddr_to_guest(address, (uint64_t)address_length);
        if (guest_write(cpu, args[4], address, address_length) < 0) {
            return fail(cpu, EFAULT);
        }
    }
    return host_result(cpu, result);
}

static int sys_setsockopt(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    uint8_t value[RV_MAX_SOCKET_OPTION];
    uint64_t length = args[4] > RV_MAX_SOCKET_OPTION ? RV_MAX_SOCKET_OPTION : args[4];
    int level = guest_sockopt_level_to_host((int)args[1]);
    int optname = guest_sockopt_optname_to_host((int)args[1], (int)args[2]);

    if (length > 0) {
        if (args[3] == 0 || guest_read(cpu, args[3], value, length) < 0) {
            return fail(cpu, EFAULT);
        }
    }
    return host_result(cpu, setsockopt((int)args[0], level, optname, value, (socklen_t)length));
}

static int sys_getsockopt(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    uint8_t value[RV_MAX_SOCKET_OPTION];
    uint32_t guest_length = 0;
    socklen_t length;
    int level = guest_sockopt_level_to_host((int)args[1]);
    int optname = guest_sockopt_optname_to_host((int)args[1], (int)args[2]);

    if (args[3] == 0 || args[4] == 0) {
        return fail(cpu, EFAULT);
    }
    if (guest_load_u32(cpu, args[4], &guest_length) < 0) {
        return fail(cpu, EFAULT);
    }
    length = (socklen_t)guest_length;
    if (length > RV_MAX_SOCKET_OPTION) {
        length = RV_MAX_SOCKET_OPTION;
    }
    if (getsockopt((int)args[0], level, optname, value, &length) < 0) {
        return fail(cpu, errno);
    }
    if (length > 0 && guest_write(cpu, args[3], value, length) < 0) {
        return fail(cpu, EFAULT);
    }
    if (guest_store_u32(cpu, args[4], (uint32_t)length) < 0) {
        return fail(cpu, EFAULT);
    }
    return finish(cpu, 0);
}

static int sys_shutdown(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) { return host_result(cpu, shutdown((int)args[0], (int)args[1])); }

static int sys_set_tid_address(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    (void)args;
#ifdef SYS_gettid
    return finish(cpu, (int64_t)syscall(SYS_gettid));
#else
    return finish(cpu, (int64_t)getpid());
#endif
}

static int sys_exit(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    exit((int)args[0]);
    (void)cpu;
    return 0;
}

static int sys_noop(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    return finish(cpu, 0);
}

static int sys_sched_yield(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    return host_result(cpu, sched_yield());
}

static int sys_getpid(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    return finish(cpu, (int64_t)getpid());
}

static int sys_getppid(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    return finish(cpu, (int64_t)getppid());
}

static int sys_getuid(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    return finish(cpu, (int64_t)getuid());
}

static int sys_geteuid(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    return finish(cpu, (int64_t)geteuid());
}

static int sys_getgid(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    return finish(cpu, (int64_t)getgid());
}

static int sys_getegid(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
    return finish(cpu, (int64_t)getegid());
}

static int sys_gettid(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]) {
#ifdef SYS_gettid
    return finish(cpu, (int64_t)syscall(SYS_gettid));
#else
    return finish(cpu, (int64_t)getpid());
#endif
}

/* Default dispatch table, indexed by syscall number. Entries are NULL
 * by default (ENOSYS); syscall_init copies this into sys->syscalls and the
 * application may then override individual numbers there. The numbers 98
 * (futex), 220 (clone) and 221 (execve) have no entry on purpose. */
static const syscall_fn default_syscalls[RV_SYS_MAX] = {
    [RV_SYS_getcwd] = sys_getcwd,
    [RV_SYS_dup] = sys_dup,
    [RV_SYS_dup3] = sys_dup3,
    [RV_SYS_fcntl] = sys_fcntl,
    [RV_SYS_ioctl] = sys_ioctl,
    [RV_SYS_mkdirat] = sys_mkdirat,
    [RV_SYS_unlinkat] = sys_unlinkat,
    [RV_SYS_symlinkat] = sys_symlinkat,
    [RV_SYS_linkat] = sys_linkat,
    [RV_SYS_renameat] = sys_renameat,
    [RV_SYS_faccessat] = sys_faccessat,
    [RV_SYS_chdir] = sys_chdir,
    [RV_SYS_openat] = sys_openat,
    [RV_SYS_close] = sys_close,
    [RV_SYS_pipe2] = sys_pipe2,
    [RV_SYS_getdents64] = sys_getdents64,
    [RV_SYS_lseek] = sys_lseek,
    [RV_SYS_read] = sys_read,
    [RV_SYS_write] = sys_write,
    [RV_SYS_readv] = sys_readv,
    [RV_SYS_writev] = sys_writev,
    [RV_SYS_pread64] = sys_pread64,
    [RV_SYS_pwrite64] = sys_pwrite64,
    [RV_SYS_newfstatat] = sys_newfstatat,
    [RV_SYS_fstat] = sys_fstat,
    [RV_SYS_exit] = sys_exit,
    [RV_SYS_exit_group] = sys_exit,
    [RV_SYS_set_tid_address] = sys_set_tid_address,
    [RV_SYS_nanosleep] = sys_nanosleep,
    [RV_SYS_clock_gettime] = sys_clock_gettime,
    [RV_SYS_clock_getres] = sys_clock_getres,
    [RV_SYS_sched_yield] = sys_sched_yield,
    [RV_SYS_rt_sigaction] = sys_noop,
    [RV_SYS_rt_sigprocmask] = sys_noop,
    [RV_SYS_set_robust_list] = sys_noop,
    [RV_SYS_uname] = sys_uname,
    [RV_SYS_getrlimit] = sys_getrlimit,
    [RV_SYS_setrlimit] = sys_setrlimit,
    [RV_SYS_gettimeofday] = sys_gettimeofday,
    [RV_SYS_getpid] = sys_getpid,
    [RV_SYS_getppid] = sys_getppid,
    [RV_SYS_getuid] = sys_getuid,
    [RV_SYS_geteuid] = sys_geteuid,
    [RV_SYS_getgid] = sys_getgid,
    [RV_SYS_getegid] = sys_getegid,
    [RV_SYS_gettid] = sys_gettid,
    [RV_SYS_socket] = sys_socket,
    [RV_SYS_connect] = sys_connect,
    [RV_SYS_sendto] = sys_sendto,
    [RV_SYS_recvfrom] = sys_recvfrom,
    [RV_SYS_setsockopt] = sys_setsockopt,
    [RV_SYS_getsockopt] = sys_getsockopt,
    [RV_SYS_shutdown] = sys_shutdown,
    [RV_SYS_brk] = sys_brk,
    [RV_SYS_munmap] = sys_munmap,
    [RV_SYS_mmap] = sys_mmap,
    [RV_SYS_mprotect] = sys_mprotect,
    [RV_SYS_madvise] = sys_madvise,
    [RV_SYS_prlimit64] = sys_prlimit64,
    [RV_SYS_getrandom] = sys_getrandom,
    [RV_SYS_membarrier] = sys_noop,
};
/* clang-format on */

void syscall_init(sysstate_t *sys, uint64_t image_size) {
    memset(sys, 0, sizeof(*sys));
    memcpy(sys->syscalls, default_syscalls, sizeof(default_syscalls));

    uint64_t base = align_up(DRAM_BASE + image_size, RV_PAGE_SIZE);
    if (base < DRAM_BASE + DRAM_SIZE / 2) {
        base = DRAM_BASE + DRAM_SIZE / 2;
    }

    sys->brk_base = base;
    sys->brk = base;
    sys->mmap_top = DRAM_BASE + DRAM_SIZE - 64 * 1024;
    if (sys->mmap_top < sys->brk) {
        sys->mmap_top = sys->brk;
    }
    sys->trace = getenv("RVEMU_TRACE_SYSCALLS") != NULL;
}

int syscall_dispatch(cpu_t *cpu, sysstate_t *sys) {
    uint64_t number = cpu->regs[17];
    uint64_t args[6] = {cpu->regs[10], cpu->regs[11], cpu->regs[12], cpu->regs[13], cpu->regs[14], cpu->regs[15]};

    if (sys->trace) {
        fprintf(stderr, "rvemu: syscall %lu(%#lx, %#lx, %#lx, %#lx)\n", (unsigned long)number, (unsigned long)args[0], (unsigned long)args[1], (unsigned long)args[2],
                (unsigned long)args[3]);
    }

    /* Out-of-range numbers and unimplemented entries both yield ENOSYS. */
    if (number >= RV_SYS_MAX) {
        return fail(cpu, ENOSYS);
    }
    syscall_fn fn = sys->syscalls[number];
    if (fn == NULL) {
        return fail(cpu, ENOSYS);
    }
    int result = fn(cpu, sys, args);
    if (sys->trace) {
        fprintf(stderr, "rvemu: syscall %lu -> %ld\n", (unsigned long)number, (long)(int64_t)cpu->regs[10]);
    }
    return result;
}
