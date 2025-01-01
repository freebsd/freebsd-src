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
#include <libecc/nn/nn_modinv.h>
#include <libecc/nn/nn_div_public.h>
#include <libecc/nn/nn_logical.h>
#include <libecc/nn/nn_add.h>
#include <libecc/nn/nn_mod_pow.h>
#include <libecc/nn/nn.h>
/* Include the "internal" header as we use non public API here */
#include "../nn/nn_mul.h"

/*
 * Compute out = x^-1 mod m, i.e. out such that (out * x) = 1 mod m
 * out is initialized by the function, i.e. caller needs not initialize
 * it; only provide the associated storage space. Done in *constant
 * time* if underlying routines are.
 *
 * Asserts that m is odd and that x is smaller than m.
 * This second condition is not strictly necessary,
 * but it allows to perform all operations on nn's of the same length,
 * namely the length of m.
 *
 * Uses a binary xgcd algorithm,
 * only keeps track of coefficient for inverting x,
 * and performs reduction modulo m at each step.
 *
 * This does not normalize out on return.
 *
 * 0 is returned on success (everything went ok and x has reciprocal), -1
 * on error or if x has no reciprocal. On error, out is not meaningful.
 *
 * The function is an internal helper: caller MUST check params have been
 * initialized, i.e. this is not done by the function.
 *
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_modinv_odd(nn_t out, nn_src_t x, nn_src_t m)
{
	int isodd, swap, smaller, ret, cmp, iszero, tmp_isodd;
	nn a, b, u, tmp, mp1d2;
	nn_t uu = out;
	bitcnt_t cnt;
	a.magic = b.magic = u.magic = tmp.magic = mp1d2.magic = WORD(0);

	ret = nn_init(out, 0); EG(ret, err);
	ret = nn_init(&a, (u16)(m->wlen * WORD_BYTES)); EG(ret, err);
	ret = nn_init(&b, (u16)(m->wlen * WORD_BYTES)); EG(ret, err);
	ret = nn_init(&u, (u16)(m->wlen * WORD_BYTES)); EG(ret, err);
	ret = nn_init(&mp1d2, (u16)(m->wlen * WORD_BYTES)); EG(ret, err);
	/*
	 * Temporary space needed to only deal with positive stuff.
	 */
	ret = nn_init(&tmp, (u16)(m->wlen * WORD_BYTES)); EG(ret, err);

	MUST_HAVE((!nn_isodd(m, &isodd)) && isodd, ret, err);
	MUST_HAVE((!nn_cmp(x, m, &cmp)) && (cmp < 0), ret, err);
	MUST_HAVE((!nn_iszero(x, &iszero)) && (!iszero), ret, err);

	/*
	 * Maintain:
	 *
	 * a = u * x (mod m)
	 * b = uu * x (mod m)
	 *
	 * and b odd at all times. Initially,
	 *
	 * a = x, u = 1
	 * b = m, uu = 0
	 */
	ret = nn_copy(&a, x); EG(ret, err);
	ret = nn_set_wlen(&a, m->wlen); EG(ret, err);
	ret = nn_copy(&b, m); EG(ret, err);
	ret = nn_one(&u); EG(ret, err);
	ret = nn_zero(uu); EG(ret, err);

	/*
	 * The lengths of u and uu should not affect constant timeness but it
	 * does not hurt to set them already.
	 * They will always be strictly smaller than m.
	 */
	ret = nn_set_wlen(&u, m->wlen); EG(ret, err);
	ret = nn_set_wlen(uu, m->wlen); EG(ret, err);

	/*
	 * Precompute inverse of 2 mod m:
	 *	2^-1 = (m+1)/2
	 * computed as (m >> 1) + 1.
	 */
	ret = nn_rshift_fixedlen(&mp1d2, m, 1); EG(ret, err);

	ret = nn_inc(&mp1d2, &mp1d2); EG(ret, err); /* no carry can occur here
						       because of prev. shift */

	cnt = (bitcnt_t)((a.wlen + b.wlen) * WORD_BITS);
	while (cnt > 0) {
		cnt = (bitcnt_t)(cnt - 1);
		/*
		 * Always maintain b odd. The logic of the iteration is as
		 * follows.
		 */

		/*
		 * For a, b:
		 *
		 * odd = a & 1
		 * swap = odd & (a < b)
		 * if (swap)
		 *      swap(a, b)
		 * if (odd)
		 *      a -= b
		 * a /= 2
		 */

		MUST_HAVE((!nn_isodd(&b, &tmp_isodd)) && tmp_isodd, ret, err);

		ret = nn_isodd(&a, &isodd); EG(ret, err);
		ret = nn_cmp(&a, &b, &cmp); EG(ret, err);
		swap = isodd & (cmp == -1);

		ret = nn_cnd_swap(swap, &a, &b); EG(ret, err);
		ret = nn_cnd_sub(isodd, &a, &a, &b); EG(ret, err);

		MUST_HAVE((!nn_isodd(&a, &tmp_isodd)) && (!tmp_isodd), ret, err); /* a is now even */

		ret = nn_rshift_fixedlen(&a, &a, 1);  EG(ret, err);/* division by 2 */

		/*
		 * For u, uu:
		 *
		 * if (swap)
		 *      swap u, uu
		 * smaller = (u < uu)
		 * if (odd)
		 *      if (smaller)
		 *              u += m - uu
		 *      else
		 *              u -= uu
		 * u >>= 1
		 * if (u was odd)
		 *      u += (m+1)/2
		 */
		ret = nn_cnd_swap(swap, &u, uu); EG(ret, err);

		/* This parameter is used to avoid handling negative numbers. */
		ret = nn_cmp(&u, uu, &cmp); EG(ret, err);
		smaller = (cmp == -1);

		/* Computation of 'm - uu' can always be performed. */
		ret = nn_sub(&tmp, m, uu); EG(ret, err);

		/* Selection btw 'm-uu' and '-uu' is made by the following function calls. */
		ret = nn_cnd_add(isodd & smaller, &u, &u, &tmp); EG(ret, err); /* no carry can occur as 'u+(m-uu) = m-(uu-u) < m' */
		ret = nn_cnd_sub(isodd & (!smaller), &u, &u, uu); EG(ret, err);

		/* Divide u by 2 */
		ret = nn_isodd(&u, &isodd); EG(ret, err);
		ret = nn_rshift_fixedlen(&u, &u, 1); EG(ret, err);
		ret = nn_cnd_add(isodd, &u, &u, &mp1d2); EG(ret, err); /* no carry can occur as u=1+u' with u'<m-1 and u' even so u'/2+(m+1)/2<(m-1)/2+(m+1)/2=m */

		MUST_HAVE((!nn_cmp(&u, m, &cmp)) && (cmp < 0), ret, err);
		MUST_HAVE((!nn_cmp(uu, m, &cmp)) && (cmp < 0), ret, err);

		/*
		 * As long as a > 0, the quantity
		 * (bitsize of a) + (bitsize of b)
		 * is reduced by at least one bit per iteration,
		 * hence after (bitsize of x) + (bitsize of m) - 1
		 * iterations we surely have a = 0. Then b = gcd(x, m)
		 * and if b = 1 then also uu = x^{-1} (mod m).
		 */
	}

	MUST_HAVE((!nn_iszero(&a, &iszero)) && iszero, ret, err);

	/* Check that gcd is one. */
	ret = nn_cmp_word(&b, WORD(1), &cmp); EG(ret, err);

	/* If not, set "inverse" to zero. */
	ret = nn_cnd_sub(cmp != 0, uu, uu, uu); EG(ret, err);

	ret = cmp ? -1 : 0;

