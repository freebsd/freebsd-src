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
#ifdef WITH_CURVE_GOST256

#ifndef __EC_PARAMS_GOST256_H__
#define __EC_PARAMS_GOST256_H__
#include "ec_params_external.h"

static const u8 GOST_256bits_curve_p[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x31
};

TO_EC_STR_PARAM(GOST_256bits_curve_p);

#define CURVE_GOST256_P_BITLEN 256
static const u8 GOST_256bits_curve_p_bitlen[] = { 0x01, 0x00 };

TO_EC_STR_PARAM(GOST_256bits_curve_p_bitlen);

static const u8 GOST_256bits_curve_r[] = {
	0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xcf
};

TO_EC_STR_PARAM(GOST_256bits_curve_r);

static const u8 GOST_256bits_curve_r_square[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x46, 0x45, 0x84
};

TO_EC_STR_PARAM(GOST_256bits_curve_r_square);

static const u8 GOST_256bits_curve_mpinv[] = {
	0xdb, 0xf9, 0x51, 0xd5, 0x88, 0x3b, 0x2b, 0x2f
};

TO_EC_STR_PARAM(GOST_256bits_curve_mpinv);

static const u8 GOST_256bits_curve_p_shift[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

TO_EC_STR_PARAM(GOST_256bits_curve_p_shift);

#if (WORD_BYTES == 8)		/* 64-bit words */
static const u8 GOST_256bits_curve_p_reciprocal[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};
#elif (WORD_BYTES == 4)		/* 32-bit words */
static const u8 GOST_256bits_curve_p_reciprocal[] = {
	0xff, 0xff, 0xff, 0xff
};
#elif (WORD_BYTES == 2)		/* 16-bit words */
static const u8 GOST_256bits_curve_p_reciprocal[] = {
	0xff, 0xff
};
#else /* unknown word size */
#error "Unsupported word size"
#endif
TO_EC_STR_PARAM(GOST_256bits_curve_p_reciprocal);

static const u8 GOST_256bits_curve_a[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07
};

TO_EC_STR_PARAM(GOST_256bits_curve_a);

static const u8 GOST_256bits_curve_b[] = {
	0x5F, 0xBF, 0xF4, 0x98, 0xAA, 0x93, 0x8C, 0xE7,
	0x39, 0xB8, 0xE0, 0x22, 0xFB, 0xAF, 0xEF, 0x40,
	0x56, 0x3F, 0x6E, 0x6A, 0x34, 0x72, 0xFC, 0x2A,
	0x51, 0x4C, 0x0C, 0xE9, 0xDA, 0xE2, 0x3B, 0x7E
};

TO_EC_STR_PARAM(GOST_256bits_curve_b);

#define CURVE_GOST256_CURVE_ORDER_BITLEN 256
static const u8 GOST_256bits_curve_order[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x50, 0xFE, 0x8A, 0x18, 0x92, 0x97, 0x61, 0x54,
	0xC5, 0x9C, 0xFC, 0x19, 0x3A, 0xCC, 0xF5, 0xB3
};

TO_EC_STR_PARAM(GOST_256bits_curve_order);

static const u8 GOST_256bits_curve_gx[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02
};

TO_EC_STR_PARAM(GOST_256bits_curve_gx);

static const u8 GOST_256bits_curve_gy[] = {
	0x08, 0xE2, 0xA8, 0xA0, 0xE6, 0x51, 0x47, 0xD4,
	0xBD, 0x63, 0x16, 0x03, 0x0E, 0x16, 0xD1, 0x9C,
	0x85, 0xC9, 0x7F, 0x0A, 0x9C, 0xA2, 0x67, 0x12,
	0x2B, 0x96, 0xAB, 0xBC, 0xEA, 0x7E, 0x8F, 0xC8
};

TO_EC_STR_PARAM(GOST_256bits_curve_gy);

static const u8 GOST_256bits_curve_gz[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
};

TO_EC_STR_PARAM(GOST_256bits_curve_gz);

static const u8 GOST_256bits_curve_gen_order[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x50, 0xFE, 0x8A, 0x18, 0x92, 0x97, 0x61, 0x54,
	0xC5, 0x9C, 0xFC, 0x19, 0x3A, 0xCC, 0xF5, 0xB3
};

TO_EC_STR_PARAM(GOST_256bits_curve_gen_order);

#define CURVE_GOST256_Q_BITLEN 256
static const u8 GOST_256bits_curve_gen_order_bitlen[] = { 0x01, 0x00 };

TO_EC_STR_PARAM(GOST_256bits_curve_gen_order_bitlen);

static const u8 GOST_256bits_curve_cofactor[] = { 0x01 };

TO_EC_STR_PARAM(GOST_256bits_curve_cofactor);

static const u8 GOST_256bits_curve_alpha_montgomery[] = {
	0x00,
};

TO_EC_STR_PARAM_FIXED_SIZE(GOST_256bits_curve_alpha_montgomery, 0);

static const u8 GOST_256bits_curve_gamma_montgomery[] = {
	0x00,
};

TO_EC_STR_PARAM_FIXED_SIZE(GOST_256bits_curve_gamma_montgomery, 0);

static const u8 GOST_256bits_curve_alpha_edwards[] = {
	0x00,
};

TO_EC_STR_PARAM_FIXED_SIZE(GOST_256bits_curve_alpha_edwards, 0);

static const u8 GOST_256bits_curve_oid[] = "unknown";
TO_EC_STR_PARAM(GOST_256bits_curve_oid);

static const u8 GOST_256bits_curve_name[] = "GOST256";
TO_EC_STR_PARAM(GOST_256bits_curve_name);

static const ec_str_params GOST_256bits_curve_str_params = {
	.p = &GOST_256bits_curve_p_str_param,
	.p_bitlen = &GOST_256bits_curve_p_bitlen_str_param,
	.r = &GOST_256bits_curve_r_str_param,
	.r_square = &GOST_256bits_curve_r_square_str_param,
	.mpinv = &GOST_256bits_curve_mpinv_str_param,
	.p_shift = &GOST_256bits_curve_p_shift_str_param,
	.p_normalized = &GOST_256bits_curve_p_str_param,
	.p_reciprocal = &GOST_256bits_curve_p_reciprocal_str_param,
	.a = &GOST_256bits_curve_a_str_param,
	.b = &GOST_256bits_curve_b_str_param,
	.curve_order = &GOST_256bits_curve_order_str_param,
	.gx = &GOST_256bits_curve_gx_str_param,
	.gy = &GOST_256bits_curve_gy_str_param,
	.gz = &GOST_256bits_curve_gz_str_param,
	.gen_order = &GOST_256bits_curve_gen_order_str_param,
	.gen_order_bitlen = &GOST_256bits_curve_gen_order_bitlen_str_param,
	.cofactor = &GOST_256bits_curve_cofactor_str_param,
	.alpha_montgomery = &GOST_256bits_curve_alpha_montgomery_str_param,
	.gamma_montgomery = &GOST_256bits_curve_gamma_montgomery_str_param,
	.alpha_edwards = &GOST_256bits_curve_alpha_edwards_str_param,
	.oid = &GOST_256bits_curve_oid_str_param,
	.name = &GOST_256bits_curve_name_str_param,
};

/*
 * Compute max bit length of all curves for p and q
 */
#ifndef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN	0
#endif
#if (CURVES_MAX_P_BIT_LEN < CURVE_GOST256_P_BITLEN)
#undef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN CURVE_GOST256_P_BITLEN
#endif
#ifndef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN    0
#endif
#if (CURVES_MAX_Q_BIT_LEN < CURVE_GOST256_Q_BITLEN)
#undef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN CURVE_GOST256_Q_BITLEN
#endif
#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN  0
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_GOST256_CURVE_ORDER_BITLEN)
#undef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_GOST256_CURVE_ORDER_BITLEN
#endif

#endif /* __EC_PARAMS_GOST256_H__ */

#endif /* WITH_CURVE_GOST256 */
