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
#ifdef WITH_SIG_SM2

#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_logical.h>

#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/ec_key.h>
#include <libecc/utils/utils.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "SM2"
#endif
#include <libecc/utils/dbg_sig.h>

/*
 * NOTE: SM2 has an oddity in private key generation when compared to
 * other EC*DSA style signature algorithms described in ISO14888-3:
 * the private key x MUST be in ]0, q-1[ instead of ]0, q[ (this is actually
 * explained by the fact that (1 + x) must be inversible modulo q during the
 * signature process).
 *
 * Hence the following specific key generation function.
 *
 */
int sm2_gen_priv_key(ec_priv_key *priv_key)
{
	int ret;
	nn tmp;
	tmp.magic = WORD(0);

	ret = priv_key_check_initialized_and_type(priv_key, SM2); EG(ret, err);

	/* Get a random value in ]0,q-1[ where q is the group generator order */
	ret = nn_init(&tmp, 0); EG(ret, err);
	ret = nn_dec(&tmp,  &(priv_key->params->ec_gen_order)); EG(ret, err);
	ret = nn_get_random_mod(&(priv_key->x), &tmp);

err:
	nn_uninit(&tmp);

	return ret;
}

int sm2_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv)
{
	prj_pt_src_t G;
	int ret, cmp;
	nn tmp;
	tmp.magic = WORD(0);

	MUST_HAVE((out_pub != NULL), ret, err);

	ret = priv_key_check_initialized_and_type(in_priv, SM2); EG(ret, err);

	/*
	 * We verify that the private key is valid, i.e. in
	 * ]0, q-1[. This excluded q-1 is an oddity but is what the
	 * ISO14888-3:2018 has.
	 */
	ret = nn_init(&tmp, 0); EG(ret, err);
	ret = nn_dec(&tmp, &in_priv->params->ec_gen_order); EG(ret, err);
	/* If x >= (q - 1), this is an error */
	MUST_HAVE((!nn_cmp(&(in_priv->x), &tmp, &cmp)) && (cmp < 0), ret, err);

	/* Y = xG */
	G = &(in_priv->params->ec_gen);

	/* Zero init public key to be generated */
	ret = local_memset(out_pub, 0, sizeof(ec_pub_key)); EG(ret, err);

	/* Use blinding with scalar_b when computing point scalar multiplication */
	ret = prj_pt_mul_blind(&(out_pub->y), &(in_priv->x), G); EG(ret, err);

	out_pub->key_type = SM2;
	out_pub->params = in_priv->params;
	out_pub->magic = PUB_KEY_MAGIC;

err:
	nn_uninit(&tmp);

	return ret;
}

int sm2_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen)
{
	int ret;

	MUST_HAVE((siglen != NULL), ret, err);
	MUST_HAVE((p_bit_len <= CURVES_MAX_P_BIT_LEN) &&
		  (q_bit_len <= CURVES_MAX_Q_BIT_LEN) &&
		  (hsize <= MAX_DIGEST_SIZE) && (blocksize <= MAX_BLOCK_SIZE), ret, err);

	(*siglen) = (u8)SM2_SIGLEN(q_bit_len);
	ret = 0;

err:
	return ret;
}

/*
 * Helper to compute Z from user ID, curve parameters, public key and hash
 * function as defined in section 6.12.4.3 of ISO14888-3:2018. The function
 * returns 0 on success, -1 on error. On success, the number of bytes
 * written to Z is provided using Zlen. On input, Zlen provides the size of
 * Z buffer, which must be large enough for selected hash function (Z has
 * the digest size of the hash function). 'id' buffer of size 'id_len' must
 * be smaller than SM2_MAX_ID_LEN (see sm2.h).
 *
 * Z = h(ENTL || ID || FE2BS(p, a) || FE2BS(p, b) || FE2BS(p, Gx) ||
 *       FE2BS(p, Gy) || FE2BS(p, Yx) || FE2BS(p, Yy)).
 *
 * with:
 *
 *  - GF(p), Finite field of cardinality p.
 *  - Curve Weierstrass Equation y^2 = x^3 + a * x + b.
 *  - ID string containing an identifier of the signer
 *  - G = (Gx, Gy) an element of order q in E.
 *  - entlen is the bit-length of ID and ENTL the two bytes string transformed
 *    from the integer entlen, i.e. ENTL = I2BS(12, entlen).
 *
 */
