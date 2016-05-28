/*-
 * Copyright 2005 Colin Percival
 * Copyright (c) 2015 Allan Jude <allanjude@FreeBSD.org>
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

#include <sys/endian.h>
#include <sys/types.h>

#ifdef _KERNEL
#include <sys/systm.h>
#else
#include <string.h>
#endif

#include "sha512.h"
#include "sha512t.h"
#include "sha384.h"

#if BYTE_ORDER == BIG_ENDIAN

/* Copy a vector of big-endian uint64_t into a vector of bytes */
#define be64enc_vect(dst, src, len)	\
	memcpy((void *)dst, (const void *)src, (size_t)len)

/* Copy a vector of bytes into a vector of big-endian uint64_t */
#define be64dec_vect(dst, src, len)	\
	memcpy((void *)dst, (const void *)src, (size_t)len)

#else /* BYTE_ORDER != BIG_ENDIAN */

/*
 * Encode a length len/4 vector of (uint64_t) into a length len vector of
 * (unsigned char) in big-endian form.  Assumes len is a multiple of 8.
 */
static void
be64enc_vect(unsigned char *dst, const uint64_t *src, size_t len)
{
	size_t i;

	for (i = 0; i < len / 8; i++)
		be64enc(dst + i * 8, src[i]);
}

/*
 * Decode a big-endian length len vector of (unsigned char) into a length
 * len/4 vector of (uint64_t).  Assumes len is a multiple of 8.
 */
static void
be64dec_vect(uint64_t *dst, const unsigned char *src, size_t len)
{
	size_t i;

	for (i = 0; i < len / 8; i++)
		dst[i] = be64dec(src + i * 8);
}

#endif /* BYTE_ORDER != BIG_ENDIAN */

/* Elementary functions used by SHA512 */
#define Ch(x, y, z)	((x & (y ^ z)) ^ z)
#define Maj(x, y, z)	((x & (y | z)) | (y & z))
#define SHR(x, n)	(x >> n)
#define ROTR(x, n)	((x >> n) | (x << (64 - n)))
#define S0(x)		(ROTR(x, 28) ^ ROTR(x, 34) ^ ROTR(x, 39))
#define S1(x)		(ROTR(x, 14) ^ ROTR(x, 18) ^ ROTR(x, 41))
#define s0(x)		(ROTR(x, 1) ^ ROTR(x, 8) ^ SHR(x, 7))
#define s1(x)		(ROTR(x, 19) ^ ROTR(x, 61) ^ SHR(x, 6))

/* SHA512 round function */
#define RND(a, b, c, d, e, f, g, h, k)			\
	t0 = h + S1(e) + Ch(e, f, g) + k;		\
	t1 = S0(a) + Maj(a, b, c);			\
	d += t0;					\
	h  = t0 + t1;

/* Adjusted round function for rotating state */
#define RNDr(S, W, i, k)			\
	RND(S[(80 - i) % 8], S[(81 - i) % 8],	\
	    S[(82 - i) % 8], S[(83 - i) % 8],	\
	    S[(84 - i) % 8], S[(85 - i) % 8],	\
	    S[(86 - i) % 8], S[(87 - i) % 8],	\
	    W[i] + k)

/*
 * SHA512 block compression function.  The 512-bit state is transformed via
 * the 512-bit input block to produce a new state.
 */
