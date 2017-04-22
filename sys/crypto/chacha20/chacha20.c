/*-
 * Copyright (c) 2017 Dag-Erling Smørgrav
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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

#ifdef _KERNEL
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/module.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#else
#include <stdint.h>
#include <string.h>
#endif

#include <sys/endian.h>


#include <crypto/chacha20/chacha20.h>

#define rol32(x, n) \
	((x) << ((n) & 0x1f) | (x) >> (-(n) & 0x1f))

#define CHACHA_QR(x, a, b, c, d)					\
	do {								\
		x[a] = x[a] + x[b]; x[d] = rol32(x[d] ^ x[a], 16);	\
		x[c] = x[c] + x[d]; x[b] = rol32(x[b] ^ x[c], 12);	\
		x[a] = x[a] + x[b]; x[d] = rol32(x[d] ^ x[a],  8);	\
		x[c] = x[c] + x[d]; x[b] = rol32(x[b] ^ x[c],  7);	\
	} while (0)

static const char magic128[] = "expand 16-byte k";
static const char magic256[] = "expand 32-byte k";

/*
 * Fill the state array with 16 bytes of magic and 32 bytes of key,
 * repeating the key if necessary.  The 8-byte stream position and the
 * 8-byte nonce are initialized to all-zeroes.
 */
void
chacha20_init(chacha20_ctx *ctx, const uint8_t *key, size_t keylen)
{

	memset(ctx, 0, sizeof *ctx);
	if (keylen == 32) {
		/* magic */
		ctx->state[ 0] = le32dec(magic256 + 0);
		ctx->state[ 1] = le32dec(magic256 + 4);
		ctx->state[ 2] = le32dec(magic256 + 8);
		ctx->state[ 3] = le32dec(magic256 + 12);
		/* first half of key */
		ctx->state[ 4] = le32dec(key + 0);
		ctx->state[ 5] = le32dec(key + 4);
		ctx->state[ 6] = le32dec(key + 8);
		ctx->state[ 7] = le32dec(key + 12);
		/* second half of key */
		ctx->state[ 8] = le32dec(key + 16);
		ctx->state[ 9] = le32dec(key + 20);
		ctx->state[10] = le32dec(key + 24);
		ctx->state[11] = le32dec(key + 28);
	} else {
		/* magic */
		ctx->state[ 0] = le32dec(magic128 + 0);
		ctx->state[ 1] = le32dec(magic128 + 4);
		ctx->state[ 2] = le32dec(magic128 + 8);
		ctx->state[ 3] = le32dec(magic128 + 12);
		/* first half of key */
		ctx->state[ 4] = le32dec(key + 0);
		ctx->state[ 5] = le32dec(key + 4);
		ctx->state[ 6] = le32dec(key + 8);
		ctx->state[ 7] = le32dec(key + 12);
		/* repeat first half of key */
		ctx->state[ 8] = le32dec(key + 0);
		ctx->state[ 9] = le32dec(key + 4);
		ctx->state[10] = le32dec(key + 8);
		ctx->state[11] = le32dec(key + 12);
	}
}

/*
 * Reset the stream position and load a new nonce.
 */
void
chacha20_reset(chacha20_ctx *ctx, const uint8_t *nonce)
{

	/* reset stream counter */
	ctx->state[12] = 0;
	ctx->state[13] = 0;
	/* copy nonce */
	ctx->state[14] = le32dec(nonce + 0);
	ctx->state[15] = le32dec(nonce + 4);
}

/*
 * Encryption: generate a block of keystream, xor it with the plaintext to
 * produce the ciphertext, and increment the stream position.  If vpt is
 * NULL, simply copy the requested keystream to the output buffer.
 */
size_t
chacha20_encrypt(chacha20_ctx *ctx, const void *vpt, uint8_t *ct, size_t len)
{
	const uint8_t *pt = vpt;
	uint32_t mix[16];
	uint8_t ks[64];
	unsigned int b, i;

	len -= len % sizeof ks;
	for (b = 0; b < len; b += sizeof ks) {
		memcpy(mix, ctx->state, sizeof mix);
		for (i = 0; i < 20; i += 2) {
			CHACHA_QR(mix,  0,  4,  8, 12);
			CHACHA_QR(mix,  1,  5,  9, 13);
			CHACHA_QR(mix,  2,  6, 10, 14);
			CHACHA_QR(mix,  3,  7, 11, 15);
			CHACHA_QR(mix,  0,  5, 10, 15);
			CHACHA_QR(mix,  1,  6, 11, 12);
			CHACHA_QR(mix,  2,  7,  8, 13);
			CHACHA_QR(mix,  3,  4,  9, 14);
		}
		for (i = 0; i < 16; ++i)
			le32enc(ks + i * 4, ctx->state[i] + mix[i]);
		if (pt == NULL) {
			memcpy(ct, ks, sizeof ks);
			ct += sizeof ks;
		} else {
			for (i = 0; i < 64 && i < len; ++i)
				*ct++ = *pt++ ^ ks[i];
		}
		if (++ctx->state[12] == 0)
			++ctx->state[13];
	}
	return (len);
}

/*
 * Decryption: identical to encryption.
 */
size_t
chacha20_decrypt(chacha20_ctx *ctx, const uint8_t *ct, void *vpt, size_t len)
{

	return (chacha20_encrypt(ctx, ct, vpt, len));
}

/*
 * Wipe our state.
 */
void
chacha20_finish(chacha20_ctx *ctx)
{

	memset(ctx, 0, sizeof *ctx);
}

#ifdef _KERNEL

static int
chacha20_modevent(module_t mod, int evtype, void *unused)
{

	switch (evtype) {
	case MOD_LOAD:
		return (0);
	case MOD_UNLOAD:
		return (0);
	default:
		return (EOPNOTSUPP);
	}
}

static moduledata_t chacha20_mod = {
	"chacha20",
	chacha20_modevent,
	0
};

DECLARE_MODULE(chacha20, chacha20_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(chacha20, 1);
#endif
