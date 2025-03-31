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
#include <libecc/sig/sig_algs.h>

/*
 * Generic private key generation (generate a scalar in ]0,q[
 * Common accross many schemes, but might diverge for some.
 */
int generic_gen_priv_key(ec_priv_key *priv_key)
{
	nn_src_t q;
	int ret;

	ret = priv_key_check_initialized(priv_key); EG(ret, err);

	q = &(priv_key->params->ec_gen_order);

	/* Get a random value in ]0,q[ where q is the group generator order */
	ret = nn_get_random_mod(&(priv_key->x), q);

err:
	return ret;
}

/* Private key generation function per signature scheme */
int gen_priv_key(ec_priv_key *priv_key)
{
	const ec_sig_mapping *sm;
	int ret;
	u8 i;

	ret = priv_key_check_initialized(priv_key); EG(ret, err);

	ret = -1;
	for (i = 0, sm = &ec_sig_maps[i];
	     sm->type != UNKNOWN_ALG; sm = &ec_sig_maps[++i]) {
		if (sm->type == priv_key->key_type) {
			/* NOTE: since sm is initalized with a structure
			 * coming from a const source, we can safely call
			 * the callback here, but better safe than sorry.
			 */
			MUST_HAVE((sm->gen_priv_key != NULL), ret, err);
			ret = sm->gen_priv_key(priv_key);
			break;
		}
	}

err:
	return ret;
}

/*
 * Generic function to init a uninitialized public key from an initialized
 * private key. The function uses the expected logic to derive the key
 * (e.g. Y=xG, Y=(x^-1)G, etc). It returns -1 on error (i.e. if the signature
 * alg is unknown) in which case the public key has not been initialized.
 * It returns 0 on success.
 */
int init_pubkey_from_privkey(ec_pub_key *pub_key, ec_priv_key *priv_key)
{
	const ec_sig_mapping *sm;
	int ret;
	u8 i;

	ret = priv_key_check_initialized(priv_key); EG(ret, err);

	ret = -1;
	for (i = 0, sm = &ec_sig_maps[i];
	     sm->type != UNKNOWN_ALG; sm = &ec_sig_maps[++i]) {
		if (sm->type == priv_key->key_type) {
			/* NOTE: since sm is initalized with a structure
			 * coming from a const source, we can safely call
			 * the callback here, but better safe than sorry.
			 */
			MUST_HAVE((sm->init_pub_key != NULL), ret, err);
			ret = sm->init_pub_key(pub_key, priv_key);
			break;
		}
	}

err:
	return ret;
}

/*
 * On success, 0 is returned and out parameter 'sig_mapping' provides a
 * pointer to the ec_sig_mapping matching given input parameter
 * 'sig_name' (a null-terminated string, e.g. "ECDSA"). -1 is returned on error
 * in which case 'sig_mapping' is not meaningful.
 */
int get_sig_by_name(const char *ec_sig_name, const ec_sig_mapping **sig_mapping)
{
	const ec_sig_mapping *sm;
	int ret, check;
	u8 i;

	MUST_HAVE((ec_sig_name != NULL), ret, err);
	MUST_HAVE((sig_mapping != NULL), ret, err);

	ret = -1;
	for (i = 0, sm = &ec_sig_maps[i];
	     sm->type != UNKNOWN_ALG; sm = &ec_sig_maps[++i]) {
		if((!are_str_equal(ec_sig_name, sm->name, &check)) && check){
			(*sig_mapping) = sm;
			ret = 0;
			break;
		}
	}

err:
	return ret;
}

/*
 * On success, 0 is returned and out parameter 'sig_mapping' provides a
 * pointer to the ec_sig_mapping matching given input parameter
 * 'sig_type' (e.g. ECDSA, ECSDA). -1 is returned on error in which
 * case 'sig_mapping' is not meaningful.
 */
int get_sig_by_type(ec_alg_type sig_type, const ec_sig_mapping **sig_mapping)
{
	const ec_sig_mapping *sm;
	int ret;
	u8 i;

	MUST_HAVE((sig_mapping != NULL), ret, err);

	ret = -1;
	for (i = 0, sm = &ec_sig_maps[i];
	     sm->type != UNKNOWN_ALG; sm = &ec_sig_maps[++i]) {
		if (sm->type == sig_type) {
			(*sig_mapping) = sm;
			ret = 0;
			break;
		}
	}

err:
	return ret;
}

/*
 * Here, we provide a helper that sanity checks the provided signature
 * mapping against the constant ones. 0 is returned on success, -1 on
 * error.
 */
