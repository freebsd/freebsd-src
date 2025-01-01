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
#include "gostr34_10_94.h"


/* We include the rand external dependency because we have to generate
 * some random data for the nonces.
 */
#include <libecc/external_deps/rand.h>
/* We include the printf external dependency for printf output */
#include <libecc/external_deps/print.h>
/* We include our common helpers */
#include "../common/common.h"

/*
 * The purpose of this example is to implement the GOSTR34-10-94
 * based on libecc arithmetic primitives.
 *
 * XXX: Please be aware that libecc has been designed for Elliptic
 * Curve cryptography, and as so the arithmetic primitives are
 * not optimized for big numbers >= 1024 bits usually used for GOSTR.
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
 * of GOSTR (e.g. by protecting the private key and nonces using constant
 * time and blinding WHEN activated with BLINDING=1), please consider this
 * code as a proof of concept and use it at your own risk.
 *
 * All-in-all, this piece of code can be useful in some contexts, or risky to
 * use in other sensitive ones where advanced side-channels or fault attacks
 * have to be considered. Use this GOSTR code knowingly and at your own risk!
 *
 */

/* NOTE: since GOSTR34_10_94 is very similar to DSA, we reuse some of our DSA
 * primitives to factorize some code. Also, GOSTR34_10_94 private and public keys
 * have the exact same type as DSA keys.
 */

/* Import a GOSTR34_10_94 private key from buffers */
int gostr34_10_94_import_priv_key(gostr34_10_94_priv_key *priv, const u8 *p, u16 plen,
			const u8 *q, u16 qlen,
			const u8 *g, u16 glen,
			const u8 *x, u16 xlen)
{
	return dsa_import_priv_key(priv, p, plen, q, qlen, g, glen, x, xlen);
}

/* Import a GOSTR34_10_94 public key from buffers */
int gostr34_10_94_import_pub_key(gostr34_10_94_pub_key *pub, const u8 *p, u16 plen,
			const u8 *q, u16 qlen,
			const u8 *g, u16 glen,
			const u8 *y, u16 ylen)
{
	return dsa_import_pub_key(pub, p, plen, q, qlen, g, glen, y, ylen);
}



/* Compute a GOSTR34_10_94 public key from a private key.
 * The public key is computed using modular exponentiation of the generator
 * with the private key.
 */
int gostr34_10_94_compute_pub_from_priv(gostr34_10_94_pub_key *pub, const gostr34_10_94_priv_key *priv)
{
	return dsa_compute_pub_from_priv(pub, priv);
}

/* Generate a GOSTR34_10_94 signature
 */
