/*
 * Mini raw X11 paint example for rvemu guest.
 *
 * Zero external dependencies: raw X11 protocol over Unix-domain socket.
 * Build:  make bin/x11_paint.bin
 * Run:    ./bin/riscv64i bin/x11_paint.bin
 *
 * Controls: drag draw, 1-6 color, e eraser, [/] or +/- buttons size, c clear, q/Esc quit
 */

#include "x11.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BAR_H 44

static X11 xdisp;
static uint32_t wid, gc, pix;
static int win_w = 640;
static int win_h = 400;

static uint32_t cur_color = 0xFFFFFFFF;
static int cur_brush = 4;
static int cur_sel = 0;
static int cur_eraser = 0;
static uint8_t pending_ev[32];
static int has_pending_ev = 0;

static const uint32_t palette[] = {
    0xFFFFFFFF, 0xFFFF5555, 0xFF50FA7B, 0xFF89B4FA, 0xFFF1FA8C, 0xFF000000,
};
#define NPALETTE (int)(sizeof(palette) / sizeof(palette[0]))

static int canvas_h(void) { return win_h - BAR_H; }

static int next_event(uint8_t ev[32]) {
    if (has_pending_ev) { memcpy(ev, pending_ev, 32); has_pending_ev = 0; return 0; }
    return xread_all(ev, 32);
}

static int get_geometry(uint32_t d, int *x, int *y, int *w, int *h) {
    uint8_t req[8], ev[32];
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_GET_GEOMETRY; put_u16(req + 2, 2); put_u32(req + 4, d);
    if (xwrite_all(req, 8) < 0) return -1;
    for (;;) {
        if (xread_all(ev, 32) < 0) return -1;
        if (ev[0] == X11_EV_REPLY) {
            if (x) *x = (int16_t)get_u16(ev + 12);
            if (y) *y = (int16_t)get_u16(ev + 14);
            if (w) *w = get_u16(ev + 16);
            if (h) *h = get_u16(ev + 18);
            return 0;
        }
        if (ev[0] == X11_EV_ERROR) return -1;
        if (!has_pending_ev) { memcpy(pending_ev, ev, 32); has_pending_ev = 1; }
    }
}

static int gc_set_clip_canvas(void) {
    uint8_t req[20];
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_SET_CLIP_RECTANGLES; req[1] = 0;
    put_u16(req + 2, 5);
    put_u32(req + 4, gc);
    put_u16(req + 8, 0); put_u16(req + 10, 0);
    put_u16(req + 12, 0); put_u16(req + 14, (uint16_t)BAR_H);
    put_u16(req + 16, (uint16_t)win_w); put_u16(req + 18, (uint16_t)(win_h - BAR_H));
    return xwrite_all(req, 20);
}

static int gc_clear_clip(void) {
    uint8_t req[16];
    memset(req, 0, sizeof(req));
    req[0] = X11_OP_CHANGE_GC; put_u16(req + 2, 4);
    put_u32(req + 4, gc);
    put_u32(req + 8, X11_GC_CLIP_MASK);
    put_u32(req + 12, 0); /* None */
    return xwrite_all(req, 16);
}

static void draw_dot(uint32_t d, int x, int y, int r) {
    if (r < 1) r = 1;
    x11_fill_arc(d, gc, x - r, y - r, r * 2, r * 2, 0, 360 * 64);
}

static void draw_brush(int x, int y, int r) {
    uint32_t c = cur_eraser ? 0xFF1E1E2E : cur_color;
    x11_change_gc_fg(gc, c);
    draw_dot(wid, x, y, r);
    if (pix) draw_dot(pix, x, y, r);
}

static void draw_line_brush(int x0, int y0, int x1, int y1, int r) {
    int dx = x1 - x0, dy = y1 - y0;
    int steps = dx < 0 ? -dx : dx;
    int ay = dy < 0 ? -dy : dy;
    if (ay > steps) steps = ay;
    if (steps == 0) { draw_brush(x0, y0, r); return; }
    for (int i = 0; i <= steps; i++)
        draw_brush(x0 + dx * i / steps, y0 + dy * i / steps, r);
}

static void draw_button(int x, int y, int w, int h, uint32_t bg, int active) {
    uint32_t border = active ? 0xFFFFAA00 : 0xFF888888;
    x11_change_gc_fg(gc, bg);
    x11_fill_rect(wid, gc, x, y, w, h);
    if (pix) x11_fill_rect(pix, gc, x, y, w, h);
    x11_change_gc_fg(gc, border);
    x11_fill_rect(wid, gc, x, y, w, 2); x11_fill_rect(wid, gc, x, y + h - 2, w, 2);
    x11_fill_rect(wid, gc, x, y, 2, h); x11_fill_rect(wid, gc, x + w - 2, y, 2, h);
    if (pix) {
        x11_fill_rect(pix, gc, x, y, w, 2); x11_fill_rect(pix, gc, x, y + h - 2, w, 2);
        x11_fill_rect(pix, gc, x, y, 2, h); x11_fill_rect(pix, gc, x + w - 2, y, 2, h);
    }
}

