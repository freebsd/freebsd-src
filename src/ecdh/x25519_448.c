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
#if defined(WITH_X25519) || defined(WITH_X448)

/*
 * XXX: X25519 and X448 are incompatible with small stack devices for now ...
 */
#if defined(USE_SMALL_STACK)
#error "Error: X25519 and X448 are incompatible with USE_SMALL_STACK (devices low on memory)"
#endif

#if defined(WITH_X25519) && !defined(WITH_CURVE_WEI25519)
#error "Error: X25519 needs curve WEI25519 to be defined! Please define it in libecc config file"
#endif
#if defined(WITH_X448) && !defined(WITH_CURVE_WEI448)
#error "Error: X448 needs curve WEI448 to be defined! Please define it in libecc config file"
#endif

#include <libecc/ecdh/x25519_448.h>

#include <libecc/curves/curves.h>
#include <libecc/sig/ec_key.h>
#include <libecc/utils/utils.h>
#include <libecc/fp/fp_sqrt.h>

/* For randomness source */
#include <libecc/external_deps/rand.h>

/* This module mainly implements the X25519 and X448 functions strictly as defined in
 * RFC7748.
 */


/* Scalar clamping/decoding
 *
 * NOTE: the scalar encoding is mainly here to ensure that it is of the form
 * 2^254 plus eight times a value between 0 and 2^251 - 1 inclusive for X25519
 * (2^447 plus four times a value between 0 and 2^445 - 1 inclusive for X448).
 *
 * This ensures "clearing the cofactor" to avoid small subgroup attacks as well
 * as setting the scalar MSB to avoid timing/SCA attacks on scalar multiplication.
 * These two desirable properties are part of the rationale behind X25519/X448).
 */
ATTRIBUTE_WARN_UNUSED_RET static int decode_scalar(u8 *scalar_decoded, const u8 *scalar, u8 len)
{
        int ret;
        u8 i;

        /* Aliasing is not supported */
        MUST_HAVE((scalar != scalar_decoded), ret, err);
	/* Zero length is not accepted */
	MUST_HAVE((len > 0), ret, err);

	/* Endianness swapping */
        for(i = 0; i < len; i++){
                scalar_decoded[len - 1 - i] = scalar[i];
        }
	if(len == X25519_SIZE){
		/* Curve25519 */
		scalar_decoded[len - 1] &= 248;
        	scalar_decoded[0]       &= 127;
		scalar_decoded[0]       |= 64;
	}
	else if(len == X448_SIZE){
		/* Curve448 */
		scalar_decoded[len - 1] &= 252;
		scalar_decoded[0]       |= 128;
	}
	else{
		/* Error, unknown type */
		ret = -1;
		goto err;
	}

        ret = 0;

err:
        return ret;
}

/* U coordinate decoding, mainly endianness swapping  */
ATTRIBUTE_WARN_UNUSED_RET static int decode_u_coordinate(u8 *u_decoded, const u8 *u, u8 len)
{
	int ret;
	u8 i;

	/* Aliasing is not supported */
	MUST_HAVE((u != u_decoded), ret, err);
	/* Zero length is not accepted */
	MUST_HAVE((len > 0), ret, err);

	for(i = 0; i < len; i++){
		u_decoded[len - 1 - i] = u[i];
        }

	ret = 0;

err:
	return ret;
}

/* U coordinate encoding, mainly endianness swapping */
ATTRIBUTE_WARN_UNUSED_RET static int encode_u_coordinate(u8 *u_encoded, const u8 *u, u8 len)
{
	return decode_u_coordinate(u_encoded, u, len);
}


/* Find V coordinate from U coordinate on Curve25519 or Curve448 */
ATTRIBUTE_WARN_UNUSED_RET static int compute_v_from_u(fp_src_t u, fp_t v, ec_montgomery_crv_src_t crv)
{
	int ret;
	fp tmp;

	tmp.magic = 0;

	ret = aff_pt_montgomery_v_from_u(v, &tmp, u, crv);
	/* NOTE: this square root is possibly non-existing if the
	 * u coordinate is on the quadratic twist of the curve.
	 * An error is returned in this case.
	 */

	fp_uninit(&tmp);

	return ret;
}


