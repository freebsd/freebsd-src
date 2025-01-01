/*
 *  Copyright (C) 2022 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#include <libecc/lib_ecc_config.h>
#if defined(WITH_SIG_BIP0340)

/* BIP0340 needs SHA-256: check it */
#if !defined(WITH_HASH_SHA256)
#error "Error: BIP0340 needs SHA-256 to be defined! Please define it in libecc config file"
#endif

#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_logical.h>

#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/sig_algs.h>
#include <libecc/sig/ec_key.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "BIP0340"
#endif
#include <libecc/utils/dbg_sig.h>

/*
 * The current implementation is for the BIP0340 signature as described
 * in https://github.com/bitcoin/bips/blob/master/bip-0340.mediawiki
 *
 * The BIP0340 signature is only compatible with SHA-256 and secp256k1,
 * but we extend it to any hash function or curve.
 *
 */

/* The "hash" function static prefixes */
#define BIP0340_AUX	 "BIP0340/aux"
#define BIP0340_NONCE	 "BIP0340/nonce"
#define BIP0340_CHALLENGE "BIP0340/challenge"

ATTRIBUTE_WARN_UNUSED_RET static int _bip0340_hash(const u8 *tag, u32 tag_len,
						   const u8 *m, u32 m_len,
						   const hash_mapping *hm, hash_context *h_ctx)
{
	int ret;
	u8 hash[MAX_DIGEST_SIZE];

	MUST_HAVE((h_ctx != NULL), ret, err);

	ret = hash_mapping_callbacks_sanity_check(hm); EG(ret, err);

	ret = hm->hfunc_init(h_ctx); EG(ret, err);
	ret = hm->hfunc_update(h_ctx, tag, tag_len); EG(ret, err);
	ret = hm->hfunc_finalize(h_ctx, hash); EG(ret, err);

	/* Now compute hash(hash(tag) || hash(tag) || m) */
	ret = hm->hfunc_init(h_ctx); EG(ret, err);
	ret = hm->hfunc_update(h_ctx, hash, hm->digest_size); EG(ret, err);
	ret = hm->hfunc_update(h_ctx, hash, hm->digest_size); EG(ret, err);
	ret = hm->hfunc_update(h_ctx, m, m_len); EG(ret, err);

	ret = 0;
err:
	return ret;
}

/* Set the scalar value depending on the parity bit of the input
 * point y coordinate.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _bip0340_set_scalar(nn_t scalar,
							 nn_src_t q,
							 prj_pt_src_t P)
{
	int ret, isodd, isone;

	/* Sanity check */
	ret = prj_pt_check_initialized(P); EG(ret, err);

	/* This operation is only meaningful on the "affine" representative.
	 * Check it.
	 */
	ret = nn_isone(&(P->Z.fp_val), &isone); EG(ret, err);
	MUST_HAVE((isone), ret, err);

	/* Check if Py is odd or even */
	ret = nn_isodd(&(P->Y.fp_val), &isodd); EG(ret, err);

	if(isodd){
		/* Replace the input scalar by (q - scalar)
		 * (its opposite modulo q)
		 */
		ret = nn_mod_neg(scalar, scalar, q); EG(ret, err);
	}

err:
	return ret;
}

/*
 * Generic *internal* helper for BIP340 public key initialization
 * functions. The function returns 0 on success, -1 on error.
 */
int bip0340_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv)
{
	prj_pt_src_t G;
	int ret;

	MUST_HAVE((out_pub != NULL), ret, err);

	/* Zero init public key to be generated */
	ret = local_memset(out_pub, 0, sizeof(ec_pub_key)); EG(ret, err);

	ret = priv_key_check_initialized_and_type(in_priv, BIP0340); EG(ret, err);

	/* Y = xG */
	G = &(in_priv->params->ec_gen);
	/* Use blinding when computing point scalar multiplication */
	ret = prj_pt_mul_blind(&(out_pub->y), &(in_priv->x), G); EG(ret, err);

	out_pub->key_type = BIP0340;
	out_pub->params = in_priv->params;
	out_pub->magic = PUB_KEY_MAGIC;

err:
	return ret;
}

/*
 * Generic *internal* helper for BIP0340 signature length functions.
 */
int bip0340_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize,
		    u8 *siglen)
{
	int ret;

	MUST_HAVE((siglen != NULL), ret, err);
	MUST_HAVE(((p_bit_len <= CURVES_MAX_P_BIT_LEN) &&
		   (q_bit_len <= CURVES_MAX_Q_BIT_LEN) &&
		   (hsize <= MAX_DIGEST_SIZE) && (blocksize <= MAX_BLOCK_SIZE)),
		   ret, err);

	(*siglen) = (u8)BIP0340_SIGLEN(p_bit_len, q_bit_len);
	ret = 0;

err:
	return ret;
}

/*
 * Generic *internal* helper for BIP0340 signature.
 * NOTE: because of the semi-deterministinc nonce generation
 * process, streaming mode is NOT supported for signing.
 * Hence the following all-in-one signature function.
 *
 * The function returns 0 on success, -1 on error.
 */
