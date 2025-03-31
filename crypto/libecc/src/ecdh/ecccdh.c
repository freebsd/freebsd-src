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
#if defined(WITH_ECCCDH)

#include <libecc/ecdh/ecccdh.h>

/*
 * This module implements the "Elliptic Curve Cryptography Cofactor Diffie-Hellman (ECC CDH)
 * Primitive" as described in section 5.7.1.2 of the NIST SP 800-56A Rev. 3 standard.
 *
 */

/*
 * Get the size of the shared secret associated to the curve parameters.
 */
int ecccdh_shared_secret_size(const ec_params *params, u8 *size)
{
	int ret;

	MUST_HAVE((params != NULL) && (size != NULL), ret, err);
	MUST_HAVE((BYTECEIL(params->ec_fp.p_bitlen) <= 255), ret, err);

	(*size) = (u8)(BYTECEIL(params->ec_fp.p_bitlen));
	ret = 0;

err:
	return ret;
}

/*
 * Get the size of the serialized public key associated to the curve parameters.
 */
int ecccdh_serialized_pub_key_size(const ec_params *params, u8 *size)
{
	int ret;

	MUST_HAVE((params != NULL) && (size != NULL), ret, err);
	MUST_HAVE(((2 * BYTECEIL(params->ec_fp.p_bitlen)) <= 255), ret, err);

	(*size) = (u8)(2 * BYTECEIL(params->ec_fp.p_bitlen));
	ret = 0;

err:
	return ret;
}


/*
 * Initialize ECCCDH public key from an initialized private key.
 */
int ecccdh_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv)
{
	prj_pt_src_t G;
	int ret, cmp;
	nn_src_t q;

	MUST_HAVE((out_pub != NULL), ret, err);

	/* Zero init public key to be generated */
	ret = local_memset(out_pub, 0, sizeof(ec_pub_key)); EG(ret, err);

	ret = priv_key_check_initialized_and_type(in_priv, ECCCDH); EG(ret, err);
	q = &(in_priv->params->ec_gen_order);

	/* Sanity check on key compliance */
	MUST_HAVE((!nn_cmp(&(in_priv->x), q, &cmp)) && (cmp < 0), ret, err);

	/* Y = xG */
	G = &(in_priv->params->ec_gen);
	/* Use blinding when computing point scalar multiplication */
	ret = prj_pt_mul_blind(&(out_pub->y), &(in_priv->x), G); EG(ret, err);

	out_pub->key_type = ECCCDH;
	out_pub->params = in_priv->params;
	out_pub->magic = PUB_KEY_MAGIC;

err:
        return ret;
}

/*
 * Generate a key pair for ECCCDH given curve parameters as input.
 */
int ecccdh_gen_key_pair(ec_key_pair *kp, const ec_params *params)
{
	int ret;

	MUST_HAVE((kp != NULL) && (params != NULL), ret, err);

	/* Use our generic key pair generation primitive */
	kp->priv_key.magic = PRIV_KEY_MAGIC;
	kp->priv_key.key_type = ECCCDH;
	kp->priv_key.params = params;
	ret = generic_gen_priv_key(&(kp->priv_key)); EG(ret, err);

	/* Then, derive the public key */
	ret = ecccdh_init_pub_key(&(kp->pub_key), &(kp->priv_key));

err:
	/* If we have failed our generation, uninitialize
	 * the key pair.
	 */
	if(ret && (kp != NULL)){
		IGNORE_RET_VAL(local_memset(kp, 0, sizeof(ec_key_pair)));
	}
	return ret;
}

/*
 * Create a key pair from a serialized private key.
 */
int ecccdh_import_key_pair_from_priv_key_buf(ec_key_pair *kp, const ec_params *params, const u8 *priv_key_buf, u8 priv_key_buf_len)
{
	int ret;

	MUST_HAVE((kp != NULL), ret, err);

	/* Use our import primitive */
	ret = ec_priv_key_import_from_buf(&(kp->priv_key), params, priv_key_buf, priv_key_buf_len, ECCCDH); EG(ret, err);

	/* Now derive the public key from the private one */
	ret = ecccdh_init_pub_key(&(kp->pub_key), &(kp->priv_key));

err:
	return ret;
}

