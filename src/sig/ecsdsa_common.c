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
#if (defined(WITH_SIG_ECSDSA) || defined(WITH_SIG_ECOSDSA))

#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_logical.h>

#include <libecc/sig/ecsdsa_common.h>
#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/ec_key.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "EC[O]SDSA"
#endif
#include <libecc/utils/dbg_sig.h>

/*
 * Generic *internal* helper for EC-{,O}SDSA public key initialization
 * functions. The function returns 0 on success, -1 on error.
 */
int __ecsdsa_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv,
			   ec_alg_type key_type)
{
	prj_pt_src_t G;
	int ret;

	MUST_HAVE((out_pub != NULL), ret, err);

	/* Zero init public key to be generated */
	ret = local_memset(out_pub, 0, sizeof(ec_pub_key)); EG(ret, err);

	ret = priv_key_check_initialized_and_type(in_priv, key_type); EG(ret, err);

	/* Y = xG */
	G = &(in_priv->params->ec_gen);
	/* Use blinding when computing point scalar multiplication */
	ret = prj_pt_mul_blind(&(out_pub->y), &(in_priv->x), G); EG(ret, err);

	out_pub->key_type = key_type;
	out_pub->params = in_priv->params;
	out_pub->magic = PUB_KEY_MAGIC;

err:
	return ret;
}

/*
 * Generic *internal* helper for EC{,O}SDSA signature length functions.
 * It provides signature length when exported to a buffer based on hash
 * algorithm digest and block size, generator point order bit length, and
 * uderlying prime field order bit length. The function returns 0 on success,
 * -1 on error. On success, signature length is provided via 'siglen' out
 * parameter. The function returns 0 on success, -1 on error. On success,
 * 'siglen' out parameter provides the length of signature fonction. It is
 * not meaningful on error.
 */
int __ecsdsa_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize,
		    u8 *siglen)
{
	int ret;

	MUST_HAVE((siglen != NULL), ret, err);
	MUST_HAVE(((p_bit_len <= CURVES_MAX_P_BIT_LEN) &&
		   (q_bit_len <= CURVES_MAX_Q_BIT_LEN) &&
		   (hsize <= MAX_DIGEST_SIZE) && (blocksize <= MAX_BLOCK_SIZE)),
		   ret, err);

	(*siglen) = (u8)ECSDSA_SIGLEN(hsize, q_bit_len);
	ret = 0;

err:
	return ret;
}

/*
 * Generic *internal* EC-{,O}SDSA signature functions. There purpose is to
 * allow passing specific hash functions and the random ephemeral
 * key k, so that compliance tests against test vector be made
 * without ugly hack in the code itself.
 *
 * The 'optimized' parameter tells the function if the r value of
 * the signature is computed using only the x ccordinate of the
 * the user's public key (normal version uses both coordinates).
 *
 * Normal:     r = h(Wx || Wy || m)
 * Optimized : r = h(Wx || m)
 *
 *| IUF - ECSDSA/ECOSDSA signature
 *|
 *| I	1. Get a random value k in ]0, q[
 *| I	2. Compute W = kG = (Wx, Wy)
 *| IUF 3. Compute r = H(Wx [|| Wy] || m)
 *|	   - In the normal version (ECSDSA), r = H(Wx || Wy || m).
 *|	   - In the optimized version (ECOSDSA), r = H(Wx || m).
 *|   F 4. Compute e = OS2I(r) mod q
 *|   F 5. if e == 0, restart at step 1.
 *|   F 6. Compute s = (k + ex) mod q.
 *|   F 7. if s == 0, restart at step 1.
 *|   F 8. Return (r, s)
 *
 * In the project, the normal mode is named ECSDSA, the optimized
 * one is ECOSDSA.
 *
 * Implementation note:
 *
 * In ISO-14888-3, the option is provided to the developer to check
 * whether r = 0 and restart the process in that case. Even if
 * unlikely to trigger, that check makes a lot of sense because the
 * verifier expects a non-zero value for r. In the  specification, r
 * is a string (r =  H(Wx [|| Wy] || m)). But r is used in practice
 * - both on the signer and the verifier - after conversion to an
 * integer and reduction mod q. The value resulting from that step
 * is named e (e = OS2I(r) mod q). The check for the case when r = 0
 * should be replaced by a check for e = 0. This is more conservative
 * and what is described above and done below in the implementation.
 */

