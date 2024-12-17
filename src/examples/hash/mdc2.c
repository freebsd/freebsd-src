/*
 *  Copyright (C) 2021 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include "mdc2.h"

/* Include DES helpers */
#include "tdes.h"

ATTRIBUTE_WARN_UNUSED_RET int mdc2_set_padding_type(mdc2_context *ctx,
								padding_type p)
{
	int ret;

	MDC2_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* We cannot change the padding type after the first update */
	MUST_HAVE((ctx->mdc2_total == 0), ret, err);

	if((p != ISOIEC10118_TYPE1) && (p != ISOIEC10118_TYPE2)){
		ret = -1;
		goto err;
	}

	ctx->padding = p;

	ret = 0;

err:
	return ret;
}

/* MDC-2 core processing. Returns 0 on success, -1 on error. */
ATTRIBUTE_WARN_UNUSED_RET static inline int mdc2_process(mdc2_context *ctx,
			   const u8 data[MDC2_BLOCK_SIZE])
{
	int ret;
	unsigned int j;
	u8 V[8], W[8];
	u8 *A, *B;
	des_context des_ctx;

	/* Get the current internal state in A and B */
	A = (u8*)&(ctx->mdc2_state[0]);
	B = (u8*)&(ctx->mdc2_state[8]);

	A[0] = (u8)((A[0] & 0x9f) | 0x40);
	B[0] = (u8)((B[0] & 0x9f) | 0x20);
	/* Set odd parity */
	for(j = 0; j < 8; j++){
		A[j] = odd_parity[A[j]];
		B[j] = odd_parity[B[j]];
	}
	/* Compute V_i = M_i + E(M_i, A_i) */
	ret = local_memset(&des_ctx, 0, sizeof(des_context)); EG(ret, err);
	ret = des_set_key(&des_ctx, A, DES_ENCRYPTION); EG(ret, err);
	ret = des(&des_ctx, &data[0], V); EG(ret, err);
	for(j = 0; j < 8; j++){
		V[j] = (V[j] ^ data[j]);
	}
	/* Compute W_i = M_i + E(M_i, B_i) */
	ret = local_memset(&des_ctx, 0, sizeof(des_context)); EG(ret, err);
	ret = des_set_key(&des_ctx, B, DES_ENCRYPTION); EG(ret, err);
	ret = des(&des_ctx, &data[0], W); EG(ret, err);
	for(j = 0; j < 8; j++){
		W[j] = (W[j] ^ data[j]);
	}
	/* Cross the results */
	/* In A */
	ret = local_memcpy(&A[0], &V[0], 4); EG(ret, err);
	ret = local_memcpy(&A[4], &W[4], 4); EG(ret, err);
	/* In B */
	ret = local_memcpy(&B[0], &W[0], 4); EG(ret, err);
	ret = local_memcpy(&B[4], &V[4], 4); EG(ret, err);

err:
	return ret;
}

/* Init hash function. Returns 0 on success, -1 on error. */
ATTRIBUTE_WARN_UNUSED_RET int mdc2_init(mdc2_context *ctx)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

        /* Sanity check on size */
	MUST_HAVE((MDC2_DIGEST_SIZE <= MAX_DIGEST_SIZE), ret, err);

	ctx->mdc2_total = 0;
	/* Initialize A1 */
	ret = local_memset(&(ctx->mdc2_state[0]), 0x52, 8); EG(ret, err);
	/* Initialize B1 */
	ret = local_memset(&(ctx->mdc2_state[8]), 0x25, 8); EG(ret, err);
	/* Initialize default padding type */
	ctx->padding = ISOIEC10118_TYPE1;

	/* Tell that we are initialized */
	ctx->magic = MDC2_HASH_MAGIC;

	ret = 0;

err:
	return ret;
}

ATTRIBUTE_WARN_UNUSED_RET int mdc2_update(mdc2_context *ctx, const u8 *input, u32 ilen)
{
	const u8 *data_ptr = input;
	u32 remain_ilen = ilen;
	u16 fill;
	u8 left;
	int ret;

	MUST_HAVE((input != NULL) || (ilen == 0), ret, err);
	MDC2_HASH_CHECK_INITIALIZED(ctx, ret, err);

	/* Nothing to process, return */
	if (ilen == 0) {
		ret = 0;
		goto err;
	}

	/* Get what's left in our local buffer */
	left = (ctx->mdc2_total & 0xF);
	fill = (u16)(MDC2_BLOCK_SIZE - left);

	ctx->mdc2_total += ilen;

	if ((left > 0) && (remain_ilen >= fill)) {
		/* Copy data at the end of the buffer */
		ret = local_memcpy(ctx->mdc2_buffer + left, data_ptr, fill); EG(ret, err);
		ret = mdc2_process(ctx, ctx->mdc2_buffer); EG(ret, err);
		data_ptr += fill;
		remain_ilen -= fill;
		left = 0;
	}

	while (remain_ilen >= MDC2_BLOCK_SIZE) {
		ret = mdc2_process(ctx, data_ptr); EG(ret, err);
		data_ptr += MDC2_BLOCK_SIZE;
		remain_ilen -= MDC2_BLOCK_SIZE;
	}

	if (remain_ilen > 0) {
		ret = local_memcpy(ctx->mdc2_buffer + left, data_ptr, remain_ilen); EG(ret, err);
	}

	ret = 0;

err:
	return ret;
}

