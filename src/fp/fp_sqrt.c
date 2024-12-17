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
#include <libecc/fp/fp_sqrt.h>
#include <libecc/nn/nn_add.h>
#include <libecc/nn/nn_logical.h>

/*
 * Compute the legendre symbol of an element of Fp:
 *
 *   Legendre(a) = a^((p-1)/2) (p) = { -1, 0, 1 }
 *
 */
ATTRIBUTE_WARN_UNUSED_RET static int legendre(fp_src_t a)
{
	int ret, iszero, cmp;
	fp pow; /* The result if the exponentiation is in Fp */
	fp one; /* The element 1 in the field */
	nn exp; /* The power exponent is in NN */
	pow.magic = one.magic = WORD(0);
	exp.magic = WORD(0);

	/* Initialize elements */
	ret = fp_check_initialized(a); EG(ret, err);
	ret = fp_init(&pow, a->ctx); EG(ret, err);
	ret = fp_init(&one, a->ctx); EG(ret, err);
	ret = nn_init(&exp, 0); EG(ret, err);

	/* Initialize our variables from the Fp context of the
	 * input a.
	 */
	ret = fp_init(&pow, a->ctx); EG(ret, err);
	ret = fp_init(&one, a->ctx); EG(ret, err);
	ret = nn_init(&exp, 0); EG(ret, err);

	/* one = 1 in Fp */
	ret = fp_one(&one); EG(ret, err);

	/* Compute the exponent (p-1)/2
	 * The computation is done in NN, and the division by 2
	 * is performed using a right shift by one
	 */
	ret = nn_dec(&exp, &(a->ctx->p)); EG(ret, err);
	ret = nn_rshift(&exp, &exp, 1); EG(ret, err);

	/* Compute a^((p-1)/2) in Fp using our fp_pow
	 * API.
	 */
	ret = fp_pow(&pow, a, &exp); EG(ret, err);

	ret = fp_iszero(&pow, &iszero); EG(ret, err);
	ret = fp_cmp(&pow, &one, &cmp); EG(ret, err);
	if (iszero) {
		ret = 0;
	} else if (cmp == 0) {
		ret = 1;
	} else {
		ret = -1;
	}

err:
	/* Cleaning */
	fp_uninit(&pow);
	fp_uninit(&one);
	nn_uninit(&exp);

	return ret;
}

/*
 * We implement the Tonelli-Shanks algorithm for finding
 * square roots (quadratic residues) modulo a prime number,
 * i.e. solving the equation:
 *     x^2 = n (p)
 * where p is a prime number. This can be seen as an equation
 * over the finite field Fp where a and x are elements of
 * this finite field.
 *   Source: https://en.wikipedia.org/wiki/Tonelli%E2%80%93Shanks_algorithm
 *   All   ≡   are taken to mean   (mod p)   unless stated otherwise.
 *   Input : p an odd prime, and an integer n .
 *       Step 0. Check that n is indeed a square  : (n | p) must be ≡ 1
 *       Step 1. [Factors out powers of 2 from p-1] Define q -odd- and s such as p-1 = q * 2^s
 *           - if s = 1 , i.e p ≡ 3 (mod 4) , output the two solutions r ≡ +/- n^((p+1)/4) .
 *       Step 2. Select a non-square z such as (z | p) = -1 , and set c ≡ z^q .
 *       Step 3. Set r ≡ n ^((q+1)/2) , t ≡ n^q, m = s .
 *       Step 4. Loop.
 *           - if t ≡ 1 output r, p-r .
 *           - Otherwise find, by repeated squaring, the lowest i , 0 < i < m , such as t^(2^i) ≡ 1
 *           - Let b ≡ c^(2^(m-i-1)), and set r ≡ r*b, t ≡ t*b^2 , c ≡ b^2 and m = i.
 *
 * NOTE: the algorithm is NOT constant time.
 *
 * The outputs, sqrt1 and sqrt2 ARE initialized by the function.
 * The function returns 0 on success, -1 on error (in which case values of sqrt1 and sqrt2
 * must not be considered).
 *
 * Aliasing is supported.
 * 
 */
