/*
 * Copyright 2004-2016 The OpenSSL Project Authors. All Rights Reserved.
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

/* sha256-x86_64.S */
void sha256_block_data_order(SHA256_CTX *c, const void *in, size_t num);

/* From crypto/sha/sha256.c */

static void
ossl_sha224_init(void *c_)
{
    SHA256_CTX *c = c_;
    memset(c, 0, sizeof(*c));
    c->h[0] = 0xc1059ed8UL;
    c->h[1] = 0x367cd507UL;
    c->h[2] = 0x3070dd17UL;
    c->h[3] = 0xf70e5939UL;
    c->h[4] = 0xffc00b31UL;
    c->h[5] = 0x68581511UL;
    c->h[6] = 0x64f98fa7UL;
    c->h[7] = 0xbefa4fa4UL;
    c->md_len = SHA224_DIGEST_LENGTH;
}

static void
ossl_sha256_init(void *c_)
{
    SHA256_CTX *c = c_;
    memset(c, 0, sizeof(*c));
    c->h[0] = 0x6a09e667UL;
    c->h[1] = 0xbb67ae85UL;
    c->h[2] = 0x3c6ef372UL;
    c->h[3] = 0xa54ff53aUL;
    c->h[4] = 0x510e527fUL;
    c->h[5] = 0x9b05688cUL;
    c->h[6] = 0x1f83d9abUL;
    c->h[7] = 0x5be0cd19UL;
    c->md_len = SHA256_DIGEST_LENGTH;
}


#define DATA_ORDER_IS_BIG_ENDIAN

#define HASH_LONG               SHA_LONG
#define HASH_CTX                SHA256_CTX
#define HASH_CBLOCK             SHA_CBLOCK

/*
 * Note that FIPS180-2 discusses "Truncation of the Hash Function Output."
 * default: case below covers for it. It's not clear however if it's
 * permitted to truncate to amount of bytes not divisible by 4. I bet not,
 * but if it is, then default: case shall be extended. For reference.
 * Idea behind separate cases for pre-defined lengths is to let the
 * compiler decide if it's appropriate to unroll small loops.
 */
#define HASH_MAKE_STRING(c,s)   do {    \
        unsigned long ll;               \
        unsigned int  nn;               \
        switch ((c)->md_len)            \
        {   case SHA224_DIGEST_LENGTH:  \
                for (nn=0;nn<SHA224_DIGEST_LENGTH/4;nn++)       \
                {   ll=(c)->h[nn]; (void)HOST_l2c(ll,(s));   }  \
                break;                  \
            case SHA256_DIGEST_LENGTH:  \
                for (nn=0;nn<SHA256_DIGEST_LENGTH/4;nn++)       \
                {   ll=(c)->h[nn]; (void)HOST_l2c(ll,(s));   }  \
                break;                  \
            default:                    \
                __assert_unreachable(); \
                break;                  \
        }                               \
        } while (0)

#define HASH_UPDATE             ossl_sha256_update
#define HASH_FINAL              ossl_sha256_final
#define HASH_BLOCK_DATA_ORDER   sha256_block_data_order

#include "ossl_hash.h"

struct auth_hash ossl_hash_sha224 = {
	.type = CRYPTO_SHA2_224,
	.name = "OpenSSL-SHA2-224",
	.hashsize = SHA2_224_HASH_LEN,
	.ctxsize = sizeof(SHA256_CTX),
	.blocksize = SHA2_224_BLOCK_LEN,
	.Init = ossl_sha224_init,
	.Update = HASH_UPDATE,
	.Final = HASH_FINAL,
};

struct auth_hash ossl_hash_sha256 = {
	.type = CRYPTO_SHA2_256,
	.name = "OpenSSL-SHA2-256",
	.hashsize = SHA2_256_HASH_LEN,
	.ctxsize = sizeof(SHA256_CTX),
	.blocksize = SHA2_256_BLOCK_LEN,
	.Init = ossl_sha256_init,
	.Update = HASH_UPDATE,
	.Final = HASH_FINAL,
};

_Static_assert(sizeof(SHA256_CTX) <= sizeof(struct ossl_hash_context),
    "ossl_hash_context too small");
