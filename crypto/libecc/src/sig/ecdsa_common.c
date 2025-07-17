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
#if defined(WITH_SIG_ECDSA) || defined(WITH_SIG_DECDSA)

#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_logical.h>

#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/ec_key.h>
#include <libecc/utils/utils.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "ECDSA"
#endif
#include <libecc/utils/dbg_sig.h>


#if defined(WITH_SIG_DECDSA)
#include <libecc/hash/hmac.h>

/*
 * Deterministic nonce generation function for deterministic ECDSA, as
 * described in RFC6979.
 * NOTE: Deterministic nonce generation for ECDSA is useful against attackers
 * in contexts where only poor RNG/entropy are available, or when nonce bits
 * leaking can be possible through side-channel attacks.
 * However, in contexts where fault attacks are easy to mount, deterministic
 * ECDSA can bring more security risks than regular ECDSA.
 *
 * Depending on the context where you use the library, choose carefully if
 * you want to use the deterministic version or not.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET static int __ecdsa_rfc6979_nonce(nn_t k, nn_src_t q, bitcnt_t q_bit_len,
				 nn_src_t x, const u8 *hash, u8 hsize,
				 hash_alg_type hash_type)
{
	int ret, cmp;
	u8 V[MAX_DIGEST_SIZE];
	u8 K[MAX_DIGEST_SIZE];
	u8 T[BYTECEIL(CURVES_MAX_Q_BIT_LEN) + MAX_DIGEST_SIZE];
	u8 priv_key_buff[EC_PRIV_KEY_MAX_SIZE];
	hmac_context hmac_ctx;
	bitcnt_t t_bit_len;
	u8 q_len;
	u8 hmac_size;
	u8 tmp;

	/* Sanity checks */
	MUST_HAVE((k != NULL), ret, err);
	MUST_HAVE((hash != NULL), ret, err);
	ret = nn_check_initialized(q); EG(ret, err);
	ret = nn_check_initialized(x); EG(ret, err);

	q_len = (u8)BYTECEIL(q_bit_len);

	MUST_HAVE((q_len <= EC_PRIV_KEY_MAX_SIZE) && (hsize <= MAX_BLOCK_SIZE), ret, err);

	/* Steps b. and c.: set V = 0x01 ... 0x01 and K = 0x00 ... 0x00 */
	ret = local_memset(V, 0x01, hsize); EG(ret, err);
	ret = local_memset(K, 0x00, hsize); EG(ret, err);
	/* Export our private key in a buffer */
	ret = nn_export_to_buf(priv_key_buff, q_len, x); EG(ret, err);
	/* Step d.: set K = HMAC_K(V || 0x00 || int2octets(x) || bits2octets(h1))
	 * where x is the private key and h1 the message hash.
	 */
	ret = hmac_init(&hmac_ctx, K, hsize, hash_type); EG(ret, err);
	ret = hmac_update(&hmac_ctx, V, hsize); EG(ret, err);

	tmp = 0x00;
	ret = hmac_update(&hmac_ctx, &tmp, 1); EG(ret, err);
	ret = hmac_update(&hmac_ctx, priv_key_buff, q_len); EG(ret, err);

	/* We compute bits2octets(hash) here */
	ret = nn_init_from_buf(k, hash, hsize); EG(ret, err);
	if((8 * hsize) > q_bit_len){
		ret = nn_rshift(k, k, (bitcnt_t)((8 * hsize) - q_bit_len)); EG(ret, err);
	}
	ret = nn_mod(k, k, q); EG(ret, err);
	ret = nn_export_to_buf(T, q_len, k); EG(ret, err);
	ret = hmac_update(&hmac_ctx, T, q_len); EG(ret, err);
	hmac_size = sizeof(K);
	ret = hmac_finalize(&hmac_ctx, K, &hmac_size); EG(ret, err);

	/* Step e.: set V = HMAC_K(V) */
	hmac_size = sizeof(V);
	ret = hmac(K, hsize, hash_type, V, hsize, V, &hmac_size); EG(ret, err);
	/*  Step f.: K = HMAC_K(V || 0x01 || int2octets(x) || bits2octets(h1)) */
	ret = hmac_init(&hmac_ctx, K, hsize, hash_type); EG(ret, err);
	ret = hmac_update(&hmac_ctx, V, hsize); EG(ret, err);

	tmp = 0x01;
	ret = hmac_update(&hmac_ctx, &tmp, 1); EG(ret, err);
	ret = hmac_update(&hmac_ctx, priv_key_buff, q_len); EG(ret, err);

	/* We compute bits2octets(hash) here */
	ret = hmac_update(&hmac_ctx, T, q_len); EG(ret, err);
	hmac_size = sizeof(K);
	ret = hmac_finalize(&hmac_ctx, K, &hmac_size); EG(ret, err);
	/* Step g.: set V = HMAC_K(V)*/
	hmac_size = sizeof(V);
	ret = hmac(K, hsize, hash_type, V, hsize, V, &hmac_size); EG(ret, err);

	/* Step h. now apply the generation algorithm until we get
	 * a proper nonce value:
	 * 1.  Set T to the empty sequence.  The length of T (in bits) is
	 * denoted tlen; thus, at that point, tlen = 0.
	 * 2.  While tlen < qlen, do the following:
	 *    V = HMAC_K(V)
	 *    T = T || V
	 * 3.  Compute:
	 *    k = bits2int(T)
	 * If that value of k is within the [1,q-1] range, and is
	 * suitable for DSA or ECDSA (i.e., it results in an r value
	 * that is not 0; see Section 3.4), then the generation of k is
	 * finished.  The obtained value of k is used in DSA or ECDSA.
	 * Otherwise, compute:
	 *    K = HMAC_K(V || 0x00)
	 *    V = HMAC_K(V)
	 * and loop (try to generate a new T, and so on).
	 */