#define Z_INPUT_MAX_LEN (2 + SM2_MAX_ID_LEN + (6 * BYTECEIL(CURVES_MAX_P_BIT_LEN)))

ATTRIBUTE_WARN_UNUSED_RET static int sm2_compute_Z(u8 *Z, u16 *Zlen, const u8 *id, u16 id_len,
		  const ec_pub_key *pub_key, hash_alg_type hash_type)
{
	u16 hsize, entlen, p_len;
	u8 buf[2 * BYTECEIL(CURVES_MAX_P_BIT_LEN)];
	const hash_mapping *hm;
	prj_pt_src_t G, Y;
	hash_context hctx;
	bitcnt_t p_bit_len;
	fp_src_t a, b;
	int ret;

	MUST_HAVE((Z != NULL) && (Zlen != NULL), ret, err);
	MUST_HAVE((id != NULL) && (pub_key != NULL), ret, err);
	/* Maximum size is Entlen on 16 bits in *bits*, i.e. 8192 bytes */
	MUST_HAVE((id_len <= SM2_MAX_ID_LEN), ret, err);
	ret = pub_key_check_initialized_and_type(pub_key, SM2); EG(ret, err);

	ret = get_hash_by_type(hash_type, &hm); EG(ret, err);
	MUST_HAVE((hm != NULL), ret, err);

	/* Zlen must be large enough to receive digest */
	hsize = hm->digest_size;
	MUST_HAVE((*Zlen) >= hsize, ret, err);

	/* Make things more readable */
	G = &(pub_key->params->ec_gen);
	Y = &(pub_key->y);
	p_bit_len = pub_key->params->ec_fp.p_bitlen;
	p_len = (u8)BYTECEIL(p_bit_len);
	entlen = (u16)(id_len * 8);
	a = &(pub_key->params->ec_curve.a);
	b = &(pub_key->params->ec_curve.b);

	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(hm); EG(ret, err);
	ret = hm->hfunc_init(&hctx); EG(ret, err);

	/* ENTL */
	buf[0] = (u8)((entlen >> 8) & 0xff);
	buf[1] = (u8)(entlen & 0xff);
	ret = hm->hfunc_update(&hctx, buf, 2); EG(ret, err);

	/* ID */
	ret = hm->hfunc_update(&hctx, id, id_len); EG(ret, err);

	/* FE2BS(p, a) */
	ret = fp_export_to_buf(buf, p_len, a); EG(ret, err);
	ret = hm->hfunc_update(&hctx, buf, p_len); EG(ret, err);

	/* FE2BS(p, b) */
	ret = fp_export_to_buf(buf, p_len, b); EG(ret, err);
	ret = hm->hfunc_update(&hctx, buf, p_len); EG(ret, err);

	/* FE2BS(p, Gx) || FE2BS(p, Gy) */
	ret = prj_pt_export_to_aff_buf(G, buf, (u32)(2 * p_len)); EG(ret, err);
	ret = hm->hfunc_update(&hctx, buf, (u32)(2 * p_len)); EG(ret, err);

	/* FE2BS(p, Yx) || FE2BS(p, Yy) */
	ret = prj_pt_export_to_aff_buf(Y, buf, (u32)(2 * p_len)); EG(ret, err);
	ret = hm->hfunc_update(&hctx, buf, (u32)(2 * p_len)); EG(ret, err);

	/* Let's now finalize hash computation */
	ret = hm->hfunc_finalize(&hctx, Z); EG(ret, err);
	dbg_buf_print("Z", Z, hsize);

	ret = local_memset(buf, 0, sizeof(buf)); EG(ret, err);
	ret = local_memset(&hctx, 0, sizeof(hctx)); EG(ret, err);

	(*Zlen) = hsize;

err:
	if (ret && (Zlen != NULL)){
		(*Zlen) = 0;
	}
	return ret;
}


/*
 * Generic *internal* SM2 signature functions (init, update and finalize).
 * Their purpose is to allow passing a specific hash function (along with
 * its output size) and the random ephemeral key k, so that compliance
 * tests against test vectors can be made without ugly hack in the code
 * itself.
 *
 * Global SM2 signature process is as follows (I,U,F provides information
 * in which function(s) (init(), update() or finalize()) a specific step
 * is performed):
 *
 *| IUF  - SM2 signature
 *|
 *|  UF  1. set M1 = Z || M   (See (*) below)
 *|   F  2. Compute H = h(M1)
 *|   F  3. Get a random value k in ]0,q[
 *|   F  4. Compute W = (W_x,W_y) = kG
 *|   F  5. Compute r = (OS2I(H) + Wx) mod q
 *|   F  6. If r is 0, restart the process at step 3.
 *|   F  7. If r + k is q, restart the process at step 3.
 *|   F  8. Compute s = ((1 + x)^(-1) * (k - rx)) mod q
 *|   F  9. If s is 0, restart the process at step 3.
 *|   F  10. Export r and s
 *
 * (*) It is user responsibility to pass the ID string in the optional ancillary
 *     data of the API.
 */