int ec_sig_mapping_callbacks_sanity_check(const ec_sig_mapping *sig)
{
	const ec_sig_mapping *sm;
	int ret = -1, check;
	u8 i;

	MUST_HAVE((sig != NULL), ret, err);

	/* We just check is our mapping is indeed
	 * one of the registered mappings.
	 */
	for (i = 0, sm = &ec_sig_maps[i];
	     sm->type != UNKNOWN_ALG; sm = &ec_sig_maps[++i]) {
		if (sm->type == sig->type){
			if ((!are_str_equal_nlen(sm->name, sig->name, MAX_SIG_ALG_NAME_LEN, &check)) && (!check)){
				goto err;
			} else if (sm->siglen != sig->siglen){
				goto err;
			} else if (sm->gen_priv_key != sig->gen_priv_key){
				goto err;
			} else if (sm->init_pub_key != sig->init_pub_key){
				goto err;
			} else if (sm->sign_init != sig->sign_init){
				goto err;
			} else if (sm->sign_update != sig->sign_update){
				goto err;
			} else if (sm->sign_finalize != sig->sign_finalize){
				goto err;
			} else if (sm->sign != sig->sign){
				goto err;
			} else if (sm->verify_init != sig->verify_init){
				goto err;
			} else if (sm->verify_update != sig->verify_update){
				goto err;
			} else if (sm->verify_finalize != sig->verify_finalize){
				goto err;
			} else if (sm->verify != sig->verify){
				goto err;
			} else{
				ret = 0;
			}
		}
	}

err:
	return ret;
}

/*
 * Sanity checks of a signature context to see if everything seems OK. 0 is
 * returned on cucces, -1 on error.
 */
int ec_sig_ctx_callbacks_sanity_check(const struct ec_sign_context *sig_ctx)
{
	int ret;

	MUST_HAVE((sig_ctx != NULL) && (sig_ctx->ctx_magic == SIG_SIGN_MAGIC), ret, err);

	ret = hash_mapping_callbacks_sanity_check(sig_ctx->h); EG(ret, err);
	ret = ec_sig_mapping_callbacks_sanity_check(sig_ctx->sig);

err:
	return ret;
}

/*
 * Sanity check of a verification context to see if everything seems
 * OK. 0 is returned on success, -1 on error.
 */
int ec_verify_ctx_callbacks_sanity_check(const struct ec_verify_context *verify_ctx)
{
	int ret;

	MUST_HAVE((verify_ctx != NULL) && (verify_ctx->ctx_magic == SIG_VERIFY_MAGIC), ret, err);

	ret = hash_mapping_callbacks_sanity_check(verify_ctx->h); EG(ret, err);
	ret = ec_sig_mapping_callbacks_sanity_check(verify_ctx->sig);

err:
	return ret;
}


/*
 * Compute generic effective signature length (in bytes) depending on the curve
 * parameters, the signature algorithm and the hash function. On success, 0 is
 * returned and The signature length is returned using 'siglen' parameter. -1 is
 * returned on error.
 */
int ec_get_sig_len(const ec_params *params, ec_alg_type sig_type,
		   hash_alg_type hash_type, u8 *siglen)
{
	const ec_sig_mapping *sm;
	u8 digest_size = 0;
	u8 block_size = 0;
	int ret;
	u8 i;

	MUST_HAVE(((params != NULL) && (siglen != NULL)), ret, err);

	ret = get_hash_sizes(hash_type, &digest_size, &block_size); EG(ret, err);

	ret = -1;
	for (i = 0, sm = &ec_sig_maps[i];
	     sm->type != UNKNOWN_ALG; sm = &ec_sig_maps[++i]) {
		if (sm->type == sig_type) {
			/* NOTE: since sm is initalized with a structure
			 * coming from a const source, we can safely call
			 * the callback here, but better safe than sorry.
			 */
			MUST_HAVE((sm->siglen != NULL), ret, err);
			ret = sm->siglen(params->ec_fp.p_bitlen,
					 params->ec_gen_order_bitlen,
					 digest_size, block_size, siglen);
			break;
		}
	}

err:
	return ret;
}

/* Generic signature */

/*
 * Core version of generic signature initialization function. Its purpose
 * is to initialize given sign context structure 'ctx' based on given key pair,
 * nn random function, signature and hash types. This version allows passing
 * a specific nn random function. It returns 0 on success, -1 on error.
 *
 * The random function is expected to initialize a nn 'out' with a value taken
 * uniformly at random in [1, q-1]. It returns 0 on success and -1 on error. See
 * nn_get_random_mod() in nn_rand.c for a function that fits the dscription.
 */
