/*
 * Mini raw X11 paint example for rvemu guest.
 *
 * Zero external dependencies: raw X11 protocol over Unix-domain socket.
 * Build:  make bin/x11.bin
 * Run:    ./bin/riscv64i bin/x11.bin
 *
 * Controls: drag draw, 1-6 color, e eraser, [/] or +/- buttons size, c clear, q/Esc quit
 */

#include "x11.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

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

static const uint32_t palette[] = {
    0xFFFFFFFF, 0xFFFF5555, 0xFF50FA7B, 0xFF89B4FA, 0xFFF1FA8C, 0xFF000000,
};
#define NPALETTE (int)(sizeof(palette)/sizeof(palette[0]))

static int next_event(uint8_t ev[32]) {
    if (has_pending_ev) { memcpy(ev, pending_ev, 32); has_pending_ev = 0; return 0; }
    return xread_all(ev, 32);
}

static int get_geometry(uint32_t d, int *x, int *y, int *w, int *h) {
    uint8_t req[8], ev[32];
    memset(req, 0, sizeof(req));
    req[0] = 14; put_u16(req + 2, 2); put_u32(req + 4, d);
    if (xwrite_all(req, 8) < 0) return -1;
    for (;;) {
        if (xread_all(ev, 32) < 0) return -1;
        if (ev[0] == 1) {
            if (x) *x = (int16_t)get_u16(ev + 12);
            if (y) *y = (int16_t)get_u16(ev + 14);
            if (w) *w = get_u16(ev + 16);
            if (h) *h = get_u16(ev + 18);
            return 0;
        }
        if (ev[0] == 0) return -1;
        if (!has_pending_ev) { memcpy(pending_ev, ev, 32); has_pending_ev = 1; }
    }
}

static int gc_set_clip_canvas(uint32_t gc) {
    uint8_t req[20];
    memset(req, 0, sizeof(req));
    req[0] = 59; req[1] = 0;
    put_u16(req + 2, 5);
    put_u32(req + 4, gc);
    put_u16(req + 8, 0); put_u16(req + 10, 0);
    put_u16(req + 12, 0); put_u16(req + 14, (uint16_t)BAR_H);
    put_u16(req + 16, (uint16_t)win_w); put_u16(req + 18, (uint16_t)(win_h - BAR_H));
    return xwrite_all(req, 20);
}

static int gc_clear_clip(uint32_t gc) {
    uint8_t req[16];
    memset(req, 0, sizeof(req));
    req[0] = 56; put_u16(req + 2, 4);
    put_u32(req + 4, gc);
    put_u32(req + 8, 0x80000);
    put_u32(req + 12, 0);
    return xwrite_all(req, 16);
}

static void draw_dot(uint32_t d, uint32_t gc, int x, int y, int r) {
    if (r < 1) r = 1;
    x11_fill_arc(d, gc, x - r, y - r, r * 2, r * 2, 0, 360 * 64);
}

static void draw_brush(uint32_t win, uint32_t pix, uint32_t gc, int x, int y, int r) {
    uint32_t c = cur_eraser ? 0xFF1E1E2E : cur_color;
    x11_change_gc_fg(gc, c);
    draw_dot(win, gc, x, y, r);
    if (pix) draw_dot(pix, gc, x, y, r);
}

static void draw_line_brush(uint32_t win, uint32_t pix, uint32_t gc, int x0, int y0, int x1, int y1, int r) {
    int dx = x1 - x0, dy = y1 - y0;
    int steps = dx < 0 ? -dx : dx;
    int ay = dy < 0 ? -dy : dy;
    if (ay > steps) steps = ay;
    if (steps == 0) { draw_brush(win, pix, gc, x0, y0, r); return; }
    for (int i = 0; i <= steps; i++) {
        int x = x0 + dx * i / steps;
        int y = y0 + dy * i / steps;
        draw_brush(win, pix, gc, x, y, r);
    }
}

static void draw_button(uint32_t win, uint32_t pix, uint32_t gc, int x, int y, int w, int h, uint32_t bg, int active) {
    x11_change_gc_fg(gc, bg);
    x11_fill_rect(win, gc, x, y, w, h);
    if (pix) x11_fill_rect(pix, gc, x, y, w, h);
    uint32_t border = active ? 0xFFFFAA00 : 0xFF888888;
    x11_change_gc_fg(gc, border);
    x11_fill_rect(win, gc, x, y, w, 2); x11_fill_rect(win, gc, x, y+h-2, w, 2);
    x11_fill_rect(win, gc, x, y, 2, h); x11_fill_rect(win, gc, x+w-2, y, 2, h);
    if (pix) {
        x11_fill_rect(pix, gc, x, y, w, 2); x11_fill_rect(pix, gc, x, y+h-2, w, 2);
        x11_fill_rect(pix, gc, x, y, 2, h); x11_fill_rect(pix, gc, x+w-2, y, 2, h);
    }
}