static void
SHA512_Transform(uint64_t * state, const unsigned char block[SHA512_BLOCK_LENGTH])
{
	uint64_t W[80];
	uint64_t S[8];
	uint64_t t0, t1;
	int i;

	/* 1. Prepare message schedule W. */
	be64dec_vect(W, block, SHA512_BLOCK_LENGTH);
	for (i = 16; i < 80; i++)
		W[i] = s1(W[i - 2]) + W[i - 7] + s0(W[i - 15]) + W[i - 16];

	/* 2. Initialize working variables. */
	memcpy(S, state, SHA512_DIGEST_LENGTH);

	/* 3. Mix. */
	RNDr(S, W, 0, 0x428a2f98d728ae22ULL);
	RNDr(S, W, 1, 0x7137449123ef65cdULL);
	RNDr(S, W, 2, 0xb5c0fbcfec4d3b2fULL);
	RNDr(S, W, 3, 0xe9b5dba58189dbbcULL);
	RNDr(S, W, 4, 0x3956c25bf348b538ULL);
	RNDr(S, W, 5, 0x59f111f1b605d019ULL);
	RNDr(S, W, 6, 0x923f82a4af194f9bULL);
	RNDr(S, W, 7, 0xab1c5ed5da6d8118ULL);
	RNDr(S, W, 8, 0xd807aa98a3030242ULL);
	RNDr(S, W, 9, 0x12835b0145706fbeULL);
	RNDr(S, W, 10, 0x243185be4ee4b28cULL);
	RNDr(S, W, 11, 0x550c7dc3d5ffb4e2ULL);
	RNDr(S, W, 12, 0x72be5d74f27b896fULL);
	RNDr(S, W, 13, 0x80deb1fe3b1696b1ULL);
	RNDr(S, W, 14, 0x9bdc06a725c71235ULL);
	RNDr(S, W, 15, 0xc19bf174cf692694ULL);
	RNDr(S, W, 16, 0xe49b69c19ef14ad2ULL);
	RNDr(S, W, 17, 0xefbe4786384f25e3ULL);
	RNDr(S, W, 18, 0x0fc19dc68b8cd5b5ULL);
	RNDr(S, W, 19, 0x240ca1cc77ac9c65ULL);
	RNDr(S, W, 20, 0x2de92c6f592b0275ULL);
	RNDr(S, W, 21, 0x4a7484aa6ea6e483ULL);
	RNDr(S, W, 22, 0x5cb0a9dcbd41fbd4ULL);
	RNDr(S, W, 23, 0x76f988da831153b5ULL);
	RNDr(S, W, 24, 0x983e5152ee66dfabULL);
	RNDr(S, W, 25, 0xa831c66d2db43210ULL);
	RNDr(S, W, 26, 0xb00327c898fb213fULL);
	RNDr(S, W, 27, 0xbf597fc7beef0ee4ULL);
	RNDr(S, W, 28, 0xc6e00bf33da88fc2ULL);
	RNDr(S, W, 29, 0xd5a79147930aa725ULL);
	RNDr(S, W, 30, 0x06ca6351e003826fULL);
	RNDr(S, W, 31, 0x142929670a0e6e70ULL);
	RNDr(S, W, 32, 0x27b70a8546d22ffcULL);
	RNDr(S, W, 33, 0x2e1b21385c26c926ULL);
	RNDr(S, W, 34, 0x4d2c6dfc5ac42aedULL);
	RNDr(S, W, 35, 0x53380d139d95b3dfULL);
	RNDr(S, W, 36, 0x650a73548baf63deULL);
	RNDr(S, W, 37, 0x766a0abb3c77b2a8ULL);
	RNDr(S, W, 38, 0x81c2c92e47edaee6ULL);
	RNDr(S, W, 39, 0x92722c851482353bULL);
	RNDr(S, W, 40, 0xa2bfe8a14cf10364ULL);
	RNDr(S, W, 41, 0xa81a664bbc423001ULL);
	RNDr(S, W, 42, 0xc24b8b70d0f89791ULL);
	RNDr(S, W, 43, 0xc76c51a30654be30ULL);
	RNDr(S, W, 44, 0xd192e819d6ef5218ULL);
	RNDr(S, W, 45, 0xd69906245565a910ULL);
	RNDr(S, W, 46, 0xf40e35855771202aULL);
	RNDr(S, W, 47, 0x106aa07032bbd1b8ULL);
	RNDr(S, W, 48, 0x19a4c116b8d2d0c8ULL);
	RNDr(S, W, 49, 0x1e376c085141ab53ULL);
	RNDr(S, W, 50, 0x2748774cdf8eeb99ULL);
	RNDr(S, W, 51, 0x34b0bcb5e19b48a8ULL);
	RNDr(S, W, 52, 0x391c0cb3c5c95a63ULL);
	RNDr(S, W, 53, 0x4ed8aa4ae3418acbULL);
	RNDr(S, W, 54, 0x5b9cca4f7763e373ULL);
	RNDr(S, W, 55, 0x682e6ff3d6b2b8a3ULL);
	RNDr(S, W, 56, 0x748f82ee5defb2fcULL);
	RNDr(S, W, 57, 0x78a5636f43172f60ULL);
	RNDr(S, W, 58, 0x84c87814a1f0ab72ULL);
	RNDr(S, W, 59, 0x8cc702081a6439ecULL);
	RNDr(S, W, 60, 0x90befffa23631e28ULL);
	RNDr(S, W, 61, 0xa4506cebde82bde9ULL);
	RNDr(S, W, 62, 0xbef9a3f7b2c67915ULL);
	RNDr(S, W, 63, 0xc67178f2e372532bULL);
	RNDr(S, W, 64, 0xca273eceea26619cULL);
	RNDr(S, W, 65, 0xd186b8c721c0c207ULL);
	RNDr(S, W, 66, 0xeada7dd6cde0eb1eULL);
	RNDr(S, W, 67, 0xf57d4f7fee6ed178ULL);
	RNDr(S, W, 68, 0x06f067aa72176fbaULL);
	RNDr(S, W, 69, 0x0a637dc5a2c898a6ULL);
	RNDr(S, W, 70, 0x113f9804bef90daeULL);
	RNDr(S, W, 71, 0x1b710b35131c471bULL);
	RNDr(S, W, 72, 0x28db77f523047d84ULL);
	RNDr(S, W, 73, 0x32caab7b40c72493ULL);
	RNDr(S, W, 74, 0x3c9ebe0a15c9bebcULL);
	RNDr(S, W, 75, 0x431d67c49c100d4cULL);
	RNDr(S, W, 76, 0x4cc5d4becb3e42b6ULL);
	RNDr(S, W, 77, 0x597f299cfc657e2aULL);
	RNDr(S, W, 78, 0x5fcb6fab3ad6faecULL);
	RNDr(S, W, 79, 0x6c44198c4a475817ULL);

	/* 4. Mix local working variables into global state */
	for (i = 0; i < 8; i++)
		state[i] += S[i];
}

