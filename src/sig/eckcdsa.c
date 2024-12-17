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
#ifdef WITH_SIG_ECKCDSA

#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_logical.h>

#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/ec_key.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "ECKCDSA"
#endif
#include <libecc/utils/dbg_sig.h>

/*
 * Initialize public key 'out_pub' from input private key 'in_priv'. The
 * function returns 0 on success, -1 on error.
 */
int eckcdsa_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv)
{
	prj_pt_src_t G;
	int ret, cmp;
	nn xinv;
	nn_src_t q;
	xinv.magic = WORD(0);

	MUST_HAVE((out_pub != NULL), ret, err);

	ret = priv_key_check_initialized_and_type(in_priv, ECKCDSA); EG(ret, err);

	/* For readability in the remaining of the function */
	q = &(in_priv->params->ec_gen_order);

	/* Zero init public key to be generated */
	ret = local_memset(out_pub, 0, sizeof(ec_pub_key)); EG(ret, err);

	/* Sanity check on key */
	MUST_HAVE((!nn_cmp(&(in_priv->x), q, &cmp)) && (cmp < 0), ret, err);

	/* Y = (x^-1)G */
	G = &(in_priv->params->ec_gen);
        /* NOTE: we use Fermat's little theorem inversion for
         * constant time here. This is possible since q is prime.
         */
	ret = nn_modinv_fermat(&xinv, &(in_priv->x), q); EG(ret, err);

	/* Use blinding when computing point scalar multiplication */
	ret = prj_pt_mul_blind(&(out_pub->y), &xinv, G); EG(ret, err);

	out_pub->key_type = ECKCDSA;
	out_pub->params = in_priv->params;
	out_pub->magic = PUB_KEY_MAGIC;

err:
	nn_uninit(&xinv);

	return ret;
}

/*
 * Helper providing ECKCDSA signature length when exported to a buffer based on
 * hash algorithm digest and block size, generator point order bit length, and
 * underlying prime field order bit length. The function returns 0 on success,
 * -1 on error. On success, signature length is provided via 'siglen' out
 * parameter.
 */
int eckcdsa_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize,
		   u8 *siglen)
{
	int ret;

	MUST_HAVE((siglen != NULL), ret, err);
	MUST_HAVE((p_bit_len <= CURVES_MAX_P_BIT_LEN) &&
		  (q_bit_len <= CURVES_MAX_Q_BIT_LEN) &&
		  (hsize <= MAX_DIGEST_SIZE) &&
		  (blocksize <= MAX_BLOCK_SIZE), ret, err);

	(*siglen) = (u8)ECKCDSA_SIGLEN(hsize, q_bit_len);
	ret = 0;

err:
	return ret;
}

/*
 * ISO 14888-3:2016 has some insane specific case when the digest size
 * (gamma) is larger than beta, the bit length of q (i.e. hsize >
 * bitlen(q), i.e. gamma > beta). In that case, both the values of h
 * (= H(z||m)) and r (= H(FE2OS(W_x))) must be post-processed/mangled
 * in the following way:
 *
 *  - h = I2BS(beta', (BS2I(gamma, h))) mod 2^beta'
 *  - r = I2BS(beta', (BS2I(gamma, r))) mod 2^beta'
 *
 * where beta' = 8 * ceil(beta / 8)
 *
 * There are two things to consider before implementing those steps
 * using various conversions to/from nn, shifting and masking:
 *
 *  - the expected post-processing work is simply clearing the first
 *    (gamma - beta') bits at the beginning of h and r to keep only
 *    last beta ones unmodified.
 *  - In the library, we do not work on bitstring but byte strings in
 *    all cases
 *  - In EC-KCDSA sig/verif, the result (h and then r) are then XORed
 *    together and then converted to an integer (the buffer being
 *    considered in big endian order)
 *
 * For that reason, this function simply takes a buffer 'buf' of
 * 'buflen' bytes and shifts it 'shift' bytes to the left, clearing
 * the trailing 'shift' bytes at the end of the buffer. The function
 * is expected to be used with 'shift' parameter set to
 * (gamma - beta') / 8.
 *
 * This is better presented on an example:
 *
 * shift = (gamma - beta') / 8 = 4
 * before: buf = { 0xff, 0xff, 0xff, 0x12, 0x34, 0x56, 0x78}
 * after : buf = { 0x34, 0x56, 0x78, 0x00, 0x00, 0x00, 0x00}
 */
