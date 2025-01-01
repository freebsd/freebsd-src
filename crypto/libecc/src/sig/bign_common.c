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
#if defined(WITH_SIG_BIGN) || defined(WITH_SIG_DBIGN)

#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul_public.h>
#include <libecc/nn/nn_logical.h>

#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/ec_key.h>
#include <libecc/utils/utils.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "BIGN"
#endif
#include <libecc/utils/dbg_sig.h>

/*
 * This is an implementation of the BIGN signature algorithm as
 * described in the STB 34.101.45 standard
 * (http://apmi.bsu.by/assets/files/std/bign-spec29.pdf).
 *
 * The BIGN signature is a variation on the Shnorr signature scheme.
 *
 * An english high-level (less formal) description and rationale can be found
 * in the IETF archive:
 *   https://mailarchive.ietf.org/arch/msg/cfrg/pI92HSRjMBg50NVEz32L5RciVBk/
 *
 * BIGN comes in two flavors: deterministic and non-deterministic. The current
 * file implements the two.
 *
 * In this implementation, we are *on purpose* more lax than the STB standard regarding
 * the so called "internal"/"external" hash function sizes and the order size:
 *   - We accept order sizes that might be different than twice the internal hash
 *   function (HASH-BELT truncated) and the size of the external hash function.
 *   - We accept security levels that might be different from {128, 192, 256}.
 *
 * If we strictly conform to STB 34.101.45, only orders of size exactly twice the
 * internal hash function length are accepted, and only external hash functions of size
 * of the order are accepted. Also only security levels of 128, 192 or 256 bits
 * are accepted.
 *
 * Being more lax on these parameters allows to be compatible with more hash
 * functions and curves.
 *
 * Finally, although the IETF archive in english leaves the "internal" hash functions
 * as configurable (wrt size constraints), the STB 34.101.45 standard fixes the BELT hash
 * function (standardized in STB 34.101.31) as the one to be used. The current file follows
 * this mandatory requirement and uses BELT as the only possible internal hash function
 * while the external one is configurable.
 *
 */

/* NOTE: BIGN uses per its standard the BELT-HASH hash function as its "internal"
 * hash function, as well as the BELT encryption block cipher during the deterministic
 * computation of the nonce for the deterministic version of BIGN.
 * Hence the sanity check below.
 */
#if !defined(WITH_HASH_BELT_HASH)
#error "BIGN and DBIGN need BELT-HASH, please activate it!"
#endif


/* Reverses the endiannes of a buffer in place */
ATTRIBUTE_WARN_UNUSED_RET static inline int _reverse_endianness(u8 *buf, u16 buf_size)
{
	u16 i;
	u8 tmp;
	int ret;

	MUST_HAVE((buf != NULL), ret, err);

	if(buf_size > 1){
		for(i = 0; i < (buf_size / 2); i++){
			tmp = buf[i];
			buf[i] = buf[buf_size - 1 - i];
			buf[buf_size - 1 - i] = tmp;
		}
	}

	ret = 0;
err:
	return ret;
}

/* The additional data for bign are specific. We provide
 * helpers to extract them from an adata pointer.
 */
int bign_get_oid_from_adata(const u8 *adata, u16 adata_len, const u8 **oid_ptr, u16 *oid_len)
{
	int ret;
	u16 t_len;

	MUST_HAVE((adata != NULL) && (oid_ptr != NULL) && (oid_len != NULL), ret, err);
	MUST_HAVE((adata_len >= 4), ret, err);

	(*oid_len) = (u16)(((u16)adata[0] << 8) | adata[1]);
	t_len = (u16)(((u16)adata[2] << 8) | adata[3]);
	/* Check overflow */
	MUST_HAVE(((*oid_len) + t_len) >= (t_len), ret, err);
	MUST_HAVE(((*oid_len) + t_len) <= (adata_len - 4), ret, err);
	(*oid_ptr) = &adata[4];

	ret = 0;
err:
	if(ret && (oid_ptr != NULL)){
		(*oid_ptr) = NULL;
	}
	if(ret && (oid_len != NULL)){
		(*oid_len) = 0;
	}
	return ret;
}

int bign_get_t_from_adata(const u8 *adata, u16 adata_len, const u8 **t_ptr, u16 *t_len)
{
	int ret;
	u16 oid_len;

	MUST_HAVE((adata != NULL) && (t_ptr != NULL) && (t_len != NULL), ret, err);
	MUST_HAVE((adata_len >= 4), ret, err);

	oid_len = (u16)(((u16)adata[0] << 8) | adata[1]);
	(*t_len) = (u16)(((u16)adata[2] << 8) | adata[3]);
	/* Check overflow */
	MUST_HAVE((oid_len + (*t_len)) >= (oid_len), ret, err);
	MUST_HAVE((oid_len + (*t_len)) <= (adata_len - 4), ret, err);
	(*t_ptr) = &adata[4 + oid_len];

	ret = 0;
err:
	if(ret && (t_ptr != NULL)){
		(*t_ptr) = NULL;
	}
	if(ret && (t_len != NULL)){
		(*t_len) = 0;
	}
	return ret;
}