static void draw_ui(uint32_t win, uint32_t pix, uint32_t gc) {
    gc_clear_clip(gc);
    x11_change_gc_fg(gc, 0xFF2B2B3B);
    x11_fill_rect(win, gc, 0, 0, win_w, BAR_H);
    if (pix) x11_fill_rect(pix, gc, 0, 0, win_w, BAR_H);

    for (int i = 0; i < NPALETTE; i++) {
        int sx = 10 + i * 34;
        x11_change_gc_fg(gc, palette[i]);
        x11_fill_rect(win, gc, sx, 8, 28, 28);
        if (pix) x11_fill_rect(pix, gc, sx, 8, 28, 28);
        if (i == cur_sel && !cur_eraser) {
            x11_change_gc_fg(gc, 0xFFFFAA00);
            x11_fill_rect(win, gc, sx-2, 6, 32, 2); x11_fill_rect(win, gc, sx-2, 36, 32, 2);
            x11_fill_rect(win, gc, sx-2, 6, 2, 32); x11_fill_rect(win, gc, sx+30, 6, 2, 32);
            if (pix) {
                x11_fill_rect(pix, gc, sx-2, 6, 32, 2); x11_fill_rect(pix, gc, sx-2, 36, 32, 2);
                x11_fill_rect(pix, gc, sx-2, 6, 2, 32); x11_fill_rect(pix, gc, sx+30, 6, 2, 32);
            }
        }
    }

    draw_button(win, pix, gc, 224, 8, 52, 28, 0xFF444455, cur_eraser);
    x11_change_gc_fg(gc, 0xFFFFFFFF);
    x11_text8(win, gc, 232, 27, "Eraser");
    if (pix) x11_text8(pix, gc, 232, 27, "Eraser");

    draw_button(win, pix, gc, 290, 8, 28, 28, 0xFF444455, 0);
    x11_change_gc_fg(gc, 0xFFFFFFFF);
    x11_text8(win, gc, 300, 27, "-");
    if (pix) x11_text8(pix, gc, 300, 27, "-");

    char buf[16]; snprintf(buf, sizeof(buf), "%d", cur_brush);
    x11_change_gc_fg(gc, 0xFFCCCCCC);
    x11_text8(win, gc, 326, 27, buf);
    if (pix) x11_text8(pix, gc, 326, 27, buf);

    draw_button(win, pix, gc, 350, 8, 28, 28, 0xFF444455, 0);
    x11_change_gc_fg(gc, 0xFFFFFFFF);
    x11_text8(win, gc, 360, 27, "+");
    if (pix) x11_text8(pix, gc, 360, 27, "+");

    x11_change_gc_fg(gc, 0xFFAAAAAA);
    x11_text8(win, gc, 392, 27, "c:clear q:quit");
    if (pix) x11_text8(pix, gc, 392, 27, "c:clear q:quit");

    x11_change_gc_fg(gc, cur_eraser ? 0xFF1E1E2E : cur_color);
}

static void clear_canvas(uint32_t win, uint32_t pix, uint32_t gc) {
    x11_change_gc_fg(gc, 0xFF1E1E2E);
    x11_fill_rect(win, gc, 0, BAR_H, win_w, canvas_h());
    if (pix) x11_fill_rect(pix, gc, 0, BAR_H, win_w, canvas_h());
    draw_ui(win, pix, gc);
}

