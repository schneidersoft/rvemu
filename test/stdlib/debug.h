// execute ecall(cmd, ptr, len) -> the host will know what to do
static inline void debug(const char *msg) {
    register unsigned x10 __asm__("a0") = 0;
    register const void *x11 __asm__("a1") = msg;
    register int x12 __asm__("a2") = -1;
    __asm__ volatile("ecall" : "+r"(x10), "+r"(x11), "+r"(x12) : : "memory");
}