restart:
	t_bit_len = 0;
	while(t_bit_len < q_bit_len){
		/* V = HMAC_K(V) */
		hmac_size = sizeof(V);
		ret = hmac(K, hsize, hash_type, V, hsize, V, &hmac_size); EG(ret, err);
		ret = local_memcpy(&T[BYTECEIL(t_bit_len)], V, hmac_size); EG(ret, err);
		t_bit_len = (bitcnt_t)(t_bit_len + (8 * hmac_size));
	}
	ret = nn_init_from_buf(k, T, q_len); EG(ret, err);
	if((8 * q_len) > q_bit_len){
		ret = nn_rshift(k, k, (bitcnt_t)((8 * q_len) - q_bit_len)); EG(ret, err);
	}
	ret = nn_cmp(k, q, &cmp); EG(ret, err);
	if(cmp >= 0){
		/* K = HMAC_K(V || 0x00) */
		ret = hmac_init(&hmac_ctx, K, hsize, hash_type); EG(ret, err);
		ret = hmac_update(&hmac_ctx, V, hsize); EG(ret, err);

		tmp = 0x00;
		ret = hmac_update(&hmac_ctx, &tmp, 1); EG(ret, err);

		hmac_size = sizeof(K);
		ret = hmac_finalize(&hmac_ctx, K, &hmac_size); EG(ret, err);
		/* V = HMAC_K(V) */
		hmac_size = sizeof(V);
		ret = hmac(K, hsize, hash_type, V, hsize, V, &hmac_size); EG(ret, err);

		goto restart;
	}

err:
	return ret;
}
#endif