int gostr34_10_94_sign(const gostr34_10_94_priv_key *priv, const u8 *msg, u32 msglen,
	     const u8 *nonce, u16 noncelen,
	     u8 *sig, u16 siglen, gen_hash_alg_type gostr34_10_94_hash)
{
	int ret, iszero;
	/* N is the bit length of q */
	bitcnt_t N, rshift;
	/* Length of the hash function */
	u8 hlen, block_size;
	nn_src_t p, q, g, x;
	/* The nonce and its protected version */
	nn k, k_;
	/* r, s and z */
	nn r, s, z;
	/* Hash */
	u8 hash[MAX_DIGEST_SIZE];
#ifdef USE_SIG_BLINDING
	/* b is the blinding mask */
	nn b;
	b.magic = WORD(0);
#endif /* USE_SIG_BLINDING */
	k.magic = k_.magic = r.magic = s.magic = z.magic = WORD(0);

	/* Sanity checks */
	MUST_HAVE((priv != NULL) && (msg != NULL) && (sig != NULL), ret, err);

	ret = local_memset(hash, 0, sizeof(hash)); EG(ret, err);

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

	/* Let N be the bit length of q. Let min(N, outlen) denote the minimum
	 * of the positive integers N and outlen, where outlen is the bit length
	 * of the hash function output block.
	 */
	ret = nn_bitlen(q, &N); EG(ret, err);
	ret = gen_hash_get_hash_sizes(gostr34_10_94_hash, &hlen, &block_size); EG(ret, err);
	MUST_HAVE((hlen <= MAX_DIGEST_SIZE), ret, err);

	/* Sanity check on the signature length */
	MUST_HAVE((siglen == (2 * BYTECEIL(N))), ret, err);

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
	/* r = (g**k mod p) mod q */
	ret = nn_init(&r, 0); EG(ret, err);
	/* Exponentiation modulo p */
	ret = nn_mod_pow(&r, g, &k_, p); EG(ret, err);
	/* Modulo q */
	ret = nn_mod(&r, &r, q); EG(ret, err);

	/* If r is 0, restart the process */
	ret = nn_iszero(&r, &iszero); EG(ret, err);
	if (iszero) {
		goto restart;
 	}

	/* Export r */
	ret = _i2osp(&r, sig, (siglen / 2)); EG(ret, err);

	/* Compute the hash */
	ret = gen_hash_hfunc(msg, msglen, hash, gostr34_10_94_hash); EG(ret, err);
	/* Reverse the endianness of the hash as little endian is needed here */
	ret = _reverse_endianness(hash, hlen); EG(ret, err);

	/* z = the leftmost min(N, outlen) bits of Hash(M) */
        rshift = 0;
        if ((hlen * 8) > N) {
                rshift = (bitcnt_t)((hlen * 8) - N);
        }
	ret = _os2ip(&z, hash, hlen); EG(ret, err);
	if (rshift) {
		ret = nn_rshift_fixedlen(&z, &z, rshift); EG(ret, err);
	}
	ret = nn_mod(&z, &z, q); EG(ret, err);
	/* If z = 0, then set it to 1 */
	ret = nn_iszero(&z, &iszero); EG(ret, err);
	if(iszero){
		ret = nn_one(&z); EG(ret, err);
	}

#ifdef USE_SIG_BLINDING
	/* Note: if we use blinding, r and e are multiplied by
	 * a random value b in ]0,q[ */
	ret = nn_get_random_mod(&b, q); EG(ret, err);
        /* Blind r with b */
        ret = nn_mod_mul(&r, &r, &b, q); EG(ret, err);
        /* Blind the message z */
        ret = nn_mod_mul(&z, &z, &b, q); EG(ret, err);
        /*
         * In case of blinding, we compute b^-1 with
	 * little Fermat theorem. This will be used to
	 * unblind s.
         */
        ret = nn_modinv_fermat(&b, &b, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */

	/* Compute s = (xr + kz) mod q  */
	ret = nn_mod_mul(&s, &r, x, q); EG(ret, err);
	ret = nn_mod_mul(&r, &k, &z, q); EG(ret, err);
	ret = nn_mod_add(&s, &s, &r, q); EG(ret, err);

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
	ret = _i2osp(&s, sig + (siglen / 2), (siglen / 2));

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
	nn_uninit(&z);

	PTR_NULLIFY(p);
	PTR_NULLIFY(q);
	PTR_NULLIFY(g);
	PTR_NULLIFY(x);

	return ret;
}



/* Verify a GOSTR34_10_94 signature
 */
int gostr34_10_94_verify(const gostr34_10_94_pub_key *pub, const u8 *msg, u32 msglen,
	     const u8 *sig, u16 siglen, gen_hash_alg_type gostr34_10_94_hash)
{
	int ret, iszero, cmp;
	/* N is the bit length of q */
	bitcnt_t N, rshift;
	/* Length of the hash function */
	u8 hlen, block_size;
	nn_src_t p, q, g, y;
	/* r, s */
	nn r, s, z;
	/* u1, u2, and v */
	nn u1, u2, v;
	/* Hash */
	u8 hash[MAX_DIGEST_SIZE];
	r.magic = s.magic = z.magic = u1.magic = u2.magic = WORD(0);

	/* Sanity checks */
	MUST_HAVE((pub != NULL) && (msg != NULL) && (sig != NULL), ret, err);

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

	/* Sanity check on the signature length */
	ret = nn_bitlen(q, &N); EG(ret, err);
	MUST_HAVE((siglen == (2 * BYTECEIL(N))), ret, err);

	/* Extract r and s */
	ret = _os2ip(&r, sig, (siglen / 2)); EG(ret, err);
	ret = _os2ip(&s, sig + (siglen / 2), (siglen / 2)); EG(ret, err);

	/* Return an error if r = 0 or s = 0 */
	ret = nn_iszero(&r, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);
	ret = nn_iszero(&s, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);
	/* Check that 0 < r < q and 0 < s < q */
	ret = nn_cmp(&r, q, &cmp); EG(ret, err);
	MUST_HAVE((cmp < 0), ret, err);
	ret = nn_cmp(&s, q, &cmp); EG(ret, err);
	MUST_HAVE((cmp < 0), ret, err);

	/* Compute the hash */
	ret = gen_hash_get_hash_sizes(gostr34_10_94_hash, &hlen, &block_size); EG(ret, err);
	MUST_HAVE((hlen <= MAX_DIGEST_SIZE), ret, err);
	ret = gen_hash_hfunc(msg, msglen, hash, gostr34_10_94_hash); EG(ret, err);
	/* Reverse the endianness of the hash as little endian is needed here */
	ret = _reverse_endianness(hash, hlen); EG(ret, err);

	/* z = the leftmost min(N, outlen) bits of Hash(M) */
        rshift = 0;
        if ((hlen * 8) > N) {
                rshift = (bitcnt_t)((hlen * 8) - N);
        }
	ret = _os2ip(&z, hash, hlen); EG(ret, err);
	if (rshift) {
		ret = nn_rshift_fixedlen(&z, &z, rshift); EG(ret, err);
	}
	ret = nn_mod(&z, &z, q); EG(ret, err);
	/* If z = 0, then set it to 1 */
	ret = nn_iszero(&z, &iszero); EG(ret, err);
	if(iszero){
		ret = nn_one(&z); EG(ret, err);
	}

	/* Initialize internal variables */
	ret = nn_init(&v, 0); EG(ret, err);
	ret = nn_init(&u1, 0); EG(ret, err);
	ret = nn_init(&u2, 0); EG(ret, err);

	/* Compute v = z ** (q-2) mod (q) in s */
	ret = nn_dec(&u1, q); EG(ret, err); /* use u1 as temp here */
	ret = nn_dec(&u1, &u1); EG(ret, err);
	ret = _nn_mod_pow_insecure(&v, &z, &u1, q); EG(ret, err);
	/* u1 = (s * v) mod q */
	ret = nn_mod_mul(&u1, &s, &v, q); EG(ret, err);
	/* u2 = ((q-r) * v) mod q */
	ret = nn_sub(&u2, q, &r); EG(ret, err);
	ret = nn_mod_mul(&u2, &u2, &v, q); EG(ret, err);
	/* Now compute v = ((g**u1 y**u2) mod p) mod q */
	/* NOTE: no need to use a secure exponentiation here as we only
	 * manipulate public data.
	 */
	ret = _nn_mod_pow_insecure(&v, g, &u1, p); EG(ret, err);
	ret = _nn_mod_pow_insecure(&s, y, &u2, p); EG(ret, err);
	ret = nn_mod_mul(&v, &v, &s, p); EG(ret, err);
	ret = nn_mod(&v, &v, q); EG(ret, err);

	/* Check that v = r */
	ret = nn_cmp(&v, &r, &cmp); EG(ret, err);
	ret = (cmp != 0) ? -1 : 0;

err:
	nn_uninit(&r);
	nn_uninit(&s);
	nn_uninit(&z);
	nn_uninit(&u1);
	nn_uninit(&u2);
	nn_uninit(&v);

	PTR_NULLIFY(p);
	PTR_NULLIFY(q);
	PTR_NULLIFY(g);
	PTR_NULLIFY(y);

	return ret;
}

#ifdef GOSTR34_10_94
#include <libecc/utils/print_buf.h>
int main(int argc, char *argv[])
{
 	int ret = 0;

	/**** Self-signed certificate taken from RFC4491 ****/
	/* NOTE1: we can only perform verification using this self-signed certificate as we do not have the private key!
	 * NOTE2: id-GostR3410-94-CryptoPro-A-ParamSet (values of p, q, g) are extracted from the same RFC
	 */
	const u8 p[] = {
		0x00, 0xB4, 0xE2, 0x5E, 0xFB, 0x01, 0x8E, 0x3C, 0x8B, 0x87, 0x50, 0x5E, 0x2A, 0x67, 0x55, 0x3C,
		0x5E, 0xDC, 0x56, 0xC2, 0x91, 0x4B, 0x7E, 0x4F, 0x89, 0xD2, 0x3F, 0x03, 0xF0, 0x33, 0x77, 0xE7,
		0x0A, 0x29, 0x03, 0x48, 0x9D, 0xD6, 0x0E, 0x78, 0x41, 0x8D, 0x3D, 0x85, 0x1E, 0xDB, 0x53, 0x17,
		0xC4, 0x87, 0x1E, 0x40, 0xB0, 0x42, 0x28, 0xC3, 0xB7, 0x90, 0x29, 0x63, 0xC4, 0xB7, 0xD8, 0x5D,
		0x52, 0xB9, 0xAA, 0x88, 0xF2, 0xAF, 0xDB, 0xEB, 0x28, 0xDA, 0x88, 0x69, 0xD6, 0xDF, 0x84, 0x6A,
		0x1D, 0x98, 0x92, 0x4E, 0x92, 0x55, 0x61, 0xBD, 0x69, 0x30, 0x0B, 0x9D, 0xDD, 0x05, 0xD2, 0x47,
		0xB5, 0x92, 0x2D, 0x96, 0x7C, 0xBB, 0x02, 0x67, 0x18, 0x81, 0xC5, 0x7D, 0x10, 0xE5, 0xEF, 0x72,
		0xD3, 0xE6, 0xDA, 0xD4, 0x22, 0x3D, 0xC8, 0x2A, 0xA1, 0xF7, 0xD0, 0x29, 0x46, 0x51, 0xA4, 0x80,
		0xDF,
	};

	const u8 q[] = {
		0x00,  0x97,  0x24,  0x32,  0xA4,  0x37,  0x17,  0x8B,  0x30,  0xBD,  0x96,  0x19,  0x5B,  0x77,  0x37,  0x89,
		0xAB,  0x2F,  0xFF,  0x15,  0x59,  0x4B,  0x17,  0x6D,  0xD1,  0x75,  0xB6,  0x32,  0x56,  0xEE,  0x5A,  0xF2,
		0xCF,
	};

	const u8 g[] = {
		0x00,  0x8F,  0xD3,  0x67,  0x31,  0x23,  0x76,  0x54,  0xBB,  0xE4,  0x1F,  0x5F,  0x1F,  0x84,  0x53,  0xE7,
		0x1C,  0xA4,  0x14,  0xFF,  0xC2,  0x2C,  0x25,  0xD9,  0x15,  0x30,  0x9E,  0x5D,  0x2E,  0x62,  0xA2,  0xA2,
		0x6C,  0x71,  0x11,  0xF3,  0xFC,  0x79,  0x56,  0x8D,  0xAF,  0xA0,  0x28,  0x04,  0x2F,  0xE1,  0xA5,  0x2A,
		0x04,  0x89,  0x80,  0x5C,  0x0D,  0xE9,  0xA1,  0xA4,  0x69,  0xC8,  0x44,  0xC7,  0xCA,  0xBB,  0xEE,  0x62,
		0x5C,  0x30,  0x78,  0x88,  0x8C,  0x1D,  0x85,  0xEE,  0xA8,  0x83,  0xF1,  0xAD,  0x5B,  0xC4,  0xE6,  0x77,
		0x6E,  0x8E,  0x1A,  0x07,  0x50,  0x91,  0x2D,  0xF6,  0x4F,  0x79,  0x95,  0x64,  0x99,  0xF1,  0xE1,  0x82,
		0x47,  0x5B,  0x0B,  0x60,  0xE2,  0x63,  0x2A,  0xDC,  0xD8,  0xCF,  0x94,  0xE9,  0xC5,  0x4F,  0xD1,  0xF3,
		0xB1,  0x09,  0xD8,  0x1F,  0x00,  0xBF,  0x2A,  0xB8,  0xCB,  0x86,  0x2A,  0xDF,  0x7D,  0x40,  0xB9,  0x36,
		0x9A,
	};

	u8 x[sizeof(q)];

	u8 y_self_signed[] = {
		0xBB, 0x84, 0x66, 0xE1, 0x79, 0x9E, 0x5B, 0x34, 0xD8, 0x2C, 0x80, 0x7F, 0x13, 0xA8, 0x19, 0x66,
		0x71, 0x57, 0xFE, 0x8C, 0x54, 0x25, 0x21, 0x47, 0x6F, 0x30, 0x0B, 0x27, 0x77, 0x46, 0x98, 0xC6,
		0xFB, 0x47, 0x55, 0xBE, 0xB7, 0xB2, 0xF3, 0x93, 0x6C, 0x39, 0xB5, 0x42, 0x37, 0x26, 0x84, 0xE2,
		0x0D, 0x10, 0x8A, 0x24, 0x0E, 0x1F, 0x0C, 0x42, 0x4D, 0x2B, 0x3B, 0x11, 0x2B, 0xA8, 0xBF, 0x66,
		0x39, 0x32, 0x5C, 0x94, 0x8B, 0xC1, 0xA8, 0xFE, 0x1B, 0x63, 0x12, 0xF6, 0x09, 0x25, 0x87, 0xCC,
		0x75, 0x1B, 0xF4, 0xE5, 0x89, 0x8A, 0x09, 0x82, 0x68, 0xD3, 0x5C, 0x77, 0xA6, 0x0F, 0xB6, 0x90,
		0x10, 0x13, 0x8D, 0xE3, 0x3E, 0x7C, 0x9C, 0x91, 0xD6, 0xAC, 0x0D, 0x08, 0x2C, 0x3E, 0x78, 0xC1,
		0xB5, 0xC2, 0xB6, 0xB7, 0x1A, 0xA8, 0x2A, 0x8B, 0x45, 0x81, 0x93, 0x32, 0x32, 0x76, 0xFA, 0x7B,
	};

	const u8 sig[32*2] = {
		/* r' */
		0x22, 0xf7, 0x85, 0xf3,	0x55, 0xbd, 0x94, 0xec, 0x46, 0x91, 0x9c, 0x67, 0xac, 0x58, 0xd7, 0x05, 0x2a, 0xa7,
		0x8c, 0xb7, 0x85, 0x2a, 0x01, 0x75, 0x85, 0xf7, 0xd7, 0x38, 0x03, 0xfb, 0xcd, 0x43,
		/* s */
		0x11, 0xc7, 0x08, 0x7e, 0x12, 0xdc, 0x02, 0xf1, 0x02, 0x23, 0x29, 0x47, 0x76, 0x8f, 0x47, 0x2a, 0x81, 0x83,
		0x50, 0xe3, 0x07, 0xcc, 0xf2, 0xe4, 0x31, 0x23, 0x89, 0x42, 0xc8, 0x73, 0xe1, 0xde,
	};
	u8 check_sig[32*2];

	const u8 msg[] = {
		0x30, 0x82, 0x01, 0xba, 0x02, 0x10, 0x23, 0x0e, 0xe3, 0x60, 0x46, 0x95,
		0x24, 0xce, 0xc7, 0x0b, 0xe4, 0x94, 0x18, 0x2e, 0x7e, 0xeb, 0x30, 0x08,
		0x06, 0x06, 0x2a, 0x85, 0x03, 0x02, 0x02, 0x04, 0x30, 0x69, 0x31, 0x1d,
		0x30, 0x1b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x14, 0x47, 0x6f, 0x73,
		0x74, 0x52, 0x33, 0x34, 0x31, 0x30, 0x2d, 0x39, 0x34, 0x20, 0x65, 0x78,
		0x61, 0x6d, 0x70, 0x6c, 0x65, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55,
		0x04, 0x0a, 0x0c, 0x09, 0x43, 0x72, 0x79, 0x70, 0x74, 0x6f, 0x50, 0x72,
		0x6f, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02,
		0x52, 0x55, 0x31, 0x27, 0x30, 0x25, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
		0xf7, 0x0d, 0x01, 0x09, 0x01, 0x16, 0x18, 0x47, 0x6f, 0x73, 0x74, 0x52,
		0x33, 0x34, 0x31, 0x30, 0x2d, 0x39, 0x34, 0x40, 0x65, 0x78, 0x61, 0x6d,
		0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x30, 0x1e, 0x17, 0x0d, 0x30,
		0x35, 0x30, 0x38, 0x31, 0x36, 0x31, 0x32, 0x33, 0x32, 0x35, 0x30, 0x5a,
		0x17, 0x0d, 0x31, 0x35, 0x30, 0x38, 0x31, 0x36, 0x31, 0x32, 0x33, 0x32,
		0x35, 0x30, 0x5a, 0x30, 0x69, 0x31, 0x1d, 0x30, 0x1b, 0x06, 0x03, 0x55,
		0x04, 0x03, 0x0c, 0x14, 0x47, 0x6f, 0x73, 0x74, 0x52, 0x33, 0x34, 0x31,
		0x30, 0x2d, 0x39, 0x34, 0x20, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65,
		0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x09, 0x43,
		0x72, 0x79, 0x70, 0x74, 0x6f, 0x50, 0x72, 0x6f, 0x31, 0x0b, 0x30, 0x09,
		0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x52, 0x55, 0x31, 0x27, 0x30,
		0x25, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01,
		0x16, 0x18, 0x47, 0x6f, 0x73, 0x74, 0x52, 0x33, 0x34, 0x31, 0x30, 0x2d,
		0x39, 0x34, 0x40, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63,
		0x6f, 0x6d, 0x30, 0x81, 0xa5, 0x30, 0x1c, 0x06, 0x06, 0x2a, 0x85, 0x03,
		0x02, 0x02, 0x14, 0x30, 0x12, 0x06, 0x07, 0x2a, 0x85, 0x03, 0x02, 0x02,
		0x20, 0x02, 0x06, 0x07, 0x2a, 0x85, 0x03, 0x02, 0x02, 0x1e, 0x01, 0x03,
		0x81, 0x84, 0x00, 0x04, 0x81, 0x80, 0xbb, 0x84, 0x66, 0xe1, 0x79, 0x9e,
		0x5b, 0x34, 0xd8, 0x2c, 0x80, 0x7f, 0x13, 0xa8, 0x19, 0x66, 0x71, 0x57,
		0xfe, 0x8c, 0x54, 0x25, 0x21, 0x47, 0x6f, 0x30, 0x0b, 0x27, 0x77, 0x46,
		0x98, 0xc6, 0xfb, 0x47, 0x55, 0xbe, 0xb7, 0xb2, 0xf3, 0x93, 0x6c, 0x39,
		0xb5, 0x42, 0x37, 0x26, 0x84, 0xe2, 0x0d, 0x10, 0x8a, 0x24, 0x0e, 0x1f,
		0x0c, 0x42, 0x4d, 0x2b, 0x3b, 0x11, 0x2b, 0xa8, 0xbf, 0x66, 0x39, 0x32,
		0x5c, 0x94, 0x8b, 0xc1, 0xa8, 0xfe, 0x1b, 0x63, 0x12, 0xf6, 0x09, 0x25,
		0x87, 0xcc, 0x75, 0x1b, 0xf4, 0xe5, 0x89, 0x8a, 0x09, 0x82, 0x68, 0xd3,
		0x5c, 0x77, 0xa6, 0x0f, 0xb6, 0x90, 0x10, 0x13, 0x8d, 0xe3, 0x3e, 0x7c,
		0x9c, 0x91, 0xd6, 0xac, 0x0d, 0x08, 0x2c, 0x3e, 0x78, 0xc1, 0xb5, 0xc2,
		0xb6, 0xb7, 0x1a, 0xa8, 0x2a, 0x8b, 0x45, 0x81, 0x93, 0x32, 0x32, 0x76,
		0xfa, 0x7b
	};

	gostr34_10_94_pub_key pub;
	gostr34_10_94_priv_key priv;
	nn x_;
	x_.magic = WORD(0);

	FORCE_USED_VAR(argc);
	FORCE_USED_VAR(argv);

	/* Sanity check on size for GOSTR34_10_94.
	 * NOTE: the double parentheses are here to handle -Wunreachable-code
	 */
	if((NN_USABLE_MAX_BIT_LEN) < (4096)){
		ext_printf("Error: you seem to have compiled libecc with usable NN size < 4096, not suitable for GOSTR34_10_94.\n");
		ext_printf("  => Please recompile libecc with EXTRA_CFLAGS=\"-DUSER_NN_BIT_LEN=4096\"\n");
		ext_printf("     This will increase usable NN for proper GOSTR34_10_94 up to 4096 bits.\n");
		ext_printf("     Then recompile the current examples with the same EXTRA_CFLAGS=\"-DUSER_NN_BIT_LEN=4096\" flag and execute again!\n");
		/* NOTE: ret = 0 here to pass self tests even if the library is not compatible */
		ret = 0;
		goto err;
	}

	/* Reverse the endianness of y in place as it is little endian encoded */
	ret = _reverse_endianness(&y_self_signed[0], sizeof(y_self_signed)); EG(ret, err);

	ret = gostr34_10_94_import_pub_key(&pub, p, sizeof(p), q, sizeof(q), g, sizeof(g), y_self_signed, sizeof(y_self_signed)); EG(ret, err);

	/* Verify the signature */
	ret = gostr34_10_94_verify(&pub, msg, sizeof(msg), sig, sizeof(sig), HASH_GOST34_11_94_RFC4357);
	ext_printf("RFC4357 self-signed signature result %d\n", ret);

	/****** Now check the signature procedure *********/
	/* Get a random private key 0 < x < q */
	ret = nn_get_random_mod(&x_, &(pub.q)); EG(ret, err);
	ret = _i2osp(&x_, &x[0], sizeof(x)); EG(ret, err);
	/* Import the private key */
	ret = gostr34_10_94_import_priv_key(&priv, p, sizeof(p), q, sizeof(q), g, sizeof(g), x, sizeof(x)); EG(ret, err);
	/* Compute the public key from the private key */
	ret = gostr34_10_94_compute_pub_from_priv(&pub, &priv); EG(ret, err);
	/* Sign the message */
	ret = gostr34_10_94_sign(&priv, msg, sizeof(msg), NULL, 0, check_sig, sizeof(check_sig), HASH_GOST34_11_94_RFC4357); EG(ret, err);
	/* Verify the message */
	ret = gostr34_10_94_verify(&pub, msg, sizeof(msg), check_sig, sizeof(check_sig), HASH_GOST34_11_94_RFC4357);
	ext_printf("Random signature result %d\n", ret);

err:
	nn_uninit(&x_);
	return ret;
}
#endif