int _bip0340_sign(u8 *sig, u8 siglen, const ec_key_pair *key_pair,
		  const u8 *m, u32 mlen, int (*rand) (nn_t out, nn_src_t q),
		  ec_alg_type sig_type, hash_alg_type hash_type,
		  const u8 *adata, u16 adata_len)
{
	prj_pt_src_t G;
	prj_pt Y;
	nn_src_t q;
	nn k, d, e;
	prj_pt kG;
	const ec_priv_key *priv_key;
	const ec_pub_key *pub_key;
	bitcnt_t p_bit_len, q_bit_len;
	u8 i, p_len, q_len;
	int ret, cmp, iszero;
	hash_context h_ctx;
	const hash_mapping *hm;
	u8 buff[MAX_DIGEST_SIZE];
#ifdef USE_SIG_BLINDING
	/* b is the blinding mask */
	nn b, binv;
	b.magic = binv.magic = WORD(0);
#endif /* USE_SIG_BLINDING */

	k.magic = d.magic = e.magic = kG.magic = Y.magic = WORD(0);

	FORCE_USED_VAR(adata_len);

	/* No ancillary data is expected with BIP0340 */
	MUST_HAVE((key_pair != NULL) && (sig != NULL) && (adata == NULL), ret, err);

	/* Check our algorithm type */
	MUST_HAVE((sig_type == BIP0340), ret, err);

	/* Check that keypair is initialized */
	ret = key_pair_check_initialized_and_type(key_pair, BIP0340); EG(ret, err);

	/* Get the hash mapping */
	ret = get_hash_by_type(hash_type, &hm); EG(ret, err);
	MUST_HAVE((hm != NULL), ret, err);
	ret = hash_mapping_callbacks_sanity_check(hm); EG(ret, err);

	/* Make things more readable */
	priv_key = &(key_pair->priv_key);
	pub_key = &(key_pair->pub_key);
	G = &(priv_key->params->ec_gen);
	q = &(priv_key->params->ec_gen_order);
	p_bit_len = priv_key->params->ec_fp.p_bitlen;
	q_bit_len = priv_key->params->ec_gen_order_bitlen;
	q_len = (u8)BYTECEIL(q_bit_len);
	p_len = (u8)BYTECEIL(p_bit_len);

	/* Copy the public key point to work on the unique
	 * affine representative.
	 */
	ret = prj_pt_copy(&Y, &(pub_key->y)); EG(ret, err);
	ret = prj_pt_unique(&Y, &Y); EG(ret, err);

	ret = nn_init(&d, 0); EG(ret, err);
	ret = nn_copy(&d, &(priv_key->x)); EG(ret, err);

	dbg_nn_print("d", &d);

	/* Check signature size */
	MUST_HAVE((siglen == BIP0340_SIGLEN(p_bit_len, q_bit_len)), ret, err);
	MUST_HAVE((p_len == BIP0340_R_LEN(p_bit_len)), ret, err);
	MUST_HAVE((q_len == BIP0340_S_LEN(q_bit_len)), ret, err);

	/* Fail if d = 0 or d >= q */
	ret = nn_iszero(&d, &iszero); EG(ret, err);
	ret = nn_cmp(&d, q, &cmp); EG(ret, err);
	MUST_HAVE((!iszero) && (cmp < 0), ret, err);

	/* Adjust d depending on public key y */
	ret = _bip0340_set_scalar(&d, q, &Y); EG(ret, err);

	/* Compute the nonce in a deterministic way.
	 * First, we get the random auxilary data.
	 */
#ifdef NO_KNOWN_VECTORS
	/* NOTE: when we do not need self tests for known vectors,
	 * we can be strict about random function handler!
	 * This allows us to avoid the corruption of such a pointer.
	 */
	/* Sanity check on the handler before calling it */
	MUST_HAVE((rand == nn_get_random_mod), ret, err);
#endif
	ret = nn_init(&e, 0); EG(ret, err);
	ret = nn_one(&e); EG(ret, err);
	ret = nn_lshift(&e, &e, (bitcnt_t)(8 * q_len)); EG(ret, err);
	if(rand == NULL){
		rand = nn_get_random_mod;
	}
	ret = rand(&k, &e); EG(ret, err);
	dbg_nn_print("a", &k);

	MUST_HAVE((siglen >= q_len), ret, err);
	ret = nn_export_to_buf(&sig[0], q_len, &k); EG(ret, err);

	/* Compute the seed for the nonce computation */
	ret = _bip0340_hash((const u8*)BIP0340_AUX, sizeof(BIP0340_AUX) - 1,
		      &sig[0], q_len, hm, &h_ctx); EG(ret, err);
	ret = hm->hfunc_finalize(&h_ctx, buff); EG(ret, err);

	ret = nn_export_to_buf(&sig[0], q_len, &d); EG(ret, err);

	if(q_len > hm->digest_size){
		for(i = 0; i < hm->digest_size; i++){
			sig[i] ^= buff[i];
		}
		ret = _bip0340_hash((const u8*)BIP0340_NONCE, sizeof(BIP0340_NONCE) - 1,
				    &sig[0], q_len, hm, &h_ctx); EG(ret, err);
	}
	else{
		for(i = 0; i < q_len; i++){
			buff[i] ^= sig[i];
		}
		ret = _bip0340_hash((const u8*)BIP0340_NONCE, sizeof(BIP0340_NONCE) - 1,
				    &buff[0], hm->digest_size, hm, &h_ctx); EG(ret, err);
	}
	ret = fp_export_to_buf(&sig[0], p_len, &(Y.X)); EG(ret, err);
	ret = hm->hfunc_update(&h_ctx, &sig[0], p_len); EG(ret, err);
	ret = hm->hfunc_update(&h_ctx, m, mlen); EG(ret, err);
	ret = hm->hfunc_finalize(&h_ctx, buff); EG(ret, err);

	/* Now import the semi-deterministic nonce modulo q */
	ret = nn_init_from_buf(&k, buff, hm->digest_size); EG(ret, err);
	ret = nn_mod(&k, &k, q); EG(ret, err);

	dbg_nn_print("k", &k);

	/* Fail if the nonce is zero */
	ret = nn_iszero(&k, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	/* Proceed with the modulation exponentiation kG */
#ifdef USE_SIG_BLINDING
	/* We use blinding for the scalar multiplication */
	ret = prj_pt_mul_blind(&kG, &k, G); EG(ret, err);
#else
	ret = prj_pt_mul(&kG, &k, G); EG(ret, err);
#endif
	ret = prj_pt_unique(&kG, &kG); EG(ret, err);

	dbg_ec_point_print("(k G)", &kG);

	/* Update k depending on the kG y coordinate */
	ret = _bip0340_set_scalar(&k, q, &kG); EG(ret, err);

	/* Compute e */
	/* We export our r here */
	ret = fp_export_to_buf(&sig[0], p_len, &(kG.X)); EG(ret, err);
	ret = _bip0340_hash((const u8*)BIP0340_CHALLENGE, sizeof(BIP0340_CHALLENGE) - 1,
			    &sig[0], p_len, hm, &h_ctx); EG(ret, err);
	/* Export our public key */
	ret = fp_export_to_buf(&sig[0], p_len, &(Y.X)); EG(ret, err);
	ret = hm->hfunc_update(&h_ctx, &sig[0], p_len); EG(ret, err);
	/* Update with the message */
	ret = hm->hfunc_update(&h_ctx, m, mlen); EG(ret, err);
	ret = hm->hfunc_finalize(&h_ctx, buff); EG(ret, err);
	ret = nn_init_from_buf(&e, buff, hm->digest_size); EG(ret, err);
	ret = nn_mod(&e, &e, q); EG(ret, err);
	dbg_nn_print("e", &e);

	/* Export our r in the signature */
	dbg_nn_print("r", &(kG.X.fp_val));
	ret = fp_export_to_buf(&sig[0], p_len, &(kG.X)); EG(ret, err);

	/* Compute (k + ed) mod n */
#ifdef USE_SIG_BLINDING
	ret = nn_get_random_mod(&b, q); EG(ret, err);
	dbg_nn_print("b", &b);
#endif /* USE_SIG_BLINDING */

#ifdef USE_SIG_BLINDING
	/* Blind e with b */
	ret = nn_mod_mul(&e, &e, &b, q); EG(ret, err);
	/* Blind k with b */
	ret = nn_mod_mul(&k, &k, &b, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */

	ret = nn_mod_mul(&e, &e, &d, q); EG(ret, err);
	ret = nn_mod_add(&e, &k, &e, q); EG(ret, err);

#ifdef USE_SIG_BLINDING
	/* Unblind */
	/* NOTE: we use Fermat's little theorem inversion for
	 * constant time here. This is possible since q is prime.
	 */
	ret = nn_modinv_fermat(&binv, &b, q); EG(ret, err);
	ret = nn_mod_mul(&e, &e, &binv, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */

	/* Export our s in the signature */
	dbg_nn_print("s", &e);
	ret = nn_export_to_buf(&sig[p_len], q_len, &e); EG(ret, err);

err:
	PTR_NULLIFY(G);
	PTR_NULLIFY(q);
	PTR_NULLIFY(priv_key);
	PTR_NULLIFY(pub_key);
	PTR_NULLIFY(hm);

	prj_pt_uninit(&Y);
	nn_uninit(&k);
	nn_uninit(&e);
	nn_uninit(&d);

	return ret;
}

/* local helper for context sanity checks. Returns 0 on success, -1 on error. */
#define BIP0340_VERIFY_MAGIC ((word_t)(0x340175910abafcddULL))
#define BIP0340_VERIFY_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((const void *)(A)) != NULL) && \
		  ((A)->magic == BIP0340_VERIFY_MAGIC), ret, err)

/*
 * Generic *internal* helper for BIP0340 verification initialization functions.
 * The function returns 0 on success, -1 on error.
 */
int _bip0340_verify_init(struct ec_verify_context *ctx,
			 const u8 *sig, u8 siglen)
{
	bitcnt_t p_bit_len, q_bit_len;
	u8 p_len, q_len;
	int ret, cmp;
	nn_src_t q;
	prj_pt Y;
	fp *rx;
	nn *s;
	u8 Pubx[NN_MAX_BYTE_LEN];

	/* First, verify context has been initialized */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);

	/* Do some sanity checks on input params */
	ret = pub_key_check_initialized_and_type(ctx->pub_key, BIP0340); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		(ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);

	/* Make things more readable */
	q = &(ctx->pub_key->params->ec_gen_order);
	p_bit_len = ctx->pub_key->params->ec_fp.p_bitlen;
	q_bit_len = ctx->pub_key->params->ec_gen_order_bitlen;
	p_len = (u8)BYTECEIL(p_bit_len);
	q_len = (u8)BYTECEIL(q_bit_len);
	s = &(ctx->verify_data.bip0340.s);
	rx = &(ctx->verify_data.bip0340.r);

	MUST_HAVE((siglen == BIP0340_SIGLEN(p_bit_len, q_bit_len)), ret, err);
	MUST_HAVE((p_len == BIP0340_R_LEN(p_bit_len)), ret, err);
	MUST_HAVE((q_len == BIP0340_S_LEN(q_bit_len)), ret, err);

	/* Copy the public key point to work on the unique
	 * affine representative.
	 */
	ret = prj_pt_copy(&Y, &(ctx->pub_key->y)); EG(ret, err);
	ret = prj_pt_unique(&Y, &Y); EG(ret, err);

	/* Extract r and s */
	ret = fp_init(rx, ctx->pub_key->params->ec_curve.a.ctx); EG(ret, err);
	ret = fp_import_from_buf(rx, &sig[0], p_len); EG(ret, err);
	ret = nn_init_from_buf(s, &sig[p_len], q_len); EG(ret, err);
	ret = nn_cmp(s, q, &cmp); EG(ret, err);
	MUST_HAVE((cmp < 0), ret, err);

	dbg_nn_print("r", &(rx->fp_val));
	dbg_nn_print("s", s);

	/* Initialize our hash context */
	ret = _bip0340_hash((const u8*)BIP0340_CHALLENGE, sizeof(BIP0340_CHALLENGE) - 1,
			    &sig[0], p_len, ctx->h,
			    &(ctx->verify_data.bip0340.h_ctx)); EG(ret, err);
	ret = fp_export_to_buf(&Pubx[0], p_len, &(Y.X)); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->verify_data.bip0340.h_ctx), &Pubx[0], p_len); EG(ret, err);
	ret = local_memset(Pubx, 0, sizeof(Pubx)); EG(ret, err);

	ctx->verify_data.bip0340.magic = BIP0340_VERIFY_MAGIC;

