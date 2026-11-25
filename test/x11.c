/*
 * Minimal raw X11 window example for rvemu guest.
 *
 * Zero external dependencies: speaks X11 wire protocol over a Unix-domain
 * socket using guest libc/socket syscalls proxied by rvemu.
 *
 * Build:  make bin/x11.bin
 * Run:    ./bin/riscv64i bin/x11.bin
 *
 * Demonstrates: setup, CreateWindow/CreateGC/MapWindow, WM_NAME,
 * WM_DELETE_WINDOW, Expose drawing (rectangles + ImageText8), KeyPress exit.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static int xfd = -1;

static int xwrite_all(const void *buf, size_t n) {
    const uint8_t *p = (const uint8_t *)buf;
    while (n > 0) {
        ssize_t r = write(xfd, p, n);
        if (r <= 0) return -1;
        p += (size_t)r;
        n -= (size_t)r;
    }
    return 0;
}

static int xread_all(void *buf, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    while (n > 0) {
        ssize_t r = read(xfd, p, n);
        if (r <= 0) return -1;
        p += (size_t)r;
        n -= (size_t)r;
    }
    return 0;
}

static void put_u16(uint8_t *b, uint16_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); }
static void put_u32(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
}
static uint16_t get_u16(const uint8_t *b) { return (uint16_t)b[0] | ((uint16_t)b[1] << 8); }
static uint32_t get_u32(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}
static size_t xpad_len(size_t n) { return (4 - (n & 3)) & 3; }

static int xwrite_pad(const void *buf, size_t n) {
    static const uint8_t z[3] = {0, 0, 0};
    if (xwrite_all(buf, n) < 0) return -1;
    size_t p = xpad_len(n);
    return p ? xwrite_all(z, p) : 0;
}

/* InternAtom (opcode 16): returns atom via reply. only-if-exists=0. */
static uint32_t intern_atom(const char *name) {
    uint8_t req[64];
    uint8_t reply[32];
    size_t n = strlen(name);
    size_t len = 2 + (n + xpad_len(n)) / 4;
    if (len * 4 > sizeof(req)) return 0;
    memset(req, 0, len * 4);
    req[0] = 16; req[1] = 0; /* only-if-exists = False */
    put_u16(req + 2, (uint16_t)len);
    put_u16(req + 4, (uint16_t)n); put_u16(req + 6, 0);
    memcpy(req + 8, name, n);
    if (xwrite_all(req, len * 4) < 0) return 0;
    if (xread_all(reply, 32) < 0) return 0;
    if (reply[0] != 1) return 0; /* not a reply */
    return get_u32(reply + 8);
}

/* ChangeProperty (opcode 18), format=32, mode=Replace(0). */
static int change_property32(uint32_t win, uint32_t prop, uint32_t type,
                             const uint32_t *vals, uint32_t nvals) {
    uint8_t hdr[24];
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 18; hdr[1] = 0; /* Replace */
    put_u16(hdr + 2, (uint16_t)(6 + nvals));
    put_u32(hdr + 4, win);
    put_u32(hdr + 8, prop);
    put_u32(hdr + 12, type);
    hdr[16] = 32;
    put_u32(hdr + 20, nvals);
    if (xwrite_all(hdr, 24) < 0) return -1;
    for (uint32_t i = 0; i < nvals; i++) {
        uint8_t v[4]; put_u32(v, vals[i]);
        if (xwrite_all(v, 4) < 0) return -1;
    }
    return 0;
}

/* ChangeProperty (opcode 18), format=8, mode=Replace(0). */
static int change_property8(uint32_t win, uint32_t prop, uint32_t type,
                            const char *data, uint32_t n) {
    uint8_t hdr[24];
    size_t len = 6 + (n + xpad_len(n)) / 4;
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 18; hdr[1] = 0;
    put_u16(hdr + 2, (uint16_t)len);
    put_u32(hdr + 4, win);
    put_u32(hdr + 8, prop);
    put_u32(hdr + 12, type);
    hdr[16] = 8;
    put_u32(hdr + 20, n);
    if (xwrite_all(hdr, 24) < 0) return -1;
    return xwrite_pad(data, n);
}

static int send_poly_fill_rect(uint32_t drawable, uint32_t gc,
                               uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint8_t req[20];
    memset(req, 0, sizeof(req));
    req[0] = 70; /* PolyFillRectangle */
    put_u16(req + 2, 5);
    put_u32(req + 4, drawable); put_u32(req + 8, gc);
    put_u16(req + 12, x); put_u16(req + 14, y);
    put_u16(req + 16, w); put_u16(req + 18, h);
    return xwrite_all(req, 20);
}

/* ImageText8 (opcode 76): byte1 = string length. */
static int send_image_text8(uint32_t drawable, uint32_t gc,
                            uint16_t x, uint16_t y, const char *s) {
    uint8_t hdr[16];
    size_t n = strlen(s);
    if (n > 255) n = 255;
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 76; hdr[1] = (uint8_t)n;
    put_u16(hdr + 2, (uint16_t)(4 + (n + xpad_len(n)) / 4));
    put_u32(hdr + 4, drawable); put_u32(hdr + 8, gc);
    put_u16(hdr + 12, x); put_u16(hdr + 14, y);
    if (xwrite_all(hdr, 16) < 0) return -1;
    return xwrite_pad(s, n);
}

