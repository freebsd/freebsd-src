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
#if defined(WITH_SIG_EDDSA25519) || defined(WITH_SIG_EDDSA448)

/*
 * XXX: EdDSA is incompatible with small stack devices for now ...
 */
#if defined(USE_SMALL_STACK)
#error "Error: EDDSA25519 and EDDSA448 are incompatible with USE_SMALL_STACK (devices low on memory)"
#endif

/*
 * Sanity checks on the hash functions and curves depending on the EdDSA variant.
 */
/* EDDSA25519 used SHA-512 as a fixed hash function and WEI25519 as a fixed
 * curve.
 */
#if defined(WITH_SIG_EDDSA25519)
#if !defined(WITH_HASH_SHA512) || !defined(WITH_CURVE_WEI25519)
#error "Error: EDDSA25519 needs SHA-512 and WEI25519 to be defined! Please define them in libecc config file"
#endif
#endif
/* EDDSA448 used SHAKE256 as a fixed hash function and WEI448 as a fixed
 * curve.
 */
#if defined(WITH_SIG_EDDSA448)
#if !defined(WITH_HASH_SHAKE256) || !defined(WITH_CURVE_WEI448)
#error "Error: EDDSA25519 needs SHAKE256 and WEI448 to be defined! Please define them in libecc config file"
#endif
#endif

#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_logical.h>
#include <libecc/fp/fp.h>
#include <libecc/fp/fp_sqrt.h>
/* Include the "internal" header as we use non public API here */
#include "../nn/nn_div.h"


#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/sig_algs.h>
#include <libecc/sig/ec_key.h>
#include <libecc/utils/utils.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "EDDSA"
#endif
#include <libecc/utils/dbg_sig.h>


ATTRIBUTE_WARN_UNUSED_RET static inline int dom(u16 x, const u8 *y, u16 olen_y, const hash_mapping *h,
		      hash_context *h_ctx, u8 dom_type){
	u8 tmp[2];
	int ret;

	MUST_HAVE((h != NULL) && (h_ctx != NULL), ret, err);
	/* Sanity check on ancillary data len, its size must not exceed 255 bytes as per RFC8032 */
	MUST_HAVE((x <= 255) && (olen_y <= 255), ret, err);

	if(dom_type == 2){
		ret = h->hfunc_update(h_ctx, (const u8*)"SigEd25519 no Ed25519 collisions", 32); EG(ret, err);
	}
	else if(dom_type == 4){
		ret = h->hfunc_update(h_ctx, (const u8*)"SigEd448", 8); EG(ret, err);
	}
	else{
		ret = -1;
		goto err;
	}
	tmp[0] = (u8)x;
	tmp[1] = (u8)olen_y;
	ret = h->hfunc_update(h_ctx, tmp, 2); EG(ret, err);
	if(y != NULL){
		ret = h->hfunc_update(h_ctx, y, olen_y); EG(ret, err);
	}

err:
	return ret;
}

#if defined(WITH_SIG_EDDSA25519)
/* Helper for dom2(x, y).
 *
 * See RFC8032:
 *
 * dom2(x, y)     The blank octet string when signing or verifying
 *                Ed25519.  Otherwise, the octet string: "SigEd25519 no
 *                Ed25519 collisions" || octet(x) || octet(OLEN(y)) ||
 *                y, where x is in range 0-255 and y is an octet string
 *                of at most 255 octets.  "SigEd25519 no Ed25519
 *                collisions" is in ASCII (32 octets).
 */
ATTRIBUTE_WARN_UNUSED_RET static inline int dom2(u16 x, const u8 *y, u16 olen_y, const hash_mapping *h,
		       hash_context *h_ctx){
	return dom(x, y, olen_y, h, h_ctx, 2);
}
#endif /* defined(WITH_SIG_EDDSA25519) */

#if defined(WITH_SIG_EDDSA448)
/* Helper for dom4(x, y).
 *
 * See RFC8032:
 *
 * dom4(x, y)     The octet string "SigEd448" || octet(x) ||
 *                octet(OLEN(y)) || y, where x is in range 0-255 and y
 *                is an octet string of at most 255 octets.  "SigEd448"
 *                is in ASCII (8 octets).
 */
ATTRIBUTE_WARN_UNUSED_RET static inline int dom4(u16 x, const u8 *y, u16 olen_y, const hash_mapping *h,
		       hash_context *h_ctx){
	return dom(x, y, olen_y, h, h_ctx, 4);
}
#endif /* defined(WITH_SIG_EDDSA448) */

/* EdDSA sanity check on keys.
 * EDDSA25519 and variants only support WEI25519 as a curve, and SHA-512 as a hash function.
 * EDDSA448 and variants only support WEI448 as a curve, and SHAKE256 as a "hash function".
 */
ATTRIBUTE_WARN_UNUSED_RET static inline hash_alg_type get_eddsa_hash_type(ec_alg_type sig_type){
	hash_alg_type hash_type = UNKNOWN_HASH_ALG;

	switch (sig_type) {
#if defined(WITH_SIG_EDDSA25519)
		case EDDSA25519:
		case EDDSA25519PH:
		case EDDSA25519CTX:{
			hash_type = SHA512;
			break;
		}
#endif
#if defined(WITH_SIG_EDDSA448)
		case EDDSA448:
		case EDDSA448PH:{
			hash_type = SHAKE256;
			break;
		}
#endif
		default:{
			hash_type = UNKNOWN_HASH_ALG;
			break;
		}
	}
	return hash_type;
}

/*
 * Check given EdDSA key type does match given curve type. Returns 0 on success,
 * and -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static int eddsa_key_type_check_curve(ec_alg_type key_type,
				      ec_curve_type curve_type)
{
	int ret;

	switch (key_type) {
#if defined(WITH_SIG_EDDSA25519)
		case EDDSA25519:
		case EDDSA25519PH:
		case EDDSA25519CTX:{
			/* Check curve */
			ret = (curve_type == WEI25519) ? 0 : -1;
			break;
		}
#endif
#if defined(WITH_SIG_EDDSA448)
		case EDDSA448:
		case EDDSA448PH:{
			/* Check curve */
			ret = (curve_type == WEI448) ? 0 : -1;
			break;
		}
#endif
		default:{
			ret = -1;
			break;
		}
	}

	return ret;
}

ATTRIBUTE_WARN_UNUSED_RET static int eddsa_priv_key_sanity_check(const ec_priv_key *in_priv)
{
	int ret;

	ret = priv_key_check_initialized(in_priv); EG(ret, err);
	ret = eddsa_key_type_check_curve(in_priv->key_type,
			       in_priv->params->curve_type);

err:
	return ret;
}

ATTRIBUTE_WARN_UNUSED_RET static int eddsa_pub_key_sanity_check(const ec_pub_key *in_pub)
{
	int ret;


	ret = pub_key_check_initialized(in_pub); EG(ret, err);
	ret = eddsa_key_type_check_curve(in_pub->key_type,
			       in_pub->params->curve_type);

err:
	return ret;
}

ATTRIBUTE_WARN_UNUSED_RET static int eddsa_key_pair_sanity_check(const ec_key_pair *key_pair)
{
	int ret;

	MUST_HAVE((key_pair != NULL), ret, err);
	ret = eddsa_priv_key_sanity_check(&(key_pair->priv_key)); EG(ret, err);
	ret = eddsa_pub_key_sanity_check(&(key_pair->pub_key)); EG(ret, err);
	MUST_HAVE((key_pair->priv_key.key_type == key_pair->pub_key.key_type), ret, err);

err:
	return ret;
}

/*
 * EdDSA decode an integer from a buffer using little endian format.
 */
ATTRIBUTE_WARN_UNUSED_RET static int eddsa_decode_integer(nn_t nn_out, const u8 *buf, u16 buf_size)
{
	u16 i;
	u8 buf_little_endian[MAX_DIGEST_SIZE];
	int ret;

	MUST_HAVE((buf != NULL), ret, err);
	MUST_HAVE((sizeof(buf_little_endian) >= buf_size), ret, err);

	ret = nn_init(nn_out, 0); EG(ret, err);

	ret = local_memset(buf_little_endian, 0, sizeof(buf_little_endian)); EG(ret, err);
	if(buf_size > 1){
		/* Inverse endianness of our input buffer */
		for(i = 0; i < buf_size; i++){
			buf_little_endian[i] = buf[buf_size - 1 - i];
		}
	}

	/* Compute an integer from the buffer */
	ret = nn_init_from_buf(nn_out, buf_little_endian, buf_size);

err:
	return ret;
}

/*
 * EdDSA encode an integer to a buffer using little endian format.
 */
ATTRIBUTE_WARN_UNUSED_RET static int eddsa_encode_integer(nn_src_t nn_in, u8 *buf, u16 buf_size)
{
	u16 i;
	u8 tmp;
	int ret;
	bitcnt_t blen;

	MUST_HAVE((buf != NULL), ret, err);
	ret = nn_check_initialized(nn_in); EG(ret, err);

	/* Sanity check that we do not lose information */
	ret = nn_bitlen(nn_in, &blen); EG(ret, err);
	MUST_HAVE((((u32)blen) <= (8 * (u32)buf_size)), ret, err);

	/* Export the number to our buffer */
	ret = nn_export_to_buf(buf, buf_size, nn_in); EG(ret, err);

	/* Now reverse endianness in place */
	if(buf_size > 1){
		for(i = 0; i < (buf_size / 2); i++){
			tmp = buf[i];
			buf[i] = buf[buf_size - 1 - i];
			buf[buf_size - 1 - i] = tmp;
		}
	}

err:
	return ret;
}

/*
 * EdDSA encoding of scalar s.
 */
ATTRIBUTE_WARN_UNUSED_RET static int eddsa_compute_s(nn_t s, const u8 *digest, u16 digest_size)
{
	int ret;

	MUST_HAVE((digest != NULL), ret, err);
	MUST_HAVE(((digest_size % 2) == 0), ret, err);

	/* s is half of the digest size encoded in little endian format */
	ret = eddsa_decode_integer(s, digest, (digest_size / 2)); EG(ret, err);

err:
	return ret;
}

/* Extract the digest from the encoded private key */
ATTRIBUTE_WARN_UNUSED_RET static int eddsa_get_digest_from_priv_key(u8 *digest, u8 *digest_size, const ec_priv_key *in_priv)
{
	int ret;
	hash_alg_type hash_type;
	const hash_mapping *hash;

	MUST_HAVE(((digest != NULL) && (digest_size != NULL)), ret, err);
	ret = eddsa_priv_key_sanity_check(in_priv); EG(ret, err);

	MUST_HAVE(((hash_type = get_eddsa_hash_type(in_priv->key_type)) != UNKNOWN_HASH_ALG), ret, err);
	ret = get_hash_by_type(hash_type, &hash); EG(ret, err);
	MUST_HAVE((hash != NULL), ret, err);

	/* Check real digest size */
	MUST_HAVE(((*digest_size) >= hash->digest_size), ret, err);

	(*digest_size) = hash->digest_size;
	ret = nn_export_to_buf(digest, *digest_size, &(in_priv->x));

err:
	return ret;
}

/* Encode an Edwards curve affine point in canonical form */
ATTRIBUTE_WARN_UNUSED_RET static int eddsa_encode_point(aff_pt_edwards_src_t in,
							fp_src_t alpha_edwards,
							u8 *buf, u16 buflen,
							ec_alg_type sig_alg)
{
	nn out_reduced;
	u8 lsb = 0;
	int ret;
	out_reduced.magic = WORD(0);

	/* Sanity checks */
	MUST_HAVE((buf != NULL), ret, err);
	ret = aff_pt_edwards_check_initialized(in); EG(ret, err);
	ret = fp_check_initialized(alpha_edwards); EG(ret, err);

	/* Zeroize the buffer */
	ret = local_memset(buf, 0, buflen); EG(ret, err);
	ret = nn_init(&out_reduced, 0); EG(ret, err);

	/* Note: we should be reduced modulo Fp for canonical encoding here as
	 * coordinate elements are in Fp ...
	 */
#if defined(WITH_SIG_EDDSA448)
	if((sig_alg == EDDSA448) || (sig_alg == EDDSA448PH)){
		/*
		 * In case of EDDSA448, we apply the 4-isogeny to transfer from
		 * Ed448 to Edwards448.
		 * The isogeny maps (x, y) on Ed448 to (x1, y1) on Edwards448
		 * using:
		 *      x1 = (4*x*y/c) / (y^2-x^2)
		 *      y1 = (2-x^2-y^2) / (x^2+y^2) = (2 - (x^2+y^2)) / (x^2 + y^2)
		 * and (0, 1) as well as (0, -1) are mapped to (0, 1)
		 * We only need to encode our y1 here, but x1 computation is
		 * unfortunately needed to get its LSB that is necessary for
		 * the encoding.
		 */
		fp tmp_x, tmp_y, y1;
		tmp_x.magic = tmp_y.magic = y1.magic = WORD(0);
		/* Compute x1 to get our LSB */
		ret = fp_init(&y1, in->y.ctx); EG(ret, err1);
		ret = fp_copy(&tmp_x, &(in->x)); EG(ret, err1);
		ret = fp_sqr(&tmp_x, &tmp_x); EG(ret, err1);
		ret = fp_copy(&tmp_y, &(in->y)); EG(ret, err1);
		ret = fp_sqr(&tmp_y, &tmp_y); EG(ret, err1);
		ret = fp_sub(&tmp_y, &tmp_y, &tmp_x); EG(ret, err1);
		/* NOTE: inversion by zero should be caught by lower layers */
		ret = fp_inv(&tmp_y, &tmp_y); EG(ret, err1);
		ret = fp_set_word_value(&tmp_x, WORD(4)); EG(ret, err1);
		ret = fp_mul(&tmp_x, &tmp_x, &(in->x)); EG(ret, err1);
		ret = fp_mul(&tmp_x, &tmp_x, &(in->y)); EG(ret, err1);
		ret = fp_mul(&tmp_x, &tmp_x, &tmp_y); EG(ret, err1);
		ret = fp_inv(&tmp_y, alpha_edwards); EG(ret, err1);
		ret = fp_mul(&tmp_x, &tmp_x, &tmp_y); EG(ret, err1);
		ret = nn_getbit(&(tmp_x.fp_val), 0, &lsb); EG(ret, err1);
		/* Compute y1 */
		ret = fp_copy(&tmp_x, &(in->x)); EG(ret, err1);
		ret = fp_sqr(&tmp_x, &tmp_x); EG(ret, err1);
		ret = fp_copy(&tmp_y, &(in->y)); EG(ret, err1);
		ret = fp_sqr(&tmp_y, &tmp_y); EG(ret, err1);
		ret = fp_set_word_value(&y1, WORD(2)); EG(ret, err1);
		ret = fp_sub(&y1, &y1, &tmp_x); EG(ret, err1);
		ret = fp_sub(&y1, &y1, &tmp_y); EG(ret, err1);
		ret = fp_add(&tmp_x, &tmp_x, &tmp_y); EG(ret, err1);
		/* NOTE: inversion by zero should be caught by lower layers */
		ret = fp_inv(&tmp_x, &tmp_x); EG(ret, err1);
		ret = fp_mul(&y1, &y1, &tmp_x); EG(ret, err1);
		ret = eddsa_encode_integer(&(y1.fp_val), buf, buflen);
err1:
		fp_uninit(&tmp_x);
		fp_uninit(&tmp_y);
		fp_uninit(&y1);
		EG(ret, err);
	}
	else
#endif /* !defined(WITH_SIG_EDDSA448) */
	{	/* EDDSA25519 and other cases */
		FORCE_USED_VAR(sig_alg); /* To avoid unused variable error */
		ret = nn_getbit(&(in->x.fp_val), 0, &lsb); EG(ret, err);
		ret = eddsa_encode_integer(&(in->y.fp_val), buf, buflen); EG(ret, err);
	}
	/*
	 * Now deal with the sign for the last bit: copy the least significant
	 * bit of the x coordinate in the MSB of the last octet.
	 */
	MUST_HAVE((buflen > 1), ret, err);
	buf[buflen - 1] |= (u8)(lsb << 7);

err:
	nn_uninit(&out_reduced);

	return ret;
}

