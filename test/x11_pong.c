/*
 * Simple X11 Pong for rvemu guest.
 *
 * Zero external dependencies: raw X11 protocol over Unix socket.
 * Build:  make bin/x11_pong.bin
 * Run:    ./bin/riscv64i bin/x11_pong.bin
 *
 * Controls: W/S or Up/Down = left paddle, q/Esc = quit
 */

#include "x11.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define W 640
#define H 400
#define PAD_W 12
#define PAD_H 80
#define BALL 10

static uint8_t evbuf[32];
static int evbuf_len = 0;

/* Non-blocking event poll: 1=event, 0=none, -1=error/disconnect */
static int poll_event(uint8_t ev[32]) {
    while (evbuf_len < 32) {
        ssize_t r = read(xfd, evbuf + evbuf_len, 32 - evbuf_len);
        if (r > 0) { evbuf_len += (int)r; continue; }
        if (r == 0) return -1;
        if (errno == EAGAIN) return 0;
        return -1;
    }
    memcpy(ev, evbuf, 32);
    evbuf_len = 0;
    return 1;
}

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int main(void) {
    X11 x;
    if (x11_open_default(&x) < 0) { fprintf(stderr, "x11 open failed\n"); return 1; }

    uint32_t wid = x11_new_id(&x);
    uint32_t gc = x11_new_id(&x);
    uint32_t pix = x11_new_id(&x);

    uint32_t evmask = X11_MASK_KEY_PRESS | X11_MASK_KEY_RELEASE | X11_MASK_EXPOSURE | X11_MASK_STRUCTURE_NOTIFY;
    if (x11_create_window(wid, x.root, 100, 100, W, H, 0xFF101018, evmask) < 0) return 1;
    if (x11_create_gc(gc, wid, X11_GC_FOREGROUND, x.white) < 0) return 1;
    if (x11_create_pixmap(pix, wid, W, H, x.root_depth) < 0) pix = wid;

    x11_set_title(wid, "rvemu pong");
    x11_map_window(wid);
    fcntl(xfd, F_SETFL, O_NONBLOCK);

    int py = H / 2 - PAD_H / 2, ay = H / 2 - PAD_H / 2;
    int bx = W / 2, by = H / 2, vx = 3, vy = 2;
    int pscore = 0, ascore = 0;
    int up = 0, down = 0;

    printf("rvemu pong: W/S or Up/Down to move, q/Esc quit\n");
    fflush(stdout);

    for (;;) {
        uint8_t ev[32];
        int pr;
        while ((pr = poll_event(ev)) == 1) {
            uint8_t type = ev[0] & 0x7f;
            if (type == X11_EV_KEY_PRESS) {
                uint8_t kc = ev[1];
                if (kc == 24 || kc == 9) goto quit;
                if (kc == 25 || kc == 111) up = 1;
                if (kc == 39 || kc == 116) down = 1;
            } else if (type == X11_EV_KEY_RELEASE) {
                uint8_t kc = ev[1];
                if (kc == 25 || kc == 111) up = 0;
                if (kc == 39 || kc == 116) down = 0;
            } else if (type == X11_EV_EXPOSE) {
                if (get_u16(ev + 16) == 0 && pix != wid)
                    x11_copy_area(pix, wid, gc, 0, 0, 0, 0, W, H);
            } else if (type == X11_EV_CLIENT_MESSAGE) {
                goto quit;
            }
        }
        if (pr < 0) break;

        if (up) py -= 5;
        if (down) py += 5;
        py = clampi(py, 0, H - PAD_H);
        int ac = ay + PAD_H / 2;
        if (ac < by - 10) ay += 3;
        else if (ac > by + 10) ay -= 3;
        ay = clampi(ay, 0, H - PAD_H);

        bx += vx; by += vy;
        if (by <= 0) { by = 0; vy = -vy; }
        if (by >= H - BALL) { by = H - BALL; vy = -vy; }
        if (bx <= 20 + PAD_W && bx >= 20 && by + BALL >= py && by <= py + PAD_H) { bx = 20 + PAD_W; vx = -vx; if (vx < 12) vx++; }
        if (bx + BALL >= W - 20 - PAD_W && bx + BALL <= W - 20 && by + BALL >= ay && by <= ay + PAD_H) { bx = W - 20 - PAD_W - BALL; vx = -vx; if (vx > -12) vx--; }
        if (bx < -20) { ascore++; bx = W / 2; by = H / 2; vx = 3; vy = (ascore & 1) ? 2 : -2; }
        if (bx > W + 20) { pscore++; bx = W / 2; by = H / 2; vx = -3; vy = (pscore & 1) ? -2 : 2; }

        uint32_t d = pix;
        x11_change_gc_fg(gc, 0xFF101018); x11_fill_rect(d, gc, 0, 0, W, H);
        x11_change_gc_fg(gc, 0xFF404060);
        for (int y = 0; y < H; y += 20) x11_fill_rect(d, gc, W / 2 - 2, y, 4, 10);
        x11_change_gc_fg(gc, 0xFF50FA7B); x11_fill_rect(d, gc, 20, py, PAD_W, PAD_H);
        x11_change_gc_fg(gc, 0xFFFF5555); x11_fill_rect(d, gc, W - 20 - PAD_W, ay, PAD_W, PAD_H);
        x11_change_gc_fg(gc, 0xFFFFFFFF); x11_fill_rect(d, gc, bx, by, BALL, BALL);
        char score[32]; snprintf(score, sizeof(score), "%d   %d", pscore, ascore);
        x11_text8(d, gc, W / 2 - 24, 30, score);
        x11_text8(d, gc, 20, H - 20, "W/S move");
        if (pix != wid) x11_copy_area(pix, wid, gc, 0, 0, 0, 0, W, H);

        struct timespec ts; ts.tv_sec = 0; ts.tv_nsec = 16000000;
        nanosleep(&ts, NULL);
    }

quit:
    if (pix != wid) x11_free_pixmap(pix);
    x11_destroy_window(wid);
    x11_close(&x);
    printf("Final score %d - %d\n", pscore, ascore);
    return 0;
}
