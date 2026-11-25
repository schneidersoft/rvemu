#ifndef RV_SYSCALL_H
#define RV_SYSCALL_H

#include <stdint.h>

#include "librv64i.h"

/* Linux RISC-V (asm-generic) syscall numbers. */
#ifndef GUEST
#define GUEST guest
#define _GUEST_H_DEFINED_NAMESPACE
#endif
#include "guest.h"
#ifdef _GUEST_H_DEFINED_NAMESPACE
#undef GUEST
#undef _GUEST_H_DEFINED_NAMESPACE
#endif
enum {
    RV_SYS_getcwd = guest_SYS_getcwd,
    RV_SYS_dup = guest_SYS_dup,
    RV_SYS_dup3 = guest_SYS_dup3,
    RV_SYS_fcntl = guest_SYS_fcntl,
    RV_SYS_ioctl = guest_SYS_ioctl,
    RV_SYS_mkdirat = guest_SYS_mkdirat,
    RV_SYS_unlinkat = guest_SYS_unlinkat,
    RV_SYS_symlinkat = guest_SYS_symlinkat,
    RV_SYS_linkat = guest_SYS_linkat,
    RV_SYS_renameat = guest_SYS_renameat,
    RV_SYS_faccessat = guest_SYS_faccessat,
    RV_SYS_chdir = guest_SYS_chdir,
    RV_SYS_openat = guest_SYS_openat,
    RV_SYS_close = guest_SYS_close,
    RV_SYS_pipe2 = guest_SYS_pipe2,
    RV_SYS_getdents64 = guest_SYS_getdents64,
    RV_SYS_lseek = guest_SYS_lseek,
    RV_SYS_read = guest_SYS_read,
    RV_SYS_write = guest_SYS_write,
    RV_SYS_readv = guest_SYS_readv,
    RV_SYS_writev = guest_SYS_writev,
    RV_SYS_pread64 = guest_SYS_pread64,
    RV_SYS_pwrite64 = guest_SYS_pwrite64,
    RV_SYS_newfstatat = guest_SYS_newfstatat,
    RV_SYS_fstat = guest_SYS_fstat,
    RV_SYS_exit = guest_SYS_exit,
    RV_SYS_exit_group = guest_SYS_exit_group,
    RV_SYS_set_tid_address = guest_SYS_set_tid_address,
    RV_SYS_futex = guest_SYS_futex,
    RV_SYS_set_robust_list = guest_SYS_set_robust_list,
    RV_SYS_nanosleep = guest_SYS_nanosleep,
    RV_SYS_clock_gettime = guest_SYS_clock_gettime,
    RV_SYS_clock_getres = guest_SYS_clock_getres,
    RV_SYS_sched_yield = guest_SYS_sched_yield,
    RV_SYS_uname = guest_SYS_uname,
    RV_SYS_rt_sigaction = guest_SYS_rt_sigaction,
    RV_SYS_rt_sigprocmask = guest_SYS_rt_sigprocmask,
    RV_SYS_getrlimit = guest_SYS_getrlimit,
    RV_SYS_setrlimit = guest_SYS_setrlimit,
    RV_SYS_gettimeofday = guest_SYS_gettimeofday,
    RV_SYS_getpid = guest_SYS_getpid,
    RV_SYS_getppid = guest_SYS_getppid,
    RV_SYS_getuid = guest_SYS_getuid,
    RV_SYS_geteuid = guest_SYS_geteuid,
    RV_SYS_getgid = guest_SYS_getgid,
    RV_SYS_getegid = guest_SYS_getegid,
    RV_SYS_gettid = guest_SYS_gettid,
    RV_SYS_socket = guest_SYS_socket,
    RV_SYS_connect = guest_SYS_connect,
    RV_SYS_sendto = guest_SYS_sendto,
    RV_SYS_recvfrom = guest_SYS_recvfrom,
    RV_SYS_setsockopt = guest_SYS_setsockopt,
    RV_SYS_getsockopt = guest_SYS_getsockopt,
    RV_SYS_shutdown = guest_SYS_shutdown,
    RV_SYS_brk = guest_SYS_brk,
    RV_SYS_munmap = guest_SYS_munmap,
    RV_SYS_clone = guest_SYS_clone,
    RV_SYS_execve = guest_SYS_execve,
    RV_SYS_mmap = guest_SYS_mmap,
    RV_SYS_mprotect = guest_SYS_mprotect,
    RV_SYS_madvise = guest_SYS_madvise,
    RV_SYS_prlimit64 = guest_SYS_prlimit64,
    RV_SYS_getrandom = guest_SYS_getrandom,
    RV_SYS_membarrier = guest_SYS_membarrier,
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
