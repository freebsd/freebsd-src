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
#include "sdsa.h"


/* We include the rand external dependency because we have to generate
 * some random data for the nonces.
 */
#include <libecc/external_deps/rand.h>
/* We include the printf external dependency for printf output */
#include <libecc/external_deps/print.h>
/* We include our common helpers */
#include "../common/common.h"

/*
 * The purpose of this example is to implement the Schnorr signature
 * scheme (aka SDSA for Schnorr DSA) based on libecc arithmetic primitives.
 * Many "variants" of Schnorr signature schemes exist, we implement here the
 * one corresponding to SDSA as described in the ISO14888-3 standard.
 *
 * XXX: Please be aware that libecc has been designed for Elliptic
 * Curve cryptography, and as so the arithmetic primitives are
 * not optimized for big numbers >= 1024 bits usually used for SDSA.
 * Additionnaly, a hard limit of our NN values makes it impossible
 * to exceed ~5300 bits in the best case (words of size 64 bits).
 *
 * All in all, please see this as a proof of concept.
 * Use it at your own risk!
 *
 * !! DISCLAIMER !!
 * ================
 *
 * Althoug some efforts have been made to secure this implementation
 * of Schnorr DSA (e.g. by protecting the private key and nonces using constant
 * time and blinding WHEN activated with BLINDING=1), please consider this
 * code as a proof of concept and use it at your own risk.
 *
 * All-in-all, this piece of code can be useful in some contexts, or risky to
 * use in other sensitive ones where advanced side-channels or fault attacks
 * have to be considered. Use this SDSA code knowingly and at your own risk!
 *
 */

/* NOTE: since SDSA is very similar to DSA, we reuse some of our DSA
 * primitives to factorize some code. Also, SDSA private and public keys
 * have the exact same type as DSA keys.
 */

/* Import a SDSA private key from buffers */
int sdsa_import_priv_key(sdsa_priv_key *priv, const u8 *p, u16 plen,
			const u8 *q, u16 qlen,
			const u8 *g, u16 glen,
			const u8 *x, u16 xlen)
{
	return dsa_import_priv_key(priv, p, plen, q, qlen, g, glen, x, xlen);
}

/* Import a SDSA public key from buffers */
int sdsa_import_pub_key(sdsa_pub_key *pub, const u8 *p, u16 plen,
			const u8 *q, u16 qlen,
			const u8 *g, u16 glen,
			const u8 *y, u16 ylen)
{
	return dsa_import_pub_key(pub, p, plen, q, qlen, g, glen, y, ylen);
}



/* Compute a SDSA public key from a private key.
 * The public key is computed using modular exponentiation of the generator
 * with the private key.
 */
int sdsa_compute_pub_from_priv(sdsa_pub_key *pub, const sdsa_priv_key *priv)
{
	return dsa_compute_pub_from_priv(pub, priv);
}

/* Generate a SDSA signature
 */
