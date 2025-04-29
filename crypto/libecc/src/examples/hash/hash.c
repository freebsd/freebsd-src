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
#include "hash.h"

/* Get a libecc hash type and mapping from a generic hash type */
ATTRIBUTE_WARN_UNUSED_RET static int get_libecc_hash(gen_hash_alg_type gen_hash_type, hash_alg_type *hash_type, const hash_mapping **hm, u8 *hlen, u8 *block_size)
{
	int ret;
	hash_alg_type htype = UNKNOWN_HASH_ALG;

	MUST_HAVE((hash_type != NULL) && (hm != NULL), ret, err);

	switch(gen_hash_type){
		case HASH_SHA224:{
#ifdef WITH_HASH_SHA224
			htype = SHA224;
#endif
			break;
		}
		case HASH_SHA256:{
#ifdef WITH_HASH_SHA256
			htype = SHA256;
#endif
			break;
		}
		case HASH_SHA384:{
#ifdef WITH_HASH_SHA384
			htype = SHA384;
#endif
			break;
		}
		case HASH_SHA512:{
#ifdef WITH_HASH_SHA512
			htype = SHA512;
#endif
			break;
		}
		case HASH_SHA512_224:{
#ifdef WITH_HASH_SHA512_224
			htype = SHA512_224;
#endif
			break;
		}
		case HASH_SHA512_256:{
#ifdef WITH_HASH_SHA512_256
			htype = SHA512_256;
#endif
			break;
		}
		case HASH_SHA3_224:{
#ifdef WITH_HASH_SHA3_224
			htype = SHA3_224;
#endif
			break;
		}
		case HASH_SHA3_256:{
#ifdef WITH_HASH_SHA3_256
			htype = SHA3_256;
#endif
			break;
		}
		case HASH_SHA3_384:{
#ifdef WITH_HASH_SHA3_384
			htype = SHA3_384;
#endif
			break;
		}
		case HASH_SHA3_512:{
#ifdef WITH_HASH_SHA3_512
			htype = SHA3_512;
#endif
			break;
		}
		case HASH_SM3:{
#ifdef WITH_HASH_SM3
			htype = SM3;
#endif
			break;
		}
		case HASH_STREEBOG256:{
#ifdef WITH_HASH_STREEBOG256
			htype = STREEBOG256;
#endif
			break;
		}
		case HASH_STREEBOG512:{
#ifdef WITH_HASH_STREEBOG512
			htype = STREEBOG512;
#endif
			break;
		}
		case HASH_SHAKE256:{
#ifdef WITH_HASH_SHAKE256
			htype = SHAKE256;
#endif
			break;
		}
		case HASH_RIPEMD160:{
#ifdef WITH_HASH_RIPEMD160
			htype = RIPEMD160;
#endif
			break;
		}
		case HASH_BELT_HASH:{
#ifdef WITH_HASH_BELT_HASH
			htype = BELT_HASH;
#endif
			break;
		}
		case HASH_BASH224:{
#ifdef WITH_HASH_BASH224
			htype = BASH224;
#endif
			break;
		}
		case HASH_BASH256:{
#ifdef WITH_HASH_BASH256
			htype = BASH256;
#endif
			break;
		}
		case HASH_BASH384:{
#ifdef WITH_HASH_BASH384
			htype = BASH384;
#endif
			break;
		}
		case HASH_BASH512:{
#ifdef WITH_HASH_BASH512
			htype = BASH512;
#endif
			break;
		}

		default:{
			htype = UNKNOWN_HASH_ALG;
			break;
		}
	}
	if(htype != UNKNOWN_HASH_ALG){
		(*hash_type) = htype;
		ret = get_hash_by_type(htype, hm); EG(ret, err);
		ret = get_hash_sizes(htype, hlen, block_size); EG(ret, err);
		MUST_HAVE(((*hlen) <= MAX_DIGEST_SIZE), ret, err);
		ret = 0;
	}
	else{
		ret = -1;
	}

err:
	if(ret && (hm != NULL)){
		(*hm) = NULL;
	}
	if(ret && (hash_type != NULL)){
		(*hash_type) = UNKNOWN_HASH_ALG;
	}
	return ret;
}

