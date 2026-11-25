#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbg.h"
#include "gdbstub.h"
#include "librv64i.h"
#include "syscall.h"

uint64_t read_file(cpu_t *cpu, const char *filename) {
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
    return (uint64_t)fileLen;
}

/* Guest process memory state (brk/mmap), paired with the cpu instance in main(). */
static sysstate_t sys;
static cpu_t cpu;
static int guest_exited = 0;    // set on a graceful guest exit (exit syscall or ebreak)
static int guest_exit_code = 0; // exit status reported back from the guest

// Every termination path reports through here, sets the guest's exit status,
// and returns non-zero to halt the run loop — the host process is only ever
// ended by main() returning, never by the core or syscall layers.
// (a0/regs[10] carries the status for both the exit syscall and ebreak.)
static int end_guest(cpu_t *cpu, const char *via) {
    guest_exited = 1;
    guest_exit_code = (int)(int32_t)cpu->regs[10];
    printf("[host] guest exited (%s) with code %d\n", via, guest_exit_code);
    fflush(stdout);
    return 1;
}

static int app_exit(cpu_t *cpu, sysstate_t *s, const uint64_t args[6]) {
    (void)s;
    (void)args;
    return end_guest(cpu, "exit");
}

static int ecall_cb(cpu_t *cpu, uint32_t inst) {
    (void)inst;
    return syscall_dispatch(cpu, &sys);
}

static int ebreak_cb(cpu_t *cpu, uint32_t inst) {
    (void)inst;
    return end_guest(cpu, "ebreak");
}

static int invop_cb(cpu_t *cpu, uint32_t inst) {
    int opcode = inst & 0x7f;         // opcode in bits 6..0
    int funct3 = (inst >> 12) & 0x7;  // funct3 in bits 14..12
    int funct7 = (inst >> 25) & 0x7f; // funct7 in bits 31..25
    DBG("%016lx [-] ERROR-> 0x%08x opcode:0x%x, funct3:0x%x, funct7:0x%x\n", cpu->pc - 4, inst, opcode, funct3, funct7);
    return -1;
}

int main(int argc, char **argv) {
    const char *bin = NULL;
    int gdb_port = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-g") == 0 && i + 1 < argc) {
            gdb_port = atoi(argv[++i]);
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return 1;
        } else {
            bin = argv[i];
        }
    }

    if (!bin) {
        fprintf(stderr, "usage: %s [-g port] <binary>\n", argv[0]);
        return 1;
    }

    cpu_init(&cpu);
    cpu.ecall_cb = ecall_cb;
    cpu.ebreak_cb = ebreak_cb;
    cpu.invop_cb = invop_cb;

    // Read input file
    uint64_t image_size = read_file(&cpu, bin);
    if (image_size < 0) {
        DBG("LOAD FILE FAILED");
        return 1;
    }
    syscall_init(&sys, image_size);

    // Override the guest's termination path: report the exit to the host and
    // stop gracefully instead of the table default, which kills the whole process.
    sys.syscalls[RV_SYS_exit] = app_exit;
    sys.syscalls[RV_SYS_exit_group] = app_exit;

    gdbstub_t gdb;
    memset(&gdb, 0, sizeof(gdb));
    gdb.listen_fd = -1;
    gdb.conn_fd = -1;
    if (gdb_port >= 0) {
        if (gdbstub_init(&gdb, gdb_port) < 0) {
            fprintf(stderr, "failed to init gdbstub on port %d\n", gdb_port);
            return 1;
        }
        if (gdbstub_wait(&gdb) < 0) {
            fprintf(stderr, "failed to accept gdb connection\n");
            gdbstub_close(&gdb);
            return 1;
        }
    }

    uint64_t skip_bp = 0;
    int skip_bp_active = 0;
    for (;;) {
        if (gdb.enabled && gdb.attached) {
            int gr = gdbstub_tick(&gdb, &cpu);
            if (gr == -2) { gdbstub_close(&gdb); return 1; }
            if (gdb.attached && !gdb.halt && !gdb.step && gdbstub_has_breakpoint(&gdb, cpu.pc)) {
                if (skip_bp_active && skip_bp == cpu.pc) {
                    skip_bp_active = 0;
                } else {
                    gr = gdbstub_stop(&gdb, &cpu, 5);
                    if (gr == -2) { gdbstub_close(&gdb); return 1; }
                    if (!gdb.step) { skip_bp = cpu.pc; skip_bp_active = 1; }
                }
            } else if (skip_bp_active && skip_bp != cpu.pc) {
                skip_bp_active = 0;
            }
        }

        int was_step = (gdb.enabled && gdb.attached && gdb.step && !gdb.halt);
        uint32_t inst = cpu_fetch(&cpu);
        int ex = cpu_execute(&cpu, inst);

        if (ex) {
            if (gdb.enabled && gdb.attached)
                gdbstub_notify_exit(&gdb, guest_exit_code);
            if (!guest_exited) {
                DBG("execute error: guest did not exit");
                if (gdb.enabled) gdbstub_close(&gdb);
                return 1;
            }
            break;
        }

        if (was_step && gdb.enabled && gdb.attached) {
            gdb.step = 0;
            int gr = gdbstub_stop(&gdb, &cpu, 5);
            if (gr == -2) { gdbstub_close(&gdb); return 1; }
        }
    }

    if (gdb.enabled) gdbstub_close(&gdb);
    return guest_exit_code;
}