static void draw_ui(void) {
    gc_clear_clip();
    x11_change_gc_fg(gc, 0xFF2B2B3B);
    x11_fill_rect(wid, gc, 0, 0, win_w, BAR_H);
    if (pix) x11_fill_rect(pix, gc, 0, 0, win_w, BAR_H);

    for (int i = 0; i < NPALETTE; i++) {
        int sx = 10 + i * 34;
        x11_change_gc_fg(gc, palette[i]);
        x11_fill_rect(wid, gc, sx, 8, 28, 28);
        if (pix) x11_fill_rect(pix, gc, sx, 8, 28, 28);
        if (i == cur_sel && !cur_eraser) {
            x11_change_gc_fg(gc, 0xFFFFAA00);
            x11_fill_rect(wid, gc, sx - 2, 6, 32, 2); x11_fill_rect(wid, gc, sx - 2, 36, 32, 2);
            x11_fill_rect(wid, gc, sx - 2, 6, 2, 32); x11_fill_rect(wid, gc, sx + 30, 6, 2, 32);
            if (pix) {
                x11_fill_rect(pix, gc, sx - 2, 6, 32, 2); x11_fill_rect(pix, gc, sx - 2, 36, 32, 2);
                x11_fill_rect(pix, gc, sx - 2, 6, 2, 32); x11_fill_rect(pix, gc, sx + 30, 6, 2, 32);
            }
        }
    }

    draw_button(224, 8, 52, 28, 0xFF444455, cur_eraser);
    x11_change_gc_fg(gc, 0xFFFFFFFF);
    x11_text8(wid, gc, 232, 27, "Eraser");
    if (pix) x11_text8(pix, gc, 232, 27, "Eraser");

    draw_button(290, 8, 28, 28, 0xFF444455, 0);
    x11_change_gc_fg(gc, 0xFFFFFFFF);
    x11_text8(wid, gc, 300, 27, "-");
    if (pix) x11_text8(pix, gc, 300, 27, "-");

    char buf[16]; snprintf(buf, sizeof(buf), "%d", cur_brush);
    x11_change_gc_fg(gc, 0xFFCCCCCC);
    x11_text8(wid, gc, 326, 27, buf);
    if (pix) x11_text8(pix, gc, 326, 27, buf);

    draw_button(350, 8, 28, 28, 0xFF444455, 0);
    x11_change_gc_fg(gc, 0xFFFFFFFF);
    x11_text8(wid, gc, 360, 27, "+");
    if (pix) x11_text8(pix, gc, 360, 27, "+");

    x11_change_gc_fg(gc, 0xFFAAAAAA);
    x11_text8(wid, gc, 392, 27, "c:clear q:quit");
    if (pix) x11_text8(pix, gc, 392, 27, "c:clear q:quit");

    x11_change_gc_fg(gc, cur_eraser ? 0xFF1E1E2E : cur_color);
}

static void clear_canvas(void) {
    x11_change_gc_fg(gc, 0xFF1E1E2E);
    x11_fill_rect(wid, gc, 0, BAR_H, win_w, canvas_h());
    if (pix) x11_fill_rect(pix, gc, 0, BAR_H, win_w, canvas_h());
    draw_ui();
}

static void apply_resize(int nw, int nh) {
    if (nw <= 0 || nh <= 0 || (nw == win_w && nh == win_h)) return;
    int ow = win_w, oh = win_h;
    uint32_t newpix = x11_new_id(&xdisp);
    if (x11_create_pixmap(newpix, wid, (uint16_t)nw, (uint16_t)nh, xdisp.root_depth) < 0) return;
    x11_change_gc_fg(gc, 0xFF1E1E2E);
    x11_fill_rect(newpix, gc, 0, 0, nw, nh);
    if (pix) {
        uint16_t cw = (uint16_t)(ow < nw ? ow : nw);
        uint16_t ch = (uint16_t)(oh < nh ? oh : nh);
        x11_copy_area(pix, newpix, gc, 0, 0, 0, 0, cw, ch);
        x11_free_pixmap(pix);
    }
    pix = newpix;
    win_w = nw; win_h = nh;
    draw_ui();
    x11_copy_area(pix, wid, gc, 0, 0, 0, 0, win_w, win_h);
}