#define SM2_SIGN_MAGIC ((word_t)(0x324300884035dae8ULL))
#define SM2_SIGN_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == SM2_SIGN_MAGIC), ret, err)

int _sm2_sign_init(struct ec_sign_context *ctx)
{
	int ret;
	u8 Z[Z_INPUT_MAX_LEN];
	u16 Zlen;

	/* First, verify context has been initialized */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);

	/* Additional sanity checks on input params from context */
	ret = key_pair_check_initialized_and_type(ctx->key_pair, SM2); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		  (ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);

	/*
	 * Initialize hash context stored in our private part of context
	 * and record data init has been done
	 */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->sign_data.sm2.h_ctx)); EG(ret, err);

	/* Compute Z from the ID */
	ret = local_memset(Z, 0, sizeof(Z)); EG(ret, err);
	Zlen = sizeof(Z);
	ret = sm2_compute_Z(Z, &Zlen, ctx->adata, ctx->adata_len,
			    &(ctx->key_pair->pub_key), ctx->h->type); EG(ret, err);

	/* Update the hash function with Z */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->sign_data.sm2.h_ctx), Z, Zlen); EG(ret, err);

	ctx->sign_data.sm2.magic = SM2_SIGN_MAGIC;

err:
	VAR_ZEROIFY(Zlen);

	return ret;
}

