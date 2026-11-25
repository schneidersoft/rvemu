#include "x11.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>

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

#define W 640
#define H 400
#define PAD_W 12
#define PAD_H 80
#define BALL 10

static int clampi(int v,int lo,int hi){ if(v<lo) return lo; if(v>hi) return hi; return v; }

int main(void) {
    struct sockaddr_un addr;
    uint8_t setup[12], sh[8], *body;
    uint16_t addlen, vendor_len; uint8_t nfmt; size_t off;
    uint32_t rid, root, white, wid, gc;

    xfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (xfd < 0) { perror("socket"); return 1; }
    memset(&addr,0,sizeof(addr)); addr.sun_family=AF_UNIX; strcpy(addr.sun_path,"/tmp/.X11-unix/X0");
    if (connect(xfd,(struct sockaddr*)&addr,sizeof(addr))<0){ perror("connect"); return 1; }

    memset(setup,0,sizeof(setup)); setup[0]='l'; setup[2]=11;
    if(xwrite_all(setup,12)<0||xread_all(sh,8)<0||sh[0]!=1){ fprintf(stderr,"X11 setup failed\n"); return 1; }
    addlen=get_u16(sh+6); body=(uint8_t*)malloc((size_t)addlen*4); if(!body||xread_all(body,(size_t)addlen*4)<0) return 1;
    rid=get_u32(body+4); vendor_len=get_u16(body+16); nfmt=body[21];
    off=32+(((size_t)vendor_len+3)&~(size_t)3)+(size_t)nfmt*8;
    root=get_u32(body+off); white=get_u32(body+off+20);
    uint8_t root_depth = body[off+38];
    wid=rid; gc=rid+1; uint32_t pix=rid+2;

    /* CreateWindow */
    { uint8_t r[40]; uint32_t mask=0x2|0x800; memset(r,0,sizeof(r)); r[0]=1; put_u16(r+2,10); put_u32(r+4,wid); put_u32(r+8,root);
      put_u16(r+12,100); put_u16(r+14,100); put_u16(r+16,W); put_u16(r+18,H); put_u16(r+22,1); put_u32(r+28,mask);
      put_u32(r+32,0xFF101018); put_u32(r+36,0x1|0x2|0x8000|0x20000); if(xwrite_all(r,40)<0) return 1; }
    /* CreateGC */
    { uint8_t r[20]; memset(r,0,sizeof(r)); r[0]=55; put_u16(r+2,5); put_u32(r+4,gc); put_u32(r+8,wid); put_u32(r+12,0x4); put_u32(r+16,white); if(xwrite_all(r,20)<0) return 1; }
    /* CreatePixmap backbuffer */
    { uint8_t r[16]; memset(r,0,sizeof(r)); r[0]=53; r[1]=root_depth; put_u16(r+2,4); put_u32(r+4,pix); put_u32(r+8,wid); put_u16(r+12,W); put_u16(r+14,H); if(xwrite_all(r,16)<0) pix=wid; }
    /* WM_NAME */
    { const char*name="rvemu pong"; uint8_t h[24]; size_t n=strlen(name); size_t len=6+(n+pad4(n))/4; memset(h,0,sizeof(h)); h[0]=18; put_u16(h+2,(uint16_t)len); put_u32(h+4,wid); put_u32(h+8,39); put_u32(h+12,31); h[16]=8; put_u32(h+20,(uint32_t)n); xwrite_all(h,24); xwrite_pad(name,n); }
    /* MapWindow */
    { uint8_t r[8]; memset(r,0,sizeof(r)); r[0]=8; put_u16(r+2,2); put_u32(r+4,wid); xwrite_all(r,8); }

    /* non-blocking for game loop */
    fcntl(xfd, F_SETFL, O_NONBLOCK);

    int py = H/2 - PAD_H/2, ay = H/2 - PAD_H/2;
    int bx = W/2, by = H/2, vx = 3, vy = 2;
    int pscore = 0, ascore = 0;
    int up = 0, down = 0;

    printf("rvemu pong: W/S or Up/Down to move, q/Esc quit\n"); fflush(stdout);

    for (;;) {
        /* drain events */
        uint8_t ev[32]; int pr;
        while ((pr = poll_event(ev)) == 1) {
            uint8_t type = ev[0] & 0x7f;
            if (type == 2) { /* KeyPress */
                uint8_t kc = ev[1];
                if (kc==24||kc==9) goto quit; /* q/Esc */
                if (kc==25||kc==111) up=1;    /* w / Up */
                if (kc==39||kc==116) down=1;  /* s / Down */
            } else if (type == 3) { /* KeyRelease */
                uint8_t kc = ev[1];
                if (kc==25||kc==111) up=0;
                if (kc==39||kc==116) down=0;
            } else if (type == 12) {
                if (get_u16(ev+16)==0 && pix!=wid) { uint8_t r[28]; memset(r,0,sizeof(r)); r[0]=62; put_u16(r+2,7); put_u32(r+4,pix); put_u32(r+8,wid); put_u32(r+12,gc); put_u16(r+24,W); put_u16(r+26,H); xwrite_all(r,28); }
            } else if (type == 33) { goto quit; }
        }
        if (pr < 0) break;

        /* update */
        if (up) py -= 5;
        if (down) py += 5;
        py = clampi(py, 0, H-PAD_H);
        /* simple AI */
        int ac = ay + PAD_H/2;
        if (ac < by - 10) ay += 3;
        else if (ac > by + 10) ay -= 3;
        ay = clampi(ay, 0, H-PAD_H);

        bx += vx; by += vy;
        if (by <= 0) { by = 0; vy = -vy; }
        if (by >= H-BALL) { by = H-BALL; vy = -vy; }
        /* paddle collisions */
        if (bx <= 20+PAD_W && bx >= 20 && by+BALL >= py && by <= py+PAD_H) { bx=20+PAD_W; vx = -vx; if(vx<12) vx++; }
        if (bx+BALL >= W-20-PAD_W && bx+BALL <= W-20 && by+BALL >= ay && by <= ay+PAD_H) { bx=W-20-PAD_W-BALL; vx = -vx; if(vx>-12) vx--; }
        if (bx < -20) { ascore++; bx=W/2; by=H/2; vx=3; vy=(ascore&1)?2:-2; }
        if (bx > W+20) { pscore++; bx=W/2; by=H/2; vx=-3; vy=(pscore&1)?-2:2; }

        /* render */
        uint32_t d = pix;
        x11_change_gc_fg(gc, 0xFF101018); x11_fill_rect(d,gc,0,0,W,H);
        x11_change_gc_fg(gc, 0xFF404060);
        for(int y=0;y<H;y+=20) x11_fill_rect(d,gc,W/2-2,y,4,10);
        x11_change_gc_fg(gc, 0xFF50FA7B); x11_fill_rect(d,gc,20,py,PAD_W,PAD_H);
        x11_change_gc_fg(gc, 0xFFFF5555); x11_fill_rect(d,gc,W-20-PAD_W,ay,PAD_W,PAD_H);
        x11_change_gc_fg(gc, 0xFFFFFFFF); x11_fill_rect(d,gc,bx,by,BALL,BALL);
        char score[32]; snprintf(score,sizeof(score),"%d   %d", pscore, ascore);
        x11_text8(d,gc,W/2-24,30,score);
        x11_text8(d,gc,20,H-20,"W/S move");
        if (pix!=wid) { uint8_t cr[28]; memset(cr,0,sizeof(cr)); cr[0]=62; put_u16(cr+2,7); put_u32(cr+4,pix); put_u32(cr+8,wid); put_u32(cr+12,gc); put_u16(cr+24,W); put_u16(cr+26,H); xwrite_all(cr,28); }

        struct timespec ts; ts.tv_sec=0; ts.tv_nsec=16000000;
        nanosleep(&ts,NULL);
    }

quit:
    if (pix!=wid) { uint8_t r[8]; memset(r,0,sizeof(r)); r[0]=54; put_u16(r+2,2); put_u32(r+4,pix); xwrite_all(r,8); }
    { uint8_t r[8]; memset(r,0,sizeof(r)); r[0]=4; put_u16(r+2,2); put_u32(r+4,wid); xwrite_all(r,8); }
    close(xfd); free(body);
    printf("Final score %d - %d\n", pscore, ascore);
    return 0;
}
