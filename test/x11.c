#include "x11.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int xfd = -1;

int x11_write_all(int fd, const void *buf, size_t n) {
    const uint8_t *p = (const uint8_t *)buf;
    while (n > 0) {
        ssize_t r = write(fd, p, n);
        if (r <= 0) return -1;
        p += (size_t)r;
        n -= (size_t)r;
    }
    return 0;
}

int x11_read_all(int fd, void *buf, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    while (n > 0) {
        ssize_t r = read(fd, p, n);
        if (r <= 0) return -1;
        p += (size_t)r;
        n -= (size_t)r;
    }
    return 0;
}

int xwrite_all(const void *buf, size_t n) { return x11_write_all(xfd, buf, n); }
int xread_all(void *buf, size_t n) { return x11_read_all(xfd, buf, n); }

void put_u16(uint8_t *b, uint16_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); }
void put_u32(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
}
uint16_t get_u16(const uint8_t *b) { return (uint16_t)b[0] | ((uint16_t)b[1] << 8); }
uint32_t get_u32(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}
size_t pad4(size_t n) { return (4 - (n & 3)) & 3; }

int xwrite_pad(const void *buf, size_t n) {
    static const uint8_t z[3] = {0, 0, 0};
    if (xwrite_all(buf, n) < 0) return -1;
    size_t p = pad4(n);
    return p ? xwrite_all(z, p) : 0;
}

int x11_open(X11 *x, const char *display) {
    struct sockaddr_un addr;
    uint8_t setup[12], sh[8], *body = NULL;
    uint16_t addlen, vendor_len;
    uint8_t nfmt;
    size_t off;

    memset(x, 0, sizeof(*x));
    x->fd = -1;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, display ? display : "/tmp/.X11-unix/X0", sizeof(addr.sun_path) - 1);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { close(fd); return -1; }

    memset(setup, 0, sizeof(setup));
    setup[0] = 'l'; setup[2] = 11;
    if (x11_write_all(fd, setup, 12) < 0 || x11_read_all(fd, sh, 8) < 0 || sh[0] != 1) {
        close(fd); return -1;
    }
    addlen = get_u16(sh + 6);
    body = (uint8_t *)malloc((size_t)addlen * 4);
    if (!body || x11_read_all(fd, body, (size_t)addlen * 4) < 0) { free(body); close(fd); return -1; }
    x->id_base = get_u32(body + 4);
    x->next_id = 0;
    vendor_len = get_u16(body + 16);
    nfmt = body[21];
    off = 32 + (((size_t)vendor_len + 3) & ~(size_t)3) + (size_t)nfmt * 8;
    x->root = get_u32(body + off);
    x->root_depth = body[off + 38];
    x->white = get_u32(body + off + 20);
    x->black = get_u32(body + off + 16);
    x->screen_w = get_u16(body + off + 20);
    x->screen_h = get_u16(body + off + 22);
    free(body);

    x->fd = fd;
    xfd = fd;
    return 0;
}

int x11_open_default(X11 *x) { return x11_open(x, "/tmp/.X11-unix/X0"); }

void x11_close(X11 *x) {
    if (!x) return;
    if (x->fd >= 0) close(x->fd);
    if (xfd == x->fd) xfd = -1;
    x->fd = -1;
}

uint32_t x11_new_id(X11 *x) { return x->id_base + x->next_id++; }

uint32_t x11_intern_atom_fd(int fd, const char *name) {
    uint8_t req[64], reply[32];
    size_t n = strlen(name), len = 2 + (n + pad4(n)) / 4;
    if (len * 4 > sizeof(req)) return 0;
    memset(req, 0, len * 4);
    req[0] = X11_OP_INTERN_ATOM;
    put_u16(req + 2, (uint16_t)len);
    put_u16(req + 4, (uint16_t)n);
    memcpy(req + 8, name, n);
    if (x11_write_all(fd, req, len * 4) < 0) return 0;
    if (x11_read_all(fd, reply, 32) < 0 || reply[0] != 1) return 0;
    return get_u32(reply + 8);
}

uint32_t x11_intern_atom(const char *name) { return x11_intern_atom_fd(xfd, name); }

