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
#include "sss_private.h"
#include "sss.h"

/*
 * The purpose of this example is to implement the SSS
 * (Shamir's Secret Sharing) scheme based on libecc arithmetic
 * primitives. The scheme is implemented over a ~256 bit prime
 * field.
 *
 * Secret sharing allows to combine some shares (at least k among n >= k)
 * to regenerate a secret. The current code also ensures the integrity
 * of the shares using HMAC. A maximum of (2**16 - 1) shares can be
 * generated, and beware that the time complexity of generation heavily
 * increases with k and n, and the time complexity of shares combination
 * increases with k.
 *
 * Shares regeneration from exisiting ones is also offered although it
 * is expensive in CPU cycles (as the Lagrange interpolation polynomials
 * have to be evaluated for each existing share before computing new ones).
 *
 * !! DISCLAIMER !!
 * ================
 * Some efforts have been put on providing a clean code and constant time
 * as well as some SCA (side-channel attacks) resistance (e.g. blinding some
 * operations manipulating secrets). However, no absolute guarantee can be claimed:
 * use this code knowingly and at your own risks!
 *
 * Also, as for all other libecc primitives, beware of randomness sources. By default,
 * the library uses the OS random sources (e.g. "/dev/urandom"), but the user
 * is encouraged to adapt the ../external_deps/rand.c source file to combine
 * multiple sources and add entropy there depending on the context where this
 * code is integrated. The security level of all the cryptographic primitives
 * heavily relies on random sources quality.
 *
 */

#ifndef GET_UINT16_BE
#define GET_UINT16_BE(n, b, i)                          \
do {                                                    \
        (n) =     (u16)( ((u16) (b)[(i)    ]) << 8 )    \
                | (u16)( ((u16) (b)[(i) + 1])       );  \
} while( 0 )
#endif

#ifndef PUT_UINT16_BE
#define PUT_UINT16_BE(n, b, i)                  \
do {                                            \
        (b)[(i)    ] = (u8) ( (n) >> 8 );       \
        (b)[(i) + 1] = (u8) ( (n)       );      \
} while( 0 )
#endif

/* The prime number we use: it is close to (2**256-1) but still stricly less
 * than this value, hence a theoretical security of more than 255 bits but less than
 * 256 bits. This prime p is used in the prime field of secp256k1, the "bitcoin"
 * curve.
 *
 * This can be modified with another prime, beware however of the size
 * of the prime to be in line with the shared secrets sizes, and also
 * that all our shares and secret lie in Fp, and hence are < p,
 *
 * Although bigger primes could be used, beware that SSS shares recombination
 * complexity is quadratic in the number of shares, yielding impractical
 * computation time when the prime is too big. Also, some elements related to
 * the share generation (_sss_derive_seed) must be adapated to keep proper entropy
 * if the prime (size) is modified.
 */
static const u8 prime[] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfc, 0x2f,
};

ATTRIBUTE_WARN_UNUSED_RET static int _sss_derive_seed(fp_t out, const u8 seed[SSS_SECRET_SIZE], u16 idx)
{
	int ret;
	u8 hmac_val[SHA512_DIGEST_SIZE];
	u8 C[2];
	u8 len;
	nn nn_val;

	/* Sanity check on sizes to avoid entropy loss through reduction biases */
	MUST_HAVE((SHA512_DIGEST_SIZE >= (2 * SSS_SECRET_SIZE)), ret, err);

	/* out must be initialized with a context */
	ret = fp_check_initialized(out); EG(ret, err);

	ret = local_memset(hmac_val, 0, sizeof(hmac_val)); EG(ret, err);
	ret = local_memset(C, 0, sizeof(C)); EG(ret, err);

	/* Export our idx in big endian representation on two bytes */
	PUT_UINT16_BE(idx, C, 0);

	len = sizeof(hmac_val);
	ret = hmac(seed, SSS_SECRET_SIZE, SHA512, C, sizeof(C), hmac_val, &len); EG(ret, err);

	ret = nn_init_from_buf(&nn_val, hmac_val, len); EG(ret, err);
	/* Since we will put this in Fp, take the modulo */
	ret = nn_mod(&nn_val, &nn_val, &(out->ctx->p)); EG(ret, err);
	/* Now import our reduced value in Fp as the result of the derivation */
	ret = fp_set_nn(out, &nn_val);

err:
	/* Cleanup secret data */
	IGNORE_RET_VAL(local_memset(hmac_val, 0, sizeof(hmac_val)));
	IGNORE_RET_VAL(local_memset(C, 0, sizeof(C)));
	nn_uninit(&nn_val);

	return ret;
}

