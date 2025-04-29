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
#ifdef WITH_SIG_ECFSDSA

#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_logical.h>

#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/sig_algs.h>
#include <libecc/sig/ec_key.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "ECFSDSA"
#endif
#include <libecc/utils/dbg_sig.h>

int ecfsdsa_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv)
{
	int ret, cmp;
	prj_pt_src_t G;
	nn_src_t q;

	MUST_HAVE((out_pub != NULL), ret, err);

	/* Zero init public key to be generated */
	ret = local_memset(out_pub, 0, sizeof(ec_pub_key)); EG(ret, err);

	ret = priv_key_check_initialized_and_type(in_priv, ECFSDSA); EG(ret, err);
	q = &(in_priv->params->ec_gen_order);

	/* Sanity check on key compliance */
	MUST_HAVE(!nn_cmp(&(in_priv->x), q, &cmp) && (cmp < 0), ret, err);

	/* Y = xG */
	G = &(in_priv->params->ec_gen);
	/* Use blinding when computing point scalar multiplication */
	ret = prj_pt_mul_blind(&(out_pub->y), &(in_priv->x), G); EG(ret, err);

	out_pub->key_type = ECFSDSA;
	out_pub->params = in_priv->params;
	out_pub->magic = PUB_KEY_MAGIC;

err:
	return ret;
}

int ecfsdsa_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen)
{
	int ret;

	MUST_HAVE((siglen != NULL), ret, err);
	MUST_HAVE((p_bit_len <= CURVES_MAX_P_BIT_LEN) &&
		  (q_bit_len <= CURVES_MAX_Q_BIT_LEN) &&
		  (hsize <= MAX_DIGEST_SIZE) && (blocksize <= MAX_BLOCK_SIZE), ret, err);
	(*siglen) = (u8)ECFSDSA_SIGLEN(p_bit_len, q_bit_len);
	ret = 0;

err:
	return ret;
}

/*
 * Generic *internal* ECFSDSA signature functions (init, update and finalize).
 * Their purpose is to allow passing a specific hash function (along with
 * their output size) and the random ephemeral key k, so that compliance
 * tests against test vectors can be made without ugly hack in the code
 * itself.
 *
 * Global EC-FSDSA signature process is as follows (I,U,F provides
 * information in which function(s) (init(), update() or finalize())
 * a specific step is performed):
 *
 *| IUF - ECFSDSA signature
 *|
 *| I	1. Get a random value k in ]0,q[
 *| I	2. Compute W = (W_x,W_y) = kG
 *| I	3. Compute r = FE2OS(W_x)||FE2OS(W_y)
 *| I	4. If r is an all zero string, restart the process at step 1.
 *| IUF 5. Compute h = H(r||m)
 *|   F 6. Compute e = OS2I(h) mod q
 *|   F 7. Compute s = (k + ex) mod q
 *|   F 8. If s is 0, restart the process at step 1 (see c. below)
 *|   F 9. Return (r,s)
 *
 * Implementation notes:
 *
 * a) sig is built as the concatenation of r and s. r is encoded on
 *    2*ceil(bitlen(p)) bytes and s on ceil(bitlen(q)) bytes.
 * b) in EC-FSDSA, the public part of the key is not needed per se during
 *    the signature but - as it is needed in other signature algs implemented
 *    in the library - the whole key pair is passed instead of just the
 *    private key.
 * c) Implementation of EC-FSDSA in an init()/update()/finalize() logic
 *    cannot be made deterministic, in the sense that if s is 0 at step
 *    8 above, there is no way to restart the whole signature process
 *    w/o rehashing m. So, even if the event is extremely unlikely,
 *    signature process may fail to provide a signature of the data
 *    during finalize() call.
 */

#define ECFSDSA_SIGN_MAGIC ((word_t)(0x1ed9635924b48ddaULL))
#define ECFSDSA_SIGN_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && \
		  ((A)->magic == ECFSDSA_SIGN_MAGIC), ret, err)

int _ecfsdsa_sign_init(struct ec_sign_context *ctx)
{
	prj_pt_src_t G;
	nn_src_t q;
	nn *k;
	u8 *r;
	prj_pt kG;
	const ec_priv_key *priv_key;
	bitcnt_t p_bit_len;
	u8 i, p_len, r_len;
	int ret;
	kG.magic = WORD(0);

	/* First, verify context has been initialized */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);

	/* Zero init points */
	ret = local_memset(&kG, 0, sizeof(prj_pt)); EG(ret, err);

	/* Additional sanity checks on input params from context */
	ret = key_pair_check_initialized_and_type(ctx->key_pair, ECFSDSA); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		  (ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);

	/* Make things more readable */
	priv_key = &(ctx->key_pair->priv_key);
	G = &(priv_key->params->ec_gen);
	q = &(priv_key->params->ec_gen_order);
	r = ctx->sign_data.ecfsdsa.r;
	k = &(ctx->sign_data.ecfsdsa.k);
	p_bit_len = priv_key->params->ec_fp.p_bitlen;
	MUST_HAVE(((u32)BYTECEIL(p_bit_len) <= NN_MAX_BYTE_LEN), ret, err);
	p_len = (u8)BYTECEIL(p_bit_len);
	r_len = (u8)ECFSDSA_R_LEN(p_bit_len);

	dbg_nn_print("p", &(priv_key->params->ec_fp.p));
	dbg_nn_print("q", q);
	dbg_priv_key_print("x", priv_key);
	dbg_ec_point_print("G", G);
	dbg_pub_key_print("Y", &(ctx->key_pair->pub_key));

 restart:

	/*  1. Get a random value k in ]0,q[ */