/* Decode an Edwards curve affine point from canonical form */
ATTRIBUTE_WARN_UNUSED_RET static int eddsa_decode_point(aff_pt_edwards_t out, ec_edwards_crv_src_t edwards_curve,
			      fp_src_t alpha_edwards, const u8 *buf, u16 buflen,
			      ec_alg_type sig_type)
{
	fp x, y;
	fp sqrt1, sqrt2;
	u8 x_0, lsb;
	u8 buf_little_endian[MAX_DIGEST_SIZE];
	u16 i;
	int ret, iszero;

#if defined(WITH_SIG_EDDSA448)
        fp tmp;
        tmp.magic = WORD(0);
#endif
	x.magic = y.magic = sqrt1.magic = sqrt2.magic = WORD(0);

	MUST_HAVE((buf != NULL), ret, err);

	ret = ec_edwards_crv_check_initialized(edwards_curve); EG(ret, err);

	ret = fp_check_initialized(alpha_edwards); EG(ret, err);

	/* Extract the sign */
	x_0 = ((buf[buflen - 1] & 0x80) >> 7);
	/* Extract the value by reversing endianness */
	MUST_HAVE((sizeof(buf_little_endian) >= buflen), ret, err);

	/* Inverse endianness of our input buffer and mask the sign bit */
	MUST_HAVE((buflen > 1), ret, err);

	for(i = 0; i < buflen; i++){
		buf_little_endian[i] = buf[buflen - 1 - i];
		if(i == 0){
			/* Mask the sign bit */
			buf_little_endian[i] &= 0x7f;
		}
	}
	/* Try to decode the y coordinate */
	ret = fp_init_from_buf(&y, edwards_curve->a.ctx, buf_little_endian, buflen); EG(ret, err);
	/*
	 * If we suceed, try to find our x coordinate that is the square root of
	 * (y^2 - 1) / (d y^2 + 1) or (y^2 - 1) / (d y^2 - 1) depending on the
	 * algorithm (EDDSA25519 our EDDSA448).
	 */
        ret = fp_init(&sqrt1, edwards_curve->a.ctx); EG(ret, err);
        ret = fp_init(&sqrt2, edwards_curve->a.ctx); EG(ret, err);
        ret = fp_init(&x, edwards_curve->a.ctx); EG(ret, err);
#if defined(WITH_SIG_EDDSA448)
	if((sig_type == EDDSA448) || (sig_type == EDDSA448PH)){
		const u8 d_edwards448_buff[] = {
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
			0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x67, 0x56
		};
		ec_edwards_crv edwards_curve_edwards448;

		ret = fp_init(&tmp, edwards_curve->a.ctx); EG(ret, err);
		/*
		 * If we deal with EDDSA448 we must handle the point on
		 * Edwards448 so we use the dedicated d.
		 */
		ret = fp_init_from_buf(&tmp, edwards_curve->a.ctx,
				(const u8*)d_edwards448_buff, sizeof(d_edwards448_buff)); EG(ret, err);
		ret = ec_edwards_crv_init(&edwards_curve_edwards448, &(edwards_curve->a), &tmp, &(edwards_curve->order)); EG(ret, err);
		/* Compute x from y using the dedicated primitive */
		ret = aff_pt_edwards_x_from_y(&sqrt1, &sqrt2, &y, &edwards_curve_edwards448); EG(ret, err); /* Error or no square root found, this should not happen! */
		ec_edwards_crv_uninit(&edwards_curve_edwards448);
	}
	else
#endif
	{
		/* Compute x from y using the dedicated primitive */
		ret = aff_pt_edwards_x_from_y(&sqrt1, &sqrt2, &y, edwards_curve); EG(ret, err); /* Error or no square root found, this should not happen! */
	}
	/* Now select the square root of the proper sign */
	ret = nn_getbit(&(sqrt1.fp_val), 0, &lsb); EG(ret, err);
	if(lsb == x_0){
		ret = fp_copy(&x, &sqrt1); EG(ret, err);
	}
	else{
		ret = fp_copy(&x, &sqrt2); EG(ret, err);
	}
	/* If x = 0 and the sign bit is 1, this is an error */
	ret = fp_iszero(&x, &iszero); EG(ret, err);
	MUST_HAVE(!(iszero && (x_0 == 1)), ret, err);
	/*
	 * In case of EDDSA448, we apply the 4-isogeny to transfer from
	 * Edwards448 to Ed448.
	 * The isogeny maps (x1, y1) on Edwards448 to (x, y) on Ed448 using:
	 *	x = alpha_edwards * (x1*y1) / (2-x1^2-y1^2)
	 *      y = (x1^2+y1^2) / (y1^2-x1^2)
	 */
#if defined(WITH_SIG_EDDSA448)
	if((sig_type == EDDSA448) || (sig_type == EDDSA448PH)){
		/*
		 * Use sqrt1 and sqrt2 as temporary buffers for x and y, and
		 * tmp as scratch pad buffer
		 */
		ret = fp_copy(&sqrt1, &x); EG(ret, err);
		ret = fp_copy(&sqrt2, &y); EG(ret, err);

		ret = fp_set_word_value(&x, WORD(2)); EG(ret, err);
		ret = fp_sqr(&tmp, &sqrt1); EG(ret, err);
		ret = fp_sub(&x, &x, &tmp); EG(ret, err);
		ret = fp_sqr(&tmp, &sqrt2); EG(ret, err);
		ret = fp_sub(&x, &x, &tmp); EG(ret, err);
		/* NOTE: inversion by zero should be caught by lower layers */
		ret = fp_inv(&x, &x); EG(ret, err);
		ret = fp_mul(&x, &x, &sqrt1); EG(ret, err);
		ret = fp_mul(&x, &x, &sqrt2); EG(ret, err);
		ret = fp_mul(&x, &x, alpha_edwards); EG(ret, err);

		ret = fp_sqr(&sqrt1, &sqrt1); EG(ret, err);
		ret = fp_sqr(&sqrt2, &sqrt2); EG(ret, err);
		ret = fp_sub(&y, &sqrt2, &sqrt1); EG(ret, err);
		/* NOTE: inversion by zero should be caught by lower layers */
		ret = fp_inv(&y, &y); EG(ret, err);
		ret = fp_add(&sqrt1, &sqrt1, &sqrt2); EG(ret, err);
		ret = fp_mul(&y, &y, &sqrt1); EG(ret, err);
	}
#endif /* !defined(WITH_SIG_EDDSA448) */

	/* Initialize our point */
	ret = aff_pt_edwards_init_from_coords(out, edwards_curve, &x, &y);

err:
	fp_uninit(&sqrt1);
	fp_uninit(&sqrt2);
	fp_uninit(&x);
	fp_uninit(&y);
#if defined(WITH_SIG_EDDSA448)
	fp_uninit(&tmp);
#endif

	return ret;
}


/*
 * Derive hash from private key.
 */
ATTRIBUTE_WARN_UNUSED_RET static int eddsa_derive_priv_key_hash(const ec_priv_key *in_priv,
								u8 *buf, u16 buflen)
{
	hash_alg_type hash_type = UNKNOWN_HASH_ALG;
	const hash_mapping *hash;
	u8 x_buf[EC_PRIV_KEY_MAX_SIZE];
	int ret;
	const u8 *in[2];
	u32 in_len[2];

	MUST_HAVE((buf != NULL), ret, err);
	ret = eddsa_priv_key_sanity_check(in_priv); EG(ret, err);

	MUST_HAVE(((hash_type = get_eddsa_hash_type(in_priv->key_type)) != UNKNOWN_HASH_ALG), ret, err);
	ret = get_hash_by_type(hash_type, &hash); EG(ret, err);
	MUST_HAVE((hash != NULL), ret, err);

	/* Get the private key as a buffer and hash it */
	ret = local_memset(x_buf, 0, sizeof(x_buf)); EG(ret, err);
	MUST_HAVE((sizeof(x_buf) >= (hash->digest_size / 2)), ret, err);

	ret = ec_priv_key_export_to_buf(in_priv, x_buf, (hash->digest_size / 2)); EG(ret, err);

	ret = hash_mapping_callbacks_sanity_check(hash); EG(ret, err);

	MUST_HAVE((buflen >= hash->digest_size), ret, err);

	in[0] = x_buf; in[1] = NULL;
	in_len[0] = (hash->digest_size / 2); in_len[1] = 0;
	ret = hash->hfunc_scattered(in, in_len, buf);

err:
	PTR_NULLIFY(hash);

	return ret;
}

/*
 * Derive an EdDSA private key.
 *
 */
int eddsa_derive_priv_key(ec_priv_key *priv_key)
{
	int ret, cmp;
	u8 digest_size;
	u8 digest[MAX_DIGEST_SIZE];
	hash_alg_type hash_type = UNKNOWN_HASH_ALG;
	word_t cofactor = WORD(0);

	/* Check if private key is initialized. */
	ret = eddsa_priv_key_sanity_check(priv_key); EG(ret, err);

	/* Check hash function compatibility:
	 *   We must have 2**(b-1) > p with (2*b) the size of the hash function.
	 */
	MUST_HAVE(((hash_type = get_eddsa_hash_type(priv_key->key_type)) != UNKNOWN_HASH_ALG), ret, err);

	digest_size = 0;
	ret = get_hash_sizes(hash_type, &digest_size, NULL); EG(ret, err);

	MUST_HAVE(((2 * priv_key->params->ec_fp.p_bitlen) < (8 * (bitcnt_t)digest_size)), ret, err);
	MUST_HAVE(((digest_size % 2) == 0), ret, err);
	MUST_HAVE((digest_size <= sizeof(digest)), ret, err);

	/*
	 * Now that we have our private scalar, derive the hash value of secret
	 * key
	 */
	/* Hash the private key */
	ret = eddsa_derive_priv_key_hash(priv_key, digest, digest_size); EG(ret, err);

	/* Get the cofactor as an integer */
	cofactor = priv_key->params->ec_gen_cofactor.val[0];
	ret = nn_cmp_word(&(priv_key->params->ec_gen_cofactor), cofactor, &cmp); EG(ret, err);
	MUST_HAVE((cmp == 0), ret, err);
	/* Cofactor must be 2**2 or 2**3 as per RFC8032 standard */
	MUST_HAVE((cofactor == (0x1 << 2)) || (cofactor == (0x1 << 3)), ret, err);

	/* Now clear the low bits related to cofactor */
	digest[0] &= (u8)(~(cofactor - 1));
#if defined(WITH_SIG_EDDSA25519)
	if ((priv_key->key_type == EDDSA25519) ||
	    (priv_key->key_type == EDDSA25519CTX) ||
	    (priv_key->key_type == EDDSA25519PH)){
		/*
		 * MSB of highest octet of half must be cleared, second MSB must
		 * be set
		 */
		digest[(digest_size / 2) - 1] &= 0x7f;
		digest[(digest_size / 2) - 1] |= 0x40;
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if ((priv_key->key_type == EDDSA448) || (priv_key->key_type == EDDSA448PH)) {
		MUST_HAVE((digest_size / 2) >= 2, ret, err);
		/*
		 * All eight bits of the last octet are cleared, highest bit
		 * of the second to last octet is set.
		 */
		digest[(digest_size / 2) - 1] = 0;
		digest[(digest_size / 2) - 2] |= 0x80;
	}
#endif
#if !defined(WITH_SIG_EDDSA25519) && !defined(WITH_SIG_EDDSA448)
	ret = -1;
	goto err;
#endif
	/*
	 * Now that we have derived our hash, store it in place of our secret
	 * value NOTE: we do not need the secret value anymore since only the
	 * hash is needed.
	 */
	ret = nn_init_from_buf(&(priv_key->x), digest, digest_size);

err:
	VAR_ZEROIFY(digest_size);

	return ret;
}

/*
 * Generate an EdDSA private key.
 *
 */
int eddsa_gen_priv_key(ec_priv_key *priv_key)
{
	int ret;
	u8 digest_size;
	hash_alg_type hash_type = UNKNOWN_HASH_ALG;

	/* Check if private key is initialized. */
	ret = eddsa_priv_key_sanity_check(priv_key); EG(ret, err);

	/* Check hash function compatibility:
	 *   We must have 2**(b-1) > p with (2*b) the size of the hash function.
	 */
	MUST_HAVE(((hash_type = get_eddsa_hash_type(priv_key->key_type)) != UNKNOWN_HASH_ALG), ret, err);

	digest_size = 0;
	ret = get_hash_sizes(hash_type, &digest_size, NULL); EG(ret, err);

	MUST_HAVE(((2 * priv_key->params->ec_fp.p_bitlen) < (8 * (bitcnt_t)digest_size)), ret, err);
	MUST_HAVE(((digest_size % 2) == 0), ret, err);

	/* Generate a random private key
	 * An EdDSA secret scalar is a b bit string with (2*b) the size of the hash function
	 */
	ret = nn_get_random_len(&(priv_key->x), (digest_size / 2)); EG(ret, err);

	/* Derive the private key */
	ret = eddsa_derive_priv_key(priv_key);

err:
	VAR_ZEROIFY(digest_size);

	return ret;
}


/* Import an EdDSA private key from a raw buffer.
 * NOTE: the private key must be a big number associated to the curve that
 * depends on the flavor of EdDSA (Ed25519 or Ed448), and the result is a
 * derived private key that can be used by the internal EdDSA functions.
 * The derived key is a hash of the private key: we mainly perform this
 * derivation early to prevent side-channel attacks and other leaks on the
 * "root" private key.
 */
int eddsa_import_priv_key(ec_priv_key *priv_key, const u8 *buf, u16 buflen,
			  const ec_params *shortw_curve_params,
			  ec_alg_type sig_type)
{
	int ret;
	hash_alg_type hash_type = UNKNOWN_HASH_ALG;
	u8 digest_size;
	bitcnt_t blen;

	/* Some sanity checks */
	MUST_HAVE((priv_key != NULL) && (buf != NULL) && (shortw_curve_params != NULL), ret, err);

	/* Import the big number from our buffer */
	ret = nn_init_from_buf(&(priv_key->x), buf, buflen); EG(ret, err);
	/* The bit length of our big number must be <= b, half the digest size */
	hash_type = get_eddsa_hash_type(sig_type);
	MUST_HAVE((hash_type != UNKNOWN_HASH_ALG), ret, err);

	digest_size = 0;
	ret = get_hash_sizes(hash_type, &digest_size, NULL); EG(ret, err);

	ret = nn_bitlen(&(priv_key->x), &blen); EG(ret, err);
	MUST_HAVE((blen <= (8 * ((bitcnt_t)digest_size / 2))), ret, err);

	/* Initialize stuff */
	priv_key->key_type = sig_type;
	priv_key->params = shortw_curve_params;
	priv_key->magic = PRIV_KEY_MAGIC;

	/* Now derive the private key.
	 * NOTE: sanity check on the private key is performed during derivation.
	 */
	ret = eddsa_derive_priv_key(priv_key);

err:
	if((priv_key != NULL) && ret){
		IGNORE_RET_VAL(local_memset(priv_key, 0, sizeof(ec_priv_key)));
	}
	VAR_ZEROIFY(digest_size);
	VAR_ZEROIFY(blen);

	return ret;
}

/* NOTE: we perform EDDSA public key computation on the short Weierstrass
 * form of the curve thanks to the birational equivalence of curve
 * models (the isomorphism allows to perform the scalar multiplication
 * on the equivalent curve).
 */
int eddsa_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv)
{
	prj_pt_src_t G;
	u8 digest_size;
	u8 digest[MAX_DIGEST_SIZE];
	/* Secret scalar used for public generation */
	nn s;
	hash_alg_type hash_type;
	u8 digest_size_;
	int ret;
	s.magic = WORD(0);

	MUST_HAVE(out_pub != NULL, ret, err);
	ret = eddsa_priv_key_sanity_check(in_priv); EG(ret, err);

	ret = nn_init(&s, 0); EG(ret, err);

	/* Zero init public key to be generated */
	ret = local_memset(out_pub, 0, sizeof(ec_pub_key)); EG(ret, err);

	/* Get the generator G */
	G = &(in_priv->params->ec_gen);

	/* Get the digest in proper format */
	MUST_HAVE(((hash_type = get_eddsa_hash_type(in_priv->key_type)) != UNKNOWN_HASH_ALG), ret, err);

	digest_size_ = 0;
	ret = get_hash_sizes(hash_type, &digest_size_, NULL); EG(ret, err);

	/* Extract the digest */
	digest_size = sizeof(digest);
	ret = eddsa_get_digest_from_priv_key(digest, &digest_size, in_priv); EG(ret, err);

	/* Sanity check */
	MUST_HAVE((digest_size == digest_size_), ret, err);

	/* Encode the scalar s from the digest */
	ret = eddsa_compute_s(&s, digest, digest_size); EG(ret, err);

	/* Compute s x G where G is the base point */
	/*
	 * Use blinding when computing point scalar multiplication as we
	 * manipulate a fixed secret.
	 */
#if defined(WITH_SIG_EDDSA448)
	if((in_priv->key_type == EDDSA448) || (in_priv->key_type == EDDSA448PH)){
		/*
		 * NOTE: because of the 4-isogeny between Ed448 and Edwards448,
		 * we actually multiply by (s/4) since the base point of
		 * Edwards448 is four times the one of Ed448.
		 * Here, s/4 can be simply computed by right shifting by 2 as
		 * we are ensured that our scalar is a multiple of 4 by
		 * construction.
		 */
		ret = nn_rshift(&s, &s, 2); EG(ret, err);
	}
#endif
	ret = prj_pt_mul_blind(&(out_pub->y), &s, G); EG(ret, err);

	out_pub->key_type = in_priv->key_type;
	out_pub->params = in_priv->params;
	out_pub->magic = PUB_KEY_MAGIC;

err:
	PTR_NULLIFY(G);
	VAR_ZEROIFY(digest_size);
	nn_uninit(&s);

	return ret;
}

