/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *	Ryad BENADJILA <ryadbenadjila@gmail.com>
 *	Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *	Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *	Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *	Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/lib_ecc_config.h>
#ifdef WITH_SIG_ECGDSA

#include <libecc/nn/nn.h>
#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_logical.h>

#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/ec_key.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "ECGDSA"
#endif
#include <libecc/utils/dbg_sig.h>

int ecgdsa_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv)
{
	prj_pt_src_t G;
	nn_src_t q;
	nn xinv;
	int ret, cmp;
	xinv.magic = WORD(0);

	MUST_HAVE((out_pub != NULL), ret, err);

	/* Zero init public key to be generated */
	ret = local_memset(out_pub, 0, sizeof(ec_pub_key)); EG(ret, err);

	ret = priv_key_check_initialized_and_type(in_priv, ECGDSA); EG(ret, err);
	q = &(in_priv->params->ec_gen_order);

	/* Sanity check on key */
	MUST_HAVE((!nn_cmp(&(in_priv->x), q, &cmp)) && (cmp < 0), ret, err);

	/* Y = (x^-1)G */
	G = &(in_priv->params->ec_gen);
	/* NOTE: we use Fermat's little theorem inversion for
	 * constant time here. This is possible since q is prime.
	 */
	ret = nn_modinv_fermat(&xinv, &(in_priv->x), &(in_priv->params->ec_gen_order)); EG(ret, err);
	/* Use blinding with scalar_b when computing point scalar multiplication */
	ret = prj_pt_mul_blind(&(out_pub->y), &xinv, G); EG(ret, err);

	out_pub->key_type = ECGDSA;
	out_pub->params = in_priv->params;
	out_pub->magic = PUB_KEY_MAGIC;

err:
	nn_uninit(&xinv);

	return ret;
}

int ecgdsa_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen)
{
	int ret;

	MUST_HAVE((siglen != NULL), ret, err);
	MUST_HAVE((p_bit_len <= CURVES_MAX_P_BIT_LEN) &&
		  (q_bit_len <= CURVES_MAX_Q_BIT_LEN) &&
		  (hsize <= MAX_DIGEST_SIZE) && (blocksize <= MAX_BLOCK_SIZE), ret, err);

	(*siglen) = (u8)ECGDSA_SIGLEN(q_bit_len);

	ret = 0;

err:
	return ret;
}

/*
 * Generic *internal* EC-GDSA signature functions (init, update and finalize).
 * Their purpose is to allow passing a specific hash function (along with
 * its output size) and the random ephemeral key k, so that compliance
 * tests against test vectors can be made without ugly hack in the code
 * itself.
 *
 * Global EC-GDSA signature process is as follows (I,U,F provides
 * information in which function(s) (init(), update() or finalize())
 * a specific step is performed):
 *
 *| IUF - EC-GDSA signature
 *|
 *|  UF 1. Compute h = H(m). If |h| > bitlen(q), set h to bitlen(q)
 *|	   leftmost (most significant) bits of h
 *|   F 2. Compute e = - OS2I(h) mod q
 *|   F 3. Get a random value k in ]0,q[
 *|   F 4. Compute W = (W_x,W_y) = kG
 *|   F 5. Compute r = W_x mod q
 *|   F 6. If r is 0, restart the process at step 4.
 *|   F 7. Compute s = x(kr + e) mod q
 *|   F 8. If s is 0, restart the process at step 4.
 *|   F 9. Return (r,s)
 *
 * Implementation notes:
 *
 * a) Usually (this is for instance the case in ISO 14888-3 and X9.62), the
 *    process starts with steps 4 to 7 and is followed by steps 1 to 3.
 *    The order is modified here w/o impact on the result and the security
 *    in order to allow the algorithm to be compatible with an
 *    init/update/finish API. More explicitly, the generation of k, which
 *    may later result in a (unlikely) restart of the whole process is
 *    postponed until the hash of the message has been computed.
 * b) sig is built as the concatenation of r and s. Both r and s are
 *    encoded on ceil(bitlen(q)/8) bytes.
 * c) in EC-GDSA, the public part of the key is not needed per se during the
 *    signature but - as it is needed in other signature algs implemented
 *    in the library - the whole key pair is passed instead of just the
 *    private key.
 */