err:
	PTR_NULLIFY(q);
	PTR_NULLIFY(rx);
	PTR_NULLIFY(s);

	prj_pt_uninit(&Y);

	if (ret && (ctx != NULL)) {
		/*
		 * Signature is invalid. Clear data part of the context.
		 * This will clear magic and avoid further reuse of the
		 * whole context.
		 */
		IGNORE_RET_VAL(local_memset(&(ctx->verify_data.bip0340), 0,
			     sizeof(bip0340_verify_data)));
	}

	return ret;
}

/*
 * Generic *internal* helper for BIP0340 verification update functions.
 * The function returns 0 on success, -1 on error.
 */
int _bip0340_verify_update(struct ec_verify_context *ctx,
			   const u8 *chunk, u32 chunklen)
{
	int ret;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an BIP0340
	 * verification one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	BIP0340_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.bip0340), ret, err);

	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->verify_data.bip0340.h_ctx), chunk,
				chunklen);

err:
	return ret;
}

/*
 * Generic *internal* helper for BIP0340 verification finalization
 * functions. The function returns 0 on success, -1 on error.
 */
int _bip0340_verify_finalize(struct ec_verify_context *ctx)
{
	prj_pt_src_t G;
	nn_src_t s, q;
	fp_src_t r;
	nn e;
	prj_pt sG, eY, Y;
	u8 e_buf[MAX_DIGEST_SIZE];
	u8 hsize;
	int ret, iszero, isodd, cmp;

	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	BIP0340_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.bip0340), ret, err);

	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);

	/* Zero init points */
	ret = local_memset(&sG, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&eY, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	G = &(ctx->pub_key->params->ec_gen);
	hsize = ctx->h->digest_size;
	q = &(ctx->pub_key->params->ec_gen_order);
	s = &(ctx->verify_data.bip0340.s);
	r = &(ctx->verify_data.bip0340.r);

	/* Copy the public key point to work on the unique
	 * affine representative.
	 */
	ret = prj_pt_copy(&Y, &(ctx->pub_key->y)); EG(ret, err);
	ret = prj_pt_unique(&Y, &Y); EG(ret, err);

	/* Compute e */
	ret = ctx->h->hfunc_finalize(&(ctx->verify_data.bip0340.h_ctx),
			&e_buf[0]); EG(ret, err);
	ret = nn_init_from_buf(&e, e_buf, hsize); EG(ret, err);
	ret = nn_mod(&e, &e, q); EG(ret, err);

	dbg_nn_print("e", &e);

	/* Compute s G - e Y */
	ret = prj_pt_mul(&sG, s, G); EG(ret, err);
	ret = nn_mod_neg(&e, &e, q); EG(ret, err); /* compute -e = (q - e) mod q */
	/* Do we have to "lift" Y the public key ? */
	ret = nn_isodd(&(Y.Y.fp_val), &isodd); EG(ret, err);
	if(isodd){
		/* If yes, negate the y coordinate */
		ret = fp_neg(&(Y.Y), &(Y.Y)); EG(ret, err);
	}
	ret = prj_pt_mul(&eY, &e, &Y); EG(ret, err);
	ret = prj_pt_add(&sG, &sG, &eY); EG(ret, err);
	ret = prj_pt_unique(&sG, &sG); EG(ret, err);

	dbg_ec_point_print("(s G - e Y)", &sG);

	/* Reject point at infinity */
	ret = prj_pt_iszero(&sG, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);

	/* Reject non even Y coordinate */
	ret = nn_isodd(&(sG.Y.fp_val), &isodd); EG(ret, err);
	MUST_HAVE((!isodd), ret, err);

	/* Check the x coordinate against r */
	ret = nn_cmp(&(r->fp_val), &(sG.X.fp_val), &cmp); EG(ret, err);
	ret = (cmp == 0) ? 0 : -1;

err:
	PTR_NULLIFY(G);
	PTR_NULLIFY(s);
	PTR_NULLIFY(q);
	PTR_NULLIFY(r);

	nn_uninit(&e);
	prj_pt_uninit(&sG);
	prj_pt_uninit(&eY);
	prj_pt_uninit(&Y);

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->verify_data.bip0340), 0,
			     sizeof(bip0340_verify_data)));
	}

	return ret;
}