ATTRIBUTE_WARN_UNUSED_RET static int buf_lshift(u8 *buf, u8 buflen, u8 shift)
{
	u8 i;
	int ret;

	MUST_HAVE((buf != NULL), ret, err);

	if (shift > buflen) {
		shift = buflen;
	}

	/* Start by shifting all trailing bytes to the left ... */
	for (i = shift; i < buflen; i++) {
		buf[i - shift] = buf[i];
	}

	/* Let's now zeroize the end of the buffer ... */
	for (i = 1; i <= shift; i++) {
		buf[buflen - i] = 0;
	}

	ret = 0;

err:
	return ret;
}

/*
 * Generic *internal* EC-KCDSA signature functions (init, update and finalize).
 * Their purpose is to allow passing a specific hash function (along with
 * its output size) and the random ephemeral key k, so that compliance
 * tests against test vectors can be made without ugly hack in the code
 * itself.
 *
 * Global EC-KCDSA signature process is as follows (I,U,F provides
 * information in which function(s) (init(), update() or finalize())
 * a specific step is performed):
 *
 *| IUF - EC-KCDSA signature
 *|
 *| IUF  1. Compute h = H(z||m)
 *|   F  2. If |H| > bitlen(q), set h to beta' rightmost bits of
 *|         bitstring h (w/ beta' = 8 * ceil(bitlen(q) / 8)), i.e.
 *|         set h to I2BS(beta', BS2I(|H|, h) mod 2^beta')
 *|   F  3. Get a random value k in ]0,q[
 *|   F  4. Compute W = (W_x,W_y) = kG
 *|   F  5. Compute r = H(FE2OS(W_x)).
 *|   F  6. If |H| > bitlen(q), set r to beta' rightmost bits of
 *|         bitstring r (w/ beta' = 8 * ceil(bitlen(q) / 8)), i.e.
 *|         set r to I2BS(beta', BS2I(|H|, r) mod 2^beta')
 *|   F  7. Compute e = OS2I(r XOR h) mod q
 *|   F  8. Compute s = x(k - e) mod q
 *|   F  9. if s == 0, restart at step 3.
 *|   F 10. return (r,s)
 *
 */

#define ECKCDSA_SIGN_MAGIC ((word_t)(0x45503fcf5114bf1eULL))
#define ECKCDSA_SIGN_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && \
		  ((A)->magic == ECKCDSA_SIGN_MAGIC), ret, err)

/*
 * ECKCDSA signature initialization function. Returns 0 on success, -1 on
 * error.
 */