static void apply_resize(int nw, int nh) {
    if (nw <= 0 || nh <= 0 || (nw == win_w && nh == win_h)) return;
    int ow = win_w, oh = win_h;
    uint32_t newpix = (g_pix == g_rid_base + 2) ? g_rid_base + 3 : g_rid_base + 2;
    uint8_t req[16]; memset(req, 0, sizeof(req));
    req[0] = 53; req[1] = g_root_depth; put_u16(req + 2, 4);
    put_u32(req + 4, newpix); put_u32(req + 8, g_wid);
    put_u16(req + 12, (uint16_t)nw); put_u16(req + 14, (uint16_t)nh);
    if (xwrite_all(req, 16) < 0) return;
    x11_change_gc_fg(g_gc, 0xFF1E1E2E);
    x11_fill_rect(newpix, g_gc, 0, 0, nw, nh);
    if (g_pix) {
        uint16_t cw = (uint16_t)(ow < nw ? ow : nw);
        uint16_t ch = (uint16_t)(oh < nh ? oh : nh);
        x11_copy_area(g_pix, newpix, g_gc, 0, 0, 0, 0, cw, ch);
        uint8_t fr[8]; memset(fr, 0, sizeof(fr)); fr[0] = 54; put_u16(fr + 2, 2); put_u32(fr + 4, g_pix); xwrite_all(fr, 8);
    }
    g_pix = newpix;
    win_w = nw; win_h = nh;
    draw_ui(g_wid, g_pix, g_gc);
    x11_copy_area(g_pix, g_wid, g_gc, 0, 0, 0, 0, win_w, win_h);
}