/***** Raw versions ***********************/
/* SSS shares and secret generation */
ATTRIBUTE_WARN_UNUSED_RET static int _sss_raw_generate(sss_share *shares, u16 k, u16 n, sss_secret *secret, boolean input_secret)
{
	fp_ctx ctx;
	nn p;
	fp a0, a, s;
	fp exp, base, tmp;
	fp blind, blind_inv;
	u8 secret_seed[SSS_SECRET_SIZE];
	u16 idx_shift, num_shares;
	int ret;
	unsigned int i, j;
	p.magic = WORD(0);
	exp.magic = base.magic = tmp.magic = s.magic = a.magic = a0.magic = WORD(0);
	blind.magic = blind_inv.magic = WORD(0);

	ret = local_memset(secret_seed, 0, sizeof(secret_seed)); EG(ret, err);

	MUST_HAVE((shares != NULL) && (secret != NULL), ret, err);
	/* Sanity checks */
	MUST_HAVE((n <= (u16)(0xffff - 1)), ret, err);
	MUST_HAVE((k <= n), ret, err);
	MUST_HAVE((k >= 1), ret, err);
	MUST_HAVE((SSS_SECRET_SIZE == sizeof(prime)), ret, err);

	/* Import our prime number and create the Fp context */
	ret = nn_init_from_buf(&p, prime, sizeof(prime)); EG(ret, err);
	ret = fp_ctx_init_from_p(&ctx, &p); EG(ret, err);

	/* Generate a secret seed of the size of the secret that will be our base to
	 * generate the plolynomial coefficients.
	 */
	ret = get_random(secret_seed, sizeof(secret_seed)); EG(ret, err);
	/* NOTE: although we could generate all our a[i] coefficients using our randomness
	 * source, we prefer to derive them from a single secret seed in order to optimize
	 * the storage space as our share generation algorithm needs to parse these a[i] multiple
	 * times. This time / memory tradeoff saves a lot of memory space for embedded contexts and
	 * avoids "malloc" usage (preserving the "no dynamic allocation" philosophy of libecc).
	 *
	 * Our secret seed is SSS_SECRET_SIZE long, so on the security side there should be no
	 * loss of strength/entropy. For each index i, a[i] is computed as follows:
	 *
	 *        a[i] = HMAC(secret_seed, i)
	 * where the HMAC is interpreted as a value in Fp (i.e. modulo p), and i is represented
	 * as a string of 2 elements. The HMAC uses a hash function of at least twice the
	 * size of the secret to avoid biases in modular reduction.
	 */

	/* a0 is either derived from the secret seed or taken from input if
	 * provided.
	 */
	ret = fp_init(&a0, &ctx); EG(ret, err);
	if(input_secret == SSS_TRUE){
		/* Import the secret the user provides
		 * XXX: NOTE: the user shared secret MUST be in Fp! Since our prime is < (2**256 - 1),
		 * some 256 bit strings can be rejected here (namely those >= p and <= (2**256 - 1)).
		 */
		ret = fp_import_from_buf(&a0, secret->secret, SSS_SECRET_SIZE); EG(ret, err);
	}
	else{
		/* Generate the secret from our seed */
		ret = _sss_derive_seed(&a0, secret_seed, 0); EG(ret, err);
	}

	/* Compute the shares P(x) for x in [idx_shift + 0, ..., idx_shift + n] (or
	 * [idx_shift + 0, ..., idx_shift + n + 1] to avoid the 0 index),
	 * with idx_shift a non-zero random index shift to avoid leaking the number of shares.
	 */
	ret = fp_init(&base, &ctx); EG(ret, err);
	ret = fp_init(&exp, &ctx); EG(ret, err);
	ret = fp_init(&tmp, &ctx); EG(ret, err);
	ret = fp_init(&s, &ctx); EG(ret, err);
	ret = fp_init(&a, &ctx); EG(ret, err);
	/* Get a random blind mask and invert it */
	ret = fp_get_random(&blind, &ctx); EG(ret, err);
	ret = fp_init(&blind_inv, &ctx); EG(ret, err);
	ret = fp_inv(&blind_inv, &blind); EG(ret, err);
	/* Generate a non-zero random index base for x to avoid leaking
	 * the number of shares. We could use a static sequence from x = 1 to n
	 * but this would leak some information to the participants about the number
	 * of shares (e.g. if a participant gets the share with x = 4, she surely knows
	 * that n >= 4). To avoid the leak we randomize the base value of the index where
	 * we begin our x.
	 */
	idx_shift = 0;
	while(idx_shift == 0){
		ret = get_random((u8*)&idx_shift, sizeof(idx_shift)); EG(ret, err);
	}
	num_shares = 0;
	i = 0;
	while(num_shares < n){
		_sss_raw_share *cur_share_i = &(shares[num_shares].raw_share);
		u16 curr_idx = (u16)(idx_shift + i);
		if(curr_idx == 0){
			/* Skip the index 0 specific case */
			i++;
			continue;
		}
		/* Set s[i] to the a[0] as blinded initial value */
		ret = fp_mul(&s, &blind, &a0); EG(ret, err);
		/* Get a random base x as u16 for share index */
		ret = fp_set_word_value(&base, (word_t)curr_idx); EG(ret, err);
		/* Set the exp to 1 */
		ret = fp_one(&exp); EG(ret, err);
		for(j = 1; j < k; j++){
			/* Compute x**j by iterative multiplications */
			ret = fp_mul_monty(&exp, &exp, &base); EG(ret, err);
			/* Compute our a[j] coefficient */
			ret = _sss_derive_seed(&a, secret_seed, (u16)j); EG(ret, err);
			/* Blind a[j] */
			ret = fp_mul_monty(&a, &a, &blind); EG(ret, err);
			/* NOTE1: actually, the real a[j] coefficients are _sss_derive_seed(secret_seed, j)
			 * multiplied by some power of r^-1 (the Montgomery constant), but this is OK as
			 * we need any random values (computable from the secret seed) here. We use this "trick"
			 * to be able to use our more performant redcified versions of Fp multiplication.
			 *
			 * NOTE2: this trick makes also this generation not deterministic with the same seed
			 * on binaries with different WORD sizes (16, 32, 64 bits) as the r Montgomery constant will
			 * differ depending on this size. However, this is not really an issue per se for our SSS
			 * as we are in our generation primitive and the a[j] coefficients are expected to be
			 * random (the only drawback is that deterministic test vectors will not be consistent
			 * across WORD sizes).
			 */
			/* Accumulate */
			ret = fp_mul_monty(&tmp, &exp, &a); EG(ret, err);
			ret = fp_add(&s, &s, &tmp); EG(ret, err);
		}
		/* Export the computed share */
		PUT_UINT16_BE(curr_idx, (u8*)&(cur_share_i->index), 0);
		/* Unblind */
		ret = fp_mul(&s, &s, &blind_inv); EG(ret, err);
		ret = fp_export_to_buf(cur_share_i->share, SSS_SECRET_SIZE, &s); EG(ret, err);
		num_shares++;
		i++;
	}
	/* The secret is a[0] */
	ret = fp_export_to_buf(secret->secret, SSS_SECRET_SIZE, &a0);

err:
	/* We can throw away our secret seed now that the shares have
	 * been generated.
	 */
	IGNORE_RET_VAL(local_memset(secret_seed, 0, sizeof(secret_seed)));
	IGNORE_RET_VAL(local_memset(&ctx, 0, sizeof(ctx)));
	nn_uninit(&p);
	fp_uninit(&a0);
	fp_uninit(&a);
	fp_uninit(&s);
	fp_uninit(&base);
	fp_uninit(&exp);
	fp_uninit(&tmp);
	fp_uninit(&blind);
	fp_uninit(&blind_inv);

	return ret;
}

