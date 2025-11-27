#pragma once

#include <stdint.h>

void dbg(unsigned int line, const char *fmt, ...);
void dbg_hex(unsigned int line, void *buf, uint32_t len, const char *fmt, ...);
#define DBG(...) dbg(__LINE__, __VA_ARGS__)
#define DBG_HEX(buf, sz, ...) dbg_hex(__LINE__, buf, sz, __VA_ARGS__)
