#ifndef LIBRISC_H
#define LIBRISC_H

#include <stdint.h>

// 1 MiB DRAM
#define DRAM_SIZE 1024 * 1024 * 1
#define DRAM_BASE 0x00000000

typedef struct dram_t {
    uint8_t mem[DRAM_SIZE];
} dram_t;

typedef struct bus_t {
    struct dram_t dram;
} bus_t;

typedef struct cpu_t {
    uint64_t regs[32]; // 32 64-bit registers (x0-x31)
    uint64_t pc;       // 64-bit program counter
    struct bus_t bus;  // cpu_t connected to bus_t
} cpu_t;

uint64_t dram_load(dram_t *dram, uint64_t addr, uint64_t size);
void dram_store(dram_t *dram, uint64_t addr, uint64_t size, uint64_t value);
uint64_t dram_load_bin(dram_t *dram, uint64_t addr, void *data, uint64_t datalen);

void cpu_init(struct cpu_t *cpu);
uint32_t cpu_fetch(struct cpu_t *cpu);
int cpu_execute(struct cpu_t *cpu, uint32_t inst);

extern void ECALL_cb(cpu_t *cpu, uint32_t inst);
extern void EBREAK_cb(cpu_t *cpu, uint32_t inst);
extern void INVOP_cb(cpu_t *cpu, uint32_t inst);
#endif