static unsigned char PAD[SHA512_BLOCK_LENGTH] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Add padding and terminating bit-count. */
static void
SHA512_Pad(SHA512_CTX * ctx)
{
	unsigned char len[16];
	uint64_t r, plen;

	/*
	 * Convert length to a vector of bytes -- we do this now rather
	 * than later because the length will change after we pad.
	 */
	be64enc_vect(len, ctx->count, 16);

	/* Add 1--128 bytes so that the resulting length is 112 mod 128 */
	r = (ctx->count[1] >> 3) & 0x7f;
	plen = (r < 112) ? (112 - r) : (240 - r);
	SHA512_Update(ctx, PAD, (size_t)plen);

	/* Add the terminating bit-count */
	SHA512_Update(ctx, len, 16);
}

/* SHA-512 initialization.  Begins a SHA-512 operation. */
void
SHA512_Init(SHA512_CTX * ctx)
{

	/* Zero bits processed so far */
	ctx->count[0] = ctx->count[1] = 0;

	/* Magic initialization constants */
	ctx->state[0] = 0x6a09e667f3bcc908ULL;
	ctx->state[1] = 0xbb67ae8584caa73bULL;
	ctx->state[2] = 0x3c6ef372fe94f82bULL;
	ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
	ctx->state[4] = 0x510e527fade682d1ULL;
	ctx->state[5] = 0x9b05688c2b3e6c1fULL;
	ctx->state[6] = 0x1f83d9abfb41bd6bULL;
	ctx->state[7] = 0x5be0cd19137e2179ULL;
}

