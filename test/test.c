#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dbg.h"
#include "aes.h"
#include "sha256.h"
#include "uECC.h"

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

// int printf(const char *msg, ...) {
//     ecall(ECALL_DEBUG, msg, -1);
//     return -;
// }

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

char *spigot(void) {
    int i, j, k, q, x = 0;
    int len, nines = 0, predigit = 0;
    int precision = 1001;
    char buff[10 + 2];
    static char res[2000];
    int respos = 0;

    // calculate array length and create empty array
    len = (10 * precision / 3) + 1;
    int a[len];

    // initialize array with all 2's
    for (i = 0; i < len; i = i + 1) {
        a[i] = 2;
    }

    // repeat calculation loop 'precision' times - depends on desired precision
    for (j = 1; j <= precision; j = j + 1) {
        q = 0;

        // calculate q
        for (i = len; i > 0; i = i - 1) {
            x = 10 * a[i - 1] + q * i;
            a[i - 1] = x % (2 * i - 1);
            q = x / (2 * i - 1);
        }

        a[0] = q % 10;
        q = q / 10;

        // append different digits based on q value
        if (q == 9) {
            // if q is 9, increment nines counter
            nines = nines + 1;
        } else if (q == 10) {
            // if q is 10 (overflow case), write 9 then predigit + 1
            sprintf(buff, "%d", predigit + 1);
            for (int i = 0; buff[i] != 0; i++)
                res[respos++] = buff[i];
            // strcat(res, buff);

            for (k = 0; k < nines; k = k + 1) {
                sprintf(buff, "%d", 0);
                // strcat(res, buff);
                for (int i = 0; buff[i] != 0; i++)
                    res[respos++] = buff[i];
            }

            predigit = 0;
            nines = 0;
        } else {
            // if q is not 9 or 10, print predigit
            sprintf(buff, "%d", predigit);
            // strcat(res, buff);
            for (int i = 0; buff[i] != 0; i++)
                res[respos++] = buff[i];

            // advance predigit to next q
            predigit = q % 10;

            // handle nines which were tracked
            if (nines != 0) {
                for (k = 0; k < nines; k = k + 1) {
                    sprintf(buff, "%d", 9);
                    // strcat(res, buff);
                    for (int i = 0; buff[i] != 0; i++)
                        res[respos++] = buff[i];
                }
                nines = 0;
            }
        }
    }

    // add the final digit
    sprintf(buff, "%d9", predigit);
    // strcat(res, buff);
    for (int i = 0; buff[i] != 0; i++)
        res[respos++] = buff[i];

    // return the pi result in string format
    return res;
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

    //    char *pi = spigot();
    //    DBG("pi: %s", pi);

    uint8_t *aeskey = expectd;

    struct AES_ctx ctx;
    AES_init_ctx(&ctx, aeskey);

    uint8_t pt[16+1] = "0123456789abcdef";
    DBG_HEX(pt, 16, "AES PT: ");
    AES_ECB_encrypt(&ctx, pt);
    DBG_HEX(pt, 16, "AES CT: ");


    uECC_set_rng(uECC_PRNG);

    uint8_t pub[64];
    uint8_t key[32];

    if (!uECC_make_key(pub, key, uECC_secp256k1())) {
        DBG("uECC_make_key");
    }

    DBG_HEX(pub, sizeof(pub), "PUB: ");
    DBG_HEX(key, sizeof(key), "KEY: ");

    uint8_t digest[32];
    for (int i = 0; i < 32; i++) digest[i] = rand8(&r);

    DBG_HEX(digest, sizeof(digest), "DIG: ");

    uint8_t signature[64];

    if (!uECC_sign(key, digest, 32, signature, uECC_secp256k1())) {
        DBG("uECC_sign");
    }

    DBG_HEX(signature, sizeof(signature), "SIG: ");

    if (!uECC_verify(pub, digest, 64, signature, uECC_secp256k1())) {
        DBG("SIGNATURE NOT OK");
    } else {
        DBG("SIGNATURE OK");
    }

    //    char tmp[] =
    //    "031415926535897932384626433832795028841971693993751058209749445923078164062862089986280348253421170679821480865132823066470938446095505822317253594081284811174502841027019385211055596446229489549303819644288109756659334461284756482337867831652712019091456485669234603486104543266482133936072602491412737245870066063155881748815209209628292540917153643678925903600113305305488204665213841469519415116094330572703657595919530921861173819326117931051185480744623799627495673518857527248912279381830119491298336733624406566430860213949463952247371907021798609437027705392171762931767523846748184676694051320005681271452635608277857713427577896091736371787214684409012249534301465495853710507922796892589235420199561121290219608640344181598136297747713099605187072113499999983729780499510597317328160963185950244594553469083026425223082533446850352619311881710100031378387528865875332083814206171776691473035982534904287554687311595628638823537875937519577818577805321712268066130019278766111959092164201989";
    //    if (memcmp(tmp, pi, sizeof(tmp)) != 0) {
    //        DBG("spigot FAILED");
    //    }

    return 0;
}
