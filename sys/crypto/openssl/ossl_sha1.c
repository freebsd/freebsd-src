/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <sys/cdefs.h>
#include <sys/libkern.h>
#include <sys/malloc.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform_auth.h>

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_sha.h>

/* sha1-x86_64.S */
void sha1_block_data_order(SHA_CTX *c, const void *p, size_t len);

/* From crypto/sha/sha_local.h */
#define DATA_ORDER_IS_BIG_ENDIAN

#define HASH_LONG               SHA_LONG
#define HASH_CTX                SHA_CTX
#define HASH_CBLOCK             SHA_CBLOCK
#define HASH_MAKE_STRING(c,s)   do {    \
        unsigned long ll;               \
        ll=(c)->h0; (void)HOST_l2c(ll,(s));     \
        ll=(c)->h1; (void)HOST_l2c(ll,(s));     \
        ll=(c)->h2; (void)HOST_l2c(ll,(s));     \
        ll=(c)->h3; (void)HOST_l2c(ll,(s));     \
        ll=(c)->h4; (void)HOST_l2c(ll,(s));     \
        } while (0)

#define HASH_UPDATE                     ossl_sha1_update
#define HASH_FINAL                      ossl_sha1_final
#define HASH_INIT                       ossl_sha1_init
#define HASH_BLOCK_DATA_ORDER           sha1_block_data_order

#define INIT_DATA_h0 0x67452301UL
#define INIT_DATA_h1 0xefcdab89UL
#define INIT_DATA_h2 0x98badcfeUL
#define INIT_DATA_h3 0x10325476UL
#define INIT_DATA_h4 0xc3d2e1f0UL

static void
HASH_INIT(void *c_)
{
    SHA_CTX *c = c_;
    memset(c, 0, sizeof(*c));
    c->h0 = INIT_DATA_h0;
    c->h1 = INIT_DATA_h1;
    c->h2 = INIT_DATA_h2;
    c->h3 = INIT_DATA_h3;
    c->h4 = INIT_DATA_h4;
}

#include "ossl_hash.h"

struct auth_hash ossl_hash_sha1 = {
	.type = CRYPTO_SHA1,
	.name = "OpenSSL-SHA1",
	.hashsize = SHA1_HASH_LEN,
	.ctxsize = sizeof(SHA_CTX),
	.blocksize = SHA1_BLOCK_LEN,
	.Init = HASH_INIT,
	.Update = HASH_UPDATE,
	.Final = HASH_FINAL,
};

_Static_assert(sizeof(SHA_CTX) <= sizeof(struct ossl_hash_context),
    "ossl_hash_context too small");
