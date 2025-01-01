/*
 *  Copyright (C) 2022 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/lib_ecc_config.h>
#ifdef WITH_HASH_BELT_HASH

#include <libecc/hash/belt-hash.h>

/*
 * This is an implementation of the BELT-HASH hash function as
 * defined int STB 34.101.31.
 */


/*
 * The BELT-HASH function uses an underlying BELT block cipher
 * defined in STB 34.101.31. This is a simple and straitforward
 * implementation.
 */
#define ROTL_BELT(x, n)      ((((u32)(x)) << (n)) | (((u32)(x)) >> (32-(n))))

#define SWAP_BELT(x, y) do {		\
	u32 z;				\
	z = (x);			\
	(x) = (y);			\
	(y) = z;			\
} while(0)

/* The S-Box */
static u8 S[256] =
{
	0xB1, 0x94, 0xBA, 0xC8, 0x0A, 0x08, 0xF5, 0x3B, 0x36, 0x6D, 0x00, 0x8E, 0x58, 0x4A, 0x5D, 0xE4,
	0x85, 0x04, 0xFA, 0x9D, 0x1B, 0xB6, 0xC7, 0xAC, 0x25, 0x2E, 0x72, 0xC2, 0x02, 0xFD, 0xCE, 0x0D,
	0x5B, 0xE3, 0xD6, 0x12, 0x17, 0xB9, 0x61, 0x81, 0xFE, 0x67, 0x86, 0xAD, 0x71, 0x6B, 0x89, 0x0B,
	0x5C, 0xB0, 0xC0, 0xFF, 0x33, 0xC3, 0x56, 0xB8, 0x35, 0xC4, 0x05, 0xAE, 0xD8, 0xE0, 0x7F, 0x99,
	0xE1, 0x2B, 0xDC, 0x1A, 0xE2, 0x82, 0x57, 0xEC, 0x70, 0x3F, 0xCC, 0xF0, 0x95, 0xEE, 0x8D, 0xF1,
	0xC1, 0xAB, 0x76, 0x38, 0x9F, 0xE6, 0x78, 0xCA, 0xF7, 0xC6, 0xF8, 0x60, 0xD5, 0xBB, 0x9C, 0x4F,
	0xF3, 0x3C, 0x65, 0x7B, 0x63, 0x7C, 0x30, 0x6A, 0xDD, 0x4E, 0xA7, 0x79, 0x9E, 0xB2, 0x3D, 0x31,
	0x3E, 0x98, 0xB5, 0x6E, 0x27, 0xD3, 0xBC, 0xCF, 0x59, 0x1E, 0x18, 0x1F, 0x4C, 0x5A, 0xB7, 0x93,
	0xE9, 0xDE, 0xE7, 0x2C, 0x8F, 0x0C, 0x0F, 0xA6, 0x2D, 0xDB, 0x49, 0xF4, 0x6F, 0x73, 0x96, 0x47,
	0x06, 0x07, 0x53, 0x16, 0xED, 0x24, 0x7A, 0x37, 0x39, 0xCB, 0xA3, 0x83, 0x03, 0xA9, 0x8B, 0xF6,
	0x92, 0xBD, 0x9B, 0x1C, 0xE5, 0xD1, 0x41, 0x01, 0x54, 0x45, 0xFB, 0xC9, 0x5E, 0x4D, 0x0E, 0xF2,
	0x68, 0x20, 0x80, 0xAA, 0x22, 0x7D, 0x64, 0x2F, 0x26, 0x87, 0xF9, 0x34, 0x90, 0x40, 0x55, 0x11,
	0xBE, 0x32, 0x97, 0x13, 0x43, 0xFC, 0x9A, 0x48, 0xA0, 0x2A, 0x88, 0x5F, 0x19, 0x4B, 0x09, 0xA1,
	0x7E, 0xCD, 0xA4, 0xD0, 0x15, 0x44, 0xAF, 0x8C, 0xA5, 0x84, 0x50, 0xBF, 0x66, 0xD2, 0xE8, 0x8A,
	0xA2, 0xD7, 0x46, 0x52, 0x42, 0xA8, 0xDF, 0xB3, 0x69, 0x74, 0xC5, 0x51, 0xEB, 0x23, 0x29, 0x21,
	0xD4, 0xEF, 0xD9, 0xB4, 0x3A, 0x62, 0x28, 0x75, 0x91, 0x14, 0x10, 0xEA, 0x77, 0x6C, 0xDA, 0x1D,
};