int _ec_sign_init(struct ec_sign_context *ctx,
		  const ec_key_pair *key_pair,
		  int (*rand) (nn_t out, nn_src_t q),
		  ec_alg_type sig_type, hash_alg_type hash_type,
		  const u8 *adata, u16 adata_len)
{
	const ec_sig_mapping *sm;
	const hash_mapping *hm;
	int ret;
	u8 i;

	MUST_HAVE((ctx != NULL), ret, err);

	ret = key_pair_check_initialized_and_type(key_pair, sig_type); EG(ret, err);

	/* We first need to get the specific hash structure */
	ret = -1;
	for (i = 0, hm = &hash_maps[i];
	     hm->type != UNKNOWN_HASH_ALG; hm = &hash_maps[++i]) {
		if (hm->type == hash_type) {
			ret = 0;
			break;
		}
	}
	if (ret) {
		goto err;
	}

	/* Now, let's try and get the specific key alg which was requested */
	ret = -1;
	for (i = 0, sm = &ec_sig_maps[i];
	     sm->type != UNKNOWN_ALG; sm = &ec_sig_maps[++i]) {
		if ((sm->type == sig_type) && (sm->sign_init != NULL)) {
			ret = 0;
			break;
		}
	}
	if (ret) {
		goto err;
	}

#ifdef NO_KNOWN_VECTORS
	/*
	 * NOTE: when we do not need self tests for known vectors,
	 * we can be strict about random function handler!
	 * We only use our internal method to provide random integers
	 * (which avoids honest mistakes ...).
	 *
	 * This also allows us to avoid the corruption of such a pointer
	 * in our signature contexts.
	 */
	if (rand) {
		MUST_HAVE((rand == nn_get_random_mod), ret, err);
	}
	rand = nn_get_random_mod;
#else
	/* Use given random function if provided or fallback to ours */
	if (!rand) {
		rand = nn_get_random_mod;
	}
#endif
	/* Sanity checks on our mappings */
	ret = hash_mapping_sanity_check(hm); EG(ret, err);
	ret = sig_mapping_sanity_check(sm); EG(ret, err);

	/* Initialize context for specific signature function */
	ret = local_memset(ctx, 0, sizeof(struct ec_sign_context)); EG(ret, err);
	ctx->key_pair = key_pair;
	ctx->rand = rand;
	ctx->h = hm;
	ctx->sig = sm;
	ctx->adata = adata;
	ctx->adata_len = adata_len;
	ctx->ctx_magic = SIG_SIGN_MAGIC;

	/*
	 * NOTE: since sm has been previously initalized with a structure
	 * coming from a const source, we can safely call the callback here.
	 */
	ret = sm->sign_init(ctx);

 err:
	if (ret && (ctx != NULL)) {
		/* Clear the whole context to prevent future reuse */
		IGNORE_RET_VAL(local_memset(ctx, 0, sizeof(struct ec_sign_context)));
	}

	return ret;
}

/*
 * Same as previous but for public use; it forces our internal nn random
 * function (nn_get_random_mod()). Returns 0 on success, -1 on error.
 */
int ec_sign_init(struct ec_sign_context *ctx, const ec_key_pair *key_pair,
		 ec_alg_type sig_type, hash_alg_type hash_type,
		 const u8 *adata, u16 adata_len)
{
	return _ec_sign_init(ctx, key_pair, NULL, sig_type, hash_type,
			     adata, adata_len);
}

/*
 * Signature update function. Returns 0 on success, -1 on error. On error,
 * signature context is zeroized and is no more usable.
 */
int ec_sign_update(struct ec_sign_context *ctx, const u8 *chunk, u32 chunklen)
{
	int ret;

	/* Sanity checks */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ret = sig_mapping_sanity_check(ctx->sig); EG(ret, err);
	ret = hash_mapping_sanity_check(ctx->h); EG(ret, err);
	ret = ec_sig_ctx_callbacks_sanity_check(ctx); EG(ret, err);
	ret = ctx->sig->sign_update(ctx, chunk, chunklen);

err:
	if (ret && (ctx != NULL)) {
		/* Clear the whole context to prevent future reuse */
		IGNORE_RET_VAL(local_memset(ctx, 0, sizeof(struct ec_sign_context)));
	}

	return ret;
}

/*
 * Signature finalization function. Returns 0 on success, -1 on error.
 * Upon call, the signature context is cleared to prevent future use.
 */