int _sm2_sign_update(struct ec_sign_context *ctx,
		       const u8 *chunk, u32 chunklen)
{
	int ret;

	/*
	 * First, verify context has been initialized and private part too.
	 * This guarantees the context is an SM2 signature one and we do not
	 * update() or finalize() before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	SM2_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.sm2), ret, err);

	/* 1. Compute h = H(m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->sign_data.sm2.h_ctx), chunk, chunklen);

err:
	return ret;
}

int _sm2_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen)
{
	const ec_priv_key *priv_key;
	u8 hash[MAX_DIGEST_SIZE];
	bitcnt_t q_bit_len;
	u8 hsize, q_len;
	prj_pt_src_t G;
	nn_src_t q, x;
	prj_pt kG;
	int ret, iszero, cmp;
	nn k, r, s, tmp, tmp2, tmp3;
#ifdef USE_SIG_BLINDING
	nn b;        /* blinding mask */
	b.magic = WORD(0);
#endif

	kG.magic = WORD(0);
	k.magic = r.magic = s.magic = tmp.magic = tmp2.magic = tmp3.magic = WORD(0);

	/*
	 * First, verify context has been initialized and private part too.
	 * This guarantees the context is an SM2 signature one and we do not
	 * update() or finalize() before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	SM2_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.sm2), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Zero init out point */
	ret = local_memset(&kG, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	priv_key = &(ctx->key_pair->priv_key);
	q = &(priv_key->params->ec_gen_order);
	q_bit_len = priv_key->params->ec_gen_order_bitlen;
	G = &(priv_key->params->ec_gen);
	q_len = (u8)BYTECEIL(q_bit_len);
	x = &(priv_key->x);
	hsize = ctx->h->digest_size;

	dbg_nn_print("p", &(priv_key->params->ec_fp.p));
	dbg_nn_print("q", &(priv_key->params->ec_gen_order));
	dbg_priv_key_print("x", priv_key);
	dbg_ec_point_print("G", &(priv_key->params->ec_gen));
	dbg_pub_key_print("Y", &(ctx->key_pair->pub_key));

	/* Check given signature buffer length has the expected size */
	MUST_HAVE((siglen == SM2_SIGLEN(q_bit_len)), ret, err);

	ret = local_memset(hash, 0, hsize); EG(ret, err);
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);

	/* 2. Compute H = h(M1) */
	ret = ctx->h->hfunc_finalize(&(ctx->sign_data.sm2.h_ctx), hash); EG(ret, err);
	dbg_buf_print("h", hash, hsize);

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
	ret = ctx->rand(&k, q); EG(ret, err);
	dbg_nn_print("k", &k);

	/* 4. Compute W = (W_x,W_y) = kG */
#ifdef USE_SIG_BLINDING
	ret = prj_pt_mul_blind(&kG, &k, G); EG(ret, err);
#else
	ret = prj_pt_mul(&kG, &k, G); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	ret = prj_pt_unique(&kG, &kG); EG(ret, err);

	dbg_nn_print("W_x", &(kG.X.fp_val));
	dbg_nn_print("W_y", &(kG.Y.fp_val));

	/* 5. Compute r = (OS2I(H) + Wx) mod q */
	ret = nn_init_from_buf(&tmp, hash, hsize); EG(ret, err);
	ret = local_memset(hash, 0, hsize); EG(ret, err);
	dbg_nn_print("OS2I(H)", &tmp);
	ret = nn_add(&tmp2, &tmp, &(kG.X.fp_val)); EG(ret, err);
	ret = nn_mod(&r, &tmp2, q); EG(ret, err);
	dbg_nn_print("r", &r);

	/* 6. If r is 0, restart the process at step 3. */
	ret = nn_iszero(&r, &iszero); EG(ret, err);
	if (iszero) {
		goto restart;
	}

	/* 7. If r + k is q, restart the process at step 3. */
	ret = nn_add(&tmp, &r, q); EG(ret, err);
	ret = nn_cmp(&tmp, q, &cmp); EG(ret, err);
	if (cmp == 0) {
		goto restart;
	}

	/* 8. Compute s = ((1 + x)^(-1) * (k - rx)) mod q */
#ifdef USE_SIG_BLINDING
	/*
	 * With blinding enabled, the computation above is performed in the
	 * following way s = ((b*(1 + x))^(-1) * (kb - (br)x)) mod q
	 */
	ret = nn_get_random_mod(&b, q); EG(ret, err);
	dbg_nn_print("b", &b);
	ret = nn_inc(&tmp2, x); EG(ret, err);
	ret = nn_mod_mul(&tmp2, &tmp2, &b, q); EG(ret, err);
        /* NOTE: we use Fermat's little theorem inversion for
         * constant time here. This is possible since q is prime.
         */
	ret = nn_modinv_fermat(&tmp, &tmp2, q); EG(ret, err); /* tmp = (b*(1 + x))^(-1) */
	dbg_nn_print("(b*(1 + x))^(-1)", &tmp);
	ret = nn_mod_mul(&tmp3, &r, &b, q); EG(ret, err); /* rb */
	ret = nn_mod_mul(&k, &k, &b, q); EG(ret, err); /* kb */
	ret = nn_mod_mul(&tmp3, &tmp3, x, q); EG(ret, err); /* (rb)x mod q */
	ret = nn_mod_sub(&tmp2, &k, &tmp3, q); EG(ret, err); /* tmp2 = (kb - (rb)x) mod q */
	ret = nn_mod_mul(&s, &tmp, &tmp2, q); EG(ret, err);
	dbg_nn_print("s", &s);
#else
	ret = nn_inc(&tmp2, x); EG(ret, err);
        /* NOTE: we use Fermat's little theorem inversion for
         * constant time here. This is possible since q is prime.
         */
	ret = nn_modinv_fermat(&tmp, &tmp2, q); EG(ret, err); /* tmp = (1 + x)^(-1) */
	dbg_nn_print("(1 + x)^(-1)", &tmp);
	ret = nn_mod_mul(&tmp3, &r, x, q); EG(ret, err); /* rx mod q */
	ret = nn_mod_sub(&tmp2, &k, &tmp3, q); EG(ret, err); /* tmp2 = (k - rx) mod q */
	ret = nn_mod_mul(&s, &tmp, &tmp2, q); EG(ret, err);
	dbg_nn_print("s", &s);
#endif

	/* 9. If s is 0, restart the process at step 3. */
	ret = nn_iszero(&s, &iszero); EG(ret, err);
	if (iszero) {
		goto restart;
	}

	/* 10. Export r and s */
	ret = nn_export_to_buf(sig, q_len, &r); EG(ret, err);
	ret = nn_export_to_buf(sig + q_len, q_len, &s);

err:
	prj_pt_uninit(&kG);
	nn_uninit(&k);
	nn_uninit(&r);
	nn_uninit(&s);
	nn_uninit(&tmp);
	nn_uninit(&tmp2);
	nn_uninit(&tmp3);
#ifdef USE_SIG_BLINDING
	nn_uninit(&b);
#endif
	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	IGNORE_RET_VAL(local_memset(&(ctx->sign_data.sm2), 0, sizeof(sm2_sign_data)));

	/* Clean what remains on the stack */
	PTR_NULLIFY(priv_key);
	PTR_NULLIFY(G);
	PTR_NULLIFY(q);
	PTR_NULLIFY(x);
	VAR_ZEROIFY(q_len);
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(hsize);

	return ret;
}


