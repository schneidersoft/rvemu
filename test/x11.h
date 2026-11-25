#ifndef X11_H
#define X11_H

#include <stddef.h>
#include <stdint.h>

extern int xfd;

int xwrite_all(const void *buf, size_t n);
int xread_all(void *buf, size_t n);

void put_u16(uint8_t *b, uint16_t v);
void put_u32(uint8_t *b, uint32_t v);
uint16_t get_u16(const uint8_t *b);
uint32_t get_u32(const uint8_t *b);
size_t pad4(size_t n);
int xwrite_pad(const void *buf, size_t n);

uint32_t x11_intern_atom(const char *name);
int x11_change_property32(uint32_t win, uint32_t prop, uint32_t type, const uint32_t *vals, uint32_t nvals);
int x11_change_property8(uint32_t win, uint32_t prop, uint32_t type, const char *data, uint32_t n);

int x11_change_gc_fg(uint32_t gc, uint32_t pixel);
int x11_fill_rect(uint32_t d, uint32_t gc, int x, int y, int w, int h);
int x11_fill_arc(uint32_t d, uint32_t gc, int x, int y, int w, int h, int a1, int a2);
int x11_copy_area(uint32_t src, uint32_t dst, uint32_t gc, int sx, int sy, int dx, int dy, int w, int h);
int x11_text8(uint32_t d, uint32_t gc, int x, int y, const char *s);

#endif
