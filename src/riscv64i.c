#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbg.h"
#include "librv64i.h"
#include "syscall.h"

long read_file(cpu_t *cpu, const char *filename) {
    FILE *file;
    uint8_t *buffer;
    long fileLen;

    // Open file
    file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Unable to open file %s\n", filename);
        return -1;
    }
    // Get file length
    if (fseek(file, 0, SEEK_END) || (fileLen = ftell(file)) < 0) {
        fprintf(stderr, "Unable to determine size of %s\n", filename);
        fclose(file);
        return -1;
    }
    fseek(file, 0, SEEK_SET);

    if (fileLen > DRAM_SIZE) {
        fprintf(stderr, "File %s does not fit in %d bytes of DRAM\n", filename, DRAM_SIZE);
        fclose(file);
        return -1;
    }

    // Allocate memory
    buffer = (uint8_t *)malloc(fileLen + 1);
    if (!buffer) {
        fprintf(stderr, "Memory error!");
        fclose(file);
        return -1;
    }
    // Read file contents into buffer
    if (fread(buffer, fileLen, 1, file) != 1 && fileLen != 0) {
        fprintf(stderr, "Unable to read %s\n", filename);
        free(buffer);
        fclose(file);
        return -1;
    }
    fclose(file);

    // copy the bin executable to dram
    memcpy(cpu->bus.dram.mem, buffer, fileLen * sizeof(uint8_t));
    free(buffer);
    return fileLen;
}

/* Guest process memory state (brk/mmap), paired with the cpu instance in main(). */
static sysstate_t sys_state;
cpu_t cpu;

int ECALL_cb(cpu_t *cpu, uint32_t inst) {
    (void)inst;
    return syscall_dispatch(cpu, &sys_state);
}

int EBREAK_cb(cpu_t *cpu, uint32_t inst) {
    (void)inst;
    exit((int)(int32_t)cpu->regs[10]);
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
    if (argc < 2) {
        fprintf(stderr, "usage: %s <binary>\n", argv[0]);
        return 1;
    }

    cpu_init(&cpu);

    // Read input file
    long image_size = read_file(&cpu, argv[1]);
    if (image_size < 0) {
        DBG("LOAD FILE FAILED");
        return 1;
    }
    syscall_init(&sys_state, (uint64_t)image_size);

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