int __ecdsa_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv,
			 ec_alg_type key_type)
{
	prj_pt_src_t G;
	int ret, cmp;
	nn_src_t q;

	MUST_HAVE((out_pub != NULL), ret, err);

	/* Zero init public key to be generated */
	ret = local_memset(out_pub, 0, sizeof(ec_pub_key)); EG(ret, err);

	ret = priv_key_check_initialized_and_type(in_priv, key_type); EG(ret, err);
	q = &(in_priv->params->ec_gen_order);

	/* Sanity check on key compliance */
	MUST_HAVE((!nn_cmp(&(in_priv->x), q, &cmp)) && (cmp < 0), ret, err);

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

int __ecdsa_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen)
{
	int ret;

	MUST_HAVE(siglen != NULL, ret, err);
	MUST_HAVE((p_bit_len <= CURVES_MAX_P_BIT_LEN) &&
		  (q_bit_len <= CURVES_MAX_Q_BIT_LEN) &&
		  (hsize <= MAX_DIGEST_SIZE) && (blocksize <= MAX_BLOCK_SIZE), ret, err);
	(*siglen) = (u8)ECDSA_SIGLEN(q_bit_len);
	ret = 0;

err:
	return ret;
}

/*
 * Generic *internal* ECDSA signature functions (init, update and finalize).
 * Their purpose is to allow passing a specific hash function (along with
 * its output size) and the random ephemeral key k, so that compliance
 * tests against test vectors can be made without ugly hack in the code
 * itself.
 *
 * Global EC-DSA signature process is as follows (I,U,F provides
 * information in which function(s) (init(), update() or finalize())
 * a specific step is performed):
 *
 *| IUF	 - ECDSA signature
 *|
 *|  UF	 1. Compute h = H(m)
 *|   F	 2. If |h| > bitlen(q), set h to bitlen(q)
 *|	    leftmost (most significant) bits of h
 *|   F	 3. e = OS2I(h) mod q
 *|   F	 4. Get a random value k in ]0,q[
 *|   F	 5. Compute W = (W_x,W_y) = kG
 *|   F	 6. Compute r = W_x mod q
 *|   F	 7. If r is 0, restart the process at step 4.
 *|   F	 8. If e == rx, restart the process at step 4.
 *|   F	 9. Compute s = k^-1 * (xr + e) mod q
 *|   F 10. If s is 0, restart the process at step 4.
 *|   F 11. Return (r,s)
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
 * c) in EC-DSA, the public part of the key is not needed per se during the
 *    signature but - as it is needed in other signature algs implemented
 *    in the library - the whole key pair is passed instead of just the
 *    private key.
 */

#define ECDSA_SIGN_MAGIC ((word_t)(0x80299a2bf630945bULL))
#define ECDSA_SIGN_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == ECDSA_SIGN_MAGIC), ret, err)

int __ecdsa_sign_init(struct ec_sign_context *ctx, ec_alg_type key_type)
{
	int ret;

	/* First, verify context has been initialized */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);

	/* Additional sanity checks on input params from context */
	ret = key_pair_check_initialized_and_type(ctx->key_pair, key_type); EG(ret, err);

	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		  (ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);

	/*
	 * Initialize hash context stored in our private part of context
	 * and record data init has been done
	 */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->sign_data.ecdsa.h_ctx)); EG(ret, err);

	ctx->sign_data.ecdsa.magic = ECDSA_SIGN_MAGIC;

err:
	return ret;
}