int bign_set_adata(u8 *adata, u16 adata_len, const u8 *oid, u16 oid_len, const u8 *t, u16 t_len)
{
	int ret;

	MUST_HAVE((adata != NULL), ret, err);

	MUST_HAVE((oid != NULL) || (oid_len == 0), ret, err);
	MUST_HAVE((t != NULL) || (t_len == 0), ret, err);
	MUST_HAVE((adata_len >= 4), ret, err);
	/* Check overflow */
	MUST_HAVE(((oid_len + t_len) >= oid_len), ret, err);
	MUST_HAVE(((adata_len - 4) >= (oid_len + t_len)), ret, err);

	if(oid != NULL){
		adata[0] = (u8)(oid_len >> 8);
		adata[1] = (u8)(oid_len & 0xff);
		ret = local_memcpy(&adata[4], oid, oid_len); EG(ret, err);
	}
	else{
		adata[0] = adata[1] = 0;
	}
	if(t != NULL){
		adata[2] = (u8)(t_len >> 8);
		adata[3] = (u8)(t_len & 0xff);
		ret = local_memcpy(&adata[4 + oid_len], t, t_len); EG(ret, err);

	}
	else{
		adata[2] = adata[3] = 0;
	}

	ret = 0;
err:
	return ret;
}

#if defined(WITH_SIG_DBIGN)
/*
 * Deterministic nonce generation function for deterministic BIGN, as
 * described in STB 34.101.45 6.3.3.
 *
 * NOTE: Deterministic nonce generation for BIGN is useful against attackers
 * in contexts where only poor RNG/entropy are available, or when nonce bits
 * leaking can be possible through side-channel attacks.
 * However, in contexts where fault attacks are easy to mount, deterministic
 * BIGN can bring more security risks than regular BIGN.
 *
 * Depending on the context where you use the library, choose carefully if
 * you want to use the deterministic version or not.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET static int __bign_determinitic_nonce(nn_t k, nn_src_t q, bitcnt_t q_bit_len,
							       nn_src_t x, const u8 *adata, u16 adata_len,
							       const u8 *h, u8 hlen)
{
	int ret, cmp, iszero;
	u8 theta[BELT_HASH_DIGEST_SIZE];
	u8 FE2OS_D[LOCAL_MAX(BYTECEIL(CURVES_MAX_Q_BIT_LEN), 2 * BELT_HASH_DIGEST_SIZE)];
	u8 r[((MAX_DIGEST_SIZE / BELT_BLOCK_LEN) * BELT_BLOCK_LEN) + (2 * BELT_BLOCK_LEN)];
	u8 r_bar[((MAX_DIGEST_SIZE / BELT_BLOCK_LEN) * BELT_BLOCK_LEN) + (2 * BELT_BLOCK_LEN)];
	u8 q_len, l;
	unsigned int j, z, n;
	u32 i;
	u16 r_bar_len;

	belt_hash_context belt_hash_ctx;
	const u8 *oid_ptr = NULL;
	const u8 *t_ptr = NULL;
	u16 oid_len = 0, t_len = 0;

	MUST_HAVE((adata != NULL) && (h != NULL), ret, err);
	ret = nn_check_initialized(q); EG(ret, err);
	ret = nn_check_initialized(x); EG(ret, err);

	ret = local_memset(theta, 0, sizeof(theta)); EG(ret, err);
	ret = local_memset(FE2OS_D, 0, sizeof(FE2OS_D)); EG(ret, err);
	ret = local_memset(r_bar, 0, sizeof(r_bar)); EG(ret, err);

	q_len = (u8)BYTECEIL(q_bit_len);

	/* Compute l depending on the order */
	l = (u8)BIGN_S0_LEN(q_bit_len);

	/* Extract oid and t from the additional data */
	ret = bign_get_oid_from_adata(adata, adata_len, &oid_ptr, &oid_len); EG(ret, err);
	ret = bign_get_t_from_adata(adata, adata_len, &t_ptr, &t_len); EG(ret, err);

	ret = belt_hash_init(&belt_hash_ctx); EG(ret, err);
	ret = belt_hash_update(&belt_hash_ctx, oid_ptr, oid_len); EG(ret, err);

	/* Put the private key in a string <d>2*l */
	ret = local_memset(FE2OS_D, 0, sizeof(FE2OS_D)); EG(ret, err);
	ret = nn_export_to_buf(&FE2OS_D[0], q_len, x); EG(ret, err);
	ret = _reverse_endianness(&FE2OS_D[0], q_len); EG(ret, err);
	/* Only hash the 2*l bytes of d */
	ret = belt_hash_update(&belt_hash_ctx, &FE2OS_D[0], (u32)(2*l)); EG(ret, err);

	ret = belt_hash_update(&belt_hash_ctx, t_ptr, t_len); EG(ret, err);

	ret = belt_hash_final(&belt_hash_ctx, theta); EG(ret, err);

	dbg_buf_print("theta", theta, BELT_HASH_DIGEST_SIZE);

	/* n is the number of 128 bits blocks in H */
	n = (hlen / BELT_BLOCK_LEN);

	MUST_HAVE((hlen <= sizeof(r)), ret, err);
	ret = local_memset(r, 0, sizeof(r));
	ret = local_memcpy(r, h, hlen); EG(ret, err);
	/* If we have less than two blocks for the input hash size, we use zero
	 * padding to achieve at least two blocks.
	 * NOTE: this is not in the standard but allows to be compatible with small
	 * size hash functions.
	 */
	if(n <= 1){
		n = 2;
	}

	/* Now iterate until the nonce is computed in [1, q-1]
	 * NOTE: we are ensured here that n >= 2, which allows us to
	 * index (n-1) and (n-2) blocks in r.
	 */
	i = (u32)1;

	while(1){
		u8 s[BELT_BLOCK_LEN];
		u8 i_block[BELT_BLOCK_LEN];
		ret = local_memset(s, 0, sizeof(s)); EG(ret, err);

		/* Put the xor of all n-1 elements in s */
		for(j = 0; j < (n - 1); j++){
			for(z = 0; z < BELT_BLOCK_LEN; z++){
				s[z] ^= r[(BELT_BLOCK_LEN * j) + z];
			}
		}
		/* Move elements left for the first n-2 elements */
		ret = local_memcpy(&r[0], &r[BELT_BLOCK_LEN], (n - 2) * BELT_BLOCK_LEN); EG(ret, err);

		/* r_n-1 = belt-block(s, theta) ^ r_n ^ <i>128 */
		ret = local_memset(i_block, 0, sizeof(i_block)); EG(ret, err);
		PUT_UINT32_LE(i, i_block, 0);
		belt_encrypt(s, &r[(n - 2) * BELT_BLOCK_LEN], theta);
		for(z = 0; z < BELT_BLOCK_LEN; z++){
			r[((n - 2) * BELT_BLOCK_LEN) + z] ^= (r[((n - 1) * BELT_BLOCK_LEN) + z] ^ i_block[z]);
		}

		/* r_n = s */
		ret = local_memcpy(&r[(n - 1) * BELT_BLOCK_LEN], s, BELT_BLOCK_LEN); EG(ret, err);

		/* Import r_bar as a big number in little endian
		 * (truncate our import to the bitlength size of q)
		 */
		if(q_len < (n * BELT_BLOCK_LEN)){
			r_bar_len = q_len;
			ret = local_memcpy(&r_bar[0], &r[0], r_bar_len); EG(ret, err);
			/* Handle the useless bits between q_bit_len and (8 * q_len) */
			if((q_bit_len % 8) != 0){
				r_bar[r_bar_len - 1] &= (u8)((0x1 << (q_bit_len % 8)) - 1);
			}
		}
		else{
			/* In this case, q_len is bigger than the size of r, we need to adapt:
			 * we truncate to the size of r.
			 * NOTE: we of course lose security, but this is the explicit choice
			 * of the user using a "small" hash function with a "big" order.
			 */
			MUST_HAVE((n * BELT_BLOCK_LEN) <= 0xffff, ret, err);
			r_bar_len = (u16)(n * BELT_BLOCK_LEN);
			ret = local_memcpy(&r_bar[0], &r[0], r_bar_len); EG(ret, err);
		}
		ret = _reverse_endianness(&r_bar[0], r_bar_len); EG(ret, err);
		ret = nn_init_from_buf(k, &r_bar[0], r_bar_len); EG(ret, err);

		/* Compare it to q */
		ret = nn_cmp(k, q, &cmp); EG(ret, err);
		/* Compare it to 0 */
		ret = nn_iszero(k, &iszero); EG(ret, err);

		if((i >= (2 * n)) && (cmp < 0) && (!iszero)){
			break;
		}
		i += (u32)1;
		/* If we have wrapped (meaning i > 2^32), we exit with failure */
		MUST_HAVE((i != 0), ret, err);
	}

	ret = 0;