/* SSS helper to compute Lagrange interpolation on an input value.
 *     - k is the number of shares pointed by the shares pointer
 *     - secret is the computed secret
 *     - val is the 'index' on which the Lagrange interpolation must be computed, i.e.
 *       the idea is to have using Lagrage formulas the value f(val) where f is our polynomial. Of course
 *       the proper value can only be computed if enough shares k are provided (the interpolation
 *       does not hold in other cases and the result will be an incorrect value)
 */
ATTRIBUTE_WARN_UNUSED_RET static int _sss_raw_lagrange(const sss_share *shares, u16 k, sss_secret *secret, u16 val)
{
	fp_ctx ctx;
	nn p;
	fp s, x, y;
	fp x_i, x_j, tmp, tmp2;
	fp blind, blind_inv, r_inv;
	int ret;
	unsigned int i, j;
	p.magic = WORD(0);
	x_i.magic = x_j.magic = tmp.magic = tmp2.magic = s.magic = y.magic = x.magic = WORD(0);
	blind.magic = blind_inv.magic = r_inv.magic = WORD(0);

	MUST_HAVE((shares != NULL) && (secret != NULL), ret, err);
	/* Sanity checks */
	MUST_HAVE((k >= 1), ret, err);
	MUST_HAVE((SSS_SECRET_SIZE == sizeof(prime)), ret, err);

	/* Import our prime number and create the Fp context */
	ret = nn_init_from_buf(&p, prime, sizeof(prime)); EG(ret, err);
	ret = fp_ctx_init_from_p(&ctx, &p); EG(ret, err);

	/* Recombine our shared secrets */
	ret = fp_init(&s, &ctx); EG(ret, err);
	ret = fp_init(&y, &ctx); EG(ret, err);
	ret = fp_init(&x_i, &ctx); EG(ret, err);
	ret = fp_init(&x_j, &ctx); EG(ret, err);
	ret = fp_init(&tmp, &ctx); EG(ret, err);
	ret = fp_init(&tmp2, &ctx); EG(ret, err);
	if(val != 0){
		/* NOTE: we treat the case 'val = 0' in a specific case for
		 * optimization. This optimization is of interest since computing
		 * f(0) (where f(.) is our polynomial) is the formula for getting the
		 * SSS secret (which happens to be the constant of degree 0 of the
		 * polynomial).
		 */
		ret = fp_init(&x, &ctx); EG(ret, err);
		ret = fp_set_word_value(&x, (word_t)val); EG(ret, err);
	}
	/* Get a random blind mask and invert it */
	ret = fp_get_random(&blind, &ctx); EG(ret, err);
	ret = fp_init(&blind_inv, &ctx); EG(ret, err);
	ret = fp_inv(&blind_inv, &blind); EG(ret, err);
	/* Perform the computation of r^-1 to optimize our multiplications using Montgomery
	 * multiplication in the main loop.
	 */
	ret = fp_init(&r_inv, &ctx); EG(ret, err);
	ret = fp_set_nn(&r_inv, &(ctx.r)); EG(ret, err);
	ret = fp_inv(&r_inv, &r_inv); EG(ret, err);
	/* Proceed with the interpolation */
	for(i = 0; i < k; i++){
		u16 curr_idx;
		const _sss_raw_share *cur_share_i = &(shares[i].raw_share);
		/* Import s[i] */
		ret = fp_import_from_buf(&s, cur_share_i->share, SSS_SECRET_SIZE); EG(ret, err);
		/* Blind s[i] */
		ret = fp_mul_monty(&s, &s, &blind); EG(ret, err);
		/* Get the index */
		GET_UINT16_BE(curr_idx, (const u8*)&(cur_share_i->index), 0);
		ret = fp_set_word_value(&x_i, (word_t)(curr_idx)); EG(ret, err);
		/* Initialize multiplication with "one" (actually Montgomery r^-1 for multiplication optimization) */
		ret = fp_copy(&tmp2, &r_inv); EG(ret, err);
		/* Compute the product for all k other than i
		 * NOTE: we use fp_mul in its redcified version as the multiplication by r^-1 is
		 * cancelled by the fraction of (x_j - x) * r^-1 / (x_j - x_i) * r^-1 = (x_j - x) / (x_j - x_i)
		 */
		for(j = 0; j < k; j++){
			const _sss_raw_share *cur_share_j = &(shares[j].raw_share);
			GET_UINT16_BE(curr_idx, (const u8*)&(cur_share_j->index), 0);
			ret = fp_set_word_value(&x_j, (word_t)(curr_idx)); EG(ret, err);
			if(j != i){
				if(val != 0){
					ret = fp_sub(&tmp, &x_j, &x); EG(ret, err);
					ret = fp_mul_monty(&s, &s, &tmp); EG(ret, err);
				}
				else{
					/* NOTE: we treat the case 'val = 0' in a specific case for
					 * optimization. This optimization is of interest since computing
					 * f(0) (where f(.) is our polynomial) is the formula for getting the
					 * SSS secret (which happens to be the constant of degree 0 of the
					 * polynomial).
					 */
					ret = fp_mul_monty(&s, &s, &x_j); EG(ret, err);
				}
				ret = fp_sub(&tmp, &x_j, &x_i); EG(ret, err);
				ret = fp_mul_monty(&tmp2, &tmp2, &tmp); EG(ret, err);
			}
		}
		/* Invert all the (x_j - x_i) poducts */
		ret = fp_inv(&tmp, &tmp2); EG(ret, err);
		ret = fp_mul_monty(&s, &s, &tmp); EG(ret, err);
		/* Accumulate in secret */
		ret = fp_add(&y, &y, &s); EG(ret, err);
	}
	/* Unblind y */
	ret = fp_redcify(&y, &y); EG(ret, err);
	ret = fp_mul(&y, &y, &blind_inv); EG(ret, err);
	/* We should have our secret in y */
	ret = fp_export_to_buf(secret->secret, SSS_SECRET_SIZE, &y);

err:
	IGNORE_RET_VAL(local_memset(&ctx, 0, sizeof(ctx)));
	nn_uninit(&p);
	fp_uninit(&s);
	fp_uninit(&y);
	fp_uninit(&x_i);
	fp_uninit(&x_j);
	fp_uninit(&tmp);
	fp_uninit(&tmp2);
	fp_uninit(&blind);
	fp_uninit(&blind_inv);
	fp_uninit(&r_inv);
	if(val != 0){
		fp_uninit(&x);
	}

	return ret;
}