/* */
#define GET_BYTE(x, a) ( ((x) >> (a)) & 0xff )
#define PUT_BYTE(x, a) ( (u32)(x) << (a) )
#define SB(x, a) PUT_BYTE( S[GET_BYTE((x), (a))], (a) )

#define G(x, r) ROTL_BELT( SB((x), 24) | SB((x), 16) | SB((x), 8) | SB((x), 0), (r) )

static u32 KIdx[8][7] =
{
	{ 0, 1, 2, 3, 4, 5, 6 },
	{ 7, 0, 1, 2, 3, 4, 5 },
	{ 6, 7, 0, 1, 2, 3, 4 },
	{ 5, 6, 7, 0, 1, 2, 3 },
	{ 4, 5, 6, 7, 0, 1, 2 },
	{ 3, 4, 5, 6, 7, 0, 1 },
	{ 2, 3, 4, 5, 6, 7, 0 },
	{ 1, 2, 3, 4, 5, 6, 7 },
};

int belt_init(const u8 *k, u32 k_len, u8 ks[BELT_KEY_SCHED_LEN])
{
	int ret = -1;
	unsigned int i;

	switch(k_len){
		case 16:{
			for(i = 0; i < 16; i++){
				ks[i]      = k[i];
				ks[i + 16] = k[i];
			}
			break;
		}
		case 24:{
			for(i = 0; i < 24; i++){
				ks[i] = k[i];
			}
			for(i = 24; i < 32; i++){
				ks[i] = k[i - 24] ^ k[i - 20] ^ k[i - 16];
			}
			break;
		}
		case 32:{
			for(i = 0; i < 32; i++){
				ks[i] = k[i];
			}
			break;
		}
		default:{
			ret = -1;
			goto err;
		}


	}

	ret = 0;
err:
	return ret;
}

void belt_encrypt(const u8 in[BELT_BLOCK_LEN], u8 out[BELT_BLOCK_LEN], const u8 ks[BELT_KEY_SCHED_LEN])
{
	u32 a, b, c, d, e;
	u32 i;

	GET_UINT32_LE(a, in, 0);
	GET_UINT32_LE(b, in, 4);
	GET_UINT32_LE(c, in, 8);
	GET_UINT32_LE(d, in, 12);

	for(i = 0; i < 8; i++){
		u32 key;
		GET_UINT32_LE(key, ks, 4*KIdx[i][0]);
		b ^= G(a + key, 5);
		GET_UINT32_LE(key, ks, 4*KIdx[i][1]);
		c ^= G(d + key, 21);
		GET_UINT32_LE(key, ks, 4*KIdx[i][2]);
		a = (u32)(a - G(b + key, 13));
		GET_UINT32_LE(key, ks, 4*KIdx[i][3]);
		e = G(b + c + key, 21) ^ (i + 1);
		b += e;
		c = (u32)(c - e);
		GET_UINT32_LE(key, ks, 4*KIdx[i][4]);
		d += G(c + key, 13);
		GET_UINT32_LE(key, ks, 4*KIdx[i][5]);
		b ^= G(a + key, 21);
		GET_UINT32_LE(key, ks, 4*KIdx[i][6]);
		c ^= G(d + key, 5);
		SWAP_BELT(a, b);
		SWAP_BELT(c, d);
		SWAP_BELT(b, c);
	}

	PUT_UINT32_LE(b, out, 0);
	PUT_UINT32_LE(d, out, 4);
	PUT_UINT32_LE(a, out, 8);
	PUT_UINT32_LE(c, out, 12);

	return;
}