#define ECSDSA_SIGN_MAGIC ((word_t)(0x743c03ae409d15c4ULL))
#define ECSDSA_SIGN_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && \
		  ((A)->magic == ECSDSA_SIGN_MAGIC), ret, err)

/*
 * Generic *internal* helper for EC-{,O}SDSA signature initialization functions.
 * The function returns 0 on success, -1 on error.
 */
int __ecsdsa_sign_init(struct ec_sign_context *ctx,
		       ec_alg_type key_type, int optimized)
{
	u8 Wx[BYTECEIL(CURVES_MAX_P_BIT_LEN)];
	u8 Wy[BYTECEIL(CURVES_MAX_P_BIT_LEN)];
	const ec_priv_key *priv_key;
	prj_pt_src_t G;
	bitcnt_t p_bit_len;
	u8 p_len;
	prj_pt kG;
	nn_src_t q;
	int ret;
	nn k;
	kG.magic = k.magic = WORD(0);

	/* First, verify context has been initialized */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);

	/* Zero init points */
	ret = local_memset(&kG, 0, sizeof(prj_pt)); EG(ret, err);

	/* Additional sanity checks on input params from context */
	ret = key_pair_check_initialized_and_type(ctx->key_pair, key_type); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		  (ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);

	/* Make things more readable */
	priv_key = &(ctx->key_pair->priv_key);
	G = &(priv_key->params->ec_gen);
	q = &(priv_key->params->ec_gen_order);
	p_bit_len = priv_key->params->ec_fp.p_bitlen;
	p_len = (u8)BYTECEIL(p_bit_len);

	dbg_nn_print("p", &(priv_key->params->ec_fp.p));
	dbg_nn_print("q", q);
	dbg_priv_key_print("x", priv_key);
	dbg_ec_point_print("G", G);
	dbg_pub_key_print("Y", &(ctx->key_pair->pub_key));

	/* 1. Get a random value k in ]0, q[ */
#ifdef NO_KNOWN_VECTORS
	/* NOTE: when we do not need self tests for known vectors,
	 * we can be strict about random function handler!
	 * This allows us to avoid the corruption of such a pointer.
	 */
	/* Sanity check on the handler before calling it */
	MUST_HAVE((ctx->rand == nn_get_random_mod), ret, err);
#endif
	MUST_HAVE((ctx->rand != NULL), ret, err);
	ret = ctx->rand(&k, q); EG(ret, err);
	dbg_nn_print("k", &k);

	/* 2. Compute W = kG = (Wx, Wy). */
#ifdef USE_SIG_BLINDING
	ret = prj_pt_mul_blind(&kG, &k, G); EG(ret, err);
#else
	ret = prj_pt_mul(&kG, &k, G); EG(ret, err);
#endif
	ret = prj_pt_unique(&kG, &kG); EG(ret, err);
	dbg_nn_print("W_x", &(kG.X.fp_val));
	dbg_nn_print("W_y", &(kG.Y.fp_val));

	/*
	 * 3. Compute r = H(Wx [|| Wy] || m)
	 *
	 *    - In the normal version (ECSDSA), r = h(Wx || Wy || m).
	 *    - In the optimized version (ECOSDSA), r = h(Wx || m).
	 */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->sign_data.ecsdsa.h_ctx)); EG(ret, err);
	ret = fp_export_to_buf(Wx, p_len, &(kG.X)); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->sign_data.ecsdsa.h_ctx), Wx, p_len); EG(ret, err);
	if (!optimized) {
		ret = fp_export_to_buf(Wy, p_len, &(kG.Y)); EG(ret, err);
		ret = ctx->h->hfunc_update(&(ctx->sign_data.ecsdsa.h_ctx), Wy,
					   p_len); EG(ret, err);
	}
	ret = local_memset(Wx, 0, p_len); EG(ret, err);
	ret = local_memset(Wy, 0, p_len); EG(ret, err);

	/* Initialize the remaining of sign context. */
	ret = nn_copy(&(ctx->sign_data.ecsdsa.k), &k); EG(ret, err);
	ctx->sign_data.ecsdsa.magic = ECSDSA_SIGN_MAGIC;

 err:
	prj_pt_uninit(&kG);
	nn_uninit(&k);

	PTR_NULLIFY(priv_key);
	PTR_NULLIFY(G);
	PTR_NULLIFY(q);
	VAR_ZEROIFY(p_len);
	VAR_ZEROIFY(p_bit_len);

	return ret;
}

/*
 * Generic *internal* helper for EC-{,O}SDSA signature update functions.
 * The function returns 0 on success, -1 on error.
 */