int _eckcdsa_sign_init(struct ec_sign_context *ctx)
{
	u8 tmp_buf[LOCAL_MAX(2 * BYTECEIL(CURVES_MAX_P_BIT_LEN), MAX_BLOCK_SIZE)];
	const ec_pub_key *pub_key;
	aff_pt y_aff;
	u8 p_len;
	u16 z_len;
	int ret;
	y_aff.magic = WORD(0);

	/* First, verify context has been initialized */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);

	/* Additional sanity checks on input params from context */
	ret = key_pair_check_initialized_and_type(ctx->key_pair, ECKCDSA); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		(ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);

	/* Make things more readable */
	pub_key = &(ctx->key_pair->pub_key);
	p_len = (u8)BYTECEIL(pub_key->params->ec_fp.p_bitlen);
	z_len = ctx->h->block_size;

	/*
	 * 1. Compute h = H(z||m)
	 *
	 * We first need to compute z, the certificate data that will be
	 * prepended to the message m prior to hashing. In ISO-14888-3:2016,
	 * z is basically the concatenation of Yx and Yy (the affine coordinates
	 * of the public key Y) up to the block size of the hash function.
	 * If the concatenation of those coordinates is smaller than blocksize,
	 * 0 are appended.
	 *
	 * So, we convert the public key point to its affine representation and
	 * concatenate the two coordinates in a temporary (zeroized) buffer, of
	 * which the first z_len (i.e. blocksize) bytes are exported to z.
	 *
	 * Message m will be handled during following update() calls.
	 */
	ret = prj_pt_to_aff(&y_aff, &(pub_key->y)); EG(ret, err);
	ret = local_memset(tmp_buf, 0, sizeof(tmp_buf)); EG(ret, err);
	ret = fp_export_to_buf(tmp_buf, p_len, &(y_aff.x)); EG(ret, err);
	ret = fp_export_to_buf(tmp_buf + p_len, p_len, &(y_aff.y)); EG(ret, err);

	dbg_pub_key_print("Y", pub_key);

	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->sign_data.eckcdsa.h_ctx)); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->sign_data.eckcdsa.h_ctx), tmp_buf, z_len); EG(ret, err);
	ret = local_memset(tmp_buf, 0, sizeof(tmp_buf)); EG(ret, err);

	/* Initialize data part of the context */
	ctx->sign_data.eckcdsa.magic = ECKCDSA_SIGN_MAGIC;

 err:
	aff_pt_uninit(&y_aff);

	VAR_ZEROIFY(p_len);
	VAR_ZEROIFY(z_len);
	PTR_NULLIFY(pub_key);

	return ret;
}

/* ECKCDSA signature update function. Returns 0 on success, -1 on error. */
int _eckcdsa_sign_update(struct ec_sign_context *ctx,
			 const u8 *chunk, u32 chunklen)
{
	int ret;

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an EC-KCDSA
	 * signature one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ECKCDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.eckcdsa), ret, err);

	/* 1. Compute h = H(z||m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->sign_data.eckcdsa.h_ctx), chunk, chunklen);

err:
	return ret;
}

/*
 * ECKCDSA signature finalization function. Returns 0 on success, -1 on
 * error.
 */