void belt_decrypt(const u8 in[BELT_BLOCK_LEN], u8 out[BELT_BLOCK_LEN], const u8 ks[BELT_KEY_SCHED_LEN])
{
	u32 a, b, c, d, e;
	u32 i;

	GET_UINT32_LE(a, in, 0);
	GET_UINT32_LE(b, in, 4);
	GET_UINT32_LE(c, in, 8);
	GET_UINT32_LE(d, in, 12);

	for(i = 0; i < 8; i++){
		u32 key;
		u32 j = (7 - i);
		GET_UINT32_LE(key, ks, 4*KIdx[i][6]);
		b ^= G(a + key, 5);
		GET_UINT32_LE(key, ks, 4*KIdx[i][5]);
		c ^= G(d + key, 21);
		GET_UINT32_LE(key, ks, 4*KIdx[i][4]);
		a = (u32)(a - G(b + key, 13));
		GET_UINT32_LE(key, ks, 4*KIdx[i][3]);
		e = G(b + c + key, 21) ^ (j + 1);
		b += e;
		c = (u32)(c - e);
		GET_UINT32_LE(key, ks, 4*KIdx[i][2]);
		d += G(c + key, 13);
		GET_UINT32_LE(key, ks, 4*KIdx[i][1]);
		b ^= G(a + key, 21);
		GET_UINT32_LE(key, ks, 4*KIdx[i][0]);
		c ^= G(d + key, 5);
		SWAP_BELT(a, b);
		SWAP_BELT(c, d);
		SWAP_BELT(a, d);
	}

	PUT_UINT32_LE(c, out, 0);
	PUT_UINT32_LE(a, out, 4);
	PUT_UINT32_LE(d, out, 8);
	PUT_UINT32_LE(b, out, 12);

	return;
}

/* BELT-HASH primitives */
static void sigma1_xor(const u8 x[2 * BELT_BLOCK_LEN], const u8 h[2 * BELT_BLOCK_LEN], u8 s[BELT_BLOCK_LEN], u8 use_xor){
	u8 tmp1[BELT_BLOCK_LEN];
	unsigned int i;

	for(i = 0; i < (BELT_BLOCK_LEN / 2); i++){
		tmp1[i] = (h[i] ^ h[i + BELT_BLOCK_LEN]);
		tmp1[i + (BELT_BLOCK_LEN / 2)] = (h[i + (BELT_BLOCK_LEN / 2)] ^ h[i + BELT_BLOCK_LEN + (BELT_BLOCK_LEN / 2)]);
	}

	if(use_xor){
		u8 tmp2[BELT_BLOCK_LEN];

		belt_encrypt(tmp1, tmp2, x);

		for(i = 0; i < (BELT_BLOCK_LEN / 2); i++){
			s[i] ^= (tmp1[i] ^ tmp2[i]);
			s[i + (BELT_BLOCK_LEN / 2)] ^= (tmp1[i + (BELT_BLOCK_LEN / 2)] ^ tmp2[i + (BELT_BLOCK_LEN / 2)]);
		}
	}
	else{
		belt_encrypt(tmp1, s, x);
		for(i = 0; i < (BELT_BLOCK_LEN / 2); i++){
			s[i] ^= tmp1[i];
			s[i + (BELT_BLOCK_LEN / 2)] ^= tmp1[i + (BELT_BLOCK_LEN / 2)];
		}
	}

	return;
}