int __ecsdsa_sign_update(struct ec_sign_context *ctx,
			 const u8 *chunk, u32 chunklen)
{
	int ret;

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an ECSDSA
	 * signature one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ECSDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.ecsdsa), ret, err);

	/* 3. Compute r = H(Wx [|| Wy] || m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->sign_data.ecsdsa.h_ctx), chunk, chunklen); EG(ret, err);

err:
	return ret;
}

/*
 * Generic *internal* helper for EC-{,O}SDSA signature finalization functions.
 * The function returns 0 on success, -1 on error.
 */
int __ecsdsa_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen)
{
	nn_src_t q, x;
	nn s, e, ex;
	u8 r[MAX_DIGEST_SIZE];
	const ec_priv_key *priv_key;
	bitcnt_t q_bit_len;
	u8 r_len, s_len;
	u8 hsize;
	int ret;
	int iszero;
#ifdef USE_SIG_BLINDING
	/* b is the blinding mask */
	nn b, binv;
	b.magic = binv.magic = WORD(0);
#endif /* USE_SIG_BLINDING */

	s.magic = e.magic = ex.magic = WORD(0);

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an ECSDSA
	 * signature one and we do not finalize() before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ECSDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.ecsdsa), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Make things more readable */
	priv_key = &(ctx->key_pair->priv_key);
	q = &(priv_key->params->ec_gen_order);
	x = &(priv_key->x);
	q_bit_len = priv_key->params->ec_gen_order_bitlen;
	hsize = ctx->h->digest_size;
	r_len = (u8)ECSDSA_R_LEN(hsize);
	s_len = (u8)ECSDSA_S_LEN(q_bit_len);

	MUST_HAVE((siglen == ECSDSA_SIGLEN(hsize, q_bit_len)), ret, err);

#ifdef USE_SIG_BLINDING
	ret = nn_get_random_mod(&b, q); EG(ret, err);
	dbg_nn_print("b", &b);
#endif /* USE_SIG_BLINDING */

	/* 3. Compute r = H(Wx [|| Wy] || m) */
	ret = local_memset(r, 0, hsize); EG(ret, err);
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&(ctx->sign_data.ecsdsa.h_ctx), r); EG(ret, err);

	dbg_buf_print("r", r, r_len);

	/* 4. Compute e = OS2I(r) mod q */
	ret = nn_init_from_buf(&e, r, r_len); EG(ret, err);
	ret = nn_mod(&e, &e, q); EG(ret, err);
	dbg_nn_print("e", &e);

	/*
	 * 5. if e == 0, restart at step 1.
	 *
	 * As we cannot restart at that point (step 1. is in init()),
	 * we just stop and return an error.
	 */
	MUST_HAVE(!nn_iszero(&e, &iszero) && !iszero, ret, err);

#ifdef USE_SIG_BLINDING
	/* Blind e with b */
	ret = nn_mod_mul(&e, &e, &b, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */

	/* 6. Compute s = (k + ex) mod q. */
	ret = nn_mod_mul(&ex, x, &e, q); EG(ret, err);
#ifdef USE_SIG_BLINDING
	/* Blind k with b */
	ret = nn_mod_mul(&s, &(ctx->sign_data.ecsdsa.k), &b, q); EG(ret, err);
	ret = nn_mod_add(&s, &s, &ex, q); EG(ret, err);
#else
	ret = nn_mod_add(&s, &(ctx->sign_data.ecsdsa.k), &ex, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */

#ifdef USE_SIG_BLINDING
	/* Unblind s */
        /* NOTE: we use Fermat's little theorem inversion for
         * constant time here. This is possible since q is prime.
         */
	ret = nn_modinv_fermat(&binv, &b, q); EG(ret, err);
	ret = nn_mod_mul(&s, &s, &binv, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	dbg_nn_print("s", &s);

	/*
	 * 7. if s == 0, restart at step 1.
	 *
	 * As we cannot restart at that point (step 1. is in init()),
	 * we just stop and return an error.
	 */
	MUST_HAVE((!nn_iszero(&s, &iszero)) && (!iszero), ret, err);

	/* 8. Return (r, s) */
	ret = local_memcpy(sig, r, r_len); EG(ret, err);
	ret = local_memset(r, 0, r_len); EG(ret, err);
	ret = nn_export_to_buf(sig + r_len, s_len, &s);

 err:
	nn_uninit(&s);
	nn_uninit(&e);
	nn_uninit(&ex);
#ifdef USE_SIG_BLINDING
	nn_uninit(&b);
	nn_uninit(&binv);
#endif /* USE_SIG_BLINDING */

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->sign_data.ecsdsa), 0, sizeof(ecsdsa_sign_data)));
	}

	/* Clean what remains on the stack */
	PTR_NULLIFY(q);
	PTR_NULLIFY(x);
	PTR_NULLIFY(priv_key);
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(r_len);
	VAR_ZEROIFY(s_len);
	VAR_ZEROIFY(hsize);

	return ret;
}

