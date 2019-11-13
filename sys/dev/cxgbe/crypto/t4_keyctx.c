/*-
 * Copyright (c) 2017-2019 Chelsio Communications, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/malloc.h>

#include <opencrypto/cryptodev.h>
#include <opencrypto/xform.h>

#include "common/common.h"
#include "crypto/t4_crypto.h"

/*
 * Crypto operations use a key context to store cipher keys and
 * partial hash digests.  They can either be passed inline as part of
 * a work request using crypto or they can be stored in card RAM.  For
 * the latter case, work requests must replace the inline key context
 * with a request to read the context from card RAM.
 *
 * The format of a key context:
 *
 * +-------------------------------+
 * | key context header            |
 * +-------------------------------+
 * | AES key                       |  ----- For requests with AES
 * +-------------------------------+
 * | Hash state                    |  ----- For hash-only requests
 * +-------------------------------+ -
 * | IPAD (16-byte aligned)        |  \
 * +-------------------------------+  +---- For requests with HMAC
 * | OPAD (16-byte aligned)        |  /
 * +-------------------------------+ -
 * | GMAC H                        |  ----- For AES-GCM
 * +-------------------------------+ -
 */

/*
 * Generate the initial GMAC hash state for a AES-GCM key.
 *
 * Borrowed from AES_GMAC_Setkey().
 */
void
t4_init_gmac_hash(const char *key, int klen, char *ghash)
{
	static char zeroes[GMAC_BLOCK_LEN];
	uint32_t keysched[4 * (RIJNDAEL_MAXNR + 1)];
	int rounds;

	rounds = rijndaelKeySetupEnc(keysched, key, klen);
	rijndaelEncrypt(keysched, rounds, zeroes, ghash);
}

/* Copy out the partial hash state from a software hash implementation. */
void
t4_copy_partial_hash(int alg, union authctx *auth_ctx, void *dst)
{
	uint32_t *u32;
	uint64_t *u64;
	u_int i;

	u32 = (uint32_t *)dst;
	u64 = (uint64_t *)dst;
	switch (alg) {
	case CRYPTO_SHA1:
	case CRYPTO_SHA1_HMAC:
		for (i = 0; i < SHA1_HASH_LEN / 4; i++)
			u32[i] = htobe32(auth_ctx->sha1ctx.h.b32[i]);
		break;
	case CRYPTO_SHA2_224:
	case CRYPTO_SHA2_224_HMAC:
		for (i = 0; i < SHA2_256_HASH_LEN / 4; i++)
			u32[i] = htobe32(auth_ctx->sha224ctx.state[i]);
		break;
	case CRYPTO_SHA2_256:
	case CRYPTO_SHA2_256_HMAC:
		for (i = 0; i < SHA2_256_HASH_LEN / 4; i++)
			u32[i] = htobe32(auth_ctx->sha256ctx.state[i]);
		break;
	case CRYPTO_SHA2_384:
	case CRYPTO_SHA2_384_HMAC:
		for (i = 0; i < SHA2_512_HASH_LEN / 8; i++)
			u64[i] = htobe64(auth_ctx->sha384ctx.state[i]);
		break;
	case CRYPTO_SHA2_512:
	case CRYPTO_SHA2_512_HMAC:
		for (i = 0; i < SHA2_512_HASH_LEN / 8; i++)
			u64[i] = htobe64(auth_ctx->sha512ctx.state[i]);
		break;
	}
}

void
t4_init_hmac_digest(struct auth_hash *axf, u_int partial_digest_len,
    char *key, int klen, char *dst)
{
	union authctx auth_ctx;
	char ipad[SHA2_512_BLOCK_LEN], opad[SHA2_512_BLOCK_LEN];
	u_int i;

	/*
	 * If the key is larger than the block size, use the digest of
	 * the key as the key instead.
	 */
	klen /= 8;
	if (klen > axf->blocksize) {
		axf->Init(&auth_ctx);
		axf->Update(&auth_ctx, key, klen);
		axf->Final(ipad, &auth_ctx);
		klen = axf->hashsize;
	} else
		memcpy(ipad, key, klen);

	memset(ipad + klen, 0, axf->blocksize - klen);
	memcpy(opad, ipad, axf->blocksize);

	for (i = 0; i < axf->blocksize; i++) {
		ipad[i] ^= HMAC_IPAD_VAL;
		opad[i] ^= HMAC_OPAD_VAL;
	}

	/*
	 * Hash the raw ipad and opad and store the partial results in
	 * the key context.
	 */
	axf->Init(&auth_ctx);
	axf->Update(&auth_ctx, ipad, axf->blocksize);
	t4_copy_partial_hash(axf->type, &auth_ctx, dst);

	dst += roundup2(partial_digest_len, 16);
	axf->Init(&auth_ctx);
	axf->Update(&auth_ctx, opad, axf->blocksize);
	t4_copy_partial_hash(axf->type, &auth_ctx, dst);
}

/*
 * Borrowed from cesa_prep_aes_key().
 *
 * NB: The crypto engine wants the words in the decryption key in reverse
 * order.
 */
void
t4_aes_getdeckey(void *dec_key, const void *enc_key, unsigned int kbits)
{
	uint32_t ek[4 * (RIJNDAEL_MAXNR + 1)];
	uint32_t *dkey;
	int i;

	rijndaelKeySetupEnc(ek, enc_key, kbits);
	dkey = dec_key;
	dkey += (kbits / 8) / 4;

	switch (kbits) {
	case 128:
		for (i = 0; i < 4; i++)
			*--dkey = htobe32(ek[4 * 10 + i]);
		break;
	case 192:
		for (i = 0; i < 2; i++)
			*--dkey = htobe32(ek[4 * 11 + 2 + i]);
		for (i = 0; i < 4; i++)
			*--dkey = htobe32(ek[4 * 12 + i]);
		break;
	case 256:
		for (i = 0; i < 4; i++)
			*--dkey = htobe32(ek[4 * 13 + i]);
		for (i = 0; i < 4; i++)
			*--dkey = htobe32(ek[4 * 14 + i]);
		break;
	}
	MPASS(dkey == dec_key);
}