int gen_hash_get_hash_sizes(gen_hash_alg_type gen_hash_type, u8 *hlen, u8 *block_size)
{
        int ret;

        MUST_HAVE((hlen != NULL) && (block_size != NULL), ret, err);

        switch(gen_hash_type){
                case HASH_MD2:{
                        (*hlen) = MD2_DIGEST_SIZE;
                        (*block_size) = MD2_BLOCK_SIZE;
                        ret = 0;
                        break;
                }
                case HASH_MD4:{
                        (*hlen) = MD4_DIGEST_SIZE;
                        (*block_size) = MD4_BLOCK_SIZE;
                        ret = 0;
                        break;
                }
                case HASH_MD5:{
                        (*hlen) = MD5_DIGEST_SIZE;
                        (*block_size) = MD5_BLOCK_SIZE;
                        ret = 0;
                        break;
                }
                case HASH_SHA0:{
                        (*hlen) = SHA0_DIGEST_SIZE;
                        (*block_size) = SHA0_BLOCK_SIZE;
                        ret = 0;
                        break;
                }
                case HASH_SHA1:{
                        (*hlen) = SHA1_DIGEST_SIZE;
                        (*block_size) = SHA1_BLOCK_SIZE;
                        ret = 0;
                        break;
                }
		case HASH_MDC2_PADDING1:
		case HASH_MDC2_PADDING2:{
                        (*hlen) = MDC2_DIGEST_SIZE;
                        (*block_size) = MDC2_BLOCK_SIZE;
                        ret = 0;
                        break;
                }
		case HASH_GOST34_11_94_NORM:
		case HASH_GOST34_11_94_RFC4357:{
                        (*hlen) = GOSTR34_11_94_DIGEST_SIZE;
                        (*block_size) = GOSTR34_11_94_BLOCK_SIZE;
                        ret = 0;
                        break;
                }
                /* The default case falls back to a genuine libecc hash function */
                default:{
                        const hash_mapping *hm;
                        hash_alg_type hash_type;
                        ret = get_libecc_hash(gen_hash_type, &hash_type, &hm, hlen, block_size); EG(ret, err);
                        break;
                }
        }

err:
        return ret;
}

int gen_hash_hfunc_scattered(const u8 **input, const u32 *ilen, u8 *digest, gen_hash_alg_type gen_hash_type)
{
	int ret;

	switch(gen_hash_type){
		case HASH_MD2:{
			ret = md2_scattered(input, ilen, digest); EG(ret, err);
			break;
		}
		case HASH_MD4:{
			ret = md4_scattered(input, ilen, digest); EG(ret, err);
			break;
		}
		case HASH_MD5:{
			ret = md5_scattered(input, ilen, digest); EG(ret, err);
			break;
		}
		case HASH_SHA0:{
			ret = sha0_scattered(input, ilen, digest); EG(ret, err);
			break;
		}
		case HASH_SHA1:{
			ret = sha1_scattered(input, ilen, digest); EG(ret, err);
			break;
		}
		case HASH_MDC2_PADDING1:{
			ret = mdc2_scattered_padding1(input, ilen, digest); EG(ret, err);
			break;
		}
		case HASH_MDC2_PADDING2:{
			ret = mdc2_scattered_padding2(input, ilen, digest); EG(ret, err);
			break;
		}
		case HASH_GOST34_11_94_NORM:{
			ret = gostr34_11_94_scattered_norm(input, ilen, digest); EG(ret, err);
			break;
		}
		case HASH_GOST34_11_94_RFC4357:{
			ret = gostr34_11_94_scattered_rfc4357(input, ilen, digest); EG(ret, err);
			break;
		}
		/* The fallback should be libecc type */
		default:{
			const hash_mapping *hm;
			hash_alg_type hash_type;
			u8 hlen, block_size;
			ret = get_libecc_hash(gen_hash_type, &hash_type, &hm, &hlen, &block_size); EG(ret, err);
			MUST_HAVE((hm != NULL), ret, err);
			ret = hm->hfunc_scattered(input, ilen, digest); EG(ret, err);
			break;
		}
	}

err:
	return ret;
}