/* Add bytes into the hash */
void
SHA512_Update(SHA512_CTX * ctx, const void *in, size_t len)
{
	uint64_t bitlen[2];
	uint64_t r;
	const unsigned char *src = in;

	/* Number of bytes left in the buffer from previous updates */
	r = (ctx->count[1] >> 3) & 0x7f;

	/* Convert the length into a number of bits */
	bitlen[1] = ((uint64_t)len) << 3;
	bitlen[0] = ((uint64_t)len) >> 61;

	/* Update number of bits */
	if ((ctx->count[1] += bitlen[1]) < bitlen[1])
		ctx->count[0]++;
	ctx->count[0] += bitlen[0];

	/* Handle the case where we don't need to perform any transforms */
	if (len < SHA512_BLOCK_LENGTH - r) {
		memcpy(&ctx->buf[r], src, len);
		return;
	}

	/* Finish the current block */
	memcpy(&ctx->buf[r], src, SHA512_BLOCK_LENGTH - r);
	SHA512_Transform(ctx->state, ctx->buf);
	src += SHA512_BLOCK_LENGTH - r;
	len -= SHA512_BLOCK_LENGTH - r;

	/* Perform complete blocks */
	while (len >= SHA512_BLOCK_LENGTH) {
		SHA512_Transform(ctx->state, src);
		src += SHA512_BLOCK_LENGTH;
		len -= SHA512_BLOCK_LENGTH;
	}

	/* Copy left over data into buffer */
	memcpy(ctx->buf, src, len);
}

/*
 * SHA-512 finalization.  Pads the input data, exports the hash value,
 * and clears the context state.
 */
void
SHA512_Final(unsigned char digest[static SHA512_DIGEST_LENGTH], SHA512_CTX *ctx)
{

	/* Add padding */
	SHA512_Pad(ctx);

	/* Write the hash */
	be64enc_vect(digest, ctx->state, SHA512_DIGEST_LENGTH);

	/* Clear the context state */
	memset(ctx, 0, sizeof(*ctx));
}

/*** SHA-512t: *********************************************************/
/*
 * the SHA512t transforms are identical to SHA512 so reuse the existing function
 */
void
SHA512_224_Init(SHA512_CTX * ctx)
{

	/* Zero bits processed so far */
	ctx->count[0] = ctx->count[1] = 0;

	/* Magic initialization constants */
	ctx->state[0] = 0x8c3d37c819544da2ULL;
	ctx->state[1] = 0x73e1996689dcd4d6ULL;
	ctx->state[2] = 0x1dfab7ae32ff9c82ULL;
	ctx->state[3] = 0x679dd514582f9fcfULL;
	ctx->state[4] = 0x0f6d2b697bd44da8ULL;
	ctx->state[5] = 0x77e36f7304c48942ULL;
	ctx->state[6] = 0x3f9d85a86a1d36c8ULL;
	ctx->state[7] = 0x1112e6ad91d692a1ULL;
}

void
SHA512_224_Update(SHA512_CTX * ctx, const void *in, size_t len)
{

	SHA512_Update(ctx, in, len);
}

void
SHA512_224_Final(unsigned char digest[static SHA512_224_DIGEST_LENGTH], SHA512_CTX * ctx)
{

	/* Add padding */
	SHA512_Pad(ctx);

	/* Write the hash */
	be64enc_vect(digest, ctx->state, SHA512_224_DIGEST_LENGTH);

	/* Clear the context state */
	memset(ctx, 0, sizeof(*ctx));
}

void
SHA512_256_Init(SHA512_CTX * ctx)
{

	/* Zero bits processed so far */
	ctx->count[0] = ctx->count[1] = 0;

	/* Magic initialization constants */
	ctx->state[0] = 0x22312194fc2bf72cULL;
	ctx->state[1] = 0x9f555fa3c84c64c2ULL;
	ctx->state[2] = 0x2393b86b6f53b151ULL;
	ctx->state[3] = 0x963877195940eabdULL;
	ctx->state[4] = 0x96283ee2a88effe3ULL;
	ctx->state[5] = 0xbe5e1e2553863992ULL;
	ctx->state[6] = 0x2b0199fc2c85b8aaULL;
	ctx->state[7] = 0x0eb72ddc81c52ca2ULL;
}

