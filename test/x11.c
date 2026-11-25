/*
 * Mini raw X11 paint example for rvemu guest.
 *
 * Zero external dependencies: speaks X11 wire protocol over a Unix-domain
 * socket using guest libc/socket syscalls proxied by rvemu.
 *
 * Build:  make bin/x11.bin
 * Run:    ./bin/riscv64i bin/x11.bin
 *
 * Controls:
 *   Left mouse drag  - draw
 *   1-6              - cur_select cur_color
 *   [ / ]            - cur_brush smaller / larger
 *   c                - clear canvas
 *   q / Esc / close  - quit
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
        if (r <= 0)
            return -1;
        p += (size_t)r;
        n -= (size_t)r;
    }
    return 0;
}

static int xread_all(void *buf, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    while (n > 0) {
        ssize_t r = read(xfd, p, n);
        if (r <= 0)
            return -1;
        p += (size_t)r;
        n -= (size_t)r;
    }
    return 0;
}

static void put_u16(uint8_t *b, uint16_t v) {
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
}
static void put_u32(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)v;
    b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16);
    b[3] = (uint8_t)(v >> 24);
}
static uint16_t get_u16(const uint8_t *b) { return (uint16_t)b[0] | ((uint16_t)b[1] << 8); }
static uint32_t get_u32(const uint8_t *b) { return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24); }
static size_t pad4(size_t n) { return (4 - (n & 3)) & 3; }

static int xwrite_pad(const void *buf, size_t n) {
    static const uint8_t z[3] = {0, 0, 0};
    if (xwrite_all(buf, n) < 0)
        return -1;
    size_t p = pad4(n);
    return p ? xwrite_all(z, p) : 0;
}

static uint32_t intern_atom(const char *name) {
    uint8_t req[64], reply[32];
    size_t n = strlen(name), len = 2 + (n + pad4(n)) / 4;
    if (len * 4 > sizeof(req))
        return 0;
    memset(req, 0, len * 4);
    req[0] = 16;
    put_u16(req + 2, (uint16_t)len);
    put_u16(req + 4, (uint16_t)n);
    memcpy(req + 8, name, n);
    if (xwrite_all(req, len * 4) < 0)
        return 0;
    if (xread_all(reply, 32) < 0 || reply[0] != 1)
        return 0;
    return get_u32(reply + 8);
}

static int change_property32(uint32_t win, uint32_t prop, uint32_t type, const uint32_t *vals, uint32_t nvals) {
    uint8_t hdr[24];
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 18;
    put_u16(hdr + 2, (uint16_t)(6 + nvals));
    put_u32(hdr + 4, win);
    put_u32(hdr + 8, prop);
    put_u32(hdr + 12, type);
    hdr[16] = 32;
    put_u32(hdr + 20, nvals);
    if (xwrite_all(hdr, 24) < 0)
        return -1;
    for (uint32_t i = 0; i < nvals; i++) {
        uint8_t v[4];
        put_u32(v, vals[i]);
        if (xwrite_all(v, 4) < 0)
            return -1;
    }
    return 0;
}

static int change_property8(uint32_t win, uint32_t prop, uint32_t type, const char *data, uint32_t n) {
    uint8_t hdr[24];
    size_t len = 6 + (n + pad4(n)) / 4;
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 18;
    put_u16(hdr + 2, (uint16_t)len);
    put_u32(hdr + 4, win);
    put_u32(hdr + 8, prop);
    put_u32(hdr + 12, type);
    hdr[16] = 8;
    put_u32(hdr + 20, n);
    if (xwrite_all(hdr, 24) < 0)
        return -1;
    return xwrite_pad(data, n);
}

static int change_gc_fg(uint32_t gc, uint32_t pixel) {
    uint8_t req[16];
    memset(req, 0, sizeof(req));
    req[0] = 56; /* ChangeGC */
    put_u16(req + 2, 4);
    put_u32(req + 4, gc);
    put_u32(req + 8, 0x0004); /* GCForeground */
    put_u32(req + 12, pixel);
    return xwrite_all(req, 16);
}