int __ecdsa_sign_update(struct ec_sign_context *ctx,
		       const u8 *chunk, u32 chunklen, ec_alg_type key_type)
{
	int ret;

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an ECDSA
	 * signature one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ECDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.ecdsa), ret, err);

	/* Additional sanity checks on input params from context */
	ret = key_pair_check_initialized_and_type(ctx->key_pair, key_type); EG(ret, err);

	/* 1. Compute h = H(m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->sign_data.ecdsa.h_ctx), chunk, chunklen);

err:
	return ret;
}

int __ecdsa_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen,
			  ec_alg_type key_type)
{
	int ret, iszero, cmp;
	const ec_priv_key *priv_key;
	prj_pt_src_t G;
	u8 hash[MAX_DIGEST_SIZE];
	bitcnt_t rshift, q_bit_len;
	prj_pt kG;
	nn_src_t q, x;
	u8 hsize, q_len;
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
	MUST_HAVE((sig != NULL), ret, err);

	/* Additional sanity checks on input params from context */
	ret = key_pair_check_initialized_and_type(ctx->key_pair, key_type); EG(ret, err);

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

	MUST_HAVE((priv_key->key_type == key_type), ret, err);

	/* Sanity check */
	ret = nn_cmp(x, q, &cmp); EG(ret, err);
	/* This should not happen and means that our
	 * private key is not compliant!
	 */
	MUST_HAVE((cmp < 0), ret, err);

	dbg_nn_print("p", &(priv_key->params->ec_fp.p));
	dbg_nn_print("q", &(priv_key->params->ec_gen_order));
	dbg_priv_key_print("x", priv_key);
	dbg_ec_point_print("G", &(priv_key->params->ec_gen));
	dbg_pub_key_print("Y", &(ctx->key_pair->pub_key));

	/* Check given signature buffer length has the expected size */
	MUST_HAVE((siglen == ECDSA_SIGLEN(q_bit_len)), ret, err);

	/* 1. Compute h = H(m) */
	ret = local_memset(hash, 0, hsize); EG(ret, err);
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&(ctx->sign_data.ecdsa.h_ctx), hash); EG(ret, err);
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
	dbg_nn_print("h initial import as nn", &e);
	if (rshift) {
		ret = nn_rshift_fixedlen(&e, &e, rshift); EG(ret, err);
	}
	dbg_nn_print("h	  final import as nn", &e);
	ret = nn_mod(&e, &e, q); EG(ret, err);
	dbg_nn_print("e", &e);

 restart:
	/* 4. get a random value k in ]0,q[ */
#ifdef NO_KNOWN_VECTORS
	/* NOTE: when we do not need self tests for known vectors,
	 * we can be strict about random function handler!
	 * This allows us to avoid the corruption of such a pointer.
	 */
	/* Sanity check on the handler before calling it */
	if(ctx->rand != nn_get_random_mod){
#ifdef WITH_SIG_DECDSA
		/* In deterministic ECDSA, nevermind! */
		if(key_type != DECDSA)
#endif
		{
			ret = -1;
			goto err;
		}
	}
#endif
	if(ctx->rand != NULL){
		/* Non-deterministic generation, or deterministic with
		 * test vectors.
		 */
		ret = ctx->rand(&k, q);
	}
	else
#if defined(WITH_SIG_DECDSA)
	{
		/* Only applies for DETERMINISTIC ECDSA */
		if(key_type != DECDSA){
			ret = -1;
			goto err;
		}
		/* Deterministically generate k as RFC6979 mandates */
		ret = __ecdsa_rfc6979_nonce(&k, q, q_bit_len, &(priv_key->x),
					    hash, hsize, ctx->h->type);
	}
#else
	{
		/* NULL rand function is not accepted for regular ECDSA */
		ret = -1;
		goto err;
	}
#endif
	if (ret) {
		ret = -1;
		goto err;
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
	ret = nn_iszero(&r, &iszero); EG(ret, err);
	if (iszero) {
		goto restart;
	}

	/* Clean hash buffer as we do not need it anymore */
	ret = local_memset(hash, 0, hsize); EG(ret, err);

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
	ret = nn_cmp(&e, &tmp, &cmp); EG(ret, err);
	if (!cmp) {
		goto restart;
	}

	/* 9. Compute s = k^-1 * (xr + e) mod q */

	/* tmp = (e + xr) mod q */
	ret = nn_mod_add(&tmp, &tmp, &e, q); EG(ret, err);
	dbg_nn_print("(xr + e) mod q", &tmp);

#ifdef USE_SIG_BLINDING
	/*
	 * In case of blinding, we compute (b*k)^-1, and b^-1 will
	 * automatically unblind (r*x) in the following.
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
	ret = nn_iszero(&s, &iszero); EG(ret, err);
	if (iszero) {
		goto restart;
	}

	/* 11. return (r,s) */
	ret = nn_export_to_buf(sig + q_len, q_len, &s);

err:
	nn_uninit(&k);
	nn_uninit(&r);
	nn_uninit(&e);
	nn_uninit(&tmp);
	nn_uninit(&s);
	nn_uninit(&kinv);
	prj_pt_uninit(&kG);
#ifdef USE_SIG_BLINDING
	nn_uninit(&b);
#endif

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

	return ret;
}