#define ECGDSA_SIGN_MAGIC ((word_t)(0xe2f60ea3353ecc9eULL))
#define ECGDSA_SIGN_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && \
		  ((A)->magic == ECGDSA_SIGN_MAGIC), ret, err)

int _ecgdsa_sign_init(struct ec_sign_context *ctx)
{
	int ret;

	/* First, verify context has been initialized */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);

	/* Additional sanity checks on input params from context */
	ret = key_pair_check_initialized_and_type(ctx->key_pair, ECGDSA); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		  (ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);

	/*
	 * Initialize hash context stored in our private part of context
	 * and record data init has been done
	 */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->sign_data.ecgdsa.h_ctx)); EG(ret, err);

	ctx->sign_data.ecgdsa.magic = ECGDSA_SIGN_MAGIC;

err:
	return ret;
}

int _ecgdsa_sign_update(struct ec_sign_context *ctx,
			const u8 *chunk, u32 chunklen)
{
	int ret;

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an EC-GDSA
	 * signature one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ECGDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.ecgdsa), ret, err);

	/* 1. Compute h = H(m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->sign_data.ecgdsa.h_ctx), chunk, chunklen);

err:
	return ret;
}

int _ecgdsa_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen)
{
	nn_src_t q, x;
	u8 e_buf[MAX_DIGEST_SIZE];
	const ec_priv_key *priv_key;
	prj_pt_src_t G;
	u8 hsize, r_len, s_len;
	bitcnt_t q_bit_len, p_bit_len, rshift;
	prj_pt kG;
	int ret, cmp, iszero;
	nn tmp, s, e, kr, k, r;
#ifdef USE_SIG_BLINDING
	/* b is the blinding mask */
	nn b, binv;
	b.magic = binv.magic = WORD(0);
#endif

	tmp.magic = s.magic = e.magic = WORD(0);
	kr.magic = k.magic = r.magic = WORD(0);
	kG.magic = WORD(0);

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an EC-GDSA
	 * signature one and we do not finalize() before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ECGDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.ecgdsa), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Zero init points */
	ret = local_memset(&kG, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	priv_key = &(ctx->key_pair->priv_key);
	G = &(priv_key->params->ec_gen);
	q = &(priv_key->params->ec_gen_order);
	x = &(priv_key->x);
	q_bit_len = priv_key->params->ec_gen_order_bitlen;
	p_bit_len = priv_key->params->ec_fp.p_bitlen;
	MUST_HAVE(((u32)BYTECEIL(p_bit_len) <= NN_MAX_BYTE_LEN), ret, err);
	r_len = (u8)ECGDSA_R_LEN(q_bit_len);
	s_len = (u8)ECGDSA_S_LEN(q_bit_len);
	hsize = ctx->h->digest_size;

	/* Sanity check */
	ret = nn_cmp(x, q, &cmp); EG(ret, err);
	/* This should not happen and means that our
	 * private key is not compliant!
	 */
	MUST_HAVE((cmp < 0), ret, err);

	MUST_HAVE((siglen == ECGDSA_SIGLEN(q_bit_len)), ret, err);

	dbg_nn_print("p", &(priv_key->params->ec_fp.p));
	dbg_nn_print("q", q);
	dbg_priv_key_print("x", priv_key);
	dbg_ec_point_print("G", G);
	dbg_pub_key_print("Y", &(ctx->key_pair->pub_key));

	/* 1. Compute h = H(m) */
	ret = local_memset(e_buf, 0, hsize); EG(ret, err);
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&(ctx->sign_data.ecgdsa.h_ctx), e_buf); EG(ret, err);
	dbg_buf_print("H(m)", e_buf, hsize);

	/*
	 * If |h| > bitlen(q), set h to bitlen(q)
	 * leftmost bits of h.
	 *
	 */
	rshift = 0;
	if ((hsize * 8) > q_bit_len) {
		rshift = (bitcnt_t)((hsize * 8) - q_bit_len);
	}
	ret = nn_init_from_buf(&tmp, e_buf, hsize); EG(ret, err);
	ret = local_memset(e_buf, 0, hsize); EG(ret, err);
	if (rshift) {
		ret = nn_rshift_fixedlen(&tmp, &tmp, rshift); EG(ret, err);
	}
	dbg_nn_print("H(m) truncated as nn", &tmp);

	/*
	 * 2. Convert h to an integer and then compute e = -h mod q,
	 *    i.e. compute e = - OS2I(h) mod q
	 *
	 * Because we only support positive integers, we compute
	 * e = q - (h mod q) (except when h is 0).
	 */
	ret = nn_mod(&tmp, &tmp, q); EG(ret, err);
	ret = nn_mod_neg(&e, &tmp, q); EG(ret, err);

 restart:
	/* 3. Get a random value k in ]0,q[ */
