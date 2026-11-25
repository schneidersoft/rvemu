/* Guest ABI constants shared by the RISC-V guest and the host emulator.
 *
 * Default (guest side):  #include "guest.h"          -> EPERM, O_CREAT, ...
 * Namespaced (host):     #define GUEST guest
 *                        #include "guest.h"          -> guest_EPERM, ...
 *
 * Values are Linux asm-generic / RISC-V ABI numbers. */
#ifndef GUEST_H
#define GUEST_H

#ifndef GUEST
#define _GUEST(x) x
#else
#define _GUEST_CAT_(a, b) a##b
#define _GUEST_CAT(a, b) _GUEST_CAT_(a, b)
#define _GUEST_SYM(ns, name) _GUEST_CAT(ns, _GUEST_CAT(_, name))
#define _GUEST(name) _GUEST_SYM(GUEST, name)
#endif

/* Errno (Linux asm-generic). */
enum {
    _GUEST(EPERM)               = 1,
    _GUEST(ENOENT)              = 2,
    _GUEST(ESRCH)               = 3,
    _GUEST(EINTR)               = 4,
    _GUEST(EIO)                 = 5,
    _GUEST(ENXIO)               = 6,
    _GUEST(E2BIG)               = 7,
    _GUEST(ENOEXEC)             = 8,
    _GUEST(EBADF)               = 9,
    _GUEST(ECHILD)              = 10,
    _GUEST(EAGAIN)              = 11,
    _GUEST(ENOMEM)              = 12,
    _GUEST(EACCES)              = 13,
    _GUEST(EFAULT)              = 14,
    _GUEST(ENOTBLK)             = 15,
    _GUEST(EBUSY)               = 16,
    _GUEST(EEXIST)              = 17,
    _GUEST(EXDEV)               = 18,
    _GUEST(ENODEV)              = 19,
    _GUEST(ENOTDIR)             = 20,
    _GUEST(EISDIR)              = 21,
    _GUEST(EINVAL)              = 22,
    _GUEST(ENFILE)              = 23,
    _GUEST(EMFILE)              = 24,
    _GUEST(ENOTTY)              = 25,
    _GUEST(ETXTBSY)             = 26,
    _GUEST(EFBIG)               = 27,
    _GUEST(ENOSPC)              = 28,
    _GUEST(ESPIPE)              = 29,
    _GUEST(EROFS)               = 30,
    _GUEST(EMLINK)              = 31,
    _GUEST(EPIPE)               = 32,
    _GUEST(EDOM)                = 33,
    _GUEST(ERANGE)              = 34,
    _GUEST(EDEADLK)             = 35,
    _GUEST(ENAMETOOLONG)        = 36,
    _GUEST(ENOLCK)              = 37,
    _GUEST(ENOSYS)              = 38,
    _GUEST(ENOTEMPTY)           = 39,
    _GUEST(ELOOP)               = 40,
    _GUEST(ENOMSG)              = 42,
    _GUEST(ENODATA)             = 61,
    _GUEST(ENOSR)               = 63,
    _GUEST(ENOTSOCK)            = 88,
    _GUEST(EDESTADDRREQ)        = 89,
    _GUEST(EMSGSIZE)            = 90,
    _GUEST(EPROTOTYPE)          = 91,
    _GUEST(ENOPROTOOPT)         = 92,
    _GUEST(EPROTONOSUPPORT)     = 93,
    _GUEST(EOPNOTSUPP)          = 95,
    _GUEST(EAFNOSUPPORT)        = 97,
    _GUEST(EADDRINUSE)          = 98,
    _GUEST(EADDRNOTAVAIL)       = 99,
    _GUEST(ENETDOWN)            = 100,
    _GUEST(ENETUNREACH)         = 101,
    _GUEST(ENETRESET)           = 102,
    _GUEST(ECONNABORTED)        = 103,
    _GUEST(ECONNRESET)          = 104,
    _GUEST(ENOBUFS)             = 105,
    _GUEST(EISCONN)             = 106,
    _GUEST(ENOTCONN)            = 107,
    _GUEST(ESHUTDOWN)           = 108,
    _GUEST(ETOOMANYREFS)        = 109,
    _GUEST(ETIMEDOUT)           = 110,
    _GUEST(ECONNREFUSED)        = 111,
    _GUEST(EHOSTDOWN)           = 112,
    _GUEST(EHOSTUNREACH)        = 113,
    _GUEST(EALREADY)            = 114,
    _GUEST(EINPROGRESS)         = 115,
    _GUEST(EREMOTEIO)           = 121,
    _GUEST(EDQUOT)              = 122,
    _GUEST(EWOULDBLOCK)         = _GUEST(EAGAIN),
};