/*
 * Helper to compute the seed to generate batch verification randomizing scalars.
 *
 */
/****************************************************/
/*
 * 32-bit integer manipulation macros (big endian)
 */
#ifndef GET_UINT32_LE
#define GET_UINT32_LE(n, b, i)                          \
do {                                                    \
        (n) =     ( ((u32) (b)[(i) + 3]) << 24 )        \
                | ( ((u32) (b)[(i) + 2]) << 16 )        \
                | ( ((u32) (b)[(i) + 1]) <<  8 )        \
                | ( ((u32) (b)[(i)    ])       );       \
} while( 0 )
#endif

#ifndef PUT_UINT32_LE
#define PUT_UINT32_LE(n, b, i)				\
do {							\
        (b)[(i) + 3] = (u8) ( (n) >> 24 );		\
        (b)[(i) + 2] = (u8) ( (n) >> 16 );		\
        (b)[(i) + 1] = (u8) ( (n) >>  8 );		\
        (b)[(i)    ] = (u8) ( (n)       );		\
} while( 0 )
#endif

#ifndef PUT_UINT32_BE
#define PUT_UINT32_BE(n, b, i)				\
do {							\
        (b)[(i)    ] = (u8) ( (n) >> 24 );		\
        (b)[(i) + 1] = (u8) ( (n) >> 16 );		\
        (b)[(i) + 2] = (u8) ( (n) >>  8 );		\
        (b)[(i) + 3] = (u8) ( (n)       );		\
} while( 0 )
#endif

#define _CHACHA20_ROTL_(x, y) (((x) << (y)) | ((x) >> ((sizeof(u32) * 8) - (y))))
#define CHACA20_ROTL(x, y) ((((y) < (sizeof(u32) * 8)) && ((y) > 0)) ? (_CHACHA20_ROTL_(x, y)) : (x))

#define CHACHA20_QROUND(a, b, c, d) do {			\
	(a) += (b);						\
	(d) ^= (a);						\
	(d) = CHACA20_ROTL((d), 16);				\
	(c) += (d);						\
	(b) ^= (c);						\
	(b) = CHACA20_ROTL((b), 12);				\
	(a) += (b);						\
	(d) ^= (a);						\
	(d) = CHACA20_ROTL((d), 8);				\
	(c) += (d);						\
	(b) ^= (c);						\
	(b) = CHACA20_ROTL((b), 7);				\
} while(0)