err:
	/* Destroy local variables potentially containing sensitive data */
	IGNORE_RET_VAL(local_memset(theta, 0, sizeof(theta)));
	IGNORE_RET_VAL(local_memset(FE2OS_D, 0, sizeof(FE2OS_D)));

	return ret;
}
#endif

int __bign_init_pub_key(ec_pub_key *out_pub, const ec_priv_key *in_priv,
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

int __bign_siglen(u16 p_bit_len, u16 q_bit_len, u8 hsize, u8 blocksize, u8 *siglen)
{
	int ret;

	MUST_HAVE(siglen != NULL, ret, err);
	MUST_HAVE((p_bit_len <= CURVES_MAX_P_BIT_LEN) &&
		  (q_bit_len <= CURVES_MAX_Q_BIT_LEN) &&
		  (hsize <= MAX_DIGEST_SIZE) && (blocksize <= MAX_BLOCK_SIZE), ret, err);
	(*siglen) = (u8)BIGN_SIGLEN(q_bit_len);
	ret = 0;

err:
	return ret;
}

/*
 * Generic *internal* BIGN signature functions (init, update and finalize).
 * Their purpose is to allow passing a specific hash function (along with
 * its output size) and the random ephemeral key k, so that compliance
 * tests against test vectors can be made without ugly hack in the code
 * itself.
 *
 * Implementation notes:
 *
 * a) The BIGN algorithm makes use of the OID of the external hash function.
 *    We let the upper layer provide us with this in the "adata" field of the
 *    context.
 *
 */

#define BIGN_SIGN_MAGIC ((word_t)(0x63439a2b38921340ULL))
#define BIGN_SIGN_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == BIGN_SIGN_MAGIC), ret, err)

int __bign_sign_init(struct ec_sign_context *ctx, ec_alg_type key_type)
{
	int ret;

	/* First, verify context has been initialized */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);

	/* Additional sanity checks on input params from context */
	ret = key_pair_check_initialized_and_type(ctx->key_pair, key_type); EG(ret, err);

	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		  (ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);

	/* We check that our additional data is not NULL as it must contain
	 * the mandatory external hash OID.
	 */
	MUST_HAVE((ctx->adata != NULL) && (ctx->adata_len != 0), ret, err);

	/*
	 * Initialize hash context stored in our private part of context
	 * and record data init has been done
	 */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->sign_data.bign.h_ctx)); EG(ret, err);

	ctx->sign_data.bign.magic = BIGN_SIGN_MAGIC;

