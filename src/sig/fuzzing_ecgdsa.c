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
#if defined(WITH_SIG_ECGDSA) && defined(USE_CRYPTOFUZZ)

#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul.h>
#include <libecc/nn/nn_logical.h>

#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/ec_key.h>
#include <libecc/utils/utils.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "ECGDSA"
#endif
#include <libecc/utils/dbg_sig.h>

/* NOTE: the following versions of ECGDSA are "raw" with
 * no hash functions and nonce override. They are DANGEROUS and
 * should NOT be used in production mode! They are however useful
 * for corner cases tests and fuzzing.
 */
#define ECGDSA_SIGN_MAGIC ((word_t)(0xe2f60ea3353ecc9eULL))
#define ECGDSA_SIGN_CHECK_INITIALIZED(A, ret, err) \
        MUST_HAVE((((const void *)(A)) != NULL) && \
                  ((A)->magic == ECGDSA_SIGN_MAGIC), ret, err)

int ecgdsa_sign_raw(struct ec_sign_context *ctx, const u8 *input, u8 inputlen, u8 *sig, u8 siglen, const u8 *nonce, u8 noncelen)
{
        nn_src_t q, x;
	/* NOTE: hash here is not really a hash ... */
	u8 e_buf[LOCAL_MIN(255, BIT_LEN_WORDS(NN_MAX_BIT_LEN) * (WORDSIZE / 8))];
        const ec_priv_key *priv_key;
        prj_pt_src_t G;
        u8 hsize, r_len, s_len;
        bitcnt_t q_bit_len, p_bit_len, rshift;
        prj_pt kG;
        int ret, iszero;
        nn tmp, tmp2, s, e, kr, k, r;
#ifdef USE_SIG_BLINDING
        /* b is the blinding mask */
        nn b, binv;
	b.magic = binv.magic = WORD(0);
#endif
	tmp.magic = tmp2.magic = s.magic = e.magic = WORD(0);
	kr.magic = k.magic = r.magic = WORD(0);
	kG.magic = WORD(0);

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an EC-GDSA
	 * signature one and we do not finalize() before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ECGDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.ecgdsa), ret, err);
	MUST_HAVE((sig != NULL) && (input != NULL), ret, err);

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
	hsize = inputlen;

	MUST_HAVE((siglen == ECGDSA_SIGLEN(q_bit_len)), ret, err);

	dbg_nn_print("p", &(priv_key->params->ec_fp.p));
	dbg_nn_print("q", q);
	dbg_priv_key_print("x", priv_key);
	dbg_ec_point_print("G", G);
	dbg_pub_key_print("Y", &(ctx->key_pair->pub_key));

	/* 1. Compute h = H(m) */
        /* NOTE: here we have raw ECGDSA, this is the raw input */
	/* NOTE: the MUST_HAVE is protected by a preprocessing check
	 * to avoid -Werror=type-limits errors:
	 * "error: comparison is always true due to limited range of data type"
	 */
#if LOCAL_MIN(255, BIT_LEN_WORDS(NN_MAX_BIT_LEN) * (WORDSIZE / 8)) < 255
	MUST_HAVE(((u32)inputlen <= sizeof(e_buf)), ret, err);
#endif
        ret = local_memset(e_buf, 0, sizeof(e_buf)); EG(ret, err);
        ret = local_memcpy(e_buf, input, hsize); EG(ret, err);
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
	ret = nn_mod(&tmp2, &tmp, q); EG(ret, err);
	ret = nn_mod_neg(&e, &tmp2, q); EG(ret, err);

/*
     NOTE: the restart label is removed in CRYPTOFUZZ mode as
     we trigger MUST_HAVE instead of restarting in this mode.
 restart:
*/
	/* 3. Get a random value k in ]0,q[ */
        /* NOTE: copy our input nonce if not NULL */
        if(nonce != NULL){
		MUST_HAVE((noncelen <= (u8)(BYTECEIL(q_bit_len))), ret, err);
		ret = nn_init_from_buf(&k, nonce, noncelen); EG(ret, err);
        }
        else{
                ret = ctx->rand(&k, q); EG(ret, err);
        }

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
        /* NOTE: for the CRYPTOFUZZ mode, we do not restart
         * the procedure but throw an assert exception instead.
         */
	ret = nn_iszero(&r, &iszero); EG(ret, err);
        MUST_HAVE((!iszero), ret, err);

	/* Export r */
	ret = nn_export_to_buf(sig, r_len, &r); EG(ret, err);