static int poly_fill_rect(uint32_t d, uint32_t gc, int16_t x, int16_t y, uint16_t w, uint16_t h) {
    uint8_t req[20];
    memset(req, 0, sizeof(req));
    req[0] = 70;
    put_u16(req + 2, 5);
    put_u32(req + 4, d);
    put_u32(req + 8, gc);
    put_u16(req + 12, (uint16_t)x);
    put_u16(req + 14, (uint16_t)y);
    put_u16(req + 16, w);
    put_u16(req + 18, h);
    return xwrite_all(req, 20);
}

static int poly_fill_arc(uint32_t d, uint32_t gc, int16_t x, int16_t y, uint16_t w, uint16_t h, int16_t a1, int16_t a2) {
    uint8_t req[28];
    memset(req, 0, sizeof(req));
    req[0] = 71;         /* PolyFillArc */
    put_u16(req + 2, 7); /* 3 words hdr + 2 words arc */
    put_u32(req + 4, d);
    put_u32(req + 8, gc);
    put_u16(req + 12, (uint16_t)x);
    put_u16(req + 14, (uint16_t)y);
    put_u16(req + 16, w);
    put_u16(req + 18, h);
    put_u16(req + 20, (uint16_t)a1);
    put_u16(req + 22, (uint16_t)a2);
    /* 12 hdr + 12 arc = 24 bytes = 6 words? Wait: request header 4 bytes + drawable 4 + gc 4 = 12, each arc 12 bytes. 12+12=24=6 words. */
    req[2] = 6;
    req[3] = 0;
    return xwrite_all(req, 24);
}

static int copy_area(uint32_t src, uint32_t dst, uint32_t gc, int16_t sx, int16_t sy, int16_t dx, int16_t dy, uint16_t w, uint16_t h) {
    uint8_t req[28];
    memset(req, 0, sizeof(req));
    req[0] = 62;
    put_u16(req + 2, 7);
    put_u32(req + 4, src);
    put_u32(req + 8, dst);
    put_u32(req + 12, gc);
    put_u16(req + 16, (uint16_t)sx);
    put_u16(req + 18, (uint16_t)sy);
    put_u16(req + 20, (uint16_t)dx);
    put_u16(req + 22, (uint16_t)dy);
    put_u16(req + 24, w);
    put_u16(req + 26, h);
    return xwrite_all(req, 28);
}

static int poly_text8(uint32_t d, uint32_t gc, int16_t x, int16_t y, const char *s) {
    uint8_t hdr[16];
    uint8_t item[2];
    size_t n = strlen(s);
    if (n > 254)
        n = 254;
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 74; /* PolyText8 */
    put_u16(hdr + 2, (uint16_t)(4 + ((2 + n + pad4(2 + n)) / 4)));
    put_u32(hdr + 4, d);
    put_u32(hdr + 8, gc);
    put_u16(hdr + 12, (uint16_t)x);
    put_u16(hdr + 14, (uint16_t)y);
    if (xwrite_all(hdr, 16) < 0)
        return -1;
    item[0] = (uint8_t)n;
    item[1] = 0;
    if (xwrite_all(item, 2) < 0)
        return -1;
    if (xwrite_all(s, n) < 0)
        return -1;
    {
        static const uint8_t z[3] = {0, 0, 0};
        size_t p = pad4(2 + n);
        if (p && xwrite_all(z, p) < 0)
            return -1;
    }
    return 0;
}

#define BAR_H 44
static int win_w = 640;
static int win_h = 400;
static int canvas_h(void) { return win_h - BAR_H; }

static uint32_t cur_color = 0xFFFFFFFF;
static int cur_brush = 4;
static int cur_sel = 0;
static int cur_eraser = 0;
static int screen_w = 640, screen_h = 480;
static uint8_t pending_ev[32];
static int has_pending_ev = 0;
static uint32_t g_wid, g_gc, g_pix, g_rid_base;
static uint8_t g_root_depth;

static int next_event(uint8_t ev[32]) {
    if (has_pending_ev) {
        memcpy(ev, pending_ev, 32);
        has_pending_ev = 0;
        return 0;
    }
    return xread_all(ev, 32);
}

