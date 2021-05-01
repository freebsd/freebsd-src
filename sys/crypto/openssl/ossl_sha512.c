/*
 * Copyright 2004-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/libkern.h>
#include <sys/malloc.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform_auth.h>

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_sha.h>

/* sha512-x86_64.S */
void sha512_block_data_order(SHA512_CTX *c, const void *in, size_t num);

/* From crypto/sha/sha512.c */

#if defined(__i386__) || defined(__amd64__) || defined(__aarch64__)
# define SHA512_BLOCK_CAN_MANAGE_UNALIGNED_DATA
#endif

static void
ossl_sha384_init(void *c_)
{
    SHA512_CTX *c = c_;
    c->h[0] = U64(0xcbbb9d5dc1059ed8);
    c->h[1] = U64(0x629a292a367cd507);
    c->h[2] = U64(0x9159015a3070dd17);
    c->h[3] = U64(0x152fecd8f70e5939);
    c->h[4] = U64(0x67332667ffc00b31);
    c->h[5] = U64(0x8eb44a8768581511);
    c->h[6] = U64(0xdb0c2e0d64f98fa7);
    c->h[7] = U64(0x47b5481dbefa4fa4);

    c->Nl = 0;
    c->Nh = 0;
    c->num = 0;
    c->md_len = SHA384_DIGEST_LENGTH;
}

static void
ossl_sha512_init(void *c_)
{
    SHA512_CTX *c = c_;
    c->h[0] = U64(0x6a09e667f3bcc908);
    c->h[1] = U64(0xbb67ae8584caa73b);
    c->h[2] = U64(0x3c6ef372fe94f82b);
    c->h[3] = U64(0xa54ff53a5f1d36f1);
    c->h[4] = U64(0x510e527fade682d1);
    c->h[5] = U64(0x9b05688c2b3e6c1f);
    c->h[6] = U64(0x1f83d9abfb41bd6b);
    c->h[7] = U64(0x5be0cd19137e2179);

    c->Nl = 0;
    c->Nh = 0;
    c->num = 0;
    c->md_len = SHA512_DIGEST_LENGTH;
}

static void
ossl_sha512_final(uint8_t *md, void *c_)
{
    SHA512_CTX *c = c_;
    unsigned char *p = (unsigned char *)c->u.p;
    size_t n = c->num;

    p[n] = 0x80;                /* There always is a room for one */
    n++;
    if (n > (sizeof(c->u) - 16)) {
        memset(p + n, 0, sizeof(c->u) - n);
        n = 0;
        sha512_block_data_order(c, p, 1);
    }

    memset(p + n, 0, sizeof(c->u) - 16 - n);
#if _BYTE_ORDER == _BIG_ENDIAN
    c->u.d[SHA_LBLOCK - 2] = c->Nh;
    c->u.d[SHA_LBLOCK - 1] = c->Nl;
#else
    p[sizeof(c->u) - 1] = (unsigned char)(c->Nl);
    p[sizeof(c->u) - 2] = (unsigned char)(c->Nl >> 8);
    p[sizeof(c->u) - 3] = (unsigned char)(c->Nl >> 16);
    p[sizeof(c->u) - 4] = (unsigned char)(c->Nl >> 24);
    p[sizeof(c->u) - 5] = (unsigned char)(c->Nl >> 32);
    p[sizeof(c->u) - 6] = (unsigned char)(c->Nl >> 40);
    p[sizeof(c->u) - 7] = (unsigned char)(c->Nl >> 48);
    p[sizeof(c->u) - 8] = (unsigned char)(c->Nl >> 56);
    p[sizeof(c->u) - 9] = (unsigned char)(c->Nh);
    p[sizeof(c->u) - 10] = (unsigned char)(c->Nh >> 8);
    p[sizeof(c->u) - 11] = (unsigned char)(c->Nh >> 16);
    p[sizeof(c->u) - 12] = (unsigned char)(c->Nh >> 24);
    p[sizeof(c->u) - 13] = (unsigned char)(c->Nh >> 32);
    p[sizeof(c->u) - 14] = (unsigned char)(c->Nh >> 40);
    p[sizeof(c->u) - 15] = (unsigned char)(c->Nh >> 48);
    p[sizeof(c->u) - 16] = (unsigned char)(c->Nh >> 56);
#endif

    sha512_block_data_order(c, p, 1);

    switch (c->md_len) {
    /* Let compiler decide if it's appropriate to unroll... */
    case SHA224_DIGEST_LENGTH:
        for (n = 0; n < SHA224_DIGEST_LENGTH / 8; n++) {
            SHA_LONG64 t = c->h[n];

            *(md++) = (unsigned char)(t >> 56);
            *(md++) = (unsigned char)(t >> 48);
            *(md++) = (unsigned char)(t >> 40);
            *(md++) = (unsigned char)(t >> 32);
            *(md++) = (unsigned char)(t >> 24);
            *(md++) = (unsigned char)(t >> 16);
            *(md++) = (unsigned char)(t >> 8);
            *(md++) = (unsigned char)(t);
        }
        /*
         * For 224 bits, there are four bytes left over that have to be
         * processed separately.
         */
        {
            SHA_LONG64 t = c->h[SHA224_DIGEST_LENGTH / 8];

            *(md++) = (unsigned char)(t >> 56);
            *(md++) = (unsigned char)(t >> 48);
            *(md++) = (unsigned char)(t >> 40);
            *(md++) = (unsigned char)(t >> 32);
        }
        break;
    case SHA256_DIGEST_LENGTH:
        for (n = 0; n < SHA256_DIGEST_LENGTH / 8; n++) {
            SHA_LONG64 t = c->h[n];

            *(md++) = (unsigned char)(t >> 56);
            *(md++) = (unsigned char)(t >> 48);
            *(md++) = (unsigned char)(t >> 40);
            *(md++) = (unsigned char)(t >> 32);
            *(md++) = (unsigned char)(t >> 24);
            *(md++) = (unsigned char)(t >> 16);
            *(md++) = (unsigned char)(t >> 8);
            *(md++) = (unsigned char)(t);
        }
        break;
    case SHA384_DIGEST_LENGTH:
        for (n = 0; n < SHA384_DIGEST_LENGTH / 8; n++) {
            SHA_LONG64 t = c->h[n];

            *(md++) = (unsigned char)(t >> 56);
            *(md++) = (unsigned char)(t >> 48);
            *(md++) = (unsigned char)(t >> 40);
            *(md++) = (unsigned char)(t >> 32);
            *(md++) = (unsigned char)(t >> 24);
            *(md++) = (unsigned char)(t >> 16);
            *(md++) = (unsigned char)(t >> 8);
            *(md++) = (unsigned char)(t);
        }
        break;
    case SHA512_DIGEST_LENGTH:
        for (n = 0; n < SHA512_DIGEST_LENGTH / 8; n++) {
            SHA_LONG64 t = c->h[n];

            *(md++) = (unsigned char)(t >> 56);
            *(md++) = (unsigned char)(t >> 48);
            *(md++) = (unsigned char)(t >> 40);
            *(md++) = (unsigned char)(t >> 32);
            *(md++) = (unsigned char)(t >> 24);
            *(md++) = (unsigned char)(t >> 16);
            *(md++) = (unsigned char)(t >> 8);
            *(md++) = (unsigned char)(t);
        }
        break;
    /* ... as well as make sure md_len is not abused. */
    default:
        __assert_unreachable();
    }
}