#define CHACHA20_INNER_BLOCK(s) do {				\
	CHACHA20_QROUND(s[0], s[4], s[ 8], s[12]);		\
	CHACHA20_QROUND(s[1], s[5], s[ 9], s[13]);		\
	CHACHA20_QROUND(s[2], s[6], s[10], s[14]);		\
	CHACHA20_QROUND(s[3], s[7], s[11], s[15]);		\
	CHACHA20_QROUND(s[0], s[5], s[10], s[15]);		\
	CHACHA20_QROUND(s[1], s[6], s[11], s[12]);		\
	CHACHA20_QROUND(s[2], s[7], s[ 8], s[13]);		\
	CHACHA20_QROUND(s[3], s[4], s[ 9], s[14]);		\
} while(0)

#define CHACHA20_MAX_ASKED_LEN 64

ATTRIBUTE_WARN_UNUSED_RET static int _bip0340_chacha20_block(const u8 key[32], const u8 nonce[12], u32 block_counter, u8 *stream, u32 stream_len){
	int ret;
	u32 state[16];
	u32 initial_state[16];
	unsigned int i;

	MUST_HAVE((stream != NULL), ret, err);
	MUST_HAVE((stream_len <= CHACHA20_MAX_ASKED_LEN), ret, err);

	/* Initial state */
	state[0] = 0x61707865;
	state[1] = 0x3320646e;
	state[2] = 0x79622d32;
	state[3] = 0x6b206574;

	for(i = 4; i < 12; i++){
		GET_UINT32_LE(state[i], key, (4 * (i - 4)));
	}
	state[12] = block_counter;
	for(i = 13; i < 16; i++){
		GET_UINT32_LE(state[i], nonce, (4 * (i - 13)));
	}

	/* Core loop */
	ret = local_memcpy(initial_state, state, sizeof(state)); EG(ret, err);
	for(i = 0; i < 10; i++){
		CHACHA20_INNER_BLOCK(state);
	}
	/* Serialize and output the block */
	for(i = 0; i < 16; i++){
		u32 tmp = (u32)(state[i] + initial_state[i]);
		PUT_UINT32_LE(tmp, (u8*)(&state[i]), 0);
	}
	ret = local_memcpy(stream, &state[0], stream_len);

err:
	return ret;
}

ATTRIBUTE_WARN_UNUSED_RET static int _bip0340_compute_batch_csprng_one_scalar(const u8 *seed, u32 seedlen,
									      u8 *scalar, u32 scalar_len, u32 num)
{
	int ret;
	u8 nonce[12];

	/* Sanity check for ChaCha20 */
	MUST_HAVE((seedlen == SHA256_DIGEST_SIZE) && (scalar_len <= CHACHA20_MAX_ASKED_LEN), ret, err);

	/* NOTE: nothing in the BIP340 specification fixes the nonce for
	 * ChaCha20. We simply use 0 here for the nonce. */
	ret = local_memset(nonce, 0, sizeof(nonce)); EG(ret, err);

	/* Use our CSPRNG based on ChaCha20 to generate the scalars */
	ret = _bip0340_chacha20_block(seed, nonce, num, scalar, scalar_len);

err:
	return ret;
}

ATTRIBUTE_WARN_UNUSED_RET static int _bip0340_compute_batch_csprng_scalars(const u8 *seed, u32 seedlen,
									   u8 *scalar, u32 scalar_len,
									   u32 *num, nn_src_t q,
									   bitcnt_t q_bit_len, u8 q_len,
									   nn_t a)
{
	int ret, iszero, cmp;
	u32 size, remain;

	MUST_HAVE((seed != NULL) && (scalar != NULL) && (num != NULL) && (a != NULL), ret, err);
	MUST_HAVE((scalar_len >= q_len), ret, err);

gen_scalar_again:
	size = remain = 0;
	while(size < q_len){
		MUST_HAVE((*num) < 0xffffffff, ret, err);
		remain = ((q_len - size) < CHACHA20_MAX_ASKED_LEN) ? (q_len - size): CHACHA20_MAX_ASKED_LEN;
		ret = _bip0340_compute_batch_csprng_one_scalar(seed, seedlen,
							       &scalar[size], remain,
							       (*num)); EG(ret, err);
		(*num)++;
		size += remain;
	}
	if((q_bit_len % 8) != 0){
		/* Handle the cutoff when q_bit_len is not a byte multiple */
		scalar[0] &= (u8)((0x1 << (q_bit_len % 8)) - 1);
	}
	/* Import the scalar */
	ret = nn_init_from_buf(a, scalar, q_len); EG(ret, err);
	/* Check if the scalar is between 1 and q-1 */
	ret = nn_iszero(a, &iszero); EG(ret, err);
	ret = nn_cmp(a, q, &cmp); EG(ret, err);
	if((iszero) || (cmp >= 0)){
		goto gen_scalar_again;
	}

	ret = 0;
err:
	return ret;
}

ATTRIBUTE_WARN_UNUSED_RET static int _bip0340_compute_batch_csprng_seed(const u8 **s, const u8 *s_len,
								        const ec_pub_key **pub_keys,
								        const u8 **m, const u32 *m_len, u32 num,
								        u8 p_len, u8 *seed, u32 seedlen)
{
	int ret;
	u32 i;
	hash_context h_ctx;
	u8 Pubx[NN_MAX_BYTE_LEN];
	const hash_mapping *hm;

	/* NOTE: sanity checks on inputs are performed by the upper layer */

	ret = local_memset(Pubx, 0, sizeof(Pubx)); EG(ret, err);

        /* Get our hash mapping for SHA-256 as we need a fixed 256-bit key
	 * for keying our ChaCha20 CSPRNG
	 */
        ret = get_hash_by_type(SHA256, &hm); EG(ret, err);
        MUST_HAVE((hm != NULL), ret, err);

	MUST_HAVE((seedlen == hm->digest_size), ret, err);

	/* As per specification, seed = seed_hash(pk1..pku || m1..mu || sig1..sigu), instantiated
	 * with SHA-256 */
	ret = hm->hfunc_init(&h_ctx); EG(ret, err);
	for(i = 0; i < num; i++){
		ret = fp_export_to_buf(&Pubx[0], p_len, &(pub_keys[i]->y.X)); EG(ret, err);
		ret = hm->hfunc_update(&h_ctx, &Pubx[0], p_len); EG(ret, err);
	}
	for(i = 0; i < num; i++){
		ret = hm->hfunc_update(&h_ctx, m[i], m_len[i]); EG(ret, err);
	}
	for(i = 0; i < num; i++){
		ret = hm->hfunc_update(&h_ctx, s[i], s_len[i]); EG(ret, err);
	}
	ret = hm->hfunc_finalize(&h_ctx, seed);

err:
	return ret;
}