/* local helper for context sanity checks. Returns 0 on success, -1 on error. */
#define ECSDSA_VERIFY_MAGIC ((word_t)(0x8eac1ff89995bb0aULL))
#define ECSDSA_VERIFY_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((const void *)(A)) != NULL) && \
		  ((A)->magic == ECSDSA_VERIFY_MAGIC), ret, err)

/*
 *| IUF - ECSDSA/ECOSDSA verification
 *|
 *| I	1. if s is not in ]0,q[, reject the signature.
 *| I	2. Compute e = -r mod q
 *| I	3. If e == 0, reject the signature.
 *| I	4. Compute W' = sG + eY
 *| IUF 5. Compute r' = H(W'x [|| W'y] || m)
 *|	   - In the normal version (ECSDSA), r' = H(W'x || W'y || m).
 *|	   - In the optimized version (ECOSDSA), r' = H(W'x || m).
 *|   F 6. Accept the signature if and only if r and r' are the same
 */

/*
 * Generic *internal* helper for EC-{,O}SDSA verification initialization functions.
 * The function returns 0 on success, -1 on error.
 */
int __ecsdsa_verify_init(struct ec_verify_context *ctx,
			 const u8 *sig, u8 siglen,
			 ec_alg_type key_type, int optimized)
{
	prj_pt_src_t G, Y;
	const ec_pub_key *pub_key;
	nn_src_t q;
	nn rmodq, e, r, s;
	prj_pt sG, eY;
	prj_pt_t Wprime;
	u8 Wprimex[BYTECEIL(CURVES_MAX_P_BIT_LEN)];
	u8 Wprimey[BYTECEIL(CURVES_MAX_P_BIT_LEN)];
	u8 p_len, r_len, s_len;
	bitcnt_t q_bit_len;
	u8 hsize;
	int ret, iszero, cmp;

	rmodq.magic = e.magic = r.magic = s.magic = WORD(0);
	sG.magic = eY.magic = WORD(0);

	/* NOTE: we reuse sG for Wprime to optimize local variables */
	Wprime = &sG;

	/* First, verify context has been initialized */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);

	/* Zero init points */
	ret = local_memset(&sG, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&eY, 0, sizeof(prj_pt)); EG(ret, err);

	/* Do some sanity checks on input params */
	ret = pub_key_check_initialized_and_type(ctx->pub_key, key_type); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		  (ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Make things more readable */
	pub_key = ctx->pub_key;
	G = &(pub_key->params->ec_gen);
	Y = &(pub_key->y);
	q = &(pub_key->params->ec_gen_order);
	p_len = (u8)BYTECEIL(pub_key->params->ec_fp.p_bitlen);
	q_bit_len = pub_key->params->ec_gen_order_bitlen;
	hsize = ctx->h->digest_size;
	r_len = (u8)ECSDSA_R_LEN(hsize);
	s_len = (u8)ECSDSA_S_LEN(q_bit_len);

	MUST_HAVE((siglen == ECSDSA_SIGLEN(hsize, q_bit_len)), ret, err);

	/* 1. if s is not in ]0,q[, reject the signature. */
	ret = nn_init_from_buf(&s, sig + r_len, s_len); EG(ret, err);
	ret = nn_iszero(&s, &iszero); EG(ret, err);
	ret = nn_cmp(&s, q, &cmp); EG(ret, err);
	MUST_HAVE((!iszero) && (cmp < 0), ret, err);

	/*
	 * 2. Compute e = -r mod q
	 *
	 * To avoid dealing w/ negative numbers, we simply compute
	 * e = -r mod q = q - (r mod q) (except when r is 0).
	 */
	ret = nn_init_from_buf(&r, sig, r_len); EG(ret, err);
	ret = nn_mod(&rmodq, &r, q); EG(ret, err);
	ret = nn_mod_neg(&e, &rmodq, q); EG(ret, err);

	/* 3. If e == 0, reject the signature. */
	ret = nn_iszero(&e, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	/* 4. Compute W' = sG + eY */
	ret = prj_pt_mul(&sG, &s, G); EG(ret, err);
	ret = prj_pt_mul(&eY, &e, Y); EG(ret, err);
	ret = prj_pt_add(Wprime, &sG, &eY); EG(ret, err);
	ret = prj_pt_unique(Wprime, Wprime); EG(ret, err);

	/*
	 * 5. Compute r' = H(W'x [|| W'y] || m)
	 *
	 *    - In the normal version (ECSDSA), r = h(W'x || W'y || m).
	 *    - In the optimized version (ECOSDSA), r = h(W'x || m).
	 */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->verify_data.ecsdsa.h_ctx)); EG(ret, err);
	ret = fp_export_to_buf(Wprimex, p_len, &(Wprime->X)); EG(ret, err);
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->verify_data.ecsdsa.h_ctx), Wprimex, p_len); EG(ret, err);
	if (!optimized) {
		ret = fp_export_to_buf(Wprimey, p_len, &(Wprime->Y)); EG(ret, err);
		/* Since we call a callback, sanity check our mapping */
		ret =  hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
		ret = ctx->h->hfunc_update(&(ctx->verify_data.ecsdsa.h_ctx),
					   Wprimey, p_len); EG(ret, err);
	}
	ret = local_memset(Wprimex, 0, p_len); EG(ret, err);
	ret = local_memset(Wprimey, 0, p_len); EG(ret, err);

	/* Initialize the remaining of verify context. */
	ret = local_memcpy(ctx->verify_data.ecsdsa.r, sig, r_len); EG(ret, err);
	ret = nn_copy(&(ctx->verify_data.ecsdsa.s), &s); EG(ret, err);

	ctx->verify_data.ecsdsa.magic = ECSDSA_VERIFY_MAGIC;

 err:
	nn_uninit(&rmodq);
	nn_uninit(&e);
	nn_uninit(&r);
	nn_uninit(&s);
	prj_pt_uninit(&sG);
	prj_pt_uninit(&eY);

	/* Clean what remains on the stack */
	PTR_NULLIFY(Wprime);
	PTR_NULLIFY(G);
	PTR_NULLIFY(Y);
	PTR_NULLIFY(pub_key);
	PTR_NULLIFY(q);
	VAR_ZEROIFY(p_len);
	VAR_ZEROIFY(r_len);
	VAR_ZEROIFY(s_len);
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(hsize);

	return ret;
}