/* Open/fcntl flags (asm-generic). */
enum {
    _GUEST(O_ACCMODE)           = 0003,
    _GUEST(O_RDONLY)            = 0000,
    _GUEST(O_WRONLY)            = 0001,
    _GUEST(O_RDWR)              = 0002,
    _GUEST(O_CREAT)             = 0100,
    _GUEST(O_EXCL)              = 0200,
    _GUEST(O_NOCTTY)            = 0400,
    _GUEST(O_TRUNC)             = 01000,
    _GUEST(O_APPEND)            = 02000,
    _GUEST(O_NONBLOCK)          = 04000,
    _GUEST(O_DSYNC)             = 010000,
    _GUEST(O_DIRECT)            = 040000,
    _GUEST(O_LARGEFILE)         = 0100000,
    _GUEST(O_DIRECTORY)         = 0200000,
    _GUEST(O_NOFOLLOW)          = 0400000,
    _GUEST(O_NOATIME)           = 01000000,
    _GUEST(O_CLOEXEC)           = 02000000,
};

/* File/socket seek, access, and misc constants shared with guest. */
enum {
    _GUEST(AT_FDCWD)            = -100,
    _GUEST(SEEK_SET)            = 0,
    _GUEST(SEEK_CUR)            = 1,
    _GUEST(SEEK_END)            = 2,
};

/* Mmap/clock constants shared with guest. */
enum {
    _GUEST(PROT_READ)           = 1,
    _GUEST(PROT_WRITE)          = 2,
    _GUEST(MAP_PRIVATE)         = 2,
    _GUEST(MAP_ANONYMOUS)       = 0x20,
    _GUEST(CLOCK_REALTIME)      = 0,
};

/* Socket ABI (Linux-generic/asm-generic). */
enum {
    _GUEST(AF_UNSPEC)           = 0,
    _GUEST(AF_UNIX)             = 1,
    _GUEST(AF_INET)             = 2,
    _GUEST(AF_INET6)            = 10,

    _GUEST(SOCK_STREAM)         = 1,
    _GUEST(SOCK_DGRAM)          = 2,
    _GUEST(SOCK_RAW)            = 3,
    _GUEST(SOCK_RDM)            = 4,
    _GUEST(SOCK_SEQPACKET)      = 5,
    _GUEST(SOCK_DCCP)           = 6,
    _GUEST(SOCK_PACKET)         = 10,
    _GUEST(SOCK_NONBLOCK)       = 00004000,
    _GUEST(SOCK_CLOEXEC)        = 02000000,

    _GUEST(SOL_SOCKET)          = 1,
    _GUEST(IPPROTO_IP)          = 0,
    _GUEST(IPPROTO_ICMP)        = 1,
    _GUEST(IPPROTO_TCP)         = 6,
    _GUEST(IPPROTO_UDP)         = 17,
    _GUEST(IPPROTO_ICMPV6)      = 58,
    _GUEST(IPPROTO_IPV6)        = 41,
    _GUEST(IPPROTO_RAW)         = 255,

    _GUEST(SHUT_RD)             = 0,
    _GUEST(SHUT_WR)             = 1,
    _GUEST(SHUT_RDWR)           = 2,

    _GUEST(SO_DEBUG)            = 1,
    _GUEST(SO_REUSEADDR)        = 2,
    _GUEST(SO_TYPE)             = 3,
    _GUEST(SO_ERROR)            = 4,
    _GUEST(SO_DONTROUTE)        = 5,
    _GUEST(SO_BROADCAST)        = 6,
    _GUEST(SO_SNDBUF)           = 7,
    _GUEST(SO_RCVBUF)           = 8,
    _GUEST(SO_KEEPALIVE)        = 9,
    _GUEST(SO_OOBINLINE)        = 10,
    _GUEST(SO_LINGER)           = 13,
    _GUEST(SO_REUSEPORT)        = 15,
    _GUEST(SO_RCVTIMEO)         = 20,
    _GUEST(SO_SNDTIMEO)         = 21,
    _GUEST(SO_ACCEPTCONN)       = 30,
};