/* Batch verification function:
 * This function takes multiple signatures/messages/public keys, and
 * checks in an optimized way all the signatures.
 *
 * This returns 0 if *all* the signatures are correct, and -1 if at least
 * one signature is not correct.
 *
 */
static int _bip0340_verify_batch_no_memory(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
		                           const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
					   hash_alg_type hash_type, const u8 **adata, const u16 *adata_len)
{
	nn_src_t q = NULL;
	prj_pt_src_t G = NULL;
	prj_pt_t R = NULL, Y = NULL;
	prj_pt Tmp, R_sum, P_sum;
	nn S, S_sum, e, a;
	fp rx;
	u8 hash[MAX_DIGEST_SIZE];
	u8 Pubx[NN_MAX_BYTE_LEN];
	const ec_pub_key *pub_key, *pub_key0;
	int ret, iszero, isodd, cmp;
	prj_pt_src_t pub_key_y;
	hash_context h_ctx;
	const hash_mapping *hm;
	ec_shortw_crv_src_t shortw_curve;
	ec_alg_type key_type = UNKNOWN_ALG;
	bitcnt_t p_bit_len, q_bit_len;
	u8 p_len, q_len;
	u16 hsize;
	u32 i;
	u8 chacha20_seed[SHA256_DIGEST_SIZE];
	u8 chacha20_scalar[BYTECEIL(CURVES_MAX_Q_BIT_LEN)];
	u32 chacha20_scalar_counter = 1;

	Tmp.magic = R_sum.magic = P_sum.magic = WORD(0);
	S.magic = S_sum.magic = e.magic = a.magic = WORD(0);
	rx.magic = WORD(0);

	FORCE_USED_VAR(adata_len);
	FORCE_USED_VAR(adata);

        /* First, some sanity checks */
        MUST_HAVE((s != NULL) && (pub_keys != NULL) && (m != NULL), ret, err);
        /* We need at least one element in our batch data bags */
        MUST_HAVE((num > 0), ret, err);

	/* Zeroize buffers */
	ret = local_memset(hash, 0, sizeof(hash)); EG(ret, err);
	ret = local_memset(Pubx, 0, sizeof(Pubx)); EG(ret, err);
	ret = local_memset(chacha20_seed, 0,sizeof(chacha20_seed)); EG(ret, err);
	ret = local_memset(chacha20_scalar, 0,sizeof(chacha20_scalar)); EG(ret, err);

        pub_key0 = pub_keys[0];
        MUST_HAVE((pub_key0 != NULL), ret, err);

        /* Get our hash mapping */
        ret = get_hash_by_type(hash_type, &hm); EG(ret, err);
        hsize = hm->digest_size;
        MUST_HAVE((hm != NULL), ret, err);

	for(i = 0; i < num; i++){
		u8 siglen;
		const u8 *sig = NULL;

		ret = pub_key_check_initialized_and_type(pub_keys[i], BIP0340); EG(ret, err);

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
		MUST_HAVE((siglen == BIP0340_SIGLEN(p_bit_len, q_bit_len)), ret, err);
		MUST_HAVE((siglen == (BIP0340_R_LEN(p_bit_len) + BIP0340_S_LEN(q_bit_len))), ret, err);

		/* Check the key type versus the algorithm */
		MUST_HAVE((key_type == sig_type), ret, err);

                if(i == 0){
                        /* Initialize our sums to zero/point at infinity */
                        ret = nn_init(&S_sum, 0); EG(ret, err);
                        ret = prj_pt_init(&R_sum, shortw_curve); EG(ret, err);
                        ret = prj_pt_zero(&R_sum); EG(ret, err);
                        ret = prj_pt_init(&P_sum, shortw_curve); EG(ret, err);
                        ret = prj_pt_zero(&P_sum); EG(ret, err);
			ret = prj_pt_init(&Tmp, shortw_curve); EG(ret, err);
                        ret = nn_init(&e, 0); EG(ret, err);
			ret = nn_init(&a, 0); EG(ret, err);
			/* Compute the ChaCha20 seed */
			ret = _bip0340_compute_batch_csprng_seed(s, s_len, pub_keys, m, m_len, num,
								 p_len, chacha20_seed,
								 sizeof(chacha20_seed)); EG(ret, err);
		}
		else{
			/* Get a pseudo-random scalar a for randomizing the linear combination */
			ret = _bip0340_compute_batch_csprng_scalars(chacha20_seed, sizeof(chacha20_seed),
								    chacha20_scalar, sizeof(chacha20_scalar),
								    &chacha20_scalar_counter, q,
								    q_bit_len, q_len, &a); EG(ret, err);
		}

		/***************************************************/
		/* Extract r and s */
		ret = fp_init(&rx, pub_key->params->ec_curve.a.ctx); EG(ret, err);
		ret = fp_import_from_buf(&rx, &sig[0], p_len); EG(ret, err);
		ret = nn_init_from_buf(&S, &sig[p_len], q_len); EG(ret, err);
		ret = nn_cmp(&S, q, &cmp); EG(ret, err);
		MUST_HAVE((cmp < 0), ret, err);

		dbg_nn_print("r", &(rx.fp_val));
		dbg_nn_print("s", &S);

		/***************************************************/
		/* Add S to the sum */
		/* Multiply S by a */
		if(i != 0){
			ret = nn_mod_mul(&S, &a, &S, q); EG(ret, err);
		}
		ret = nn_mod_add(&S_sum, &S_sum, &S, q); EG(ret, err);

		/***************************************************/
		R = &Tmp;
		/* Compute R from rx */
		ret = fp_copy(&(R->X), &rx); EG(ret, err);
		ret = aff_pt_y_from_x(&(R->Y), &(R->Z), &rx, shortw_curve); EG(ret, err);
		/* "Lift" R by choosing the even solution */
		ret = nn_isodd(&(R->Y.fp_val), &isodd); EG(ret, err);
		if(isodd){
			ret = fp_copy(&(R->Y), &(R->Z)); EG(ret, err);
		}
		ret = fp_one(&(R->Z)); EG(ret, err);
		/* Now multiply R by a */
		if(i != 0){
			ret = _prj_pt_unprotected_mult(R, &a, R); EG(ret, err);
		}
		/* Add to the sum */
		ret = prj_pt_add(&R_sum, &R_sum, R); EG(ret, err);
		dbg_ec_point_print("aR", R);

		/***************************************************/
		/* Compute P and add it to P_sum */
		Y = &Tmp;
		/* Copy the public key point to work on the unique
		 * affine representative.
		 */
		ret = prj_pt_copy(Y, pub_key_y); EG(ret, err);
		ret = prj_pt_unique(Y, Y); EG(ret, err);
		/* Do we have to "lift" Y the public key ? */
		ret = nn_isodd(&(Y->Y.fp_val), &isodd); EG(ret, err);
		if(isodd){
			/* If yes, negate the y coordinate */
			ret = fp_neg(&(Y->Y), &(Y->Y)); EG(ret, err);
		}
		dbg_ec_point_print("Y", Y);
		/* Compute e */
		ret = _bip0340_hash((const u8*)BIP0340_CHALLENGE, sizeof(BIP0340_CHALLENGE) - 1,
				    &sig[0], p_len, hm,
				    &h_ctx); EG(ret, err);
		ret = fp_export_to_buf(&Pubx[0], p_len, &(Y->X)); EG(ret, err);
		ret = hm->hfunc_update(&h_ctx, &Pubx[0], p_len); EG(ret, err);
		ret = hm->hfunc_update(&h_ctx, m[i], m_len[i]); EG(ret, err);
		ret = hm->hfunc_finalize(&h_ctx, hash); EG(ret, err);

		ret = nn_init_from_buf(&e, hash, hsize); EG(ret, err);
		ret = nn_mod(&e, &e, q); EG(ret, err);

		dbg_nn_print("e", &e);

		/* Multiply e by 'a' */
		if(i != 0){
			ret = nn_mod_mul(&e, &e, &a, q); EG(ret, err);
		}
		ret = _prj_pt_unprotected_mult(Y, &e, Y); EG(ret, err);
		dbg_ec_point_print("eY", Y);
		/* Add to the sum */
		ret = prj_pt_add(&P_sum, &P_sum, Y); EG(ret, err);
	}

	/* Sanity check */
        MUST_HAVE((q != NULL) && (G != NULL), ret, err);

	/* Compute S_sum * G */
	ret = nn_mod_neg(&S_sum, &S_sum, q); EG(ret, err); /* -S_sum = q - S_sum*/
	ret = _prj_pt_unprotected_mult(&Tmp, &S_sum, G); EG(ret, err);
	/* Add P_sum and R_sum */
	ret = prj_pt_add(&Tmp, &Tmp, &R_sum); EG(ret, err);
	ret = prj_pt_add(&Tmp, &Tmp, &P_sum); EG(ret, err);
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
        PTR_NULLIFY(R);
        PTR_NULLIFY(Y);

        prj_pt_uninit(&R_sum);
        prj_pt_uninit(&P_sum);
        prj_pt_uninit(&Tmp);
        nn_uninit(&S);
        nn_uninit(&S_sum);
        nn_uninit(&e);
        nn_uninit(&a);
	fp_uninit(&rx);

	return ret;
}