/*
 * This is the core computation of X25519 and X448.
 *
 * NOTE: the user of this primitive should be warned and aware that is is not fully compliant with the
 * RFC7748 description as u coordinates on the quadratic twist of the curve are rejected as well
 * as non canonical u.
 * See the explanations in the implementation of the function for more context and explanations.
 */
ATTRIBUTE_WARN_UNUSED_RET static int x25519_448_core(const u8 *k, const u8 *u, u8 *res, u8 len)
{
	int ret, iszero, loaded, cmp;
	/* Note: our local variables holding scalar and coordinate have the maximum size
	 * (X448 size if it is defined, X25519 else).
	 */
#if defined(WITH_X448)
	u8 k_[X448_SIZE], u_[X448_SIZE];
#else
	u8 k_[X25519_SIZE], u_[X25519_SIZE];
#endif
	aff_pt_montgomery _Tmp;
	prj_pt Q;
	ec_montgomery_crv montgomery_curve;
	ec_params shortw_curve_params;
	ec_shortw_crv_src_t shortw_curve;
	fp_src_t alpha_montgomery;
	fp_src_t gamma_montgomery;
	nn scalar;
	nn_t v_coord_nn;
	fp_t u_coord, v_coord;
	nn_t cofactor;

	_Tmp.magic = montgomery_curve.magic = Q.magic = WORD(0);
	scalar.magic = WORD(0);

	MUST_HAVE((k != NULL) && (u != NULL) && (res != NULL), ret, err);
	/* Sanity check on sizes */
	MUST_HAVE(((len == X25519_SIZE) || (len == X448_SIZE)), ret, err);
	MUST_HAVE(((sizeof(k_) >= len) && (sizeof(u_) >= len)), ret, err);

	/* First of all, we clamp and decode the scalar and u */
	ret = decode_scalar(k_, k, len); EG(ret, err);
	ret = decode_u_coordinate(u_, u, len); EG(ret, err);

	/* Import curve parameters */
	loaded = 0;
#if defined(WITH_X25519)
	if(len == X25519_SIZE){
		ret = import_params(&shortw_curve_params, &wei25519_str_params); EG(ret, err);
		loaded = 1;
	}
#endif
#if defined(WITH_X448)
	if(len == X448_SIZE){
		ret = import_params(&shortw_curve_params, &wei448_str_params); EG(ret, err);
		loaded = 1;
	}
#endif
	/* Sanity check that we have loaded our curve parameters */
	MUST_HAVE((loaded == 1), ret, err);

	/* Make things more readable */
	shortw_curve = &(shortw_curve_params.ec_curve);
	cofactor = &(shortw_curve_params.ec_gen_cofactor);
	alpha_montgomery = &(shortw_curve_params.ec_alpha_montgomery);
	gamma_montgomery = &(shortw_curve_params.ec_gamma_montgomery);
	/* NOTE: we use the projective point Q Fp values as temporary
	 * space to save some stack here.
	 */
	u_coord = &(Q.X);
	v_coord = &(Q.Y);
	v_coord_nn = &(v_coord->fp_val);

	/* Get the isogenic Montgomery curve */
	ret = curve_shortw_to_montgomery(shortw_curve, &montgomery_curve, alpha_montgomery,
			gamma_montgomery); EG(ret, err);

	/* Import the u coordinate as a big integer and Fp element */
	ret = nn_init_from_buf(v_coord_nn, u_, len); EG(ret, err);
	/* Reject non canonical u values.
	 * NOTE: we use v here as a temporary value.
	 */
	ret = nn_cmp(v_coord_nn, &(montgomery_curve.A.ctx->p), &cmp); EG(ret, err);
	MUST_HAVE((cmp < 0), ret, err);
	/* Now initialize u as Fp element with the reduced value */
	ret = fp_init(u_coord, montgomery_curve.A.ctx); EG(ret, err);
	ret = fp_set_nn(u_coord, v_coord_nn); EG(ret, err);

	/* Compute the v coordinate from u */
	ret = compute_v_from_u(u_coord, v_coord, &montgomery_curve); EG(ret, err);
	/* NOTE: since we use isogenies of the Curve25519/448, we must stick to points
	 * belonging to this curve. Since not all u coordinates provide a v coordinate
	 * (i.e. a square residue from the curve formula), the computation above can trigger an error.
	 * When this is the case, this means that the u coordinate is on the quadtratic twist of
	 * the Montgomery curve (which is a secure curve by design here). We could perform computations
	 * on an isogenic curve of this twist, however we choose to return an error instead.
	 *
	 * Although this is not compliant with the Curve2551/448 original spirit (that accepts any u
	 * coordinate thanks to the x-coordinate only computations with the Montgomery Ladder),
	 * we emphasize here that in the key exchange defined in RFC7748 all the exchanged points
	 * (i.e. public keys) are derived from base points that are on the curve and not on its twist, meaning
	 * that all the exchanged u coordinates should belong to the curve. Diverging from this behavior would
	 * suggest that an attacker is trying to inject other values, and we are safe to reject them in the
	 * context of Diffie-Hellman based key exchange as defined in RFC7748.
	 *
	 * On the other hand, the drawback of rejecting u coordinates on the quadratic twist is that
	 * using the current X25519/448 primitive in other contexts than RFC7748 Diffie-Hellman could be
	 * limited and non interoperable with other implementations of this primive. Another issue is that
	 * this specific divergence exposes our implementation to be distinguishable from other standard ones
	 * in a "black box" analysis context, which is generally not very desirable even if no real security
	 * issue is induced.
	 */

	/* Get the affine point in Montgomery */
	ret = aff_pt_montgomery_init_from_coords(&_Tmp, &montgomery_curve, u_coord, v_coord); EG(ret, err);
	/* Transfer from Montgomery to short Weierstrass using the isogeny */
	ret = aff_pt_montgomery_to_prj_pt_shortw(&_Tmp, shortw_curve, &Q); EG(ret, err);

	/*
	 * Reject small order public keys: while this is not a strict requirement of RFC7748, there is no
	 * good reason to accept these weak values!
	 */
	ret = check_prj_pt_order(&Q, cofactor, PUBLIC_PT, &cmp); EG(ret, err);
	MUST_HAVE((!cmp), ret, err);

	/* Import the scalar as big number NN value */
	ret = nn_init_from_buf(&scalar, k_, len); EG(ret, err);
	/* Now proceed with the scalar multiplication */
#ifdef USE_SIG_BLINDING
	ret = prj_pt_mul_blind(&Q, &scalar, &Q); EG(ret, err);
#else
	ret = prj_pt_mul(&Q, &scalar, &Q); EG(ret, err);
#endif

	/* Transfer back from short Weierstrass to Montgomery using the isogeny */
	ret = prj_pt_shortw_to_aff_pt_montgomery(&Q, &montgomery_curve, &_Tmp); EG(ret, err);

	/* Reject the all zero output */
	ret = fp_iszero(&(_Tmp.u), &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	/* Now export the resulting u coordinate ... */
	ret = fp_export_to_buf(u_, len, &(_Tmp.u)); EG(ret, err);
	/* ... and encode it in the output */
	ret = encode_u_coordinate(res, u_, len);

err:
	IGNORE_RET_VAL(local_memset(u_, 0, sizeof(u_)));
	IGNORE_RET_VAL(local_memset(k_, 0, sizeof(k_)));
	IGNORE_RET_VAL(local_memset(&shortw_curve_params, 0, sizeof(shortw_curve_params)));

	nn_uninit(&scalar);
	aff_pt_montgomery_uninit(&_Tmp);
	prj_pt_uninit(&Q);
	ec_montgomery_crv_uninit(&montgomery_curve);

	PTR_NULLIFY(shortw_curve);
	PTR_NULLIFY(alpha_montgomery);
	PTR_NULLIFY(gamma_montgomery);
	PTR_NULLIFY(u_coord);
	PTR_NULLIFY(v_coord);
	PTR_NULLIFY(v_coord_nn);
	PTR_NULLIFY(cofactor);

        return ret;
}

ATTRIBUTE_WARN_UNUSED_RET static int x25519_448_gen_priv_key(u8 *priv_key, u8 len)
{
	int ret;

	MUST_HAVE((priv_key != NULL), ret, err);
	MUST_HAVE(((len == X25519_SIZE) || (len == X448_SIZE)), ret, err);

	/* Generating a private key consists in generating a random byte string */
	ret = get_random(priv_key, len);

err:
	return ret;
}


ATTRIBUTE_WARN_UNUSED_RET static int x25519_448_init_pub_key(const u8 *priv_key, u8 *pub_key, u8 len)
{
	int ret;

	MUST_HAVE((priv_key != NULL) && (pub_key != NULL), ret, err);
	MUST_HAVE(((len == X25519_SIZE) || (len == X448_SIZE)), ret, err);

	/* Computing the public key is x25519(priv_key, 9) or x448(priv_key, 5)
	 *
	 * NOTE: although we could optimize and accelerate the computation of the public
	 * key by skipping the Montgomery to Weierstrass mapping (as the base point on the two
	 * isomorphic curves are known), we rather use the regular x25519_448_core primitive
	 * as it has the advantages of keeping the code clean and simple (and the performance
	 * cost is not so expensive as the core scalar multiplication will take most of the
	 * cycles ...).
	 *
	 */
	if(len == X25519_SIZE){
		u8 u[X25519_SIZE];

		ret = local_memset(u, 0, sizeof(u)); EG(ret, err);
		/* X25519 uses the base point with x-coordinate = 0x09 */
		u[0] = 0x09;
		ret = x25519_448_core(priv_key, u, pub_key, len);
	}
	else if(len == X448_SIZE){
		u8 u[X448_SIZE];

		ret = local_memset(u, 0, sizeof(u)); EG(ret, err);
		/* X448 uses the base point with x-coordinate = 0x05 */
		u[0] = 0x05;
		ret = x25519_448_core(priv_key, u, pub_key, len);
	}
	else{
		ret = -1;
	}
err:
	return ret;
}

ATTRIBUTE_WARN_UNUSED_RET static int x25519_448_derive_secret(const u8 *priv_key, const u8 *peer_pub_key, u8 *shared_secret, u8 len)
{
	int ret;

	MUST_HAVE((priv_key != NULL) && (peer_pub_key != NULL) && (shared_secret != NULL), ret, err);
	MUST_HAVE(((len == X25519_SIZE) || (len == X448_SIZE)), ret, err);

	/* Computing the shared secret is x25519(priv_key, peer_pub_key) or x448(priv_key, peer_pub_key) */
	ret = x25519_448_core(priv_key, peer_pub_key, shared_secret, len);

err:
	return ret;
}


#if defined(WITH_X25519)
/* The X25519 function as specified in RFC7748.
 *
 * NOTE: we use isogenies between Montgomery Curve25519 and short Weierstrass
 * WEI25519 to perform the Elliptic Curves computations.
 */
int x25519(const u8 k[X25519_SIZE], const u8 u[X25519_SIZE], u8 res[X25519_SIZE])
{
	return x25519_448_core(k, u, res, X25519_SIZE);
}

int x25519_gen_priv_key(u8 priv_key[X25519_SIZE])
{
	return x25519_448_gen_priv_key(priv_key, X25519_SIZE);
}

int x25519_init_pub_key(const u8 priv_key[X25519_SIZE], u8 pub_key[X25519_SIZE])
{
	return x25519_448_init_pub_key(priv_key, pub_key, X25519_SIZE);
}

int x25519_derive_secret(const u8 priv_key[X25519_SIZE], const u8 peer_pub_key[X25519_SIZE], u8 shared_secret[X25519_SIZE])
{
	return x25519_448_derive_secret(priv_key, peer_pub_key, shared_secret, X25519_SIZE);
}
#endif

#if defined(WITH_X448)
/* The X448 function as specified in RFC7748.
 *
 * NOTE: we use isogenies between Montgomery Curve448 and short Weierstrass
 * WEI448 to perform the Elliptic Curves computations.
 */
int x448(const u8 k[X448_SIZE], const u8 u[X448_SIZE], u8 res[X448_SIZE])
{
	return x25519_448_core(k, u, res, X448_SIZE);
}

int x448_gen_priv_key(u8 priv_key[X448_SIZE])
{
	return x25519_448_gen_priv_key(priv_key, X448_SIZE);
}

int x448_init_pub_key(const u8 priv_key[X448_SIZE], u8 pub_key[X448_SIZE])
{
	return x25519_448_init_pub_key(priv_key, pub_key, X448_SIZE);
}

int x448_derive_secret(const u8 priv_key[X448_SIZE], const u8 peer_pub_key[X448_SIZE], u8 shared_secret[X448_SIZE])
{
	return x25519_448_derive_secret(priv_key, peer_pub_key, shared_secret, X448_SIZE);
}
#endif

#else /* !(defined(WITH_X25519) || defined(WITH_X448)) */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;

#endif /* defined(WITH_X25519) || defined(WITH_X448) */
