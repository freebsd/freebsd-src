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
#include <libecc/lib_ecc_config.h>
#ifdef WITH_HMAC

#include <libecc/hash/hmac.h>

int hmac_init(hmac_context *ctx, const u8 *hmackey, u32 hmackey_len,
	      hash_alg_type hash_type)
{
	u8 ipad[MAX_BLOCK_SIZE];
	u8 opad[MAX_BLOCK_SIZE];
	u8 local_hmac_key[MAX_BLOCK_SIZE];
	unsigned int i, local_hmac_key_len;
	int ret;
	const hash_mapping *h;

	MUST_HAVE((ctx != NULL) && (hmackey != NULL), ret, err);

	ret = local_memset(local_hmac_key, 0, sizeof(local_hmac_key)); EG(ret, err);
	/* Set ipad and opad to appropriate values */
	ret = local_memset(ipad, 0x36, sizeof(ipad)); EG(ret, err);
	ret = local_memset(opad, 0x5c, sizeof(opad)); EG(ret, err);

	/* Get the hash mapping of the current asked hash function */
	ret = get_hash_by_type(hash_type, &(ctx->hash)); EG(ret, err);
	MUST_HAVE((ctx->hash != NULL), ret, err);

	/* Make things more readable */
	h = ctx->hash;

	if(hmackey_len <= ctx->hash->block_size){
		/* The key size is less than the hash function block size */
		ret = local_memcpy(local_hmac_key, hmackey, hmackey_len); EG(ret, err);
		local_hmac_key_len = hmackey_len;
	}
	else{
		/* The key size is greater than the hash function block size.
		 * We hash it to shorten it.
		 */
		hash_context tmp_ctx;
		/* Check our callback */
		ret = hash_mapping_callbacks_sanity_check(h); EG(ret, err);
		ret = h->hfunc_init(&tmp_ctx); EG(ret, err);
		ret = h->hfunc_update(&tmp_ctx, hmackey, hmackey_len); EG(ret, err);
		ret = h->hfunc_finalize(&tmp_ctx, local_hmac_key); EG(ret, err);
		local_hmac_key_len = h->digest_size;
	}

	/* Initialize our input and output hash contexts */
	/* Check our callback */
	ret = hash_mapping_callbacks_sanity_check(h); EG(ret, err);
	ret = h->hfunc_init(&(ctx->in_ctx)); EG(ret, err);
	/* Check our callback */
	ret = hash_mapping_callbacks_sanity_check(h); EG(ret, err);
	ret = h->hfunc_init(&(ctx->out_ctx)); EG(ret, err);

	/* Update our input context with K^ipad */
	for(i = 0; i < local_hmac_key_len; i++){
		ipad[i] ^= local_hmac_key[i];
	}
	ret = h->hfunc_update(&(ctx->in_ctx), ipad, h->block_size); EG(ret, err);
	/* Update our output context with K^opad */
	for(i = 0; i < local_hmac_key_len; i++){
		opad[i] ^= local_hmac_key[i];
	}
	ret = h->hfunc_update(&(ctx->out_ctx), opad, h->block_size); EG(ret, err);

	/* Initialize our magic */
	ctx->magic = HMAC_MAGIC;

err:
	return ret;
}

int hmac_update(hmac_context *ctx, const u8 *input, u32 ilen)
{
	int ret;
	const hash_mapping *h;

	HMAC_CHECK_INITIALIZED(ctx, ret, err);
	MUST_HAVE((input != NULL) || (ilen == 0), ret, err);

	/* Make things more readable */
	h = ctx->hash;
	/* Check our callback */
	ret = hash_mapping_callbacks_sanity_check(h); EG(ret, err);
	ret = h->hfunc_update(&(ctx->in_ctx), input, ilen); EG(ret, err);

err:
	return ret;
}

int hmac_finalize(hmac_context *ctx, u8 *output, u8 *outlen)
{
	int ret;
	u8 in_hash[MAX_DIGEST_SIZE];
	const hash_mapping *h;

	HMAC_CHECK_INITIALIZED(ctx, ret, err);
	MUST_HAVE((output != NULL) && (outlen != NULL), ret, err);

	/* Make things more readable */
	h = ctx->hash;

	MUST_HAVE(((*outlen) >= h->digest_size), ret, err);

	/* Check our callback */
	ret = hash_mapping_callbacks_sanity_check(h); EG(ret, err);
	ret = h->hfunc_finalize(&(ctx->in_ctx), in_hash); EG(ret, err);
	ret = h->hfunc_update(&(ctx->out_ctx), in_hash, h->digest_size); EG(ret, err);
	ret = h->hfunc_finalize(&(ctx->out_ctx), output); EG(ret, err);
	(*outlen) = h->digest_size;

err:
	if(ctx != NULL){
		/* Clear the hash contexts that could contain sensitive data */
		IGNORE_RET_VAL(local_memset(ctx, 0, sizeof(hmac_context)));
		/* Uninitialize the context  */
		ctx->magic = WORD(0);
	}
	if(ret && (outlen != NULL)){
		(*outlen) = 0;
	}
	return ret;
}

int hmac(const u8 *hmackey, u32 hmackey_len, hash_alg_type hash_type,
	 const u8 *input, u32 ilen, u8 *output, u8 *outlen)
{
	int ret;
	hmac_context ctx;

	ret = hmac_init(&ctx, hmackey, hmackey_len, hash_type); EG(ret, err);
	ret = hmac_update(&ctx, input, ilen); EG(ret, err);
	ret = hmac_finalize(&ctx, output, outlen);

err:
	/* Clean our context as it can contain sensitive data */
	IGNORE_RET_VAL(local_memset(&ctx, 0, sizeof(hmac_context)));

	return ret;
}

/* Scattered version */
int hmac_scattered(const u8 *hmackey, u32 hmackey_len, hash_alg_type hash_type,
	 const u8 **inputs, const u32 *ilens, u8 *output, u8 *outlen)
{
	int ret, pos = 0;
	hmac_context ctx;

	MUST_HAVE((inputs != NULL) && (ilens != NULL) && (output != NULL), ret, err);

	ret = hmac_init(&ctx, hmackey, hmackey_len, hash_type); EG(ret, err);

	while (inputs[pos] != NULL) {
		ret = hmac_update(&ctx, inputs[pos], ilens[pos]); EG(ret, err);
		pos += 1;
	}

	ret = hmac_finalize(&ctx, output, outlen);

err:
	/* Clean our context as it can contain sensitive data */
	IGNORE_RET_VAL(local_memset(&ctx, 0, sizeof(hmac_context)));

	return ret;
}


#else /* WITH_HMAC */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_HMAC */