err:
	nn_uninit(&a);
	nn_uninit(&b);
	nn_uninit(&u);
	nn_uninit(&mp1d2);
	nn_uninit(&tmp);

	PTR_NULLIFY(uu);

	return ret;
}

/*
 * Same as above without restriction on m.
 * No attempt to make it constant time.
 * Uses the above constant-time binary xgcd when m is odd
 * and a not constant time plain Euclidean xgcd when m is even.
 *
 * _out parameter need not be initialized; this will be done by the function.
 * x and m must be initialized nn.
 *
 * Return -1 on error or if if x has no reciprocal modulo m. out is zeroed.
 * Return  0 if x has reciprocal modulo m.
 *
 * The function supports aliasing.
 */
int nn_modinv(nn_t _out, nn_src_t x, nn_src_t m)
{
	int sign, ret, cmp, isodd, isone;
	nn_t x_mod_m;
	nn u, v, out; /* Out to support aliasing */
	out.magic = u.magic = v.magic = WORD(0);

	ret = nn_check_initialized(x); EG(ret, err);
	ret = nn_check_initialized(m); EG(ret, err);

	/* Initialize out */
	ret = nn_init(&out, 0); EG(ret, err);
	ret = nn_isodd(m, &isodd); EG(ret, err);
	if (isodd) {
		ret = nn_cmp(x, m, &cmp); EG(ret, err);
		if (cmp >= 0) {
			/*
			 * If x >= m, (x^-1) mod m = ((x mod m)^-1) mod m
			 * Hence, compute x mod m. In order to avoid
			 * additional stack usage, we use 'u' (not
			 * already useful at that point in the function).
			 */
			x_mod_m = &u;
			ret = nn_mod(x_mod_m, x, m); EG(ret, err);
			ret = _nn_modinv_odd(&out, x_mod_m, m); EG(ret, err);
		} else {
			ret = _nn_modinv_odd(&out, x, m); EG(ret, err);
		}
		ret = nn_copy(_out, &out);
		goto err;
	}

	/* Now m is even */
	ret = nn_isodd(x, &isodd); EG(ret, err);
	MUST_HAVE(isodd, ret, err);

	ret = nn_init(&u, 0); EG(ret, err);
	ret = nn_init(&v, 0); EG(ret, err);
	ret = nn_xgcd(&out, &u, &v, x, m, &sign); EG(ret, err);
	ret = nn_isone(&out, &isone); EG(ret, err);
	MUST_HAVE(isone, ret, err);

	ret = nn_mod(&out, &u, m); EG(ret, err);
	if (sign == -1) {
		ret = nn_sub(&out, m, &out); EG(ret, err);
	}
	ret = nn_copy(_out, &out);

err:
	nn_uninit(&out);
	nn_uninit(&u);
	nn_uninit(&v);

	PTR_NULLIFY(x_mod_m);

	return ret;
}