err:
	return ret;
}

int __bign_sign_update(struct ec_sign_context *ctx,
		       const u8 *chunk, u32 chunklen, ec_alg_type key_type)
{
	int ret;

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an BIGN
	 * signature one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	BIGN_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.bign), ret, err);

	/* Additional sanity checks on input params from context */
	ret = key_pair_check_initialized_and_type(ctx->key_pair, key_type); EG(ret, err);

	/* 1. Compute h = H(m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->sign_data.bign.h_ctx), chunk, chunklen);

err:
	return ret;
}

int __bign_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen,
			  ec_alg_type key_type)
{
	int ret, cmp;
	const ec_priv_key *priv_key;
	prj_pt_src_t G;
	u8 hash[MAX_DIGEST_SIZE];
	u8 hash_belt[BELT_HASH_DIGEST_SIZE];
	u8 FE2OS_W[LOCAL_MAX(2 * BYTECEIL(CURVES_MAX_P_BIT_LEN), 2 * BIGN_S0_LEN(CURVES_MAX_Q_BIT_LEN))];
	bitcnt_t q_bit_len, p_bit_len;
	prj_pt kG;
	nn_src_t q, x;
	u8 hsize, p_len, l;
	nn k, h, tmp, s1;
	belt_hash_context belt_hash_ctx;
	const u8 *oid_ptr = NULL;
	u16 oid_len = 0;
#ifdef USE_SIG_BLINDING
	/* b is the blinding mask */
	nn b, binv;
	b.magic = binv.magic = WORD(0);
#endif

	k.magic = h.magic = WORD(0);
	tmp.magic = s1.magic = WORD(0);
	kG.magic = WORD(0);

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an BIGN
	 * signature one and we do not finalize() before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	BIGN_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.bign), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* Additional sanity checks on input params from context */
	ret = key_pair_check_initialized_and_type(ctx->key_pair, key_type); EG(ret, err);

	/* Zero init out point */
	ret = local_memset(&kG, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	priv_key = &(ctx->key_pair->priv_key);
	q = &(priv_key->params->ec_gen_order);
	q_bit_len = priv_key->params->ec_gen_order_bitlen;
	p_bit_len = priv_key->params->ec_fp.p_bitlen;
	G = &(priv_key->params->ec_gen);
	p_len = (u8)BYTECEIL(p_bit_len);
	x = &(priv_key->x);
	hsize = ctx->h->digest_size;

	MUST_HAVE((priv_key->key_type == key_type), ret, err);

	/* Compute l depending on the order */
	l = (u8)BIGN_S0_LEN(q_bit_len);

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
	MUST_HAVE((siglen == BIGN_SIGLEN(q_bit_len)), ret, err);

	/* We check that our additional data is not NULL as it must contain
	 * the mandatory external hash OID.
	 */
	MUST_HAVE((ctx->adata != NULL) && (ctx->adata_len != 0), ret, err);

	/* 1. Compute h = H(m) */
	ret = local_memset(hash, 0, hsize); EG(ret, err);
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&(ctx->sign_data.bign.h_ctx), hash); EG(ret, err);
	dbg_buf_print("h", hash, hsize);


	/* 2. get a random value k in ]0,q[ */
