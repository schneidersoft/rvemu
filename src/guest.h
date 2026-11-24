/* Guest ABI constants shared by the RISC-V guest and the host emulator.
 *
 * Default (guest side):  #include "guest.h"          -> EPERM, O_CREAT, ...
 * Namespaced (host):     #define NAMESPACE guest
 *                        #include "guest.h"          -> guest_EPERM, ...
 *
 * Values are Linux asm-generic / RISC-V ABI numbers. */
#ifndef GUEST_H
#define GUEST_H

#ifdef NAMESPACE
#define _GUEST_CAT_(a, b) a##b
#define _GUEST_CAT(a, b) _GUEST_CAT_(a, b)
#define _GUEST_SYM(ns, name) _GUEST_CAT(ns, _GUEST_CAT(_, name))
#define G_(name) _GUEST_SYM(NAMESPACE, name)
#else
#define G_(name) name
#endif

/* Errno (Linux asm-generic). */
enum {
    G_(EPERM)           = 1,
    G_(ENOENT)          = 2,
    G_(ESRCH)           = 3,
    G_(EINTR)           = 4,
    G_(EIO)             = 5,
    G_(ENXIO)           = 6,
    G_(E2BIG)           = 7,
    G_(ENOEXEC)         = 8,
    G_(EBADF)           = 9,
    G_(ECHILD)          = 10,
    G_(EAGAIN)          = 11,
    G_(ENOMEM)          = 12,
    G_(EACCES)          = 13,
    G_(EFAULT)          = 14,
    G_(ENOTBLK)         = 15,
    G_(EBUSY)           = 16,
    G_(EEXIST)          = 17,
    G_(EXDEV)           = 18,
    G_(ENODEV)          = 19,
    G_(ENOTDIR)         = 20,
    G_(EISDIR)          = 21,
    G_(EINVAL)          = 22,
    G_(ENFILE)          = 23,
    G_(EMFILE)          = 24,
    G_(ENOTTY)          = 25,
    G_(ETXTBSY)         = 26,
    G_(EFBIG)           = 27,
    G_(ENOSPC)          = 28,
    G_(ESPIPE)          = 29,
    G_(EROFS)           = 30,
    G_(EMLINK)          = 31,
    G_(EPIPE)           = 32,
    G_(EDOM)            = 33,
    G_(ERANGE)          = 34,
    G_(EDEADLK)         = 35,
    G_(ENAMETOOLONG)    = 36,
    G_(ENOLCK)          = 37,
    G_(ENOSYS)          = 38,
    G_(ENOTEMPTY)       = 39,
    G_(ELOOP)           = 40,
    G_(ENOMSG)          = 42,
    G_(ENODATA)         = 61,
    G_(ENOSR)           = 63,
    G_(ENOTSOCK)        = 88,
    G_(EDESTADDRREQ)    = 89,
    G_(EMSGSIZE)        = 90,
    G_(EPROTOTYPE)      = 91,
    G_(ENOPROTOOPT)     = 92,
    G_(EPROTONOSUPPORT) = 93,
    G_(EOPNOTSUPP)      = 95,
    G_(EAFNOSUPPORT)    = 97,
    G_(EADDRINUSE)      = 98,
    G_(EADDRNOTAVAIL)   = 99,
    G_(ENETDOWN)        = 100,
    G_(ENETUNREACH)     = 101,
    G_(ENETRESET)       = 102,
    G_(ECONNABORTED)    = 103,
    G_(ECONNRESET)      = 104,
    G_(ENOBUFS)         = 105,
    G_(EISCONN)         = 106,
    G_(ENOTCONN)        = 107,
    G_(ESHUTDOWN)       = 108,
    G_(ETOOMANYREFS)    = 109,
    G_(ETIMEDOUT)       = 110,
    G_(ECONNREFUSED)    = 111,
    G_(EHOSTDOWN)       = 112,
    G_(EHOSTUNREACH)    = 113,
    G_(EALREADY)        = 114,
    G_(EINPROGRESS)     = 115,
    G_(EREMOTEIO)       = 121,
    G_(EDQUOT)          = 122,
};

#ifndef NAMESPACE
#define EWOULDBLOCK EAGAIN
#endif

/* Open/fcntl flags (asm-generic). */
enum {
    G_(O_ACCMODE)   = 0003,
    G_(O_RDONLY)    = 0000,
    G_(O_WRONLY)    = 0001,
    G_(O_RDWR)      = 0002,
    G_(O_CREAT)     = 0100,
    G_(O_EXCL)      = 0200,
    G_(O_NOCTTY)    = 0400,
    G_(O_TRUNC)     = 01000,
    G_(O_APPEND)    = 02000,
    G_(O_NONBLOCK)  = 04000,
    G_(O_DSYNC)     = 010000,
    G_(O_DIRECT)    = 040000,
    G_(O_LARGEFILE) = 0100000,
    G_(O_DIRECTORY) = 0200000,
    G_(O_NOFOLLOW)  = 0400000,
    G_(O_NOATIME)   = 01000000,
    G_(O_CLOEXEC)   = 02000000,
};

/* File/socket seek, access, and misc constants shared with guest. */
enum {
    G_(AT_FDCWD)       = -100,
    G_(SEEK_SET)       = 0,
    G_(SEEK_CUR)       = 1,
    G_(SEEK_END)       = 2,
};