/*
 * Compute (A - B) % 2^(storagebitsizeof(B) + 1). A and B must be initialized nn.
 * the function is an internal helper and does not verify params have been
 * initialized; this must be done by the caller. No assumption on A and B values
 * such as A >= B. Done in *constant time. Returns 0 on success, -1 on error.
 */
ATTRIBUTE_WARN_UNUSED_RET static inline int _nn_sub_mod_2exp(nn_t A, nn_src_t B)
{
	u8 Awlen = A->wlen;
	int ret;

	ret = nn_set_wlen(A, (u8)(Awlen + 1)); EG(ret, err);

	/* Make sure A > B */
	/* NOTE: A->wlen - 1 is not an issue here thant to the nn_set_wlen above */
	A->val[A->wlen - 1] = WORD(1);
	ret = nn_sub(A, A, B); EG(ret, err);

	/* The artificial word will be cleared in the following function call */
	ret = nn_set_wlen(A, Awlen);

err:
	return ret;
}

/*
 * Invert x modulo 2^exp using Hensel lifting. Returns 0 on success, -1 on
 * error. On success, x_isodd is 1 if x is odd, 0 if it is even.
 * Please note that the result is correct (inverse of x) only when x is prime
 * to 2^exp, i.e. x is odd (x_odd is 1).
 *
 * Operations are done in *constant time*.
 *
 * Aliasing is supported.
 */