#ifdef NO_KNOWN_VECTORS
	/* NOTE: when we do not need self tests for known vectors,
	 * we can be strict about random function handler!
	 * This allows us to avoid the corruption of such a pointer.
	 */
	/* Sanity check on the handler before calling it */
	MUST_HAVE((ctx->rand == nn_get_random_mod), ret, err);
#endif
	MUST_HAVE((ctx->rand != NULL), ret, err);
	ret = ctx->rand(k, q); EG(ret, err);

	/*  2. Compute W = (W_x,W_y) = kG */
#ifdef USE_SIG_BLINDING
	/* We use blinding for the scalar multiplication */
	ret = prj_pt_mul_blind(&kG, k, G); EG(ret, err);
#else
	ret = prj_pt_mul(&kG, k, G); EG(ret, err);
#endif
	ret = prj_pt_unique(&kG, &kG); EG(ret, err);

	dbg_nn_print("Wx", &(kG.X.fp_val));
	dbg_nn_print("Wy", &(kG.Y.fp_val));

	/*  3. Compute r = FE2OS(W_x)||FE2OS(W_y) */
	ret = fp_export_to_buf(r, p_len, &(kG.X)); EG(ret, err);
	ret = fp_export_to_buf(r + p_len, p_len, &(kG.Y)); EG(ret, err);
	dbg_buf_print("r: ", r, r_len);

	/*  4. If r is an all zero string, restart the process at step 1. */
	ret = 0;
	for (i = 0; i < r_len; i++) {
		ret |= r[i];
	}
	if (ret == 0) {
		goto restart;
	}

	/*  5. Compute h = H(r||m).
	 *
	 * Note that we only start the hash work here by initializing the hash
	 * context and processing r. Message m will be handled during following
	 * update() calls.
	 */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->sign_data.ecfsdsa.h_ctx)); EG(ret, err);
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->sign_data.ecfsdsa.h_ctx), r, r_len); EG(ret, err);

	ctx->sign_data.ecfsdsa.magic = ECFSDSA_SIGN_MAGIC;

 err:
	prj_pt_uninit(&kG);

	PTR_NULLIFY(G);
	PTR_NULLIFY(q);
	PTR_NULLIFY(k);
	PTR_NULLIFY(r);
	PTR_NULLIFY(priv_key);
	VAR_ZEROIFY(i);
	VAR_ZEROIFY(p_len);
	VAR_ZEROIFY(r_len);

	return ret;
}

int _ecfsdsa_sign_update(struct ec_sign_context *ctx,
			 const u8 *chunk, u32 chunklen)
{
	int ret;

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an ECFSDSA
	 * signature one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ECFSDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.ecfsdsa), ret, err);

	/*  5. Compute h = H(r||m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->sign_data.ecfsdsa.h_ctx), chunk, chunklen); EG(ret, err);

err:
	return ret;
}

int _ecfsdsa_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen)
{
	nn_src_t q, x;
	nn s, e, ex, *k;
	const ec_priv_key *priv_key;
	u8 e_buf[MAX_DIGEST_SIZE];
	bitcnt_t p_bit_len, q_bit_len;
	u8 hsize, s_len, r_len;
	int ret, iszero, cmp;
	u8 *r;

#ifdef USE_SIG_BLINDING
	/* b is the blinding mask */
	nn b, binv;
	b.magic = binv.magic = WORD(0);
#endif /* USE_SIG_BLINDING */

	s.magic = e.magic = ex.magic = WORD(0);

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an ECFSDSA
	 * signature one and we do not finalize() before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ECFSDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.ecfsdsa), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Make things more readable */
	priv_key = &(ctx->key_pair->priv_key);
	q = &(priv_key->params->ec_gen_order);
	x = &(priv_key->x);
	p_bit_len = ctx->key_pair->priv_key.params->ec_fp.p_bitlen;
	q_bit_len = ctx->key_pair->priv_key.params->ec_gen_order_bitlen;
	k = &(ctx->sign_data.ecfsdsa.k);
	r_len = (u8)ECFSDSA_R_LEN(p_bit_len);
	s_len = (u8)ECFSDSA_S_LEN(q_bit_len);
	hsize = ctx->h->digest_size;
	r = ctx->sign_data.ecfsdsa.r;

	/* Sanity check */
	ret = nn_cmp(x, q, &cmp); EG(ret, err);
	/* This should not happen and means that our
	 * private key is not compliant!
	 */
	MUST_HAVE((cmp < 0), ret, err);

	MUST_HAVE((siglen == ECFSDSA_SIGLEN(p_bit_len, q_bit_len)), ret, err);

