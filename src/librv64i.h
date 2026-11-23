#ifndef LIBRISC_H
#define LIBRISC_H

#include <stdint.h>

// 1 MiB DRAM
#define DRAM_SIZE 1024 * 1024 * 1
#define DRAM_BASE 0x00000000

// The guest links its stack at the top of DRAM (see test/riscv.ld), with the
// start pointer one past the top slot. The initial stack pointer points at
// that same top address — the value startup loads via `la sp, _stack_top` —
// so it is always a valid in-DRAM value rather than one past the end of RAM.
#define STACK_SIZE 16 * 1024
#define STACK_TOP (DRAM_BASE + DRAM_SIZE - STACK_SIZE)

typedef struct dram_t {
    uint8_t mem[DRAM_SIZE];
} dram_t;

typedef struct bus_t {
    struct dram_t dram;
} bus_t;

struct cpu_t;

// Trap handlers: invoked by the core when a trap instruction or an invalid
// instruction is executed. The core owns no such concepts; the application
// (or an intermediate layer) wires these in per cpu_t instance. A NULL
// handler means the trap is reported as an unimplemented instruction.
typedef int (*trap_cb)(struct cpu_t *cpu, uint32_t inst);

typedef struct cpu_t {
    uint64_t regs[32]; // 32 64-bit registers (x0-x31)
    uint64_t pc;       // 64-bit program counter
    struct bus_t bus;  // cpu_t connected to bus_t
    trap_cb ecall_cb;  // invoked on ECALL (imm_I == 0x0)
    trap_cb ebreak_cb; // invoked on EBREAK (imm_I == 0x1)
    trap_cb invop_cb;  // invoked on an unrecognized instruction
} cpu_t;

uint64_t dram_load(dram_t *dram, uint64_t addr, uint64_t size);
void dram_store(dram_t *dram, uint64_t addr, uint64_t size, uint64_t value);
uint64_t dram_load_bin(dram_t *dram, uint64_t addr, void *data, uint64_t datalen);

void cpu_init(struct cpu_t *cpu);
uint32_t cpu_fetch(struct cpu_t *cpu);
int cpu_execute(struct cpu_t *cpu, uint32_t inst);
#endif