int ec_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen)
{
	int ret;

	/* Sanity checks */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ret = sig_mapping_sanity_check(ctx->sig); EG(ret, err);
	ret = hash_mapping_sanity_check(ctx->h); EG(ret, err);
	ret = ec_sig_ctx_callbacks_sanity_check(ctx); EG(ret, err);
	ret = ctx->sig->sign_finalize(ctx, sig, siglen);

err:
	if (ctx != NULL) {
		/* Clear the whole context to prevent future reuse */
		IGNORE_RET_VAL(local_memset(ctx, 0, sizeof(struct ec_sign_context)));
	}

	return ret;
}

/*
 * Single call version of signature function (init, update and finalize). It
 * returns 0 on success, -1 on error. This version allows passing a custom
 * random function. This is useful for test vectors but should be done with
 * care.
 *
 * The random function is expected to initialize a nn 'out' with a value taken
 * uniformly at random in [1, q-1]. It returns 0 on success and -1 on error. See
 * nn_get_random_mod() in nn_rand.c for a function that fits the dscription.
 */
int generic_ec_sign(u8 *sig, u8 siglen, const ec_key_pair *key_pair,
		    const u8 *m, u32 mlen,
		    int (*rand) (nn_t out, nn_src_t q),
		    ec_alg_type sig_type, hash_alg_type hash_type,
		    const u8 *adata, u16 adata_len)
{
	struct ec_sign_context ctx;
	int ret;

	ret = _ec_sign_init(&ctx, key_pair, rand, sig_type,
			    hash_type, adata, adata_len); EG(ret, err);
	ret = ec_sign_update(&ctx, m, mlen); EG(ret, err);
	ret = ec_sign_finalize(&ctx, sig, siglen);

err:
	return ret;
}


int _ec_sign(u8 *sig, u8 siglen, const ec_key_pair *key_pair,
	     const u8 *m, u32 mlen,
	     int (*rand) (nn_t out, nn_src_t q),
	     ec_alg_type sig_type, hash_alg_type hash_type,
	     const u8 *adata, u16 adata_len)
{
	const ec_sig_mapping *sm;
	int ret;

	ret = get_sig_by_type(sig_type, &sm); EG(ret, err);
	MUST_HAVE(((sm != NULL) && (sm->sign != NULL)), ret, err);

	ret = sm->sign(sig, siglen, key_pair, m, mlen, rand,
		       sig_type, hash_type, adata, adata_len);

err:
	return ret;
}

/*
 * Same as previous but for public use; it forces our internal nn random
 * function (nn_get_random_mod()) by pasing NULL for 'rand' argument
 * _ec_sign(). Returns 0 on success, -1 on error.
 */
int ec_sign(u8 *sig, u8 siglen, const ec_key_pair *key_pair,
	    const u8 *m, u32 mlen,
	    ec_alg_type sig_type, hash_alg_type hash_type,
	    const u8 *adata, u16 adata_len)
{
	return _ec_sign(sig, siglen, key_pair, m, mlen,
			NULL, sig_type, hash_type, adata, adata_len);
}

/*
 * Generic signature verification initialization function. Returns 0 on success,
 * -1 on error. On error, verification context is cleared to prevent further
 * reuse.
 */
int ec_verify_init(struct ec_verify_context *ctx, const ec_pub_key *pub_key,
		   const u8 *sig, u8 siglen,
		   ec_alg_type sig_type, hash_alg_type hash_type,
		   const u8 *adata, u16 adata_len)
{
	const ec_sig_mapping *sm;
	const hash_mapping *hm;
	u8 i;
	int ret;

	MUST_HAVE((ctx != NULL), ret, err);

	ret = pub_key_check_initialized_and_type(pub_key, sig_type); EG(ret, err);

	/* We first need to get the specific hash structure */
	ret = -1;
	for (i = 0, hm = &hash_maps[i];
	     hm->type != UNKNOWN_HASH_ALG; hm = &hash_maps[++i]) {
		if (hm->type == hash_type) {
			ret = 0;
			break;
		}
	}
	if (ret) {
		goto err;
	}

	/*
	 * Now, let's try and get the specific key algorithm which was
	 * requested
	 */
	ret = -1;
	for (i = 0, sm = &ec_sig_maps[i];
	     sm->type != UNKNOWN_ALG; sm = &ec_sig_maps[++i]) {
		if ((sm->type == sig_type) && (sm->verify_init != NULL)) {
			ret = 0;
			break;
		}
	}
	if (ret) {
		goto err;
	}

	/* Sanity checks on our mappings */
	ret = hash_mapping_sanity_check(hm); EG(ret, err);
	ret = sig_mapping_sanity_check(sm); EG(ret, err);

	/* Initialize context for specific signature function */
	ret = local_memset(ctx, 0, sizeof(struct ec_verify_context)); EG(ret, err);
	ctx->pub_key = pub_key;
	ctx->h = hm;
	ctx->sig = sm;
	ctx->adata = adata;
	ctx->adata_len = adata_len;
	ctx->ctx_magic = SIG_VERIFY_MAGIC;

	/*
	 * NOTE: since sm has been previously initalized with a structure
	 * coming from a const source, we can safely call the callback
	 * here.
	 */
	ret = sm->verify_init(ctx, sig, siglen);

 err:

	if (ret && (ctx != NULL)) {
		/* Clear the whole context to prevent future reuse */
		IGNORE_RET_VAL(local_memset(ctx, 0, sizeof(struct ec_verify_context)));
	}

	return ret;
}