#ifdef USE_SIG_BLINDING
	ret = nn_get_random_mod(&b, q); EG(ret, err);
	dbg_nn_print("b", &b);
#endif /* USE_SIG_BLINDING */

	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	/*  5. Compute h = H(r||m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&(ctx->sign_data.ecfsdsa.h_ctx), e_buf); EG(ret, err);
	dbg_buf_print("h(R||m)", e_buf, hsize);

	/*  6. Compute e by converting h to an integer and reducing it mod q */
	ret = nn_init_from_buf(&e, e_buf, hsize); EG(ret, err);
	ret = local_memset(e_buf, 0, hsize); EG(ret, err);
	ret = nn_mod(&e, &e, q); EG(ret, err);

#ifdef USE_SIG_BLINDING
	/* Blind e with b */
	ret = nn_mod_mul(&e, &e, &b, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	/*  7. Compute s = (k + ex) mod q */
	ret = nn_mod_mul(&ex, &e, x, q); EG(ret, err);
#ifdef USE_SIG_BLINDING
	/* Blind k with b */
	ret = nn_mod_mul(&s, k, &b, q); EG(ret, err);
	ret = nn_mod_add(&s, &s, &ex, q); EG(ret, err);
#else
	ret = nn_mod_add(&s, k, &ex, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */
#ifdef USE_SIG_BLINDING
	/* Unblind s */
	/* NOTE: we use Fermat's little theorem inversion for
	 * constant time here. This is possible since q is prime.
	 */
	ret = nn_modinv_fermat(&binv, &b, q); EG(ret, err);
	ret = nn_mod_mul(&s, &s, &binv, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	dbg_nn_print("s: ", &s);

	/*
	 * 8. If s is 0, restart the process at step 1.
	 *
	 * In practice, as we cannot restart the whole process in
	 * finalize() we just report an error.
	 */
	MUST_HAVE((!nn_iszero(&s, &iszero)) && (!iszero), ret, err);

	/*  9. Return (r,s) */
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
#endif

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->sign_data.ecfsdsa), 0, sizeof(ecfsdsa_sign_data)));
	}

	PTR_NULLIFY(q);
	PTR_NULLIFY(x);
	PTR_NULLIFY(k);
	PTR_NULLIFY(priv_key);
	PTR_NULLIFY(r);
	VAR_ZEROIFY(hsize);
	VAR_ZEROIFY(p_bit_len);
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(r_len);
	VAR_ZEROIFY(s_len);

	return ret;
}

/*
 * Generic *internal* ECFSDSA verification functions (init, update and
 * finalize). Their purpose is to allow passing a specific hash function
 * (along with their output size) and the random ephemeral key k, so
 * that compliance tests against test vectors can be made without ugly
 * hack in the code itself.
 *
 * Global EC-FSDSA verification process is as follows (I,U,F provides
 * information in which function(s) (init(), update() or finalize())
 * a specific step is performed):
 *
 *| IUF - ECFSDSA verification
 *|
 *| I	1. Reject the signature if r is not a valid point on the curve.
 *| I	2. Reject the signature if s is not in ]0,q[
 *| IUF 3. Compute h = H(r||m)
 *|   F 4. Convert h to an integer and then compute e = -h mod q
 *|   F 5. compute W' = sG + eY, where Y is the public key
 *|   F 6. Compute r' = FE2OS(W'_x)||FE2OS(W'_y)
 *|   F 7. Accept the signature if and only if r equals r'
 *
 */

#define ECFSDSA_VERIFY_MAGIC ((word_t)(0x26afb13ccd96fa04ULL))
#define ECFSDSA_VERIFY_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && \
		  ((A)->magic == ECFSDSA_VERIFY_MAGIC), ret, err)