int main(void) {
    struct sockaddr_un addr;
    uint8_t setup[12], hdr[8], *body;
    uint16_t addlen, vendor_len;
    uint8_t num_formats;
    size_t off;
    uint32_t rid_base, root, white, black, wid, gc, gc2;
    uint32_t wm_protocols = 0, wm_delete = 0, atom_wm_name = 0, atom_string = 0, atom_utf8 = 0;

    xfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (xfd < 0) { perror("socket"); return 1; }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/tmp/.X11-unix/X0");
    if (connect(xfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect /tmp/.X11-unix/X0");
        return 1;
    }

    /* X11 setup: little-endian, protocol 11.0, no auth. */
    memset(setup, 0, sizeof(setup));
    setup[0] = 'l'; setup[2] = 11;
    if (xwrite_all(setup, sizeof(setup)) < 0) return 1;
    if (xread_all(hdr, 8) < 0) return 1;
    if (hdr[0] != 1) { fprintf(stderr, "X11 setup failed (status=%u)\n", hdr[0]); return 1; }

    addlen = get_u16(hdr + 6);
    body = (uint8_t *)malloc((size_t)addlen * 4);
    if (!body) return 1;
    if (xread_all(body, (size_t)addlen * 4) < 0) return 1;

    rid_base = get_u32(body + 4);
    vendor_len = get_u16(body + 16);
    num_formats = body[21];
    off = 32 + (((size_t)vendor_len + 3) & ~(size_t)3) + (size_t)num_formats * 8;
    root = get_u32(body + off + 0);
    white = get_u32(body + off + 20);
    black = get_u32(body + off + 24);

    wid = rid_base;
    gc = rid_base + 1;
    gc2 = rid_base + 2;

    /* CreateWindow */
    {
        uint8_t req[40];
        uint32_t mask = 0x0002 /*bgPixel*/ | 0x0800 /*eventMask*/;
        memset(req, 0, sizeof(req));
        req[0] = 1; put_u16(req + 2, 10);
        put_u32(req + 4, wid); put_u32(req + 8, root);
        put_u16(req + 12, 100); put_u16(req + 14, 100);
        put_u16(req + 16, 420); put_u16(req + 18, 260);
        put_u16(req + 22, 1); /* InputOutput */
        put_u32(req + 28, mask);
        put_u32(req + 32, 0xFF1E1E2E); /* dark background */
        put_u32(req + 36, 0x0001 /*KeyPress*/ | 0x8000 /*Exposure*/ | 0x20000 /*StructureNotify*/);
        if (xwrite_all(req, sizeof(req)) < 0) return 1;
    }

    /* GC for white foreground text/shapes */
    {
        uint8_t req[20];
        memset(req, 0, sizeof(req));
        req[0] = 55; put_u16(req + 2, 5);
        put_u32(req + 4, gc); put_u32(req + 8, wid);
        put_u32(req + 12, 0x0004); /* foreground */
        put_u32(req + 16, white);
        if (xwrite_all(req, sizeof(req)) < 0) return 1;
    }
    /* GC for accent color */
    {
        uint8_t req[20];
        memset(req, 0, sizeof(req));
        req[0] = 55; put_u16(req + 2, 5);
        put_u32(req + 4, gc2); put_u32(req + 8, wid);
        put_u32(req + 12, 0x0004);
        put_u32(req + 16, 0xFF89B4FA);
        if (xwrite_all(req, sizeof(req)) < 0) return 1;
    }

    /* Window manager properties: name and WM_DELETE_WINDOW protocol. */
    atom_wm_name = 39;   /* predefined WM_NAME */
    atom_string = 31;    /* predefined STRING */
    atom_utf8 = intern_atom("UTF8_STRING");
    wm_protocols = intern_atom("WM_PROTOCOLS");
    wm_delete = intern_atom("WM_DELETE_WINDOW");
    if (atom_utf8) change_property8(wid, atom_wm_name, atom_utf8, "rvemu X11", 9);
    else change_property8(wid, atom_wm_name, atom_string, "rvemu X11", 9);
    if (wm_protocols && wm_delete) change_property32(wid, wm_protocols, 4 /*ATOM*/, &wm_delete, 1);

    /* MapWindow */
    {
        uint8_t req[8];
        memset(req, 0, sizeof(req));
        req[0] = 8; put_u16(req + 2, 2); put_u32(req + 4, wid);
        if (xwrite_all(req, sizeof(req)) < 0) return 1;
    }

    printf("rvemu x11: window 0x%x mapped (press key or close to exit)\n", wid);
    fflush(stdout);

    for (;;) {
        uint8_t ev[32];
        if (xread_all(ev, 32) < 0) break;
        uint8_t type = ev[0] & 0x7f;
        if (ev[0] == 0) continue; /* error */

        if (type == 12) { /* Expose */
            /* Draw only on the last expose in a sequence (count == 0). */
            uint16_t count = get_u16(ev + 16);
            if (count != 0) continue;
            send_poly_fill_rect(wid, gc2, 20, 20, 380, 60);
            send_poly_fill_rect(wid, gc,  30, 100, 120, 80);
            send_poly_fill_rect(wid, gc2, 170, 100, 120, 80);
            send_poly_fill_rect(wid, gc,  310, 100, 80, 80);
            send_image_text8(wid, gc, 40, 55, "Hello from rvemu raw X11!");
            send_image_text8(wid, gc, 30, 210, "Press any key or close window to exit.");
        } else if (type == 2) { /* KeyPress */
            break;
        } else if (type == 33) { /* ClientMessage */
            uint32_t atom = get_u32(ev + 12);
            if (atom == wm_delete) break;
        }
    }

    { /* DestroyWindow */
        uint8_t req[8]; memset(req, 0, sizeof(req));
        req[0] = 4; put_u16(req + 2, 2); put_u32(req + 4, wid);
        xwrite_all(req, 8);
    }
    close(xfd);
    free(body);
    (void)black;
    return 0;
}