/* SSS shares and secret combination */
ATTRIBUTE_WARN_UNUSED_RET static int _sss_raw_combine(const sss_share *shares, u16 k, sss_secret *secret)
{
	return _sss_raw_lagrange(shares, k, secret, 0);
}

/***** Secure versions (public APIs) ***********************/
/* SSS shares and secret generation:
 *     Inputs:
 *         - n: is the number of shares to generate
 *         - k: the quorum of shares to regenerate the secret (of course k <= n)
 *         - secret: the secret value when input_secret is set to 'true'
 *     Output:
 *         - shares: a pointer to the generated n shares
 *         - secret: the secret value when input_secret is set to 'false', this
 *           value being randomly generated
 */
int sss_generate(sss_share *shares, unsigned short k, unsigned short n, sss_secret *secret, boolean input_secret)
{
	int ret;
	unsigned int i;
	u8 len;
	u8 session_id[SSS_SESSION_ID_SIZE];

	ret = local_memset(session_id, 0, sizeof(session_id)); EG(ret, err);

	/* Generate raw shares */
	ret = _sss_raw_generate(shares, k, n, secret, input_secret); EG(ret, err);

	/* Sanity check */
	MUST_HAVE((SSS_HMAC_SIZE == sizeof(shares[0].raw_share_hmac)), ret, err);
	MUST_HAVE((SHA256_DIGEST_SIZE >= sizeof(shares[0].raw_share_hmac)), ret, err);

	/* Generate a random session ID */
	ret = get_random(session_id, sizeof(session_id)); EG(ret, err);

	/* Compute the authenticity seal for each share with HMAC */
	for(i = 0; i < n; i++){
		_sss_raw_share *cur_share = &(shares[i].raw_share);
		u8 *cur_id = (u8*)&(shares[i].session_id);
		u8 *cur_share_hmac = (u8*)&(shares[i].raw_share_hmac);
		/* NOTE: we 'abuse' casts here for shares[i].raw_share to u8*, but this should be OK since
		 * our structures are packed.
		 */
		const u8 *inputs[3] = { (const u8*)cur_share, cur_id, NULL };
		const u32 ilens[3] = { sizeof(*cur_share), SSS_SESSION_ID_SIZE, 0 };

		/* Copy the session ID */
		ret = local_memcpy(cur_id, session_id, SSS_SESSION_ID_SIZE); EG(ret, err);

		len = SSS_HMAC_SIZE;
		ret = hmac_scattered((const u8*)secret, SSS_SECRET_SIZE, SHA256, inputs, ilens, cur_share_hmac, &len); EG(ret, err);
	}

err:
	IGNORE_RET_VAL(local_memset(session_id, 0, sizeof(session_id)));

	return ret;
}