int _eckcdsa_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen)
{
	prj_pt_src_t G;
	nn_src_t q, x;
	prj_pt kG;
	unsigned int i;
	nn e, tmp, s, k;
	u8 hzm[MAX_DIGEST_SIZE];
	u8 r[MAX_DIGEST_SIZE];
	u8 tmp_buf[BYTECEIL(CURVES_MAX_P_BIT_LEN)];
	hash_context r_ctx;
	const ec_priv_key *priv_key;
	u8 p_len, r_len, s_len, hsize, shift;
	bitcnt_t q_bit_len;
	int ret, iszero, cmp;
#ifdef USE_SIG_BLINDING
	/* b is the blinding mask */
	nn b, binv;
	b.magic = binv.magic = WORD(0);
#endif /* USE_SIG_BLINDING */

	kG.magic = WORD(0);
	e.magic = tmp.magic = s.magic = k.magic = WORD(0);

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an EC-KCDSA
	 * signature one and we do not finalize() before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ECKCDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.eckcdsa), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Zero init points */
	ret = local_memset(&kG, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	priv_key = &(ctx->key_pair->priv_key);
	G = &(priv_key->params->ec_gen);
	q = &(priv_key->params->ec_gen_order);
	hsize = ctx->h->digest_size;
	p_len = (u8)BYTECEIL(priv_key->params->ec_fp.p_bitlen);
	q_bit_len = priv_key->params->ec_gen_order_bitlen;
	r_len = (u8)ECKCDSA_R_LEN(hsize, q_bit_len);
	s_len = (u8)ECKCDSA_S_LEN(q_bit_len);
	x = &(priv_key->x);

	/* Sanity check */
	ret = nn_cmp(x, q, &cmp); EG(ret, err);
	/* This should not happen and means that our
	 * private key is not compliant!
	 */
	MUST_HAVE((cmp < 0), ret, err);

	MUST_HAVE((siglen == ECKCDSA_SIGLEN(hsize, q_bit_len)), ret, err);

	dbg_nn_print("p", &(priv_key->params->ec_fp.p));
	dbg_nn_print("q", q);
	dbg_priv_key_print("x", priv_key);
	dbg_ec_point_print("G", G);

	/* 1. Compute h = H(z||m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&(ctx->sign_data.eckcdsa.h_ctx), hzm); EG(ret, err);
	dbg_buf_print("h = H(z||m)  pre-mask", hzm, hsize);

	/*
	 * 2. If |H| > bitlen(q), set h to beta' rightmost bits of
	 *    bitstring h (w/ beta' = 8 * ceil(bitlen(q) / 8)), i.e.
	 *    set h to I2BS(beta', BS2I(|H|, h) mod 2^beta')
	 */
	shift = (u8)((hsize > r_len) ? (hsize - r_len) : 0);
	MUST_HAVE((hsize <= sizeof(hzm)), ret, err);

	ret = buf_lshift(hzm, hsize, shift); EG(ret, err);
	dbg_buf_print("h = H(z||m) post-mask", hzm, r_len);

 restart:
	/* 3. Get a random value k in ]0,q[ */
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

#ifdef USE_SIG_BLINDING
	/* Note: if we use blinding, k and e are multiplied by
	 * a random value b in ]0,q[ */
	ret = nn_get_random_mod(&b, q); EG(ret, err);
	dbg_nn_print("b", &b);
#endif /* USE_SIG_BLINDING */

	/* 4. Compute W = (W_x,W_y) = kG */
#ifdef USE_SIG_BLINDING
	/* We use blinding for the scalar multiplication */
	ret = prj_pt_mul_blind(&kG, &k, G); EG(ret, err);
#else
	ret = prj_pt_mul(&kG, &k, G); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	ret = prj_pt_unique(&kG, &kG); EG(ret, err);
	dbg_nn_print("W_x", &(kG.X.fp_val));
	dbg_nn_print("W_y", &(kG.Y.fp_val));

	/* 5 Compute r = h(FE2OS(W_x)). */
	ret = local_memset(tmp_buf, 0, sizeof(tmp_buf)); EG(ret, err);
	ret = fp_export_to_buf(tmp_buf, p_len, &(kG.X)); EG(ret, err);
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&r_ctx); EG(ret, err);
	ret = ctx->h->hfunc_update(&r_ctx, tmp_buf, p_len); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&r_ctx, r); EG(ret, err);
	ret = local_memset(tmp_buf, 0, p_len); EG(ret, err);
	ret = local_memset(&r_ctx, 0, sizeof(hash_context)); EG(ret, err);

	/*
	 * 6. If |H| > bitlen(q), set r to beta' rightmost bits of
	 *    bitstring r (w/ beta' = 8 * ceil(bitlen(q) / 8)), i.e.
	 *    set r to I2BS(beta', BS2I(|H|, r) mod 2^beta')
	 */
	dbg_buf_print("r  pre-mask", r, hsize);
	MUST_HAVE((hsize <= sizeof(r)), ret, err);

	ret = buf_lshift(r, hsize, shift); EG(ret, err);
	dbg_buf_print("r post-mask", r, r_len);

	/* 7. Compute e = OS2I(r XOR h) mod q */
	for (i = 0; i < r_len; i++) {
		hzm[i] ^= r[i];
	}
	ret = nn_init_from_buf(&tmp, hzm, r_len); EG(ret, err);
	ret = local_memset(hzm, 0, r_len); EG(ret, err);
	ret = nn_mod(&e, &tmp, q); EG(ret, err);
	dbg_nn_print("e", &e);

