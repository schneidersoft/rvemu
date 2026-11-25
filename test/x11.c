#include "x11.h"

#include <string.h>
#include <unistd.h>

int xfd = -1;

int xwrite_all(const void *buf, size_t n) {
    const uint8_t *p = (const uint8_t *)buf;
    while (n > 0) {
        ssize_t r = write(xfd, p, n);
        if (r <= 0) return -1;
        p += (size_t)r;
        n -= (size_t)r;
    }
    return 0;
}

int xread_all(void *buf, size_t n) {
    uint8_t *p = (uint8_t *)buf;
    while (n > 0) {
        ssize_t r = read(xfd, p, n);
        if (r <= 0) return -1;
        p += (size_t)r;
        n -= (size_t)r;
    }
    return 0;
}

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

uint32_t x11_intern_atom(const char *name) {
    uint8_t req[64], reply[32];
    size_t n = strlen(name), len = 2 + (n + pad4(n)) / 4;
    if (len * 4 > sizeof(req)) return 0;
    memset(req, 0, len * 4);
    req[0] = 16;
    put_u16(req + 2, (uint16_t)len);
    put_u16(req + 4, (uint16_t)n);
    memcpy(req + 8, name, n);
    if (xwrite_all(req, len * 4) < 0) return 0;
    if (xread_all(reply, 32) < 0 || reply[0] != 1) return 0;
    return get_u32(reply + 8);
}

int x11_change_property32(uint32_t win, uint32_t prop, uint32_t type, const uint32_t *vals, uint32_t nvals) {
    uint8_t hdr[24];
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 18;
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
    hdr[0] = 18;
    put_u16(hdr + 2, (uint16_t)len);
    put_u32(hdr + 4, win);
    put_u32(hdr + 8, prop);
    put_u32(hdr + 12, type);
    hdr[16] = 8;
    put_u32(hdr + 20, n);
    if (xwrite_all(hdr, 24) < 0) return -1;
    return xwrite_pad(data, n);
}

int x11_change_gc_fg(uint32_t gc, uint32_t pixel) {
    uint8_t req[16];
    memset(req, 0, sizeof(req));
    req[0] = 56;
    put_u16(req + 2, 4);
    put_u32(req + 4, gc);
    put_u32(req + 8, 0x0004);
    put_u32(req + 12, pixel);
    return xwrite_all(req, 16);
}

int x11_fill_rect(uint32_t d, uint32_t gc, int x, int y, int w, int h) {
    uint8_t req[20];
    memset(req, 0, sizeof(req));
    req[0] = 70;
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
    req[0] = 71;
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
    req[0] = 62;
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
    hdr[0] = 74;
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