/*
 * Import a public key in canonical form.
 * (imports a public key from a buffer and checks its canonical form.)
 *
 */
int eddsa_import_pub_key(ec_pub_key *pub_key, const u8 *buf, u16 buflen,
			 const ec_params *shortw_curve_params,
			 ec_alg_type sig_type)
{
	aff_pt_edwards _Tmp;
	ec_edwards_crv edwards_curve;
	int ret;
	ec_shortw_crv_src_t shortw_curve;
	fp_src_t alpha_montgomery;
	fp_src_t gamma_montgomery;
	fp_src_t alpha_edwards;
	prj_pt_t pub_key_y;
	_Tmp.magic = edwards_curve.magic = WORD(0);

#if defined(WITH_SIG_EDDSA25519) && defined(WITH_SIG_EDDSA448)
	if((sig_type != EDDSA25519) && (sig_type != EDDSA25519CTX) && (sig_type != EDDSA25519PH) && \
	   (sig_type != EDDSA448) && (sig_type != EDDSA448PH)){
#endif
#if defined(WITH_SIG_EDDSA25519) && !defined(WITH_SIG_EDDSA448)
	if((sig_type != EDDSA25519) && (sig_type != EDDSA25519CTX) && (sig_type != EDDSA25519PH)){
#endif
#if !defined(WITH_SIG_EDDSA25519) && defined(WITH_SIG_EDDSA448)
	if((sig_type != EDDSA448) && (sig_type != EDDSA448PH)){
#endif
		ret = -1;
		goto err;
	}

	MUST_HAVE((pub_key != NULL) && (shortw_curve_params != NULL) && (buf != NULL), ret, err);

	/* Check for sizes on the buffer for strict size depending on EdDSA types */
#if defined(WITH_SIG_EDDSA25519)
	if((sig_type == EDDSA25519) || (sig_type == EDDSA25519CTX) || (sig_type == EDDSA25519PH)){
		MUST_HAVE((buflen == EDDSA25519_PUB_KEY_ENCODED_LEN), ret, err);
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if((sig_type == EDDSA448) || (sig_type == EDDSA448PH)){
		MUST_HAVE((buflen == EDDSA448_PUB_KEY_ENCODED_LEN), ret, err);
	}
#endif

	/* Make things more readable */
	shortw_curve = &(shortw_curve_params->ec_curve);
	alpha_montgomery = &(shortw_curve_params->ec_alpha_montgomery);
	gamma_montgomery = &(shortw_curve_params->ec_gamma_montgomery);
	alpha_edwards = &(shortw_curve_params->ec_alpha_edwards);
	pub_key_y = &(pub_key->y);

	/* Get the isogenic Edwards curve */
	ret = curve_shortw_to_edwards(shortw_curve, &edwards_curve, alpha_montgomery,
				gamma_montgomery, alpha_edwards); EG(ret, err);

	/* Decode the point in Edwards */
	ret = eddsa_decode_point(&_Tmp, &edwards_curve, alpha_edwards, buf, buflen,
			      sig_type); EG(ret, err);
	/* Then transfer to short Weierstrass in our public key */
	ret = aff_pt_edwards_to_prj_pt_shortw(&_Tmp, shortw_curve, pub_key_y,
					alpha_edwards); EG(ret, err);
#if defined(WITH_SIG_EDDSA448)
	if((sig_type == EDDSA448) || (sig_type == EDDSA448PH)){
		nn_src_t gen_order = &(shortw_curve_params->ec_gen_order);
		nn tmp;
		tmp.magic = WORD(0);

		/*
		 * NOTE: because of the 4-isogeny between Ed448 and Edwards448,
		 * we actually multiply by (s/4) since the base point of
		 * Edwards448 is four times the one of Ed448.
		 * Here, s/4 is computed by multiplying s by the modular
		 * inverse of 4.
		 */
		ret = nn_init(&tmp, 0); EG(ret, err1);
		ret = nn_modinv_word(&tmp, WORD(4), gen_order); EG(ret, err1);
		ret = prj_pt_mul(&(pub_key->y), &tmp, pub_key_y); EG(ret, err1);
err1:
		nn_uninit(&tmp);
		PTR_NULLIFY(gen_order);
		EG(ret, err);
	}
#endif
	/* Mark the public key as initialized */
	pub_key->key_type = sig_type;
	pub_key->params = shortw_curve_params;
	pub_key->magic = PUB_KEY_MAGIC;

	/* Now sanity check our public key before validating the import */
	ret = eddsa_pub_key_sanity_check(pub_key);

err:
	if((pub_key != NULL) && ret){
		IGNORE_RET_VAL(local_memset(pub_key, 0, sizeof(ec_pub_key)));
	}
	PTR_NULLIFY(shortw_curve);
	PTR_NULLIFY(alpha_montgomery);
	PTR_NULLIFY(gamma_montgomery);
	PTR_NULLIFY(alpha_edwards);
	PTR_NULLIFY(pub_key_y);
	aff_pt_edwards_uninit(&_Tmp);
	ec_edwards_crv_uninit(&edwards_curve);

	return ret;
}

/*
 * Export a public key in canonical form.
 * (exports a public key to a buffer in canonical form.)
 */
int eddsa_export_pub_key(const ec_pub_key *in_pub, u8 *buf, u16 buflen)
{
	aff_pt_edwards _Tmp;
	ec_edwards_crv edwards_curve;
	ec_alg_type sig_type;
	int ret;
	ec_shortw_crv_src_t shortw_curve;
	fp_src_t alpha_montgomery;
	fp_src_t gamma_montgomery;
	fp_src_t alpha_edwards;
	prj_pt_src_t pub_key_y;
	_Tmp.magic = edwards_curve.magic = WORD(0);

	ret = pub_key_check_initialized(in_pub); EG(ret, err);
	MUST_HAVE((buf != NULL), ret, err);

	/* Make things more readable */
	shortw_curve = &(in_pub->params->ec_curve);
	alpha_montgomery = &(in_pub->params->ec_alpha_montgomery);
	gamma_montgomery = &(in_pub->params->ec_gamma_montgomery);
	alpha_edwards = &(in_pub->params->ec_alpha_edwards);
	pub_key_y = &(in_pub->y);
	sig_type = in_pub->key_type;

	/* Check for sizes on the buffer for strict size depending on EdDSA types */
#if defined(WITH_SIG_EDDSA25519)
	if((sig_type == EDDSA25519) || (sig_type == EDDSA25519CTX) || (sig_type == EDDSA25519PH)){
		MUST_HAVE((buflen == EDDSA25519_PUB_KEY_ENCODED_LEN), ret, err);
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if((sig_type == EDDSA448) || (sig_type == EDDSA448PH)){
		MUST_HAVE((buflen == EDDSA448_PUB_KEY_ENCODED_LEN), ret, err);
	}
#endif

	/* Transfer our short Weierstrass to Edwards representation */
	ret = curve_shortw_to_edwards(shortw_curve, &edwards_curve, alpha_montgomery,
				gamma_montgomery, alpha_edwards); EG(ret, err);
	ret = prj_pt_shortw_to_aff_pt_edwards(pub_key_y, &edwards_curve, &_Tmp,
					alpha_edwards); EG(ret, err);
	/* Export to buffer canonical form */
	ret = eddsa_encode_point(&_Tmp, alpha_edwards, buf,
			      buflen, in_pub->key_type);

err:
	PTR_NULLIFY(shortw_curve);
	PTR_NULLIFY(alpha_montgomery);
	PTR_NULLIFY(gamma_montgomery);
	PTR_NULLIFY(alpha_edwards);
	PTR_NULLIFY(pub_key_y);
	aff_pt_edwards_uninit(&_Tmp);
	ec_edwards_crv_uninit(&edwards_curve);

	return ret;
}

/* Import an EdDSA key pair from a private key buffer */
int eddsa_import_key_pair_from_priv_key_buf(ec_key_pair *kp,
					    const u8 *buf, u16 buflen,
					    const ec_params *shortw_curve_params,
					    ec_alg_type sig_type)
{
	int ret;

	MUST_HAVE((kp != NULL), ret, err);

	/* Try to import the private key */
	ret = eddsa_import_priv_key(&(kp->priv_key), buf, buflen,
				 shortw_curve_params, sig_type); EG(ret, err);

	/* Now derive the public key */
	ret = eddsa_init_pub_key(&(kp->pub_key), &(kp->priv_key));

err:
	return ret;
}

/* Compute PH(M) with PH being the hash depending on the key type */
ATTRIBUTE_WARN_UNUSED_RET static int eddsa_compute_pre_hash(const u8 *message, u32 message_size,
				  u8 *digest, u8 *digest_size,
				  ec_alg_type sig_type)
{
	hash_alg_type hash_type;
	const hash_mapping *hash;
	hash_context hash_ctx;
	int ret;

	MUST_HAVE((message != NULL) && (digest != NULL) && (digest_size != NULL), ret, err);

	MUST_HAVE(((hash_type = get_eddsa_hash_type(sig_type)) != UNKNOWN_HASH_ALG), ret, err);

	ret = get_hash_by_type(hash_type, &hash); EG(ret, err);
	MUST_HAVE((hash != NULL), ret, err);

	/* Sanity check on the size */
	MUST_HAVE(((*digest_size) >= hash->digest_size), ret, err);

	(*digest_size) = hash->digest_size;
	/* Hash the message */
	ret = hash_mapping_callbacks_sanity_check(hash); EG(ret, err);
	ret = hash->hfunc_init(&hash_ctx); EG(ret, err);
	ret = hash->hfunc_update(&hash_ctx, message, message_size); EG(ret, err);
	ret = hash->hfunc_finalize(&hash_ctx, digest); EG(ret, err);

err:
	return ret;
}

/*****************/

/* EdDSA signature length */
int eddsa_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen)
{
	int ret;

	MUST_HAVE((siglen != NULL), ret, err);
	MUST_HAVE((p_bit_len <= CURVES_MAX_P_BIT_LEN) &&
		  (q_bit_len <= CURVES_MAX_Q_BIT_LEN) &&
		  (hsize <= MAX_DIGEST_SIZE) && (blocksize <= MAX_BLOCK_SIZE), ret, err);

	(*siglen) = (u8)EDDSA_SIGLEN(hsize);
	ret = 0;
err:
	return ret;
}

/*
 * Generic *internal* EdDSA signature functions (init, update and finalize).
 *
 * Global EdDSA signature process is as follows (I,U,F provides
 * information in which function(s) (init(), update() or finalize())
 * a specific step is performed):
 *
 */

#define EDDSA_SIGN_MAGIC ((word_t)(0x7632542bf630972bULL))
#define EDDSA_SIGN_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == EDDSA_SIGN_MAGIC), ret, err)

int _eddsa_sign_init_pre_hash(struct ec_sign_context *ctx)
{
	int ret;
	bitcnt_t blen;
	u8 use_message_pre_hash = 0;
	ec_alg_type key_type = UNKNOWN_ALG;
	const ec_key_pair *key_pair;
	const hash_mapping *h;

	/* First, verify context has been initialized */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);

	/* Make things more readable */
	key_pair = ctx->key_pair;
	h = ctx->h;
	key_type = ctx->key_pair->priv_key.key_type;

	/* Sanity check: this function is only supported in PH mode */
#if defined(WITH_SIG_EDDSA25519)
	if(key_type == EDDSA25519PH){
		use_message_pre_hash = 1;
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if(key_type == EDDSA448PH){
		use_message_pre_hash = 1;
	}
#endif
	MUST_HAVE((use_message_pre_hash == 1), ret, err);

	/* Additional sanity checks on input params from context */
	ret = eddsa_key_pair_sanity_check(key_pair); EG(ret, err);

	MUST_HAVE((h != NULL) && (h->digest_size <= MAX_DIGEST_SIZE) && (h->block_size <= MAX_BLOCK_SIZE), ret, err);

	/* Sanity check on hash types */
	MUST_HAVE((key_type == key_pair->pub_key.key_type) && (h->type == get_eddsa_hash_type(key_type)), ret, err);

	/*
	 * Sanity check on hash size versus private key size
	 */
	ret = nn_bitlen(&(key_pair->priv_key.x), &blen); EG(ret, err);
	MUST_HAVE(blen <= (8 * h->digest_size), ret, err);

	/*
	 * Initialize hash context stored in our private part of context
	 * and record data init has been done
	 */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(h); EG(ret, err);
	ret = h->hfunc_init(&(ctx->sign_data.eddsa.h_ctx)); EG(ret, err);

	/* Initialize other elements in the context */
	ctx->sign_data.eddsa.magic = EDDSA_SIGN_MAGIC;

err:
	PTR_NULLIFY(key_pair);
	PTR_NULLIFY(h);
	VAR_ZEROIFY(use_message_pre_hash);

	return ret;
}

int _eddsa_sign_update_pre_hash(struct ec_sign_context *ctx,
		       const u8 *chunk, u32 chunklen)
{
	int ret;
	ec_alg_type key_type = UNKNOWN_ALG;
	u8 use_message_pre_hash = 0;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an EDDSA
	 * verification one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	EDDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.eddsa), ret, err);
	MUST_HAVE((chunk != NULL), ret, err);

	key_type = ctx->key_pair->priv_key.key_type;

	/* Sanity check: this function is only supported in PH mode */
#if defined(WITH_SIG_EDDSA25519)
	if(key_type == EDDSA25519PH){
		use_message_pre_hash = 1;
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if(key_type == EDDSA448PH){
		use_message_pre_hash = 1;
	}
#endif
	MUST_HAVE(use_message_pre_hash == 1, ret, err);

	/* Sanity check on hash types */
	MUST_HAVE((ctx->h->type == get_eddsa_hash_type(key_type)), ret, err);

	/* 2. Compute h = H(m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);

	ret = ctx->h->hfunc_update(&(ctx->sign_data.eddsa.h_ctx), chunk, chunklen);

err:
	VAR_ZEROIFY(use_message_pre_hash);

	return ret;

}

int _eddsa_sign_finalize_pre_hash(struct ec_sign_context *ctx, u8 *sig, u8 siglen)
{
	const ec_priv_key *priv_key;
	const ec_pub_key *pub_key;
	prj_pt_src_t G;
	u8 hash[MAX_DIGEST_SIZE];
	u8 ph_hash[MAX_DIGEST_SIZE];
	prj_pt R;
	ec_edwards_crv crv_edwards;
	aff_pt_edwards Tmp_edwards;
	nn_src_t q;
	u8 hsize, hash_size;
	int ret;
	ec_shortw_crv_src_t shortw_curve;
	fp_src_t alpha_montgomery;
	fp_src_t gamma_montgomery;
	fp_src_t alpha_edwards;
	prj_pt_src_t pub_key_y;
	u8 use_message_pre_hash = 0;
	u16 use_message_pre_hash_hsize = 0;
	ec_alg_type key_type = UNKNOWN_ALG;
	u8 r_len, s_len;
	const hash_mapping *h;

	nn r, s, S;
#ifdef USE_SIG_BLINDING
	/* b is the blinding mask */
	nn b, binv;
	b.magic = binv.magic = WORD(0);
#endif /* !USE_SIG_BLINDING */
	r.magic = s.magic = S.magic = WORD(0);
	R.magic = crv_edwards.magic = Tmp_edwards.magic = WORD(0);

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an EDDSA
	 * signature one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	EDDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.eddsa), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Zero init out points and data */
	ret = local_memset(&R, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&Tmp_edwards, 0, sizeof(aff_pt_edwards)); EG(ret, err);
	ret = local_memset(&crv_edwards, 0, sizeof(ec_edwards_crv)); EG(ret, err);
	ret = local_memset(hash, 0, sizeof(hash)); EG(ret, err);
	ret = local_memset(ph_hash, 0, sizeof(ph_hash)); EG(ret, err);

	/* Key type */
	key_type = ctx->key_pair->priv_key.key_type;
	/* Sanity check on hash types */
	MUST_HAVE((key_type == ctx->key_pair->pub_key.key_type) && (ctx->h->type == get_eddsa_hash_type(key_type)), ret, err);

	/* Make things more readable */
	priv_key = &(ctx->key_pair->priv_key);
	pub_key = &(ctx->key_pair->pub_key);
	q = &(priv_key->params->ec_gen_order);
	G = &(priv_key->params->ec_gen);
	h = ctx->h;
	hsize = h->digest_size;
	r_len = EDDSA_R_LEN(hsize);
	s_len = EDDSA_S_LEN(hsize);

	shortw_curve = &(priv_key->params->ec_curve);
	alpha_montgomery = &(priv_key->params->ec_alpha_montgomery);
	gamma_montgomery = &(priv_key->params->ec_gamma_montgomery);
	alpha_edwards = &(priv_key->params->ec_alpha_edwards);
	pub_key_y = &(pub_key->y);

	dbg_nn_print("p", &(priv_key->params->ec_fp.p));
	dbg_nn_print("q", &(priv_key->params->ec_gen_order));
	dbg_priv_key_print("x", priv_key);
	dbg_ec_point_print("G", &(priv_key->params->ec_gen));
	dbg_pub_key_print("Y", &(ctx->key_pair->pub_key));

	/* Check provided signature length */
	MUST_HAVE((siglen == EDDSA_SIGLEN(hsize)) && (siglen == (r_len + s_len)), ret, err);

	/* Is it indeed a PH version of the algorithm? */
#if defined(WITH_SIG_EDDSA25519)
	if(key_type == EDDSA25519PH){
		use_message_pre_hash = 1;
		use_message_pre_hash_hsize = hsize;
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if(key_type == EDDSA448PH){
		use_message_pre_hash = 1;
		/* NOTE: as per RFC8032, EDDSA448PH uses
		 * SHAKE256 with 64 bytes output.
		 */
		use_message_pre_hash_hsize = 64;
	}
#endif
	/* Sanity check: this function is only supported in PH mode */
	MUST_HAVE((use_message_pre_hash == 1), ret, err);

	/* Finish the message hash session */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(h); EG(ret, err);

	ret = h->hfunc_finalize(&(ctx->sign_data.eddsa.h_ctx), ph_hash); EG(ret, err);

	/* 1. Finish computing the nonce r = H(h256 || ... || h511 || m) */
	/* Update our hash context with half of the secret key */
	hash_size = sizeof(hash);
	ret = eddsa_get_digest_from_priv_key(hash, &hash_size, priv_key); EG(ret, err);

	/* Sanity check */
	MUST_HAVE((hash_size == hsize), ret, err);

	/* Hash half the digest */
	ret = h->hfunc_init(&(ctx->sign_data.eddsa.h_ctx)); EG(ret, err);

	/* At this point, we are ensured that we have PH versions of the algorithms */
#if defined(WITH_SIG_EDDSA25519)
	if(key_type == EDDSA25519PH){
		ret = dom2(1, ctx->adata, ctx->adata_len, h,
			&(ctx->sign_data.eddsa.h_ctx)); EG(ret, err);
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if(key_type == EDDSA448PH){
		ret = dom4(1, ctx->adata, ctx->adata_len, h,
			&(ctx->sign_data.eddsa.h_ctx)); EG(ret, err);
	}
#endif
	ret = h->hfunc_update(&(ctx->sign_data.eddsa.h_ctx), &hash[hsize / 2], hsize / 2); EG(ret, err);

	/* Update hash h with message hash PH(m) */
	MUST_HAVE((use_message_pre_hash_hsize <= hsize), ret, err);

	ret = h->hfunc_update(&(ctx->sign_data.eddsa.h_ctx), ph_hash,
			use_message_pre_hash_hsize); EG(ret, err);

	/* 1. Finish computing the nonce r = H(h256 || ... || h511 || PH(m)) */
	ret = h->hfunc_finalize(&(ctx->sign_data.eddsa.h_ctx), hash); EG(ret, err);
	dbg_buf_print("h(h || m)", hash, hsize);

	/* Import r as the hash scalar */
	ret = eddsa_decode_integer(&r, hash, hsize); EG(ret, err);

#ifdef USE_SIG_BLINDING
	/* Get a random b for blinding the r modular operations before the
	 * scalar multiplication as we do not want it to leak.
	 */
	ret = nn_get_random_mod(&b, q); EG(ret, err);
	dbg_nn_print("b", &b);
	/* NOTE: we use Fermat's little theorem inversion for
	 * constant time here. This is possible since q is prime.
	 */
	ret = nn_modinv_fermat(&binv, &b, q); EG(ret, err);

	/* Blind r */
	ret = nn_mul(&r, &r, &b); EG(ret, err);
#endif /* USE_SIG_BLINDING */

	/* Reduce r modulo q for the next computation.
	 * (this is a blind reduction if USE_SIG_BLINDING).
	 */
	ret = nn_mod_notrim(&r, &r, q); EG(ret, err);

	/* Now perform our scalar multiplication.
	 */
#if defined(WITH_SIG_EDDSA448)
	if(key_type == EDDSA448PH){
		/*
		 * NOTE: in case of EDDSA448, because of the 4-isogeny we must
		 * divide our scalar by 4.
		 */
		nn r_tmp;
		r_tmp.magic = WORD(0);

		ret = nn_init(&r_tmp, 0); EG(ret, err1);
		ret = nn_modinv_word(&r_tmp, WORD(4), q); EG(ret, err1);
		ret = nn_mod_mul(&r_tmp, &r_tmp, &r, q); EG(ret, err1);

#ifdef USE_SIG_BLINDING
		/* Unblind r_tmp */
		ret = nn_mod_mul(&r_tmp, &r_tmp, &binv, q); EG(ret, err1);
		ret = prj_pt_mul_blind(&R, &r_tmp, G);
#else
		ret = prj_pt_mul(&R, &r_tmp, G);
#endif /* !USE_SIG_BLINDING */
err1:
		nn_uninit(&r_tmp);
		EG(ret, err);
	}
	else
#endif /* !defined(WITH_SIG_EDDSA448) */
	{
#ifdef USE_SIG_BLINDING
		nn r_tmp;
		r_tmp.magic = WORD(0);

		ret = nn_init(&r_tmp, 0); EG(ret, err2);
		ret = nn_copy(&r_tmp, &r); EG(ret, err2);

		/* Unblind r_tmp */
		ret = nn_mod_mul(&r_tmp, &r_tmp, &binv, q); EG(ret, err2);
		ret = prj_pt_mul_blind(&R, &r_tmp, G); EG(ret, err2);
err2:
		nn_uninit(&r_tmp);
		EG(ret, err);
#else
		ret = prj_pt_mul(&R, &r, G); EG(ret, err);
#endif /* !USE_SIG_BLINDING */
	}

	/* Now compute S = (r + H(R || PubKey || PH(m)) * secret) mod q */
	ret = h->hfunc_init(&(ctx->sign_data.eddsa.h_ctx)); EG(ret, err);
	/* Transfer R to Edwards */
	ret = curve_shortw_to_edwards(shortw_curve, &crv_edwards, alpha_montgomery,
				gamma_montgomery, alpha_edwards); EG(ret, err);
	ret = prj_pt_shortw_to_aff_pt_edwards(&R, &crv_edwards, &Tmp_edwards,
					alpha_edwards); EG(ret, err);
	dbg_ec_edwards_point_print("R", &Tmp_edwards);

	MUST_HAVE((r_len <= siglen), ret, err);
	/* Encode R and update */
	ret = eddsa_encode_point(&Tmp_edwards, alpha_edwards, &sig[0],
			      r_len, key_type); EG(ret, err);
	/* At this point, we are ensured that we have PH versions of the algorithms */
#if defined(WITH_SIG_EDDSA25519)
	if(key_type == EDDSA25519PH){
		ret = dom2(1, ctx->adata, ctx->adata_len, h,
			&(ctx->sign_data.eddsa.h_ctx)); EG(ret, err);
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if(key_type == EDDSA448PH){
		ret = dom4(1, ctx->adata, ctx->adata_len, h,
			&(ctx->sign_data.eddsa.h_ctx)); EG(ret, err);
	}
#endif
	/* Update the hash with the encoded R point */
	ret = h->hfunc_update(&(ctx->sign_data.eddsa.h_ctx), &sig[0], r_len); EG(ret, err);
	/* Encode the public key */
	/* Transfer the public key to Edwards */
	ret = prj_pt_shortw_to_aff_pt_edwards(pub_key_y, &crv_edwards,
					&Tmp_edwards, alpha_edwards); EG(ret, err);
	dbg_ec_edwards_point_print("A", &Tmp_edwards);
	MUST_HAVE(r_len <= sizeof(hash), ret, err);

	/* NOTE: we use the hash buffer as a temporary buffer */
	ret = eddsa_encode_point(&Tmp_edwards, alpha_edwards, hash,
			      r_len, key_type); EG(ret, err);

	/* Update the hash with the encoded public key point */
	ret = h->hfunc_update(&(ctx->sign_data.eddsa.h_ctx), hash, r_len); EG(ret, err);
	/* Update the hash with PH(m) */
	ret = h->hfunc_update(&(ctx->sign_data.eddsa.h_ctx), ph_hash,
			use_message_pre_hash_hsize); EG(ret, err);
	/* Finalize the hash */
	ret = h->hfunc_finalize(&(ctx->sign_data.eddsa.h_ctx), hash); EG(ret, err);
	dbg_buf_print("h(R || PubKey || PH(m))", hash, hsize);
	/* Import our resulting hash as an integer in S */
	ret = eddsa_decode_integer(&S, hash, hsize); EG(ret, err);
	ret = nn_mod(&S, &S, q); EG(ret, err);
	/* Extract the digest */
	hsize = sizeof(hash);
	ret = eddsa_get_digest_from_priv_key(hash, &hsize, priv_key); EG(ret, err);
	/* Encode the scalar s from the digest */
	ret = eddsa_compute_s(&s, hash, hsize); EG(ret, err);
	ret = nn_mod(&s, &s, q); EG(ret, err);

#ifdef USE_SIG_BLINDING
	/* If we use blinding, multiply by b */
	ret = nn_mod_mul(&S, &S, &b, q); EG(ret, err);
#endif /* !USE_SIG_BLINDING */
	/* Multiply by the secret */
	ret = nn_mod_mul(&S, &S, &s, q); EG(ret, err);
	/* The secret is not needed anymore */
	nn_uninit(&s);
	/* Add to r */
	ret = nn_mod_add(&S, &S, &r, q); EG(ret, err);
#ifdef USE_SIG_BLINDING
	/* Unblind the result */
	ret = nn_mod_mul(&S, &S, &binv, q); EG(ret, err);
#endif /* !USE_SIG_BLINDING */
	/* Store our S in the context as an encoded buffer */
	MUST_HAVE((s_len <= (siglen - r_len)), ret, err);
	ret = eddsa_encode_integer(&S, &sig[r_len], s_len);

 err:
	/* Clean what remains on the stack */
	PTR_NULLIFY(h);
	PTR_NULLIFY(priv_key);
	PTR_NULLIFY(pub_key);
	PTR_NULLIFY(G);
	PTR_NULLIFY(q);
	PTR_NULLIFY(shortw_curve);
	PTR_NULLIFY(alpha_montgomery);
	PTR_NULLIFY(gamma_montgomery);
	PTR_NULLIFY(alpha_edwards);
	PTR_NULLIFY(pub_key_y);
	VAR_ZEROIFY(hsize);
	VAR_ZEROIFY(hash_size);
	VAR_ZEROIFY(use_message_pre_hash);
	VAR_ZEROIFY(use_message_pre_hash_hsize);
	VAR_ZEROIFY(r_len);
	VAR_ZEROIFY(s_len);

	prj_pt_uninit(&R);
	ec_edwards_crv_uninit(&crv_edwards);
	aff_pt_edwards_uninit(&Tmp_edwards);
	nn_uninit(&s);
	nn_uninit(&r);
	nn_uninit(&S);

#ifdef USE_SIG_BLINDING
	nn_uninit(&b);
	nn_uninit(&binv);
#endif /* !USE_SIG_BLINDING */

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->sign_data.eddsa), 0, sizeof(eddsa_sign_data)));
	}
	IGNORE_RET_VAL(local_memset(ph_hash, 0, sizeof(ph_hash)));

	return ret;
}