#ifdef USE_SIG_BLINDING
	/* In case of blinding, we compute (k*b - e*b) * x * b^-1 */
	ret = nn_mod_mul(&k, &k, &b, q); EG(ret, err);
	ret = nn_mod_mul(&e, &e, &b, q); EG(ret, err);
        /* NOTE: we use Fermat's little theorem inversion for
         * constant time here. This is possible since q is prime.
         */
	ret = nn_modinv_fermat(&binv, &b, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	/*
	 * 8. Compute s = x(k - e) mod q
	 *
	 * This is equivalent to computing s = x(k + (q - e)) mod q.
	 * This second version avoids checking if k < e before the
	 * subtraction, because e has already been reduced mod q
	 */
	ret = nn_mod_neg(&tmp, &e, q); EG(ret, err);
	ret = nn_mod_add(&tmp, &k, &tmp, q); EG(ret, err);
	ret = nn_mod_mul(&s, x, &tmp, q); EG(ret, err);
#ifdef USE_SIG_BLINDING
	/* Unblind s with b^-1 */
	ret = nn_mod_mul(&s, &s, &binv, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */

	/* 9. if s == 0, restart at step 3. */
	ret = nn_iszero(&s, &iszero); EG(ret, err);
	if (iszero) {
		goto restart;
	}

	dbg_nn_print("s", &s);

	/* 10. return (r,s) */
	ret = local_memcpy(sig, r, r_len); EG(ret, err);
	ret = local_memset(r, 0, r_len); EG(ret, err);
	ret = nn_export_to_buf(sig + r_len, s_len, &s);

 err:
	prj_pt_uninit(&kG);
	nn_uninit(&e);
	nn_uninit(&tmp);
	nn_uninit(&s);
	nn_uninit(&k);
#ifdef USE_SIG_BLINDING
	nn_uninit(&b);
	nn_uninit(&binv);
#endif /* USE_SIG_BLINDING */

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->sign_data.eckcdsa), 0, sizeof(eckcdsa_sign_data)));
	}

	PTR_NULLIFY(G);
	PTR_NULLIFY(q);
	PTR_NULLIFY(x);
	VAR_ZEROIFY(i);
	PTR_NULLIFY(priv_key);
	VAR_ZEROIFY(p_len);
	VAR_ZEROIFY(r_len);
	VAR_ZEROIFY(s_len);
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(hsize);

	return ret;
}

/*
 * Generic *internal* EC-KCDSA verification functions (init, update and
 * finalize). Their purpose is to allow passing a specific hash function
 * (along with its output size) and the random ephemeral key k, so that
 * compliance tests against test vectors can be made without ugly hack
 * in the code itself.
 *
 * Global EC-CKDSA verification process is as follows (I,U,F provides
 * information in which function(s) (init(), update() or finalize())
 * a specific step is performed):
 *
 *| IUF - EC-KCDSA verification
 *|
 *| I   1. Check the length of r:
 *|         - if |H| > bitlen(q), r must be of length
 *|           beta' = 8 * ceil(bitlen(q) / 8)
 *|         - if |H| <= bitlen(q), r must be of length hsize
 *| I   2. Check that s is in ]0,q[
 *| IUF 3. Compute h = H(z||m)
 *|   F 4. If |H| > bitlen(q), set h to beta' rightmost bits of
 *|        bitstring h (w/ beta' = 8 * ceil(bitlen(q) / 8)), i.e.
 *|        set h to I2BS(beta', BS2I(|H|, h) mod 2^beta')
 *|   F 5. Compute e = OS2I(r XOR h) mod q
 *|   F 6. Compute W' = sY + eG, where Y is the public key
 *|   F 7. Compute r' = h(W'x)
 *|   F 8. If |H| > bitlen(q), set r' to beta' rightmost bits of
 *|        bitstring r' (w/ beta' = 8 * ceil(bitlen(q) / 8)), i.e.
 *|        set r' to I2BS(beta', BS2I(|H|, r') mod 2^beta')
 *|   F 9. Check if r == r'
 *
 */