int main(void) {
    struct sockaddr_un addr;
    uint8_t setup[12], sh[8], *body = NULL;
    uint16_t addlen, vendor_len; uint8_t nfmt; size_t off;
    uint32_t rid, root, white, wid, gc, pix;

    xfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (xfd < 0) { perror("socket"); return 1; }
    memset(&addr, 0, sizeof(addr)); addr.sun_family = AF_UNIX; strcpy(addr.sun_path, "/tmp/.X11-unix/X0");
    if (connect(xfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("connect"); return 1; }

    memset(setup, 0, sizeof(setup)); setup[0] = 'l'; setup[2] = 11;
    if (xwrite_all(setup, 12) < 0 || xread_all(sh, 8) < 0 || sh[0] != 1) { fprintf(stderr, "X11 setup failed\n"); return 1; }
    addlen = get_u16(sh + 6);
    body = (uint8_t *)malloc((size_t)addlen * 4);
    if (!body || xread_all(body, (size_t)addlen * 4) < 0) return 1;
    rid = get_u32(body + 4); vendor_len = get_u16(body + 16); nfmt = body[21];
    off = 32 + (((size_t)vendor_len + 3) & ~(size_t)3) + (size_t)nfmt * 8;
    root = get_u32(body + off); white = get_u32(body + off + 20);
    g_root_depth = body[off + 38];
    screen_w = (int)get_u16(body + off + 20); screen_h = (int)get_u16(body + off + 22);
    if (screen_w <= 0) screen_w = 640;
    if (screen_h <= 0) screen_h = 480;

    wid = rid; gc = rid + 1; pix = rid + 2;
    g_wid = wid; g_gc = gc; g_pix = pix; g_rid_base = rid;

    /* CreateWindow */
    { uint8_t r[40]; uint32_t mask = 0x2|0x800; memset(r,0,sizeof(r)); r[0]=1; put_u16(r+2,10); put_u32(r+4,wid); put_u32(r+8,root);
      put_u16(r+12,60); put_u16(r+14,60); put_u16(r+16,(uint16_t)win_w); put_u16(r+18,(uint16_t)win_h); put_u16(r+22,1); put_u32(r+28,mask);
      put_u32(r+32,0xFF1E1E2E); put_u32(r+36,0x1|0x4|0x8|0x40|0x8000|0x20000); if(xwrite_all(r,40)<0) return 1; }
    /* CreateGC */
    { uint8_t r[20]; memset(r,0,sizeof(r)); r[0]=55; put_u16(r+2,5); put_u32(r+4,gc); put_u32(r+8,wid); put_u32(r+12,0x4); put_u32(r+16,white); if(xwrite_all(r,20)<0) return 1; }
    /* CreatePixmap */
    { uint8_t r[16]; memset(r,0,sizeof(r)); r[0]=53; r[1]=g_root_depth; put_u16(r+2,4); put_u32(r+4,pix); put_u32(r+8,wid); put_u16(r+12,(uint16_t)win_w); put_u16(r+14,(uint16_t)win_h); if(xwrite_all(r,16)<0) pix=0; g_pix=pix; }

    /* WM properties */
    { uint32_t wm_protocols = x11_intern_atom("WM_PROTOCOLS"); uint32_t wm_delete = x11_intern_atom("WM_DELETE_WINDOW"); uint32_t utf8 = x11_intern_atom("UTF8_STRING");
      const char *title = "rvemu paint"; uint32_t ttype = utf8 ? utf8 : 31;
      x11_change_property8(wid, 39, ttype, title, (uint32_t)strlen(title));
      if (wm_protocols && wm_delete) x11_change_property32(wid, wm_protocols, 4, &wm_delete, 1); }

    /* MapWindow */
    { uint8_t r[8]; memset(r,0,sizeof(r)); r[0]=8; put_u16(r+2,2); put_u32(r+4,wid); xwrite_all(r,8); }

    printf("rvemu paint: drag draw, 1-6 color, e eraser, [/] size, c clear, q quit\n"); fflush(stdout);
    { int gx,gy,gw,gh; if(get_geometry(wid,&gx,&gy,&gw,&gh)==0){ apply_resize(gw,gh); pix=g_pix; } }
    clear_canvas(wid, pix, gc);

    int drawing = 0, last_x = 0, last_y = 0;
    uint32_t wm_delete = x11_intern_atom("WM_DELETE_WINDOW");

    for (;;) {
        uint8_t ev[32];
        if (next_event(ev) < 0) break;
        uint8_t type = ev[0] & 0x7f;
        if (ev[0] == 0) continue;
        if (type == 12) {
            uint16_t count = get_u16(ev + 16);
            if (count == 0 && pix) x11_copy_area(pix, wid, gc, 0, 0, 0, 0, win_w, win_h);
        } else if (type == 22) {
            int nw = (int)get_u16(ev + 16), nh = (int)get_u16(ev + 18);
            apply_resize(nw, nh); pix = g_pix;
        } else if (type == 4) {
            uint32_t btn = ev[1]; int16_t x = (int16_t)get_u16(ev+24), y = (int16_t)get_u16(ev+26);
            if (btn == 1 && y >= BAR_H) { drawing = 1; last_x = x; last_y = y; gc_set_clip_canvas(gc); draw_brush(wid,pix,gc,x,y,cur_brush); }
            else if (btn == 1 && y < BAR_H) {
                int handled = 0;
                for (int i = 0; i < NPALETTE; i++) { int sx = 10+i*34; if (x>=sx-2 && x<sx+30) { cur_sel=i; cur_color=palette[i]; cur_eraser=0; draw_ui(wid,pix,gc); handled=1; break; } }
                if (!handled && x>=224 && x<276 && y>=8 && y<36) { cur_eraser=!cur_eraser; draw_ui(wid,pix,gc); }
                else if (!handled && x>=290 && x<318 && y>=8 && y<36) { if(cur_brush>1){cur_brush--; draw_ui(wid,pix,gc);} }
                else if (!handled && x>=350 && x<378 && y>=8 && y<36) { if(cur_brush<40){cur_brush++; draw_ui(wid,pix,gc);} }
            }
        } else if (type == 5) { if (ev[1]==1) drawing=0; }
        else if (type == 6) {
            int16_t x = (int16_t)get_u16(ev+24), y = (int16_t)get_u16(ev+26);
            if (drawing) { if (y < BAR_H) y = BAR_H; gc_set_clip_canvas(gc); draw_line_brush(wid,pix,gc,last_x,last_y,x,y,cur_brush); last_x=x; last_y=y; }
        } else if (type == 2) {
            uint8_t kc = ev[1];
            if (kc==24||kc==9) break;
            if (kc==54) { int gx,gy,gw,gh; if(get_geometry(wid,&gx,&gy,&gw,&gh)==0){apply_resize(gw,gh);pix=g_pix;} clear_canvas(wid,pix,gc); }
            if (kc>=10 && kc<=15) { cur_sel=kc-10; if(cur_sel>=NPALETTE) cur_sel=NPALETTE-1; cur_color=palette[cur_sel]; cur_eraser=0; draw_ui(wid,pix,gc); }
            if (kc==26) { cur_eraser=!cur_eraser; draw_ui(wid,pix,gc); }
            if (kc==34 && cur_brush>1) { cur_brush--; draw_ui(wid,pix,gc); }
            if (kc==35 && cur_brush<40) { cur_brush++; draw_ui(wid,pix,gc); }
        } else if (type == 33) { if (get_u32(ev+12)==wm_delete) break; }
    }

    if (pix) { uint8_t r[8]; memset(r,0,sizeof(r)); r[0]=54; put_u16(r+2,2); put_u32(r+4,pix); xwrite_all(r,8); }
    { uint8_t r[8]; memset(r,0,sizeof(r)); r[0]=4; put_u16(r+2,2); put_u32(r+4,wid); xwrite_all(r,8); }
    close(xfd); free(body);
    return 0;
}
