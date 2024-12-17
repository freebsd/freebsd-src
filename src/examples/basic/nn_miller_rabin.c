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
/* We include the NN layer API header */
#include <libecc/libarith.h>

ATTRIBUTE_WARN_UNUSED_RET int miller_rabin(nn_src_t n, const unsigned int t, int *res);

/* Miller-Rabin primality test.
 * See "Handbook of Applied Cryptography", alorithm 4.24:
 *
 *   Algorithm: Miller-Rabin probabilistic primality test
 *   MILLER-RABIN(n,t)
 *   INPUT: an odd integer n ≥ 3 and security parameter t ≥ 1.
 *   OUTPUT: an answer “prime” or “composite” to the question: “Is n prime?”
 *     1. Write n − 1 = 2**s x r such that r is odd.
 *     2. For i from 1 to t do the following:
 *       2.1 Choose a random integer a, 2 ≤ a ≤ n − 2.
 *       2.2 Compute y = a**r mod n using Algorithm 2.143.
 *       2.3 If y != 1 and y != n − 1 then do the following:
 *         j←1.
 *         While j ≤ s − 1 and y != n − 1 do the following:
 *           Compute y←y2 mod n.
 *           If y = 1 then return(“composite”).
 *           j←j + 1.
 *           If y != n − 1 then return (“composite”).
 *     3. Return(“maybe prime”).
 *
 * The Miller-Rabin test can give false positives when
 * answering "maybe prime", but is always right when answering
 * "composite".
 */
