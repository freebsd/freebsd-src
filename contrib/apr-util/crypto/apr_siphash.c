/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * SipHash (C reference implementation, APR-ized), originating from:
 *      https://131002.net/siphash/siphash24.c.
 */

#include "apr_siphash.h"

#define ROTL64(x, n) (((x) << (n)) | ((x) >> (64 - (n))))

#define U8TO64_LE(p) \
    (((apr_uint64_t)((p)[0])      ) | \
     ((apr_uint64_t)((p)[1]) <<  8) | \
     ((apr_uint64_t)((p)[2]) << 16) | \
     ((apr_uint64_t)((p)[3]) << 24) | \
     ((apr_uint64_t)((p)[4]) << 32) | \
     ((apr_uint64_t)((p)[5]) << 40) | \
     ((apr_uint64_t)((p)[6]) << 48) | \
     ((apr_uint64_t)((p)[7]) << 56))

#define U64TO8_LE(p, v) \
do { \
    (p)[0] = (unsigned char)((v)      ); \
    (p)[1] = (unsigned char)((v) >>  8); \
    (p)[2] = (unsigned char)((v) >> 16); \
    (p)[3] = (unsigned char)((v) >> 24); \
    (p)[4] = (unsigned char)((v) >> 32); \
    (p)[5] = (unsigned char)((v) >> 40); \
    (p)[6] = (unsigned char)((v) >> 48); \
    (p)[7] = (unsigned char)((v) >> 56); \
} while (0)

#define SIPROUND() \
do { \
    v0 += v1; v1=ROTL64(v1,13); v1 ^= v0; v0=ROTL64(v0,32); \
    v2 += v3; v3=ROTL64(v3,16); v3 ^= v2; \
    v0 += v3; v3=ROTL64(v3,21); v3 ^= v0; \
    v2 += v1; v1=ROTL64(v1,17); v1 ^= v2; v2=ROTL64(v2,32); \
} while(0)

#define SIPHASH(r, s, n, k) \
do { \
    const unsigned char *ptr, *end; \
    apr_uint64_t v0, v1, v2, v3, m; \
    apr_uint64_t k0, k1; \
    unsigned int rem; \
    \
    k0 = U8TO64_LE(k + 0); \
    k1 = U8TO64_LE(k + 8); \
    v3 = k1 ^ (apr_uint64_t)0x7465646279746573ULL; \
    v2 = k0 ^ (apr_uint64_t)0x6c7967656e657261ULL; \
    v1 = k1 ^ (apr_uint64_t)0x646f72616e646f6dULL; \
    v0 = k0 ^ (apr_uint64_t)0x736f6d6570736575ULL; \
    \
    rem = (unsigned int)(n & 0x7); \
    for (ptr = s, end = ptr + n - rem; ptr < end; ptr += 8) { \
        m = U8TO64_LE(ptr); \
        v3 ^= m; \
        cROUNDS \
        v0 ^= m; \
    } \
    m = (apr_uint64_t)(n & 0xff) << 56; \
    switch (rem) { \
        case 7: m |= (apr_uint64_t)ptr[6] << 48; \
        case 6: m |= (apr_uint64_t)ptr[5] << 40; \
        case 5: m |= (apr_uint64_t)ptr[4] << 32; \
        case 4: m |= (apr_uint64_t)ptr[3] << 24; \
        case 3: m |= (apr_uint64_t)ptr[2] << 16; \
        case 2: m |= (apr_uint64_t)ptr[1] << 8; \
        case 1: m |= (apr_uint64_t)ptr[0]; \
        case 0: break; \
    } \
    v3 ^= m; \
    cROUNDS \
    v0 ^= m; \
    \
    v2 ^= 0xff; \
    dROUNDS \
    \
    r = v0 ^ v1 ^ v2 ^ v3; \
} while (0)

APU_DECLARE(apr_uint64_t) apr_siphash(const void *src, apr_size_t len,
                              const unsigned char key[APR_SIPHASH_KSIZE],
                                      unsigned int c, unsigned int d)
{
    apr_uint64_t h;
    unsigned int i;

#undef  cROUNDS
#define cROUNDS \
        for (i = 0; i < c; ++i) { \
            SIPROUND(); \
        }

#undef  dROUNDS
#define dROUNDS \
        for (i = 0; i < d; ++i) { \
            SIPROUND(); \
        }

    SIPHASH(h, src, len, key);
    return h;
}

APU_DECLARE(void) apr_siphash_auth(unsigned char out[APR_SIPHASH_DSIZE],
                                   const void *src, apr_size_t len,
                             const unsigned char key[APR_SIPHASH_KSIZE],
                                   unsigned int c, unsigned int d)
{
    apr_uint64_t h;
    h = apr_siphash(src, len, key, c, d);
    U64TO8_LE(out, h);
}

APU_DECLARE(apr_uint64_t) apr_siphash24(const void *src, apr_size_t len,
                               const unsigned char key[APR_SIPHASH_KSIZE])
{
    apr_uint64_t h;

#undef  cROUNDS
#define cROUNDS \
        SIPROUND(); \
        SIPROUND();

#undef  dROUNDS
#define dROUNDS \
        SIPROUND(); \
        SIPROUND(); \
        SIPROUND(); \
        SIPROUND();

    SIPHASH(h, src, len, key);
    return h;
}

APU_DECLARE(void) apr_siphash24_auth(unsigned char out[APR_SIPHASH_DSIZE],
                                     const void *src, apr_size_t len,
                               const unsigned char key[APR_SIPHASH_KSIZE])
{
    apr_uint64_t h;
    h = apr_siphash24(src, len, key);
    U64TO8_LE(out, h);
}

APU_DECLARE(apr_uint64_t) apr_siphash48(const void *src, apr_size_t len,
                               const unsigned char key[APR_SIPHASH_KSIZE])
{
    apr_uint64_t h;

#undef  cROUNDS
#define cROUNDS \
        SIPROUND(); \
        SIPROUND(); \
        SIPROUND(); \
        SIPROUND();

#undef  dROUNDS
#define dROUNDS \
        SIPROUND(); \
        SIPROUND(); \
        SIPROUND(); \
        SIPROUND(); \
        SIPROUND(); \
        SIPROUND(); \
        SIPROUND(); \
        SIPROUND();

    SIPHASH(h, src, len, key);
    return h;
}

APU_DECLARE(void) apr_siphash48_auth(unsigned char out[APR_SIPHASH_DSIZE],
                                     const void *src, apr_size_t len,
                               const unsigned char key[APR_SIPHASH_KSIZE])
{
    apr_uint64_t h;
    h = apr_siphash48(src, len, key);
    U64TO8_LE(out, h);
}