#ifdef NO_KNOWN_VECTORS
	/* NOTE: when we do not need self tests for known vectors,
	 * we can be strict about random function handler!
	 * This allows us to avoid the corruption of such a pointer.
	 */
	/* Sanity check on the handler before calling it */
	MUST_HAVE(ctx->rand == nn_get_random_mod, ret, err);
#endif
	MUST_HAVE(ctx->rand != NULL, ret, err);

	ret = ctx->rand(&k, q); EG(ret, err);

#ifdef USE_SIG_BLINDING
	/* Note: if we use blinding, e and e are multiplied by
	 * a random value b in ]0,q[ */
	ret = nn_get_random_mod(&b, q); EG(ret, err);
	dbg_nn_print("b", &b);
#endif /* USE_SIG_BLINDING */

	/* 4. Compute W = kG = (Wx, Wy) */
#ifdef USE_SIG_BLINDING
	/* We use blinding for the scalar multiplication */
	ret = prj_pt_mul_blind(&kG, &k, G); EG(ret, err);
#else
	ret = prj_pt_mul(&kG, &k, G); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	ret = prj_pt_unique(&kG, &kG); EG(ret, err);

	dbg_nn_print("W_x", &(kG.X.fp_val));
	dbg_nn_print("W_y", &(kG.Y.fp_val));

	/* 5. Compute r = Wx mod q */
	ret = nn_mod(&r, &(kG.X.fp_val), q); EG(ret, err);
	dbg_nn_print("r", &r);

	/* 6. If r is 0, restart the process at step 4. */
	ret = nn_iszero(&r, &iszero); EG(ret, err);
	if (iszero) {
		goto restart;
	}

	/* Export r */
	ret = nn_export_to_buf(sig, r_len, &r); EG(ret, err);

#ifdef USE_SIG_BLINDING
	/* Blind e and r with b */
	ret = nn_mod_mul(&e, &e, &b, q); EG(ret, err);
	ret = nn_mod_mul(&r, &r, &b, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	/* 7. Compute s = x(kr + e) mod q */
	ret = nn_mod_mul(&kr, &k, &r, q); EG(ret, err);
	ret = nn_mod_add(&tmp, &kr, &e, q); EG(ret, err);
	ret = nn_mod_mul(&s, x, &tmp, q); EG(ret, err);
#ifdef USE_SIG_BLINDING
	/* Unblind s */
	/* NOTE: we use Fermat's little theorem inversion for
	 * constant time here. This is possible since q is prime.
	 */
	ret = nn_modinv_fermat(&binv, &b, q); EG(ret, err);
	ret = nn_mod_mul(&s, &s, &binv, q); EG(ret, err);
#endif
	dbg_nn_print("s", &s);

	/* 8. If s is 0, restart the process at step 4. */
	ret = nn_iszero(&s, &iszero); EG(ret, err);
	if (iszero) {
		goto restart;
	}

	/* 9. Return (r,s) */
	ret = nn_export_to_buf(sig + r_len, s_len, &s);

 err:
	nn_uninit(&tmp);
	nn_uninit(&s);
	nn_uninit(&e);
	nn_uninit(&kr);
	nn_uninit(&k);
	nn_uninit(&r);
	prj_pt_uninit(&kG);
#ifdef USE_SIG_BLINDING
	nn_uninit(&b);
	nn_uninit(&binv);
#endif

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->sign_data.ecgdsa), 0, sizeof(ecgdsa_sign_data)));
	}

	/* Clean what remains on the stack */
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(p_bit_len);
	VAR_ZEROIFY(r_len);
	VAR_ZEROIFY(s_len);
	VAR_ZEROIFY(hsize);
	PTR_NULLIFY(q);
	PTR_NULLIFY(x);
	PTR_NULLIFY(priv_key);
	PTR_NULLIFY(G);

	return ret;
}

