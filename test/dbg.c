#include "dbg.h"

#include <stdarg.h>
#include <stdio.h>

void dbg(unsigned int line, const char *fmt, ...) {
    fprintf(stderr, "%d: ", line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
}

void dbg_hex(unsigned int line, void *buf, uint32_t len, const char *fmt, ...) {
    fprintf(stderr, "%d: ", line);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    while (len--) {
        fprintf(stderr, "%02x", *(uint8_t *)buf);
        buf = ((uint8_t *)buf) + 1;
    }
    fprintf(stderr, "\n");
}
