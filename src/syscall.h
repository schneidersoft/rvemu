#ifndef RV_SYSCALL_H
#define RV_SYSCALL_H

#include <stdint.h>

#include "librv64i.h"

/* Guest process memory state backing the brk()/mmap() emulation.
 * Owned by the application: one instance per emulated guest process,
 * paired with a cpu_t instance by the caller. */
typedef struct sysstate_t {
    uint64_t brk_base; // initial program break (brk() lower bound)
    uint64_t brk;      // current program break (heap top)
    uint64_t mmap_top; // top-down mmap allocation pointer
    int trace;         // log emulated syscalls when enabled
} sysstate_t;

void syscall_init(sysstate_t *sys, uint64_t image_size);
int syscall_dispatch(cpu_t *cpu, sysstate_t *sys);

#endif