static int get_geometry(uint32_t d, int *x, int *y, int *w, int *h) {
    uint8_t req[8], ev[32];
    memset(req, 0, sizeof(req));
    req[0] = 14;
    put_u16(req + 2, 2);
    put_u32(req + 4, d);
    if (xwrite_all(req, 8) < 0)
        return -1;
    for (;;) {
        if (xread_all(ev, 32) < 0)
            return -1;
        if (ev[0] == 1) { /* reply */
            if (x)
                *x = (int16_t)get_u16(ev + 12);
            if (y)
                *y = (int16_t)get_u16(ev + 14);
            if (w)
                *w = get_u16(ev + 16);
            if (h)
                *h = get_u16(ev + 18);
            return 0;
        }
        if (ev[0] == 0)
            return -1; /* error */
        /* stash event for main loop */
        if (!has_pending_ev) {
            memcpy(pending_ev, ev, 32);
            has_pending_ev = 1;
        }
        /* if already have pending, drop it (rare) */
    }
}
static int gc_set_clip_canvas(uint32_t gc) {
    uint8_t req[20];
    memset(req, 0, sizeof(req));
    req[0] = 59;         /* SetClipRectangles */
    req[1] = 0;          /* UnSorted */
    put_u16(req + 2, 5); /* 12 + 8 = 20 bytes */
    put_u32(req + 4, gc);
    put_u16(req + 8, 0);  /* clip-x-origin */
    put_u16(req + 10, 0); /* clip-y-origin */
    put_u16(req + 12, 0);
    put_u16(req + 14, (uint16_t)BAR_H);
    put_u16(req + 16, (uint16_t)win_w);
    put_u16(req + 18, (uint16_t)(win_h - BAR_H));
    return xwrite_all(req, 20);
}

static int gc_clear_clip(uint32_t gc) {
    uint8_t req[16];
    memset(req, 0, sizeof(req));
    req[0] = 56; /* ChangeGC */
    put_u16(req + 2, 4);
    put_u32(req + 4, gc);
    put_u32(req + 8, 0x80000); /* GCClipMask */
    put_u32(req + 12, 0);      /* None */
    return xwrite_all(req, 16);
}

static const uint32_t palette[] = {
    0xFFFFFFFF, /* 1 white */
    0xFFFF5555, /* 2 red */
    0xFF50FA7B, /* 3 green */
    0xFF89B4FA, /* 4 blue */
    0xFFF1FA8C, /* 5 yellow */
    0xFF000000, /* 6 black */
};
#define NPALETTE (sizeof(palette) / sizeof(palette[0]))

static void draw_dot(uint32_t d, uint32_t gc, int x, int y, int r) {
    if (r < 1)
        r = 1;
    /* PolyFillArc: angles in 1/64 degree, 0..360*64 full circle */
    poly_fill_arc(d, gc, (int16_t)(x - r), (int16_t)(y - r), (uint16_t)(r * 2), (uint16_t)(r * 2), 0, 360 * 64);
}

static void draw_brush(uint32_t win, uint32_t pix, uint32_t gc, int x, int y, int r) {
    uint32_t c = cur_eraser ? 0xFF1E1E2E : cur_color;
    change_gc_fg(gc, c);
    draw_dot(win, gc, x, y, r);
    if (pix)
        draw_dot(pix, gc, x, y, r);
}

static void draw_line_brush(uint32_t win, uint32_t pix, uint32_t gc, int x0, int y0, int x1, int y1, int r) {
    int dx = x1 - x0, dy = y1 - y0;
    int steps = dx < 0 ? -dx : dx;
    int ay = dy < 0 ? -dy : dy;
    if (ay > steps)
        steps = ay;
    if (steps == 0) {
        draw_brush(win, pix, gc, x0, y0, r);
        return;
    }
    for (int i = 0; i <= steps; i++) {
        int x = x0 + dx * i / steps;
        int y = y0 + dy * i / steps;
        draw_brush(win, pix, gc, x, y, r);
    }
}