/*
 * Generic *internal* ECDSA verification functions (init, update and finalize).
 * Their purpose is to allow passing a specific hash function (along with
 * its output size) and the random ephemeral key k, so that compliance
 * tests against test vectors can be made without ugly hack in the code
 * itself.
 *
 * Global ECDSA verification process is as follows (I,U,F provides
 * information in which function(s) (init(), update() or finalize())
 * a specific step is performed):
 *
 *| IUF	 - ECDSA verification
 *|
 *| I	 1. Reject the signature if r or s is 0.
 *|  UF	 2. Compute h = H(m)
 *|   F	 3. If |h| > bitlen(q), set h to bitlen(q)
 *|	    leftmost (most significant) bits of h
 *|   F	 4. Compute e = OS2I(h) mod q
 *|   F	 5. Compute u = (s^-1)e mod q
 *|   F	 6. Compute v = (s^-1)r mod q
 *|   F	 7. Compute W' = uG + vY
 *|   F	 8. If W' is the point at infinity, reject the signature.
 *|   F	 9. Compute r' = W'_x mod q
 *|   F 10. Accept the signature if and only if r equals r'
 *
 */

#define ECDSA_VERIFY_MAGIC ((word_t)(0x5155fe73e7fd51beULL))
#define ECDSA_VERIFY_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == ECDSA_VERIFY_MAGIC), ret, err)

int __ecdsa_verify_init(struct ec_verify_context *ctx, const u8 *sig, u8 siglen,
			ec_alg_type key_type)
{
	bitcnt_t q_bit_len;
	u8 q_len;
	nn_src_t q;
	nn *r, *s;
	int ret, cmp1, cmp2, iszero1, iszero2;

	/* First, verify context has been initialized */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);

	/* Do some sanity checks on input params */
	ret = pub_key_check_initialized_and_type(ctx->pub_key, key_type); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		(ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Make things more readable */
	q = &(ctx->pub_key->params->ec_gen_order);
	q_bit_len = ctx->pub_key->params->ec_gen_order_bitlen;
	q_len = (u8)BYTECEIL(q_bit_len);
	r = &(ctx->verify_data.ecdsa.r);
	s = &(ctx->verify_data.ecdsa.s);

	/* Check given signature length is the expected one */
	MUST_HAVE((siglen == ECDSA_SIGLEN(q_bit_len)), ret, err);

	/* Import r and s values from signature buffer */
	ret = nn_init_from_buf(r, sig, q_len); EG(ret, err);
	ret = nn_init_from_buf(s, sig + q_len, q_len); EG(ret, err);
	dbg_nn_print("r", r);
	dbg_nn_print("s", s);

	/* 1. Reject the signature if r or s is 0. */
	ret = nn_iszero(r, &iszero1); EG(ret, err);
	ret = nn_iszero(s, &iszero2); EG(ret, err);
	ret = nn_cmp(r, q, &cmp1); EG(ret, err);
	ret = nn_cmp(s, q, &cmp2); EG(ret, err);
	MUST_HAVE(((!iszero1) && (cmp1 < 0) && !iszero2 && (cmp2 < 0)), ret, err);

	/* Initialize the remaining of verify context. */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->verify_data.ecdsa.h_ctx)); EG(ret, err);

	ctx->verify_data.ecdsa.magic = ECDSA_VERIFY_MAGIC;

 err:
	VAR_ZEROIFY(q_len);
	VAR_ZEROIFY(q_bit_len);
	PTR_NULLIFY(q);
	PTR_NULLIFY(r);
	PTR_NULLIFY(s);

	return ret;
}

