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
#ifndef __EC_PARAMS_EXTERNAL_H__
#define __EC_PARAMS_EXTERNAL_H__
#include <libecc/words/words.h>

typedef struct {
	const u8 *buf;
	const u8 buflen;
} ec_str_param;

#define TO_EC_STR_PARAM(pname) \
	static const ec_str_param pname##_str_param = { \
		.buf = pname,				\
		.buflen = sizeof(pname)                 \
	}


#define TO_EC_STR_PARAM_FIXED_SIZE(pname, sz) \
	static const ec_str_param pname##_str_param = { \
		.buf = pname,				\
		.buflen = (sz)				\
	}


#define PARAM_BUF_LEN(param) ((param)->buflen)
#define PARAM_BUF_PTR(param) ((param)->buf)

typedef struct {
	/*
	 * Prime p:
	 *  o p_bitlen = bitsizeof(p)
	 */
	const ec_str_param *p;
	const ec_str_param *p_bitlen;

	/*
	 * Precomputed Montgomery parameters:
	 *  o r = 2^bitsizeof(p) mod p
	 *  o r_square = 2^(2*bitsizeof(p)) mod p
	 *  o mpinv = -p^-1 mod B
	 * where B = 2^(bitsizeof(word_t))
	 */
	const ec_str_param *r;
	const ec_str_param *r_square;
	const ec_str_param *mpinv;

	/*
	 * Precomputed division parameters:
	 *  o p_shift = nn_clz(p)
	 *  o p_normalized =  p << p_shift
	 *  o p_reciprocal = floor(B^3/(DMSW(p_normalized) + 1)) - B
	 * where B = 2^(bitsizeof(word_t))
	 */
	const ec_str_param *p_shift;
	const ec_str_param *p_normalized;
	const ec_str_param *p_reciprocal;

	/* Curve coefficients and number of points */
	const ec_str_param *a;
	const ec_str_param *b;
	const ec_str_param *curve_order;

	/*
	 * Projective coordinates of generator
	 * and order and cofactor of associated subgroup.
	 */
	const ec_str_param *gx;
	const ec_str_param *gy;
	const ec_str_param *gz;
	const ec_str_param *gen_order;
	const ec_str_param *gen_order_bitlen;
	const ec_str_param *cofactor;

	/*
	 * Optional transfert coefficients to Montgomery curve.
	 */
	const ec_str_param *alpha_montgomery;
	const ec_str_param *gamma_montgomery;

	/*
	 * Optional transfert coefficient to Edwards curve.
	 */
	const ec_str_param *alpha_edwards;

	/* OID and pretty name */
	const ec_str_param *oid;
	const ec_str_param *name;
} ec_str_params;

#endif /* __EC_PARAMS_EXTERNAL_H__ */