static void draw_button(uint32_t win, uint32_t pix, uint32_t gc, int x, int y, int w, int h, uint32_t bg, int active) {
    change_gc_fg(gc, bg);
    poly_fill_rect(win, gc, (int16_t)x, (int16_t)y, (uint16_t)w, (uint16_t)h);
    if (pix)
        poly_fill_rect(pix, gc, (int16_t)x, (int16_t)y, (uint16_t)w, (uint16_t)h);
    uint32_t border = active ? 0xFFFFAA00 : 0xFF888888;
    change_gc_fg(gc, border);
    poly_fill_rect(win, gc, (int16_t)x, (int16_t)y, (uint16_t)w, 2);
    poly_fill_rect(win, gc, (int16_t)x, (int16_t)(y + h - 2), (uint16_t)w, 2);
    poly_fill_rect(win, gc, (int16_t)x, (int16_t)y, 2, (uint16_t)h);
    poly_fill_rect(win, gc, (int16_t)(x + w - 2), (int16_t)y, 2, (uint16_t)h);
    if (pix) {
        poly_fill_rect(pix, gc, (int16_t)x, (int16_t)y, (uint16_t)w, 2);
        poly_fill_rect(pix, gc, (int16_t)x, (int16_t)(y + h - 2), (uint16_t)w, 2);
        poly_fill_rect(pix, gc, (int16_t)x, (int16_t)y, 2, (uint16_t)h);
        poly_fill_rect(pix, gc, (int16_t)(x + w - 2), (int16_t)y, 2, (uint16_t)h);
    }
}

static void draw_ui(uint32_t win, uint32_t pix, uint32_t gc, uint32_t cur_color, int cur_brush, int cur_sel) {
    gc_clear_clip(gc);
    change_gc_fg(gc, 0xFF2B2B3B);
    poly_fill_rect(win, gc, 0, 0, (uint16_t)win_w, BAR_H);
    if (pix)
        poly_fill_rect(pix, gc, 0, 0, (uint16_t)win_w, BAR_H);

    for (int i = 0; i < (int)NPALETTE; i++) {
        int sx = 10 + i * 34;
        change_gc_fg(gc, palette[i]);
        poly_fill_rect(win, gc, (int16_t)sx, 8, 28, 28);
        if (pix)
            poly_fill_rect(pix, gc, (int16_t)sx, 8, 28, 28);
        if (i == cur_sel && !cur_eraser) {
            change_gc_fg(gc, 0xFFFFAA00);
            poly_fill_rect(win, gc, (int16_t)(sx - 2), 6, 32, 2);
            poly_fill_rect(win, gc, (int16_t)(sx - 2), 36, 32, 2);
            poly_fill_rect(win, gc, (int16_t)(sx - 2), 6, 2, 32);
            poly_fill_rect(win, gc, (int16_t)(sx + 30), 6, 2, 32);
            if (pix) {
                poly_fill_rect(pix, gc, (int16_t)(sx - 2), 6, 32, 2);
                poly_fill_rect(pix, gc, (int16_t)(sx - 2), 36, 32, 2);
                poly_fill_rect(pix, gc, (int16_t)(sx - 2), 6, 2, 32);
                poly_fill_rect(pix, gc, (int16_t)(sx + 30), 6, 2, 32);
            }
        }
    }

    /* Eraser button */
    draw_button(win, pix, gc, 224, 8, 52, 28, 0xFF444455, cur_eraser);
    change_gc_fg(gc, 0xFFFFFFFF);
    poly_text8(win, gc, 232, 27, "Eraser");
    if (pix)
        poly_text8(pix, gc, 232, 27, "Eraser");

    /* Brush size buttons */
    draw_button(win, pix, gc, 290, 8, 28, 28, 0xFF444455, 0);
    change_gc_fg(gc, 0xFFFFFFFF);
    poly_text8(win, gc, 300, 27, "-");
    if (pix)
        poly_text8(pix, gc, 300, 27, "-");

    char buf[16];
    snprintf(buf, sizeof(buf), "%d", cur_brush);
    change_gc_fg(gc, 0xFFCCCCCC);
    poly_text8(win, gc, 326, 27, buf);
    if (pix)
        poly_text8(pix, gc, 326, 27, buf);

    draw_button(win, pix, gc, 350, 8, 28, 28, 0xFF444455, 0);
    change_gc_fg(gc, 0xFFFFFFFF);
    poly_text8(win, gc, 360, 27, "+");
    if (pix)
        poly_text8(pix, gc, 360, 27, "+");

    change_gc_fg(gc, 0xFFAAAAAA);
    poly_text8(win, gc, 392, 27, "c:clear q:quit");
    if (pix)
        poly_text8(pix, gc, 392, 27, "c:clear q:quit");

    change_gc_fg(gc, cur_eraser ? 0xFF1E1E2E : cur_color);
}

