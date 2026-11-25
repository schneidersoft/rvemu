#ifndef X11_H
#define X11_H

#include <stddef.h>
#include <stdint.h>

typedef struct X11 {
    int fd;
    uint32_t root;
    uint32_t white;
    uint32_t black;
    uint32_t id_base;
    uint32_t next_id;
    uint8_t root_depth;
    uint16_t screen_w;
    uint16_t screen_h;
} X11;

extern int xfd;

/* low-level I/O */
int x11_write_all(int fd, const void *buf, size_t n);
int x11_read_all(int fd, void *buf, size_t n);
int xwrite_all(const void *buf, size_t n);
int xread_all(void *buf, size_t n);

/* byte helpers */
void put_u16(uint8_t *b, uint16_t v);
void put_u32(uint8_t *b, uint32_t v);
uint16_t get_u16(const uint8_t *b);
uint32_t get_u32(const uint8_t *b);
size_t pad4(size_t n);
int xwrite_pad(const void *buf, size_t n);

/* connection/display */
int x11_open(X11 *x, const char *display);
int x11_open_default(X11 *x);
void x11_close(X11 *x);
uint32_t x11_new_id(X11 *x);

/* protocol helpers */
uint32_t x11_intern_atom(const char *name);
uint32_t x11_intern_atom_fd(int fd, const char *name);
int x11_change_property32(uint32_t win, uint32_t prop, uint32_t type, const uint32_t *vals, uint32_t nvals);
int x11_change_property8(uint32_t win, uint32_t prop, uint32_t type, const char *data, uint32_t n);
int x11_set_title(uint32_t win, const char *title);
uint32_t x11_set_wm_delete_window(uint32_t win);

/* resource creation */
int x11_create_window(uint32_t wid, uint32_t parent, int16_t x, int16_t y,
                      uint16_t w, uint16_t h, uint32_t bg, uint32_t event_mask);
int x11_create_gc(uint32_t gc, uint32_t drawable, uint32_t value_mask, uint32_t fg);
int x11_create_pixmap(uint32_t pid, uint32_t drawable, uint16_t w, uint16_t h, uint8_t depth);
int x11_free_pixmap(uint32_t pid);
int x11_free_gc(uint32_t gc);
int x11_destroy_window(uint32_t wid);
int x11_map_window(uint32_t wid);

/* drawing */
int x11_change_gc_fg(uint32_t gc, uint32_t pixel);
int x11_fill_rect(uint32_t d, uint32_t gc, int x, int y, int w, int h);
int x11_fill_arc(uint32_t d, uint32_t gc, int x, int y, int w, int h, int a1, int a2);
int x11_copy_area(uint32_t src, uint32_t dst, uint32_t gc, int sx, int sy, int dx, int dy, int w, int h);
int x11_text8(uint32_t d, uint32_t gc, int x, int y, const char *s);

#endif