static int _bip0340_verify_batch(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
		                 const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
				 hash_alg_type hash_type, const u8 **adata, const u16 *adata_len,
				 verify_batch_scratch_pad *scratch_pad_area, u32 *scratch_pad_area_len)
{
	nn_src_t q = NULL;
	prj_pt_src_t G = NULL;
	prj_pt_t R = NULL, Y = NULL;
	nn S, a;
	nn_t e = NULL;
	fp rx;
	u8 hash[MAX_DIGEST_SIZE];
	u8 Pubx[NN_MAX_BYTE_LEN];
	const ec_pub_key *pub_key, *pub_key0;
	int ret, iszero, isodd, cmp;
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
	u8 chacha20_seed[SHA256_DIGEST_SIZE];
	u8 chacha20_scalar[BYTECEIL(CURVES_MAX_Q_BIT_LEN)];
	u32 chacha20_scalar_counter = 1;

	S.magic = a.magic = WORD(0);
	rx.magic = WORD(0);

	FORCE_USED_VAR(adata_len);
	FORCE_USED_VAR(adata);

        /* First, some sanity checks */
        MUST_HAVE((s != NULL) && (pub_keys != NULL) && (m != NULL), ret, err);

	MUST_HAVE((scratch_pad_area_len != NULL), ret, err);
	MUST_HAVE(((2 * num) >= num), ret, err);
        MUST_HAVE(((2 * num) + 1) >= num, ret, err);

	/* Zeroize buffers */
	ret = local_memset(hash, 0, sizeof(hash)); EG(ret, err);
	ret = local_memset(Pubx, 0, sizeof(Pubx)); EG(ret, err);
	ret = local_memset(chacha20_seed, 0,sizeof(chacha20_seed)); EG(ret, err);
	ret = local_memset(chacha20_scalar, 0,sizeof(chacha20_scalar)); EG(ret, err);

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
                        ret = _bip0340_verify_batch_no_memory(s, s_len, pub_keys, m, m_len, num, sig_type,
                                                              hash_type, adata, adata_len); EG(ret, err);
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

		ret = pub_key_check_initialized_and_type(pub_keys[i], BIP0340); EG(ret, err);

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
		MUST_HAVE((siglen == BIP0340_SIGLEN(p_bit_len, q_bit_len)), ret, err);
		MUST_HAVE((siglen == (BIP0340_R_LEN(p_bit_len) + BIP0340_S_LEN(q_bit_len))), ret, err);

		/* Check the key type versus the algorithm */
		MUST_HAVE((key_type == sig_type), ret, err);

                if(i == 0){
                        /* Initialize our sums to zero/point at infinity */
			ret = nn_init(&a, 0); EG(ret, err);
			ret = nn_init(&elements[(2 * num)].number, 0); EG(ret, err);
			ret = prj_pt_copy(&elements[(2 * num)].point, G); EG(ret, err);
			/* Compute the ChaCha20 seed */
			ret = _bip0340_compute_batch_csprng_seed(s, s_len, pub_keys, m, m_len, num,
								 p_len, chacha20_seed,
								 sizeof(chacha20_seed)); EG(ret, err);
		}
		else{
			/* Get a pseudo-random scalar a for randomizing the linear combination */
			ret = _bip0340_compute_batch_csprng_scalars(chacha20_seed, sizeof(chacha20_seed),
								    chacha20_scalar, sizeof(chacha20_scalar),
								    &chacha20_scalar_counter, q,
								    q_bit_len, q_len, &a); EG(ret, err);
		}

		/***************************************************/
		/* Extract r and s */
		ret = fp_init(&rx, pub_key->params->ec_curve.a.ctx); EG(ret, err);
		ret = fp_import_from_buf(&rx, &sig[0], p_len); EG(ret, err);
		ret = nn_init_from_buf(&S, &sig[p_len], q_len); EG(ret, err);
		ret = nn_cmp(&S, q, &cmp); EG(ret, err);
		MUST_HAVE((cmp < 0), ret, err);

		dbg_nn_print("r", &(rx.fp_val));
		dbg_nn_print("s", &S);

		/***************************************************/
		/* Add S to the sum */
		/* Multiply S by a */
		if(i != 0){
			ret = nn_mod_mul(&S, &a, &S, q); EG(ret, err);
		}
		ret = nn_mod_add(&elements[(2 * num)].number, &elements[(2 * num)].number,
				 &S, q); EG(ret, err);

		/***************************************************/
		/* Initialize R */
		R = &elements[i].point;
		ret = prj_pt_init(R, shortw_curve); EG(ret, err);
		/* Compute R from rx */
		ret = fp_copy(&(R->X), &rx); EG(ret, err);
		ret = aff_pt_y_from_x(&(R->Y), &(R->Z), &rx, shortw_curve); EG(ret, err);
		/* "Lift" R by choosing the even solution */
		ret = nn_isodd(&(R->Y.fp_val), &isodd); EG(ret, err);
		if(isodd){
			ret = fp_copy(&(R->Y), &(R->Z)); EG(ret, err);
		}
		ret = fp_one(&(R->Z)); EG(ret, err);

		if(i != 0){
			ret = nn_init(&elements[i].number, 0); EG(ret, err);
			ret = nn_copy(&elements[i].number, &a); EG(ret, err);
		}
		else{
			ret = nn_init(&elements[i].number, 0); EG(ret, err);
			ret = nn_one(&elements[i].number); EG(ret, err);
		}
		dbg_ec_point_print("R", R);

		/***************************************************/
		/* Compute P and add it to P_sum */
		Y = &elements[num + i].point;
		/* Copy the public key point to work on the unique
		 * affine representative.
		 */
		ret = prj_pt_copy(Y, pub_key_y); EG(ret, err);
		ret = prj_pt_unique(Y, Y); EG(ret, err);
		/* Do we have to "lift" Y the public key ? */
		ret = nn_isodd(&(Y->Y.fp_val), &isodd); EG(ret, err);
		if(isodd){
			/* If yes, negate the y coordinate */
			ret = fp_neg(&(Y->Y), &(Y->Y)); EG(ret, err);
		}
		dbg_ec_point_print("Y", Y);
		/* Compute e */
		/* Store the coefficient */
		e = &elements[num + i].number;
		ret = nn_init(e, 0); EG(ret, err);
		ret = _bip0340_hash((const u8*)BIP0340_CHALLENGE, sizeof(BIP0340_CHALLENGE) - 1,
				    &sig[0], p_len, hm,
				    &h_ctx); EG(ret, err);
		ret = fp_export_to_buf(&Pubx[0], p_len, &(Y->X)); EG(ret, err);
		ret = hm->hfunc_update(&h_ctx, &Pubx[0], p_len); EG(ret, err);
		ret = hm->hfunc_update(&h_ctx, m[i], m_len[i]); EG(ret, err);
		ret = hm->hfunc_finalize(&h_ctx, hash); EG(ret, err);

		ret = nn_init_from_buf(e, hash, hsize); EG(ret, err);
		ret = nn_mod(e, e, q); EG(ret, err);

		dbg_nn_print("e", e);

		/* Multiply e by 'a' */
		if(i != 0){
			ret = nn_mod_mul(e, e, &a, q); EG(ret, err);
		}
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
                        ret = _bip0340_verify_batch_no_memory(s, s_len, pub_keys, m, m_len, num, sig_type,
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
        PTR_NULLIFY(R);
        PTR_NULLIFY(Y);

        /* Unitialize all our scratch_pad_area */
        if((scratch_pad_area != NULL) && (scratch_pad_area_len != NULL)){
                IGNORE_RET_VAL(local_memset((u8*)scratch_pad_area, 0, (*scratch_pad_area_len)));
        }

        nn_uninit(&S);
        nn_uninit(&a);
	fp_uninit(&rx);

	return ret;
}

int bip0340_verify_batch(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
	                 const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
			 hash_alg_type hash_type, const u8 **adata, const u16 *adata_len,
			 verify_batch_scratch_pad *scratch_pad_area, u32 *scratch_pad_area_len)
{
	int ret;

	if(scratch_pad_area != NULL){
		MUST_HAVE((scratch_pad_area_len != NULL), ret, err);
		ret = _bip0340_verify_batch(s, s_len, pub_keys, m, m_len, num, sig_type,
				            hash_type, adata, adata_len,
				            scratch_pad_area, scratch_pad_area_len); EG(ret, err);

	}
	else{
		ret = _bip0340_verify_batch_no_memory(s, s_len, pub_keys, m, m_len, num, sig_type,
					              hash_type, adata, adata_len); EG(ret, err);
	}

err:
	return ret;
}

#else /* defined(WITH_SIG_BIP0340) */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* defined(WITH_SIG_BIP0340) */