/* Syscall numbers (Linux RISC-V / asm-generic). */
enum {
    _GUEST(SYS_getcwd)          = 17,
    _GUEST(SYS_dup)             = 23,
    _GUEST(SYS_dup3)            = 24,
    _GUEST(SYS_fcntl)           = 25,
    _GUEST(SYS_ioctl)           = 29,
    _GUEST(SYS_mkdirat)         = 34,
    _GUEST(SYS_unlinkat)        = 35,
    _GUEST(SYS_symlinkat)       = 36,
    _GUEST(SYS_linkat)          = 37,
    _GUEST(SYS_renameat)        = 38,
    _GUEST(SYS_faccessat)       = 48,
    _GUEST(SYS_chdir)           = 49,
    _GUEST(SYS_openat)          = 56,
    _GUEST(SYS_close)           = 57,
    _GUEST(SYS_pipe2)           = 59,
    _GUEST(SYS_getdents64)      = 61,
    _GUEST(SYS_lseek)           = 62,
    _GUEST(SYS_read)            = 63,
    _GUEST(SYS_write)           = 64,
    _GUEST(SYS_readv)           = 65,
    _GUEST(SYS_writev)          = 66,
    _GUEST(SYS_pread64)         = 67,
    _GUEST(SYS_pwrite64)        = 68,
    _GUEST(SYS_newfstatat)      = 79,
    _GUEST(SYS_fstat)           = 80,
    _GUEST(SYS_exit)            = 93,
    _GUEST(SYS_exit_group)      = 94,
    _GUEST(SYS_set_tid_address) = 96,
    _GUEST(SYS_futex)           = 98,
    _GUEST(SYS_set_robust_list) = 99,
    _GUEST(SYS_nanosleep)       = 101,
    _GUEST(SYS_clock_gettime)   = 113,
    _GUEST(SYS_clock_getres)    = 114,
    _GUEST(SYS_sched_yield)     = 124,
    _GUEST(SYS_rt_sigaction)    = 134,
    _GUEST(SYS_rt_sigprocmask)  = 135,
    _GUEST(SYS_uname)           = 160,
    _GUEST(SYS_getrlimit)       = 163,
    _GUEST(SYS_setrlimit)       = 164,
    _GUEST(SYS_gettimeofday)    = 169,
    _GUEST(SYS_getpid)          = 172,
    _GUEST(SYS_getppid)         = 173,
    _GUEST(SYS_getuid)          = 174,
    _GUEST(SYS_geteuid)         = 175,
    _GUEST(SYS_getgid)          = 176,
    _GUEST(SYS_getegid)         = 177,
    _GUEST(SYS_gettid)          = 178,
    _GUEST(SYS_socket)          = 198,
    _GUEST(SYS_connect)         = 203,
    _GUEST(SYS_sendto)          = 206,
    _GUEST(SYS_recvfrom)        = 207,
    _GUEST(SYS_setsockopt)      = 208,
    _GUEST(SYS_getsockopt)      = 209,
    _GUEST(SYS_shutdown)        = 210,
    _GUEST(SYS_brk)             = 214,
    _GUEST(SYS_munmap)          = 215,
    _GUEST(SYS_clone)           = 220,
    _GUEST(SYS_execve)          = 221,
    _GUEST(SYS_mmap)            = 222,
    _GUEST(SYS_mprotect)        = 226,
    _GUEST(SYS_madvise)         = 233,
    _GUEST(SYS_prlimit64)       = 261,
    _GUEST(SYS_getrandom)       = 278,
    _GUEST(SYS_membarrier)      = 283,
};

#undef _GUEST
#undef _GUEST_SYM
#undef _GUEST_CAT
#undef _GUEST_CAT_

#endif /* GUEST_H */
