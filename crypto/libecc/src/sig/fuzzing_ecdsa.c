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
#include <libecc/lib_ecc_config.h>
#if defined(WITH_SIG_ECDSA) && defined(USE_CRYPTOFUZZ)

#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul.h>
#include <libecc/nn/nn_logical.h>

#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/ec_key.h>
#include <libecc/utils/utils.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "ECDSA"
#endif
#include <libecc/utils/dbg_sig.h>

/* NOTE: the following versions of ECDSA are "raw" with
 * no hash functions and nonce override. They are DANGEROUS and
 * should NOT be used in production mode! They are however useful
 * for corner cases tests and fuzzing.
 */

#define ECDSA_SIGN_MAGIC ((word_t)(0x80299a2bf630945bULL))
#define ECDSA_SIGN_CHECK_INITIALIZED(A, ret, err) \
        MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == ECDSA_SIGN_MAGIC), ret, err)

int ecdsa_sign_raw(struct ec_sign_context *ctx, const u8 *input, u8 inputlen, u8 *sig, u8 siglen, const u8 *nonce, u8 noncelen)
{
	const ec_priv_key *priv_key;
	prj_pt_src_t G;
	/* NOTE: hash here is not really a hash ... */
	u8 hash[LOCAL_MIN(255, BIT_LEN_WORDS(NN_MAX_BIT_LEN) * (WORDSIZE / 8))];
	bitcnt_t rshift, q_bit_len;
	prj_pt kG;
	nn_src_t q, x;
	u8 hsize, q_len;
	int ret, iszero, cmp;
	nn k, r, e, tmp, s, kinv;
#ifdef USE_SIG_BLINDING
        /* b is the blinding mask */
        nn b;
	b.magic = WORD(0);
#endif
	k.magic = r.magic = e.magic = WORD(0);
	tmp.magic = s.magic = kinv.magic = WORD(0);
	kG.magic = WORD(0);

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an ECDSA
	 * signature one and we do not finalize() before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ECDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.ecdsa), ret, err);
	MUST_HAVE((input != NULL) && (sig != NULL), ret, err);

	/* Zero init out poiny */
	ret = local_memset(&kG, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	priv_key = &(ctx->key_pair->priv_key);
	q = &(priv_key->params->ec_gen_order);
	q_bit_len = priv_key->params->ec_gen_order_bitlen;
	G = &(priv_key->params->ec_gen);
	q_len = (u8)BYTECEIL(q_bit_len);
	x = &(priv_key->x);
	hsize = inputlen;

	dbg_nn_print("p", &(priv_key->params->ec_fp.p));
	dbg_nn_print("q", &(priv_key->params->ec_gen_order));
	dbg_priv_key_print("x", priv_key);
	dbg_ec_point_print("G", &(priv_key->params->ec_gen));
	dbg_pub_key_print("Y", &(ctx->key_pair->pub_key));

	/* Check given signature buffer length has the expected size */
	MUST_HAVE((siglen == ECDSA_SIGLEN(q_bit_len)), ret, err);

	/* 1. Compute h = H(m) */
	/* NOTE: here we have raw ECDSA, this is the raw input */
	/* NOTE: the MUST_HAVE is protected by a preprocessing check
	 * to avoid -Werror=type-limits errors:
	 * "error: comparison is always true due to limited range of data type"
	 */
#if LOCAL_MIN(255, BIT_LEN_WORDS(NN_MAX_BIT_LEN) * (WORDSIZE / 8)) < 255
	MUST_HAVE(((u32)inputlen <= sizeof(hash)), ret, err);
#endif
	ret = local_memset(hash, 0, sizeof(hash)); EG(ret, err);
	ret = local_memcpy(hash, input, hsize); EG(ret, err);

	dbg_buf_print("h", hash, hsize);

	/*
	 * 2. If |h| > bitlen(q), set h to bitlen(q)
	 *    leftmost bits of h.
	 *
	 * Note that it's easier to check if the truncation has
	 * to be done here but only implement it using a logical
	 * shift at the beginning of step 3. below once the hash
	 * has been converted to an integer.
	 */
	rshift = 0;
	if ((hsize * 8) > q_bit_len) {
		rshift = (bitcnt_t)((hsize * 8) - q_bit_len);
	}

	/*
	 * 3. Compute e = OS2I(h) mod q, i.e. by converting h to an
	 *    integer and reducing it mod q
	 */
	ret = nn_init_from_buf(&e, hash, hsize); EG(ret, err);
	ret = local_memset(hash, 0, hsize); EG(ret, err);
	dbg_nn_print("h initial import as nn", &e);
	if (rshift) {
		ret = nn_rshift_fixedlen(&e, &e, rshift); EG(ret, err);
	}
	dbg_nn_print("h   final import as nn", &e);
	ret = nn_mod(&e, &e, q); EG(ret, err);
	dbg_nn_print("e", &e);

/*
     NOTE: the restart label is removed in CRYPTOFUZZ mode as
     we trigger MUST_HAVE instead of restarting in this mode.
 restart:
*/
	/* 4. get a random value k in ]0,q[ */
	/* NOTE: copy our input nonce if not NULL */
	if(nonce != NULL){
		MUST_HAVE((noncelen <= (u8)(BYTECEIL(q_bit_len))), ret, err);
		ret = nn_init_from_buf(&k, nonce, noncelen); EG(ret, err);
	}
	else{
		ret = ctx->rand(&k, q); EG(ret, err);
	}
	dbg_nn_print("k", &k);

#ifdef USE_SIG_BLINDING
	/* Note: if we use blinding, r and e are multiplied by
	 * a random value b in ]0,q[ */
        ret = nn_get_random_mod(&b, q); EG(ret, err);
        dbg_nn_print("b", &b);
#endif /* USE_SIG_BLINDING */


	/* 5. Compute W = (W_x,W_y) = kG */
#ifdef USE_SIG_BLINDING
	ret = prj_pt_mul_blind(&kG, &k, G); EG(ret, err);
#else
 	ret = prj_pt_mul(&kG, &k, G); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	ret = prj_pt_unique(&kG, &kG); EG(ret, err);

	dbg_nn_print("W_x", &(kG.X.fp_val));
	dbg_nn_print("W_y", &(kG.Y.fp_val));

	/* 6. Compute r = W_x mod q */
	ret = nn_mod(&r, &(kG.X.fp_val), q); EG(ret, err);
	dbg_nn_print("r", &r);

	/* 7. If r is 0, restart the process at step 4. */
	/* NOTE: for the CRYPTOFUZZ mode, we do not restart
	 * the procedure but throw an assert exception instead.
	 */
	ret = nn_iszero(&r, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	/* Export r */
	ret = nn_export_to_buf(sig, q_len, &r); EG(ret, err);

#ifdef USE_SIG_BLINDING
	/* Blind r with b */
	ret = nn_mod_mul(&r, &r, &b, q); EG(ret, err);

	/* Blind the message e */
	ret = nn_mod_mul(&e, &e, &b, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */

	/* tmp = xr mod q */
	ret = nn_mod_mul(&tmp, x, &r, q); EG(ret, err);
	dbg_nn_print("x*r mod q", &tmp);

	/* 8. If e == rx, restart the process at step 4. */
	/* NOTE: for the CRYPTOFUZZ mode, we do not restart
	 * the procedure but throw an assert exception instead.
	 */
	ret = nn_cmp(&e, &tmp, &cmp); EG(ret, err);
	MUST_HAVE(cmp, ret, err);

	/* 9. Compute s = k^-1 * (xr + e) mod q */

	/* tmp2 = (e + xr) mod q */
	ret = nn_mod_add(&tmp, &tmp, &e, q); EG(ret, err);
	dbg_nn_print("(xr + e) mod q", &tmp);

#ifdef USE_SIG_BLINDING
	/* In case of blinding, we compute (b*k)^-1, and
	 * b^-1 will automatically unblind (r*x) in the following
	 */
	ret = nn_mod_mul(&k, &k, &b, q); EG(ret, err);
#endif
	/* Compute k^-1 mod q */
        /* NOTE: we use Fermat's little theorem inversion for
         * constant time here. This is possible since q is prime.
         */
	ret = nn_modinv_fermat(&kinv, &k, q); EG(ret, err);

	dbg_nn_print("k^-1 mod q", &kinv);

	/* s = k^-1 * tmp2 mod q */
	ret = nn_mod_mul(&s, &tmp, &kinv, q); EG(ret, err);

	dbg_nn_print("s", &s);

	/* 10. If s is 0, restart the process at step 4. */
	/* NOTE: for the CRYPTOFUZZ mode, we do not restart
	 * the procedure but throw an assert exception instead.
	 */
	ret = nn_iszero(&s, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	/* 11. return (r,s) */
	ret = nn_export_to_buf(sig + q_len, q_len, &s);

 err:

	nn_uninit(&r);
	nn_uninit(&s);
	nn_uninit(&e);
	nn_uninit(&tmp);
	nn_uninit(&k);
	nn_uninit(&kinv);
	prj_pt_uninit(&kG);

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->sign_data.ecdsa), 0, sizeof(ecdsa_sign_data)));
	}

	/* Clean what remains on the stack */
	PTR_NULLIFY(priv_key);
	PTR_NULLIFY(G);
	PTR_NULLIFY(q);
	PTR_NULLIFY(x);
	VAR_ZEROIFY(q_len);
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(rshift);
	VAR_ZEROIFY(hsize);