/*
 * Generic *internal* EC-GDSA verification functions (init, update and finalize).
 * Their purpose is to allow passing a specific hash function (along with
 * their output size) and the random ephemeral key k, so that compliance
 * tests against test vectors can be made without ugly hack in the code
 * itself.
 *
 * Global EC-GDSA verification process is as follows (I,U,F provides
 * information in which function(s) (init(), update() or finalize())
 * a specific step is performed):
 *
 *| IUF - EC-GDSA verification
 *|
 *| I	1. Reject the signature if r or s is 0.
 *|  UF 2. Compute h = H(m). If |h| > bitlen(q), set h to bitlen(q)
 *|	   leftmost (most significant) bits of h
 *|   F 3. Compute e = OS2I(h) mod q
 *|   F 4. Compute u = ((r^-1)e mod q)
 *|   F 5. Compute v = ((r^-1)s mod q)
 *|   F 6. Compute W' = uG + vY
 *|   F 7. Compute r' = W'_x mod q
 *|   F 8. Accept the signature if and only if r equals r'
 *
 */

#define ECGDSA_VERIFY_MAGIC ((word_t)(0xd4da37527288d1b6ULL))
#define ECGDSA_VERIFY_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && \
		  ((A)->magic == ECGDSA_VERIFY_MAGIC), ret, err)

int _ecgdsa_verify_init(struct ec_verify_context *ctx,
			const u8 *sig, u8 siglen)
{
	u8 r_len, s_len;
	bitcnt_t q_bit_len;
	nn_src_t q;
	nn *s, *r;
	int ret, iszero1, iszero2, cmp1, cmp2;

	/* First, verify context has been initialized */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);

	/* Do some sanity checks on input params */
	ret = pub_key_check_initialized_and_type(ctx->pub_key, ECGDSA); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		(ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Make things more readable */
	q = &(ctx->pub_key->params->ec_gen_order);
	q_bit_len = ctx->pub_key->params->ec_gen_order_bitlen;
	r = &(ctx->verify_data.ecgdsa.r);
	s = &(ctx->verify_data.ecgdsa.s);
	r_len = (u8)ECGDSA_R_LEN(q_bit_len);
	s_len = (u8)ECGDSA_S_LEN(q_bit_len);

	/* Check given signature length is the expected one */
	MUST_HAVE((siglen == ECGDSA_SIGLEN(q_bit_len)), ret, err);

	/* 1. Reject the signature if r or s is 0. */

	/* Let's first import r, the x coordinates of the point reduced mod q */
	ret = nn_init_from_buf(r, sig, r_len); EG(ret, err);

	/* Import s as a nn */
	ret = nn_init_from_buf(s, sig + r_len, s_len); EG(ret, err);

	/* Check that r and s are both in ]0,q[ */
	ret = nn_iszero(s, &iszero1); EG(ret, err);
	ret = nn_iszero(r, &iszero2); EG(ret, err);
	ret = nn_cmp(s, q, &cmp1); EG(ret, err);
	ret = nn_cmp(r, q, &cmp2); EG(ret, err);

	MUST_HAVE((!iszero1) && (cmp1 < 0) && (!iszero2) && (cmp2 < 0), ret, err);

	/* Initialize the remaining of verify context */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->verify_data.ecgdsa.h_ctx)); EG(ret, err);

	ctx->verify_data.ecgdsa.magic = ECGDSA_VERIFY_MAGIC;

 err:
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(r_len);
	VAR_ZEROIFY(s_len);
	PTR_NULLIFY(q);
	PTR_NULLIFY(s);
	PTR_NULLIFY(r);

	return ret;
}

