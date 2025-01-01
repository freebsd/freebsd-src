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
#if defined(WITH_SIG_ECRDSA) && defined(USE_CRYPTOFUZZ)

#include <libecc/nn/nn_rand.h>
#include <libecc/nn/nn_mul.h>
#include <libecc/nn/nn_logical.h>

#include <libecc/sig/sig_algs_internal.h>
#include <libecc/sig/ec_key.h>
#include <libecc/utils/utils.h>
#ifdef VERBOSE_INNER_VALUES
#define EC_SIG_ALG "ECRDSA"
#endif
#include <libecc/utils/dbg_sig.h>

/* NOTE: the following versions of ECRDSA are "raw" with
 * no hash functions and nonce override. They are DANGEROUS and
 * should NOT be used in production mode! They are however useful
 * for corner cases tests and fuzzing.
 */

/*
 * NOTE: ISO/IEC 14888-3 standard seems to diverge from the existing implementations
 * of ECRDSA when treating the message hash, and from the examples of certificates provided
 * in RFC 7091 and draft-deremin-rfc4491-bis. While in ISO/IEC 14888-3 it is explicitely asked
 * to proceed with the hash of the message as big endian, the RFCs derived from the Russian
 * standard expect the hash value to be treated as little endian when importing it as an integer
 * (this discrepancy is exhibited and confirmed by test vectors present in ISO/IEC 14888-3, and
 * by X.509 certificates present in the RFCs). This seems (to be confirmed) to be a discrepancy of
 * ISO/IEC 14888-3 algorithm description that must be fixed there.
 *
 * In order to be conservative, libecc uses the Russian standard behavior as expected to be in line with
 * other implemetations, but keeps the ISO/IEC 14888-3 behavior if forced/asked by the user using
 * the USE_ISO14888_3_ECRDSA toggle. This allows to keep backward compatibility with previous versions of the
 * library if needed.
 *
 */
#ifndef USE_ISO14888_3_ECRDSA
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
#endif

#define ECRDSA_SIGN_MAGIC ((word_t)(0xcc97bbc8ada8973cULL))
#define ECRDSA_SIGN_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((const void *)(A)) != NULL) && \
		  ((A)->magic == ECRDSA_SIGN_MAGIC), ret, err)