int gen_hash_hfunc(const u8 *input, u32 ilen, u8 *digest, gen_hash_alg_type gen_hash_type)
{
	const u8 *inputs[2] = { input, NULL };
	u32 ilens[2] = { ilen, 0 };

	return gen_hash_hfunc_scattered(inputs, ilens, digest, gen_hash_type);
}

int gen_hash_init(gen_hash_context *ctx, gen_hash_alg_type gen_hash_type)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

	switch(gen_hash_type){
		case HASH_MD2:{
			ret = md2_init(&(ctx->md2ctx)); EG(ret, err);
			break;
		}
		case HASH_MD4:{
			ret = md4_init(&(ctx->md4ctx)); EG(ret, err);
			break;
		}
		case HASH_MD5:{
			ret = md5_init(&(ctx->md5ctx)); EG(ret, err);
			break;
		}
		case HASH_SHA0:{
			ret = sha0_init(&(ctx->sha0ctx)); EG(ret, err);
			break;
		}
		case HASH_SHA1:{
			ret = sha1_init(&(ctx->sha1ctx)); EG(ret, err);
			break;
		}
		case HASH_MDC2_PADDING1:{
			ret = mdc2_init(&(ctx->mdc2ctx)); EG(ret, err);
			ret = mdc2_set_padding_type(&(ctx->mdc2ctx), ISOIEC10118_TYPE1); EG(ret, err);
			break;
		}
		case HASH_MDC2_PADDING2:{
			ret = mdc2_init(&(ctx->mdc2ctx)); EG(ret, err);
			ret = mdc2_set_padding_type(&(ctx->mdc2ctx), ISOIEC10118_TYPE2); EG(ret, err);
			break;
		}
		case HASH_GOST34_11_94_NORM:{
			ret = gostr34_11_94_init(&(ctx->gostr34_11_94ctx)); EG(ret, err);
			ret = gostr34_11_94_set_type(&(ctx->gostr34_11_94ctx), GOST34_11_94_NORM); EG(ret, err);
			break;
		}
		case HASH_GOST34_11_94_RFC4357:{
			ret = gostr34_11_94_init(&(ctx->gostr34_11_94ctx)); EG(ret, err);
			ret = gostr34_11_94_set_type(&(ctx->gostr34_11_94ctx), GOST34_11_94_RFC4357); EG(ret, err);
			break;
		}
		/* The fallback should be libecc type */
		default:{
			const hash_mapping *hm;
			hash_alg_type hash_type;
			u8 hlen, block_size;
			ret = get_libecc_hash(gen_hash_type, &hash_type, &hm, &hlen, &block_size); EG(ret, err);
			MUST_HAVE((hm != NULL), ret, err);
			ret = hm->hfunc_init(&(ctx->hctx)); EG(ret, err);
			break;
		}
	}

err:
	return ret;
}

int gen_hash_update(gen_hash_context *ctx, const u8 *chunk, u32 chunklen, gen_hash_alg_type gen_hash_type)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

	switch(gen_hash_type){
		case HASH_MD2:{
			ret = md2_update(&(ctx->md2ctx), chunk, chunklen); EG(ret, err);
			break;
		}
		case HASH_MD4:{
			ret = md4_update(&(ctx->md4ctx), chunk, chunklen); EG(ret, err);
			break;
		}
		case HASH_MD5:{
			ret = md5_update(&(ctx->md5ctx), chunk, chunklen); EG(ret, err);
			break;
		}
		case HASH_SHA0:{
			ret = sha0_update(&(ctx->sha0ctx), chunk, chunklen); EG(ret, err);
			break;
		}
		case HASH_SHA1:{
			ret = sha1_update(&(ctx->sha1ctx), chunk, chunklen); EG(ret, err);
			break;
		}
		case HASH_MDC2_PADDING1:
		case HASH_MDC2_PADDING2:{
			ret = mdc2_update(&(ctx->mdc2ctx), chunk, chunklen); EG(ret, err);
			break;
		}
		case HASH_GOST34_11_94_NORM:
		case HASH_GOST34_11_94_RFC4357:{
			ret = gostr34_11_94_update(&(ctx->gostr34_11_94ctx), chunk, chunklen); EG(ret, err);
			break;
		}
		/* The fallback should be libecc type */
		default:{
			const hash_mapping *hm;
			hash_alg_type hash_type;
			u8 hlen, block_size;
			ret = get_libecc_hash(gen_hash_type, &hash_type, &hm, &hlen, &block_size); EG(ret, err);
			MUST_HAVE((hm != NULL), ret, err);
			ret = hm->hfunc_update(&(ctx->hctx), chunk, chunklen); EG(ret, err);
			break;
		}
	}