int nn_modinv_2exp(nn_t _out, nn_src_t x, bitcnt_t exp, int *x_isodd)
{
	bitcnt_t cnt;
	u8 exp_wlen = (u8)BIT_LEN_WORDS(exp);
	bitcnt_t exp_cnt = exp % WORD_BITS;
	word_t mask = (word_t)((exp_cnt == 0) ? WORD_MASK : (word_t)((WORD(1) << exp_cnt) - WORD(1)));
	nn tmp_sqr, tmp_mul;
	/* for aliasing */
	int isodd, ret;
	nn out;
	out.magic = tmp_sqr.magic = tmp_mul.magic = WORD(0);

	MUST_HAVE((x_isodd != NULL), ret, err);
	ret = nn_check_initialized(x); EG(ret, err);
	ret = nn_check_initialized(_out); EG(ret, err);

	ret = nn_init(&out, 0); EG(ret, err);
	ret = nn_init(&tmp_sqr, 0); EG(ret, err);
	ret = nn_init(&tmp_mul, 0); EG(ret, err);
	ret = nn_isodd(x, &isodd); EG(ret, err);
	if (exp == (bitcnt_t)0){
		/* Specific case of zero exponent, output 0 */
		(*x_isodd) = isodd;
		goto err;
	}
	if (!isodd) {
		ret = nn_zero(_out); EG(ret, err);
		(*x_isodd) = 0;
		goto err;
	}

	/*
	 * Inverse modulo 2.
	 */
	cnt = 1;
	ret = nn_one(&out); EG(ret, err);

	/*
	 * Inverse modulo 2^(2^i) <= 2^WORD_BITS.
	 * Assumes WORD_BITS is a power of two.
	 */
	for (; cnt < WORD_MIN(WORD_BITS, exp); cnt = (bitcnt_t)(cnt << 1)) {
		ret = nn_sqr_low(&tmp_sqr, &out, out.wlen); EG(ret, err);
		ret = nn_mul_low(&tmp_mul, &tmp_sqr, x, out.wlen); EG(ret, err);
		ret = nn_lshift_fixedlen(&out, &out, 1); EG(ret, err);

		/*
		 * Allowing "negative" results for a subtraction modulo
		 * a power of two would allow to use directly:
		 * nn_sub(out, out, tmp_mul)
		 * which is always negative in ZZ except when x is one.
		 *
		 * Another solution is to add the opposite of tmp_mul.
		 * nn_modopp_2exp(tmp_mul, tmp_mul);
		 * nn_add(out, out, tmp_mul);
		 *
		 * The current solution is to add a sufficiently large power
		 * of two to out unconditionally to absorb the potential
		 * borrow. The result modulo 2^(2^i) is correct whether the
		 * borrow occurs or not.
		 */
		ret = _nn_sub_mod_2exp(&out, &tmp_mul); EG(ret, err);
	}

	/*
	 * Inverse modulo 2^WORD_BITS < 2^(2^i) < 2^exp.
	 */
	for (; cnt < ((exp + 1) >> 1); cnt = (bitcnt_t)(cnt << 1)) {
		ret = nn_set_wlen(&out, (u8)(2 * out.wlen)); EG(ret, err);
		ret = nn_sqr_low(&tmp_sqr, &out, out.wlen); EG(ret, err);
		ret = nn_mul_low(&tmp_mul, &tmp_sqr, x, out.wlen); EG(ret, err);
		ret = nn_lshift_fixedlen(&out, &out, 1); EG(ret, err);
		ret = _nn_sub_mod_2exp(&out, &tmp_mul); EG(ret, err);
	}

	/*
	 * Inverse modulo 2^(2^i + j) >= 2^exp.
	 */
	if (exp > WORD_BITS) {
		ret = nn_set_wlen(&out, exp_wlen); EG(ret, err);
		ret = nn_sqr_low(&tmp_sqr, &out, out.wlen); EG(ret, err);
		ret = nn_mul_low(&tmp_mul, &tmp_sqr, x, out.wlen); EG(ret, err);
		ret = nn_lshift_fixedlen(&out, &out, 1); EG(ret, err);
		ret = _nn_sub_mod_2exp(&out, &tmp_mul); EG(ret, err);
	}

	/*
	 * Inverse modulo 2^exp.
	 */
	out.val[exp_wlen - 1] &= mask;

	ret = nn_copy(_out, &out); EG(ret, err);

	(*x_isodd) = 1;

err:
	nn_uninit(&out);
	nn_uninit(&tmp_sqr);
	nn_uninit(&tmp_mul);

	return ret;
}

/*
 * Invert word w modulo m.
 *
 * The function supports aliasing.
 */
int nn_modinv_word(nn_t out, word_t w, nn_src_t m)
{
	nn nn_tmp;
	int ret;
	nn_tmp.magic = WORD(0);

	ret = nn_init(&nn_tmp, 0); EG(ret, err);
	ret = nn_set_word_value(&nn_tmp, w); EG(ret, err);
	ret = nn_modinv(out, &nn_tmp, m);

err:
	nn_uninit(&nn_tmp);

	return ret;
}


/*
 * Internal function for nn_modinv_fermat and nn_modinv_fermat_redc used
 * hereafter.
 */