static void sigma2(const u8 x[2 * BELT_BLOCK_LEN], u8 const h[2 * BELT_BLOCK_LEN], u8 result[2 * BELT_BLOCK_LEN])
{
	u8 teta[BELT_KEY_SCHED_LEN];
	u8 tmp[BELT_BLOCK_LEN];
	unsigned int i;

	/* Copy the beginning of h for later in case it is lost */
	IGNORE_RET_VAL(local_memcpy(&tmp[0], &h[0], BELT_BLOCK_LEN));

	sigma1_xor(x, h, teta, 0);
	IGNORE_RET_VAL(local_memcpy(&teta[BELT_BLOCK_LEN], &h[BELT_BLOCK_LEN], BELT_BLOCK_LEN));

	belt_encrypt(x, result, teta);
	for(i = 0; i < BELT_BLOCK_LEN; i++){
		result[i]  ^= x[i];
		teta[i]    ^= 0xff;
		teta[i + BELT_BLOCK_LEN] = tmp[i];
	}

	belt_encrypt(&x[BELT_BLOCK_LEN], &result[BELT_BLOCK_LEN], teta);

	for(i = 0; i < (BELT_BLOCK_LEN / 2); i++){
		result[i + BELT_BLOCK_LEN] ^= x[i + BELT_BLOCK_LEN];
		result[i + BELT_BLOCK_LEN + (BELT_BLOCK_LEN / 2)] ^= x[i + BELT_BLOCK_LEN + (BELT_BLOCK_LEN / 2)];
	}

	return;
}

static void _belt_hash_process(const u8 x[2 * BELT_BLOCK_LEN], u8 h[2 * BELT_BLOCK_LEN], u8 s[BELT_BLOCK_LEN])
{
	sigma1_xor(x, h, s, 1);

	sigma2(x, h, h);

	return;
}

ATTRIBUTE_WARN_UNUSED_RET static int belt_hash_process(belt_hash_context *ctx, const u8 data[BELT_HASH_BLOCK_SIZE])
{
	_belt_hash_process(data, ctx->belt_hash_h, &(ctx->belt_hash_state[BELT_BLOCK_LEN]));

	return 0;
}

ATTRIBUTE_WARN_UNUSED_RET static int belt_hash_finalize(const u8 s[2 * BELT_BLOCK_LEN], const u8 h[2 * BELT_BLOCK_LEN], u8 res[2 * BELT_BLOCK_LEN])
{
	sigma2(s, h, res);

	return 0;
}

static void belt_update_ctr(belt_hash_context *ctx, u8 len_bytes)
{
	/* Perform a simple addition on 128 bits on the first part of the state */
	u64 a0, a1, b, c;

	GET_UINT64_LE(a0, (const u8*)(ctx->belt_hash_state), 0);
	GET_UINT64_LE(a1, (const u8*)(ctx->belt_hash_state), 8);

	b = (u64)(len_bytes << 3);

	c = (a0 + b);
	if(c < b){
		/* Handle carry */
		a1 += 1;
	}

	/* Store the result */
	PUT_UINT64_LE(c,  (u8*)(ctx->belt_hash_state), 0);
	PUT_UINT64_LE(a1, (u8*)(ctx->belt_hash_state), 8);

	return;
}

/* Init hash function. Returns 0 on success, -1 on error. */
int belt_hash_init(belt_hash_context *ctx)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

	ctx->belt_hash_total = 0;

	ret = local_memset(ctx->belt_hash_state, 0, sizeof(ctx->belt_hash_state)); EG(ret, err);

	PUT_UINT64_LE(0x3bf5080ac8ba94b1ULL, ctx->belt_hash_h,  0);
	PUT_UINT64_LE(0xe45d4a588e006d36ULL, ctx->belt_hash_h,  8);
	PUT_UINT64_LE(0xacc7b61b9dfa0485ULL, ctx->belt_hash_h, 16);
	PUT_UINT64_LE(0x0dcefd02c2722e25ULL, ctx->belt_hash_h, 24);

	/* Tell that we are initialized */
	ctx->magic = BELT_HASH_HASH_MAGIC;

	ret = 0;

err:
	return ret;
}