int __ecdsa_verify_update(struct ec_verify_context *ctx,
			 const u8 *chunk, u32 chunklen, ec_alg_type key_type)
{
	int ret;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an ECDSA
	 * verification one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ECDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.ecdsa), ret, err);
	/* Do some sanity checks on input params */
	ret = pub_key_check_initialized_and_type(ctx->pub_key, key_type); EG(ret, err);

	/* 2. Compute h = H(m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->verify_data.ecdsa.h_ctx), chunk, chunklen);

err:
	return ret;
}

int __ecdsa_verify_finalize(struct ec_verify_context *ctx,
			    ec_alg_type key_type)
{
	prj_pt uG, vY;
	prj_pt_t W_prime;
	nn e, sinv, uv, r_prime;
	prj_pt_src_t G, Y;
	u8 hash[MAX_DIGEST_SIZE];
	bitcnt_t rshift, q_bit_len;
	nn_src_t q;
	nn *s, *r;
	u8 hsize;
	int ret, iszero, cmp;

	uG.magic = vY.magic = WORD(0);
	e.magic = sinv.magic = uv.magic = r_prime.magic = WORD(0);

	/* NOTE: we reuse uG for W_prime to optimize local variables */
	W_prime = &uG;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an ECDSA
	 * verification one and we do not finalize() before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ECDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.ecdsa), ret, err);
	/* Do some sanity checks on input params */
	ret = pub_key_check_initialized_and_type(ctx->pub_key, key_type); EG(ret, err);

	/* Zero init points */
	ret = local_memset(&uG, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&vY, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	G = &(ctx->pub_key->params->ec_gen);
	Y = &(ctx->pub_key->y);
	q = &(ctx->pub_key->params->ec_gen_order);
	q_bit_len = ctx->pub_key->params->ec_gen_order_bitlen;
	hsize = ctx->h->digest_size;
	r = &(ctx->verify_data.ecdsa.r);
	s = &(ctx->verify_data.ecdsa.s);

	/* 2. Compute h = H(m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&(ctx->verify_data.ecdsa.h_ctx), hash); EG(ret, err);
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
	dbg_nn_print("h	  final import as nn", &e);

	ret = nn_mod(&e, &e, q); EG(ret, err);
	dbg_nn_print("e", &e);

	/* Compute s^-1 mod q */
	ret = nn_modinv(&sinv, s, q); EG(ret, err);
	dbg_nn_print("s", s);
	dbg_nn_print("sinv", &sinv);

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
	MUST_HAVE(!iszero, ret, err);

	/* 9. Compute r' = W'_x mod q */
	ret = prj_pt_unique(W_prime, W_prime); EG(ret, err);
	dbg_nn_print("W'_x", &(W_prime->X.fp_val));
	dbg_nn_print("W'_y", &(W_prime->Y.fp_val));
	ret = nn_mod(&r_prime, &(W_prime->X.fp_val), q); EG(ret, err);

	/* 10. Accept the signature if and only if r equals r' */
	ret = nn_cmp(&r_prime, r, &cmp); EG(ret, err);
	ret = (cmp != 0) ? -1 : 0;

 err:
	prj_pt_uninit(&uG);
	prj_pt_uninit(&vY);
	nn_uninit(&e);
	nn_uninit(&sinv);
	nn_uninit(&uv);
	nn_uninit(&r_prime);

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

/* Public key recovery from a signature.
 * For ECDSA, it is possible to recover two possible public keys from
 * a signature and a digest.
 *
 * Please note that this recovery is not perfect as some information is
 * lost when reducing Rx modulo the order q during the signature. Hence,
 * a few possible R points can provide the same r. The following algorithm
 * assumes that Rx == r, i.e. Rx is < q and already reduced. This should
 * happen with a probability q / p, and "bad" cases with probability
 * (p - q) / p. Actually, some small multiples of r are also tested,
 * but we give up after 10 tries as this can be very time consuming.
 *
 * With usual curve parameters, this last probability is negligible if
 * everything is random (which should be the case for a "regular" signature
 * algorithm) for curves with cofactor = 1. However, an adversary could
 * willingly choose a Rx > q and the following algorithm will most certainly
 * fail.
 *
 * For curves with cofactor > 1, q is usually some orders of magnitudes
 * smaller than p and this function will certainly fail.
 *
 * Please use the resulting public keys with care and with all these
 * warnings in mind!
 *
 */
int __ecdsa_public_key_from_sig(ec_pub_key *out_pub1, ec_pub_key *out_pub2, const ec_params *params,
				const u8 *sig, u8 siglen, const u8 *hash, u8 hsize,
	                        ec_alg_type key_type)
{
	int ret, iszero1, iszero2, cmp1, cmp2;
	prj_pt uG;
	prj_pt_t Y1, Y2;
	prj_pt_src_t G;
	nn u, v, e, r, s;
	nn_src_t q, p;
	bitcnt_t rshift, q_bit_len;
	u8 q_len;
	word_t order_multiplier = WORD(1);

	uG.magic = WORD(0);
	u.magic = v.magic = e.magic = r.magic = s.magic = WORD(0);

	/* Zero init points */
	ret = local_memset(&uG, 0, sizeof(prj_pt)); EG(ret, err);

	/* Sanity checks */
	MUST_HAVE((params != NULL) && (sig != NULL) && (hash != NULL) && (out_pub1 != NULL) && (out_pub2 != NULL), ret, err);

	/* Import our params */
	G = &(params->ec_gen);
	p = &(params->ec_fp.p);
	q = &(params->ec_gen_order);
	q_bit_len = params->ec_gen_order_bitlen;
	q_len = (u8)BYTECEIL(q_bit_len);
	Y1 = &(out_pub1->y);
	Y2 = &(out_pub2->y);

	/* Check given signature length is the expected one */
	MUST_HAVE((siglen == ECDSA_SIGLEN(q_bit_len)), ret, err);

restart:
	/* Import r and s values from signature buffer */
	ret = nn_init_from_buf(&r, sig, q_len); EG(ret, err);
	ret = nn_init_from_buf(&s, sig + q_len, q_len); EG(ret, err);

	/* Reject the signature if r or s is 0. */
	ret = nn_iszero(&r, &iszero1); EG(ret, err);
	ret = nn_iszero(&s, &iszero2); EG(ret, err);
	ret = nn_cmp(&r, q, &cmp1); EG(ret, err);
	ret = nn_cmp(&s, q, &cmp2); EG(ret, err);
	MUST_HAVE(((!iszero1) && (cmp1 < 0) && !iszero2 && (cmp2 < 0)), ret, err);

	/* Add a multiple of the order to r using our current order multiplier */
	if(order_multiplier > 1){
		int cmp;
		ret = nn_init(&u, 0);
		ret = nn_mul_word(&u, q, order_multiplier); EG(ret, err);
		ret = nn_add(&r, &r, &u); EG(ret, err);
		/* If we have reached > p, leave with an error */
		ret = nn_cmp(&r, p, &cmp); EG(ret, err);
		/* NOTE: we do not use a MUST_HAVE macro here since
		 * this condition can nominally happen, and we do not want
		 * a MUST_HAVE in debug mode (i.e. with an assert) to break
		 * the execution flow.
		 */
		if(cmp < 0){
			ret = -1;
			goto err;
		}
	}

	/*
	 * Compute e.
	 * If |h| > bitlen(q), set h to bitlen(q)
	 * leftmost bits of h.
	 *
	 * Note that it's easier to check here if the truncation
	 * needs to be done but implement it using a logical
	 * shift.
	 */
	rshift = 0;
	if ((hsize * 8) > q_bit_len) {
		rshift = (bitcnt_t)((hsize * 8) - q_bit_len);
	}
	ret = nn_init_from_buf(&e, hash, hsize); EG(ret, err);
	if (rshift) {
		ret = nn_rshift_fixedlen(&e, &e, rshift); EG(ret, err);
	}
	ret = nn_mod(&e, &e, q); EG(ret, err);

	/* Now to find the y coordinate by solving the curve equation.
	 * NOTE: we use uG as temporary storage.
	 */
	ret = fp_init(&(uG.X), &(params->ec_fp)); EG(ret, err);
	ret = fp_init(&(uG.Y), &(params->ec_fp)); EG(ret, err);
	ret = fp_init(&(uG.Z), &(params->ec_fp)); EG(ret, err);
	ret = fp_set_nn(&(uG.Z), &r); EG(ret, err);
	ret = aff_pt_y_from_x(&(uG.X), &(uG.Y), &(uG.Z), &(params->ec_curve));
	if(ret){
		/* If we have failed here, this means that our r has certainly been
		 * reduced. Increment our multiplier and restart the process.
		 */
		order_multiplier = (word_t)(order_multiplier + 1);
		if(order_multiplier > 10){
			/* Too much tries, leave ... */
			ret = -1;
			goto err;
		}
		goto restart;
	}

	/* Initialize Y1 and Y2 */
	ret = fp_init(&(Y2->Z), &(params->ec_fp)); EG(ret, err);
	ret = fp_one(&(Y2->Z)); EG(ret, err);
	/* Y1 */
	ret = prj_pt_init_from_coords(Y1, &(params->ec_curve), &(uG.Z), &(uG.X), &(Y2->Z)); EG(ret, err);
	/* Y2 */
	ret = prj_pt_init_from_coords(Y2, &(params->ec_curve), &(uG.Z), &(uG.Y), &(Y1->Z)); EG(ret, err);

	/* Now compute u = (-e r^-1) mod q, and v = (s r^-1) mod q */
	ret = nn_init(&u, 0); EG(ret, err);
	ret = nn_init(&v, 0); EG(ret, err);
	ret = nn_modinv(&r, &r, q); EG(ret, err);
	/* u */
	ret = nn_mod_mul(&u, &e, &r, q); EG(ret, err);
	/* NOTE: -x mod q is (q - x) mod q, i.e. (q - x) when x is reduced, except for 0 */
	ret = nn_mod_neg(&u, &u, q); EG(ret, err);
	/* v */
	ret = nn_mod_mul(&v, &s, &r, q); EG(ret, err);

	/* Compute uG */
	ret = prj_pt_mul(&uG, &u, G); EG(ret, err);
	/* Compute vR1 and possible Y1 */
	ret = prj_pt_mul(Y1, &v, Y1); EG(ret, err);
	ret = prj_pt_add(Y1, Y1, &uG); EG(ret, err);
	/* Compute vR2 and possible Y2 */
	ret = prj_pt_mul(Y2, &v, Y2); EG(ret, err);
	ret = prj_pt_add(Y2, Y2, &uG); EG(ret, err);

	/* Now initialize our two public keys */
	/* out_pub1 */
	out_pub1->key_type = key_type;
	out_pub1->params = params;
	out_pub1->magic = PUB_KEY_MAGIC;
	/* out_pub2 */
	out_pub2->key_type = key_type;
	out_pub2->params = params;
	out_pub2->magic = PUB_KEY_MAGIC;

	ret = 0;

err:
	prj_pt_uninit(&uG);
	nn_uninit(&r);
	nn_uninit(&s);
	nn_uninit(&u);
	nn_uninit(&v);
	nn_uninit(&e);

	/* Clean what remains on the stack */
	PTR_NULLIFY(G);
	PTR_NULLIFY(Y1);
	PTR_NULLIFY(Y2);
	VAR_ZEROIFY(rshift);
	VAR_ZEROIFY(q_bit_len);
	PTR_NULLIFY(q);
	PTR_NULLIFY(p);

	return ret;
}

#else /* defined(WITH_SIG_ECDSA) || defined(WITH_SIG_DECDSA) */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_SIG_ECDSA */