int _ecgdsa_verify_update(struct ec_verify_context *ctx,
			  const u8 *chunk, u32 chunklen)
{
	int ret;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an EC-GDSA
	 * verification one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ECGDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.ecgdsa), ret, err);

	/* 2. Compute h = H(m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->verify_data.ecgdsa.h_ctx), chunk,
			     chunklen);

err:
	return ret;
}

int _ecgdsa_verify_finalize(struct ec_verify_context *ctx)
{
	nn e, r_prime, rinv, uv, *r, *s;
	prj_pt uG, vY;
	prj_pt_t Wprime;
	prj_pt_src_t G, Y;
	u8 e_buf[MAX_DIGEST_SIZE];
	nn_src_t q;
	u8 hsize;
	bitcnt_t q_bit_len, rshift;
	int ret, cmp;

	e.magic = r_prime.magic = WORD(0);
	rinv.magic = uv.magic = WORD(0);
	uG.magic = vY.magic = WORD(0);

	/* NOTE: we reuse uG for Wprime to optimize local variables */
	Wprime = &uG;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an EC-GDSA
	 * verification one and we do not finalize() before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ECGDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.ecgdsa), ret, err);

	/* Zero init points */
	ret = local_memset(&uG, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&vY, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	G = &(ctx->pub_key->params->ec_gen);
	Y = &(ctx->pub_key->y);
	q = &(ctx->pub_key->params->ec_gen_order);
	r = &(ctx->verify_data.ecgdsa.r);
	s = &(ctx->verify_data.ecgdsa.s);
	q_bit_len = ctx->pub_key->params->ec_gen_order_bitlen;
	hsize = ctx->h->digest_size;

	/* 2. Compute h = H(m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&(ctx->verify_data.ecgdsa.h_ctx), e_buf); EG(ret, err);
	dbg_buf_print("H(m)", e_buf, hsize);

	/*
	 * If |h| > bitlen(q), set h to bitlen(q)
	 * leftmost bits of h.
	 *
	 */
	rshift = 0;
	if ((hsize * 8) > q_bit_len) {
		rshift = (bitcnt_t)((hsize * 8) - q_bit_len);
	}
	ret = nn_init_from_buf(&e, e_buf, hsize); EG(ret, err);
	ret = local_memset(e_buf, 0, hsize); EG(ret, err);
	if (rshift) {
		ret = nn_rshift_fixedlen(&e, &e, rshift); EG(ret, err);
	}
	dbg_nn_print("H(m) truncated as nn", &e);

	/* 3. Compute e by converting h to an integer and reducing it mod q */
	ret = nn_mod(&e, &e, q); EG(ret, err);

	/* 4. Compute u = (r^-1)e mod q */
	ret = nn_modinv(&rinv, r, q); EG(ret, err); /* r^-1 */
	ret = nn_mod_mul(&uv, &rinv, &e, q); EG(ret, err);
	ret = prj_pt_mul(&uG, &uv, G); EG(ret, err);

	/* 5. Compute v = (r^-1)s mod q */
	ret = nn_mod_mul(&uv, &rinv, s, q); EG(ret, err);
	ret = prj_pt_mul(&vY, &uv, Y); EG(ret, err);

	/* 6. Compute W' = uG + vY */
	ret = prj_pt_add(Wprime, &uG, &vY); EG(ret, err);

	/* 7. Compute r' = W'_x mod q */
	ret = prj_pt_unique(Wprime, Wprime); EG(ret, err);
	dbg_nn_print("W'_x", &(Wprime->X.fp_val));
	dbg_nn_print("W'_y", &(Wprime->Y.fp_val));
	ret = nn_mod(&r_prime, &(Wprime->X.fp_val), q); EG(ret, err);

	/* 8. Accept the signature if and only if r equals r' */
	ret = nn_cmp(r, &r_prime, &cmp); EG(ret, err);
	ret = (cmp != 0) ? -1 : 0;

err:
	nn_uninit(&e);
	nn_uninit(&r_prime);
	nn_uninit(&rinv);
	nn_uninit(&uv);
	prj_pt_uninit(&uG);
	prj_pt_uninit(&vY);

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->verify_data.ecgdsa), 0,
			     sizeof(ecgdsa_verify_data)));
	}

	PTR_NULLIFY(Wprime);
	PTR_NULLIFY(r);
	PTR_NULLIFY(s);
	PTR_NULLIFY(G);
	PTR_NULLIFY(Y);
	PTR_NULLIFY(q);
	VAR_ZEROIFY(hsize);

	return ret;
}

#else /* WITH_SIG_ECGDSA */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_SIG_ECGDSA */