int fp_sqrt(fp_t sqrt1, fp_t sqrt2, fp_src_t n)
{
	int ret, iszero, cmp, isodd;
	nn q, s, one_nn, two_nn, m, i, tmp_nn;
	fp z, t, b, r, c, one_fp, tmp_fp, __n;
	fp_t _n = &__n;
	q.magic = s.magic = one_nn.magic = two_nn.magic = m.magic = WORD(0);
	i.magic = tmp_nn.magic = z.magic = t.magic = b.magic = WORD(0);
	r.magic = c.magic = one_fp.magic = tmp_fp.magic = __n.magic = WORD(0);

	ret = nn_init(&q, 0); EG(ret, err);
	ret = nn_init(&s, 0); EG(ret, err);
	ret = nn_init(&tmp_nn, 0); EG(ret, err);
	ret = nn_init(&one_nn, 0); EG(ret, err);
	ret = nn_init(&two_nn, 0); EG(ret, err);
	ret = nn_init(&m, 0); EG(ret, err);
	ret = nn_init(&i, 0); EG(ret, err);
	ret = fp_init(&z, n->ctx); EG(ret, err);
	ret = fp_init(&t, n->ctx); EG(ret, err);
	ret = fp_init(&b, n->ctx); EG(ret, err);
	ret = fp_init(&r, n->ctx); EG(ret, err);
	ret = fp_init(&c, n->ctx); EG(ret, err);
	ret = fp_init(&one_fp, n->ctx); EG(ret, err);
	ret = fp_init(&tmp_fp, n->ctx); EG(ret, err);

	/* Handle input aliasing */
	ret = fp_copy(_n, n); EG(ret, err);

	/* Initialize outputs */
	ret = fp_init(sqrt1, _n->ctx); EG(ret, err);
	ret = fp_init(sqrt2, _n->ctx); EG(ret, err);

	/* one_nn = 1 in NN */
	ret = nn_one(&one_nn); EG(ret, err);
	/* two_nn = 2 in NN */
	ret = nn_set_word_value(&two_nn, WORD(2)); EG(ret, err);

	/* If our p prime of Fp is 2, then return the input as square roots */
	ret = nn_cmp(&(_n->ctx->p), &two_nn, &cmp); EG(ret, err);
	if (cmp == 0) {
		ret = fp_copy(sqrt1, _n); EG(ret, err);
		ret = fp_copy(sqrt2, _n); EG(ret, err);
		ret = 0;
		goto err;
	}

	/* Square root of 0 is 0 */
	ret = fp_iszero(_n, &iszero); EG(ret, err);
	if (iszero) {
		ret = fp_zero(sqrt1); EG(ret, err);
		ret = fp_zero(sqrt2); EG(ret, err);
		ret = 0;
		goto err;
	}
	/* Step 0. Check that n is indeed a square  : (n | p) must be ≡ 1 */
	if (legendre(_n) != 1) {
		/* a is not a square */
		ret = -1;
		goto err;
	}
	/* Step 1. [Factors out powers of 2 from p-1] Define q -odd- and s such as p-1 = q * 2^s */
	/* s = 0 */
	ret = nn_zero(&s); EG(ret, err);
	/* q = p - 1 */
	ret = nn_copy(&q, &(_n->ctx->p)); EG(ret, err);
	ret = nn_dec(&q, &q); EG(ret, err);
	while (1) {
		/* i is used as a temporary unused variable here */
		ret = nn_divrem(&tmp_nn, &i, &q, &two_nn); EG(ret, err);
		ret = nn_inc(&s, &s); EG(ret, err);
		ret = nn_copy(&q, &tmp_nn); EG(ret, err);
		/* If r is odd, we have finished our division */
		ret = nn_isodd(&q, &isodd); EG(ret, err);
		if (isodd) {
			break;
		}
	}
	/* - if s = 1 , i.e p ≡ 3 (mod 4) , output the two solutions r ≡ +/- n^((p+1)/4) . */
	ret = nn_cmp(&s, &one_nn, &cmp); EG(ret, err);
	if (cmp == 0) {
		ret = nn_inc(&tmp_nn, &(_n->ctx->p)); EG(ret, err);
		ret = nn_rshift(&tmp_nn, &tmp_nn, 2); EG(ret, err);
		ret = fp_pow(sqrt1, _n, &tmp_nn); EG(ret, err);
		ret = fp_neg(sqrt2, sqrt1); EG(ret, err);
		ret = 0;
		goto err;
	}
	/* Step 2. Select a non-square z such as (z | p) = -1 , and set c ≡ z^q . */
	ret = fp_zero(&z); EG(ret, err);
	while (legendre(&z) != -1) {
		ret = fp_inc(&z, &z); EG(ret, err);
	}
	ret = fp_pow(&c, &z, &q); EG(ret, err);
	/* Step 3. Set r ≡ n ^((q+1)/2) , t ≡ n^q, m = s . */
	ret = nn_inc(&tmp_nn, &q); EG(ret, err);
	ret = nn_rshift(&tmp_nn, &tmp_nn, 1); EG(ret, err);
	ret = fp_pow(&r, _n, &tmp_nn); EG(ret, err);
	ret = fp_pow(&t, _n, &q); EG(ret, err);
	ret = nn_copy(&m, &s); EG(ret, err);
	ret = fp_one(&one_fp); EG(ret, err);

	/* Step 4. Loop. */
	while (1) {
		/* - if t ≡ 1 output r, p-r . */
		ret = fp_cmp(&t, &one_fp, &cmp); EG(ret, err);
		if (cmp == 0) {
			ret = fp_copy(sqrt1, &r); EG(ret, err);
			ret = fp_neg(sqrt2, sqrt1); EG(ret, err);
			ret = 0;
			goto err;
		}
		/* - Otherwise find, by repeated squaring, the lowest i , 0 < i < m , such as t^(2^i) ≡ 1 */
		ret = nn_one(&i); EG(ret, err);
		ret = fp_copy(&tmp_fp, &t); EG(ret, err);
		while (1) {
			ret = fp_sqr(&tmp_fp, &tmp_fp); EG(ret, err);
			ret = fp_cmp(&tmp_fp, &one_fp, &cmp); EG(ret, err);
			if (cmp == 0) {
				break;
			}
			ret = nn_inc(&i, &i); EG(ret, err);
			ret = nn_cmp(&i, &m, &cmp); EG(ret, err);
			if (cmp == 0) {
				/* i has reached m, that should not happen ... */
				ret = -2;
				goto err;
			}
		}
		/* - Let b ≡ c^(2^(m-i-1)), and set r ≡ r*b, t ≡ t*b^2 , c ≡ b^2 and m = i. */
		ret = nn_sub(&tmp_nn, &m, &i); EG(ret, err);
		ret = nn_dec(&tmp_nn, &tmp_nn); EG(ret, err);
		ret = fp_copy(&b, &c); EG(ret, err);
		ret = nn_iszero(&tmp_nn, &iszero); EG(ret, err);
		while (!iszero) {
			ret = fp_sqr(&b, &b); EG(ret, err);
			ret = nn_dec(&tmp_nn, &tmp_nn); EG(ret, err);
			ret = nn_iszero(&tmp_nn, &iszero); EG(ret, err);
		}
		/* r ≡ r*b */
		ret = fp_mul(&r, &r, &b); EG(ret, err);
		/* c ≡ b^2 */
		ret = fp_sqr(&c, &b); EG(ret, err);
		/* t ≡ t*b^2 */
		ret = fp_mul(&t, &t, &c); EG(ret, err);
		/* m = i */
		ret = nn_copy(&m, &i); EG(ret, err);
	}

 err:
	/* Uninitialize local variables */
	nn_uninit(&q);
	nn_uninit(&s);
	nn_uninit(&tmp_nn);
	nn_uninit(&one_nn);
	nn_uninit(&two_nn);
	nn_uninit(&m);
	nn_uninit(&i);
	fp_uninit(&z);
	fp_uninit(&t);
	fp_uninit(&b);
	fp_uninit(&r);
	fp_uninit(&c);
	fp_uninit(&one_fp);
	fp_uninit(&tmp_fp);
	fp_uninit(&__n);

	PTR_NULLIFY(_n);

	return ret;
}
