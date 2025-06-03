/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/hash/hash_algs.h>

/*
 * Return the hash mapping entry 'hm' associated with given hash name
 * 'hash_name'. The function returns 0 on success, -1 on error. 'hm'
 * is only meaningful on success.
 */
ATTRIBUTE_WARN_UNUSED_RET int get_hash_by_name(const char *hash_name, const hash_mapping **hm)
{
	const hash_mapping *_hm = NULL;
	int ret, check;
	u8 i;

	MUST_HAVE(((hash_name != NULL) && (hm != NULL)), ret, err);

	ret = -1;
	for (i = 0, _hm = &hash_maps[i]; _hm->type != UNKNOWN_HASH_ALG;
	     _hm = &hash_maps[++i]) {
		const char *exp_name = (const char *)_hm->name;

		if ((!are_str_equal(hash_name, exp_name, &check)) && check) {
			(*hm) = _hm;
			ret = 0;
			break;
		}
	}

err:
	return ret;
}

/*
 * Return the hash mapping entry 'hm' associated with given hash type value.
 * The function returns 0 on success, -1 on error. 'hm' is not meaningfull
 * on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int get_hash_by_type(hash_alg_type hash_type, const hash_mapping **hm)
{
	const hash_mapping *_hm = NULL;
	int ret;
	u8 i;

	MUST_HAVE((hm != NULL), ret, err);

	ret = -1;
	for (i = 0, _hm = &hash_maps[i]; _hm->type != UNKNOWN_HASH_ALG;
	     _hm = &hash_maps[++i]) {
		if (_hm->type == hash_type) {
			(*hm) = _hm;
			ret = 0;
			break;
		}
	}

err:
	return ret;
}

/*
 * Returns respectively in digest_size and block_size param the digest size
 * and block size for given hash function, if return value of the function is 0.
 * If return value is -1, then the hash algorithm is not known and output
 * parameters are not modified.
 */
ATTRIBUTE_WARN_UNUSED_RET int get_hash_sizes(hash_alg_type hash_type, u8 *digest_size, u8 *block_size)
{
	const hash_mapping *m;
	int ret;
	u8 i;

	ret = -1;
	for (i = 0, m = &hash_maps[i]; m->type != UNKNOWN_HASH_ALG;
	     m = &hash_maps[++i]) {
		if (m->type == hash_type) {
			if (digest_size != NULL) {
				(*digest_size) = m->digest_size;
			}
			if (block_size != NULL) {
				(*block_size) = m->block_size;
			}
			ret = 0;
			break;
		}
	}

	return ret;
}

/*
 * Helper that sanity checks the provided hash mapping against our
 * constant ones. Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET int hash_mapping_callbacks_sanity_check(const hash_mapping *h)
{
	const hash_mapping *m;
	int ret = -1, check;
	u8 i;

	MUST_HAVE((h != NULL), ret, err);

	/* We just check is our mapping is indeed
	 * one of the registered mappings.
	 */
	for (i = 0, m = &hash_maps[i]; m->type != UNKNOWN_HASH_ALG;
	     m = &hash_maps[++i]) {
		if (m->type == h->type) {
			if ((!are_str_equal_nlen(m->name, h->name, MAX_HASH_ALG_NAME_LEN, &check)) && (!check)){
				goto err;
			} else if (m->digest_size != h->digest_size) {
				goto err;
			} else if(m->block_size != h->block_size) {
				goto err;
			} else if(m->hfunc_init != h->hfunc_init) {
				goto err;
			} else if(m->hfunc_update != h->hfunc_update) {
				goto err;
			} else if(m->hfunc_finalize != h->hfunc_finalize) {
				goto err;
			} else if(m->hfunc_scattered != h->hfunc_scattered) {
				goto err;
			} else{
				ret = 0;
			}
		}
	}

err:
	return ret;
}

/*****************************************/
/* Trampolines to each specific function to
 * handle typing of our generic union structure.
 */