/* Finalize. Returns 0 on success, -1 on error.*/
ATTRIBUTE_WARN_UNUSED_RET int mdc2_final(mdc2_context *ctx, u8 output[MDC2_DIGEST_SIZE])
{
	int ret;
	unsigned int i;
	u8 pad_byte;

	MUST_HAVE((output != NULL), ret, err);
	MDC2_HASH_CHECK_INITIALIZED(ctx, ret, err);

	if(ctx->padding == ISOIEC10118_TYPE1){
		/* "Padding method 1" in ISO-IEC-10118 */
		/* This is our final step, so we proceed with the padding: the last block
		 * is padded with zeroes.
		 */
		pad_byte = 0x00;
		if((ctx->mdc2_total % MDC2_BLOCK_SIZE) != 0){
			for(i = (ctx->mdc2_total % MDC2_BLOCK_SIZE); i < MDC2_BLOCK_SIZE; i++){
				ctx->mdc2_buffer[i] = pad_byte;
			}
			/* And process the block */
			ret = mdc2_process(ctx, ctx->mdc2_buffer); EG(ret, err);
		}
	}
	else if(ctx->padding == ISOIEC10118_TYPE2){
		/* "Padding method 2" in ISO-IEC-10118 */
		/* This is our final step, so we proceed with the padding: the last block
		 * is appended 0x80 and then padded with zeroes.
		 */
		ctx->mdc2_buffer[(ctx->mdc2_total % MDC2_BLOCK_SIZE)] = 0x80;
		pad_byte = 0x00;
		for(i = ((unsigned int)(ctx->mdc2_total % MDC2_BLOCK_SIZE) + 1); i < MDC2_BLOCK_SIZE; i++){
			ctx->mdc2_buffer[i] = pad_byte;
		}
		/* And process the block */
		ret = mdc2_process(ctx, ctx->mdc2_buffer); EG(ret, err);
	}
	else{
		/* Unkown padding */
		ret = -1;
		goto err;
	}

	/* Output the hash result */
	ret = local_memcpy(output, ctx->mdc2_state, MDC2_DIGEST_SIZE); EG(ret, err);

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
ATTRIBUTE_WARN_UNUSED_RET int mdc2_scattered(const u8 **inputs, const u32 *ilens,
		      u8 output[MDC2_DIGEST_SIZE], padding_type p)
{
	mdc2_context ctx;
	int ret, pos = 0;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = mdc2_init(&ctx); EG(ret, err);

	ret = mdc2_set_padding_type(&ctx, p); EG(ret, err);

	while (inputs[pos] != NULL) {
		ret = mdc2_update(&ctx, inputs[pos], ilens[pos]); EG(ret, err);
		pos += 1;
	}

	ret = mdc2_final(&ctx, output);

err:
	return ret;
}

/*
 * Scattered version performing init/update/finalize on a vector of buffers
 * 'inputs' with the length of each buffer passed via 'ilens'. The function
 * loops on pointers in 'inputs' until it finds a NULL pointer. The function
 * returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int mdc2_scattered_padding1(const u8 **inputs, const u32 *ilens,
		      u8 output[MDC2_DIGEST_SIZE])
{
	return mdc2_scattered(inputs, ilens, output, ISOIEC10118_TYPE1);
}

/*
 * Scattered version performing init/update/finalize on a vector of buffers
 * 'inputs' with the length of each buffer passed via 'ilens'. The function
 * loops on pointers in 'inputs' until it finds a NULL pointer. The function
 * returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int mdc2_scattered_padding2(const u8 **inputs, const u32 *ilens,
		      u8 output[MDC2_DIGEST_SIZE])
{
	return mdc2_scattered(inputs, ilens, output, ISOIEC10118_TYPE2);
}

/*
 * Single call version performing init/update/final on given input.
 * Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int mdc2(const u8 *input, u32 ilen, u8 output[MDC2_DIGEST_SIZE], padding_type p)
{
	mdc2_context ctx;
	int ret;

	ret = mdc2_init(&ctx); EG(ret, err);
	ret = mdc2_set_padding_type(&ctx, p); EG(ret, err);
	ret = mdc2_update(&ctx, input, ilen); EG(ret, err);
	ret = mdc2_final(&ctx, output);

err:
	return ret;
}


/*
 * Single call version performing init/update/final on given input.
 * Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int mdc2_padding1(const u8 *input, u32 ilen, u8 output[MDC2_DIGEST_SIZE])
{
	return mdc2(input, ilen, output, ISOIEC10118_TYPE1);
}

/*
 * Single call version performing init/update/final on given input.
 * Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int mdc2_padding2(const u8 *input, u32 ilen, u8 output[MDC2_DIGEST_SIZE])
{
	return mdc2(input, ilen, output, ISOIEC10118_TYPE2);
}