int _ecfsdsa_verify_init(struct ec_verify_context *ctx,
			 const u8 *sig, u8 siglen)
{
	bitcnt_t p_bit_len, q_bit_len;
	u8 p_len, r_len, s_len;
	int ret, iszero, on_curve, cmp;
	const u8 *r;
	nn_src_t q;
	fp rx, ry;
	nn *s;

	rx.magic = ry.magic = WORD(0);

	/* First, verify context has been initialized */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);

	/* Do some sanity checks on input params */
	ret = pub_key_check_initialized_and_type(ctx->pub_key, ECFSDSA); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		(ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Make things more readable */
	q = &(ctx->pub_key->params->ec_gen_order);
	p_bit_len = ctx->pub_key->params->ec_fp.p_bitlen;
	q_bit_len = ctx->pub_key->params->ec_gen_order_bitlen;
	p_len = (u8)BYTECEIL(p_bit_len);
	r_len = (u8)ECFSDSA_R_LEN(p_bit_len);
	s_len = (u8)ECFSDSA_S_LEN(q_bit_len);
	s = &(ctx->verify_data.ecfsdsa.s);

	MUST_HAVE((siglen == ECFSDSA_SIGLEN(p_bit_len, q_bit_len)), ret, err);

	/*  1. Reject the signature if r is not a valid point on the curve. */

	/* Let's first import r, i.e. x and y coordinates of the point */
	r = sig;
	ret = fp_init(&rx, ctx->pub_key->params->ec_curve.a.ctx); EG(ret, err);
	ret = fp_import_from_buf(&rx, r, p_len); EG(ret, err);
	ret = fp_init(&ry, ctx->pub_key->params->ec_curve.a.ctx); EG(ret, err);
	ret = fp_import_from_buf(&ry, r + p_len, p_len); EG(ret, err);

	/* Let's now check that r represents a point on the curve */
	ret = is_on_shortw_curve(&rx, &ry, &(ctx->pub_key->params->ec_curve), &on_curve); EG(ret, err);
	MUST_HAVE(on_curve, ret, err);

	/* 2. Reject the signature if s is not in ]0,q[ */

	/* Import s as a nn */
	ret = nn_init_from_buf(s, sig + r_len, s_len); EG(ret, err);

	/* Check that s is in ]0,q[ */
	ret = nn_iszero(s, &iszero); EG(ret, err);
	ret = nn_cmp(s, q, &cmp); EG(ret, err);
	MUST_HAVE((!iszero) && (cmp < 0), ret, err);

	/* 3. Compute h = H(r||m) */

	/* Initialize the verify context */
	ret = local_memcpy(&(ctx->verify_data.ecfsdsa.r), r, r_len); EG(ret, err);
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->verify_data.ecfsdsa.h_ctx)); EG(ret, err);

	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->verify_data.ecfsdsa.h_ctx), r, r_len); EG(ret, err);

	ctx->verify_data.ecfsdsa.magic = ECFSDSA_VERIFY_MAGIC;

 err:
	fp_uninit(&rx);
	fp_uninit(&ry);

	if (ret && (ctx != NULL)) {
		/*
		 * Signature is invalid. Clear data part of the context.
		 * This will clear magic and avoid further reuse of the
		 * whole context.
		 */
		IGNORE_RET_VAL(local_memset(&(ctx->verify_data.ecfsdsa), 0,
			     sizeof(ecfsdsa_verify_data)));
	}

	VAR_ZEROIFY(p_len);
	VAR_ZEROIFY(r_len);
	VAR_ZEROIFY(s_len);
	VAR_ZEROIFY(p_bit_len);
	VAR_ZEROIFY(q_bit_len);
	PTR_NULLIFY(r);
	PTR_NULLIFY(q);
	PTR_NULLIFY(s);

	return ret;
}

