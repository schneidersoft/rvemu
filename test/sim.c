#ifndef SIM_H
#define SIM_H

#include <stdio.h>

// execute ecall(cmd, ptr, len) -> the host will know what to do
static inline void ecall(unsigned cmd, const void *ptr, int len) {
    register unsigned x10 __asm__("a0") = cmd;
    register const void *x11 __asm__("a1") = ptr;
    register int x12 __asm__("a2") = len;
    __asm__ volatile("ecall" : "+r"(x10), "+r"(x11), "+r"(x12) : : "memory");
}

#define ECALL_DEBUG 0
#define ECALL_ASSERT 1
#define ECALL_PUTCHAR 2

void __assert_func(const char *filename, int line, const char *assert_func, const char *expr) {
    printf("%s %d %s FAILED: %s", filename, line, assert_func, expr);
    __asm__ volatile("ebreak" : : : "memory");
    while (1)
        ;
}

void _putchar(char character) { ecall(ECALL_PUTCHAR, (void *)(unsigned long)character, -1); }

#endif