#define ECKCDSA_VERIFY_MAGIC ((word_t)(0xa836a75de66643aaULL))
#define ECKCDSA_VERIFY_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && \
		  ((A)->magic == ECKCDSA_VERIFY_MAGIC), ret, err)

/*
 * ECKCDSA verification finalization function. Returns 0 on success, -1 on error.
 */
int _eckcdsa_verify_init(struct ec_verify_context *ctx,
			 const u8 *sig, u8 siglen)
{
	u8 tmp_buf[LOCAL_MAX(2 * BYTECEIL(CURVES_MAX_P_BIT_LEN), MAX_BLOCK_SIZE)];
	u8 p_len, r_len, s_len, z_len;
	bitcnt_t q_bit_len;
	const ec_pub_key *pub_key;
	aff_pt y_aff;
	nn_src_t q;
	u8 hsize;
	int ret, iszero, cmp;
	nn s;
	y_aff.magic = s.magic = WORD(0);

	/* First, verify context has been initialized */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Do some sanity checks on input params */
	ret = pub_key_check_initialized_and_type(ctx->pub_key, ECKCDSA); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		  (ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Make things more readable */
	pub_key = ctx->pub_key;
	p_len = (u8)BYTECEIL(pub_key->params->ec_fp.p_bitlen);
	q_bit_len = pub_key->params->ec_gen_order_bitlen;
	q = &(pub_key->params->ec_gen_order);
	hsize = ctx->h->digest_size;
	r_len = (u8)ECKCDSA_R_LEN(hsize, q_bit_len);
	s_len = (u8)ECKCDSA_S_LEN(q_bit_len);
	z_len = ctx->h->block_size;

	/*
	 * 1. Check the length of r:
	 *     - if |H| > bitlen(q), r must be of length
	 *       beta' = 8 * ceil(bitlen(q) / 8)
	 *     - if |H| <= bitlen(q), r must be of length hsize
	 *
	 * As we expect the signature as the concatenation of r and s, the check
	 * is done by verifying the length of the signature is the expected one.
	 */
	MUST_HAVE((siglen == ECKCDSA_SIGLEN(hsize, q_bit_len)), ret, err);

	/* 2. Check that s is in ]0,q[ */
	ret = nn_init_from_buf(&s, sig + r_len, s_len); EG(ret, err);
	ret = nn_iszero(&s, &iszero); EG(ret, err);
	ret = nn_cmp(&s, q, &cmp); EG(ret, err);
	MUST_HAVE((!iszero) && (cmp < 0), ret, err);
	dbg_nn_print("s", &s);

	/*
	 * 3. Compute h = H(z||m)
	 *
	 * We first need to compute z, the certificate data that will be
	 * prepended to the message m prior to hashing. In ISO-14888-3:2016,
	 * z is basically the concatenation of Yx and Yy (the affine coordinates
	 * of the public key Y) up to the block size of the hash function.
	 * If the concatenation of those coordinates is smaller than blocksize,
	 * 0 are appended.
	 *
	 * So, we convert the public key point to its affine representation and
	 * concatenate the two coordinates in a temporary (zeroized) buffer, of
	 * which the first z_len (i.e. blocksize) bytes are exported to z.
	 *
	 * Message m will be handled during following update() calls.
	 */
	ret = prj_pt_to_aff(&y_aff, &(pub_key->y)); EG(ret, err);
	ret = local_memset(tmp_buf, 0, sizeof(tmp_buf)); EG(ret, err);
	ret = fp_export_to_buf(tmp_buf, p_len, &(y_aff.x)); EG(ret, err);
	ret = fp_export_to_buf(tmp_buf + p_len, p_len, &(y_aff.y)); EG(ret, err);

	dbg_pub_key_print("Y", pub_key);

	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->verify_data.eckcdsa.h_ctx)); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->verify_data.eckcdsa.h_ctx), tmp_buf,
				   z_len); EG(ret, err);
	ret = local_memset(tmp_buf, 0, sizeof(tmp_buf)); EG(ret, err);

	/*
	 * Initialize the verify context by storing r and s as imported
	 * from the signature
	 */
	ret = local_memcpy(ctx->verify_data.eckcdsa.r, sig, r_len); EG(ret, err);
	ret = nn_copy(&(ctx->verify_data.eckcdsa.s), &s); EG(ret, err);

	ctx->verify_data.eckcdsa.magic = ECKCDSA_VERIFY_MAGIC;

 err:
	aff_pt_uninit(&y_aff);
	nn_uninit(&s);

	if (ret && (ctx != NULL)) {
		/*
		 * Signature is invalid. Clear data part of the context.
		 * This will clear magic and avoid further reuse of the
		 * whole context.
		 */
		IGNORE_RET_VAL(local_memset(&(ctx->verify_data.eckcdsa), 0,
				     sizeof(eckcdsa_verify_data)));
	}

	/* Let's also clear what remains on the stack */
	PTR_NULLIFY(q);
	PTR_NULLIFY(pub_key);
	VAR_ZEROIFY(p_len);
	VAR_ZEROIFY(r_len);
	VAR_ZEROIFY(s_len);
	VAR_ZEROIFY(z_len);
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(hsize);

	return ret;
}