int main(void) {
    if (x11_open_default(&xdisp) < 0) { fprintf(stderr, "x11 open failed\n"); return 1; }

    wid = x11_new_id(&xdisp);
    gc = x11_new_id(&xdisp);
    pix = x11_new_id(&xdisp);

    uint32_t evmask = X11_MASK_KEY_PRESS | X11_MASK_BUTTON_PRESS | X11_MASK_BUTTON_RELEASE |
                      X11_MASK_POINTER_MOTION | X11_MASK_EXPOSURE | X11_MASK_STRUCTURE_NOTIFY;
    if (x11_create_window(wid, xdisp.root, 60, 60, (uint16_t)win_w, (uint16_t)win_h, 0xFF1E1E2E, evmask) < 0) return 1;
    if (x11_create_gc(gc, wid, X11_GC_FOREGROUND, xdisp.white) < 0) return 1;
    if (x11_create_pixmap(pix, wid, (uint16_t)win_w, (uint16_t)win_h, xdisp.root_depth) < 0) pix = 0;

    x11_set_title(wid, "rvemu paint");
    uint32_t wm_delete = x11_set_wm_delete_window(wid);
    x11_map_window(wid);

    printf("rvemu paint: drag draw, 1-6 color, e eraser, [/] size, c clear, q quit\n");
    fflush(stdout);

    { int gx, gy, gw, gh; if (get_geometry(wid, &gx, &gy, &gw, &gh) == 0) apply_resize(gw, gh); }
    clear_canvas();

    int drawing = 0, last_x = 0, last_y = 0;
    for (;;) {
        uint8_t ev[32];
        if (next_event(ev) < 0) break;
        uint8_t type = ev[0] & 0x7f;
        if (type == X11_EV_ERROR) continue;
        if (type == X11_EV_EXPOSE) {
            if (get_u16(ev + 16) == 0 && pix) x11_copy_area(pix, wid, gc, 0, 0, 0, 0, win_w, win_h);
        } else if (type == X11_EV_CONFIGURE_NOTIFY) {
            apply_resize((int)get_u16(ev + 20), (int)get_u16(ev + 22));
        } else if (type == X11_EV_BUTTON_PRESS) {
            uint32_t btn = ev[1]; int16_t x = (int16_t)get_u16(ev + 24), y = (int16_t)get_u16(ev + 26);
            if (btn == 1 && y >= BAR_H) {
                drawing = 1; last_x = x; last_y = y; gc_set_clip_canvas(); draw_brush(x, y, cur_brush);
            } else if (btn == 1 && y < BAR_H) {
                int handled = 0;
                for (int i = 0; i < NPALETTE; i++) {
                    int sx = 10 + i * 34;
                    if (x >= sx - 2 && x < sx + 30) { cur_sel = i; cur_color = palette[i]; cur_eraser = 0; draw_ui(); handled = 1; break; }
                }
                if (!handled && x >= 224 && x < 276 && y >= 8 && y < 36) { cur_eraser = !cur_eraser; draw_ui(); }
                else if (!handled && x >= 290 && x < 318 && y >= 8 && y < 36) { if (cur_brush > 1) { cur_brush--; draw_ui(); } }
                else if (!handled && x >= 350 && x < 378 && y >= 8 && y < 36) { if (cur_brush < 40) { cur_brush++; draw_ui(); } }
            }
        } else if (type == X11_EV_BUTTON_RELEASE) {
            if (ev[1] == 1) drawing = 0;
        } else if (type == X11_EV_MOTION_NOTIFY) {
            int16_t x = (int16_t)get_u16(ev + 24), y = (int16_t)get_u16(ev + 26);
            if (drawing) { if (y < BAR_H) y = BAR_H; gc_set_clip_canvas(); draw_line_brush(last_x, last_y, x, y, cur_brush); last_x = x; last_y = y; }
        } else if (type == X11_EV_KEY_PRESS) {
            uint8_t kc = ev[1];
            if (kc == 24 || kc == 9) break;
            if (kc == 54) { int gx, gy, gw, gh; if (get_geometry(wid, &gx, &gy, &gw, &gh) == 0) apply_resize(gw, gh); clear_canvas(); }
            if (kc >= 10 && kc <= 15) { cur_sel = kc - 10; if (cur_sel >= NPALETTE) cur_sel = NPALETTE - 1; cur_color = palette[cur_sel]; cur_eraser = 0; draw_ui(); }
            if (kc == 26) { cur_eraser = !cur_eraser; draw_ui(); }
            if (kc == 34 && cur_brush > 1) { cur_brush--; draw_ui(); }
            if (kc == 35 && cur_brush < 40) { cur_brush++; draw_ui(); }
        } else if (type == X11_EV_CLIENT_MESSAGE) {
            if (wm_delete && get_u32(ev + 12) == wm_delete) break;
        }
    }

    if (pix) x11_free_pixmap(pix);
    x11_destroy_window(wid);
    x11_close(&xdisp);
    return 0;
}