/*
 * Signature verification update function. Returns 0 on success, -1 on error.
 * On error, verification context is cleared to prevent further reuse.
 */
int ec_verify_update(struct ec_verify_context *ctx,
		     const u8 *chunk, u32 chunklen)
{
	int ret;

	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ret = sig_mapping_sanity_check(ctx->sig); EG(ret, err);
	ret = hash_mapping_sanity_check(ctx->h); EG(ret, err);

	/* Since we call a callback, sanity check our contexts */
	ret = ec_verify_ctx_callbacks_sanity_check(ctx); EG(ret, err);
	ret = ctx->sig->verify_update(ctx, chunk, chunklen);

err:
	if (ret && (ctx != NULL)) {
		/* Clear the whole context to prevent future reuse */
		IGNORE_RET_VAL(local_memset(ctx, 0, sizeof(struct ec_verify_context)));
	}

	return ret;
}

/*
 * Signature verification finalize function. Returns 0 on success, -1 on error.
 * On error, verification context is cleared to prevent further reuse.
 */
int ec_verify_finalize(struct ec_verify_context *ctx)
{
	int ret;

	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ret = sig_mapping_sanity_check(ctx->sig); EG(ret, err);
	ret = hash_mapping_sanity_check(ctx->h); EG(ret, err);

	/* Since we call a callback, sanity check our contexts */
	ret = ec_verify_ctx_callbacks_sanity_check(ctx); EG(ret, err);
	ret = ctx->sig->verify_finalize(ctx);

err:
	if (ret && (ctx != NULL)) {
		/* Clear the whole context to prevent future reuse */
		IGNORE_RET_VAL(local_memset(ctx, 0, sizeof(struct ec_verify_context)));
	}
	return ret;
}

/*
 * Single call version of signature verification function (init, update and
 * finalize). It returns 0 on success, -1 on error.
 */
int generic_ec_verify(const u8 *sig, u8 siglen, const ec_pub_key *pub_key,
		      const u8 *m, u32 mlen,
		      ec_alg_type sig_type, hash_alg_type hash_type,
		      const u8 *adata, u16 adata_len)
{
	struct ec_verify_context ctx;
	int ret;

	ret = ec_verify_init(&ctx, pub_key, sig, siglen, sig_type,
			hash_type, adata, adata_len); EG(ret, err);
	ret = ec_verify_update(&ctx, m, mlen); EG(ret, err);
	ret = ec_verify_finalize(&ctx);

 err:
	return ret;
}

int ec_verify(const u8 *sig, u8 siglen, const ec_pub_key *pub_key,
	      const u8 *m, u32 mlen,
	      ec_alg_type sig_type, hash_alg_type hash_type,
	      const u8 *adata, u16 adata_len)
{

	const ec_sig_mapping *sm;
	int ret;

	ret = get_sig_by_type(sig_type, &sm); EG(ret, err);

	MUST_HAVE((sm != NULL) && (sm->verify != NULL), ret, err);

	ret = sm->verify(sig, siglen, pub_key, m, mlen, sig_type,
			 hash_type, adata, adata_len);

err:
	return ret;
}

int ec_verify_batch(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
              const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
              hash_alg_type hash_type, const u8 **adata, const u16 *adata_len,
	      verify_batch_scratch_pad *scratch_pad_area, u32 *scratch_pad_area_len)
{

	const ec_sig_mapping *sm;
	int ret;

	ret = get_sig_by_type(sig_type, &sm); EG(ret, err);

	MUST_HAVE((sm != NULL) && (sm->verify_batch != NULL), ret, err);

	ret = sm->verify_batch(s, s_len, pub_keys, m, m_len, num, sig_type,
			 hash_type, adata, adata_len,
			 scratch_pad_area, scratch_pad_area_len);

err:
	return ret;
}