static int
ossl_sha512_update(void *c_, const void *_data, unsigned int len)
{
    SHA512_CTX *c = c_;
    SHA_LONG64 l;
    unsigned char *p = c->u.p;
    const unsigned char *data = (const unsigned char *)_data;

    if (len == 0)
        return 0;

    l = (c->Nl + (((SHA_LONG64) len) << 3)) & U64(0xffffffffffffffff);
    if (l < c->Nl)
        c->Nh++;
    if (sizeof(len) >= 8)
        c->Nh += (((SHA_LONG64) len) >> 61);
    c->Nl = l;

    if (c->num != 0) {
        size_t n = sizeof(c->u) - c->num;

        if (len < n) {
            memcpy(p + c->num, data, len), c->num += (unsigned int)len;
            return 0;
        } else {
            memcpy(p + c->num, data, n), c->num = 0;
            len -= n, data += n;
            sha512_block_data_order(c, p, 1);
        }
    }

    if (len >= sizeof(c->u)) {
#ifndef SHA512_BLOCK_CAN_MANAGE_UNALIGNED_DATA
        if ((size_t)data % sizeof(c->u.d[0]) != 0)
            while (len >= sizeof(c->u))
                memcpy(p, data, sizeof(c->u)),
                sha512_block_data_order(c, p, 1),
                len -= sizeof(c->u), data += sizeof(c->u);
        else
#endif
            sha512_block_data_order(c, data, len / sizeof(c->u)),
            data += len, len %= sizeof(c->u), data -= len;
    }

    if (len != 0)
        memcpy(p, data, len), c->num = (int)len;

    return 0;
}

struct auth_hash ossl_hash_sha384 = {
	.type = CRYPTO_SHA2_384,
	.name = "OpenSSL-SHA2-384",
	.hashsize = SHA2_384_HASH_LEN,
	.ctxsize = sizeof(SHA512_CTX),
	.blocksize = SHA2_384_BLOCK_LEN,
	.Init = ossl_sha384_init,
	.Update = ossl_sha512_update,
	.Final = ossl_sha512_final,
};

struct auth_hash ossl_hash_sha512 = {
	.type = CRYPTO_SHA2_512,
	.name = "OpenSSL-SHA2-512",
	.hashsize = SHA2_512_HASH_LEN,
	.ctxsize = sizeof(SHA512_CTX),
	.blocksize = SHA2_512_BLOCK_LEN,
	.Init = ossl_sha512_init,
	.Update = ossl_sha512_update,
	.Final = ossl_sha512_final,
};

_Static_assert(sizeof(SHA512_CTX) <= sizeof(struct ossl_hash_context),
    "ossl_hash_context too small");