#ifdef WITH_HASH_SHA224
ATTRIBUTE_WARN_UNUSED_RET int _sha224_init(hash_context * hctx)
{
	return sha224_init((sha224_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha224_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return sha224_update((sha224_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha224_final(hash_context * hctx, unsigned char *output)
{
	return sha224_final((sha224_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_SHA256
ATTRIBUTE_WARN_UNUSED_RET int _sha256_init(hash_context * hctx)
{
	return sha256_init((sha256_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha256_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return sha256_update((sha256_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha256_final(hash_context * hctx, unsigned char *output)
{
	return sha256_final((sha256_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_SHA384
ATTRIBUTE_WARN_UNUSED_RET int _sha384_init(hash_context * hctx)
{
	return sha384_init((sha384_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha384_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return sha384_update((sha384_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha384_final(hash_context * hctx, unsigned char *output)
{
	return sha384_final((sha384_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_SHA512
ATTRIBUTE_WARN_UNUSED_RET int _sha512_init(hash_context * hctx)
{
	return sha512_init((sha512_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha512_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return sha512_update((sha512_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha512_final(hash_context * hctx, unsigned char *output)
{
	return sha512_final((sha512_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_SHA512_224
ATTRIBUTE_WARN_UNUSED_RET int _sha512_224_init(hash_context * hctx)
{
	return sha512_224_init((sha512_224_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha512_224_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return sha512_224_update((sha512_224_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha512_224_final(hash_context * hctx, unsigned char *output)
{
	return sha512_224_final((sha512_224_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_SHA512_256
ATTRIBUTE_WARN_UNUSED_RET int _sha512_256_init(hash_context * hctx)
{
	return sha512_256_init((sha512_256_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha512_256_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return sha512_256_update((sha512_256_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha512_256_final(hash_context * hctx, unsigned char *output)
{
	return sha512_256_final((sha512_256_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_SHA3_224
ATTRIBUTE_WARN_UNUSED_RET int _sha3_224_init(hash_context * hctx)
{
	return sha3_224_init((sha3_224_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha3_224_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return sha3_224_update((sha3_224_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha3_224_final(hash_context * hctx, unsigned char *output)
{
	return sha3_224_final((sha3_224_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_SHA3_256
ATTRIBUTE_WARN_UNUSED_RET int _sha3_256_init(hash_context * hctx)
{
	return sha3_256_init((sha3_256_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha3_256_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return sha3_256_update((sha3_256_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha3_256_final(hash_context * hctx, unsigned char *output)
{
	return sha3_256_final((sha3_256_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_SHA3_384
ATTRIBUTE_WARN_UNUSED_RET int _sha3_384_init(hash_context * hctx)
{
	return sha3_384_init((sha3_384_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha3_384_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return sha3_384_update((sha3_384_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha3_384_final(hash_context * hctx, unsigned char *output)
{
	return sha3_384_final((sha3_384_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_SHA3_512
ATTRIBUTE_WARN_UNUSED_RET int _sha3_512_init(hash_context * hctx)
{
	return sha3_512_init((sha3_512_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha3_512_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return sha3_512_update((sha3_512_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _sha3_512_final(hash_context * hctx, unsigned char *output)
{
	return sha3_512_final((sha3_512_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_SM3
ATTRIBUTE_WARN_UNUSED_RET int _sm3_init(hash_context * hctx)
{
	return sm3_init((sm3_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _sm3_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return sm3_update((sm3_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _sm3_final(hash_context * hctx, unsigned char *output)
{
	return sm3_final((sm3_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_SHAKE256
ATTRIBUTE_WARN_UNUSED_RET int _shake256_init(hash_context * hctx)
{
	return shake256_init((shake256_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _shake256_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return shake256_update((shake256_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _shake256_final(hash_context * hctx, unsigned char *output)
{
	return shake256_final((shake256_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_STREEBOG256
ATTRIBUTE_WARN_UNUSED_RET int _streebog256_init(hash_context * hctx)
{
	return streebog256_init((streebog256_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _streebog256_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return streebog256_update((streebog256_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _streebog256_final(hash_context * hctx, unsigned char *output)
{
	return streebog256_final((streebog256_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_STREEBOG512
ATTRIBUTE_WARN_UNUSED_RET int _streebog512_init(hash_context * hctx)
{
	return streebog512_init((streebog512_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _streebog512_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return streebog512_update((streebog512_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _streebog512_final(hash_context * hctx, unsigned char *output)
{
	return streebog512_final((streebog512_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_RIPEMD160
ATTRIBUTE_WARN_UNUSED_RET int _ripemd160_init(hash_context * hctx)
{
	return ripemd160_init((ripemd160_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _ripemd160_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return ripemd160_update((ripemd160_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _ripemd160_final(hash_context * hctx, unsigned char *output)
{
	return ripemd160_final((ripemd160_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_BELT_HASH
ATTRIBUTE_WARN_UNUSED_RET int _belt_hash_init(hash_context * hctx)
{
	return belt_hash_init((belt_hash_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _belt_hash_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return belt_hash_update((belt_hash_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _belt_hash_final(hash_context * hctx, unsigned char *output)
{
	return belt_hash_final((belt_hash_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_BASH224
ATTRIBUTE_WARN_UNUSED_RET int _bash224_init(hash_context * hctx)
{
	return bash224_init((bash224_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _bash224_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return bash224_update((bash224_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _bash224_final(hash_context * hctx, unsigned char *output)
{
	return bash224_final((bash224_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_BASH256
ATTRIBUTE_WARN_UNUSED_RET int _bash256_init(hash_context * hctx)
{
	return bash256_init((bash256_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _bash256_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return bash256_update((bash256_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _bash256_final(hash_context * hctx, unsigned char *output)
{
	return bash256_final((bash256_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_BASH384
ATTRIBUTE_WARN_UNUSED_RET int _bash384_init(hash_context * hctx)
{
	return bash384_init((bash384_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _bash384_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return bash384_update((bash384_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _bash384_final(hash_context * hctx, unsigned char *output)
{
	return bash384_final((bash384_context*)hctx, output);
}
#endif
#ifdef WITH_HASH_BASH512
ATTRIBUTE_WARN_UNUSED_RET int _bash512_init(hash_context * hctx)
{
	return bash512_init((bash512_context*)hctx);
}
ATTRIBUTE_WARN_UNUSED_RET int _bash512_update(hash_context * hctx, const unsigned char *chunk, u32 chunklen)
{
	return bash512_update((bash512_context*)hctx, chunk, chunklen);
}
ATTRIBUTE_WARN_UNUSED_RET int _bash512_final(hash_context * hctx, unsigned char *output)
{
	return bash512_final((bash512_context*)hctx, output);
}
#endif