static void clear_canvas(uint32_t win, uint32_t pix, uint32_t gc, uint32_t cur_color, int cur_brush, int cur_sel) {
    change_gc_fg(gc, 0xFF1E1E2E);
    poly_fill_rect(win, gc, 0, BAR_H, (uint16_t)win_w, (uint16_t)canvas_h());
    if (pix)
        poly_fill_rect(pix, gc, 0, BAR_H, (uint16_t)win_w, (uint16_t)canvas_h());
    draw_ui(win, pix, gc, cur_color, cur_brush, cur_sel);
}

static void apply_resize(int nw, int nh) {
    if (nw <= 0 || nh <= 0 || (nw == win_w && nh == win_h))
        return;
    int ow = win_w, oh = win_h;
    uint32_t newpix = (g_pix == g_rid_base + 2) ? g_rid_base + 3 : g_rid_base + 2;
    uint8_t req[16];
    memset(req, 0, sizeof(req));
    req[0] = 53;
    req[1] = g_root_depth;
    put_u16(req + 2, 4);
    put_u32(req + 4, newpix);
    put_u32(req + 8, g_wid);
    put_u16(req + 12, (uint16_t)nw);
    put_u16(req + 14, (uint16_t)nh);
    if (xwrite_all(req, 16) < 0)
        return;
    change_gc_fg(g_gc, 0xFF1E1E2E);
    poly_fill_rect(newpix, g_gc, 0, 0, (uint16_t)nw, (uint16_t)nh);
    if (g_pix) {
        uint16_t cw = (uint16_t)(ow < nw ? ow : nw);
        uint16_t ch = (uint16_t)(oh < nh ? oh : nh);
        copy_area(g_pix, newpix, g_gc, 0, 0, 0, 0, cw, ch);
        uint8_t fr[8];
        memset(fr, 0, sizeof(fr));
        fr[0] = 54;
        put_u16(fr + 2, 2);
        put_u32(fr + 4, g_pix);
        xwrite_all(fr, 8);
    }
    g_pix = newpix;
    win_w = nw;
    win_h = nh;
    draw_ui(g_wid, g_pix, g_gc, cur_color, cur_brush, cur_sel);
    copy_area(g_pix, g_wid, g_gc, 0, 0, 0, 0, (uint16_t)win_w, (uint16_t)win_h);
}