/* SSS shares and secret combination
 *     Inputs:
 *         - k: the quorum of shares to regenerate the secret
 *         - shares: a pointer to the k shares
 *     Output:
 *         - secret: the secret value computed from the k shares
 */
int sss_combine(const sss_share *shares, unsigned short k, sss_secret *secret)
{
	int ret, cmp;
	unsigned int i;
	u8 hmac_val[SSS_HMAC_SIZE];
	u8 len;

	ret = local_memset(hmac_val, 0, sizeof(hmac_val)); EG(ret, err);

	/* Recombine raw shares */
	ret = _sss_raw_combine(shares, k, secret); EG(ret, err);

	/* Compute and check the authenticity seal for each HMAC */
	for(i = 0; i < k; i++){
		const _sss_raw_share *cur_share = &(shares[i].raw_share);
		const u8 *cur_id = (const u8*)&(shares[i].session_id);
		const u8 *cur_id0 = (const u8*)&(shares[0].session_id);
		const u8 *cur_share_hmac = (const u8*)&(shares[i].raw_share_hmac);
		/* NOTE: we 'abuse' casts here for shares[i].raw_share to u8*, but this should be OK since
		 * our structures are packed.
		 */
		const u8 *inputs[3] = { (const u8*)cur_share, cur_id, NULL };
		const u32 ilens[3] = { sizeof(*cur_share), SSS_SESSION_ID_SIZE, 0 };

		/* Check that all our shares have the same session ID, return an error otherwise */
		ret = are_equal(cur_id, cur_id0, SSS_SESSION_ID_SIZE, &cmp); EG(ret, err);
		if(!cmp){
#ifdef VERBOSE
			ext_printf("[-] sss_combine error for share %d / %d: session ID is not OK!\n", i, k);
#endif
			ret = -1;
			goto err;
		}

		len = sizeof(hmac_val);
		ret = hmac_scattered((const u8*)secret, SSS_SECRET_SIZE, SHA256, inputs, ilens, hmac_val, &len); EG(ret, err);

		/* Check the HMAC */
		ret = are_equal(hmac_val, cur_share_hmac, len, &cmp); EG(ret, err);
		if(!cmp){
#ifdef VERBOSE
			ext_printf("[-] sss_combine error for share %d / %d: HMAC is not OK!\n", i, k);
#endif
			ret = -1;
			goto err;
		}
	}

err:
	IGNORE_RET_VAL(local_memset(hmac_val, 0, sizeof(hmac_val)));

	return ret;
}