/* Mmap/clock constants shared with guest. */
enum {
    G_(PROT_READ)      = 1,
    G_(PROT_WRITE)     = 2,
    G_(MAP_PRIVATE)    = 2,
    G_(MAP_ANONYMOUS)  = 0x20,
    G_(CLOCK_REALTIME) = 0,
};

/* Socket ABI (Linux-generic/asm-generic). */
enum {
    G_(AF_UNSPEC)    = 0,
    G_(AF_UNIX)      = 1,
    G_(AF_INET)      = 2,
    G_(AF_INET6)     = 10,

    G_(SOCK_STREAM)    = 1,
    G_(SOCK_DGRAM)     = 2,
    G_(SOCK_RAW)       = 3,
    G_(SOCK_RDM)       = 4,
    G_(SOCK_SEQPACKET) = 5,
    G_(SOCK_DCCP)      = 6,
    G_(SOCK_PACKET)    = 10,
    G_(SOCK_NONBLOCK)  = 00004000,
    G_(SOCK_CLOEXEC)   = 02000000,

    G_(SOL_SOCKET)   = 1,
    G_(IPPROTO_IP)   = 0,
    G_(IPPROTO_ICMP) = 1,
    G_(IPPROTO_TCP)  = 6,
    G_(IPPROTO_UDP)  = 17,
    G_(IPPROTO_ICMPV6)= 58,
    G_(IPPROTO_IPV6) = 41,
    G_(IPPROTO_RAW)  = 255,

    G_(SHUT_RD)      = 0,
    G_(SHUT_WR)      = 1,
    G_(SHUT_RDWR)    = 2,

    G_(SO_DEBUG)     = 1,
    G_(SO_REUSEADDR) = 2,
    G_(SO_TYPE)      = 3,
    G_(SO_ERROR)     = 4,
    G_(SO_DONTROUTE) = 5,
    G_(SO_BROADCAST) = 6,
    G_(SO_SNDBUF)    = 7,
    G_(SO_RCVBUF)    = 8,
    G_(SO_KEEPALIVE) = 9,
    G_(SO_OOBINLINE) = 10,
    G_(SO_LINGER)    = 13,
    G_(SO_REUSEPORT) = 15,
    G_(SO_RCVTIMEO)  = 20,
    G_(SO_SNDTIMEO)  = 21,
    G_(SO_ACCEPTCONN)= 30,
};

/* Syscall numbers (Linux RISC-V / asm-generic). */
enum {
    G_(SYS_getcwd)          = 17,
    G_(SYS_dup)             = 23,
    G_(SYS_dup3)            = 24,
    G_(SYS_fcntl)           = 25,
    G_(SYS_ioctl)           = 29,
    G_(SYS_mkdirat)         = 34,
    G_(SYS_unlinkat)        = 35,
    G_(SYS_symlinkat)       = 36,
    G_(SYS_linkat)          = 37,
    G_(SYS_renameat)        = 38,
    G_(SYS_faccessat)       = 48,
    G_(SYS_chdir)           = 49,
    G_(SYS_openat)          = 56,
    G_(SYS_close)           = 57,
    G_(SYS_pipe2)           = 59,
    G_(SYS_getdents64)      = 61,
    G_(SYS_lseek)           = 62,
    G_(SYS_read)            = 63,
    G_(SYS_write)           = 64,
    G_(SYS_readv)           = 65,
    G_(SYS_writev)          = 66,
    G_(SYS_pread64)         = 67,
    G_(SYS_pwrite64)        = 68,
    G_(SYS_newfstatat)      = 79,
    G_(SYS_fstat)           = 80,
    G_(SYS_exit)            = 93,
    G_(SYS_exit_group)      = 94,
    G_(SYS_set_tid_address) = 96,
    G_(SYS_futex)           = 98,
    G_(SYS_set_robust_list) = 99,
    G_(SYS_nanosleep)       = 101,
    G_(SYS_clock_gettime)   = 113,
    G_(SYS_clock_getres)    = 114,
    G_(SYS_sched_yield)     = 124,
    G_(SYS_rt_sigaction)    = 134,
    G_(SYS_rt_sigprocmask)  = 135,
    G_(SYS_uname)           = 160,
    G_(SYS_getrlimit)       = 163,
    G_(SYS_setrlimit)       = 164,
    G_(SYS_gettimeofday)    = 169,
    G_(SYS_getpid)          = 172,
    G_(SYS_getppid)         = 173,
    G_(SYS_getuid)          = 174,
    G_(SYS_geteuid)         = 175,
    G_(SYS_getgid)          = 176,
    G_(SYS_getegid)         = 177,
    G_(SYS_gettid)          = 178,
    G_(SYS_socket)          = 198,
    G_(SYS_connect)         = 203,
    G_(SYS_sendto)          = 206,
    G_(SYS_recvfrom)        = 207,
    G_(SYS_setsockopt)      = 208,
    G_(SYS_getsockopt)      = 209,
    G_(SYS_shutdown)        = 210,
    G_(SYS_brk)             = 214,
    G_(SYS_munmap)          = 215,
    G_(SYS_clone)           = 220,
    G_(SYS_execve)          = 221,
    G_(SYS_mmap)            = 222,
    G_(SYS_mprotect)        = 226,
    G_(SYS_madvise)         = 233,
    G_(SYS_prlimit64)       = 261,
    G_(SYS_getrandom)       = 278,
    G_(SYS_membarrier)      = 283,
};

#undef G_
#undef _GUEST_SYM
#undef _GUEST_CAT
#undef _GUEST_CAT_

#endif /* GUEST_H */