/******** Signature function specific to pure EdDSA where the message
********* streaming mode via init/update/finalize is not supported.
 */
int _eddsa_sign(u8 *sig, u8 siglen, const ec_key_pair *key_pair,
		const u8 *m, u32 mlen, int (*rand) (nn_t out, nn_src_t q),
		ec_alg_type sig_type, hash_alg_type hash_type,
		const u8 *adata, u16 adata_len)
{
	int ret;
	ec_alg_type key_type = UNKNOWN_ALG;
	ec_shortw_crv_src_t shortw_curve;
	fp_src_t alpha_montgomery;
	fp_src_t gamma_montgomery;
	fp_src_t alpha_edwards;
	prj_pt_src_t pub_key_y;
	u8 use_message_pre_hash = 0;
	u16 use_message_pre_hash_hsize = 0;
	prj_pt_src_t G;
	prj_pt R;
	aff_pt_edwards Tmp_edwards;
	ec_edwards_crv crv_edwards;
	u8 hash[MAX_DIGEST_SIZE];
	u8 ph_hash[MAX_DIGEST_SIZE];
	const ec_priv_key *priv_key;
	const ec_pub_key *pub_key;
	nn_src_t q;
	u8 hsize, hash_size;
	hash_context h_ctx;
	u8 r_len, s_len;
	bitcnt_t blen;
	const hash_mapping *h;

	nn r, s, S;
#ifdef USE_SIG_BLINDING
	/* b is the blinding mask */
	nn b, binv;
	b.magic = binv.magic = WORD(0);
#endif

	r.magic = s.magic = S.magic = WORD(0);
	R.magic = Tmp_edwards.magic = crv_edwards.magic = WORD(0);

	/*
	 * NOTE: EdDSA does not use any notion of random Nonce, so no need
	 * to use 'rand' here: we strictly check that NULL is provided.
	 */
	MUST_HAVE((rand == NULL), ret, err);

	/* Zero init out points and data */
	ret = local_memset(&R, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&Tmp_edwards, 0, sizeof(aff_pt_edwards)); EG(ret, err);
	ret = local_memset(&crv_edwards, 0, sizeof(ec_edwards_crv)); EG(ret, err);
	ret = local_memset(hash, 0, sizeof(hash)); EG(ret, err);
	ret = local_memset(ph_hash, 0, sizeof(ph_hash)); EG(ret, err);

	/* Sanity check on the key pair */
	ret = eddsa_key_pair_sanity_check(key_pair); EG(ret, err);

	/* Make things more readable */
	ret = get_hash_by_type(hash_type, &h); EG(ret, err);
	key_type = key_pair->priv_key.key_type;

	/* Sanity check on the hash type */
	MUST_HAVE((h != NULL), ret, err);
	MUST_HAVE((get_eddsa_hash_type(sig_type) == hash_type), ret, err);
	/* Sanity check on the key type */
	MUST_HAVE(key_type == sig_type, ret, err);
	MUST_HAVE((h != NULL) && (h->digest_size <= MAX_DIGEST_SIZE) && (h->block_size <= MAX_BLOCK_SIZE), ret, err);
	/*
	 * Sanity check on hash size versus private key size
	 */
	ret = nn_bitlen(&(key_pair->priv_key.x), &blen); EG(ret, err);
	MUST_HAVE((blen <= (8 * h->digest_size)), ret, err);

	/* Make things more readable */
	priv_key = &(key_pair->priv_key);
	pub_key = &(key_pair->pub_key);
	q = &(priv_key->params->ec_gen_order);
	G = &(priv_key->params->ec_gen);
	hsize = h->digest_size;
	r_len = EDDSA_R_LEN(hsize);
	s_len = EDDSA_S_LEN(hsize);

	shortw_curve = &(priv_key->params->ec_curve);
	alpha_montgomery = &(priv_key->params->ec_alpha_montgomery);
	gamma_montgomery = &(priv_key->params->ec_gamma_montgomery);
	alpha_edwards = &(priv_key->params->ec_alpha_edwards);
	pub_key_y = &(pub_key->y);

	dbg_nn_print("p", &(priv_key->params->ec_fp.p));
	dbg_nn_print("q", &(priv_key->params->ec_gen_order));
	dbg_priv_key_print("x", priv_key);
	dbg_ec_point_print("G", &(priv_key->params->ec_gen));
	dbg_pub_key_print("Y", pub_key);

	/* Check provided signature length */
	MUST_HAVE((siglen == EDDSA_SIGLEN(hsize)) && (siglen == (r_len + s_len)), ret, err);

	/* Do we use the raw message or its PH(M) hashed version? */
#if defined(WITH_SIG_EDDSA25519)
	if(key_type == EDDSA25519PH){
		use_message_pre_hash = 1;
		use_message_pre_hash_hsize = hsize;
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if(key_type == EDDSA448PH){
		use_message_pre_hash = 1;
		/* NOTE: as per RFC8032, EDDSA448PH uses
		 * SHAKE256 with 64 bytes output.
		 */
		use_message_pre_hash_hsize = 64;
	}
#endif
	/* First of all, compute the message hash if necessary */
	if(use_message_pre_hash){
		hash_size = sizeof(ph_hash);
		ret = eddsa_compute_pre_hash(m, mlen, ph_hash, &hash_size, sig_type); EG(ret, err);
		MUST_HAVE(use_message_pre_hash_hsize <= hash_size, ret, err);
	}
	/* Initialize our hash context */
	/* Compute half of the secret key */
	hash_size = sizeof(hash);
	ret = eddsa_get_digest_from_priv_key(hash, &hash_size, &(key_pair->priv_key)); EG(ret, err);
	/* Sanity check */
	MUST_HAVE((hash_size == hsize), ret, err);
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(h); EG(ret, err);
	ret = h->hfunc_init(&h_ctx); EG(ret, err);
#if defined(WITH_SIG_EDDSA25519)
	if(key_type == EDDSA25519CTX){
		/* As per RFC8032, for EDDSA25519CTX the context SHOULD NOT be empty */
		MUST_HAVE(adata != NULL, ret, err);
		ret = dom2(0, adata, adata_len, h, &h_ctx); EG(ret, err);
	}
	if(key_type == EDDSA25519PH){
		ret = dom2(1, adata, adata_len, h, &h_ctx); EG(ret, err);
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if(key_type == EDDSA448){
		ret = dom4(0, adata, adata_len, h, &h_ctx); EG(ret, err);
	}
	if(key_type == EDDSA448PH){
		ret = dom4(1, adata, adata_len, h, &h_ctx); EG(ret, err);
	}
#endif
	ret = h->hfunc_update(&h_ctx, &hash[hsize / 2], hsize / 2); EG(ret, err);

	/* Now finish computing the scalar r */
	if(use_message_pre_hash){
		ret = h->hfunc_update(&h_ctx, ph_hash, use_message_pre_hash_hsize); EG(ret, err);
	}
	else{
		ret = h->hfunc_update(&h_ctx, m, mlen); EG(ret, err);
	}
	ret = h->hfunc_finalize(&h_ctx, hash); EG(ret, err);
	dbg_buf_print("h(h || PH(m))", hash, hsize);

	/* Import r as the hash scalar */
	ret = eddsa_decode_integer(&r, hash, hsize); EG(ret, err);

#ifdef USE_SIG_BLINDING
	/* Get a random b for blinding the r modular operations before the
	 * scalar multiplication as we do not want it to leak.
	 */
	ret = nn_get_random_mod(&b, q); EG(ret, err);
	dbg_nn_print("b", &b);
	/* NOTE: we use Fermat's little theorem inversion for
	 * constant time here. This is possible since q is prime.
	 */
	ret = nn_modinv_fermat(&binv, &b, q); EG(ret, err);

	/* Blind r */
	ret = nn_mul(&r, &r, &b); EG(ret, err);
#endif /* !USE_SIG_BLINDING */

	/* Reduce r modulo q for the next computation.
	 * (this is a blind reduction if USE_SIG_BLINDING).
	 */
	ret = nn_mod_notrim(&r, &r, q); EG(ret, err);

	/* Now perform our scalar multiplication.
	 */
#if defined(WITH_SIG_EDDSA448)
	if((key_type == EDDSA448) || (key_type == EDDSA448PH)){
		/*
		 * NOTE: in case of EDDSA448, because of the 4-isogeny we must
		 * divide our scalar by 4.
		 */
		nn r_tmp;
		r_tmp.magic = WORD(0);

		ret = nn_init(&r_tmp, 0); EG(ret, err1);
		ret = nn_modinv_word(&r_tmp, WORD(4), q); EG(ret, err1);
		ret = nn_mod_mul(&r_tmp, &r_tmp, &r, q); EG(ret, err1);

#ifdef USE_SIG_BLINDING
		/* Unblind r_tmp */
		ret = nn_mod_mul(&r_tmp, &r_tmp, &binv, q); EG(ret, err1);
		ret = prj_pt_mul_blind(&R, &r_tmp, G);
#else
		ret = prj_pt_mul(&R, &r_tmp, G);
#endif /* !USE_SIG_BLINDING */
err1:
		nn_uninit(&r_tmp);
		EG(ret, err);
	}
	else
#endif /* !defined(WITH_SIG_EDDSA448) */
	{
#ifdef USE_SIG_BLINDING
		nn r_tmp;
		r_tmp.magic = WORD(0);

		ret = nn_init(&r_tmp, 0); EG(ret, err2);
		ret = nn_copy(&r_tmp, &r); EG(ret, err2);

		/* Unblind r_tmp */
		ret = nn_mod_mul(&r_tmp, &r_tmp, &binv, q); EG(ret, err2);
		ret = prj_pt_mul_blind(&R, &r_tmp, G); EG(ret, err2);
err2:
		nn_uninit(&r_tmp);
		EG(ret, err);
#else
		ret = prj_pt_mul(&R, &r, G); EG(ret, err);
#endif /* !USE_SIG_BLINDING */
	}

	/* Now compute S = (r + H(R || PubKey || PH(m)) * secret) mod q */
	ret = hash_mapping_callbacks_sanity_check(h); EG(ret, err);
	ret = h->hfunc_init(&h_ctx); EG(ret, err);
	/* Transfer R to Edwards */
	ret = curve_shortw_to_edwards(shortw_curve, &crv_edwards, alpha_montgomery,
				gamma_montgomery, alpha_edwards); EG(ret, err);
	ret = prj_pt_shortw_to_aff_pt_edwards(&R, &crv_edwards, &Tmp_edwards,
					alpha_edwards); EG(ret, err);
	dbg_ec_edwards_point_print("R", &Tmp_edwards);
	MUST_HAVE((r_len <= siglen), ret, err);
	/* Encode R and update */
	ret = eddsa_encode_point(&Tmp_edwards, alpha_edwards, &sig[0],
			      r_len, key_type); EG(ret, err);
#if defined(WITH_SIG_EDDSA25519)
	if(key_type == EDDSA25519CTX){
		/*
		 * As per RFC8032, for EDDSA25519CTX the context
		 * SHOULD NOT be empty
		 */
		MUST_HAVE((adata != NULL), ret, err);
		ret = dom2(0, adata, adata_len, h, &h_ctx); EG(ret, err);
	}
	if(key_type == EDDSA25519PH){
		ret = dom2(1, adata, adata_len, h, &h_ctx); EG(ret, err);
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if(key_type == EDDSA448){
		ret = dom4(0, adata, adata_len, h, &h_ctx); EG(ret, err);
	}
	if(key_type == EDDSA448PH){
		ret = dom4(1, adata, adata_len, h, &h_ctx); EG(ret, err);
	}
#endif
	/* Update the hash with the encoded R point */
	ret = h->hfunc_update(&h_ctx, &sig[0], r_len); EG(ret, err);
	/* Transfer the public key to Edwards */
	ret = prj_pt_shortw_to_aff_pt_edwards(pub_key_y, &crv_edwards, &Tmp_edwards,
					alpha_edwards); EG(ret, err);
	dbg_ec_edwards_point_print("A", &Tmp_edwards);
	MUST_HAVE((r_len <= sizeof(hash)), ret, err);
	/* Encode the public key */
	/* NOTE: we use the hash buffer as a temporary buffer */
	ret = eddsa_encode_point(&Tmp_edwards, alpha_edwards,
			      hash, r_len, key_type); EG(ret, err);
	/* Update the hash with the encoded public key point */
	ret = h->hfunc_update(&h_ctx, hash, r_len); EG(ret, err);
	/* Update the hash with the message or its hash for the PH versions */
	if(use_message_pre_hash){
		ret = h->hfunc_update(&h_ctx, ph_hash, use_message_pre_hash_hsize); EG(ret, err);
	}
	else{
		ret = h->hfunc_update(&h_ctx, m, mlen); EG(ret, err);
	}
	/* Finalize the hash */
	ret = h->hfunc_finalize(&h_ctx, hash); EG(ret, err);
	dbg_buf_print("h(R || PubKey || PH(m))", hash, hsize);
	/* Import our resulting hash as an integer in S */
	ret = eddsa_decode_integer(&S, hash, hsize); EG(ret, err);
	ret = nn_mod(&S, &S, q); EG(ret, err);
	/* Extract the digest */
	hsize = sizeof(hash);
	ret = eddsa_get_digest_from_priv_key(hash, &hsize, priv_key); EG(ret, err);
	ret = eddsa_compute_s(&s, hash, hsize); EG(ret, err);
	ret = nn_mod(&s, &s, q); EG(ret, err);
#ifdef USE_SIG_BLINDING
	/* If we use blinding, multiply by b */
	ret = nn_mod_mul(&S, &S, &b, q); EG(ret, err);
#endif /* !USE_SIG_BLINDING */
	/* Multiply by the secret */
	ret = nn_mod_mul(&S, &S, &s, q); EG(ret, err);
	/* The secret is not needed anymore */
	nn_uninit(&s);
	/* Add to r */
	ret = nn_mod_add(&S, &S, &r, q); EG(ret, err);
#ifdef USE_SIG_BLINDING
	/* Unblind the result */
	ret = nn_mod_mul(&S, &S, &binv, q); EG(ret, err);
#endif /* !USE_SIG_BLINDING */
	/* Store our S in the context as an encoded buffer */
	MUST_HAVE((s_len <= (siglen - r_len)), ret, err);
	/* Encode the scalar s from the digest */
	ret = eddsa_encode_integer(&S, &sig[r_len], s_len);

err:
	/* Clean what remains on the stack */
	PTR_NULLIFY(priv_key);
	PTR_NULLIFY(pub_key);
	PTR_NULLIFY(G);
	PTR_NULLIFY(q);
	PTR_NULLIFY(shortw_curve);
	PTR_NULLIFY(alpha_montgomery);
	PTR_NULLIFY(gamma_montgomery);
	PTR_NULLIFY(alpha_edwards);
	PTR_NULLIFY(pub_key_y);
	PTR_NULLIFY(h);
	VAR_ZEROIFY(hsize);
	VAR_ZEROIFY(hash_size);
	VAR_ZEROIFY(use_message_pre_hash);
	VAR_ZEROIFY(use_message_pre_hash_hsize);
	VAR_ZEROIFY(r_len);
	VAR_ZEROIFY(s_len);
	VAR_ZEROIFY(blen);
	IGNORE_RET_VAL(local_memset(&h_ctx, 0, sizeof(h_ctx)));
	IGNORE_RET_VAL(local_memset(hash, 0, sizeof(hash)));
	IGNORE_RET_VAL(local_memset(ph_hash, 0, sizeof(ph_hash)));

	prj_pt_uninit(&R);
	ec_edwards_crv_uninit(&crv_edwards);
	aff_pt_edwards_uninit(&Tmp_edwards);
	nn_uninit(&s);
	nn_uninit(&r);
	nn_uninit(&S);

#ifdef USE_SIG_BLINDING
	nn_uninit(&b);
	nn_uninit(&binv);
#endif /* USE_SIG_BLINDING */

	return ret;
}

/******************************************************************************/
/*
 * Generic *internal* EDDSA verification functions (init, update and finalize).
 *
 */

#define EDDSA_VERIFY_MAGIC ((word_t)(0x3298fe87e77151beULL))
#define EDDSA_VERIFY_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == EDDSA_VERIFY_MAGIC), ret, err)

int _eddsa_verify_init(struct ec_verify_context *ctx, const u8 *sig, u8 siglen)
{
	nn_src_t q;
	ec_edwards_crv crv_edwards;
	aff_pt_edwards R;
	prj_pt _Tmp;
	prj_pt_t _R;
	aff_pt_edwards A;
	nn *S;
	u8 buff[MAX_DIGEST_SIZE];
	int ret, iszero, cmp;
	u16 hsize;
	const ec_pub_key *pub_key;
	ec_shortw_crv_src_t shortw_curve;
	fp_src_t alpha_montgomery;
	fp_src_t gamma_montgomery;
	fp_src_t alpha_edwards;
	nn_src_t gen_cofactor;
	prj_pt_src_t pub_key_y;
	hash_context *h_ctx;
	hash_context *h_ctx_pre_hash;
	ec_alg_type key_type = UNKNOWN_ALG;

	R.magic = crv_edwards.magic = _Tmp.magic = A.magic = WORD(0);

	/* First, verify context has been initialized */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Zero init our local data */
	ret = local_memset(&A, 0, sizeof(aff_pt_edwards)); EG(ret, err);
	ret = local_memset(&crv_edwards, 0, sizeof(ec_edwards_crv)); EG(ret, err);
	ret = local_memset(buff, 0, sizeof(buff)); EG(ret, err);
	ret = local_memset(&R, 0, sizeof(R)); EG(ret, err);
	ret = local_memset(&_Tmp, 0, sizeof(_Tmp)); EG(ret, err);

	/* Do some sanity checks on input params */
	ret = eddsa_pub_key_sanity_check(ctx->pub_key); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) && (ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);

	/* Make things more readable */
	q = &(ctx->pub_key->params->ec_gen_order);
	_R = &(ctx->verify_data.eddsa._R);
	S = &(ctx->verify_data.eddsa.S);
	hsize = ctx->h->digest_size;

	pub_key = ctx->pub_key;
	shortw_curve = &(pub_key->params->ec_curve);
	alpha_montgomery = &(pub_key->params->ec_alpha_montgomery);
	gamma_montgomery = &(pub_key->params->ec_gamma_montgomery);
	alpha_edwards = &(pub_key->params->ec_alpha_edwards);
	gen_cofactor = &(pub_key->params->ec_gen_cofactor);
	pub_key_y = &(pub_key->y);
	key_type = pub_key->key_type;
	h_ctx = &(ctx->verify_data.eddsa.h_ctx);
	h_ctx_pre_hash = &(ctx->verify_data.eddsa.h_ctx_pre_hash);

	/* Sanity check on hash types */
	MUST_HAVE((ctx->h->type == get_eddsa_hash_type(key_type)), ret, err);

	/* Check given signature length is the expected one */
	MUST_HAVE((siglen == EDDSA_SIGLEN(hsize)), ret, err);
	MUST_HAVE((siglen == (EDDSA_R_LEN(hsize) + EDDSA_S_LEN(hsize))), ret, err);

	/* Initialize the hash context */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(h_ctx); EG(ret, err);
	ret = ctx->h->hfunc_init(h_ctx_pre_hash); EG(ret, err);
#if defined(WITH_SIG_EDDSA25519)
	if(key_type == EDDSA25519CTX){
		/* As per RFC8032, for EDDSA25519CTX the context SHOULD NOT be empty */
		MUST_HAVE((ctx->adata != NULL), ret, err);
		ret = dom2(0, ctx->adata, ctx->adata_len, ctx->h, h_ctx); EG(ret, err);
	}
	if(key_type == EDDSA25519PH){
		ret = dom2(1, ctx->adata, ctx->adata_len, ctx->h, h_ctx); EG(ret, err);
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if(key_type == EDDSA448){
		ret = dom4(0, ctx->adata, ctx->adata_len, ctx->h, h_ctx); EG(ret, err);
	}
	if(key_type == EDDSA448PH){
		ret = dom4(1, ctx->adata, ctx->adata_len, ctx->h, h_ctx); EG(ret, err);
	}
#endif
	/* Import R and S values from signature buffer */
	/*******************************/
	/* Import R as an Edwards point */
	ret = curve_shortw_to_edwards(shortw_curve, &crv_edwards, alpha_montgomery,
				gamma_montgomery, alpha_edwards); EG(ret, err);
	/* NOTE: non canonical R are checked and rejected here */
	ret = eddsa_decode_point(&R, &crv_edwards, alpha_edwards, &sig[0],
			      EDDSA_R_LEN(hsize), key_type); EG(ret, err);
	dbg_ec_edwards_point_print("R", &R);
	/* Transfer our public point R to Weierstrass */
	ret = aff_pt_edwards_to_prj_pt_shortw(&R, shortw_curve, _R, alpha_edwards); EG(ret, err);
	/* Update the hash with the encoded R */
	ret = ctx->h->hfunc_update(h_ctx, &sig[0], EDDSA_R_LEN(hsize)); EG(ret, err);

	/*******************************/
	/* Import S as an integer */
	ret = eddsa_decode_integer(S, &sig[EDDSA_R_LEN(hsize)], EDDSA_S_LEN(hsize)); EG(ret, err);
	/* Reject S if it is not reduced modulo q */
	ret = nn_cmp(S, q, &cmp); EG(ret, err);
	MUST_HAVE((cmp < 0), ret, err);
	dbg_nn_print("S", S);

	/*******************************/
	/* Encode the public key
	 * NOTE: since we deal with a public key transfered to Weierstrass,
	 * encoding checking has been handled elsewhere.
	 */
	/* Reject the signature if the public key is one of small order points.
	 * We multiply by the cofactor: since this is a public verification,
	 * we use a basic double and add algorithm.
	 */
	ret = _prj_pt_unprotected_mult(&_Tmp, gen_cofactor, pub_key_y); EG(ret, err);
	/* Reject the signature if we have point at infinity here as this means
	 * that the public key is of small order.
	 */
	ret = prj_pt_iszero(&_Tmp, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	/* Transfer the public key to Edwards */
	ret = prj_pt_shortw_to_aff_pt_edwards(pub_key_y, &crv_edwards, &A, alpha_edwards); EG(ret, err);
	dbg_ec_edwards_point_print("A", &A);
	MUST_HAVE((EDDSA_R_LEN(hsize) <= sizeof(buff)), ret, err);
	/* NOTE: we use the hash buffer as a temporary buffer */
	ret = eddsa_encode_point(&A, alpha_edwards, buff, EDDSA_R_LEN(hsize), key_type); EG(ret, err);

	/* Update the hash with the encoded public key */
	ret = ctx->h->hfunc_update(h_ctx, buff, EDDSA_R_LEN(hsize)); EG(ret, err);

	/* Context magic set */
	ctx->verify_data.eddsa.magic = EDDSA_VERIFY_MAGIC;

 err:
	PTR_NULLIFY(q);
	PTR_NULLIFY(_R);
	PTR_NULLIFY(S);
	PTR_NULLIFY(pub_key);
	PTR_NULLIFY(shortw_curve);
	PTR_NULLIFY(alpha_montgomery);
	PTR_NULLIFY(gamma_montgomery);
	PTR_NULLIFY(alpha_edwards);
	PTR_NULLIFY(gen_cofactor);
	PTR_NULLIFY(pub_key_y);

	ec_edwards_crv_uninit(&crv_edwards);
	aff_pt_edwards_uninit(&A);
	aff_pt_edwards_uninit(&R);
	prj_pt_uninit(&_Tmp);

	return ret;
}

int _eddsa_verify_update(struct ec_verify_context *ctx,
			 const u8 *chunk, u32 chunklen)
{
	int ret;
	ec_alg_type key_type = UNKNOWN_ALG;
	u8 use_message_pre_hash = 0;
	hash_context *h_ctx;
	hash_context *h_ctx_pre_hash;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an EDDSA
	 * verification one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	EDDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.eddsa), ret, err);

	key_type = ctx->pub_key->key_type;
	h_ctx = &(ctx->verify_data.eddsa.h_ctx);
	h_ctx_pre_hash = &(ctx->verify_data.eddsa.h_ctx_pre_hash);

	/* Sanity check on hash types */
	MUST_HAVE(ctx->h->type == get_eddsa_hash_type(key_type), ret, err);

	/* Do we use the raw message or its PH(M) hashed version? */
#if defined(WITH_SIG_EDDSA25519)
	if(key_type == EDDSA25519PH){
		use_message_pre_hash = 1;
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if(key_type == EDDSA448PH){
		use_message_pre_hash = 1;
	}
#endif
	/* 2. Compute h = H(m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	if(use_message_pre_hash == 1){
		/* In PH mode, update the dedicated hash context */
		ret = ctx->h->hfunc_update(h_ctx_pre_hash,
				     chunk, chunklen); EG(ret, err);
	}
	else{
		/* In normal mode, update the nominal hash context */
		ret = ctx->h->hfunc_update(h_ctx, chunk, chunklen); EG(ret, err);
	}

err:
	VAR_ZEROIFY(use_message_pre_hash);

	return ret;
}

int _eddsa_verify_finalize(struct ec_verify_context *ctx)
{
	prj_pt_src_t G, _R, A;
	prj_pt _Tmp1, _Tmp2;
	nn_src_t q, S;
	nn h;
	u16 hsize;
	u8 hash[MAX_DIGEST_SIZE];
	nn_src_t gen_cofactor;
	int ret, iszero, cmp;
	ec_alg_type key_type = UNKNOWN_ALG;
	u8 use_message_pre_hash = 0;
	u16 use_message_pre_hash_hsize = 0;
	hash_context *h_ctx;
	hash_context *h_ctx_pre_hash;

	_Tmp1.magic = _Tmp2.magic = h.magic = WORD(0);

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an EDDSA
	 * verification one and we do not finalize() before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	EDDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.eddsa), ret, err);

	/* Zero init points */
	ret = local_memset(&_Tmp1, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&_Tmp2, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(hash, 0, sizeof(hash)); EG(ret, err);

	/* Make things more readable */
	G = &(ctx->pub_key->params->ec_gen);
	A = &(ctx->pub_key->y);
	q = &(ctx->pub_key->params->ec_gen_order);
	hsize = ctx->h->digest_size;
	S = &(ctx->verify_data.eddsa.S);
	_R = &(ctx->verify_data.eddsa._R);
	gen_cofactor = &(ctx->pub_key->params->ec_gen_cofactor);
	key_type = ctx->pub_key->key_type;
	h_ctx = &(ctx->verify_data.eddsa.h_ctx);
	h_ctx_pre_hash = &(ctx->verify_data.eddsa.h_ctx_pre_hash);

	/* Sanity check on hash types */
	MUST_HAVE((ctx->h->type == get_eddsa_hash_type(key_type)), ret, err);

	/* Do we use the raw message or its PH(M) hashed version? */
#if defined(WITH_SIG_EDDSA25519)
	if(key_type == EDDSA25519PH){
		use_message_pre_hash = 1;
		use_message_pre_hash_hsize = hsize;
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if(key_type == EDDSA448PH){
		use_message_pre_hash = 1;
		/* NOTE: as per RFC8032, EDDSA448PH uses
		 * SHAKE256 with 64 bytes output.
		 */
		use_message_pre_hash_hsize = 64;
	}
#endif

	/* Reject S if it is not reduced modulo q */
	ret = nn_cmp(S, q, &cmp); EG(ret, err);
	MUST_HAVE((cmp < 0), ret, err);

	MUST_HAVE((hsize <= sizeof(hash)), ret, err);

	/* 2. Finish our computation of h = H(R || A || M) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	/* Update the hash with the message or its hash for the PH versions */
	if(use_message_pre_hash == 1){
		ret = ctx->h->hfunc_finalize(h_ctx_pre_hash, hash); EG(ret, err);
		MUST_HAVE((use_message_pre_hash_hsize <= hsize), ret, err);
		ret = ctx->h->hfunc_update(h_ctx, hash, use_message_pre_hash_hsize); EG(ret, err);
	}
	ret = ctx->h->hfunc_finalize(h_ctx, hash); EG(ret, err);
	dbg_buf_print("hash = H(R || A || PH(M))", hash, hsize);

	/* 3. Import our hash as a NN and reduce it modulo q */
	ret = eddsa_decode_integer(&h, hash, hsize); EG(ret, err);
	ret = nn_mod(&h, &h, q); EG(ret, err);
	dbg_nn_print("h = ", &h);

#if defined(WITH_SIG_EDDSA448)
	if((key_type == EDDSA448) || (key_type == EDDSA448PH)){
		/* When dealing with EDDSA448, because of our 4-isogeny between Edwars448 and Ed448
		 * mapping base point to four times base point, we actually multiply our public key by 4 here
		 * to be inline with the other computations (the public key stored in Weierstrass )
		 */
		ret = nn_lshift(&h, &h, 2); EG(ret, err);
		ret = nn_mod(&h, &h, q); EG(ret, err);
	}
#endif
	/* 4. Compute (S * G) - R - (h * A)  */
	ret = prj_pt_mul(&_Tmp1, S, G); EG(ret, err);
	ret = prj_pt_neg(&_Tmp2, _R); EG(ret, err);
	ret = prj_pt_add(&_Tmp1, &_Tmp1, &_Tmp2); EG(ret, err);
	ret = prj_pt_mul(&_Tmp2, &h, A); EG(ret, err);
	ret = prj_pt_neg(&_Tmp2, &_Tmp2); EG(ret, err);
	ret = prj_pt_add(&_Tmp1, &_Tmp1, &_Tmp2); EG(ret, err);

	/* 5. We use cofactored multiplication, so multiply by the cofactor:
	 *    since this is a public verification, we use a basic double and add
	 *    algorithm.
	 */
	ret = _prj_pt_unprotected_mult(&_Tmp2, gen_cofactor, &_Tmp1); EG(ret, err);

	/* Reject the signature if we do not have point at infinity here */
	ret = prj_pt_iszero(&_Tmp2, &iszero); EG(ret, err);
	ret = iszero ? 0 : -1;

err:
	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->verify_data.eddsa), 0, sizeof(eddsa_verify_data)));
	}

	/* Clean what remains on the stack */
	PTR_NULLIFY(G);
	PTR_NULLIFY(A);
	PTR_NULLIFY(q);
	PTR_NULLIFY(S);
	PTR_NULLIFY(_R);
	PTR_NULLIFY(gen_cofactor);
	VAR_ZEROIFY(hsize);
	VAR_ZEROIFY(use_message_pre_hash);
	VAR_ZEROIFY(use_message_pre_hash_hsize);

	nn_uninit(&h);
	prj_pt_uninit(&_Tmp1);
	prj_pt_uninit(&_Tmp2);

	return ret;
}