/* SSS shares regeneration from existing shares
 *     Inputs:
 *         - shares: a pointer to the input k shares allowing the regeneration
 *         - n: is the number of shares to regenerate
 *         - k: the input shares (of course k <= n)
 *     Output:
 *         - shares: a pointer to the generated n shares (among which the k first are
 *           the ones provided as inputs)
 *         - secret: the recomputed secret value
 */
int sss_regenerate(sss_share *shares, unsigned short k, unsigned short n, sss_secret *secret)
{
	int ret, cmp;
	unsigned int i;
	u16 max_idx, num_shares;
	u8 hmac_val[SSS_HMAC_SIZE];
	u8 len;

	/* Sanity check */
	MUST_HAVE((n <= (u16)(0xffff - 1)), ret, err);
	MUST_HAVE((n >= k), ret, err);

	ret = local_memset(hmac_val, 0, sizeof(hmac_val)); EG(ret, err);

	/* Compute the secret */
	ret = _sss_raw_lagrange(shares, k, secret, 0); EG(ret, err);
	/* Check the authenticity of our shares */
	for(i = 0; i < k; i++){
		_sss_raw_share *cur_share = &(shares[i].raw_share);
		u8 *cur_id = (u8*)&(shares[i].session_id);
		u8 *cur_id0 = (u8*)&(shares[0].session_id);
		u8 *cur_share_hmac = (u8*)&(shares[i].raw_share_hmac);
		/* NOTE: we 'abuse' casts here for shares[i].raw_share to u8*, but this should be OK since
		 * our structures are packed.
		 */
		const u8 *inputs[3] = { (const u8*)cur_share, cur_id, NULL };
		const u32 ilens[3] = { sizeof(*cur_share), SSS_SESSION_ID_SIZE, 0 };

		/* Check that all our shares have the same session ID, return an error otherwise */
		ret = are_equal(cur_id, cur_id0, SSS_SESSION_ID_SIZE, &cmp); EG(ret, err);
		if(!cmp){
#ifdef VERBOSE
			ext_printf("[-] sss_regenerate error for share %d / %d: session ID is not OK!\n", i, k);
#endif
			ret = -1;
			goto err;
		}

		len = sizeof(hmac_val);
		/* NOTE: we 'abuse' cast here for secret to (const u8*), but this should be OK since our
		 * structures are packed.
		 */
		ret = hmac_scattered((const u8*)secret, SSS_SECRET_SIZE, SHA256, inputs, ilens, hmac_val, &len); EG(ret, err);
		ret = are_equal(hmac_val, cur_share_hmac, len, &cmp); EG(ret, err);
		if(!cmp){
#ifdef VERBOSE
			ext_printf("[-] sss_regenerate error for share %d / %d: HMAC is not OK!\n", i, k);
#endif
			ret = -1;
			goto err;
		}
	}

	/* Our secret regeneration consists of determining the maximum index, and
	 * proceed with Lagrange interpolation on new values.
	 */
	max_idx = 0;
	for(i = 0; i < k; i++){
		u16 curr_idx;
		GET_UINT16_BE(curr_idx, (u8*)&(shares[i].raw_share.index), 0);
		if(curr_idx > max_idx){
			max_idx = curr_idx;
		}
	}
	/* Now regenerate as many shares as we need */
	num_shares = 0;
	i = k;
	while(num_shares < (n - k)){
		_sss_raw_share *cur_share = &(shares[k + num_shares].raw_share);
		u8 *cur_id = (u8*)&(shares[k + num_shares].session_id);
		u8 *cur_id0 = (u8*)&(shares[0].session_id);
		u8 *cur_share_hmac = (u8*)&(shares[k + num_shares].raw_share_hmac);
		u16 curr_idx;
		/* NOTE: we 'abuse' casts here for shares[i].raw_share.share to sss_secret*, but this should be OK since
		 * our shares[i].raw_share.share is a SSS_SECRET_SIZE as the sss_secret.secret type encapsulates and our
		 * structures are packed.
		 */
		const u8 *inputs[3] = { (const u8*)cur_share, cur_id, NULL };
		const u32 ilens[3] = { sizeof(*cur_share), SSS_SESSION_ID_SIZE, 0 };

		/* Skip the index = 0 case */
		curr_idx = (u16)(max_idx + (u16)(i - k + 1));
		if(curr_idx == 0){
			i++;
			continue;
		}

		/* Copy our session ID */
		ret = local_memcpy(cur_id, cur_id0, SSS_SESSION_ID_SIZE); EG(ret, err);

		ret = _sss_raw_lagrange(shares, k, (sss_secret*)(cur_share->share), curr_idx); EG(ret, err);
		PUT_UINT16_BE(curr_idx, (u8*)&(cur_share->index), 0);

		/* Compute the HMAC */
		len = SSS_HMAC_SIZE;
		ret = hmac_scattered((const u8*)secret, SSS_SECRET_SIZE, SHA256, inputs, ilens, cur_share_hmac, &len); EG(ret, err);
		num_shares++;
		i++;
	}

err:
	IGNORE_RET_VAL(local_memset(hmac_val, 0, sizeof(hmac_val)));

	return ret;
}