#ifdef NO_KNOWN_VECTORS
	/* NOTE: when we do not need self tests for known vectors,
	 * we can be strict about random function handler!
	 * This allows us to avoid the corruption of such a pointer.
	 */
	/* Sanity check on the handler before calling it */
	if(ctx->rand != nn_get_random_mod){
#ifdef WITH_SIG_DBIGN
		/* In deterministic BIGN, nevermind! */
		if(key_type != DBIGN)
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
#if defined(WITH_SIG_DBIGN)
	{
		/* Only applies for DETERMINISTIC BIGN */
		if(key_type != DBIGN){
			ret = -1;
			goto err;
		}
		/* Deterministically generate k as STB 34.101.45 mandates */
		ret = __bign_determinitic_nonce(&k, q, q_bit_len, &(priv_key->x), ctx->adata, ctx->adata_len,  hash, hsize);
	}
#else
	{
		/* NULL rand function is not accepted for regular BIGN */
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
	/* NOTE: we use Fermat's little theorem inversion for
	 * constant time here. This is possible since q is prime.
	 */
	ret = nn_modinv_fermat(&binv, &b, q); EG(ret, err);

	dbg_nn_print("b", &b);
#endif /* USE_SIG_BLINDING */


	/* 3. Compute W = (W_x,W_y) = kG */
#ifdef USE_SIG_BLINDING
	ret = prj_pt_mul_blind(&kG, &k, G); EG(ret, err);
#else
	ret = prj_pt_mul(&kG, &k, G); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	ret = prj_pt_unique(&kG, &kG); EG(ret, err);

	dbg_nn_print("W_x", &(kG.X.fp_val));
	dbg_nn_print("W_y", &(kG.Y.fp_val));

	/* 4. Compute s0 = <BELT-HASH(OID(H) || <<FE2OS(W_x)> || <FE2OS(W_y)>>2*l || H(X))>l */
	ret = belt_hash_init(&belt_hash_ctx); EG(ret, err);
	ret = bign_get_oid_from_adata(ctx->adata, ctx->adata_len, &oid_ptr, &oid_len); EG(ret, err);
	ret = belt_hash_update(&belt_hash_ctx, oid_ptr, oid_len); EG(ret, err);
	/**/
	ret = local_memset(FE2OS_W, 0, sizeof(FE2OS_W)); EG(ret, err);
	ret = fp_export_to_buf(&FE2OS_W[0],  p_len, &(kG.X)); EG(ret, err);
	ret = _reverse_endianness(&FE2OS_W[0],  p_len); EG(ret, err);
	ret = fp_export_to_buf(&FE2OS_W[p_len], p_len, &(kG.Y)); EG(ret, err);
	ret = _reverse_endianness(&FE2OS_W[p_len], p_len); EG(ret, err);
	/* Only hash the 2*l bytes of FE2OS(W_x) || FE2OS(W_y) */
	ret = belt_hash_update(&belt_hash_ctx, &FE2OS_W[0], (u32)(2*l)); EG(ret, err);
	/**/
	ret = belt_hash_update(&belt_hash_ctx, hash, hsize); EG(ret, err);
	/* Store our s0 */
	ret = local_memset(hash_belt, 0, sizeof(hash_belt)); EG(ret, err);
	ret = belt_hash_final(&belt_hash_ctx, hash_belt); EG(ret, err);
	ret = local_memset(&sig[0], 0, l); EG(ret, err);
	ret = local_memcpy(&sig[0], &hash_belt[0], LOCAL_MIN(l, BELT_HASH_DIGEST_SIZE)); EG(ret, err);
	dbg_buf_print("s0", &sig[0], LOCAL_MIN(l, BELT_HASH_DIGEST_SIZE));

	/* 5. Now compute s1 = (k - H_bar - (s0_bar + 2**l) * d) mod q */
	/* First import H and s0 as numbers modulo q */
	/* Import H */
	ret = _reverse_endianness(hash, hsize); EG(ret, err);
	ret = nn_init_from_buf(&h, hash, hsize); EG(ret, err);
	ret = nn_mod(&h, &h, q); EG(ret, err);
	/* Import s0_bar */
	ret = local_memcpy(FE2OS_W, &sig[0], l); EG(ret, err);
	ret = _reverse_endianness(FE2OS_W, l); EG(ret, err);
	ret = nn_init_from_buf(&s1, FE2OS_W, l); EG(ret, err);
	ret = nn_mod(&s1, &s1, q); EG(ret, err);
	/* Compute (s0_bar + 2**l) * d */
	ret = nn_init(&tmp, 0); EG(ret, err);
	ret = nn_one(&tmp); EG(ret, err);
	ret = nn_lshift(&tmp, &tmp, (bitcnt_t)(8*l)); EG(ret, err);
	ret = nn_mod(&tmp, &tmp, q); EG(ret, err);
	ret = nn_mod_add(&s1, &s1, &tmp, q); EG(ret, err);
#ifdef USE_SIG_BLINDING
	/* Blind s1 with b */
	ret = nn_mod_mul(&s1, &s1, &b, q); EG(ret, err);

	/* Blind the message hash */
	ret = nn_mod_mul(&h, &h, &b, q); EG(ret, err);

	/* Blind the nonce */
	ret = nn_mod_mul(&k, &k, &b, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */

	ret = nn_mod_mul(&s1, &s1, &(priv_key->x), q); EG(ret, err);
	ret = nn_mod_sub(&s1, &k, &s1, q); EG(ret, err);
	ret = nn_mod_sub(&s1, &s1, &h, q); EG(ret, err);

#ifdef USE_SIG_BLINDING
	/* Unblind s1 */
	ret = nn_mod_mul(&s1, &s1, &binv, q); EG(ret, err);
#endif
	dbg_nn_print("s1", &s1);

	/* Clean hash buffer as we do not need it anymore */
	ret = local_memset(hash, 0, hsize); EG(ret, err);

	/* Now export s1 and reverse its endianness */
	ret = nn_export_to_buf(&sig[l], (u16)BIGN_S1_LEN(q_bit_len), &s1); EG(ret, err);
	ret = _reverse_endianness(&sig[l], (u16)BIGN_S1_LEN(q_bit_len));

err:
	nn_uninit(&k);
	nn_uninit(&h);
	nn_uninit(&tmp);
	nn_uninit(&s1);
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
		IGNORE_RET_VAL(local_memset(&(ctx->sign_data.bign), 0, sizeof(bign_sign_data)));
	}

	/* Clean what remains on the stack */
	PTR_NULLIFY(priv_key);
	PTR_NULLIFY(G);
	PTR_NULLIFY(q);
	PTR_NULLIFY(x);
	PTR_NULLIFY(oid_ptr);
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(hsize);
	VAR_ZEROIFY(oid_len);

	return ret;
}

/*
 * Generic *internal* BIGN verification functions (init, update and finalize).
 * Their purpose is to allow passing a specific hash function (along with
 * its output size) and the random ephemeral key k, so that compliance
 * tests against test vectors can be made without ugly hack in the code
 * itself.
 *
 * Implementation notes:
 *
 * a) The BIGN algorithm makes use of the OID of the external hash function.
 *    We let the upper layer provide us with this in the "adata" field of the
 *    context.
 */

#define BIGN_VERIFY_MAGIC ((word_t)(0xceff8344927346abULL))
#define BIGN_VERIFY_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((void *)(A)) != NULL) && ((A)->magic == BIGN_VERIFY_MAGIC), ret, err)

int __bign_verify_init(struct ec_verify_context *ctx, const u8 *sig, u8 siglen,
			ec_alg_type key_type)
{
	bitcnt_t q_bit_len;
	nn_src_t q;
	nn *s0, *s1;
	u8 *s0_sig;
	u8 TMP[BYTECEIL(CURVES_MAX_Q_BIT_LEN)];
	u8 l;
	int ret, cmp;

	/* First, verify context has been initialized */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);

	ret = local_memset(TMP, 0, sizeof(TMP)); EG(ret, err);

	/* Do some sanity checks on input params */
	ret = pub_key_check_initialized_and_type(ctx->pub_key, key_type); EG(ret, err);
	MUST_HAVE((ctx->h != NULL) && (ctx->h->digest_size <= MAX_DIGEST_SIZE) &&
		(ctx->h->block_size <= MAX_BLOCK_SIZE), ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	/* We check that our additional data is not NULL as it must contain
	 * the mandatory external hash OID.
	 */
	MUST_HAVE((ctx->adata != NULL) && (ctx->adata_len != 0), ret, err);

	/* Make things more readable */
	q = &(ctx->pub_key->params->ec_gen_order);
	q_bit_len = ctx->pub_key->params->ec_gen_order_bitlen;
	s0 = &(ctx->verify_data.bign.s0);
	s1 = &(ctx->verify_data.bign.s1);
	s0_sig = (u8*)(&(ctx->verify_data.bign.s0_sig));

	/* Compute l depending on the order */
	l = (u8)BIGN_S0_LEN(q_bit_len);

	/* Check given signature length is the expected one */
	MUST_HAVE((siglen == BIGN_SIGLEN(q_bit_len)), ret, err);

	/* Copy s0 to be checked later */
	ret = local_memcpy(s0_sig, sig, l); EG(ret, err);

	/* Import s0 and s1 values from signature buffer */
	ret = local_memcpy(&TMP[0], sig, l); EG(ret, err);
	ret = _reverse_endianness(&TMP[0], l); EG(ret, err);
	ret = nn_init_from_buf(s0, &TMP[0], l); EG(ret, err);
	/**/
	ret = local_memcpy(&TMP[0], &sig[l], (u32)BIGN_S1_LEN(q_bit_len)); EG(ret, err);
	ret = _reverse_endianness(&TMP[0], (u16)BIGN_S1_LEN(q_bit_len)); EG(ret, err);
	ret = nn_init_from_buf(s1, &TMP[0], (u8)BIGN_S1_LEN(q_bit_len)); EG(ret, err);
	dbg_nn_print("s0", s0);
	dbg_nn_print("s1", s1);

	/* 1. Reject the signature if s1 >= q */
	ret = nn_cmp(s1, q, &cmp); EG(ret, err);
	MUST_HAVE((cmp < 0), ret, err);

	/* Initialize the remaining of verify context. */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_init(&(ctx->verify_data.bign.h_ctx)); EG(ret, err);

	ctx->verify_data.bign.magic = BIGN_VERIFY_MAGIC;

 err:
	VAR_ZEROIFY(q_bit_len);
	PTR_NULLIFY(q);
	PTR_NULLIFY(s0);
	PTR_NULLIFY(s1);
	PTR_NULLIFY(s0_sig);

	return ret;
}

int __bign_verify_update(struct ec_verify_context *ctx,
			 const u8 *chunk, u32 chunklen, ec_alg_type key_type)
{
	int ret;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an BIGN
	 * verification one and we do not update() or finalize()
	 * before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	BIGN_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.bign), ret, err);
	/* Do some sanity checks on input params */
	ret = pub_key_check_initialized_and_type(ctx->pub_key, key_type); EG(ret, err);

	/* 2. Compute h = H(m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_update(&(ctx->verify_data.bign.h_ctx), chunk, chunklen);

err:
	return ret;
}

int __bign_verify_finalize(struct ec_verify_context *ctx,
			    ec_alg_type key_type)
{
	prj_pt uG, vY;
	prj_pt_src_t G, Y;
	prj_pt_t W;
	u8 hash[MAX_DIGEST_SIZE];
	u8 hash_belt[BELT_HASH_DIGEST_SIZE];
	u8 t[BIGN_S0_LEN(CURVES_MAX_Q_BIT_LEN)];
	u8 FE2OS_W[LOCAL_MAX(2 * BYTECEIL(CURVES_MAX_P_BIT_LEN), 2 * BIGN_S0_LEN(CURVES_MAX_Q_BIT_LEN))];
	bitcnt_t p_bit_len, q_bit_len;
	nn_src_t q;
	nn h, tmp;
	nn *s0, *s1;
	u8 *s0_sig;
	u8 hsize, p_len, l;
	belt_hash_context belt_hash_ctx;
	int ret, iszero, cmp;
	const u8 *oid_ptr = NULL;
	u16 oid_len = 0;

	h.magic = tmp.magic = WORD(0);
	uG.magic = vY.magic = WORD(0);

	/* NOTE: we reuse uG for W to optimize local variables */
	W = &uG;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an BIGN
	 * verification one and we do not finalize() before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	BIGN_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.bign), ret, err);
	/* Do some sanity checks on input params */
	ret = pub_key_check_initialized_and_type(ctx->pub_key, key_type); EG(ret, err);

	/* We check that our additional data is not NULL as it must contain
	 * the mandatory external hash OID.
	 */
	MUST_HAVE((ctx->adata != NULL) && (ctx->adata_len != 0), ret, err);

	/* Zero init points */
	ret = local_memset(&uG, 0, sizeof(prj_pt)); EG(ret, err);
	ret = local_memset(&vY, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	G = &(ctx->pub_key->params->ec_gen);
	Y = &(ctx->pub_key->y);
	q = &(ctx->pub_key->params->ec_gen_order);
	p_bit_len = ctx->pub_key->params->ec_fp.p_bitlen;
	q_bit_len = ctx->pub_key->params->ec_gen_order_bitlen;
	p_len = (u8)BYTECEIL(p_bit_len);
	hsize = ctx->h->digest_size;
	s0 = &(ctx->verify_data.bign.s0);
	s1 = &(ctx->verify_data.bign.s1);
	s0_sig = (u8*)(&(ctx->verify_data.bign.s0_sig));

	/* Sanity check */
	MUST_HAVE((sizeof(t) == sizeof(ctx->verify_data.bign.s0_sig)), ret, err);

	/* Compute our l that is inherited from q size */
	l = (u8)BIGN_S0_LEN(q_bit_len);

	/* 2. Compute h = H(m) */
	/* Since we call a callback, sanity check our mapping */
	ret = hash_mapping_callbacks_sanity_check(ctx->h); EG(ret, err);
	ret = ctx->h->hfunc_finalize(&(ctx->verify_data.bign.h_ctx), hash); EG(ret, err);
	dbg_buf_print("h = H(m)", hash, hsize);

	/* Import H */
	ret = _reverse_endianness(hash, hsize); EG(ret, err);
	ret = nn_init_from_buf(&h, hash, hsize); EG(ret, err);
	ret = nn_mod(&h, &h, q); EG(ret, err);
	/* NOTE: we reverse endianness again of the hash since we will
	 * have to use the original value.
	 */
	ret = _reverse_endianness(hash, hsize); EG(ret, err);

	/* Compute ((s1_bar + h_bar) mod q) */
	ret = nn_mod_add(&h, &h, s1, q); EG(ret, err);
	/* Compute (s0_bar + 2**l) mod q */
	ret = nn_init(&tmp, 0); EG(ret, err);
	ret = nn_one(&tmp); EG(ret, err);
	ret = nn_lshift(&tmp, &tmp, (bitcnt_t)(8*l)); EG(ret, err);
	ret = nn_mod(&tmp, &tmp, q); EG(ret, err);
	ret = nn_mod_add(&tmp, &tmp, s0, q); EG(ret, err);

	/* 3. Compute ((s1_bar + h_bar) mod q) * G + ((s0_bar + 2**l) mod q) * Y. */
	ret = prj_pt_mul(&uG, &h, G); EG(ret, err);
	ret = prj_pt_mul(&vY, &tmp, Y); EG(ret, err);
	ret = prj_pt_add(W, &uG, &vY); EG(ret, err);
	/* 5. If the result is point at infinity, return false. */
	ret = prj_pt_iszero(W, &iszero); EG(ret, err);
	MUST_HAVE((!iszero), ret, err);
	ret = prj_pt_unique(W, W); EG(ret, err);

	/* 6. Compute t = <BELT-HASH(OID(H) || <<FE2OS(W_x)> || <FE2OS(W_y)>>2*l || H(X))>l */
	ret = belt_hash_init(&belt_hash_ctx); EG(ret, err);
	ret = bign_get_oid_from_adata(ctx->adata, ctx->adata_len, &oid_ptr, &oid_len); EG(ret, err);
	ret = belt_hash_update(&belt_hash_ctx, oid_ptr, oid_len); EG(ret, err);
	/**/
	ret = local_memset(FE2OS_W, 0, sizeof(FE2OS_W)); EG(ret, err);
	ret = fp_export_to_buf(&FE2OS_W[0], p_len, &(W->X)); EG(ret, err);
	ret = _reverse_endianness(&FE2OS_W[0], p_len); EG(ret, err);
	ret = fp_export_to_buf(&FE2OS_W[p_len], p_len, &(W->Y)); EG(ret, err);
	ret = _reverse_endianness(&FE2OS_W[p_len], p_len); EG(ret, err);
	/* Only hash the 2*l bytes of FE2OS(W_x) || FE2OS(W_y) */
	ret = belt_hash_update(&belt_hash_ctx, &FE2OS_W[0], (u32)(2*l)); EG(ret, err);
	/**/
	ret = belt_hash_update(&belt_hash_ctx, hash, hsize); EG(ret, err);
	/* Store our t */
	ret = local_memset(hash_belt, 0, sizeof(hash_belt)); EG(ret, err);
	ret = belt_hash_final(&belt_hash_ctx, hash_belt); EG(ret, err);
	ret = local_memset(&t[0], 0, l); EG(ret, err);
	ret = local_memcpy(&t[0], &hash_belt[0], LOCAL_MIN(l, BELT_HASH_DIGEST_SIZE)); EG(ret, err);

	/* 10. Accept the signature if and only if t equals s0_sig' */
	ret = are_equal(t, s0_sig, l, &cmp); EG(ret, err);
	ret = (cmp == 0) ? -1 : 0;

 err:
	prj_pt_uninit(&uG);
	prj_pt_uninit(&vY);
	nn_uninit(&h);
	nn_uninit(&tmp);

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->verify_data.bign), 0, sizeof(bign_verify_data)));
	}

	/* Clean what remains on the stack */
	PTR_NULLIFY(G);
	PTR_NULLIFY(Y);
	PTR_NULLIFY(W);
	VAR_ZEROIFY(p_bit_len);
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(p_len);
	PTR_NULLIFY(q);
	PTR_NULLIFY(s0);
	PTR_NULLIFY(s1);
	PTR_NULLIFY(s0_sig);
	PTR_NULLIFY(oid_ptr);
	VAR_ZEROIFY(hsize);
	VAR_ZEROIFY(oid_len);

	return ret;
}

#else /* defined(WITH_SIG_BIGN) || defined(WITH_SIG_DBIGN) */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_SIG_BIGN */