/* Batch verification function:
 * This function takes multiple signatures/messages/public keys, and
 * checks all the signatures.
 *
 * This returns 0 if *all* the signatures are correct, and -1 if at least
 * one signature is not correct.
 *
 * NOTE: the "no_memory" version is not optimized and straightforwardly
 * checks for the signature using naive sums. See below for an optimized
 * Bos-Coster version (but requiring additional memory to work).
 *
 */
ATTRIBUTE_WARN_UNUSED_RET static int _eddsa_verify_batch_no_memory(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
					 const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
		                         hash_alg_type hash_type, const u8 **adata, const u16 *adata_len)
{
	nn_src_t q = NULL;
	ec_edwards_crv crv_edwards;
	aff_pt_edwards R, A;
	prj_pt_src_t G = NULL;
	prj_pt _Tmp, _R_sum, _A_sum;
	nn S, S_sum, z, h;
	u8 hash[MAX_DIGEST_SIZE];
	int ret, iszero, cmp;
	u16 hsize;
	const ec_pub_key *pub_key, *pub_key0;
	ec_shortw_crv_src_t shortw_curve;
	fp_src_t alpha_montgomery;
	fp_src_t gamma_montgomery;
	fp_src_t alpha_edwards;
	nn_src_t gen_cofactor = NULL;
	prj_pt_src_t pub_key_y;
	hash_context h_ctx;
	hash_context h_ctx_pre_hash;
	u8 use_message_pre_hash = 0;
	u16 use_message_pre_hash_hsize = 0;
	const hash_mapping *hm;
	ec_alg_type key_type = UNKNOWN_ALG;
	u32 i;

	R.magic = S.magic = S_sum.magic = crv_edwards.magic = WORD(0);
	_Tmp.magic = _R_sum.magic = _A_sum.magic = WORD(0);
	z.magic = h.magic = WORD(0);

	/* First, some sanity checks */
	MUST_HAVE((s != NULL) && (pub_keys != NULL) && (m != NULL) && (adata != NULL), ret, err);
	/* We need at least one element in our batch data bags */
	MUST_HAVE((num > 0), ret, err);


	/* Zero init our local data */
	ret = local_memset(&crv_edwards, 0, sizeof(ec_edwards_crv)); EG(ret, err);
	ret = local_memset(hash, 0, sizeof(hash)); EG(ret, err);
	ret = local_memset(&A, 0, sizeof(aff_pt_edwards)); EG(ret, err);
	ret = local_memset(&R, 0, sizeof(aff_pt_edwards)); EG(ret, err);
	ret = local_memset(&_R_sum, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&_A_sum, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&_Tmp, 0, sizeof(prj_pt)); EG(ret, err);

	pub_key0 = pub_keys[0];
	MUST_HAVE((pub_key0 != NULL), ret, err);

	/* Get our hash mapping */
	ret = get_hash_by_type(hash_type, &hm); EG(ret, err);
	hsize = hm->digest_size;
	MUST_HAVE((hm != NULL), ret, err);

	/* Do we use the raw message or its PH(M) hashed version? */
#if defined(WITH_SIG_EDDSA25519)
	if(sig_type == EDDSA25519PH){
		use_message_pre_hash = 1;
		use_message_pre_hash_hsize = hsize;
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if(sig_type == EDDSA448PH){
		use_message_pre_hash = 1;
		/* NOTE: as per RFC8032, EDDSA448PH uses
		 * SHAKE256 with 64 bytes output.
		 */
		use_message_pre_hash_hsize = 64;
	}
#endif

	for(i = 0; i < num; i++){
		u8 siglen;
		const u8 *sig = NULL;

		ret = eddsa_pub_key_sanity_check(pub_keys[i]); EG(ret, err);

		/* Make things more readable */
		pub_key = pub_keys[i];

		/* Sanity check that all our public keys have the same parameters */
		MUST_HAVE((pub_key->params) == (pub_key0->params), ret, err);

		q = &(pub_key->params->ec_gen_order);
		shortw_curve = &(pub_key->params->ec_curve);
		alpha_montgomery = &(pub_key->params->ec_alpha_montgomery);
		gamma_montgomery = &(pub_key->params->ec_gamma_montgomery);
		alpha_edwards = &(pub_key->params->ec_alpha_edwards);
		gen_cofactor = &(pub_key->params->ec_gen_cofactor);
		pub_key_y = &(pub_key->y);
		key_type = pub_key->key_type;
		G = &(pub_key->params->ec_gen);

		/* Check the key type versus the algorithm */
		MUST_HAVE((key_type == sig_type), ret, err);

		if(i == 0){
			/* Initialize our sums to zero/point at infinity */
			ret = nn_init(&S_sum, 0); EG(ret, err);
			ret = prj_pt_init(&_R_sum, shortw_curve); EG(ret, err);
			ret = prj_pt_zero(&_R_sum); EG(ret, err);
			ret = prj_pt_init(&_A_sum, shortw_curve); EG(ret, err);
			ret = prj_pt_zero(&_A_sum); EG(ret, err);
			ret = nn_init(&z, 0); EG(ret, err);
			ret = nn_init(&h, 0); EG(ret, err);
		}

gen_z_again:
		/* Get a random z for randomizing the linear combination */
		ret = nn_get_random_len(&z, (hsize / 4)); EG(ret, err);
		ret = nn_iszero(&z, &iszero); EG(ret, err);
		if(iszero){
			goto gen_z_again;
		}

		/* Sanity check on hash types */
		MUST_HAVE((hash_type == get_eddsa_hash_type(key_type)), ret, err);

		/* Check given signature length is the expected one */
		siglen = s_len[i];
		sig = s[i];
		MUST_HAVE((siglen == EDDSA_SIGLEN(hsize)), ret, err);
		MUST_HAVE((siglen == (EDDSA_R_LEN(hsize) + EDDSA_S_LEN(hsize))), ret, err);

		/* Initialize the hash context */
		/* Since we call a callback, sanity check our mapping */
		ret = hash_mapping_callbacks_sanity_check(hm); EG(ret, err);
		ret = hm->hfunc_init(&h_ctx); EG(ret, err);
		ret = hm->hfunc_init(&h_ctx_pre_hash); EG(ret, err);
#if defined(WITH_SIG_EDDSA25519)
		if(key_type == EDDSA25519CTX){
			/* As per RFC8032, for EDDSA25519CTX the context SHOULD NOT be empty */
			MUST_HAVE((adata[i] != NULL), ret, err);
			ret = dom2(0, adata[i], adata_len[i], hm, &h_ctx); EG(ret, err);
		}
		if(key_type == EDDSA25519PH){
			ret = dom2(1, adata[i], adata_len[i], hm, &h_ctx); EG(ret, err);
		}
#endif
#if defined(WITH_SIG_EDDSA448)
		if(key_type == EDDSA448){
			ret = dom4(0, adata[i], adata_len[i], hm, &h_ctx); EG(ret, err);
		}
		if(key_type == EDDSA448PH){
			ret = dom4(1, adata[i], adata_len[i], hm, &h_ctx); EG(ret, err);
		}
#endif
		/* Import R and S values from signature buffer */
		/*******************************/
		/* Import R as an Edwards point */
		ret = curve_shortw_to_edwards(shortw_curve, &crv_edwards, alpha_montgomery,
					gamma_montgomery, alpha_edwards); EG(ret, err);
		/* NOTE: non canonical R are checked and rejected here */
		ret = eddsa_decode_point(&R, &crv_edwards, alpha_edwards, &sig[0],
				      EDDSA_R_LEN(hsize), key_type); EG(ret, err);
		dbg_ec_edwards_point_print("R", &R);
		/* Transfer our public point R to Weierstrass */
		ret = aff_pt_edwards_to_prj_pt_shortw(&R, shortw_curve, &_Tmp, alpha_edwards); EG(ret, err);
		/* Update the hash with the encoded R */
		ret = hm->hfunc_update(&h_ctx, &sig[0], EDDSA_R_LEN(hsize)); EG(ret, err);
		/* Multiply by z.
		 */
		ret = _prj_pt_unprotected_mult(&_Tmp, &z, &_Tmp); EG(ret, err);
		/* Add to the sum */
		ret = prj_pt_add(&_R_sum, &_R_sum, &_Tmp); EG(ret, err);

		/*******************************/
		/* Import S as an integer */
		ret = eddsa_decode_integer(&S, &sig[EDDSA_R_LEN(hsize)], EDDSA_S_LEN(hsize)); EG(ret, err);
		/* Reject S if it is not reduced modulo q */
		ret = nn_cmp(&S, q, &cmp); EG(ret, err);
		MUST_HAVE((cmp < 0), ret, err);
		dbg_nn_print("S", &S);

		/* Add z S to the sum */
		ret = nn_mul(&S, &S, &z); EG(ret, err);
		ret = nn_mod(&S, &S, q); EG(ret, err);
		ret = nn_mod_add(&S_sum, &S_sum, &S, q); EG(ret, err);

		/*******************************/
		/* Encode the public key
		 * NOTE: since we deal with a public key transfered to Weierstrass,
		 * encoding checking has been handled elsewhere.
		 */
		/* Reject the signature if the public key is one of small order points.
		 * We multiply by the cofactor: since this is a public verification,
		 * we use a basic double and add algorithm.
		 */
		ret = _prj_pt_unprotected_mult(&_Tmp, gen_cofactor, pub_key_y); EG(ret, err);
		/* Reject the signature if we have point at infinity here as this means
		 * that the public key is of small order.
		 */
		ret = prj_pt_iszero(&_Tmp, &iszero); EG(ret, err);
		MUST_HAVE((!iszero), ret, err);

		/* Transfer the public key to Edwards */
		ret = prj_pt_shortw_to_aff_pt_edwards(pub_key_y, &crv_edwards, &A, alpha_edwards); EG(ret, err);
		dbg_ec_edwards_point_print("A", &A);
		MUST_HAVE((EDDSA_R_LEN(hsize) <= sizeof(hash)), ret, err);
		/* NOTE: we use the hash buffer as a temporary buffer */
		ret = eddsa_encode_point(&A, alpha_edwards, hash, EDDSA_R_LEN(hsize), key_type); EG(ret, err);

		/* Update the hash with the encoded public key */
		ret = hm->hfunc_update(&h_ctx, hash, EDDSA_R_LEN(hsize)); EG(ret, err);
		/* Finish our computation of h = H(R || A || M) */
		/* Update the hash with the message or its hash for the PH versions */
		if(use_message_pre_hash == 1){
			ret = hm->hfunc_update(&h_ctx_pre_hash, m[i], m_len[i]); EG(ret, err);
			ret = hm->hfunc_finalize(&h_ctx_pre_hash, hash); EG(ret, err);
			MUST_HAVE((use_message_pre_hash_hsize <= hsize), ret, err);
			ret = hm->hfunc_update(&h_ctx, hash, use_message_pre_hash_hsize); EG(ret, err);
		}
		else{
			ret = hm->hfunc_update(&h_ctx, m[i], m_len[i]); EG(ret, err);
		}
		ret = hm->hfunc_finalize(&h_ctx, hash); EG(ret, err);
		dbg_buf_print("hash = H(R || A || PH(M))", hash, hsize);

		/* Import our hash as a NN and reduce it modulo q */
		ret = eddsa_decode_integer(&h, hash, hsize); EG(ret, err);
		ret = nn_mod(&h, &h, q); EG(ret, err);
		dbg_nn_print("h = ", &h);
#if defined(WITH_SIG_EDDSA448)
		if((key_type == EDDSA448) || (key_type == EDDSA448PH)){
			/* When dealing with EDDSA448, because of our 4-isogeny between Edwars448 and Ed448
			 * mapping base point to four times base point, we actually multiply our public key by 4 here
			 * to be inline with the other computations (the public key stored in Weierstrass )
			 */
			ret = nn_lshift(&h, &h, 2); EG(ret, err);
			ret = nn_mod(&h, &h, q); EG(ret, err);
		}
#endif

		/* Multiply by (z * h) mod q.
		 * NOTE: we use unprotected scalar multiplication since this is a
		 * public operation.
		 */
		ret = nn_mul(&z, &z, &h); EG(ret, err);
		ret = nn_mod(&z, &z, q); EG(ret, err);
		ret = _prj_pt_unprotected_mult(&_Tmp, &z, &_Tmp); EG(ret, err);
		/* Add to the sum */
		ret = prj_pt_add(&_A_sum, &_A_sum, &_Tmp); EG(ret, err);
	}

	/* Sanity check */
	MUST_HAVE((gen_cofactor != NULL) && (q != NULL) && (G != NULL), ret, err);

	/* Multiply the S sum by the cofactor */
	ret = nn_mul(&S_sum, &S_sum, gen_cofactor); EG(ret, err);
	ret = nn_mod(&S_sum, &S_sum, q); EG(ret, err);
	/* Negate it. NOTE: -x mod q is (q - x) mod q, i.e. (q - x) when x is reduced */
	ret = nn_mod_neg(&S_sum, &S_sum, q); EG(ret, err);
	/* Multiply this by the generator */
	ret = _prj_pt_unprotected_mult(&_Tmp, &S_sum, G); EG(ret, err);

	/* Multiply the R sum by the cofactor */
	ret = _prj_pt_unprotected_mult(&_R_sum, gen_cofactor, &_R_sum); EG(ret, err);

	/* Now add the three sums */
	ret = prj_pt_add(&_Tmp, &_Tmp, &_A_sum);
	ret = prj_pt_add(&_Tmp, &_Tmp, &_R_sum);

	/* Reject the signature if we do not have point at infinity here */
	ret = prj_pt_iszero(&_Tmp, &iszero); EG(ret, err);
	ret = iszero ? 0 : -1;

err:
	PTR_NULLIFY(q);
	PTR_NULLIFY(pub_key);
	PTR_NULLIFY(pub_key0);
	PTR_NULLIFY(shortw_curve);
	PTR_NULLIFY(alpha_montgomery);
	PTR_NULLIFY(gamma_montgomery);
	PTR_NULLIFY(alpha_edwards);
	PTR_NULLIFY(gen_cofactor);
	PTR_NULLIFY(pub_key_y);
	PTR_NULLIFY(G);

	ec_edwards_crv_uninit(&crv_edwards);
	aff_pt_edwards_uninit(&A);
	aff_pt_edwards_uninit(&R);
	prj_pt_uninit(&_R_sum);
	prj_pt_uninit(&_A_sum);
	prj_pt_uninit(&_Tmp);
	nn_uninit(&S);
	nn_uninit(&S_sum);
	nn_uninit(&z);
	nn_uninit(&h);

	return ret;

}

/*
 * The following batch verification uses the Bos-Coster algorithm, presented e.g. in
 * https://ed25519.cr.yp.to/ed25519-20110705.pdf
 *
 * The Bos-Coster algorithm allows to optimize a sum of scalar multiplications using
 * addition chains.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET static int _eddsa_verify_batch(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
			       const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
                               hash_alg_type hash_type, const u8 **adata, const u16 *adata_len,
                               verify_batch_scratch_pad *scratch_pad_area, u32 *scratch_pad_area_len)
{
	nn_src_t q = NULL;
	ec_edwards_crv crv_edwards;
	aff_pt_edwards R, A;
	prj_pt_src_t G = NULL;
	nn S, z;
	u8 hash[MAX_DIGEST_SIZE];
	int ret, iszero, cmp;
	u16 hsize;
	const ec_pub_key *pub_key, *pub_key0;
	ec_shortw_crv_src_t shortw_curve;
	fp_src_t alpha_montgomery;
	fp_src_t gamma_montgomery;
	fp_src_t alpha_edwards;
	nn_src_t gen_cofactor = NULL;
	prj_pt_src_t pub_key_y;
	hash_context h_ctx;
	hash_context h_ctx_pre_hash;
	u8 use_message_pre_hash = 0;
	u16 use_message_pre_hash_hsize = 0;
	const hash_mapping *hm;
	ec_alg_type key_type = UNKNOWN_ALG;
	/* NN numbers and points pointers */
	verify_batch_scratch_pad *elements = scratch_pad_area;
	u32 i;
	u64 expected_len;
	bitcnt_t q_bit_len = 0;

	S.magic = z.magic = crv_edwards.magic = WORD(0);

	/* First, some sanity checks */
	MUST_HAVE((s != NULL) && (pub_keys != NULL) && (m != NULL) && (adata != NULL), ret, err);
	MUST_HAVE((scratch_pad_area_len != NULL), ret, err);
	MUST_HAVE(((2 * num) >= num), ret, err);
	MUST_HAVE(((2 * num) + 1) >= num, ret, err);

	/* In oder to apply the algorithm, we must have at least two
	 * elements to verify. If this is not the case, we fallback to
	 * the regular "no memory" version.
	 */
	if(num <= 1){
		if(scratch_pad_area == NULL){
			/* We do not require any memory in this case */
			(*scratch_pad_area_len) = 0;
			ret = 0;
			goto err;
		}
		else{
	                ret = _eddsa_verify_batch_no_memory(s, s_len, pub_keys, m, m_len, num, sig_type,
		                                            hash_type, adata, adata_len);
			goto err;
		}
	}

	expected_len = ((2 * num) + 1) * sizeof(verify_batch_scratch_pad);
	MUST_HAVE((expected_len < 0xffffffff), ret, err);

	if(scratch_pad_area == NULL){
		/* Return the needed size: we need to keep track of (2 * num) + 1 NN numbers
		 * and (2 * num) + 1 projective points, plus (2 * num) + 1 indices
		 */
		(*scratch_pad_area_len) = (u32)expected_len;
		ret = 0;
		goto err;
	}
	else{
		MUST_HAVE((*scratch_pad_area_len) >= expected_len, ret, err);
	}

	/********************************************/
	/****** Initialize elements *****************/
	/* Zero init our local data */
	ret = local_memset(&crv_edwards, 0, sizeof(ec_edwards_crv)); EG(ret, err);
	ret = local_memset(hash, 0, sizeof(hash)); EG(ret, err);
	ret = local_memset(&A, 0, sizeof(aff_pt_edwards)); EG(ret, err);
	ret = local_memset(&R, 0, sizeof(aff_pt_edwards)); EG(ret, err);

	pub_key0 = pub_keys[0];
	MUST_HAVE((pub_key0 != NULL), ret, err);

	/* Get our hash mapping */
	ret = get_hash_by_type(hash_type, &hm); EG(ret, err);
	hsize = hm->digest_size;
	MUST_HAVE((hm != NULL), ret, err);

	/* Do we use the raw message or its PH(M) hashed version? */
#if defined(WITH_SIG_EDDSA25519)
	if(sig_type == EDDSA25519PH){
		use_message_pre_hash = 1;
		use_message_pre_hash_hsize = hsize;
	}
#endif
#if defined(WITH_SIG_EDDSA448)
	if(sig_type == EDDSA448PH){
		use_message_pre_hash = 1;
		/* NOTE: as per RFC8032, EDDSA448PH uses
		 * SHAKE256 with 64 bytes output.
		 */
		use_message_pre_hash_hsize = 64;
	}
#endif

	/* Compute our original numbers and points */
	MUST_HAVE((num >= 1), ret, err);
	for(i = 0; i < num; i++){
		u8 siglen;
		const u8 *sig = NULL;

		ret = eddsa_pub_key_sanity_check(pub_keys[i]); EG(ret, err);

		/* Make things more readable */
		pub_key = pub_keys[i];

		/* Sanity check that all our public keys have the same parameters */
		MUST_HAVE((pub_key->params) == (pub_key0->params), ret, err);

		q = &(pub_key->params->ec_gen_order);
		shortw_curve = &(pub_key->params->ec_curve);
		alpha_montgomery = &(pub_key->params->ec_alpha_montgomery);
		gamma_montgomery = &(pub_key->params->ec_gamma_montgomery);
		alpha_edwards = &(pub_key->params->ec_alpha_edwards);
		gen_cofactor = &(pub_key->params->ec_gen_cofactor);
		pub_key_y = &(pub_key->y);
		key_type = pub_key->key_type;
		G = &(pub_key->params->ec_gen);
		q_bit_len = pub_key->params->ec_gen_order_bitlen;

		/* Check the key type versus the algorithm */
		MUST_HAVE((key_type == sig_type), ret, err);

		if(i == 0){
			/* Initialize our numbers */
			ret = nn_init(&z, 0); EG(ret, err);
			ret = nn_init(&S, 0); EG(ret, err);
			ret = nn_init(&elements[(2 * num)].number, 0); EG(ret, err);
			ret = _prj_pt_unprotected_mult(&elements[(2 * num)].point, gen_cofactor, G); EG(ret, err);
		}

gen_z_again:
		/* Get a random z for randomizing the linear combination */
		ret = nn_get_random_len(&z, (hsize / 4)); EG(ret, err);
		ret = nn_iszero(&z, &iszero); EG(ret, err);
		if(iszero){
			goto gen_z_again;
		}

		/* Sanity check on hash types */
		MUST_HAVE((hash_type == get_eddsa_hash_type(key_type)), ret, err);

		/* Check given signature length is the expected one */
		siglen = s_len[i];
		sig = s[i];
		MUST_HAVE((siglen == EDDSA_SIGLEN(hsize)), ret, err);
		MUST_HAVE((siglen == (EDDSA_R_LEN(hsize) + EDDSA_S_LEN(hsize))), ret, err);

		/* Initialize the hash context */
		/* Since we call a callback, sanity check our mapping */
		ret = hash_mapping_callbacks_sanity_check(hm); EG(ret, err);
		ret = hm->hfunc_init(&h_ctx); EG(ret, err);
		ret = hm->hfunc_init(&h_ctx_pre_hash); EG(ret, err);
#if defined(WITH_SIG_EDDSA25519)
		if(key_type == EDDSA25519CTX){
			/* As per RFC8032, for EDDSA25519CTX the context SHOULD NOT be empty */
			MUST_HAVE((adata[i] != NULL), ret, err);
			ret = dom2(0, adata[i], adata_len[i], hm, &h_ctx); EG(ret, err);
		}
		if(key_type == EDDSA25519PH){
			ret = dom2(1, adata[i], adata_len[i], hm, &h_ctx); EG(ret, err);
		}
#endif
#if defined(WITH_SIG_EDDSA448)
		if(key_type == EDDSA448){
			ret = dom4(0, adata[i], adata_len[i], hm, &h_ctx); EG(ret, err);
		}
		if(key_type == EDDSA448PH){
			ret = dom4(1, adata[i], adata_len[i], hm, &h_ctx); EG(ret, err);
		}
#endif
		/* Import R and S values from signature buffer */
		/*******************************/
		/* Import R as an Edwards point */
		ret = curve_shortw_to_edwards(shortw_curve, &crv_edwards, alpha_montgomery,
					gamma_montgomery, alpha_edwards); EG(ret, err);
		/* NOTE: non canonical R are checked and rejected here */
		ret = eddsa_decode_point(&R, &crv_edwards, alpha_edwards, &sig[0],
				      EDDSA_R_LEN(hsize), key_type); EG(ret, err);
		dbg_ec_edwards_point_print("R", &R);
		/* Transfer our public point R to Weierstrass */
		ret = aff_pt_edwards_to_prj_pt_shortw(&R, shortw_curve, &elements[i].point, alpha_edwards); EG(ret, err);
		/* Update the hash with the encoded R */
		ret = hm->hfunc_update(&h_ctx, &sig[0], EDDSA_R_LEN(hsize)); EG(ret, err);
		/* Store 8 * z in our number to be multiplied with R */
		ret = nn_init(&elements[i].number, 0); EG(ret, err);
		ret = nn_mul(&elements[i].number, gen_cofactor, &z); EG(ret, err);
		ret = nn_mod(&elements[i].number, &elements[i].number, q); EG(ret, err);

		/*******************************/
		/* Import S as an integer */
		ret = eddsa_decode_integer(&S, &sig[EDDSA_R_LEN(hsize)], EDDSA_S_LEN(hsize)); EG(ret, err);
		/* Reject S if it is not reduced modulo q */
		ret = nn_cmp(&S, q, &cmp); EG(ret, err);
		MUST_HAVE((cmp < 0), ret, err);
		dbg_nn_print("S", &S);

		/* Add (- z S) to the sum */
		ret = nn_mul(&S, &S, &z); EG(ret, err);
		ret = nn_mod(&S, &S, q); EG(ret, err);
		ret = nn_mod_neg(&S, &S, q); EG(ret, err); /* Negate S */
		ret = nn_mod_add(&elements[(2 * num)].number, &elements[(2 * num)].number, &S, q); EG(ret, err);

		/*******************************/
		/* Encode the public key
		 * NOTE: since we deal with a public key transfered to Weierstrass,
		 * encoding checking has been handled elsewhere.
		 */
		/* Reject the signature if the public key is one of small order points.
		 * We multiply by the cofactor: since this is a public verification,
		 * we use a basic double and add algorithm.
		 */
		ret = _prj_pt_unprotected_mult(&elements[num + i].point, gen_cofactor, pub_key_y); EG(ret, err);
		/* Reject the signature if we have point at infinity here as this means
		 * that the public key is of small order.
		 */
		ret = prj_pt_iszero(&elements[num + i].point, &iszero); EG(ret, err);
		MUST_HAVE((!iszero), ret, err);

		/* Transfer the public key to Edwards */
		ret = prj_pt_shortw_to_aff_pt_edwards(pub_key_y, &crv_edwards, &A, alpha_edwards); EG(ret, err);
		dbg_ec_edwards_point_print("A", &A);
		MUST_HAVE((EDDSA_R_LEN(hsize) <= sizeof(hash)), ret, err);
		/* NOTE: we use the hash buffer as a temporary buffer */
		ret = eddsa_encode_point(&A, alpha_edwards, hash, EDDSA_R_LEN(hsize), key_type); EG(ret, err);

		/* Update the hash with the encoded public key */
		ret = hm->hfunc_update(&h_ctx, hash, EDDSA_R_LEN(hsize)); EG(ret, err);
		/* Finish our computation of h = H(R || A || M) */
		/* Update the hash with the message or its hash for the PH versions */
		if(use_message_pre_hash == 1){
			ret = hm->hfunc_update(&h_ctx_pre_hash, m[i], m_len[i]); EG(ret, err);
			ret = hm->hfunc_finalize(&h_ctx_pre_hash, hash); EG(ret, err);
			MUST_HAVE((use_message_pre_hash_hsize <= hsize), ret, err);
			ret = hm->hfunc_update(&h_ctx, hash, use_message_pre_hash_hsize); EG(ret, err);
		}
		else{
			ret = hm->hfunc_update(&h_ctx, m[i], m_len[i]); EG(ret, err);
		}
		ret = hm->hfunc_finalize(&h_ctx, hash); EG(ret, err);
		dbg_buf_print("hash = H(R || A || PH(M))", hash, hsize);

		/* Import our hash as a NN and reduce it modulo q */
		ret = eddsa_decode_integer(&elements[num + i].number, hash, hsize); EG(ret, err);
		ret = nn_mod(&elements[num + i].number, &elements[num + i].number, q); EG(ret, err);
		dbg_nn_print("h = ", &elements[num + i].number);
#if defined(WITH_SIG_EDDSA448)
		if((key_type == EDDSA448) || (key_type == EDDSA448PH)){
			/* When dealing with EDDSA448, because of our 4-isogeny between Edwars448 and Ed448
			 * mapping base point to four times base point, we actually multiply our public key by 4 here
			 * to be inline with the other computations (the public key stored in Weierstrass )
			 */
			ret = nn_lshift(&elements[num + i].number, &elements[num + i].number, 2); EG(ret, err);
			ret = nn_mod(&elements[num + i].number, &elements[num + i].number, q); EG(ret, err);
		}
#endif
		/* Compute by (z * h) mod q.
		 */
		ret = nn_mul(&elements[num + i].number, &elements[num + i].number, &z); EG(ret, err);
		ret = nn_mod(&elements[num + i].number, &elements[num + i].number, q); EG(ret, err);
	}

	/* Sanity check */
	MUST_HAVE((gen_cofactor != NULL) && (q != NULL) && (G != NULL) && (q_bit_len != 0), ret, err);

	/********************************************/
	/****** Bos-Coster algorithm ****************/
	ret = ec_verify_bos_coster(elements, (2 * num) + 1, q_bit_len);
        if(ret){
                if(ret == -2){
                        /* In case of Bos-Coster time out, we fall back to the
                         * slower regular batch verification.
                         */
                        ret = _eddsa_verify_batch_no_memory(s, s_len, pub_keys, m, m_len, num, sig_type,
                                                            hash_type, adata, adata_len); EG(ret, err);
                }
                goto err;
        }

	/* The first element should contain the sum: it should
	 * be equal to zero. Reject the signature if this is not
	 * the case.
	 */
	ret = prj_pt_iszero(&elements[elements[0].index].point, &iszero); EG(ret, err);
	ret = iszero ? 0 : -1;

err:
	PTR_NULLIFY(q);
	PTR_NULLIFY(pub_key);
	PTR_NULLIFY(pub_key0);
	PTR_NULLIFY(shortw_curve);
	PTR_NULLIFY(alpha_montgomery);
	PTR_NULLIFY(gamma_montgomery);
	PTR_NULLIFY(alpha_edwards);
	PTR_NULLIFY(gen_cofactor);
	PTR_NULLIFY(pub_key_y);
	PTR_NULLIFY(G);
	PTR_NULLIFY(elements);

	/* Unitialize all our scratch_pad_area */
	if((scratch_pad_area != NULL) && (scratch_pad_area_len != NULL)){
		IGNORE_RET_VAL(local_memset((u8*)scratch_pad_area, 0, (*scratch_pad_area_len)));
	}

	ec_edwards_crv_uninit(&crv_edwards);
	aff_pt_edwards_uninit(&A);
	aff_pt_edwards_uninit(&R);
	nn_uninit(&S);
	nn_uninit(&z);

	return ret;
}

int eddsa_verify_batch(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
                       const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
                       hash_alg_type hash_type, const u8 **adata, const u16 *adata_len,
                       verify_batch_scratch_pad *scratch_pad_area, u32 *scratch_pad_area_len)
{
        int ret;

        if(scratch_pad_area != NULL){
                MUST_HAVE((scratch_pad_area_len != NULL), ret, err);
                ret = _eddsa_verify_batch(s, s_len, pub_keys, m, m_len, num, sig_type,
                                          hash_type, adata, adata_len,
                                          scratch_pad_area, scratch_pad_area_len); EG(ret, err);
        }
        else{
                ret = _eddsa_verify_batch_no_memory(s, s_len, pub_keys, m, m_len, num, sig_type,
                                                    hash_type, adata, adata_len); EG(ret, err);
        }

err:
        return ret;
}

#else /* !(defined(WITH_SIG_EDDSA25519) || defined(WITH_SIG_EDDSA448)) */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* defined(WITH_SIG_EDDSA25519) || defined(WITH_SIG_EDDSA448) */