/*
 * Import a signature with structured data containing information about the EC
 * algorithm type as well as the hash function used to produce the signature.
 * The function returns 0 on success, -1 on error. out parameters (sig_type,
 * hash_type, curve_name should only be considered on success.
 */
int ec_structured_sig_import_from_buf(u8 *sig, u32 siglen,
				      const u8 *out_buf, u32 outlen,
				      ec_alg_type * sig_type,
				      hash_alg_type * hash_type,
				      u8 curve_name[MAX_CURVE_NAME_LEN])
{
	u32 metadata_len = (3 * sizeof(u8));
	int ret;

	MUST_HAVE((out_buf != NULL) && (sig_type != NULL) &&
		  (hash_type != NULL) && (curve_name != NULL), ret, err);
	/* We only deal with signatures of length < 256 */
	MUST_HAVE((siglen <= EC_MAX_SIGLEN) && (sig != NULL), ret, err);

	/* We first import the metadata consisting of:
	 *	- One byte = the EC algorithm type
	 *	- One byte = the hash algorithm type
	 *	- One byte = the curve type (FRP256V1, ...)
	 */
	MUST_HAVE((outlen <= (siglen + metadata_len)), ret, err);

	*sig_type = (ec_alg_type)out_buf[0];
	*hash_type = (hash_alg_type)out_buf[1];
	ret = ec_get_curve_name_by_type((ec_curve_type) out_buf[2],
					curve_name, MAX_CURVE_NAME_LEN); EG(ret, err);

	/* Copy the raw signature */
	ret = local_memcpy(sig, out_buf + metadata_len, siglen);

err:
	return ret;
}

/*
 * Export a signature with structured data containing information about the
 * EC algorithm type as well as the hash function used to produce it. The
 * function returns 0 on success, -1 on error.
 */
int ec_structured_sig_export_to_buf(const u8 *sig, u32 siglen,
				    u8 *out_buf, u32 outlen,
				    ec_alg_type sig_type,
				    hash_alg_type hash_type,
				    const u8
				    curve_name[MAX_CURVE_NAME_LEN])
{
	u32 metadata_len = (3 * sizeof(u8));
	u32 len;
	u8 curve_name_len;
	ec_curve_type curve_type;
	int ret;

	MUST_HAVE((out_buf != NULL) && (curve_name != NULL), ret, err);
	/* We only deal with signatures of length < 256 */
	MUST_HAVE((siglen <= EC_MAX_SIGLEN) && (sig != NULL), ret, err);

	/* We first export the metadata consisting of:
	 *	- One byte = the EC algorithm type
	 *	- One byte = the hash algorithm type
	 *	- One byte = the curve type (FRP256V1, ...)
	 *
	 */
	MUST_HAVE(outlen >= (siglen + metadata_len), ret, err);

	out_buf[0] = (u8)sig_type;
	out_buf[1] = (u8)hash_type;
	ret = local_strlen((const char *)curve_name, &len); EG(ret, err);
	len += 1;
	MUST_HAVE((len < 256), ret, err);
	curve_name_len = (u8)len;
	ret = ec_get_curve_type_by_name(curve_name, curve_name_len, &curve_type); EG(ret, err);
	out_buf[2] = (u8)curve_type;
	MUST_HAVE((out_buf[2] != UNKNOWN_CURVE), ret, err);

	/* Copy the raw signature */
	ret = local_memcpy(out_buf + metadata_len, sig, siglen);

err:
	return ret;
}


/* Signature finalization function */
int unsupported_sign_init(struct ec_sign_context * ctx)
{
	/* Quirk to avoid unused variables */
	FORCE_USED_VAR(ctx);

	/* Return an error in any case here */
	return -1;
}

int unsupported_sign_update(struct ec_sign_context * ctx,
		    const u8 *chunk, u32 chunklen)
{
	/* Quirk to avoid unused variables */
	FORCE_USED_VAR(ctx);
	FORCE_USED_VAR(chunk);
	FORCE_USED_VAR(chunklen);

	/* Return an error in any case here */
	return -1;
}

int unsupported_sign_finalize(struct ec_sign_context *ctx, u8 *sig, u8 siglen)
{
	/* Quirk to avoid unused variables */
	FORCE_USED_VAR(ctx);
	FORCE_USED_VAR(sig);
	FORCE_USED_VAR(siglen);

	/* Return an error in any case here */
	return -1;
}