/*
 * Generic *internal* SM2 verification functions (init, update and finalize).
 * Their purpose is to allow passing a specific hash function (along with
 * its output size) and the random ephemeral key k, so that compliance
 * tests against test vectors can be made without ugly hack in the code
 * itself.
 *
 * Global SM2 verification process is as follows (I,U,F provides information
 * in which function(s) (init(), update() or finalize()) a specific step is
 * performed):
 *
 *| IUF  - SM2 verification
 *|
 *| I   1. Reject the signature if r or s is 0 or >= q.
 *|  UF 2. Compute h = H(M1) w/ M1 = Z || M   (See (*) below)
 *|   F 3. Compute t = r + s mod q
 *|   F 4. Reject signature if t is 0
 *|   F 5. Compute e = OS2I(h) mod q
 *|   F 6. Compute W' = sG + tY
 *|   F 7. If W' is the point at infinity, reject the signature.
 *|   F 8. Compute r' = (e + W'_x) mod q
 *|   F 9. Accept the signature if and only if r equals r'
 *
 * (*) It is user responsibility to pass the ID string in the optional ancillary
 *     data of the API.
 */

#define SM2_VERIFY_MAGIC ((word_t)(0x9177c61e777f9f22ULL))
#define SM2_VERIFY_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == SM2_VERIFY_MAGIC), ret, err)

int _sm2_verify_init(struct ec_verify_context *ctx,
		       const u8 *sig, u8 siglen)
{
	bitcnt_t q_bit_len;
	u8 q_len;
	nn_src_t q;
	nn *r = NULL, *s = NULL;
	int ret, iszero1, iszero2, cmp1, cmp2;
	u8 Z[Z_INPUT_MAX_LEN];
	u16 Zlen;

	/* First, verify context has been initialized */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);

	/* Do some sanity checks on input params */
	ret = pub_key_check_initialized_and_type(ctx->pub_key, SM2); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		  (ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Make things more readable */
	q = &(ctx->pub_key->params->ec_gen_order);
	q_bit_len = ctx->pub_key->params->ec_gen_order_bitlen;
	q_len = (u8)BYTECEIL(q_bit_len);
	r = &(ctx->verify_data.sm2.r);
	s = &(ctx->verify_data.sm2.s);

	/* Check given signature length is the expected one */
	MUST_HAVE((siglen == SM2_SIGLEN(q_bit_len)), ret, err);

	/* Import r and s values from signature buffer */
	ret = nn_init_from_buf(r, sig, q_len); EG(ret, err);
	ret = nn_init_from_buf(s, sig + q_len, q_len); EG(ret, err);
	dbg_nn_print("r", r);
	dbg_nn_print("s", s);

	/* 1. Reject the signature if r or s is 0 or >= q. */
	ret = nn_iszero(r, &iszero1); EG(ret, err);
	ret = nn_iszero(s, &iszero2); EG(ret, err);
	ret = nn_cmp(r, q, &cmp1); EG(ret, err);
	ret = nn_cmp(s, q, &cmp2); EG(ret, err);
	MUST_HAVE((!iszero1) && (cmp1 < 0) && (!iszero2) && (cmp2 < 0), ret, err);

	/* Initialize the remaining of verify context. */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->verify_data.sm2.h_ctx)); EG(ret, err);

	/* Compute Z from the ID */
	ret = local_memset(Z, 0, sizeof(Z)); EG(ret, err);
	Zlen = sizeof(Z);
	ret = sm2_compute_Z(Z, &Zlen, ctx->adata, ctx->adata_len, ctx->pub_key, ctx->h->type); EG(ret, err);

	/* Update the hash function with Z */
	ret = ctx->h->hfunc_update(&(ctx->verify_data.sm2.h_ctx), Z, Zlen); EG(ret, err);

	ctx->verify_data.sm2.magic = SM2_VERIFY_MAGIC;

 err:
	VAR_ZEROIFY(q_len);
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(Zlen);
	PTR_NULLIFY(q);
	PTR_NULLIFY(r);
	PTR_NULLIFY(s);

	return ret;
}


