/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#undef NDEBUG
#include "k5-platform.h"

int main ()
{
    /* Test some low-level assumptions the Kerberos code depends
       on.  */

    union {
        uint64_t n64;
        uint32_t n32;
        uint16_t n16;
        unsigned char b[9];
    } u;
    static unsigned char buf[9] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };

    assert(load_64_be(buf+1) == 0x0102030405060708LL);
    assert(load_64_le(buf+1) == 0x0807060504030201LL);
    assert(load_32_le(buf+2) == 0x05040302);
    assert(load_32_be(buf+2) == 0x02030405);
    assert(load_16_be(buf+3) == 0x0304);
    assert(load_16_le(buf+3) == 0x0403);
    u.b[0] = 0;
    assert((store_64_be(0x0102030405060708LL, u.b+1), !memcmp(buf, u.b, 9)));
    u.b[1] = 9;
    assert((store_64_le(0x0807060504030201LL, u.b+1), !memcmp(buf, u.b, 9)));
    u.b[2] = 10;
    assert((store_32_be(0x02030405, u.b+2), !memcmp(buf, u.b, 9)));
    u.b[3] = 11;
    assert((store_32_le(0x05040302, u.b+2), !memcmp(buf, u.b, 9)));
    u.b[4] = 12;
    assert((store_16_be(0x0304, u.b+3), !memcmp(buf, u.b, 9)));
    u.b[4] = 13;
    assert((store_16_le(0x0403, u.b+3), !memcmp(buf, u.b, 9)));
    /* Verify that load_*_n properly does native format.  Assume
       the unaligned thing is okay.  */
    u.n64 = 0x090a0b0c0d0e0f00LL;
    assert(load_64_n((unsigned char *) &u.n64) == 0x090a0b0c0d0e0f00LL);
    u.n32 = 0x06070809;
    assert(load_32_n((unsigned char *) &u.n32) == 0x06070809);
    u.n16 = 0x0a0b;
    assert(load_16_n((unsigned char *) &u.n16) == 0x0a0b);

    return 0;
}
