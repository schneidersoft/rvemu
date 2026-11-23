#ifndef RV_SYSCALL_H
#define RV_SYSCALL_H

#include <stdint.h>

#include "librv64i.h"

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
#define RV_SYS_MAX 512 /* size of the dispatch table; numbers below it are "known" */

typedef struct sysstate_t sysstate_t;

/* Syscall handler: called with the syscall number's six a0-a5 arguments.
 * Write the guest-visible result (or negative errno) to regs[10] via finish/fail
 * convention and return 0; callers in the core stop the guest on non-zero. */
typedef int (*syscall_fn)(cpu_t *cpu, sysstate_t *sys, const uint64_t args[6]);

/* Guest process state backing the syscall layer. Owned by the application:
 * one instance per emulated guest process, paired with a cpu_t instance
 * by the caller. The syscalls table is fully populated by syscall_init();
 * the application may override individual entries per syscall number, e.g.
 *   sys->syscalls[RV_SYS_brk] = my_brk; */
typedef struct sysstate_t {
    uint64_t brk_base; // initial program break (brk() lower bound)
    uint64_t brk;      // current program break (heap top)
    uint64_t mmap_top; // top-down mmap allocation pointer
    int trace;         // log emulated syscalls when enabled
    syscall_fn syscalls[RV_SYS_MAX]; // dispatch table; NULL entry = ENOSYS
} sysstate_t;

void syscall_init(sysstate_t *sys, uint64_t image_size);
int syscall_dispatch(cpu_t *cpu, sysstate_t *sys);

#endif