int _ecfsdsa_verify_update(struct ec_verify_context *ctx,
			   const u8 *chunk, u32 chunklen)
{
	int ret;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an ECFSDSA
	 * verification one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ECFSDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.ecfsdsa), ret, err);

	/* 3. Compute h = H(r||m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->verify_data.ecfsdsa.h_ctx), chunk,
			     chunklen);

err:
	return ret;
}

int _ecfsdsa_verify_finalize(struct ec_verify_context *ctx)
{
	prj_pt_src_t G, Y;
	nn_src_t q;
	nn tmp, e, *s;
	prj_pt sG, eY;
	prj_pt_t Wprime;
	bitcnt_t p_bit_len, r_len;
	u8 r_prime[2 * NN_MAX_BYTE_LEN];
	u8 e_buf[MAX_DIGEST_SIZE];
	u8 hsize, p_len;
	const u8 *r;
	int ret, check;

	tmp.magic = e.magic = WORD(0);
	sG.magic = eY.magic = WORD(0);

	/* NOTE: we reuse sG for Wprime to optimize local variables */
	Wprime = &sG;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an ECFSDSA
	 * verification one and we do not finalize() before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ECFSDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.ecfsdsa), ret, err);

	/* Zero init points */
	ret = local_memset(&sG, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&eY, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	G = &(ctx->pub_key->params->ec_gen);
	Y = &(ctx->pub_key->y);
	q = &(ctx->pub_key->params->ec_gen_order);
	hsize = ctx->h->digest_size;
	s = &(ctx->verify_data.ecfsdsa.s);
	r = ctx->verify_data.ecfsdsa.r;
	p_bit_len = ctx->pub_key->params->ec_fp.p_bitlen;
	p_len = (u8)BYTECEIL(p_bit_len);
	r_len = (u8)ECFSDSA_R_LEN(p_bit_len);

	/* 3. Compute h = H(r||m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&(ctx->verify_data.ecfsdsa.h_ctx), e_buf); EG(ret, err);

	/*
	 * 4. Convert h to an integer and then compute e = -h mod q
	 *
	 * Because we only support positive integers, we compute
	 * e = q - (h mod q) (except when h is 0).
	 */
	ret = nn_init_from_buf(&tmp, e_buf, hsize); EG(ret, err);
	ret = local_memset(e_buf, 0, hsize); EG(ret, err);
	ret = nn_mod(&tmp, &tmp, q); EG(ret, err);

	ret = nn_mod_neg(&e, &tmp, q); EG(ret, err);

	/* 5. compute W' = (W'_x,W'_y) = sG + tY, where Y is the public key */
	ret = prj_pt_mul(&sG, s, G); EG(ret, err);
	ret = prj_pt_mul(&eY, &e, Y); EG(ret, err);
	ret = prj_pt_add(Wprime, &sG, &eY); EG(ret, err);
	ret = prj_pt_unique(Wprime, Wprime); EG(ret, err);

	/* 6. Compute r' = FE2OS(W'_x)||FE2OS(W'_y) */
	ret = fp_export_to_buf(r_prime, p_len, &(Wprime->X)); EG(ret, err);
	ret = fp_export_to_buf(r_prime + p_len, p_len, &(Wprime->Y)); EG(ret, err);

	dbg_buf_print("r_prime: ", r_prime, r_len);

	/* 7. Accept the signature if and only if r equals r' */
	ret = are_equal(r, r_prime, r_len, &check); EG(ret, err);
	ret = check ? 0 : -1;

err:
	IGNORE_RET_VAL(local_memset(r_prime, 0, sizeof(r_prime)));

	nn_uninit(&tmp);
	nn_uninit(&e);
	prj_pt_uninit(&sG);
	prj_pt_uninit(&eY);

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->verify_data.ecfsdsa), 0,
			     sizeof(ecfsdsa_verify_data)));
	}

	/* Clean what remains on the stack */
	PTR_NULLIFY(Wprime);
	PTR_NULLIFY(G);
	PTR_NULLIFY(Y);
	PTR_NULLIFY(q);
	PTR_NULLIFY(s);
	PTR_NULLIFY(r);
	VAR_ZEROIFY(p_len);
	VAR_ZEROIFY(r_len);
	VAR_ZEROIFY(hsize);

	return ret;
}

/*
 * NOTE: among all the EC-SDSA ISO14888-3 variants, only EC-FSDSA supports
 * batch verification as it is the only one allowing the recovery of the
 * underlying signature point from the signature value (other variants make
 * use of a hash of (parts) of this point.
 */
/* Batch verification function:
 * This function takes multiple signatures/messages/public keys, and
 * checks in an optimized way all the signatures.
 *
 * This returns 0 if *all* the signatures are correct, and -1 if at least
 * one signature is not correct.
 *
 */