int unsupported_verify_init(struct ec_verify_context * ctx,
		    const u8 *sig, u8 siglen)
{
	/* Quirk to avoid unused variables */
	FORCE_USED_VAR(ctx);
	FORCE_USED_VAR(sig);
	FORCE_USED_VAR(siglen);

	/* Return an error in any case here */
	return -1;
}

int unsupported_verify_update(struct ec_verify_context * ctx,
		      const u8 *chunk, u32 chunklen)
{
	/* Quirk to avoid unused variables */
	FORCE_USED_VAR(ctx);
	FORCE_USED_VAR(chunk);
	FORCE_USED_VAR(chunklen);

	/* Return an error in any case here */
	return -1;
}

int unsupported_verify_finalize(struct ec_verify_context * ctx)
{
	/* Quirk to avoid unused variables */
	FORCE_USED_VAR(ctx);

	/* Return an error in any case here */
	return -1;
}

/* Unsupported batch verification */
int unsupported_verify_batch(const u8 **s, const u8 *s_len, const ec_pub_key **pub_keys,
              const u8 **m, const u32 *m_len, u32 num, ec_alg_type sig_type,
              hash_alg_type hash_type, const u8 **adata, const u16 *adata_len,
	      verify_batch_scratch_pad *scratch_pad_area, u32 *scratch_pad_area_len)
{
	/* Quirk to avoid unused variables */
	FORCE_USED_VAR(s);
	FORCE_USED_VAR(pub_keys);
	FORCE_USED_VAR(m);
	FORCE_USED_VAR(num);
	FORCE_USED_VAR(sig_type);
	FORCE_USED_VAR(hash_type);
	FORCE_USED_VAR(adata);
	FORCE_USED_VAR(s_len);
	FORCE_USED_VAR(m_len);
	FORCE_USED_VAR(adata_len);
	FORCE_USED_VAR(scratch_pad_area);
	FORCE_USED_VAR(scratch_pad_area_len);

	/* Return an error in any case here */
	return -1;
}

/* This function returns 1 in 'check' if the init/update/finalize mode
 * is supported by the signature algorithm, 0 otherwise.
 *
 * Return value is 0 on success, -1 on error. 'check' is only meaningful on
 * success.
 */
int is_sign_streaming_mode_supported(ec_alg_type sig_type, int *check)
{
	int ret;
	const ec_sig_mapping *sig;

	MUST_HAVE((check != NULL), ret, err);

	ret = get_sig_by_type(sig_type, &sig); EG(ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	if ((sig->sign_init == unsupported_sign_init) ||
	    (sig->sign_update == unsupported_sign_update) ||
	    (sig->sign_finalize == unsupported_sign_finalize)) {
		(*check) = 0;
	}
	else{
		(*check) = 1;
	}

err:
	return ret;
}

/* This function returns 1 in 'check' if the init/update/finalize mode
 * is supported by the verification algorithm, 0 otherwise.
 *
 * Return value is 0 on success, -1 on error. 'check' is only meaningful on
 * success.
 */
int is_verify_streaming_mode_supported(ec_alg_type sig_type, int *check)
{
	int ret;
	const ec_sig_mapping *sig;

	MUST_HAVE((check != NULL), ret, err);

	ret = get_sig_by_type(sig_type, &sig); EG(ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	if ((sig->verify_init == unsupported_verify_init) ||
	    (sig->verify_update == unsupported_verify_update) ||
	    (sig->verify_finalize == unsupported_verify_finalize)) {
		(*check) = 0;
	}
	else{
		(*check) = 1;
	}

err:
	return ret;
}

/* This function returns 1 in 'check' if the batch verification mode
 * is supported by the verification algorithm, 0 otherwise.
 *
 * Return value is 0 on success, -1 on error. 'check' is only meaningful on
 * success.
 */
int is_verify_batch_mode_supported(ec_alg_type sig_type, int *check)
{
	int ret;
	const ec_sig_mapping *sig;

	MUST_HAVE((check != NULL), ret, err);

	ret = get_sig_by_type(sig_type, &sig); EG(ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	if (sig->verify_batch == unsupported_verify_batch) {
		(*check) = 0;
	}
	else{
		(*check) = 1;
	}

err:
	return ret;
}

/* Tells if the signature scheme is deterministic or not,
 * e.g. if random nonces are used to produce signatures.
 *
 * 'check' is set to 1 if deterministic, 0 otherwise.
 *
 * Return value is 0 on success, -1 on error. 'check' is only meaningful on
 * success.

 */
int is_sign_deterministic(ec_alg_type sig_type, int *check)
{
	int ret;
	const ec_sig_mapping *sig;

	MUST_HAVE((check != NULL), ret, err);

	ret = get_sig_by_type(sig_type, &sig); EG(ret, err);
	MUST_HAVE((sig != NULL), ret, err);

	switch(sig_type) {
#if defined(WITH_SIG_EDDSA25519)
	case EDDSA25519:
	case EDDSA25519CTX:
	case EDDSA25519PH:
		(*check) = 1;
		break;
#endif
#if defined(WITH_SIG_EDDSA448)
	case EDDSA448:
	case EDDSA448PH:
		(*check) = 1;
		break;
#endif
#if defined(WITH_SIG_DECDSA)
	case DECDSA:
		(*check) = 1;
		break;
#endif
	default:
		(*check) = 0;
		break;
	}

err:
	return ret;
}


/*
 * Bubble sort the table of numbers and the table of projective points
 * accordingly in ascending order. We only work on index numbers in the table
 * to avoid useless copies.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _bubble_sort(verify_batch_scratch_pad *elements, u32 num)
{
        u32 i, j;
        int ret, swapped;

        MUST_HAVE((elements != NULL), ret, err);
        MUST_HAVE((num >= 1), ret, err);
        for(i = 0; i < (num - 1); i++){
		swapped = 0;
                for(j = 0; j < (num - i - 1); j++){
                        int check;
			u32 indexj, indexj_next;
			indexj      = elements[j].index;
			indexj_next = elements[j + 1].index;
                        ret = nn_cmp(&elements[indexj].number, &elements[indexj_next].number, &check); EG(ret, err);
                        if(check < 0){
                                /* Swap the two elements */
                                elements[j].index   = indexj_next;
                                elements[j + 1].index = indexj;
				swapped = 1;
                        }
                }
		/* If no swap occured in the inner loop, get out */
		if(!swapped){
			break;
		}
        }

        ret = 0;
