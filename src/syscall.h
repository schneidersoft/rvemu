#ifndef RV_SYSCALL_H
#define RV_SYSCALL_H

#include <stdint.h>

#include "librv64i.h"

void syscall_init(cpu_t *cpu, uint64_t image_size);
int syscall_dispatch(cpu_t *cpu);

#endif