/*
 * Serialize our public key in a buffer.
 */
int ecccdh_serialize_pub_key(const ec_pub_key *our_pub_key, u8 *buf, u8 buf_len)
{
	int ret, iszero;

	/* Sanity check */
	ret = pub_key_check_initialized_and_type(our_pub_key, ECCCDH); EG(ret, err);

	/* Reject the point at infinity */
	ret = prj_pt_iszero(&(our_pub_key->y), &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	/* Export our public key as an affine point
	 * NOTE: sanity checks on buf_len are performed in the lower layers.
	 */
	ret = ec_pub_key_export_to_aff_buf(our_pub_key, buf, buf_len);

err:
	return ret;
}

/*
 * Derive the ECCCDH shared secret and store it in a buffer given the peer
 * public key and our private key.
 *
 * The shared_secret_len length MUST be exactly equal to the expected shared secret size:
 * the function fails otherwise.
 */
int ecccdh_derive_secret(const ec_priv_key *our_priv_key, const u8 *peer_pub_key_buf, u8 peer_pub_key_buf_len, u8 *shared_secret, u8 shared_secret_len)
{
	int ret, iszero, isone;
	ec_pub_key peer_pub_key;
	prj_pt_t Q;
	nn_src_t cofactor;
	u8 expected_shared_secret_len;
	peer_pub_key.magic = WORD(0);

	/* Sanity checks */
	MUST_HAVE((shared_secret != NULL), ret, err);
	ret = priv_key_check_initialized_and_type(our_priv_key, ECCCDH); EG(ret, err);

	/* Try to import the peer public key.
	 * NOTE: the check that this public key is indeed on the curve is performed in the lower layer
	 * functions.
	 */
	ret = ec_pub_key_import_from_aff_buf(&peer_pub_key, our_priv_key->params, peer_pub_key_buf, peer_pub_key_buf_len, ECCCDH); EG(ret, err);
	Q = &(peer_pub_key.y);

	cofactor = &(our_priv_key->params->ec_gen_cofactor);
	ret = nn_isone(cofactor, &isone); EG(ret, err);
	if(!isone){
		/* Perform a cofactor multiplication if necessary.
		 * NOTE: since the cofactor and the base point are public, we perform an unprotected
		 * scalar multiplication here.
		 */
		ret = _prj_pt_unprotected_mult(Q, cofactor, Q); EG(ret, err);
	}

	/*
	 * Reject the point at infinity or low order point as input as a trivial wrong public key.
	 * This would be rejected in any case by the check post scalar multiplication below, but we
	 * do not want to use and possibly leak the secret scalar if not necessary!
	 */
	ret = prj_pt_iszero(Q, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	/* Compute the shared secret using scalar multiplication */
#ifdef USE_SIG_BLINDING
	ret = prj_pt_mul_blind(Q, &(our_priv_key->x), Q); EG(ret, err);
#else
	ret = prj_pt_mul(Q, &(our_priv_key->x), Q); EG(ret, err);
#endif

	/* NOTE: scalar multiplication primitive checks that the resulting point is on
	 * the curve.
	 */
	/* Reject the point at infinity */
	ret = prj_pt_iszero(Q, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	/* Get the unique affine representation of the resulting point */
	ret = prj_pt_unique(Q, Q); EG(ret, err);
	/* Now export the X coordinate as the shared secret in the output buffer */
	ret = ecccdh_shared_secret_size(our_priv_key->params, &expected_shared_secret_len); EG(ret, err);
	MUST_HAVE((shared_secret_len == expected_shared_secret_len), ret, err);
	ret = fp_export_to_buf(shared_secret, shared_secret_len, &(Q->X));

err:
	PTR_NULLIFY(Q);
	PTR_NULLIFY(cofactor);
	/* Uninit local peer pub key and zeroize intermediate computations */
	IGNORE_RET_VAL(local_memset(&peer_pub_key, 0, sizeof(ec_pub_key)));

	return ret;
}

#else /* !defined(WITH_ECCCDH) */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;

#endif /* WITH_ECCCDH */
