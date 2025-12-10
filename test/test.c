#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dbg.h"
#include "aes.h"
#include "sha256.h"

unsigned long int strlen(const char *f) {
    int i = 0;
    while (f[i] != 0)
        i++;
    return i;
}

void *memcpy(void *dest, const void *src, size_t n) {
    while (n--)
        ((uint8_t *)dest)[n] = ((uint8_t *)src)[n];
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    uint8_t *p1 = (uint8_t *)s1;
    uint8_t *p2 = (uint8_t *)s2;
    while (n--) {
        if (*p1 == *p2) {
            /* same */
        } else if (*p1 < *p2) {
            return -1;
        } else {
            return 1;
        }
        p1++;
        p2++;
    }
    return 0;
}

void bintohex(const uint8_t *bin, int binlen, char *out, int outlen) {
    const char lk[] = "0123456789abcdef";
    for (int i = 0; i < binlen; i++) {
        uint8_t h = (bin[i] >> 4) & 0xf;
        uint8_t l = (bin[i] >> 0) & 0xf;
        if (outlen-- == 0)
            return;
        *out = lk[h];
        out++;
        if (outlen-- == 0)
            return;
        *out = lk[l];
        out++;
    }
}

static inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

int test_rotr(void) {
    struct vec {
        uint32_t x;
        int n;
        uint32_t expect;
    } tests[] = {
        {0x00000000, 0, 0x00000000},  {0x00000000, 7, 0x00000000},  {0xffffffff, 0, 0xffffffff},  {0xffffffff, 12, 0xffffffff}, {0x12345678, 0, 0x12345678},
        {0x12345678, 1, 0x091a2b3c},  {0x12345678, 4, 0x81234567},  {0x12345678, 7, 0xf02468ac},  {0x12345678, 8, 0x78123456},  {0x12345678, 16, 0x56781234},
        {0x12345678, 24, 0x34567812}, {0x12345678, 31, 0x2468acf0}, {0x12345678, 32, 0x12345678}, {0x80000001, 1, 0xc0000000},  {0x80000001, 4, 0x18000000},
        {0x0f0f0f0f, 4, 0xf0f0f0f0},  {0xa5a5a5a5, 5, 0x2d2d2d2d},  {0xdeadbeef, 1, 0xef56df77},  {0xdeadbeef, 8, 0xefdeadbe},  {0xdeadbeef, 13, 0xf77ef56d},
        {0xcafebabe, 3, 0xd95fd757},  {0xcafebabe, 17, 0x5d5f657f},
    };

    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        uint32_t r = rotr(tests[i].x, tests[i].n);
        if (r != tests[i].expect) {
            DBG("FAIL: rotr(0x%08X,%2d) = 0x%08X (expected 0x%08X)", tests[i].x, tests[i].n, r, tests[i].expect);
            return 1;
        }
    }
    return 0;
}

int test_shiftl(void) {
    struct vec {
        uint32_t x;
        int n;
        uint32_t expect;
    } tests[] = {
        {0x00000000, 0, 0x00000000},  {0x00000000, 7, 0x00000000},  {0xFFFFFFFF, 0, 0xFFFFFFFF},  {0xFFFFFFFF, 12, 0xFFFFF000}, {0x12345678, 0, 0x12345678},
        {0x12345678, 1, 0x2468ACF0},  {0x12345678, 4, 0x23456780},  {0x12345678, 7, 0x1A2B3C00},  {0x12345678, 8, 0x34567800},  {0x12345678, 16, 0x56780000},
        {0x12345678, 24, 0x78000000}, {0x12345678, 31, 0x00000000}, {0x12345678, 32, 0x12345678}, {0x80000001, 1, 0x00000002},  {0x80000001, 4, 0x00000010},
        {0x0F0F0F0F, 4, 0xF0F0F0F0},  {0xA5A5A5A5, 5, 0xB4B4B4A0},  {0xDEADBEEF, 1, 0xBD5B7DDE},  {0xDEADBEEF, 8, 0xADBEEF00},  {0xDEADBEEF, 13, 0xB7DDE000},
        {0xCAFEBABE, 3, 0x57F5D5F0},  {0xCAFEBABE, 17, 0x757C0000},
    };

    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        uint32_t r = tests[i].x << tests[i].n;
        if (r != tests[i].expect) {
            DBG("FAIL: shiftl(0x%08X,%2d) = 0x%08X (expected 0x%08X)", tests[i].x, tests[i].n, r, tests[i].expect);
            return 1;
        }
    }
    return 0;
}

int test_or(void) {
    struct vec {
        uint32_t x;
        uint32_t n;
        uint32_t expect;
    } tests[] = {
        {0x00000000, 0x00000000, 0x00000000}, {0x00000000, 0x00000000, 0x00000000}, {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF}, {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF},
        {0x12345678, 0x12345678, 0x12345678}, {0x12345678, 0x091A2B3C, 0x1B3E7F7C}, {0x12345678, 0x81234567, 0x9337577F}, {0x12345678, 0xF02468AC, 0xF2347EFC},
        {0x12345678, 0x78123456, 0x7A36767E}, {0x12345678, 0x56781234, 0x567C567C}, {0x12345678, 0x34567812, 0x36767E7A}, {0x12345678, 0x2468ACF0, 0x367CFEF8},
        {0x12345678, 0x12345678, 0x12345678}, {0x80000001, 0xC0000000, 0xC0000001}, {0x80000001, 0x18000000, 0x98000001}, {0x0F0F0F0F, 0xF0F0F0F0, 0xFFFFFFFF},
        {0xA5A5A5A5, 0x2D2D2D2D, 0xADADADAD}, {0xDEADBEEF, 0xEF56DF77, 0xFFFFFFFF}, {0xDEADBEEF, 0xEFDEADBE, 0xFFFFBFFF}, {0xDEADBEEF, 0xF77EF56D, 0xFFFFFFEF},
        {0xCAFEBABE, 0xD95FD757, 0xDBFFFFFF}, {0xCAFEBABE, 0x5D5F657F, 0xDFFFFFFF},
    };

    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        uint32_t r = tests[i].x | tests[i].n;
        if (r != tests[i].expect) {
            DBG("FAIL: or(0x%08X,%2d) = 0x%08X (expected 0x%08X)", tests[i].x, tests[i].n, r, tests[i].expect);
            return 1;
        }
    }
    return 0;
}