int sdsa_sign(const sdsa_priv_key *priv, const u8 *msg, u32 msglen,
	     const u8 *nonce, u16 noncelen,
	     u8 *sig, u16 siglen, gen_hash_alg_type sdsa_hash)
{
	int ret, iszero;
	/* alpha is the bit length of p, beta is the bit length of q */
	bitcnt_t alpha, beta;
	/* Length of the hash function (hlen is "gamma") */
	u8 hlen, block_size;
	nn_src_t p, q, g, x;
	/* The nonce and its protected version */
	nn k, k_;
	/* r, s, pi */
	nn r, s;
	nn_t pi;
	/* This is a bit too much for stack space, but we need it for
	 * the computation of "pi" I2BS representation ...
	 */
	u8 pi_buf[NN_USABLE_MAX_BYTE_LEN];
	/* hash context */
	gen_hash_context hash_ctx;
#ifdef USE_SIG_BLINDING
	/* b is the blinding mask */
	nn b;
	b.magic = WORD(0);
#endif /* USE_SIG_BLINDING */
	k.magic = k_.magic = r.magic = s.magic = WORD(0);

	/* Sanity checks */
	MUST_HAVE((priv != NULL) && (msg != NULL) && (sig != NULL), ret, err);

	ret = local_memset(pi_buf, 0, sizeof(pi_buf)); EG(ret, err);

	/* Make things more readable */
	p = &(priv->p);
	q = &(priv->q);
	g = &(priv->g);
	x = &(priv->x);

	/* Sanity checks */
	ret = nn_check_initialized(p); EG(ret, err);
	ret = nn_check_initialized(q); EG(ret, err);
	ret = nn_check_initialized(g); EG(ret, err);
	ret = nn_check_initialized(x); EG(ret, err);

	/* Let alpha be the bit length of p */
	ret = nn_bitlen(p, &alpha); EG(ret, err);
	/* Let beta be the bit length of q */
	ret = nn_bitlen(q, &beta); EG(ret, err);
	/* Get the hash sizes (8*"gamma") */
	ret = gen_hash_get_hash_sizes(sdsa_hash, &hlen, &block_size); EG(ret, err);
	MUST_HAVE((hlen <= MAX_DIGEST_SIZE), ret, err);

	/* Sanity check on the signature length:
	 * the signature is of size hash function plus an integer modulo q
	 * "gamma" + beta
	 */
	MUST_HAVE((siglen == (hlen + BYTECEIL(beta))), ret, err);

restart:
	/* If the nonce is imposed, use it. Else get a random modulo q */
	if(nonce != NULL){
		ret = _os2ip(&k, nonce, noncelen); EG(ret, err);
	}
	else{
		ret = nn_get_random_mod(&k, q); EG(ret, err);
	}

	/* Fix the MSB of our scalar */
	ret = nn_copy(&k_, &k); EG(ret, err);
#ifdef USE_SIG_BLINDING
	/* Blind the scalar */
	ret = _blind_scalar(&k_, q, &k_); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	ret = _fix_scalar_msb(&k_, q, &k_); EG(ret, err);
	/* Use r as aliasing for pi to save some space */
	pi = &r;
	/* pi = (g**k mod p) */
	ret = nn_init(pi, 0); EG(ret, err);
	/* Exponentiation modulo p */
	ret = nn_mod_pow(pi, g, &k_, p); EG(ret, err);

	/* Compute I2BS(alpha, pi)
	 */
	ret = _i2osp(pi, pi_buf, (u16)BYTECEIL(alpha)); EG(ret, err);

	/* r = h(I2BS(alpha, pi) || M) */
	ret = gen_hash_init(&hash_ctx, sdsa_hash); EG(ret, err);
	ret = gen_hash_update(&hash_ctx, pi_buf, (u16)BYTECEIL(alpha), sdsa_hash); EG(ret, err);
	ret = gen_hash_update(&hash_ctx, msg, msglen, sdsa_hash); EG(ret, err);
	/* Export r result of the hash function in sig */
	ret = gen_hash_final(&hash_ctx, sig, sdsa_hash); EG(ret, err);

	/* Import r as an integer modulo q */
	ret = _os2ip(&r, sig, hlen); EG(ret, err);
	ret = nn_mod(&r, &r, q); EG(ret, err);

	/* If r is 0, restart the process */
	ret = nn_iszero(&r, &iszero); EG(ret, err);
	if (iszero) {
		IGNORE_RET_VAL(local_memset(sig, 0, hlen));
		goto restart;
 	}

#ifdef USE_SIG_BLINDING
	/* Note: if we use blinding, r and k are multiplied by
	 * a random value b in ]0,q[ */
	ret = nn_get_random_mod(&b, q); EG(ret, err);
        /* Blind r with b */
        ret = nn_mod_mul(&r, &r, &b, q); EG(ret, err);
        /* Blind k with b */
        ret = nn_mod_mul(&k, &k, &b, q); EG(ret, err);
        /*
         * In case of blinding, we compute b^-1 with
	 * little Fermat theorem. This will be used to
	 * unblind s.
         */
        ret = nn_modinv_fermat(&b, &b, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */

	/* Compute s = (k + r x) mod q  */
	ret = nn_mod_mul(&s, &r, x, q); EG(ret, err);
	ret = nn_mod_add(&s, &s, &k, q); EG(ret, err);

#ifdef USE_SIG_BLINDING
	/* In case of blinding, unblind s */
	ret = nn_mod_mul(&s, &s, &b, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	/* If s is 0, restart the process */
	ret = nn_iszero(&s, &iszero); EG(ret, err);
	if (iszero) {
		goto restart;
 	}

	/* Export s */
	ret = _i2osp(&s, sig + hlen, (u16)(siglen - hlen)); EG(ret, err);

err:
	if(ret && (sig != NULL)){
		IGNORE_RET_VAL(local_memset(sig, 0, siglen));
	}

	nn_uninit(&k);
	nn_uninit(&k_);
#ifdef USE_SIG_BLINDING
	nn_uninit(&b);
#endif
	nn_uninit(&r);
	nn_uninit(&s);

	PTR_NULLIFY(pi);

	PTR_NULLIFY(p);
	PTR_NULLIFY(q);
	PTR_NULLIFY(g);
	PTR_NULLIFY(x);

	return ret;
}



/* Verify a SDSA signature
 */
int sdsa_verify(const sdsa_pub_key *pub, const u8 *msg, u32 msglen,
	     const u8 *sig, u16 siglen, gen_hash_alg_type sdsa_hash)
{
	int ret, iszero, cmp;
	/* alpha is the bit length of p, beta is the bit length of q */
	bitcnt_t alpha, beta;
	/* Length of the hash function */
	u8 hlen, block_size;
	nn_src_t p, q, g, y;
	/* r, s */
	nn r, s;
	/* u, and pi */
	nn u, pi;
	/* This is a bit too much for stack space, but we need it for
	 * the computation of "pi" I2BS representation ...
	 */
	u8 pi_buf[NN_USABLE_MAX_BYTE_LEN];
	/* Hash */
	u8 hash[MAX_DIGEST_SIZE];
	/* hash context */
	gen_hash_context hash_ctx;
	r.magic = s.magic = u.magic = pi.magic = WORD(0);

	/* Sanity checks */
	MUST_HAVE((pub != NULL) && (msg != NULL) && (sig != NULL), ret, err);

	ret = local_memset(pi_buf, 0, sizeof(pi_buf)); EG(ret, err);
	ret = local_memset(hash, 0, sizeof(hash)); EG(ret, err);

	/* Make things more readable */
	p = &(pub->p);
	q = &(pub->q);
	g = &(pub->g);
	y = &(pub->y);

	/* Sanity checks */
	ret = nn_check_initialized(p); EG(ret, err);
	ret = nn_check_initialized(q); EG(ret, err);
	ret = nn_check_initialized(g); EG(ret, err);
	ret = nn_check_initialized(y); EG(ret, err);

	/* Let alpha be the bit length of p */
	ret = nn_bitlen(p, &alpha); EG(ret, err);
	/* Let beta be the bit length of q */
	ret = nn_bitlen(q, &beta); EG(ret, err);
	/* Get the hash sizes (8*"gamma") */
	ret = gen_hash_get_hash_sizes(sdsa_hash, &hlen, &block_size); EG(ret, err);
	MUST_HAVE((hlen <= MAX_DIGEST_SIZE), ret, err);

	/* Sanity check on the signature length */
	MUST_HAVE((siglen == (hlen + BYTECEIL(beta))), ret, err);

	/* Extract r and s */
	ret = _os2ip(&r, sig, hlen); EG(ret, err);
	ret = _os2ip(&s, sig + hlen, (u16)(siglen - hlen)); EG(ret, err);

	/* Return an error if r = 0 or s = 0 */
	ret = nn_iszero(&r, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);
	ret = nn_iszero(&s, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);
	/* Check that 0 < s < q */
	ret = nn_cmp(&s, q, &cmp); EG(ret, err);
	MUST_HAVE((cmp < 0), ret, err);

	/* Take r modulo q */
	ret = nn_mod(&r, &r, q); EG(ret, err);

	/* Initialize internal variables */
	ret = nn_init(&u, 0); EG(ret, err);
	ret = nn_init(&pi, 0); EG(ret, err);

	/* NOTE: no need to use a secure exponentiation here as we only
	 * manipulate public data.
	 */
	/* Compute (y ** -r) mod (p) */
	ret = nn_sub(&r, q, &r); EG(ret, err); /* compute -r = (q - r) mod q */
	ret = _nn_mod_pow_insecure(&u, y, &r, p); EG(ret, err);
	/* Compute (g ** s) mod (p) */
	ret = _nn_mod_pow_insecure(&pi, g, &s, p); EG(ret, err);
	/* Compute (y ** -r) * (g ** s) mod (p) */
	ret = nn_mod_mul(&pi, &pi, &u, p); EG(ret, err);

	/* Compute r' */
	/* I2BS(alpha, pi)
	 */
	ret = _i2osp(&pi, pi_buf, (u16)BYTECEIL(alpha)); EG(ret, err);
	/* r' = h(I2BS(alpha, pi) || M) */
	ret = gen_hash_init(&hash_ctx, sdsa_hash); EG(ret, err);
	ret = gen_hash_update(&hash_ctx, pi_buf, (u16)BYTECEIL(alpha), sdsa_hash); EG(ret, err);
	ret = gen_hash_update(&hash_ctx, msg, msglen, sdsa_hash); EG(ret, err);
	ret = gen_hash_final(&hash_ctx, hash, sdsa_hash); EG(ret, err);

	/* Check that hash values r' == r */
	ret = are_equal(sig, hash, hlen, &cmp); EG(ret, err);
	ret = (cmp != 1) ? -1 : 0;

err:
	nn_uninit(&r);
	nn_uninit(&s);
	nn_uninit(&u);
	nn_uninit(&pi);

	PTR_NULLIFY(p);
	PTR_NULLIFY(q);
	PTR_NULLIFY(g);
	PTR_NULLIFY(y);

	return ret;
}

#ifdef SDSA
#include <libecc/utils/print_buf.h>
int main(int argc, char *argv[])
{
 	int ret = 0;

	/* This example is taken from ISO14888-3 SDSA (Appendix F "Numerical examples" */
	const u8 p[] = {
		0x87, 0xA8, 0xE6, 0x1D, 0xB4, 0xB6, 0x66, 0x3C, 0xFF, 0xBB, 0xD1, 0x9C, 0x65, 0x19, 0x59, 0x99, 0x8C, 0xEE, 0xF6, 0x08, 0x66, 0x0D, 0xD0, 0xF2,
		0x5D, 0x2C, 0xEE, 0xD4, 0x43, 0x5E, 0x3B, 0x00, 0xE0, 0x0D, 0xF8, 0xF1, 0xD6, 0x19, 0x57, 0xD4, 0xFA, 0xF7, 0xDF, 0x45, 0x61, 0xB2, 0xAA, 0x30,
		0x16, 0xC3, 0xD9, 0x11, 0x34, 0x09, 0x6F, 0xAA, 0x3B, 0xF4, 0x29, 0x6D, 0x83, 0x0E, 0x9A, 0x7C, 0x20, 0x9E, 0x0C, 0x64, 0x97, 0x51, 0x7A, 0xBD,
		0x5A, 0x8A, 0x9D, 0x30, 0x6B, 0xCF, 0x67, 0xED, 0x91, 0xF9, 0xE6, 0x72, 0x5B, 0x47, 0x58, 0xC0, 0x22, 0xE0, 0xB1, 0xEF, 0x42, 0x75, 0xBF, 0x7B,
		0x6C, 0x5B, 0xFC, 0x11, 0xD4, 0x5F, 0x90, 0x88, 0xB9, 0x41, 0xF5, 0x4E, 0xB1, 0xE5, 0x9B, 0xB8, 0xBC, 0x39, 0xA0, 0xBF, 0x12, 0x30, 0x7F, 0x5C,
		0x4F, 0xDB, 0x70, 0xC5, 0x81, 0xB2, 0x3F, 0x76, 0xB6, 0x3A, 0xCA, 0xE1, 0xCA, 0xA6, 0xB7, 0x90, 0x2D, 0x52, 0x52, 0x67, 0x35, 0x48, 0x8A, 0x0E,
		0xF1, 0x3C, 0x6D, 0x9A, 0x51, 0xBF, 0xA4, 0xAB, 0x3A, 0xD8, 0x34, 0x77, 0x96, 0x52, 0x4D, 0x8E, 0xF6, 0xA1, 0x67, 0xB5, 0xA4, 0x18, 0x25, 0xD9,
		0x67, 0xE1, 0x44, 0xE5, 0x14, 0x05, 0x64, 0x25, 0x1C, 0xCA, 0xCB, 0x83, 0xE6, 0xB4, 0x86, 0xF6, 0xB3, 0xCA, 0x3F, 0x79, 0x71, 0x50, 0x60, 0x26,
		0xC0, 0xB8, 0x57, 0xF6, 0x89, 0x96, 0x28, 0x56, 0xDE, 0xD4, 0x01, 0x0A, 0xBD, 0x0B, 0xE6, 0x21, 0xC3, 0xA3, 0x96, 0x0A, 0x54, 0xE7, 0x10, 0xC3,
		0x75, 0xF2, 0x63, 0x75, 0xD7, 0x01, 0x41, 0x03, 0xA4, 0xB5, 0x43, 0x30, 0xC1, 0x98, 0xAF, 0x12, 0x61, 0x16, 0xD2, 0x27, 0x6E, 0x11, 0x71, 0x5F,
		0x69, 0x38, 0x77, 0xFA, 0xD7, 0xEF, 0x09, 0xCA, 0xDB, 0x09, 0x4A, 0xE9, 0x1E, 0x1A, 0x15, 0x97,
	};

	const u8 q[] = {
		0x8C, 0xF8, 0x36, 0x42, 0xA7, 0x09, 0xA0, 0x97, 0xB4, 0x47, 0x99, 0x76, 0x40, 0x12, 0x9D, 0xA2, 0x99, 0xB1, 0xA4, 0x7D, 0x1E, 0xB3, 0x75, 0x0B,
		0xA3, 0x08, 0xB0, 0xFE, 0x64, 0xF5, 0xFB, 0xD3,
	};

	const u8 g[] = {
		0x3F, 0xB3, 0x2C, 0x9B, 0x73, 0x13, 0x4D, 0x0B, 0x2E, 0x77, 0x50, 0x66, 0x60, 0xED, 0xBD, 0x48, 0x4C, 0xA7, 0xB1, 0x8F, 0x21, 0xEF, 0x20, 0x54,
		0x07, 0xF4, 0x79, 0x3A, 0x1A, 0x0B, 0xA1, 0x25, 0x10, 0xDB, 0xC1, 0x50, 0x77, 0xBE, 0x46, 0x3F, 0xFF, 0x4F, 0xED, 0x4A, 0xAC, 0x0B, 0xB5, 0x55,
		0xBE, 0x3A, 0x6C, 0x1B, 0x0C, 0x6B, 0x47, 0xB1, 0xBC, 0x37, 0x73, 0xBF, 0x7E, 0x8C, 0x6F, 0x62, 0x90, 0x12, 0x28, 0xF8, 0xC2, 0x8C, 0xBB, 0x18,
		0xA5, 0x5A, 0xE3, 0x13, 0x41, 0x00, 0x0A, 0x65, 0x01, 0x96, 0xF9, 0x31, 0xC7, 0x7A, 0x57, 0xF2, 0xDD, 0xF4, 0x63, 0xE5, 0xE9, 0xEC, 0x14, 0x4B,
		0x77, 0x7D, 0xE6, 0x2A, 0xAA, 0xB8, 0xA8, 0x62, 0x8A, 0xC3, 0x76, 0xD2, 0x82, 0xD6, 0xED, 0x38, 0x64, 0xE6, 0x79, 0x82, 0x42, 0x8E, 0xBC, 0x83,
		0x1D, 0x14, 0x34, 0x8F, 0x6F, 0x2F, 0x91, 0x93, 0xB5, 0x04, 0x5A, 0xF2, 0x76, 0x71, 0x64, 0xE1, 0xDF, 0xC9, 0x67, 0xC1, 0xFB, 0x3F, 0x2E, 0x55,
		0xA4, 0xBD, 0x1B, 0xFF, 0xE8, 0x3B, 0x9C, 0x80, 0xD0, 0x52, 0xB9, 0x85, 0xD1, 0x82, 0xEA, 0x0A, 0xDB, 0x2A, 0x3B, 0x73, 0x13, 0xD3, 0xFE, 0x14,
		0xC8, 0x48, 0x4B, 0x1E, 0x05, 0x25, 0x88, 0xB9, 0xB7, 0xD2, 0xBB, 0xD2, 0xDF, 0x01, 0x61, 0x99, 0xEC, 0xD0, 0x6E, 0x15, 0x57, 0xCD, 0x09, 0x15,
		0xB3, 0x35, 0x3B, 0xBB, 0x64, 0xE0, 0xEC, 0x37, 0x7F, 0xD0, 0x28, 0x37, 0x0D, 0xF9, 0x2B, 0x52, 0xC7, 0x89, 0x14, 0x28, 0xCD, 0xC6, 0x7E, 0xB6,
		0x18, 0x4B, 0x52, 0x3D, 0x1D, 0xB2, 0x46, 0xC3, 0x2F, 0x63, 0x07, 0x84, 0x90, 0xF0, 0x0E, 0xF8, 0xD6, 0x47, 0xD1, 0x48, 0xD4, 0x79, 0x54, 0x51,
		0x5E, 0x23, 0x27, 0xCF, 0xEF, 0x98, 0xC5, 0x82, 0x66, 0x4B, 0x4C, 0x0F, 0x6C, 0xC4, 0x16, 0x59,
	};

	const u8 x[] = {
		0x73, 0x01, 0x88, 0x95, 0x20, 0xD4, 0x7A, 0xA0, 0x55, 0x99, 0x5B, 0xA1, 0xD8, 0xFC, 0xD7, 0x01, 0x6E, 0xA6, 0x2E, 0x09, 0x18, 0x89, 0x2E, 0x07,
		0xB7, 0xDC, 0x23, 0xAF, 0x69, 0x00, 0x6B, 0x88,
	};

	const u8 y[] = {
		0x57, 0xA1, 0x72, 0x58, 0xD4, 0xA3, 0xF4, 0x7C, 0x45, 0x45, 0xAD, 0x51, 0xF3, 0x10, 0x9C, 0x5D, 0xB4, 0x1B, 0x78, 0x78, 0x79, 0xFC, 0xFE, 0x53,
		0x8D, 0xC1, 0xDD, 0x5D, 0x35, 0xCE, 0x42, 0xFF, 0x3A, 0x9F, 0x22, 0x5E, 0xDE, 0x65, 0x02, 0x12, 0x64, 0x08, 0xFC, 0xB1, 0x3A, 0xEA, 0x22, 0x31,
		0x80, 0xB1, 0x49, 0xC4, 0x64, 0xE1, 0x76, 0xEB, 0xF0, 0x3B, 0xA6, 0x51, 0x0D, 0x82, 0x06, 0xC9, 0x20, 0xF6, 0xB1, 0xE0, 0x93, 0x92, 0xE6, 0xC8,
		0x40, 0xA0, 0x5B, 0xDB, 0x9D, 0x68, 0x75, 0xAB, 0x3F, 0x48, 0x17, 0xEC, 0x3A, 0x65, 0xA6, 0x65, 0xB7, 0x88, 0xEC, 0xBB, 0x44, 0x71, 0x88, 0xC7,
		0xDF, 0x2E, 0xB4, 0xD3, 0xD9, 0x42, 0x4E, 0x57, 0xD9, 0x64, 0x39, 0x8D, 0xBE, 0x1C, 0x63, 0x62, 0x65, 0x9C, 0x6B, 0xD8, 0x55, 0xC1, 0xD3, 0xE5,
		0x1D, 0x64, 0x79, 0x6C, 0xA5, 0x98, 0x48, 0x0D, 0xFD, 0xD9, 0x58, 0x0E, 0x55, 0x08, 0x53, 0x45, 0xC1, 0x5E, 0x34, 0xD6, 0xA3, 0x3A, 0x2F, 0x43,
		0xE2, 0x22, 0x40, 0x7A, 0xCE, 0x05, 0x89, 0x72, 0xD3, 0x49, 0x52, 0xAE, 0x2B, 0x70, 0x5C, 0x53, 0x22, 0x43, 0xBE, 0x39, 0x4B, 0x22, 0x23, 0x29,
		0x61, 0x61, 0x14, 0x5E, 0xF2, 0x92, 0x7C, 0xDB, 0xC5, 0x5B, 0xBD, 0x56, 0x4A, 0xAE, 0x8D, 0xE4, 0xBA, 0x45, 0x00, 0xA7, 0xFA, 0x43, 0x2F, 0xE7,
		0x8B, 0x0F, 0x06, 0x89, 0x1E, 0x40, 0x80, 0x83, 0x7E, 0x76, 0x10, 0x57, 0xBC, 0x6C, 0xB8, 0xAC, 0x18, 0xFD, 0x43, 0x20, 0x75, 0x82, 0x03, 0x2A,
		0xFB, 0x63, 0xC6, 0x24, 0xF3, 0x2E, 0x66, 0xB0, 0x5F, 0xC3, 0x1C, 0x5D, 0xFF, 0xB2, 0x5F, 0xA9, 0x2D, 0x4D, 0x00, 0xE2, 0xB0, 0xD4, 0xF7, 0x21,
		0xE8, 0x8C, 0x41, 0x7D, 0x2E, 0x57, 0x79, 0x7B, 0x8F, 0x55, 0xA2, 0xFF, 0xC6, 0xEE, 0x4D, 0xDB,
	};

	const u8 msg[] = "abc";

	const u8 nonce[] = {
		0x2B, 0x73, 0xE8, 0xFF, 0x3A, 0x7C, 0x01, 0x68, 0x6C, 0xA5, 0x56, 0xE0, 0xFA, 0xBF, 0xD7, 0x4A, 0xC8, 0xD1, 0xFD, 0xA4, 0xAD, 0x3D, 0x50, 0x3F,
		0x23, 0xB8, 0xEB, 0x8A, 0xEE, 0xC6, 0x33, 0x05,
	};

	sdsa_priv_key priv;
	sdsa_pub_key pub;
	sdsa_pub_key pub2;
	u8 sig[32*2] = { 0 };

	FORCE_USED_VAR(argc);
	FORCE_USED_VAR(argv);

	/* Sanity check on size for DSA.
	 * NOTE: the double parentheses are here to handle -Wunreachable-code
	 */
	if((NN_USABLE_MAX_BIT_LEN) < (4096)){
		ext_printf("Error: you seem to have compiled libecc with usable NN size < 4096, not suitable for DSA.\n");
		ext_printf("  => Please recompile libecc with EXTRA_CFLAGS=\"-DUSER_NN_BIT_LEN=4096\"\n");
		ext_printf("     This will increase usable NN for proper DSA up to 4096 bits.\n");
		ext_printf("     Then recompile the current examples with the same EXTRA_CFLAGS=\"-DUSER_NN_BIT_LEN=4096\" flag and execute again!\n");
		/* NOTE: ret = 0 here to pass self tests even if the library is not compatible */
		ret = 0;
		goto err;
	}


	ret = sdsa_import_priv_key(&priv, p, sizeof(p), q, sizeof(q), g, sizeof(g), x, sizeof(x)); EG(ret, err);
	ret = sdsa_import_pub_key(&pub, p, sizeof(p), q, sizeof(q), g, sizeof(g), y, sizeof(y)); EG(ret, err);
	ret = sdsa_compute_pub_from_priv(&pub2, &priv); EG(ret, err);

	nn_print("y", &(pub2.y));

	ret = sdsa_sign(&priv, msg, sizeof(msg)-1, nonce, sizeof(nonce), sig, sizeof(sig), HASH_SHA256); EG(ret, err);

	buf_print("sig", sig, sizeof(sig));

	ret = sdsa_verify(&pub, msg, sizeof(msg)-1, sig, sizeof(sig), HASH_SHA256);
	ext_printf("Signature result %d\n", ret);

err:
	return ret;
}
#endif