/* Update hash function. Returns 0 on success, -1 on error. */
int belt_hash_update(belt_hash_context *ctx, const u8 *input, u32 ilen)
{
	const u8 *data_ptr = input;
	u32 remain_ilen = ilen;
	u16 fill;
	u8 left;
	int ret;

	MUST_HAVE((input != NULL) || (ilen == 0), ret, err);
	BELT_HASH_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Nothing to process, return */
	if (ilen == 0) {
		ret = 0;
		goto err;
	}

	/* Get what's left in our local buffer */
	left = (ctx->belt_hash_total & (BELT_HASH_BLOCK_SIZE - 1));
	fill = (u16)(BELT_HASH_BLOCK_SIZE - left);

	ctx->belt_hash_total += ilen;

	if ((left > 0) && (remain_ilen >= fill)) {
		/* Copy data at the end of the buffer */
		ret = local_memcpy(ctx->belt_hash_buffer + left, data_ptr, fill); EG(ret, err);
		/* Update the counter with one full block */
		belt_update_ctr(ctx, BELT_HASH_BLOCK_SIZE);
		/* Process */
		ret = belt_hash_process(ctx, ctx->belt_hash_buffer); EG(ret, err);
		data_ptr += fill;
		remain_ilen -= fill;
		left = 0;
	}

	while (remain_ilen >= BELT_HASH_BLOCK_SIZE) {
		/* Update the counter with one full block */
		belt_update_ctr(ctx, BELT_HASH_BLOCK_SIZE);
		/* Process */
		ret = belt_hash_process(ctx, data_ptr); EG(ret, err);
		data_ptr += BELT_HASH_BLOCK_SIZE;
		remain_ilen -= BELT_HASH_BLOCK_SIZE;
	}

	if (remain_ilen > 0) {
		ret = local_memcpy(ctx->belt_hash_buffer + left, data_ptr, remain_ilen); EG(ret, err);
	}

	ret = 0;

err:
	return ret;
}

/* Finalize. Returns 0 on success, -1 on error.*/
int belt_hash_final(belt_hash_context *ctx, u8 output[BELT_HASH_DIGEST_SIZE])
{
	int ret;
	unsigned int i;

	MUST_HAVE((output != NULL), ret, err);
	BELT_HASH_HASH_CHECK_INITIALIZED(ctx, ret, err);

	if((ctx->belt_hash_total % BELT_HASH_BLOCK_SIZE) != 0){
		/* Pad our last block with zeroes */
		for(i = (ctx->belt_hash_total % BELT_HASH_BLOCK_SIZE); i < BELT_HASH_BLOCK_SIZE; i++){
			ctx->belt_hash_buffer[i] = 0;
		}

		/* Update the counter with the remaining data */
		belt_update_ctr(ctx, (u8)(ctx->belt_hash_total % BELT_HASH_BLOCK_SIZE));

		/* Process the last block */
		ret = belt_hash_process(ctx, ctx->belt_hash_buffer); EG(ret, err);
	}

	/* Finalize and output the result */
	ret = belt_hash_finalize(ctx->belt_hash_state, ctx->belt_hash_h, output); EG(ret, err);

	/* Tell that we are uninitialized */
	ctx->magic = WORD(0);

	ret = 0;

err:
	return ret;
}

/*
 * Scattered version performing init/update/finalize on a vector of buffers
 * 'inputs' with the length of each buffer passed via 'ilens'. The function
 * loops on pointers in 'inputs' until it finds a NULL pointer. The function
 * returns 0 on success, -1 on error.
 */
int belt_hash_scattered(const u8 **inputs, const u32 *ilens,
		      u8 output[BELT_HASH_DIGEST_SIZE])
{
	belt_hash_context ctx;
	int ret, pos = 0;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = belt_hash_init(&ctx); EG(ret, err);

	while (inputs[pos] != NULL) {
		ret = belt_hash_update(&ctx, inputs[pos], ilens[pos]); EG(ret, err);
		pos += 1;
	}

	ret = belt_hash_final(&ctx, output);

err:
	return ret;
}

/*
 * Single call version performing init/update/final on given input.
 * Returns 0 on success, -1 on error.
 */
int belt_hash(const u8 *input, u32 ilen, u8 output[BELT_HASH_DIGEST_SIZE])
{
	belt_hash_context ctx;
	int ret;

	ret = belt_hash_init(&ctx); EG(ret, err);
	ret = belt_hash_update(&ctx, input, ilen); EG(ret, err);
	ret = belt_hash_final(&ctx, output);

err:
	return ret;
}

#else /* WITH_HASH_BELT_HASH */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;

#endif /* WITH_HASH_BELT_HASH */