int _sm2_verify_update(struct ec_verify_context *ctx,
			 const u8 *chunk, u32 chunklen)
{
	int ret;

	/*
	 * First, verify context has been initialized and public part too. This
	 * guarantees the context is a SM2 verification one and we do not
	 * update() or finalize() before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	SM2_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.sm2), ret, err);

	/* 2. Compute h = H(M1) w/ M1 = Z || M */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->verify_data.sm2.h_ctx), chunk, chunklen);

err:
	return ret;
}

int _sm2_verify_finalize(struct ec_verify_context *ctx)
{
	prj_pt sG, tY;
	prj_pt_t W_prime;
	nn e, tmp, r_prime;
	prj_pt_src_t G, Y;
	u8 hash[MAX_DIGEST_SIZE];
	nn_src_t q;
	nn *s, *r;
	nn t;
	u8 hsize;
	int ret, iszero, cmp;

	e.magic = tmp.magic = r_prime.magic = t.magic = WORD(0);
	sG.magic = tY.magic = WORD(0);

	/* NOTE: we reuse sG for W_prime to optimize local variables */
	W_prime = &sG;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an SM2
	 * verification one and we do not finalize() before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	SM2_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.sm2), ret, err);

	/* Zero init points */
	ret = local_memset(&sG, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&tY, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	G = &(ctx->pub_key->params->ec_gen);
	Y = &(ctx->pub_key->y);
	q = &(ctx->pub_key->params->ec_gen_order);
	hsize = ctx->h->digest_size;
	r = &(ctx->verify_data.sm2.r);
	s = &(ctx->verify_data.sm2.s);

	/* 2. Compute h = H(M1) w/ M1 = Z || M */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&(ctx->verify_data.sm2.h_ctx), hash); EG(ret, err);
	dbg_buf_print("h = H(m)", hash, hsize);

	/* 3. Compute t = r + s mod q */
	ret = nn_mod_add(&t, r, s, q); EG(ret, err);

	/* 4. Reject signature if t is 0 */
	ret = nn_iszero(&t, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	/* 5. Compute e = OS2I(h) mod q */
	ret = nn_init_from_buf(&tmp, hash, hsize); EG(ret, err);
	ret = local_memset(hash, 0, hsize); EG(ret, err);
	dbg_nn_print("h imported as nn", &tmp);
	ret = nn_mod(&e, &tmp, q); EG(ret, err);
	dbg_nn_print("e", &e);

	/* 6. Compute W' = sG + tY */
	ret = prj_pt_mul(&sG, s, G); EG(ret, err);
	ret = prj_pt_mul(&tY, &t, Y); EG(ret, err);
	ret = prj_pt_add(W_prime, &sG, &tY); EG(ret, err);

	/* 7. If W' is the point at infinity, reject the signature. */
	ret = prj_pt_iszero(W_prime, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	/* 8. Compute r' = (e + W'_x) mod q */
	ret = prj_pt_unique(W_prime, W_prime); EG(ret, err);
	dbg_nn_print("W'_x", &(W_prime->X.fp_val));
	dbg_nn_print("W'_y", &(W_prime->Y.fp_val));

	/* First, reduce W'_x mod q */
	ret = nn_mod(&r_prime, &(W_prime->X.fp_val), q); EG(ret, err);
	/* Then compute r' = (e + W'_x) mod q */
	ret = nn_mod_add(&r_prime, &e, &r_prime, q); EG(ret, err);

	/* 9. Accept the signature if and only if r equals r' */
	ret = nn_cmp(&r_prime, r, &cmp); EG(ret, err);
	ret = (cmp != 0) ? -1 : 0;

 err:
	nn_uninit(&e);
	nn_uninit(&tmp);
	nn_uninit(&r_prime);
	nn_uninit(&t);
	prj_pt_uninit(&sG);
	prj_pt_uninit(&tY);

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	IGNORE_RET_VAL(local_memset(&(ctx->verify_data.sm2), 0, sizeof(sm2_verify_data)));

	/* Clean what remains on the stack */
	PTR_NULLIFY(W_prime);
	PTR_NULLIFY(G);
	PTR_NULLIFY(Y);
	PTR_NULLIFY(q);
	PTR_NULLIFY(s);
	PTR_NULLIFY(r);
	VAR_ZEROIFY(hsize);

	return ret;
}

#else /* WITH_SIG_SM2 */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_SIG_SM2 */