int main(void) {
    struct sockaddr_un addr;
    uint8_t setup[12], shdr[8], *body = NULL;
    uint16_t addlen, vendor_len;
    uint8_t num_formats, root_depth;
    size_t off;
    uint32_t rid_base, root, white, wid, gc, pix;
    uint32_t wm_protocols = 0, wm_delete = 0, atom_utf8 = 0;
    int drawing = 0, last_x = 0, last_y = 0;
    cur_color = palette[0];
    cur_brush = 4;
    cur_sel = 0;
    cur_eraser = 0;

    xfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (xfd < 0) {
        perror("socket");
        return 1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/tmp/.X11-unix/X0");
    if (connect(xfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    memset(setup, 0, sizeof(setup));
    setup[0] = 'l';
    setup[2] = 11;
    if (xwrite_all(setup, sizeof(setup)) < 0)
        return 1;
    if (xread_all(shdr, 8) < 0 || shdr[0] != 1) {
        fprintf(stderr, "X11 setup failed\n");
        return 1;
    }
    addlen = get_u16(shdr + 6);
    body = (uint8_t *)malloc((size_t)addlen * 4);
    if (!body || xread_all(body, (size_t)addlen * 4) < 0)
        return 1;
    rid_base = get_u32(body + 4);
    vendor_len = get_u16(body + 16);
    num_formats = body[21];
    off = 32 + (((size_t)vendor_len + 3) & ~(size_t)3) + (size_t)num_formats * 8;
    root = get_u32(body + off + 0);
    white = get_u32(body + off + 20);
    root_depth = body[off + 38];
    screen_w = (int)get_u16(body + off + 20);
    screen_h = (int)get_u16(body + off + 22);
    if (screen_w <= 0)
        screen_w = 640;
    if (screen_h <= 0)
        screen_h = 480;

    wid = rid_base;
    gc = rid_base + 1;
    pix = rid_base + 2;
    g_wid = wid;
    g_gc = gc;
    g_pix = pix;
    g_rid_base = rid_base;
    g_root_depth = root_depth;

    /* CreateWindow */
    {
        uint8_t req[40];
        uint32_t mask = 0x0002 | 0x0800;
        memset(req, 0, sizeof(req));
        req[0] = 1;
        put_u16(req + 2, 10);
        put_u32(req + 4, wid);
        put_u32(req + 8, root);
        put_u16(req + 12, 60);
        put_u16(req + 14, 60);
        put_u16(req + 16, win_w);
        put_u16(req + 18, win_h);
        put_u16(req + 22, 1); /* InputOutput */
        put_u32(req + 28, mask);
        put_u32(req + 32, 0xFF1E1E2E);
        /* KeyPress|ButtonPress|ButtonRelease|PointerMotion|Exposure|StructureNotify */
        put_u32(req + 36, 0x0001 | 0x0004 | 0x0008 | 0x0040 | 0x8000 | 0x20000);
        if (xwrite_all(req, sizeof(req)) < 0)
            return 1;
    }

    /* CreateGC with foreground */
    {
        uint8_t req[20];
        memset(req, 0, sizeof(req));
        req[0] = 55;
        put_u16(req + 2, 5);
        put_u32(req + 4, gc);
        put_u32(req + 8, wid);
        put_u32(req + 12, 0x0004);
        put_u32(req + 16, white);
        if (xwrite_all(req, sizeof(req)) < 0)
            return 1;
    }

    /* CreatePixmap backing store */
    {
        uint8_t req[16];
        memset(req, 0, sizeof(req));
        req[0] = 53;
        req[1] = root_depth;
        put_u16(req + 2, 4);
        put_u32(req + 4, pix);
        put_u32(req + 8, wid);
        put_u16(req + 12, win_w);
        put_u16(req + 14, win_h);
        if (xwrite_all(req, sizeof(req)) < 0) {
            pix = 0;
        }
        g_pix = pix;
    }

    /* WM properties */
    atom_utf8 = intern_atom("UTF8_STRING");
    wm_protocols = intern_atom("WM_PROTOCOLS");
    wm_delete = intern_atom("WM_DELETE_WINDOW");
    {
        const char *title = "rvemu paint";
        uint32_t name_atom = 39, str_atom = 31;
        change_property8(wid, name_atom, atom_utf8 ? atom_utf8 : str_atom, title, (uint32_t)strlen(title));
        if (wm_protocols && wm_delete)
            change_property32(wid, wm_protocols, 4, &wm_delete, 1);
    }

    /* MapWindow */
    {
        uint8_t req[8];
        memset(req, 0, sizeof(req));
        req[0] = 8;
        put_u16(req + 2, 2);
        put_u32(req + 4, wid);
        xwrite_all(req, 8);
    }

    printf("rvemu paint: drag draw, 1-6 color, e eraser, [/] size, c clear, q quit\n");
    fflush(stdout);

    {
        int gx, gy, gw, gh;
        if (get_geometry(wid, &gx, &gy, &gw, &gh) == 0) {
            apply_resize(gw, gh);
            pix = g_pix;
        }
    }
    clear_canvas(wid, pix, gc, cur_color, cur_brush, cur_sel);

    for (;;) {
        uint8_t ev[32];
        if (next_event(ev) < 0)
            break;
        uint8_t type = ev[0] & 0x7f;
        if (ev[0] == 0)
            continue;

        if (type == 12) { /* Expose */
            uint16_t count = get_u16(ev + 16);
            if (count == 0 && pix)
                copy_area(pix, wid, gc, 0, 0, 0, 0, (uint16_t)win_w, (uint16_t)win_h);
        } else if (type == 22) { /* ConfigureNotify */
            int nw = (int)get_u16(ev + 16);
            int nh = (int)get_u16(ev + 18);
            apply_resize(nw, nh);
            pix = g_pix;
        } else if (type == 4) { /* ButtonPress */
            uint32_t btn = ev[1];
            int16_t x = (int16_t)get_u16(ev + 24);
            int16_t y = (int16_t)get_u16(ev + 26);
            if (btn == 1 && y >= BAR_H) {
                drawing = 1;
                last_x = x;
                last_y = y;
                gc_set_clip_canvas(gc);
                draw_brush(wid, pix, gc, x, y, cur_brush);
            } else if (btn == 1 && y < BAR_H) {
                int handled = 0;
                for (int i = 0; i < (int)NPALETTE; i++) {
                    int sx = 10 + i * 34;
                    if (x >= sx - 2 && x < sx + 30) {
                        cur_sel = i;
                        cur_color = palette[i];
                        cur_eraser = 0;
                        draw_ui(wid, pix, gc, cur_color, cur_brush, cur_sel);
                        handled = 1;
                        break;
                    }
                }
                if (!handled && x >= 224 && x < 276 && y >= 8 && y < 36) {
                    cur_eraser = !cur_eraser;
                    draw_ui(wid, pix, gc, cur_color, cur_brush, cur_sel);
                } else if (!handled && x >= 290 && x < 318 && y >= 8 && y < 36) {
                    if (cur_brush > 1) {
                        cur_brush--;
                        draw_ui(wid, pix, gc, cur_color, cur_brush, cur_sel);
                    }
                } else if (!handled && x >= 350 && x < 378 && y >= 8 && y < 36) {
                    if (cur_brush < 40) {
                        cur_brush++;
                        draw_ui(wid, pix, gc, cur_color, cur_brush, cur_sel);
                    }
                }
            }
        } else if (type == 5) { /* ButtonRelease */
            if (ev[1] == 1)
                drawing = 0;
        } else if (type == 6) { /* MotionNotify */
            int16_t x = (int16_t)get_u16(ev + 24);
            int16_t y = (int16_t)get_u16(ev + 26);
            if (drawing) {
                if (y < BAR_H)
                    y = BAR_H;
                gc_set_clip_canvas(gc);
                draw_line_brush(wid, pix, gc, last_x, last_y, x, y, cur_brush);
                last_x = x;
                last_y = y;
            }
        } else if (type == 2) { /* KeyPress */
            uint8_t keycode = ev[1];
            /* Keycodes vary by layout; also check common ASCII via rough mapping. */
            /* Use keycode -> rough handling: 'q'=24, 'c'=54, '1'-'6'=10-15, '['=34, ']'=35, Esc=9 on common XKB */
            if (keycode == 24 || keycode == 9)
                break; /* q or Esc */
            if (keycode == 54) {
                int gx, gy, gw, gh;
                if (get_geometry(wid, &gx, &gy, &gw, &gh) == 0) {
                    apply_resize(gw, gh);
                    pix = g_pix;
                }
                clear_canvas(wid, pix, gc, cur_color, cur_brush, cur_sel);
            }
            if (keycode >= 10 && keycode <= 15) {
                cur_sel = (int)keycode - 10;
                if (cur_sel >= (int)NPALETTE)
                    cur_sel = (int)NPALETTE - 1;
                cur_color = palette[cur_sel];
                cur_eraser = 0;
                draw_ui(wid, pix, gc, cur_color, cur_brush, cur_sel);
            }
            if (keycode == 26) {
                cur_eraser = !cur_eraser;
                draw_ui(wid, pix, gc, cur_color, cur_brush, cur_sel);
            } /* e */
            if (keycode == 34 && cur_brush > 1) {
                cur_brush--;
                draw_ui(wid, pix, gc, cur_color, cur_brush, cur_sel);
            }
            if (keycode == 35 && cur_brush < 40) {
                cur_brush++;
                draw_ui(wid, pix, gc, cur_color, cur_brush, cur_sel);
            }
        } else if (type == 33) { /* ClientMessage */
            if (get_u32(ev + 12) == wm_delete)
                break;
        }
    }

    if (pix) {
        uint8_t req[8];
        memset(req, 0, sizeof(req));
        req[0] = 54;
        put_u16(req + 2, 2);
        put_u32(req + 4, pix);
        xwrite_all(req, 8);
    }
    {
        uint8_t req[8];
        memset(req, 0, sizeof(req));
        req[0] = 4;
        put_u16(req + 2, 2);
        put_u32(req + 4, wid);
        xwrite_all(req, 8);
    }
    close(xfd);
    free(body);
    return 0;
}