int ecrdsa_sign_raw(struct ec_sign_context *ctx, const u8 *input, u8 inputlen, u8 *sig, u8 siglen, const u8 *nonce, u8 noncelen)
{
	bitcnt_t q_bit_len, p_bit_len;
	const ec_priv_key *priv_key;
        /* NOTE: hash here is not really a hash ... */
        u8 h_buf[LOCAL_MIN(255, BIT_LEN_WORDS(NN_MAX_BIT_LEN) * (WORDSIZE / 8))];
	prj_pt_src_t G;
	prj_pt kG;
	nn_src_t q, x;
	u8 hsize, r_len, s_len;
	int ret, iszero;
	nn tmp, s, rx, ke, k, r, e;
#ifdef USE_SIG_BLINDING
        /* b is the blinding mask */
        nn b, binv;
	b.magic = binv.magic = WORD(0);
#endif /* USE_SIG_BLINDING */

	tmp.magic = s.magic = rx.magic = ke.magic = WORD(0);
	k.magic = r.magic = e.magic = WORD(0);
	kG.magic = WORD(0);

	/*
	 * First, verify context has been initialized and private
	 * part too. This guarantees the context is an EC-RDSA
	 * signature one and we do not finalize() before init().
	 */
	ret = sig_sign_check_initialized(ctx); EG(ret, err);
	ECRDSA_SIGN_CHECK_INITIALIZED(&(ctx->sign_data.ecrdsa), ret, err);

        /* Zero init points */
        ret = local_memset(&kG, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	priv_key = &(ctx->key_pair->priv_key);
	G = &(priv_key->params->ec_gen);
	q = &(priv_key->params->ec_gen_order);
	p_bit_len = priv_key->params->ec_fp.p_bitlen;
	q_bit_len = priv_key->params->ec_gen_order_bitlen;
	x = &(priv_key->x);
	r_len = (u8)ECRDSA_R_LEN(q_bit_len);
	s_len = (u8)ECRDSA_S_LEN(q_bit_len);
	hsize = inputlen;

	MUST_HAVE((NN_MAX_BIT_LEN >= p_bit_len), ret, err);

	MUST_HAVE((siglen == ECRDSA_SIGLEN(q_bit_len)), ret, err);

	dbg_nn_print("p", &(priv_key->params->ec_fp.p));
	dbg_nn_print("q", q);
	dbg_priv_key_print("x", priv_key);
	dbg_pub_key_print("Y", &(ctx->key_pair->pub_key));
	dbg_ec_point_print("G", G);

/*
     NOTE: the restart label is removed in CRYPTOFUZZ mode as
     we trigger MUST_HAVE instead of restarting in this mode.
 restart:
*/
	/* 2. Get a random value k in ]0, q[ ... */
        /* NOTE: copy our input nonce if not NULL */
        if(nonce != NULL){
		MUST_HAVE((noncelen <= (u8)(BYTECEIL(q_bit_len))), ret, err);
		ret = nn_init_from_buf(&k, nonce, noncelen); EG(ret, err);
        }
        else{
                ret = ctx->rand(&k, q); EG(ret, err);
        }

	dbg_nn_print("k", &k);
#ifdef USE_SIG_BLINDING
        /* Note: if we use blinding, k and e are multiplied by
         * a random value b in ]0,q[ */
        ret = nn_get_random_mod(&b, q); EG(ret, err);
        dbg_nn_print("b", &b);
#endif /* USE_SIG_BLINDING */

	/* 3. Compute W = kG = (Wx, Wy) */
#ifdef USE_SIG_BLINDING
        /* We use blinding for the scalar multiplication */
        ret = prj_pt_mul_blind(&kG, &k, G); EG(ret, err);
#else
        ret = prj_pt_mul(&kG, &k, G); EG(ret, err);
#endif /* USE_SIG_BLINDING */
	ret = prj_pt_unique(&kG, &kG); EG(ret, err);
	dbg_nn_print("W_x", &(kG.X.fp_val));
	dbg_nn_print("W_y", &(kG.Y.fp_val));

	/* 4. Compute r = Wx mod q */
	ret = nn_mod(&r, &(kG.X.fp_val), q); EG(ret, err);

	/* 5. If r is 0, restart the process at step 2. */
        /* NOTE: for the CRYPTOFUZZ mode, we do not restart
         * the procedure but throw an assert exception instead.
         */
	ret = nn_iszero(&r, &iszero); EG(ret, err);
        MUST_HAVE((!iszero), ret, err);

	dbg_nn_print("r", &r);

	/* Export r */
	ret = nn_export_to_buf(sig, r_len, &r); EG(ret, err);

	/* 6. Compute e = OS2I(h) mod q. If e is 0, set e to 1. */
        /* NOTE: here we have raw ECRDSA, this is the raw input */
	MUST_HAVE((input != NULL), ret, err);
	/* NOTE: the MUST_HAVE is protected by a preprocessing check
	 * to avoid -Werror=type-limits errors:
	 * "error: comparison is always true due to limited range of data type"
	 */
#if LOCAL_MIN(255, BIT_LEN_WORDS(NN_MAX_BIT_LEN) * (WORDSIZE / 8)) < 255
	MUST_HAVE(((u32)inputlen <= sizeof(h_buf)), ret, err);
#endif
        ret = local_memset(h_buf, 0, sizeof(h_buf)); EG(ret, err);
        ret = local_memcpy(h_buf, input, hsize); EG(ret, err);
	dbg_buf_print("H(m)", h_buf, hsize);
        /* NOTE: this handles a discrepancy between ISO/IEC 14888-3 and
         * Russian standard based RFCs.
         */
#ifndef USE_ISO14888_3_ECRDSA
	ret = _reverse_endianness(h_buf, hsize); EG(ret, err);
#endif

	ret = nn_init_from_buf(&tmp, h_buf, hsize); EG(ret, err);
	ret = local_memset(h_buf, 0, hsize); EG(ret, err);
	ret = nn_mod(&e, &tmp, q); EG(ret, err);
	ret = nn_iszero(&e, &iszero); EG(ret, err);
	if (iszero) {
		ret = nn_inc(&e, &e); EG(ret, err);
	}
	dbg_nn_print("e", &e);

#ifdef USE_SIG_BLINDING
        /* In case of blinding, we blind r and e */
        ret = nn_mod_mul(&r, &r, &b, q); EG(ret, err);
        ret = nn_mod_mul(&e, &e, &b, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */

	/* Compute s = (rx + ke) mod q */
	ret = nn_mod_mul(&rx, &r, x, q); EG(ret, err);
	ret = nn_mod_mul(&ke, &k, &e, q); EG(ret, err);
	ret = nn_mod_add(&s, &rx, &ke, q); EG(ret, err);
#ifdef USE_SIG_BLINDING
	/* Unblind s */
        /* NOTE: we use Fermat's little theorem inversion for
         * constant time here. This is possible since q is prime.
         */
        ret = nn_modinv_fermat(&binv, &b, q); EG(ret, err);
	ret = nn_mod_mul(&s, &s, &binv, q); EG(ret, err);
#endif /* USE_SIG_BLINDING */

	/* If s is 0, restart the process at step 2. */
        /* 10. If s is 0, restart the process at step 4. */
        /* NOTE: for the CRYPTOFUZZ mode, we do not restart
         * the procedure but throw an assert exception instead.
         */
	ret = nn_iszero(&s, &iszero); EG(ret, err);
        MUST_HAVE((!iszero), ret, err);

	dbg_nn_print("s", &s);

	/* Return (r,s) */
	ret = nn_export_to_buf(sig + r_len, s_len, &s); EG(ret, err);

 err:
	nn_uninit(&r);
	nn_uninit(&s);
	nn_uninit(&tmp);
	nn_uninit(&rx);
	nn_uninit(&ke);
	nn_uninit(&k);
	nn_uninit(&e);
	prj_pt_uninit(&kG);

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->sign_data.ecrdsa), 0, sizeof(ecrdsa_sign_data)));
	}

	/* Clean what remains on the stack */
	VAR_ZEROIFY(r_len);
	VAR_ZEROIFY(s_len);
	VAR_ZEROIFY(q_bit_len);
	VAR_ZEROIFY(p_bit_len);
	VAR_ZEROIFY(hsize);
	PTR_NULLIFY(priv_key);
	PTR_NULLIFY(G);
	PTR_NULLIFY(q);
	PTR_NULLIFY(x);