static int _ecfsdsa_verify_batch_no_memory(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
					   const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
					   hash_alg_type hash_type, const u8 **adata, const u16 *adata_len)
{
	nn_src_t q = NULL;
	prj_pt_src_t G = NULL;
	prj_pt_t W = NULL, Y = NULL;
	prj_pt Tmp, W_sum, Y_sum;
	nn S, S_sum, e, a;
	u8 hash[MAX_DIGEST_SIZE];
	const ec_pub_key *pub_key, *pub_key0;
	int ret, iszero, cmp;
	prj_pt_src_t pub_key_y;
	hash_context h_ctx;
	const hash_mapping *hm;
	ec_shortw_crv_src_t shortw_curve;
	ec_alg_type key_type = UNKNOWN_ALG;
	bitcnt_t p_bit_len, q_bit_len;
	u8 p_len, q_len;
	u16 hsize;
	u32 i;

	Tmp.magic = W_sum.magic = Y_sum.magic = WORD(0);
	S.magic = S_sum.magic = e.magic = a.magic = WORD(0);

	FORCE_USED_VAR(adata_len);
	FORCE_USED_VAR(adata);

	/* First, some sanity checks */
	MUST_HAVE((s != NULL) && (pub_keys != NULL) && (m != NULL), ret, err);
	/* We need at least one element in our batch data bags */
	MUST_HAVE((num > 0), ret, err);

	/* Zeroize buffers */
	ret = local_memset(hash, 0, sizeof(hash)); EG(ret, err);

	pub_key0 = pub_keys[0];
	MUST_HAVE((pub_key0 != NULL), ret, err);

	/* Get our hash mapping */
	ret = get_hash_by_type(hash_type, &hm); EG(ret, err);
	hsize = hm->digest_size;
	MUST_HAVE((hm != NULL), ret, err);

	for(i = 0; i < num; i++){
		u8 siglen;
		const u8 *sig = NULL;

		ret = pub_key_check_initialized_and_type(pub_keys[i], ECFSDSA); EG(ret, err);

		/* Make things more readable */
		pub_key = pub_keys[i];

		/* Sanity check that all our public keys have the same parameters */
		MUST_HAVE((pub_key->params) == (pub_key0->params), ret, err);

		q = &(pub_key->params->ec_gen_order);
		shortw_curve = &(pub_key->params->ec_curve);
		pub_key_y = &(pub_key->y);
		key_type = pub_key->key_type;
		G = &(pub_key->params->ec_gen);
		p_bit_len = pub_key->params->ec_fp.p_bitlen;
		q_bit_len = pub_key->params->ec_gen_order_bitlen;
		p_len = (u8)BYTECEIL(p_bit_len);
		q_len = (u8)BYTECEIL(q_bit_len);

		/* Check given signature length is the expected one */
		siglen = s_len[i];
		sig = s[i];
		MUST_HAVE((siglen == ECFSDSA_SIGLEN(p_bit_len, q_bit_len)), ret, err);
		MUST_HAVE((siglen == (ECFSDSA_R_LEN(p_bit_len) + ECFSDSA_S_LEN(q_bit_len))), ret, err);

		/* Check the key type versus the algorithm */
		MUST_HAVE((key_type == sig_type), ret, err);

		if(i == 0){
			/* Initialize our sums to zero/point at infinity */
			ret = nn_init(&S_sum, 0); EG(ret, err);
			ret = prj_pt_init(&W_sum, shortw_curve); EG(ret, err);
			ret = prj_pt_zero(&W_sum); EG(ret, err);
			ret = prj_pt_init(&Y_sum, shortw_curve); EG(ret, err);
			ret = prj_pt_zero(&Y_sum); EG(ret, err);
			ret = prj_pt_init(&Tmp, shortw_curve); EG(ret, err);
			ret = nn_init(&e, 0); EG(ret, err);
			ret = nn_init(&a, 0); EG(ret, err);
		}

		/* Get a pseudo-random scalar a for randomizing the linear combination */
		ret = nn_get_random_mod(&a, q); EG(ret, err);

		/***************************************************/
		/* Extract s */
		ret = nn_init_from_buf(&S, &sig[2 * p_len], q_len); EG(ret, err);
		ret = nn_cmp(&S, q, &cmp); EG(ret, err);
		MUST_HAVE((cmp < 0), ret, err);

		dbg_nn_print("s", &S);

		/***************************************************/
		/* Add S to the sum */
		/* Multiply S by a */
		ret = nn_mod_mul(&S, &a, &S, q); EG(ret, err);
		ret = nn_mod_add(&S_sum, &S_sum,
				 &S, q); EG(ret, err);

		/***************************************************/
		/* Compute Y and add it to Y_sum */
		Y = &Tmp;
		/* Copy the public key point to work on the unique
		 * affine representative.
		 */
		ret = prj_pt_copy(Y, pub_key_y); EG(ret, err);
		ret = prj_pt_unique(Y, Y); EG(ret, err);
		dbg_ec_point_print("Y", Y);

		/* Compute e */
		ret = hm->hfunc_init(&h_ctx); EG(ret, err);
		ret = hm->hfunc_update(&h_ctx, &sig[0], (u32)(2 * p_len)); EG(ret, err);
		ret = hm->hfunc_update(&h_ctx, m[i], m_len[i]); EG(ret, err);
		ret = hm->hfunc_finalize(&h_ctx, hash); EG(ret, err);

		ret = nn_init_from_buf(&e, hash, hsize); EG(ret, err);
		ret = nn_mod(&e, &e, q); EG(ret, err);
		ret = nn_mod_neg(&e, &e, q); EG(ret, err);

		dbg_nn_print("e", &e);

		/* Multiply e by 'a' */
		ret = nn_mod_mul(&e, &e, &a, q); EG(ret, err);

		ret = _prj_pt_unprotected_mult(Y, &e, Y); EG(ret, err);
		dbg_ec_point_print("eY", Y);
		/* Add to the sum */
		ret = prj_pt_add(&Y_sum, &Y_sum, Y); EG(ret, err);

		/***************************************************/
		W = &Tmp;
		/* Compute W from rx and ry */
		ret = prj_pt_import_from_aff_buf(W, &sig[0], (u16)(2 * p_len), shortw_curve); EG(ret, err);

		/* Now multiply W by -a */
		ret = nn_mod_neg(&a, &a, q); EG(ret, err);
		ret = _prj_pt_unprotected_mult(W, &a, W); EG(ret, err);

		/* Add to the sum */
		ret = prj_pt_add(&W_sum, &W_sum, W); EG(ret, err);
		dbg_ec_point_print("aW", W);
	}
	/* Sanity check */
	MUST_HAVE((q != NULL) && (G != NULL), ret, err);

	/* Compute S_sum * G */
	ret = _prj_pt_unprotected_mult(&Tmp, &S_sum, G); EG(ret, err);
	/* Add P_sum and R_sum */
	ret = prj_pt_add(&Tmp, &Tmp, &W_sum); EG(ret, err);
	ret = prj_pt_add(&Tmp, &Tmp, &Y_sum); EG(ret, err);
	/* The result should be point at infinity */
	ret = prj_pt_iszero(&Tmp, &iszero); EG(ret, err);
	ret = (iszero == 1) ? 0 : -1;

err:
	PTR_NULLIFY(q);
	PTR_NULLIFY(pub_key);
	PTR_NULLIFY(pub_key0);
	PTR_NULLIFY(shortw_curve);
	PTR_NULLIFY(pub_key_y);
	PTR_NULLIFY(G);
	PTR_NULLIFY(W);
	PTR_NULLIFY(Y);

	prj_pt_uninit(&W_sum);
	prj_pt_uninit(&Y_sum);
	prj_pt_uninit(&Tmp);
	nn_uninit(&S);
	nn_uninit(&S_sum);
	nn_uninit(&e);
	nn_uninit(&a);

	return ret;
}