/********* main test program for SSS *************/
#ifdef SSS
#include <libecc/utils/print_buf.h>

#define K 50
#define N 150
#define MAX_N 200

int main(int argc, char *argv[])
{
	int ret = 0;
	unsigned int i;
	sss_share shares[MAX_N];
	sss_share shares_[MAX_N];
	sss_secret secret;

	FORCE_USED_VAR(argc);
	FORCE_USED_VAR(argv);

	/* Generate N shares for SSS with at least K shares OK among N */
	ext_printf("[+] Generating the secrets %d / %d, call should be OK\n", K, N);
	ret = local_memset(&secret, 0x00, sizeof(secret)); EG(ret, err);
	/* NOTE: 'false' here means that we let the library generate the secret randomly */
	ret = sss_generate(shares, K, N, &secret, SSS_FALSE);
	if(ret){
		ext_printf("  [X] Error: sss_generate error\n");
		goto err;
	}
	else{
		buf_print("  secret", (u8*)&secret, SSS_SECRET_SIZE); EG(ret, err);
	}
	/* Shuffle shares */
	for(i = 0; i < N; i++){
		shares_[i] = shares[N - 1 - i];
	}

	/* Combine (k-1) shares: this call should trigger an ERROR */
	ext_printf("[+] Combining the secrets with less shares: call should trigger an error\n");
	ret = local_memset(&secret, 0x00, sizeof(secret)); EG(ret, err);
	ret = sss_combine(shares_, K - 1, &secret);
	if (ret) {
		ext_printf("  [X] Error: sss_combine error\n");
	} else{
		buf_print("  secret", (u8*)&secret, SSS_SECRET_SIZE);
	}

	/* Combine k shares: this call should be OK and recombine the initial
	 * secret
	 */
	ext_printf("[+] Combining the secrets with minimum shares: call should be OK\n");
	ret = local_memset(&secret, 0x00, sizeof(secret)); EG(ret, err);
	ret = sss_combine(shares_, K, &secret);
	if (ret) {
		ext_printf("  [X] Error: sss_combine error\n");
		goto err;
	} else {
		buf_print("  secret", (u8*)&secret, SSS_SECRET_SIZE);
	}

	/* Combine k shares: this call should be OK and recombine the initial
	 * secret
	 */
	ext_printf("[+] Combining the secrets with more shares: call should be OK\n");
	ret = local_memset(&secret, 0x00, sizeof(secret)); EG(ret, err);
	ret = sss_combine(shares_, K + 1, &secret);
	if (ret) {
		ext_printf("  [X] Error: sss_combine error\n");
		goto err;
	} else {
		buf_print("  secret", (u8*)&secret, SSS_SECRET_SIZE);
	}

	/* Combine with a corrupted share: call should trigger an error */
	ext_printf("[+] Combining the secrets with more shares but one corrupted: call should trigger an error\n");
	ret = local_memset(&secret, 0x00, sizeof(secret)); EG(ret, err);
	shares_[K].raw_share.share[0] = 0x00;
	ret = sss_combine(shares_, K + 1, &secret);
	if (ret) {
		ext_printf("  [X] Error: sss_combine error\n");
	} else {
		buf_print("  secret", (u8*)&secret, SSS_SECRET_SIZE);
	}

	/* Regenerate more shares! call should be OK */
	ext_printf("[+] Regenerating more shares: call should be OK\n");
	ret = local_memset(&secret, 0x00, sizeof(secret)); EG(ret, err);
	ret = sss_regenerate(shares, K, MAX_N, &secret); EG(ret, err);
	if (ret) {
		ext_printf("  [X] Error: sss_regenerate error\n");
		goto err;
	} else {
		buf_print("  secret", (u8*)&secret, SSS_SECRET_SIZE);
	}
	/* Shuffle shares */
	for(i = 0; i < MAX_N; i++){
		shares_[i] = shares[MAX_N - 1 - i];
	}

	/* Combine newly generated shares: call should be OK */
	ext_printf("[+] Combining the secrets with newly generated shares: call should be OK\n");
	ret = local_memset(&secret, 0x00, sizeof(secret)); EG(ret, err);
	ret = sss_combine(shares_, K, &secret);
	if (ret) {
		ext_printf("  [X] Error: sss_combine error\n");
		goto err;
	} else {
		buf_print("  secret", (u8*)&secret, SSS_SECRET_SIZE);
	}

	/* Modify the session ID of one of the shares: call should trigger an error */
	ext_printf("[+] Combining the secrets with newly generated shares and a bad session ID: call should trigger an error\n");
	ret = local_memset(&secret, 0x00, sizeof(secret)); EG(ret, err);
	shares_[1].session_id[0] = 0x00;
	ret = sss_combine(shares_, K, &secret);
	if (ret) {
		ext_printf("  [X] Error: sss_combine error\n");
	} else {
		buf_print("  secret", (u8*)&secret, SSS_SECRET_SIZE);
	}

	ret = 0;

err:
	return ret;
}
#endif