int x11_change_property32(uint32_t win, uint32_t prop, uint32_t type, const uint32_t *vals, uint32_t nvals) {
    uint8_t hdr[24];
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = X11_OP_CHANGE_PROPERTY;
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

int x11_change_property8(uint32_t win, uint32_t prop, uint32_t type, const char *data, uint32_t n) {
    uint8_t hdr[24];
    size_t len = 6 + (n + pad4(n)) / 4;
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = X11_OP_CHANGE_PROPERTY;
    put_u16(hdr + 2, (uint16_t)len);
    put_u32(hdr + 4, win);
    put_u32(hdr + 8, prop);
    put_u32(hdr + 12, type);
    hdr[16] = 8;
    put_u32(hdr + 20, n);
    if (xwrite_all(hdr, 24) < 0) return -1;
    return xwrite_pad(data, n);
}

int x11_set_title(uint32_t win, const char *title) {
    uint32_t utf8 = x11_intern_atom("UTF8_STRING");
    uint32_t type = utf8 ? utf8 : X11_ATOM_STRING;
    return x11_change_property8(win, X11_ATOM_WM_NAME, type, title, (uint32_t)strlen(title));
}

uint32_t x11_set_wm_delete_window(uint32_t win) {
    uint32_t wm_protocols = x11_intern_atom("WM_PROTOCOLS");
    uint32_t wm_delete = x11_intern_atom("WM_DELETE_WINDOW");
    if (wm_protocols && wm_delete)
        x11_change_property32(win, wm_protocols, X11_ATOM_ATOM, &wm_delete, 1);
    return wm_delete;
}

int x11_create_window(uint32_t wid, uint32_t parent, int16_t x, int16_t y,
                      uint16_t w, uint16_t h, uint32_t bg, uint32_t event_mask) {
    uint8_t req[40];
    uint32_t mask = X11_CW_BACK_PIXEL | X11_CW_EVENT_MASK;
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_CREATE_WINDOW;
    put_u16(req + 2, 10);
    put_u32(req + 4, wid);
    put_u32(req + 8, parent);
    put_u16(req + 12, (uint16_t)x);
    put_u16(req + 14, (uint16_t)y);
    put_u16(req + 16, w);
    put_u16(req + 18, h);
    put_u16(req + 22, 1); /* border */
    put_u32(req + 28, mask);
    put_u32(req + 32, bg);
    put_u32(req + 36, event_mask);
    return xwrite_all(req, 40);
}

int x11_create_gc(uint32_t gc, uint32_t drawable, uint32_t value_mask, uint32_t fg) {
    uint8_t req[20];
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_CREATE_GC;
    put_u16(req + 2, 5);
    put_u32(req + 4, gc);
    put_u32(req + 8, drawable);
    put_u32(req + 12, value_mask);
    put_u32(req + 16, fg);
    return xwrite_all(req, 20);
}

int x11_create_pixmap(uint32_t pid, uint32_t drawable, uint16_t w, uint16_t h, uint8_t depth) {
    uint8_t req[16];
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_CREATE_PIXMAP;
    req[1] = depth;
    put_u16(req + 2, 4);
    put_u32(req + 4, pid);
    put_u32(req + 8, drawable);
    put_u16(req + 12, w);
    put_u16(req + 14, h);
    return xwrite_all(req, 16);
}

int x11_free_pixmap(uint32_t pid) {
    uint8_t req[8];
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_FREE_PIXMAP; put_u16(req + 2, 2); put_u32(req + 4, pid);
    return xwrite_all(req, 8);
}

int x11_free_gc(uint32_t gc) {
    uint8_t req[8];
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_FREE_GC; put_u16(req + 2, 2); put_u32(req + 4, gc);
    return xwrite_all(req, 8);
}

int x11_destroy_window(uint32_t wid) {
    uint8_t req[8];
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_DESTROY_WINDOW; put_u16(req + 2, 2); put_u32(req + 4, wid);
    return xwrite_all(req, 8);
}

int x11_map_window(uint32_t wid) {
    uint8_t req[8];
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_MAP_WINDOW; put_u16(req + 2, 2); put_u32(req + 4, wid);
    return xwrite_all(req, 8);
}

int x11_change_gc_fg(uint32_t gc, uint32_t pixel) {
    uint8_t req[16];
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_CHANGE_GC;
    put_u16(req + 2, 4);
    put_u32(req + 4, gc);
    put_u32(req + 8, X11_GC_FOREGROUND);
    put_u32(req + 12, pixel);
    return xwrite_all(req, 16);
}

int x11_fill_rect(uint32_t d, uint32_t gc, int x, int y, int w, int h) {
    uint8_t req[20];
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_POLY_FILL_RECTANGLE;
    put_u16(req + 2, 5);
    put_u32(req + 4, d);
    put_u32(req + 8, gc);
    put_u16(req + 12, (uint16_t)x);
    put_u16(req + 14, (uint16_t)y);
    put_u16(req + 16, (uint16_t)w);
    put_u16(req + 18, (uint16_t)h);
    return xwrite_all(req, 20);
}

int x11_fill_arc(uint32_t d, uint32_t gc, int x, int y, int w, int h, int a1, int a2) {
    uint8_t req[24];
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_POLY_FILL_ARC;
    put_u16(req + 2, 6);
    put_u32(req + 4, d);
    put_u32(req + 8, gc);
    put_u16(req + 12, (uint16_t)x);
    put_u16(req + 14, (uint16_t)y);
    put_u16(req + 16, (uint16_t)w);
    put_u16(req + 18, (uint16_t)h);
    put_u16(req + 20, (uint16_t)a1);
    put_u16(req + 22, (uint16_t)a2);
    return xwrite_all(req, 24);
}

int x11_copy_area(uint32_t src, uint32_t dst, uint32_t gc, int sx, int sy, int dx, int dy, int w, int h) {
    uint8_t req[28];
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_COPY_AREA;
    put_u16(req + 2, 7);
    put_u32(req + 4, src);
    put_u32(req + 8, dst);
    put_u32(req + 12, gc);
    put_u16(req + 16, (uint16_t)sx);
    put_u16(req + 18, (uint16_t)sy);
    put_u16(req + 20, (uint16_t)dx);
    put_u16(req + 22, (uint16_t)dy);
    put_u16(req + 24, (uint16_t)w);
    put_u16(req + 26, (uint16_t)h);
    return xwrite_all(req, 28);
}

int x11_text8(uint32_t d, uint32_t gc, int x, int y, const char *s) {
    uint8_t hdr[16], item[2];
    size_t n = strlen(s);
    if (n > 254) n = 254;
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = X11_OP_POLY_TEXT8;
    put_u16(hdr + 2, (uint16_t)(4 + ((2 + n + pad4(2 + n)) / 4)));
    put_u32(hdr + 4, d);
    put_u32(hdr + 8, gc);
    put_u16(hdr + 12, (uint16_t)x);
    put_u16(hdr + 14, (uint16_t)y);
    if (xwrite_all(hdr, 16) < 0) return -1;
    item[0] = (uint8_t)n; item[1] = 0;
    if (xwrite_all(item, 2) < 0) return -1;
    if (xwrite_all(s, n) < 0) return -1;
    { static const uint8_t z[3]={0,0,0}; size_t p=pad4(2+n); if (p && xwrite_all(z,p)<0) return -1; }
    return 0;
}
