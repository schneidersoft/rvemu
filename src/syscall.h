#ifndef RV_SYSCALL_H
#define RV_SYSCALL_H

#include <stdint.h>

#include "librv64i.h"

/* Linux RISC-V (asm-generic) syscall numbers. */
#define GUEST RV
#include "guest.h"

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
