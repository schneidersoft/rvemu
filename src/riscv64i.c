#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbg.h"
#include "librv64i.h"

int read_file(cpu_t *cpu, char *filename) {
    FILE *file;
    uint8_t *buffer;
    unsigned long fileLen;

    // Open file
    file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Unable to open file %s", filename);
        return -1;
    }
    // Get file length
    fseek(file, 0, SEEK_END);
    fileLen = ftell(file);
    fseek(file, 0, SEEK_SET);

    // Allocate memory
    buffer = (uint8_t *)malloc(fileLen + 1);
    if (!buffer) {
        fprintf(stderr, "Memory error!");
        fclose(file);
        return -1;
    }
    // Read file contents into buffer
    fread(buffer, fileLen, 1, file);
    fclose(file);

    // copy the bin executable to dram
    memcpy(cpu->bus.dram.mem, buffer, fileLen * sizeof(uint8_t));
    free(buffer);
    return 0;
}

void print_BUS_safe(struct cpu_t *cpu, uint64_t addr) {
    // addr &= 0xffffffff;
    fprintf(stderr, "::");
    while (1) {
        if (addr < DRAM_BASE) {
            break;
        }
        if (addr >= DRAM_BASE + DRAM_SIZE) {
            break;
        }

        int c = cpu->bus.dram.mem[(addr - DRAM_BASE)];
        putc(c, stderr);
        addr++;
        if (c == '\0')
            break;
    }
    putc('\n', stderr);
}

int ECALL_cb(cpu_t *cpu, uint32_t inst) {
    switch (cpu->regs[10]) {
    case 0: print_BUS_safe(cpu, cpu->regs[11]); return 0;
    case 1: exit(0); return 0;
    case 2: fputc(cpu->regs[11], stderr); return 0;
    default:
        return -1;
    }
}

int EBREAK_cb(cpu_t *cpu, uint32_t inst) {
    exit(0);
    return 0;
}

int INVOP_cb(cpu_t *cpu, uint32_t inst) {
    int opcode = inst & 0x7f;         // opcode in bits 6..0
    int funct3 = (inst >> 12) & 0x7;  // funct3 in bits 14..12
    int funct7 = (inst >> 25) & 0x7f; // funct7 in bits 31..25
    DBG("%016lx [-] ERROR-> 0x%08x opcode:0x%x, funct3:0x%x, funct7:0x%x\n", cpu->pc - 4, inst, opcode, funct3, funct7);
    return -1;
}

int main(int argc, char **argv) {
    cpu_t cpu;
    cpu_init(&cpu);

    // Read input file
    if (read_file(&cpu, argv[1])) {
        DBG("LOAD FILE FAILED");
        return -1;
    }

    // cpu loop
    do {
        uint32_t inst = cpu_fetch(&cpu);

        if (cpu_execute(&cpu, inst)) {
            DBG("execute error");
            break;
        }

    } while (1);

    return 0;
}