/* ECKCDSA verification update function. Returns 0 on success, -1 on error. */
int _eckcdsa_verify_update(struct ec_verify_context *ctx,
			   const u8 *chunk, u32 chunklen)
{
	int ret;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an EC-KCDSA
	 * verification one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ECKCDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.eckcdsa), ret, err);

	/* 3. Compute h = H(z||m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->verify_data.eckcdsa.h_ctx),
				   chunk, chunklen);

err:
	return ret;
}

/*
 * ECKCDSA verification finalization function. Returns 0 on success, -1 on error.
 */
int _eckcdsa_verify_finalize(struct ec_verify_context *ctx)
{
	u8 tmp_buf[BYTECEIL(CURVES_MAX_P_BIT_LEN)];
	bitcnt_t q_bit_len, p_bit_len;
	u8 p_len, r_len;
	prj_pt sY, eG;
	prj_pt_t Wprime;
	prj_pt_src_t G, Y;
	u8 r_prime[MAX_DIGEST_SIZE];
	const ec_pub_key *pub_key;
	hash_context r_prime_ctx;
	u8 hzm[MAX_DIGEST_SIZE];
	unsigned int i;
	nn_src_t q;
	nn e, tmp;
	u8 hsize, shift;
	int ret, check;
	u8 *r;
	nn *s;

	sY.magic = eG.magic = WORD(0);
	e.magic = tmp.magic = WORD(0);

	/* NOTE: we reuse eG for Wprime to optimize local variables */
	Wprime = &eG;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an EC-KCDSA
	 * verification one and we do not finalize() before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ECKCDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.eckcdsa), ret, err);

	/* Zero init points */
	ret = local_memset(&sY, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&eG, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	pub_key = ctx->pub_key;
	G = &(pub_key->params->ec_gen);
	Y = &(pub_key->y);
	q = &(pub_key->params->ec_gen_order);
	p_bit_len = pub_key->params->ec_fp.p_bitlen;
	q_bit_len = pub_key->params->ec_gen_order_bitlen;
	p_len = (u8)BYTECEIL(p_bit_len);
	hsize = ctx->h->digest_size;
	r_len = (u8)ECKCDSA_R_LEN(hsize, q_bit_len);
	r = ctx->verify_data.eckcdsa.r;
	s = &(ctx->verify_data.eckcdsa.s);

	/* 3. Compute h = H(z||m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&(ctx->verify_data.eckcdsa.h_ctx), hzm); EG(ret, err);
	dbg_buf_print("h = H(z||m)  pre-mask", hzm, hsize);

	/*
	 * 4. If |H| > bitlen(q), set h to beta' rightmost bits of
	 *    bitstring h (w/ beta' = 8 * ceil(bitlen(q) / 8)), i.e.
	 *    set h to I2BS(beta', BS2I(|H|, h) mod 2^beta')
	 */
	shift = (u8)((hsize > r_len) ? (hsize - r_len) : 0);
	MUST_HAVE(hsize <= sizeof(hzm), ret, err);
	ret = buf_lshift(hzm, hsize, shift); EG(ret, err);
	dbg_buf_print("h = H(z||m) post-mask", hzm, r_len);

	/* 5. Compute e = OS2I(r XOR h) mod q */
	for (i = 0; i < r_len; i++) {
		hzm[i] ^= r[i];
	}
	ret = nn_init_from_buf(&tmp, hzm, r_len); EG(ret, err);
	ret = local_memset(hzm, 0, hsize); EG(ret, err);
	ret = nn_mod(&e, &tmp, q); EG(ret, err);

	dbg_nn_print("e", &e);

	/* 6. Compute W' = sY + eG, where Y is the public key */
	ret = prj_pt_mul(&sY, s, Y); EG(ret, err);
	ret = prj_pt_mul(&eG, &e, G); EG(ret, err);
	ret = prj_pt_add(Wprime, &sY, &eG); EG(ret, err);
	ret = prj_pt_unique(Wprime, Wprime); EG(ret, err);
	dbg_nn_print("W'_x", &(Wprime->X.fp_val));
	dbg_nn_print("W'_y", &(Wprime->Y.fp_val));

	/* 7. Compute r' = h(W'x) */
	ret = local_memset(tmp_buf, 0, sizeof(tmp_buf)); EG(ret, err);
	ret = fp_export_to_buf(tmp_buf, p_len, &(Wprime->X)); EG(ret, err);
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&r_prime_ctx); EG(ret, err);
	ret = ctx->h->hfunc_update(&r_prime_ctx, tmp_buf, p_len); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&r_prime_ctx, r_prime); EG(ret, err);
	ret = local_memset(tmp_buf, 0, p_len); EG(ret, err);
	ret = local_memset(&r_prime_ctx, 0, sizeof(hash_context)); EG(ret, err);

	/*
	 * 8. If |H| > bitlen(q), set r' to beta' rightmost bits of
	 *    bitstring r' (w/ beta' = 8 * ceil(bitlen(q) / 8)), i.e.
	 *    set r' to I2BS(beta', BS2I(|H|, r') mod 2^beta')
	 */
	dbg_buf_print("r'  pre-mask", r_prime, hsize);
	ret = buf_lshift(r_prime, hsize, shift); EG(ret, err);
	dbg_buf_print("r' post-mask", r_prime, r_len);
	dbg_buf_print("r", r, r_len);

	/* 9. Check if r == r' */
	ret = are_equal(r, r_prime, r_len, &check); EG(ret, err);
	ret = check ? 0 : -1;

err:
	prj_pt_uninit(&sY);
	prj_pt_uninit(&eG);
	nn_uninit(&e);
	nn_uninit(&tmp);

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->verify_data.eckcdsa), 0,
				     sizeof(eckcdsa_verify_data)));
	}

	/* Let's also clear what remains on the stack */
	VAR_ZEROIFY(i);
	PTR_NULLIFY(Wprime);
	PTR_NULLIFY(G);
	PTR_NULLIFY(Y);
	PTR_NULLIFY(q);
	VAR_ZEROIFY(p_len);
	VAR_ZEROIFY(r_len);
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(p_bit_len);
	PTR_NULLIFY(pub_key);
	VAR_ZEROIFY(hsize);
	PTR_NULLIFY(r);
	PTR_NULLIFY(s);

	return ret;
}

#else /* WITH_SIG_ECKCDSA */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_SIG_ECKCDSA */
