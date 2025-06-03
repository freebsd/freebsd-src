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
/*
 * The purpose of this example is to implement Pollard's rho
 * algorithm to find non-trivial factors of a composite natural
 * number.
 * The prime numbers decomposition of the natural number is
 * recovered through repeated Pollard's rho. Primality checking
 * is performed using a Miller-Rabin test.
 *
 * WARNING: the code in this example is only here to illustrate
 * how to use the NN layer API. This code has not been designed
 * for production purposes (e.g. no effort has been made to make
 * it constant time).
 *
 *
 */

/* We include the NN layer API header */
#include <libecc/libarith.h>

/* Declare our Miller-Rabin test implemented
 * in another module.
 */
ATTRIBUTE_WARN_UNUSED_RET int miller_rabin(nn_src_t n, const unsigned int t, int *check);

ATTRIBUTE_WARN_UNUSED_RET int pollard_rho(nn_t d, nn_src_t n, const word_t c);
/* Pollard's rho main function, as described in
 * "Handbook of Applied Cryptography".
 *
 * Pollard's rho:
 * ==============
 * See "Handbook of Applied Cryptography", alorithm 3.9:
 *
 *   Algorithm Pollard’s rho algorithm for factoring integers
 *   INPUT: a composite integer n that is not a prime power.
 *   OUTPUT: a non-trivial factor d of n.
 *      1. Set a←2, b←2.
 *      2. For i = 1, 2, ... do the following:
 *        2.1 Compute a←a^2 + 1 mod n, b←b^2 + 1 mod n, b←b^2 + 1 mod n.
 *        2.2 Compute d = gcd(a − b, n).
 *        2.3 If 1 < d < n then return(d) and terminate with success.
 *        2.4 If d = n then terminate the algorithm with failure (see Note 3.12).
 */
int pollard_rho(nn_t d, nn_src_t n, const word_t c)
{
	int ret, cmp, cmp1, cmp2;
	/* Temporary a and b variables */
	nn a, b, tmp, one, c_bignum;
	a.magic = b.magic = tmp.magic = one.magic = c_bignum.magic = WORD(0);

	/* Initialize variables */
	ret = nn_init(&a, 0); EG(ret, err);
	ret = nn_init(&b, 0); EG(ret, err);
	ret = nn_init(&tmp, 0); EG(ret, err);
	ret = nn_init(&one, 0); EG(ret, err);
	ret = nn_init(&c_bignum, 0); EG(ret, err);
	ret = nn_init(d, 0); EG(ret, err);

	MUST_HAVE((c > 0), ret, err);

	/* Zeroize the output */
	ret = nn_zero(d); EG(ret, err);
	ret = nn_one(&one); EG(ret, err);
	/* 1. Set a←2, b←2. */
	ret = nn_set_word_value(&a, WORD(2)); EG(ret, err);
	ret = nn_set_word_value(&b, WORD(2)); EG(ret, err);
	ret = nn_set_word_value(&c_bignum, c); EG(ret, err);

	/* For i = 1, 2, . . . do the following: */
	while (1) {
		int sign;
		/* 2.1 Compute a←a^2 + c mod n */
		ret = nn_sqr(&a, &a); EG(ret, err);
		ret = nn_add(&a, &a, &c_bignum); EG(ret, err);
		ret = nn_mod(&a, &a, n); EG(ret, err);
		/* 2.1 Compute b←b^2 + c mod n twice in a row */
		ret = nn_sqr(&b, &b); EG(ret, err);
		ret = nn_add(&b, &b, &c_bignum); EG(ret, err);
		ret = nn_mod(&b, &b, n); EG(ret, err);
		ret = nn_sqr(&b, &b); EG(ret, err);
		ret = nn_add(&b, &b, &c_bignum); EG(ret, err);
		ret = nn_mod(&b, &b, n); EG(ret, err);
		/* 2.2 Compute d = gcd(a − b, n) */
		ret = nn_cmp(&a, &b, &cmp); EG(ret, err);
		if (cmp >= 0) {
			ret = nn_sub(&tmp, &a, &b); EG(ret, err);
		} else {
			ret = nn_sub(&tmp, &b, &a); EG(ret, err);
		}
		ret = nn_gcd(d, &tmp, n, &sign); EG(ret, err);
		ret = nn_cmp(d, n, &cmp1); EG(ret, err);
		ret = nn_cmp(d, &one, &cmp2); EG(ret, err);
		if ((cmp1 < 0) && (cmp2 > 0)) {
			ret = 0;
			goto err;
		}
		ret = nn_cmp(d, n, &cmp); EG(ret, err);
		if (cmp == 0) {
			ret = -1;
			goto err;
		}
	}
 err:
	/* Uninitialize local variables */
	nn_uninit(&a);
	nn_uninit(&b);
	nn_uninit(&tmp);
	nn_uninit(&one);
	nn_uninit(&c_bignum);

	return ret;
}