err:
        return ret;
}

/*
 * Bos-Coster algorithm, presented e.g. in https://ed25519.cr.yp.to/ed25519-20110705.pdf
 *
 * The Bos-Coster algorithm allows to optimize a sum of multi-scalar multiplications using
 * addition chains. This is used for example in batch signature verification of schemes
 * that support it.
 *
 */
int ec_verify_bos_coster(verify_batch_scratch_pad *elements, u32 num, bitcnt_t bits)
{
	int ret, check;
	u32 i, index0, index1, max_bos_coster_iterations;

	MUST_HAVE((elements != NULL), ret, err);
	MUST_HAVE((num > 1), ret, err);

	/* We fix our maximum attempts here.
	 *
	 * NOTE: this avoids "denial of service" when
	 * providing scalars with too big discrepancies, as
	 * the Bos-Coster algorithm supposes uniformly randomized
	 * numbers ...
	 * If we are provided with scalars with too big differences,
	 * we end up looping for a very long time. In this case, we
	 * rather quit with a specific error.
	 *
	 * The limit hereafter is fixed using the mean asymptotic complexity
	 * of the algorithm in the nominal case (multiplied by the bit size
	 * of num to be lax).
	 */
	MUST_HAVE((num * bits) >= num, ret, err);
	MUST_HAVE((num * bits) >= bits, ret, err);
	max_bos_coster_iterations = (num * bits);

        /********************************************/
        /****** Bos-Coster algorithm ****************/
        for(i = 0; i < num; i++){
                elements[i].index = i;
        }
	i = 0;
        do {
		/* Sort the elements in descending order */
		ret = _bubble_sort(elements, num); EG(ret, err);
                /* Perform the addition */
		index0 = elements[0].index;
		index1 = elements[1].index;
		ret = prj_pt_add(&elements[index1].point, &elements[index0].point,
				 &elements[index1].point); EG(ret, err);
                /* Check the two first integers */
		ret = nn_cmp(&elements[index0].number, &elements[index1].number, &check);
                /* Subtract the two first numbers */
                ret = nn_sub(&elements[index0].number, &elements[index0].number,
                             &elements[index1].number); EG(ret, err);
		i++;
		if(i > max_bos_coster_iterations){
			/* Give up with specific error code */
			ret = -2;
			goto err;
		}
	} while(check > 0);

	index0 = elements[0].index;
	/* Proceed with the last scalar multiplication */
	ret = _prj_pt_unprotected_mult(&elements[index0].point, &elements[index0].number, &elements[index0].point);

	/* The result is in point [0] of elements */
err:
	return ret;
}