static int _ecfsdsa_verify_batch(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
				 const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
				 hash_alg_type hash_type, const u8 **adata, const u16 *adata_len,
				 verify_batch_scratch_pad *scratch_pad_area, u32 *scratch_pad_area_len)
{
	nn_src_t q = NULL;
	prj_pt_src_t G = NULL;
	prj_pt_t W = NULL, Y = NULL;
	nn S, a;
	nn_t e = NULL;
	u8 hash[MAX_DIGEST_SIZE];
	const ec_pub_key *pub_key, *pub_key0;
	int ret, iszero, cmp;
	prj_pt_src_t pub_key_y;
	hash_context h_ctx;
	const hash_mapping *hm;
	ec_shortw_crv_src_t shortw_curve;
	ec_alg_type key_type = UNKNOWN_ALG;
	bitcnt_t p_bit_len, q_bit_len = 0;
	u8 p_len, q_len;
	u16 hsize;
	u32 i;
	/* NN numbers and points pointers */
	verify_batch_scratch_pad *elements = scratch_pad_area;
	u64 expected_len;

	S.magic = a.magic = WORD(0);

	FORCE_USED_VAR(adata_len);
	FORCE_USED_VAR(adata);

	/* First, some sanity checks */
	MUST_HAVE((s != NULL) && (pub_keys != NULL) && (m != NULL), ret, err);

	MUST_HAVE((scratch_pad_area_len != NULL), ret, err);
	MUST_HAVE(((2 * num) >= num), ret, err);
	MUST_HAVE(((2 * num) + 1) >= num, ret, err);

	/* Zeroize buffers */
	ret = local_memset(hash, 0, sizeof(hash)); EG(ret, err);

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
			ret = _ecfsdsa_verify_batch_no_memory(s, s_len, pub_keys, m, m_len, num, sig_type,
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

	pub_key0 = pub_keys[0];
	MUST_HAVE((pub_key0 != NULL), ret, err);

	/* Get our hash mapping */
	ret = get_hash_by_type(hash_type, &hm); EG(ret, err);
	hsize = hm->digest_size;
	MUST_HAVE((hm != NULL), ret, err);

	for(i = 0; i < num; i++){
		u8 siglen;
		const u8 *sig = NULL;

		ret = pub_key_check_initialized_and_type(pub_keys[i], ECFSDSA); EG(ret, err);

		/* Make things more readable */
		pub_key = pub_keys[i];

		/* Sanity check that all our public keys have the same parameters */
		MUST_HAVE((pub_key->params) == (pub_key0->params), ret, err);

		q = &(pub_key->params->ec_gen_order);
		shortw_curve = &(pub_key->params->ec_curve);
		pub_key_y = &(pub_key->y);
		key_type = pub_key->key_type;
		G = &(pub_key->params->ec_gen);
		p_bit_len = pub_key->params->ec_fp.p_bitlen;
		q_bit_len = pub_key->params->ec_gen_order_bitlen;
		p_len = (u8)BYTECEIL(p_bit_len);
		q_len = (u8)BYTECEIL(q_bit_len);

		/* Check given signature length is the expected one */
		siglen = s_len[i];
		sig = s[i];
		MUST_HAVE((siglen == ECFSDSA_SIGLEN(p_bit_len, q_bit_len)), ret, err);
		MUST_HAVE((siglen == (ECFSDSA_R_LEN(p_bit_len) + ECFSDSA_S_LEN(q_bit_len))), ret, err);

		/* Check the key type versus the algorithm */
		MUST_HAVE((key_type == sig_type), ret, err);

		if(i == 0){
			/* Initialize our sums to zero/point at infinity */
			ret = nn_init(&a, 0); EG(ret, err);
			ret = nn_init(&elements[(2 * num)].number, 0); EG(ret, err);
			ret = prj_pt_copy(&elements[(2 * num)].point, G); EG(ret, err);
		}

		/* Get a pseudo-random scalar a for randomizing the linear combination */
		ret = nn_get_random_mod(&a, q); EG(ret, err);

		/***************************************************/
		/* Extract s */
		ret = nn_init_from_buf(&S, &sig[2 * p_len], q_len); EG(ret, err);
		ret = nn_cmp(&S, q, &cmp); EG(ret, err);
		MUST_HAVE((cmp < 0), ret, err);

		dbg_nn_print("s", &S);

		/***************************************************/
		/* Add S to the sum */
		/* Multiply S by a */
		ret = nn_mod_mul(&S, &a, &S, q); EG(ret, err);
		ret = nn_mod_add(&elements[(2 * num)].number, &elements[(2 * num)].number,
				 &S, q); EG(ret, err);

		/***************************************************/
		/* Compute Y */
		Y = &elements[num + i].point;
		/* Copy the public key point to work on the unique
		 * affine representative.
		 */
		ret = prj_pt_copy(Y, pub_key_y); EG(ret, err);
		ret = prj_pt_unique(Y, Y); EG(ret, err);
		dbg_ec_point_print("Y", Y);

		/* Compute e */
		e = &elements[num + i].number;
		ret = nn_init(e, 0); EG(ret, err);
		ret = hm->hfunc_init(&h_ctx); EG(ret, err);
		ret = hm->hfunc_update(&h_ctx, &sig[0], (u32)(2 * p_len)); EG(ret, err);
		ret = hm->hfunc_update(&h_ctx, m[i], m_len[i]); EG(ret, err);
		ret = hm->hfunc_finalize(&h_ctx, hash); EG(ret, err);

		ret = nn_init_from_buf(e, hash, hsize); EG(ret, err);
		ret = nn_mod(e, e, q); EG(ret, err);
		ret = nn_mod_neg(e, e, q); EG(ret, err);

		dbg_nn_print("e", e);

		/* Multiply e by 'a' */
		ret = nn_mod_mul(e, e, &a, q); EG(ret, err);

		/***************************************************/
		W = &elements[i].point;
		/* Compute W from rx and ry */
		ret = prj_pt_import_from_aff_buf(W, &sig[0], (u16)(2 * p_len), shortw_curve); EG(ret, err);
		ret = nn_init(&elements[i].number, 0); EG(ret, err);
		ret = nn_copy(&elements[i].number, &a); EG(ret, err);
		ret = nn_mod_neg(&elements[i].number, &elements[i].number, q); EG(ret, err);
		dbg_ec_point_print("W", W);
	}

	/* Sanity check */
	MUST_HAVE((q != NULL) && (G != NULL) && (q_bit_len != 0), ret, err);

	/********************************************/
	/****** Bos-Coster algorithm ****************/
	ret = ec_verify_bos_coster(elements, (2 * num) + 1, q_bit_len);
	if(ret){
		if(ret == -2){
			/* In case of Bos-Coster time out, we fall back to the
			 * slower regular batch verification.
			 */
			ret = _ecfsdsa_verify_batch_no_memory(s, s_len, pub_keys, m, m_len, num, sig_type,
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
	PTR_NULLIFY(e);
	PTR_NULLIFY(pub_key);
	PTR_NULLIFY(pub_key0);
	PTR_NULLIFY(shortw_curve);
	PTR_NULLIFY(pub_key_y);
	PTR_NULLIFY(G);
	PTR_NULLIFY(W);
	PTR_NULLIFY(Y);

	nn_uninit(&S);
	nn_uninit(&a);

	return ret;
}


int ecfsdsa_verify_batch(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
			 const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
			 hash_alg_type hash_type, const u8 **adata, const u16 *adata_len,
			 verify_batch_scratch_pad *scratch_pad_area, u32 *scratch_pad_area_len)
{
	int ret;

	if(scratch_pad_area != NULL){
		MUST_HAVE((scratch_pad_area_len != NULL), ret, err);
		ret = _ecfsdsa_verify_batch(s, s_len, pub_keys, m, m_len, num, sig_type,
					    hash_type, adata, adata_len,
					    scratch_pad_area, scratch_pad_area_len); EG(ret, err);

	}
	else{
		ret = _ecfsdsa_verify_batch_no_memory(s, s_len, pub_keys, m, m_len, num, sig_type,
						      hash_type, adata, adata_len); EG(ret, err);
	}

err:
	return ret;
}


#else /* WITH_SIG_ECFSDSA */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_SIG_ECFSDSA */
