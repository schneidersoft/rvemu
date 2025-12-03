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

int main(void) {
    //    printf("test: %d\n", 1234);
    DBG("STGARTING TESTS");
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

    return 0;
}