void
SHA512_256_Update(SHA512_CTX * ctx, const void *in, size_t len)
{

	SHA512_Update(ctx, in, len);
}

void
SHA512_256_Final(unsigned char digest[static SHA512_256_DIGEST_LENGTH], SHA512_CTX * ctx)
{

	/* Add padding */
	SHA512_Pad(ctx);

	/* Write the hash */
	be64enc_vect(digest, ctx->state, SHA512_256_DIGEST_LENGTH);

	/* Clear the context state */
	memset(ctx, 0, sizeof(*ctx));
}

/*** SHA-384: *********************************************************/
/*
 * the SHA384 and SHA512 transforms are identical, so SHA384 is skipped
 */

/* SHA-384 initialization.  Begins a SHA-384 operation. */
void
SHA384_Init(SHA384_CTX * ctx)
{

	/* Zero bits processed so far */
	ctx->count[0] = ctx->count[1] = 0;

	/* Magic initialization constants */
	ctx->state[0] = 0xcbbb9d5dc1059ed8ULL;
	ctx->state[1] = 0x629a292a367cd507ULL;
	ctx->state[2] = 0x9159015a3070dd17ULL;
	ctx->state[3] = 0x152fecd8f70e5939ULL;
	ctx->state[4] = 0x67332667ffc00b31ULL;
	ctx->state[5] = 0x8eb44a8768581511ULL;
	ctx->state[6] = 0xdb0c2e0d64f98fa7ULL;
	ctx->state[7] = 0x47b5481dbefa4fa4ULL;
}

/* Add bytes into the SHA-384 hash */
void
SHA384_Update(SHA384_CTX * ctx, const void *in, size_t len)
{

	SHA512_Update((SHA512_CTX *)ctx, in, len);
}

/*
 * SHA-384 finalization.  Pads the input data, exports the hash value,
 * and clears the context state.
 */
void
SHA384_Final(unsigned char digest[static SHA384_DIGEST_LENGTH], SHA384_CTX *ctx)
{

	/* Add padding */
	SHA512_Pad((SHA512_CTX *)ctx);

	/* Write the hash */
	be64enc_vect(digest, ctx->state, SHA384_DIGEST_LENGTH);

	/* Clear the context state */
	memset(ctx, 0, sizeof(*ctx));
}

#ifdef WEAK_REFS
/* When building libmd, provide weak references. Note: this is not
   activated in the context of compiling these sources for internal
   use in libcrypt.
 */
#undef SHA512_Init
__weak_reference(_libmd_SHA512_Init, SHA512_Init);
#undef SHA512_Update
__weak_reference(_libmd_SHA512_Update, SHA512_Update);
#undef SHA512_Final
__weak_reference(_libmd_SHA512_Final, SHA512_Final);
#undef SHA512_Transform
__weak_reference(_libmd_SHA512_Transform, SHA512_Transform);

#undef SHA512_224_Init
__weak_reference(_libmd_SHA512_224_Init, SHA512_224_Init);
#undef SHA512_224_Update
__weak_reference(_libmd_SHA512_224_Update, SHA512_224_Update);
#undef SHA512_224_Final
__weak_reference(_libmd_SHA512_224_Final, SHA512_224_Final);

#undef SHA512_256_Init
__weak_reference(_libmd_SHA512_256_Init, SHA512_256_Init);
#undef SHA512_256_Update
__weak_reference(_libmd_SHA512_256_Update, SHA512_256_Update);
#undef SHA512_256_Final
__weak_reference(_libmd_SHA512_256_Final, SHA512_256_Final);

#undef SHA384_Init
__weak_reference(_libmd_SHA384_Init, SHA384_Init);
#undef SHA384_Update
__weak_reference(_libmd_SHA384_Update, SHA384_Update);
#undef SHA384_Final
__weak_reference(_libmd_SHA384_Final, SHA384_Final);
#endif