static uint32_t r = 0xdeadbeef;

static inline int8_t rand8(uint32_t *r) {
    int i10 = !(*r & (1 << 10));
    int i12 = !(*r & (1 << 12));
    int i13 = !(*r & (1 << 13));
    int i15 = !(*r & (1 << 15));

    int p = (((i15 ^ i13) ^ i12) ^ i10);
    *r <<= 1;
    *r |= p;
    return *r % 0xff;
}

int uECC_PRNG(uint8_t *dst, unsigned size) {
    while (size--)
        *dst++ = rand8(&r);
    return 1;
}

#define TEST(name, expr, expected) \
    do { \
        uint64_t result = (expr); \
        if (result != (expected)) { \
            printf("FAIL: %s -> got %ld (0x%lx), expected %ld (0x%lx)\n", \
                   name, (int64_t)result, result, (int64_t)(expected), (uint64_t)(expected)); \
        } else { \
            printf("PASS: %s\n", name); \
        } \
    } while (0)

int main(void) {
    //    printf("test: %d\n", 1234);
    DBG("STARTING TESTS");
    test_rotr();
    test_shiftl();
    test_or();

    volatile uint32_t x = 4;
    assert(x * 2 == 8);
    assert(x << 1 == 8);
    const char *msg = "Hello World"; // a591a6d40bf420404a011733cfb7b190d62c65bf0bcda32b57b277d9ad9f146e
    assert(strlen(msg) == 11);
    // char tmp[1024];
    // bintohex(msg, 12, tmp, 1024);
    // tmp[25] = 0;
    // ecall(ECALL_DEBUG, tmp, -1);

    char hash[32];
    sha256 sha;
    sha256_init(&sha);
    sha256_append(&sha, msg, strlen(msg));
    sha256_finalize_bytes(&sha, hash);

    DBG_HEX(hash, 32, "SHA: ");

    uint8_t expectd[] = "\xa5\x91\xa6\xd4\x0b\xf4\x20\x40\x4a\x01\x17\x33\xcf\xb7\xb1\x90\xd6\x2c"
                        "\x65\xbf\x0b\xcd\xa3\x2b\x57\xb2\x77\xd9\xad\x9f\x14\x6e";
    if (memcmp(expectd, hash, 32) != 0) {
        DBG("sha256 FAILED");
    }

    uint8_t *aeskey = expectd;

    struct AES_ctx ctx;
    AES_init_ctx(&ctx, aeskey);

    uint8_t pt[16+1] = "0123456789abcdef";
    DBG_HEX(pt, 16, "AES PT: ");
    AES_ECB_encrypt(&ctx, pt);
    DBG_HEX(pt, 16, "AES CT: ");


    //   MUL (low 64 bits)
    TEST("MUL signed", ({ int64_t a = 5, b = -7; (uint64_t)(a * b); }), (uint64_t)(-35));

    //   MULH and MULHU and MULHSU (high 64 bits)
    TEST("MULH signed x signed high",
         ({
             int64_t a = 0x7FFFFFFFFFFFFFFF;
             int64_t b = 0x7FFFFFFFFFFFFFFF;
             (__int128)a * (__int128)b >> 64;
         }),
         (uint64_t)0x3fffffffffffffff);

    TEST("MULHU unsigned x unsigned high",
         ({
             uint64_t a = 0xFFFFFFFFFFFFFFFFULL;
             uint64_t b = 2;
             ((__uint128_t)a * (__uint128_t)b) >> 64;
         }),
         (uint64_t)0x1);

    TEST("MULHSU signed x unsigned high",
         ({
             int64_t a = -5;
             uint64_t b = 10;
             (((__int128)a * (__uint128_t)b) >> 64);
         }),
         (uint64_t)-1);

    //   DIV
    TEST("DIV signed", ({ int64_t a = -100, b = 8; (uint64_t)(a / b); }), (uint64_t)(-12));

    TEST("DIV divide by zero -> -1",
         ({ int64_t a = 123, b = 0; (uint64_t)(b == 0 ? -1 : a / b); }),
         (uint64_t)-1);

    //   DIVU
    TEST("DIVU unsigned", ({ uint64_t a = 100, b = 7; a / b; }), 14);

    TEST("DIVU divide by zero -> 2^64-1",
         ({ uint64_t a = 100, b = 0; (b == 0) ? ~0ULL : a / b; }),
         ~0ULL);

    //   REM
    TEST("REM signed", ({ int64_t a = -100, b = 30; (uint64_t)(a % b); }), (uint64_t)-10);

    TEST("REM divide by zero -> dividend",
         ({ int64_t a = 55, b = 0; (uint64_t)(b == 0 ? a : a % b); }),
         55ULL);

    //   REMU
    TEST("REMU unsigned", ({ uint64_t a = 100, b = 30; a % b; }), 10);

    TEST("REMU div0 -> dividend",
         ({ uint64_t a = 77, b = 0; (b == 0 ? a : a % b); }),
         77ULL);
    return 0;
}