ATTRIBUTE_WARN_UNUSED_RET int find_divisors(nn_src_t in);
/* Maximum number of divisors we support */
#define MAX_DIVISORS 10
/* Function to find prime divisors of the NN input */
int find_divisors(nn_src_t in)
{
	int n_divisors_found, i, found, ret, check, cmp;
	nn n;
	nn divisors[MAX_DIVISORS];
	word_t c;

	n.magic = WORD(0);
	for(i = 0; i < MAX_DIVISORS; i++){
		divisors[i].magic = WORD(0);
	}

	ret = nn_check_initialized(in); EG(ret, err);

	ext_printf("=================\n");
	nn_print("Finding factors of:", in);

	/* First, check primality of the input */
	ret = miller_rabin(in, 10, &check); EG(ret, err);
	if (check) {
		ext_printf("The number is probably prime, leaving ...\n");
		ret = -1;
		goto err;
	}
	ext_printf("The number is composite, performing Pollard's rho\n");

	ret = nn_init(&n, 0); EG(ret, err);
	ret = nn_copy(&n, in); EG(ret, err);
	for (i = 0; i < MAX_DIVISORS; i++) {
		ret = nn_init(&(divisors[i]), 0); EG(ret, err);
	}

	n_divisors_found = 0;
	c = 0;
	while (1) {
		c++;
		ret = pollard_rho(&(divisors[n_divisors_found]), &n, c);
		if (ret) {
			continue;
		}
		found = 0;
		for (i = 0; i < n_divisors_found; i++) {
			ret = nn_cmp(&(divisors[n_divisors_found]), &(divisors[i]), &cmp); EG(ret, err);
			if (cmp == 0) {
				found = 1;
			}
		}
		if (found == 0) {
			nn q, r;
			ret = nn_init(&q, 0); EG(ret, err);
			ret = nn_init(&r, 0); EG(ret, err);
			ext_printf("Pollard's rho succeded\n");
			nn_print("d:", &(divisors[n_divisors_found]));
			/*
			 * Now we can launch the algorithm again on n / d
			 * to find new divisors. If n / d is prime, we are done!
			 */
			ret = nn_divrem(&q, &r, &n, &(divisors[n_divisors_found])); EG(ret, err);
			/*
			 * Check n / d primality with Miller-Rabin (security
			 * parameter of 10)
			 */
			ret = miller_rabin(&q, 10, &check); EG(ret, err);
			if (check == 1) {
				nn_print("Last divisor is prime:", &q);
				nn_uninit(&q);
				nn_uninit(&r);
				break;
			}
			nn_print("Now performing Pollard's rho on:", &q);
			ret = nn_copy(&n, &q); EG(ret, err);
			nn_uninit(&q);
			nn_uninit(&r);
			c = 0;
			n_divisors_found++;
			if (n_divisors_found == MAX_DIVISORS) {
				ext_printf
					("Max divisors reached, leaving ...\n");
				break;
			}
		}
	}
	ret = 0;

err:
	ext_printf("=================\n");
	nn_uninit(&n);
	for (i = 0; i < MAX_DIVISORS; i++) {
		nn_uninit(&(divisors[i]));
	}
	return ret;
}

#ifdef NN_EXAMPLE
int main(int argc, char *argv[])
{
	int ret;

	/* Fermat F5 = 2^32 + 1 = 641 x 6700417 */
	const unsigned char fermat_F5[] = { 0x01, 0x00, 0x00, 0x00, 0x01 };
	/* Fermat F6 = 2^64 + 1 = 274177 x 67280421310721 */
	const unsigned char fermat_F6[] =
		{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 };
	nn n;
	n.magic = WORD(0);

	FORCE_USED_VAR(argc);
	FORCE_USED_VAR(argv);

	ret = nn_init(&n, 0); EG(ret, err);
	/* Execute factorization on F5 */
	ret = nn_init_from_buf(&n, fermat_F5, sizeof(fermat_F5)); EG(ret, err);
	ret = find_divisors(&n); EG(ret, err);
	/* Execute factorization on F6 */
	ret = nn_init_from_buf(&n, fermat_F6, sizeof(fermat_F6)); EG(ret, err);
	ret = find_divisors(&n); EG(ret, err);
	/* Execute factorization on a random 80 bits number */
	ret = nn_one(&n); EG(ret, err);
	/* Compute 2**80 = 0x1 << 80 */
	ret = nn_lshift(&n, &n, 80); EG(ret, err);
	ret = nn_get_random_mod(&n, &n); EG(ret, err);
	ret = find_divisors(&n); EG(ret, err);

	return 0;
err:
	return -1;
}
#endif
