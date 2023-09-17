/*
 * Copyright 2015-2018 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <sys/libkern.h>
#include <sys/malloc.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform_auth.h>

#include <crypto/openssl/ossl.h>
#include <crypto/openssl/ossl_poly1305.h>

#define	POLY1305_ASM

/* From crypto/poly1305/poly1305.c */

/* pick 32-bit unsigned integer in little endian order */
static unsigned int U8TOU32(const unsigned char *p)
{
    return (((unsigned int)(p[0] & 0xff)) |
            ((unsigned int)(p[1] & 0xff) << 8) |
            ((unsigned int)(p[2] & 0xff) << 16) |
            ((unsigned int)(p[3] & 0xff) << 24));
}

/*
 * Implementations can be classified by amount of significant bits in
 * words making up the multi-precision value, or in other words radix
 * or base of numerical representation, e.g. base 2^64, base 2^32,
 * base 2^26. Complementary characteristic is how wide is the result of
 * multiplication of pair of digits, e.g. it would take 128 bits to
 * accommodate multiplication result in base 2^64 case. These are used
 * interchangeably. To describe implementation that is. But interface
 * is designed to isolate this so that low-level primitives implemented
 * in assembly can be self-contained/self-coherent.
 */
int poly1305_init(void *ctx, const unsigned char key[16], void *func);
void poly1305_blocks(void *ctx, const unsigned char *inp, size_t len,
                     unsigned int padbit);
void poly1305_emit(void *ctx, unsigned char mac[16],
                   const unsigned int nonce[4]);

void Poly1305_Init(POLY1305 *ctx, const unsigned char key[32])
{
    ctx->nonce[0] = U8TOU32(&key[16]);
    ctx->nonce[1] = U8TOU32(&key[20]);
    ctx->nonce[2] = U8TOU32(&key[24]);
    ctx->nonce[3] = U8TOU32(&key[28]);

    /*
     * Unlike reference poly1305_init assembly counterpart is expected
     * to return a value: non-zero if it initializes ctx->func, and zero
     * otherwise. Latter is to simplify assembly in cases when there no
     * multiple code paths to switch between.
     */
    if (!poly1305_init(ctx->opaque, key, &ctx->func)) {
        ctx->func.blocks = poly1305_blocks;
        ctx->func.emit = poly1305_emit;
    }

    ctx->num = 0;

}

#ifdef POLY1305_ASM
/*
 * This "eclipses" poly1305_blocks and poly1305_emit, but it's
 * conscious choice imposed by -Wshadow compiler warnings.
 */
# define poly1305_blocks (*poly1305_blocks_p)
# define poly1305_emit   (*poly1305_emit_p)
#endif

void Poly1305_Update(POLY1305 *ctx, const unsigned char *inp, size_t len)
{
#ifdef POLY1305_ASM
    /*
     * As documented, poly1305_blocks is never called with input
     * longer than single block and padbit argument set to 0. This
     * property is fluently used in assembly modules to optimize
     * padbit handling on loop boundary.
     */
    poly1305_blocks_f poly1305_blocks_p = ctx->func.blocks;
#endif
    size_t rem, num;

    if ((num = ctx->num)) {
        rem = POLY1305_BLOCK_SIZE - num;
        if (len >= rem) {
            memcpy(ctx->data + num, inp, rem);
            poly1305_blocks(ctx->opaque, ctx->data, POLY1305_BLOCK_SIZE, 1);
            inp += rem;
            len -= rem;
        } else {
            /* Still not enough data to process a block. */
            memcpy(ctx->data + num, inp, len);
            ctx->num = num + len;
            return;
        }
    }

    rem = len % POLY1305_BLOCK_SIZE;
    len -= rem;

    if (len >= POLY1305_BLOCK_SIZE) {
        poly1305_blocks(ctx->opaque, inp, len, 1);
        inp += len;
    }

    if (rem)
        memcpy(ctx->data, inp, rem);

    ctx->num = rem;
}

void Poly1305_Final(POLY1305 *ctx, unsigned char mac[16])
{
#ifdef POLY1305_ASM
    poly1305_blocks_f poly1305_blocks_p = ctx->func.blocks;
    poly1305_emit_f poly1305_emit_p = ctx->func.emit;
#endif
    size_t num;

    if ((num = ctx->num)) {
        ctx->data[num++] = 1;   /* pad bit */
        while (num < POLY1305_BLOCK_SIZE)
            ctx->data[num++] = 0;
        poly1305_blocks(ctx->opaque, ctx->data, POLY1305_BLOCK_SIZE, 0);
    }

    poly1305_emit(ctx->opaque, mac, ctx->nonce);

    /* zero out the state */
    OPENSSL_cleanse(ctx, sizeof(*ctx));
}

static void
ossl_poly1305_init(void *vctx)
{
}

static void
ossl_poly1305_setkey(void *vctx, const uint8_t *key, u_int klen)
{
	MPASS(klen == 32);
	Poly1305_Init(vctx, key);
}

int
ossl_poly1305_update(void *vctx, const void *buf, u_int len)
{
	Poly1305_Update(vctx, buf, len);
	return (0);
}

static void
ossl_poly1305_final(uint8_t *digest, void *vctx)
{
	Poly1305_Final(vctx, digest);
}

struct auth_hash ossl_hash_poly1305 = {
	.type = CRYPTO_POLY1305,
	.name = "OpenSSL-Poly1305",
	.hashsize = POLY1305_HASH_LEN,
	.ctxsize = sizeof(struct poly1305_context),
	.blocksize = POLY1305_BLOCK_SIZE,
	.Init = ossl_poly1305_init,
	.Setkey = ossl_poly1305_setkey,
	.Update = ossl_poly1305_update,
	.Final = ossl_poly1305_final,
};

_Static_assert(sizeof(struct poly1305_context) <=
    sizeof(struct ossl_hash_context), "ossl_hash_context too small");