err:
	return ret;
}

int gen_hash_final(gen_hash_context *ctx, u8 *output, gen_hash_alg_type gen_hash_type)
{
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

	switch(gen_hash_type){
		case HASH_MD2:{
			ret = md2_final(&(ctx->md2ctx), output); EG(ret, err);
			break;
		}
		case HASH_MD4:{
			ret = md4_final(&(ctx->md4ctx), output); EG(ret, err);
			break;
		}
		case HASH_MD5:{
			ret = md5_final(&(ctx->md5ctx), output); EG(ret, err);
			break;
		}
		case HASH_SHA0:{
			ret = sha0_final(&(ctx->sha0ctx), output); EG(ret, err);
			break;
		}
		case HASH_SHA1:{
			ret = sha1_final(&(ctx->sha1ctx), output); EG(ret, err);
			break;
		}
		case HASH_MDC2_PADDING1:
		case HASH_MDC2_PADDING2:{
			ret = mdc2_final(&(ctx->mdc2ctx), output); EG(ret, err);
			break;
		}
		case HASH_GOST34_11_94_NORM:
		case HASH_GOST34_11_94_RFC4357:{
			ret = gostr34_11_94_final(&(ctx->gostr34_11_94ctx), output); EG(ret, err);
			break;
		}
		/* The fallback should be libecc type */
		default:{
			const hash_mapping *hm;
			hash_alg_type hash_type;
			u8 hlen, block_size;
			ret = get_libecc_hash(gen_hash_type, &hash_type, &hm, &hlen, &block_size); EG(ret, err);
			MUST_HAVE((hm != NULL), ret, err);
			ret = hm->hfunc_finalize(&(ctx->hctx), output); EG(ret, err);
			break;
		}
	}

err:
	return ret;
}

#ifdef HASH
#include <libecc/utils/print_buf.h>
int main(int argc, char *argv[])
{
        int ret = 0;
	unsigned int i;

	const u8 input[] = "Now is the time for all ";
	const u8 input2[] = "\x54\x68\x69\x73\x20\x69\x73\x20\x6D\x65\x73\x73\x61\x67\x65\x2C\x20\x6C\x65\x6E\x67\x74\x68\x3D\x33\x32\x20\x62\x79\x74\x65\x73";
	const u8 input3[] = "";
	const u8 input4[] = "Suppose the original message has length = 50 bytes";
	u8 input5[128];
	u8 output[32];

        FORCE_USED_VAR(argc);
        FORCE_USED_VAR(argv);

	ret = local_memset(input5, 0, sizeof(input5)); EG(ret, err);

	ret = gen_hash_hfunc(input, sizeof(input)-1, output, HASH_MDC2_PADDING1); EG(ret, err);
	buf_print("mdc2 padding1", output, 16);

	ret = gen_hash_hfunc(input, sizeof(input)-1, output, HASH_MDC2_PADDING2); EG(ret, err);
	buf_print("mdc2 padding2", output, 16);

	ret = gen_hash_hfunc(input2, sizeof(input2)-1, output, HASH_GOST34_11_94_NORM); EG(ret, err);
	buf_print("gostr34_11_94 NORM", output, 32);

	ret = gen_hash_hfunc(input3, sizeof(input3)-1, output, HASH_GOST34_11_94_NORM); EG(ret, err);
	buf_print("gostr34_11_94 NORM", output, 32);

	ret = gen_hash_hfunc(input4, sizeof(input4)-1, output, HASH_GOST34_11_94_NORM); EG(ret, err);
	buf_print("gostr34_11_94 NORM", output, 32);

	for(i = 0; i < sizeof(input5); i++){
		input5[i] = 'U';
	}
	ret = gen_hash_hfunc(input5, sizeof(input5), output, HASH_GOST34_11_94_NORM); EG(ret, err);
	buf_print("gostr34_11_94 NORM", output, 32);

err:
        return ret;
}
#endif