int miller_rabin(nn_src_t n, const unsigned int t, int *res)
{
	int ret, iszero, cmp, isodd, cmp1, cmp2;
	unsigned int i;
	bitcnt_t k;
	/* Temporary NN variables */
	nn s, q, r, d, a, y, j, one, two, tmp;
	s.magic = q.magic = r.magic = d.magic = a.magic = y.magic = j.magic = WORD(0);
	one.magic = two.magic = tmp.magic = WORD(0);

	ret = nn_check_initialized(n); EG(ret, err);
	MUST_HAVE((res != NULL), ret, err);
	(*res) = 0;

	/* Initialize our local NN variables */
	ret = nn_init(&s, 0); EG(ret, err);
	ret = nn_init(&q, 0); EG(ret, err);
	ret = nn_init(&r, 0); EG(ret, err);
	ret = nn_init(&d, 0); EG(ret, err);
	ret = nn_init(&a, 0); EG(ret, err);
	ret = nn_init(&y, 0); EG(ret, err);
	ret = nn_init(&j, 0); EG(ret, err);
	ret = nn_init(&one, 0); EG(ret, err);
	ret = nn_init(&two, 0); EG(ret, err);
	ret = nn_init(&tmp, 0); EG(ret, err);

	/* Security parameter t must be >= 1 */
	MUST_HAVE((t >= 1), ret, err);

	/* one = 1 */
	ret = nn_one(&one); EG(ret, err);
	/* two = 2 */
	ret = nn_set_word_value(&two, WORD(2)); EG(ret, err);

	/* If n = 0, this is not a prime */
	ret = nn_iszero(n, &iszero); EG(ret, err);
	if (iszero) {
		ret = 0;
		(*res) = 0;
		goto err;
	}
	/* If n = 1, this is not a prime */
	ret = nn_cmp(n, &one, &cmp); EG(ret, err);
	if (cmp == 0) {
		ret = 0;
		(*res) = 0;
		goto err;
	}
	/* If n = 2, this is a prime number */
	ret = nn_cmp(n, &two, &cmp); EG(ret, err);
	if (cmp == 0) {
		ret = 0;
		(*res) = 1;
		goto err;
	}
	/* If n = 3, this is a prime number */
	ret = nn_copy(&tmp, n); EG(ret, err);
	ret = nn_dec(&tmp, &tmp); EG(ret, err);
	ret = nn_cmp(&tmp, &two, &cmp); EG(ret, err);
	if (cmp == 0) {
		ret = 0;
		(*res) = 1;
		goto err;
	}

	/* If n >= 4 is even, this is not a prime */
	ret = nn_isodd(n, &isodd); EG(ret, err);
	if (!isodd) {
		ret = 0;
		(*res) = 0;
		goto err;
	}

	/* n − 1 = 2^s x r, repeatedly try to divide n-1 by 2 */
	/* s = 0 and r = n-1 */
	ret = nn_zero(&s); EG(ret, err);
	ret = nn_copy(&r, n); EG(ret, err);
	ret = nn_dec(&r, &r); EG(ret, err);
	while (1) {
		ret = nn_divrem(&q, &d, &r, &two); EG(ret, err);
		ret = nn_inc(&s, &s); EG(ret, err);
		ret = nn_copy(&r, &q); EG(ret, err);
		/* If r is odd, we have finished our division */
		ret = nn_isodd(&r, &isodd); EG(ret, err);
		if (isodd) {
			break;
		}
	}
	/* 2. For i from 1 to t do the following: */
	for (i = 1; i <= t; i++) {
		bitcnt_t blen;
		/* 2.1 Choose a random integer a, 2 ≤ a ≤ n − 2 */
		ret = nn_copy(&tmp, n); EG(ret, err);
		ret = nn_dec(&tmp, &tmp); EG(ret, err);
		ret = nn_zero(&a); EG(ret, err);
		ret = nn_cmp(&a, &two, &cmp); EG(ret, err);
		while (cmp < 0) {
			ret = nn_get_random_mod(&a, &tmp); EG(ret, err);
			ret = nn_cmp(&a, &two, &cmp); EG(ret, err);
		}
		/* A very loose (and NOT robust) implementation of
		 * modular exponentiation with square and multiply
		 * to compute y = a**r (n)
		 * WARNING: NOT to be used in production code!
		 */
		ret = nn_one(&y); EG(ret, err);
		ret = nn_bitlen(&r, &blen); EG(ret, err);
		for (k = 0; k < blen; k++) {
			u8 bit;
			ret = nn_getbit(&r, k, &bit); EG(ret, err);
			if (bit) {
				/* Warning: the multiplication is not modular, we
				 * have to take care of our size here!
				 */
				MUST_HAVE((NN_MAX_BIT_LEN >=
					  (WORD_BITS * (y.wlen + a.wlen))), ret, err);
				ret = nn_mul(&y, &y, &a); EG(ret, err);
				ret = nn_mod(&y, &y, n); EG(ret, err);
			}
			MUST_HAVE((NN_MAX_BIT_LEN >= (2 * WORD_BITS * a.wlen)), ret, err);
			ret = nn_sqr(&a, &a); EG(ret, err);
			ret = nn_mod(&a, &a, n); EG(ret, err);
		}
		/* 2.3 If y != 1 and y != n − 1 then do the following
		 * Note: tmp still contains n - 1 here.
		 */
		ret = nn_cmp(&y, &one, &cmp1); EG(ret, err);
		ret = nn_cmp(&y, &tmp, &cmp2); EG(ret, err);
		if ((cmp1 != 0) && (cmp2 != 0)) {
			/* j←1. */
			ret = nn_one(&j); EG(ret, err);
			/*  While j ≤ s − 1 and y != n − 1 do the following: */
			ret = nn_cmp(&j, &s, &cmp1); EG(ret, err);
			ret = nn_cmp(&y, &tmp, &cmp2); EG(ret, err);
			while ((cmp1 < 0) && (cmp2 != 0)) {
				/* Compute y←y2 mod n. */
				MUST_HAVE((NN_MAX_BIT_LEN >=
					  (2 * WORD_BITS * y.wlen)), ret, err);
				ret = nn_sqr(&y, &y); EG(ret, err);
				ret = nn_mod(&y, &y, n); EG(ret, err);
				/* If y = 1 then return(“composite”). */
				ret = nn_cmp(&y, &one, &cmp); EG(ret, err);
				if (cmp == 0) {
					ret = 0;
					(*res) = 0;
					goto err;
				}
				/* j←j + 1. */
				ret = nn_inc(&j, &j); EG(ret, err);
				ret = nn_cmp(&j, &s, &cmp1); EG(ret, err);
				ret = nn_cmp(&y, &tmp, &cmp2); EG(ret, err);
			}
			/* If y != n − 1 then return (“composite”). */
			ret = nn_cmp(&y, &tmp, &cmp); EG(ret, err);
			if (cmp != 0) {
				ret = 0;
				(*res) = 0;
				goto err;
			}
		}
		/* 3. Return(“maybe prime”). */
		ret = 0;
		(*res) = 1;
	}

 err:
	nn_uninit(&s);
	nn_uninit(&q);
	nn_uninit(&r);
	nn_uninit(&d);
	nn_uninit(&a);
	nn_uninit(&y);
	nn_uninit(&j);
	nn_uninit(&one);
	nn_uninit(&two);
	nn_uninit(&tmp);

	return ret;
}