#ifdef USE_SIG_BLINDING
	/* Blind e and r with b */
	ret = nn_mod_mul(&e, &e, &b, q); EG(ret, err);
	ret = nn_mod_mul(&r, &r, &b, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	/* 7. Compute s = x(kr + e) mod q */
	ret = nn_mod_mul(&kr, &k, &r, q); EG(ret, err);
	ret = nn_mod_add(&tmp2, &kr, &e, q); EG(ret, err);
	ret = nn_mod_mul(&s, x, &tmp2, q); EG(ret, err);
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
        /* NOTE: for the CRYPTOFUZZ mode, we do not restart
         * the procedure but throw an assert exception instead.
         */
	ret = nn_iszero(&s, &iszero); EG(ret, err);
        MUST_HAVE((!iszero), ret, err);

	/* 9. Return (r,s) */
	ret = nn_export_to_buf(sig + r_len, s_len, &s);

 err:
	nn_uninit(&r);
	nn_uninit(&s);
	nn_uninit(&tmp2);
	nn_uninit(&tmp);
	nn_uninit(&e);
	nn_uninit(&kr);
	nn_uninit(&k);

	prj_pt_uninit(&kG);

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

#ifdef USE_SIG_BLINDING
	nn_uninit(&b);
	nn_uninit(&binv);
#endif /* USE_SIG_BLINDING */

	return ret;
}

/******************************/
#define ECGDSA_VERIFY_MAGIC ((word_t)(0xd4da37527288d1b6ULL))
#define ECGDSA_VERIFY_CHECK_INITIALIZED(A, ret, err) \
        MUST_HAVE((((const void *)(A)) != NULL) && \
                  ((A)->magic == ECGDSA_VERIFY_MAGIC), ret, err)

int ecgdsa_verify_raw(struct ec_verify_context *ctx, const u8 *input, u8 inputlen)
{
	nn tmp, e, r_prime, rinv, uv, *r, *s;
	prj_pt uG, vY;
	prj_pt_t Wprime;
	prj_pt_src_t G, Y;
        /* NOTE: hash here is not really a hash ... */
        u8 e_buf[LOCAL_MIN(255, BIT_LEN_WORDS(NN_MAX_BIT_LEN) * (WORDSIZE / 8))];
	nn_src_t q;
	u8 hsize;
        bitcnt_t q_bit_len, rshift;
	int ret, cmp;

	tmp.magic = e.magic = r_prime.magic = rinv.magic = uv.magic = WORD(0);
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
	MUST_HAVE((input != NULL), ret, err);

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
	hsize = inputlen;

	/* 2. Compute h = H(m) */
        /* NOTE: here we have raw ECGDSA, this is the raw input */
	MUST_HAVE((input != NULL), ret, err);
	/* NOTE: the MUST_HAVE is protected by a preprocessing check
	 * to avoid -Werror=type-limits errors:
	 * "error: comparison is always true due to limited range of data type"
	 */
#if LOCAL_MIN(255, BIT_LEN_WORDS(NN_MAX_BIT_LEN) * (WORDSIZE / 8)) < 255
	MUST_HAVE(((u32)inputlen <= sizeof(e_buf)), ret, err);
#endif

        ret = local_memset(e_buf, 0, sizeof(e_buf)); EG(ret, err);
        ret = local_memcpy(e_buf, input, hsize); EG(ret, err);
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

	/* 3. Compute e by converting h to an integer and reducing it mod q */
	ret = nn_mod(&e, &tmp, q); EG(ret, err);

	/* 4. Compute u = (r^-1)e mod q */
	ret = nn_modinv(&rinv, r, q); EG(ret, err); /* r^-1 */
	ret = nn_mul(&tmp, &rinv, &e); EG(ret, err); /* r^-1 * e */
	ret = nn_mod(&uv, &tmp, q); EG(ret, err); /* (r^-1 * e) mod q */
	ret = prj_pt_mul(&uG, &uv, G); EG(ret, err);

	/* 5. Compute v = (r^-1)s mod q */
	ret = nn_mul(&tmp, &rinv, s); EG(ret, err); /*  r^-1 * s */
	ret = nn_mod(&uv, &tmp, q); EG(ret, err); /* (r^-1 * s) mod q */
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
	nn_uninit(&r_prime);
	nn_uninit(&e);
	nn_uninit(&uv);
	nn_uninit(&tmp);
	nn_uninit(&rinv);

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


#else /* WITH_SIG_ECGDSA && USE_CRYPTOFUZZ */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_SIG_ECGDSA */