ATTRIBUTE_WARN_UNUSED_RET static int _nn_modinv_fermat_common(nn_t out, nn_src_t x, nn_src_t p, nn_t p_minus_two, int *lesstwo)
{
	int ret, cmp, isodd;
	nn two;
	two.magic = WORD(0);

	/* Sanity checks on inputs */
	ret = nn_check_initialized(x); EG(ret, err);
	ret = nn_check_initialized(p); EG(ret, err);
	/* NOTE: since this is an internal function, we are ensured that p_minus_two,
	 * two and regular are OK.
	 */

	/* 0 is not invertible in any case */
	ret = nn_iszero(x, &cmp); EG(ret, err);
	if(cmp){
		/* Zero the output and return an error */
		ret = nn_init(out, 0); EG(ret, err);
		ret = nn_zero(out); EG(ret, err);
		ret = -1;
		goto err;
	}

	/* For p <= 2, p being prime either p = 1 or p = 2.
	 * When p = 2, only 1 has an inverse, if p = 1 no one has an inverse.
	 */
	(*lesstwo) = 0;
	ret = nn_cmp_word(p, WORD(2), &cmp); EG(ret, err);
        if(cmp == 0){
		/* This is the p = 2 case, parity of x provides the result */
		ret = nn_isodd(x, &isodd); EG(ret, err);
		if(isodd){
			/* x is odd, 1 is its inverse */
			ret = nn_init(out, 0); EG(ret, err);
			ret = nn_one(out); EG(ret, err);
			ret = 0;
		}
		else{
			/* x is even, no inverse. Zero the output */
			ret = nn_init(out, 0); EG(ret, err);
			ret = nn_zero(out); EG(ret, err);
			ret = -1;
		}
		(*lesstwo) = 1;
		goto err;
        } else if (cmp < 0){
		/* This is the p = 1 case, no inverse here: hence return an error */
		/* Zero the output */
		ret = nn_init(out, 0); EG(ret, err);
		ret = nn_zero(out); EG(ret, err);
		ret = -1;
		(*lesstwo) = 1;
		goto err;
	}

	/* Else we compute (p-2) for the upper layer */
	if(p != p_minus_two){
		/* Handle aliasing of p and p_minus_two */
		ret = nn_init(p_minus_two, 0); EG(ret, err);
	}

	ret = nn_init(&two, 0); EG(ret, err);
	ret = nn_set_word_value(&two, WORD(2)); EG(ret, err);
	ret = nn_sub(p_minus_two, p, &two);

err:
	nn_uninit(&two);

	return ret;
}

/*
 * Invert NN x modulo p using Fermat's little theorem for our inversion:
 *
 *    p prime means that:
 *    x^(p-1) = 1 mod (p)
 *    which means that x^(p-2) mod(p) is the modular inverse of x mod (p)
 *    for x != 0
 *
 * NOTE: the input hypothesis is that p is prime.
 * XXX WARNING: using this function with p not prime will produce wrong
 * results without triggering an error!
 *
 * The function returns 0 on success, -1 on error
 * (e.g. if x has no inverse modulo p, i.e. x = 0).
 *
 * Aliasing is supported.
 */
int nn_modinv_fermat(nn_t out, nn_src_t x, nn_src_t p)
{
	int ret, lesstwo;
	nn p_minus_two;
	p_minus_two.magic = WORD(0);

	/* Call our helper.
	 * NOTE: "marginal" cases where x = 0 and p <= 2 should be caught in this helper.
	 */
	ret = _nn_modinv_fermat_common(out, x, p, &p_minus_two, &lesstwo); EG(ret, err);

	if(!lesstwo){
		/* Compute x^(p-2) mod (p) */
		ret = nn_mod_pow(out, x, &p_minus_two, p);
	}

err:
	nn_uninit(&p_minus_two);

	return ret;
}

/*
 * Invert NN x modulo m using Fermat's little theorem for our inversion.
 *
 * This is a version with already (pre)computed Montgomery coefficients.
 *
 * NOTE: the input hypothesis is that p is prime.
 * XXX WARNING: using this function with p not prime will produce wrong
 * results without triggering an error!
 *
 * The function returns 0 on success, -1 on error
 * (e.g. if x has no inverse modulo p, i.e. x = 0).
 *
 * Aliasing is supported.
 */
int nn_modinv_fermat_redc(nn_t out, nn_src_t x, nn_src_t p, nn_src_t r, nn_src_t r_square, word_t mpinv)
{
	int ret, lesstwo;
	nn p_minus_two;
	p_minus_two.magic = WORD(0);

	/* Call our helper.
	 * NOTE: "marginal" cases where x = 0 and p <= 2 should be caught in this helper.
	 */
	ret = _nn_modinv_fermat_common(out, x, p, &p_minus_two, &lesstwo); EG(ret, err);

	if(!lesstwo){
		/* Compute x^(p-2) mod (p) using precomputed Montgomery coefficients as input */
		ret = nn_mod_pow_redc(out, x, &p_minus_two, p, r, r_square, mpinv);
	}

err:
	nn_uninit(&p_minus_two);

	return ret;
}
