#include "gdbstub.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char hex_digit(int v) { return (v < 10) ? (char)('0' + v) : (char)('a' + v - 10); }

static void hex_encode(char *out, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = hex_digit((data[i] >> 4) & 0xf);
        out[i * 2 + 1] = hex_digit(data[i] & 0xf);
    }
    out[len * 2] = '\0';
}

static int hex_decode(uint8_t *out, const char *hex, size_t len) {
    for (size_t i = 0; i < len; i++) {
        int hi = hex_val(hex[i * 2]);
        int lo = hex_val(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 0;
}

static int send_raw(gdbstub_t *g, const char *data, size_t len) {
    const char *p = data;
    while (len > 0) {
        ssize_t n = send(g->conn_fd, p, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        len -= (size_t)n;
    }
    return 0;
}

static int send_packet(gdbstub_t *g, const char *payload) {
    char buf[4096];
    unsigned char csum = 0;
    size_t plen = strlen(payload);
    for (size_t i = 0; i < plen; i++) csum += (unsigned char)payload[i];
    int n = snprintf(buf, sizeof(buf), "$%s#%02x", payload, csum);
    if (n < 0 || (size_t)n >= sizeof(buf)) return -1;
    return send_raw(g, buf, (size_t)n);
}

static int recv_byte(gdbstub_t *g, int timeout_ms) {
    struct pollfd pfd;
    pfd.fd = g->conn_fd;
    pfd.events = POLLIN;
    int r = poll(&pfd, 1, timeout_ms);
    if (r <= 0) return -1;
    unsigned char c;
    ssize_t n = recv(g->conn_fd, &c, 1, 0);
    if (n <= 0) return -1;
    return (int)c;
}

static int recv_packet(gdbstub_t *g, char *buf, size_t bufsz, int timeout_ms) {
    int c;
    do {
        c = recv_byte(g, timeout_ms);
        if (c < 0) return -1;
        if (c == 0x03) { /* Ctrl-C interrupt */
            buf[0] = '\x03';
            buf[1] = '\0';
            return 1;
        }
    } while (c != '$');

    size_t len = 0;
    unsigned char csum = 0;
    for (;;) {
        c = recv_byte(g, 1000);
        if (c < 0) return -1;
        if (c == '#') break;
        if (c == '}') { /* escape */
            int e = recv_byte(g, 1000);
            if (e < 0) return -1;
            c = e ^ 0x20;
        }
        csum += (unsigned char)c;
        if (len + 1 < bufsz) buf[len++] = (char)c;
    }
    buf[len] = '\0';
    int h1 = recv_byte(g, 1000);
    int h2 = recv_byte(g, 1000);
    if (h1 < 0 || h2 < 0) return -1;
    int pkt_csum = (hex_val((char)h1) << 4) | hex_val((char)h2);
    if (pkt_csum < 0 || (unsigned char)pkt_csum != csum) {
        send_raw(g, "-", 1);
        return 0;
    }
    send_raw(g, "+", 1);
    return 1;
}

static int mem_read_byte(cpu_t *cpu, uint64_t addr, uint8_t *out) {
    if (addr < DRAM_BASE || addr >= DRAM_BASE + DRAM_SIZE) return -1;
    *out = cpu->bus.dram.mem[addr - DRAM_BASE];
    return 0;
}

static int mem_write_byte(cpu_t *cpu, uint64_t addr, uint8_t val) {
    if (addr < DRAM_BASE || addr >= DRAM_BASE + DRAM_SIZE) return -1;
    cpu->bus.dram.mem[addr - DRAM_BASE] = val;
    return 0;
}

int gdbstub_has_breakpoint(gdbstub_t *g, uint64_t pc) {
    for (int i = 0; i < g->bp_count; i++) {
        if (g->breakpoints[i] == pc) return 1;
    }
    return 0;
}

static int add_breakpoint(gdbstub_t *g, uint64_t addr) {
    if (gdbstub_has_breakpoint(g, addr)) return 0;
    if (g->bp_count >= GDBSTUB_MAX_BPS) return -1;
    g->breakpoints[g->bp_count++] = addr;
    return 0;
}

static int remove_breakpoint(gdbstub_t *g, uint64_t addr) {
    for (int i = 0; i < g->bp_count; i++) {
        if (g->breakpoints[i] == addr) {
            g->breakpoints[i] = g->breakpoints[--g->bp_count];
            return 0;
        }
    }
    return -1;
}

static void handle_query(gdbstub_t *g, const char *p) {
    if (strncmp(p, "qSupported", 10) == 0) {
        send_packet(g, "PacketSize=1000;swbreak+;qXfer:features:read-");
    } else if (strcmp(p, "qAttached") == 0) {
        send_packet(g, "1");
    } else if (strcmp(p, "qC") == 0) {
        send_packet(g, "QC1");
    } else if (strcmp(p, "qfThreadInfo") == 0) {
        send_packet(g, "m1");
    } else if (strcmp(p, "qsThreadInfo") == 0) {
        send_packet(g, "l");
    } else if (strcmp(p, "qOffsets") == 0) {
        send_packet(g, "Text=0;Data=0;Bss=0");
    } else if (strncmp(p, "qSymbol", 7) == 0) {
        send_packet(g, "OK");
    } else if (strncmp(p, "qXfer:features:read:target.xml:0,", 33) == 0) {
        /* Minimal target description so GDB knows it's RV64I */
        static const char xml[] =
            "l<?xml version=\"1.0\"?>"
            "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">"
            "<target><architecture>riscv:rv64</architecture><osabi>none</osabi>"
            "<feature name=\"org.gnu.gdb.riscv.cpu\">"
            "<reg name=\"x0\" bitsize=\"64\" type=\"int\" regnum=\"0\"/>"
            "<reg name=\"ra\" bitsize=\"64\" type=\"code_ptr\" regnum=\"1\"/>"
            "<reg name=\"sp\" bitsize=\"64\" type=\"data_ptr\" regnum=\"2\"/>"
            "<reg name=\"gp\" bitsize=\"64\" type=\"data_ptr\" regnum=\"3\"/>"
            "<reg name=\"tp\" bitsize=\"64\" type=\"data_ptr\" regnum=\"4\"/>"
            "<reg name=\"t0\" bitsize=\"64\" type=\"int\" regnum=\"5\"/>"
            "<reg name=\"t1\" bitsize=\"64\" type=\"int\" regnum=\"6\"/>"
            "<reg name=\"t2\" bitsize=\"64\" type=\"int\" regnum=\"7\"/>"
            "<reg name=\"fp\" bitsize=\"64\" type=\"data_ptr\" regnum=\"8\"/>"
            "<reg name=\"s1\" bitsize=\"64\" type=\"int\" regnum=\"9\"/>"
            "<reg name=\"a0\" bitsize=\"64\" type=\"int\" regnum=\"10\"/>"
            "<reg name=\"a1\" bitsize=\"64\" type=\"int\" regnum=\"11\"/>"
            "<reg name=\"a2\" bitsize=\"64\" type=\"int\" regnum=\"12\"/>"
            "<reg name=\"a3\" bitsize=\"64\" type=\"int\" regnum=\"13\"/>"
            "<reg name=\"a4\" bitsize=\"64\" type=\"int\" regnum=\"14\"/>"
            "<reg name=\"a5\" bitsize=\"64\" type=\"int\" regnum=\"15\"/>"
            "<reg name=\"a6\" bitsize=\"64\" type=\"int\" regnum=\"16\"/>"
            "<reg name=\"a7\" bitsize=\"64\" type=\"int\" regnum=\"17\"/>"
            "<reg name=\"s2\" bitsize=\"64\" type=\"int\" regnum=\"18\"/>"
            "<reg name=\"s3\" bitsize=\"64\" type=\"int\" regnum=\"19\"/>"
            "<reg name=\"s4\" bitsize=\"64\" type=\"int\" regnum=\"20\"/>"
            "<reg name=\"s5\" bitsize=\"64\" type=\"int\" regnum=\"21\"/>"
            "<reg name=\"s6\" bitsize=\"64\" type=\"int\" regnum=\"22\"/>"
            "<reg name=\"s7\" bitsize=\"64\" type=\"int\" regnum=\"23\"/>"
            "<reg name=\"s8\" bitsize=\"64\" type=\"int\" regnum=\"24\"/>"
            "<reg name=\"s9\" bitsize=\"64\" type=\"int\" regnum=\"25\"/>"
            "<reg name=\"s10\" bitsize=\"64\" type=\"int\" regnum=\"26\"/>"
            "<reg name=\"s11\" bitsize=\"64\" type=\"int\" regnum=\"27\"/>"
            "<reg name=\"t3\" bitsize=\"64\" type=\"int\" regnum=\"28\"/>"
            "<reg name=\"t4\" bitsize=\"64\" type=\"int\" regnum=\"29\"/>"
            "<reg name=\"t5\" bitsize=\"64\" type=\"int\" regnum=\"30\"/>"
            "<reg name=\"t6\" bitsize=\"64\" type=\"int\" regnum=\"31\"/>"
            "<reg name=\"pc\" bitsize=\"64\" type=\"code_ptr\" regnum=\"32\"/>"
            "</feature></target>";
        send_packet(g, xml);
    } else {
        send_packet(g, "");
    }
}

static int handle_packet(gdbstub_t *g, cpu_t *cpu, const char *buf) {
    if (buf[0] == '\x03') {
        g->halt = 1;
        send_packet(g, "S05");
        return 0;
    }
    switch (buf[0]) {
    case '?':
        send_packet(g, "S05");
        return 0;
    case 'g': {
        char out[33 * 16 + 1];
        for (int i = 0; i < 32; i++) {
            uint64_t v = cpu->regs[i];
            uint8_t b[8];
            for (int j = 0; j < 8; j++) b[j] = (uint8_t)(v >> (j * 8));
            hex_encode(out + i * 16, b, 8);
        }
        {
            uint64_t v = cpu->pc;
            uint8_t b[8];
            for (int j = 0; j < 8; j++) b[j] = (uint8_t)(v >> (j * 8));
            hex_encode(out + 32 * 16, b, 8);
        }
        out[33 * 16] = '\0';
        send_packet(g, out);
        return 0;
    }
    case 'G': {
        const char *hex = buf + 1;
        if (strlen(hex) < 33 * 16) { send_packet(g, "E01"); return 0; }
        for (int i = 0; i < 32; i++) {
            uint8_t b[8];
            if (hex_decode(b, hex + i * 16, 8) < 0) { send_packet(g, "E01"); return 0; }
            uint64_t v = 0;
            for (int j = 0; j < 8; j++) v |= ((uint64_t)b[j]) << (j * 8);
            if (i != 0) cpu->regs[i] = v;
        }
        {
            uint8_t b[8];
            if (hex_decode(b, hex + 32 * 16, 8) < 0) { send_packet(g, "E01"); return 0; }
            uint64_t v = 0;
            for (int j = 0; j < 8; j++) v |= ((uint64_t)b[j]) << (j * 8);
            cpu->pc = v;
        }
        send_packet(g, "OK");
        return 0;
    }
    case 'm': {
        uint64_t addr = 0, len = 0;
        if (sscanf(buf + 1, "%lx,%lx", &addr, &len) != 2) { send_packet(g, "E01"); return 0; }
        if (len > 1024) len = 1024;
        char out[2049];
        uint8_t tmp[1024];
        for (uint64_t i = 0; i < len; i++) {
            if (mem_read_byte(cpu, addr + i, &tmp[i]) < 0) { send_packet(g, "E14"); return 0; }
        }
        hex_encode(out, tmp, (size_t)len);
        send_packet(g, out);
        return 0;
    }
    case 'M': {
        uint64_t addr = 0, len = 0;
        const char *colon = strchr(buf, ':');
        if (!colon || sscanf(buf + 1, "%lx,%lx", &addr, &len) != 2) { send_packet(g, "E01"); return 0; }
        const char *hex = colon + 1;
        if (strlen(hex) < len * 2) { send_packet(g, "E01"); return 0; }
        for (uint64_t i = 0; i < len; i++) {
            uint8_t b;
            if (hex_decode(&b, hex + i * 2, 1) < 0) { send_packet(g, "E01"); return 0; }
            if (mem_write_byte(cpu, addr + i, b) < 0) { send_packet(g, "E14"); return 0; }
        }
        send_packet(g, "OK");
        return 0;
    }
    case 'c':
        if (buf[1] != '\0') {
            uint64_t addr = strtoull(buf + 1, NULL, 16);
            cpu->pc = addr;
        }
        g->halt = 0;
        g->step = 0;
        return 1;
    case 's':
        if (buf[1] != '\0') {
            uint64_t addr = strtoull(buf + 1, NULL, 16);
            cpu->pc = addr;
        }
        g->halt = 0;
        g->step = 1;
        return 2;
    case 'Z':
        if (buf[1] == '0') {
            uint64_t addr = 0;
            sscanf(buf + 2, ",%lx", &addr);
            if (add_breakpoint(g, addr) < 0) send_packet(g, "E22");
            else send_packet(g, "OK");
        } else {
            send_packet(g, "");
        }
        return 0;
    case 'z':
        if (buf[1] == '0') {
            uint64_t addr = 0;
            sscanf(buf + 2, ",%lx", &addr);
            remove_breakpoint(g, addr);
            send_packet(g, "OK");
        } else {
            send_packet(g, "");
        }
        return 0;
    case 'H':
        send_packet(g, "OK");
        return 0;
    case 'D':
        send_packet(g, "OK");
        g->halt = 0;
        g->attached = 0;
        return -1;
    case 'k':
        g->halt = 0;
        g->attached = 0;
        return -2;
    case 'v':
        if (strcmp(buf, "vCont?") == 0) {
            send_packet(g, "vCont;c;s");
            return 0;
        }
        if (strncmp(buf, "vCont;", 6) == 0) {
            if (strchr(buf, 's')) {
                g->halt = 0; g->step = 1; return 2;
            }
            g->halt = 0; g->step = 0; return 1;
        }
        send_packet(g, "");
        return 0;
    case 'q':
        handle_query(g, buf);
        return 0;
    default:
        send_packet(g, "");
        return 0;
    }
}

int gdbstub_init(gdbstub_t *g, int port) {
    memset(g, 0, sizeof(*g));
    g->listen_fd = -1;
    g->conn_fd = -1;
    g->port = port;
    g->enabled = 1;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((uint16_t)port);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    if (listen(fd, 1) < 0) { close(fd); return -1; }
    g->listen_fd = fd;
    fprintf(stderr, "[gdbstub] listening on 127.0.0.1:%d\n", port);
    return 0;
}

int gdbstub_wait(gdbstub_t *g) {
    if (!g->enabled || g->listen_fd < 0) return -1;
    struct sockaddr_in caddr;
    socklen_t clen = sizeof(caddr);
    int cfd = accept(g->listen_fd, (struct sockaddr *)&caddr, &clen);
    if (cfd < 0) return -1;
    g->conn_fd = cfd;
    g->attached = 1;
    g->halt = 1;
    g->step = 0;
    fprintf(stderr, "[gdbstub] connected\n");
    return 0;
}

void gdbstub_notify_exit(gdbstub_t *g, int code) {
    if (!g->enabled || !g->attached || g->conn_fd < 0) return;
    char buf[16];
    snprintf(buf, sizeof(buf), "W%02x", (unsigned char)code);
    send_packet(g, buf);
}

void gdbstub_close(gdbstub_t *g) {
    if (g->conn_fd >= 0) { close(g->conn_fd); g->conn_fd = -1; }
    if (g->listen_fd >= 0) { close(g->listen_fd); g->listen_fd = -1; }
    g->attached = 0;
    g->enabled = 0;
}

static int poll_interrupt(gdbstub_t *g) {
    struct pollfd pfd;
    pfd.fd = g->conn_fd;
    pfd.events = POLLIN;
    int r = poll(&pfd, 1, 0);
    if (r > 0 && (pfd.revents & POLLIN)) {
        unsigned char c;
        ssize_t n = recv(g->conn_fd, &c, 1, MSG_PEEK);
        if (n > 0 && c == 0x03) {
            recv(g->conn_fd, &c, 1, 0);
            return 1;
        }
    }
    return 0;
}

static int command_loop(gdbstub_t *g, cpu_t *cpu) {
    char buf[4096];
    for (;;) {
        int r = recv_packet(g, buf, sizeof(buf), -1);
        if (r < 0) { g->attached = 0; return -1; }
        if (r == 0) continue;
        int action = handle_packet(g, cpu, buf);
        if (action == 1 || action == 2) return 0;
        if (action == -1) return -1; /* detach */
        if (action == -2) return -2; /* kill */
    }
}

int gdbstub_stop(gdbstub_t *g, cpu_t *cpu, int sig) {
    if (!g->enabled || !g->attached) return 0;
    char buf[16];
    snprintf(buf, sizeof(buf), "S%02x", sig & 0xff);
    g->halt = 1;
    send_packet(g, buf);
    return command_loop(g, cpu);
}

int gdbstub_tick(gdbstub_t *g, cpu_t *cpu) {
    if (!g->enabled || !g->attached) return 0;

    if (!g->halt) {
        if (poll_interrupt(g))
            return gdbstub_stop(g, cpu, 5);
        return 0;
    }

    return command_loop(g, cpu);
}