/*
 * Generic *internal* helper for EC-{,O}SDSA verification update functions.
 * The function returns 0 on success, -1 on error.
 */
int __ecsdsa_verify_update(struct ec_verify_context *ctx,
			   const u8 *chunk, u32 chunklen)
{
	int ret;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an ECSDSA
	 * verification one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ECSDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.ecsdsa), ret, err);

	/* 5. Compute r' = H(W'x [|| W'y] || m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->verify_data.ecsdsa.h_ctx), chunk,
				chunklen);

err:
	return ret;
}

/*
 * Generic *internal* helper for EC-{,O}SDSA verification finalization
 * functions. The function returns 0 on success, -1 on error.
 */
int __ecsdsa_verify_finalize(struct ec_verify_context *ctx)
{
	u8 r_prime[MAX_DIGEST_SIZE];
	u32 r_len;
	int ret, check;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an ECSDSA
	 * verification one and we do not finalize() before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ECSDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.ecsdsa), ret, err);

	r_len = ECSDSA_R_LEN(ctx->h->digest_size);

	/* 5. Compute r' = H(W'x [|| W'y] || m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&(ctx->verify_data.ecsdsa.h_ctx), r_prime); EG(ret, err);

	/* 6. Accept the signature if and only if r and r' are the same */
	ret = are_equal(ctx->verify_data.ecsdsa.r, r_prime, r_len, &check); EG(ret, err);
	ret = check ? 0 : -1;

err:
	IGNORE_RET_VAL(local_memset(r_prime, 0, sizeof(r_prime)));
	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->verify_data.ecsdsa), 0,
			     sizeof(ecsdsa_verify_data)));
	}

	/* Clean what remains on the stack */
	VAR_ZEROIFY(r_len);

	return ret;
}

#else /* (defined(WITH_SIG_ECSDSA) || defined(WITH_SIG_ECOSDSA)) */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* (defined(WITH_SIG_ECSDSA) || defined(WITH_SIG_ECOSDSA)) */
