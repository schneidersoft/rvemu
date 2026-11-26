#ifndef GDBSTUB_H
#define GDBSTUB_H

#include <stdint.h>

#include "librv64i.h"

#define GDBSTUB_MAX_BPS 64
#define GDBSTUB_RXBUF 4096

typedef struct gdbstub_t {
    int listen_fd;
    int conn_fd;
    int port;
    int enabled;
    int attached;
    int halt;
    int step;
    int stop_reason_sent;
    int halt_on_detach;
    uint64_t skip_bp;
    int skip_bp_active;
    uint64_t breakpoints[GDBSTUB_MAX_BPS];
    int bp_count;
    unsigned char rxbuf[GDBSTUB_RXBUF];
    int rxlen;
} gdbstub_t;

int gdbstub_init(gdbstub_t *g, int port);
void gdbstub_close(gdbstub_t *g);
int gdbstub_wait(gdbstub_t *g);
int gdbstub_accept(gdbstub_t *g);
int gdbstub_command_loop(gdbstub_t *g, cpu_t *cpu);
void gdbstub_stop(gdbstub_t *g, int sig);
int gdbstub_has_breakpoint(gdbstub_t *g, uint64_t pc);
void gdbstub_notify_exit(gdbstub_t *g, int code);

#endif