#ifdef USE_SIG_BLINDING
	nn_uninit(&b);
#endif /* USE_SIG_BLINDING */

	return ret;
}

/******************************/
#define ECDSA_VERIFY_MAGIC ((word_t)(0x5155fe73e7fd51beULL))
#define ECDSA_VERIFY_CHECK_INITIALIZED(A, ret, err) \
        MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == ECDSA_VERIFY_MAGIC), ret, err)

int ecdsa_verify_raw(struct ec_verify_context *ctx, const u8 *input, u8 inputlen)
{
	prj_pt uG, vY;
	prj_pt_t W_prime;
	nn e, sinv, uv, r_prime;
	prj_pt_src_t G, Y;
	/* NOTE: hash here is not really a hash ... */
	u8 hash[LOCAL_MIN(255, BIT_LEN_WORDS(NN_MAX_BIT_LEN) * (WORDSIZE / 8))];
	bitcnt_t rshift, q_bit_len;
	nn_src_t q;
	nn *s, *r;
	u8 hsize;
	int ret, iszero, cmp;

	e.magic = sinv.magic = uv.magic = r_prime.magic = WORD(0);
	uG.magic = vY.magic = WORD(0);

	/* NOTE: we reuse uG for W_prime to optimize local variables */
	W_prime = &uG;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an ECDSA
	 * verification one and we do not finalize() before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ECDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.ecdsa), ret, err);
	MUST_HAVE((input != NULL), ret, err);

	/* Zero init points */
	ret = local_memset(&uG, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&vY, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	G = &(ctx->pub_key->params->ec_gen);
	Y = &(ctx->pub_key->y);
	q = &(ctx->pub_key->params->ec_gen_order);
	q_bit_len = ctx->pub_key->params->ec_gen_order_bitlen;
	hsize = inputlen;
	r = &(ctx->verify_data.ecdsa.r);
	s = &(ctx->verify_data.ecdsa.s);

	/* 2. Compute h = H(m) */
	/* NOTE: here we have raw ECDSA, this is the raw input */
	MUST_HAVE((input != NULL), ret, err);
	/* NOTE: the MUST_HAVE is protected by a preprocessing check
	 * to avoid -Werror=type-limits errors:
	 * "error: comparison is always true due to limited range of data type"
	 */
#if LOCAL_MIN(255, BIT_LEN_WORDS(NN_MAX_BIT_LEN) * (WORDSIZE / 8)) < 255
	MUST_HAVE(((u32)inputlen <= sizeof(hash)), ret, err);
#endif

	ret = local_memset(hash, 0, sizeof(hash)); EG(ret, err);
	ret = local_memcpy(hash, input, hsize); EG(ret, err);

	dbg_buf_print("h = H(m)", hash, hsize);

	/*
	 * 3. If |h| > bitlen(q), set h to bitlen(q)
	 *    leftmost bits of h.
	 *
	 * Note that it's easier to check here if the truncation
	 * needs to be done but implement it using a logical
	 * shift at the beginning of step 3. below once the hash
	 * has been converted to an integer.
	 */
	rshift = 0;
	if ((hsize * 8) > q_bit_len) {
		rshift = (bitcnt_t)((hsize * 8) - q_bit_len);
	}

	/*
	 * 4. Compute e = OS2I(h) mod q, by converting h to an integer
	 * and reducing it mod q
	 */
	ret = nn_init_from_buf(&e, hash, hsize); EG(ret, err);
	ret = local_memset(hash, 0, hsize); EG(ret, err);
	dbg_nn_print("h initial import as nn", &e);
	if (rshift) {
		ret = nn_rshift_fixedlen(&e, &e, rshift); EG(ret, err);
	}
	dbg_nn_print("h   final import as nn", &e);

	ret = nn_mod(&e, &e, q); EG(ret, err);
	dbg_nn_print("e", &e);

	/* Compute s^-1 mod q */
	ret = nn_modinv(&sinv, s, q); EG(ret, err);
	dbg_nn_print("s", s);
	dbg_nn_print("sinv", &sinv);
	nn_uninit(s);

	/* 5. Compute u = (s^-1)e mod q */
	ret = nn_mod_mul(&uv, &e, &sinv, q); EG(ret, err);
	dbg_nn_print("u = (s^-1)e mod q", &uv);
	ret = prj_pt_mul(&uG, &uv, G); EG(ret, err);

	/* 6. Compute v = (s^-1)r mod q */
	ret = nn_mod_mul(&uv, r, &sinv, q); EG(ret, err);
	dbg_nn_print("v = (s^-1)r mod q", &uv);
	ret = prj_pt_mul(&vY, &uv, Y); EG(ret, err);

	/* 7. Compute W' = uG + vY */
	ret = prj_pt_add(W_prime, &uG, &vY); EG(ret, err);

	/* 8. If W' is the point at infinity, reject the signature. */
	ret = prj_pt_iszero(W_prime, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	/* 9. Compute r' = W'_x mod q */
	ret = prj_pt_unique(W_prime, W_prime); EG(ret, err);
	dbg_nn_print("W'_x", &(W_prime->X.fp_val));
	dbg_nn_print("W'_y", &(W_prime->Y.fp_val));
	ret = nn_mod(&r_prime, &(W_prime->X.fp_val), q); EG(ret, err);

	/* 10. Accept the signature if and only if r equals r' */
	ret = nn_cmp(&r_prime, r, &cmp); EG(ret, err);
	ret = (cmp != 0) ? -1 : 0;

 err:
	nn_uninit(&r_prime);
	nn_uninit(&uv);
	nn_uninit(&e);
	nn_uninit(&sinv);
	prj_pt_uninit(&uG);
	prj_pt_uninit(&vY);

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->verify_data.ecdsa), 0, sizeof(ecdsa_verify_data)));
	}

	/* Clean what remains on the stack */
	PTR_NULLIFY(W_prime);
	PTR_NULLIFY(G);
	PTR_NULLIFY(Y);
	VAR_ZEROIFY(rshift);
	VAR_ZEROIFY(q_bit_len);
	PTR_NULLIFY(q);
	PTR_NULLIFY(s);
	PTR_NULLIFY(r);
	VAR_ZEROIFY(hsize);

	return ret;
}


#else /* WITH_SIG_ECDSA && USE_CRYPTOFUZZ */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_SIG_ECDSA */