#ifdef USE_SIG_BLINDING
	nn_uninit(&b);
	nn_uninit(&binv);
#endif /* USE_SIG_BLINDING */

	return ret;
}

/******************************/
#define ECRDSA_VERIFY_MAGIC ((word_t)(0xa8e16b7e8180cb9aULL))
#define ECRDSA_VERIFY_CHECK_INITIALIZED(A, ret, err) \
	MUST_HAVE((((const void *)(A)) != NULL) && \
		  ((A)->magic == ECRDSA_VERIFY_MAGIC), ret, err)

int ecrdsa_verify_raw(struct ec_verify_context *ctx, const u8 *input, u8 inputlen)
{
	prj_pt_src_t G, Y;
	nn_src_t q;
	nn tmp, h, r_prime, e, v, u;
	prj_pt vY, uG;
	prj_pt_t Wprime;
        /* NOTE: hash here is not really a hash ... */
        u8 h_buf[LOCAL_MIN(255, BIT_LEN_WORDS(NN_MAX_BIT_LEN) * (WORDSIZE / 8))];
	nn *r, *s;
	u8 hsize;
	int ret, iszero, cmp;

	tmp.magic = h.magic = r_prime.magic = e.magic = WORD(0);
	v.magic = u.magic = WORD(0);
	vY.magic = uG.magic = WORD(0);

	/* NOTE: we reuse uG for Wprime to optimize local variables */
	Wprime = &uG;

	/*
	 * First, verify context has been initialized and public
	 * part too. This guarantees the context is an EC-RDSA
	 * verification one and we do not finalize() before init().
	 */
	ret = sig_verify_check_initialized(ctx); EG(ret, err);
	ECRDSA_VERIFY_CHECK_INITIALIZED(&(ctx->verify_data.ecrdsa), ret, err);

        /* Zero init points */
        ret = local_memset(&uG, 0, sizeof(prj_pt)); EG(ret, err);
        ret = local_memset(&vY, 0, sizeof(prj_pt)); EG(ret, err);

	/* Make things more readable */
	G = &(ctx->pub_key->params->ec_gen);
	Y = &(ctx->pub_key->y);
	q = &(ctx->pub_key->params->ec_gen_order);
	r = &(ctx->verify_data.ecrdsa.r);
	s = &(ctx->verify_data.ecrdsa.s);
	hsize = inputlen;

	/* 2. Compute h = H(m) */
        /* NOTE: here we have raw ECRDSA, this is the raw input */
	MUST_HAVE((input != NULL), ret, err);
	/* NOTE: the MUST_HAVE is protected by a preprocessing check
	 * to avoid -Werror=type-limits errors:
	 * "error: comparison is always true due to limited range of data type"
	 */
#if LOCAL_MIN(255, BIT_LEN_WORDS(NN_MAX_BIT_LEN) * (WORDSIZE / 8)) < 255
	MUST_HAVE(((u32)inputlen <= sizeof(h_buf)), ret, err);
#endif

        ret = local_memset(h_buf, 0, sizeof(h_buf)); EG(ret, err);
        ret = local_memcpy(h_buf, input, hsize); EG(ret, err);
	dbg_buf_print("H(m)", h_buf, hsize);
        /* NOTE: this handles a discrepancy between ISO/IEC 14888-3 and
         * Russian standard based RFCs.
         */
#ifndef USE_ISO14888_3_ECRDSA
        ret = _reverse_endianness(h_buf, hsize); EG(ret, err);
#endif

	/* 3. Compute e = OS2I(h)^-1 mod q */
	ret = nn_init_from_buf(&tmp, h_buf, hsize); EG(ret, err);
	ret = local_memset(h_buf, 0, hsize); EG(ret, err);
	ret = nn_mod(&h, &tmp, q); EG(ret, err); /* h = OS2I(h) mod q */
	ret = nn_iszero(&h, &iszero); EG(ret, err);
	if (iszero) {	/* If h is equal to 0, set it to 1 */
		ret = nn_inc(&h, &h); EG(ret, err);
	}
	ret = nn_modinv(&e, &h, q); EG(ret, err); /* e = h^-1 mod q */

	/* 4. Compute u = es mod q */
	ret = nn_mul(&tmp, &e, s); EG(ret, err);
	ret = nn_mod(&u, &tmp, q); EG(ret, err);

	/* 5. Compute v = -er mod q
	 *
	 * Because we only support positive integers, we compute
	 * v = -er mod q = q - (er mod q) (except when er is 0).
	 */
	ret = nn_mul(&tmp, &e, r); EG(ret, err); /* tmp = er */
	ret = nn_mod(&tmp, &tmp, q); EG(ret, err); /* tmp = er mod q */
	ret = nn_mod_neg(&v, &tmp, q); EG(ret, err); /* negate tmp */

	/* 6. Compute W' = uG + vY = (W'_x, W'_y) */
	ret = prj_pt_mul(&uG, &u, G); EG(ret, err);
	ret = prj_pt_mul(&vY, &v, Y); EG(ret, err);
	ret = prj_pt_add(Wprime, &uG, &vY); EG(ret, err);
	ret = prj_pt_unique(Wprime, Wprime); EG(ret, err);
	dbg_nn_print("W'_x", &(Wprime->X.fp_val));
	dbg_nn_print("W'_y", &(Wprime->Y.fp_val));

	/* 7. Compute r' = W'_x mod q */
	ret = nn_mod(&r_prime, &(Wprime->X.fp_val), q); EG(ret, err);

	/* 8. Check r and r' are the same */
	ret = nn_cmp(r, &r_prime, &cmp); EG(ret, err);
	ret = (cmp == 0) ? 0 : -1;

err:
	nn_uninit(&r_prime);
	nn_uninit(&tmp);
	nn_uninit(&h);
	nn_uninit(&e);
	nn_uninit(&u);
	nn_uninit(&v);
	prj_pt_uninit(&vY);
	prj_pt_uninit(&uG);

	/*
	 * We can now clear data part of the context. This will clear
	 * magic and avoid further reuse of the whole context.
	 */
	if(ctx != NULL){
		IGNORE_RET_VAL(local_memset(&(ctx->verify_data.ecrdsa), 0,
			     sizeof(ecrdsa_verify_data)));
	}

	/* Clean what remains on the stack */
	PTR_NULLIFY(Wprime);
	PTR_NULLIFY(G);
	PTR_NULLIFY(Y);
	PTR_NULLIFY(q);
	PTR_NULLIFY(r);
	PTR_NULLIFY(s);
	VAR_ZEROIFY(hsize);

	return ret;
}

#else /* WITH_SIG_ECRDSA && USE_CRYPTOFUZZ */

/*
 * Dummy definition to avoid the empty translation unit ISO C warning
 */
typedef int dummy;
#endif /* WITH_SIG_ECRDSA */
