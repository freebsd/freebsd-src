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
#ifdef WITH_CURVE_SECP192R1

#ifndef __EC_PARAMS_SECP192R1_H__
#define __EC_PARAMS_SECP192R1_H__
#include "ec_params_external.h"

static const u8 secp192r1_p[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

TO_EC_STR_PARAM(secp192r1_p);

#define CURVE_SECP192R1_P_BITLEN 192
static const u8 secp192r1_p_bitlen[] = { 0xc0 };

TO_EC_STR_PARAM(secp192r1_p_bitlen);

#if (WORD_BYTES == 8)		/* 64-bit words */
static const u8 secp192r1_r[] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01
};

TO_EC_STR_PARAM(secp192r1_r);

static const u8 secp192r1_r_square[] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01
};

TO_EC_STR_PARAM(secp192r1_r_square);

static const u8 secp192r1_mpinv[] = {
	0x01
};

TO_EC_STR_PARAM(secp192r1_mpinv);

static const u8 secp192r1_p_shift[] = {
	0x00
};

TO_EC_STR_PARAM(secp192r1_p_shift);

static const u8 secp192r1_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

TO_EC_STR_PARAM(secp192r1_p_normalized);

static const u8 secp192r1_p_reciprocal[] = {
	0x00
};

TO_EC_STR_PARAM(secp192r1_p_reciprocal);

#elif (WORD_BYTES == 4)		/* 32-bit words */
static const u8 secp192r1_r[] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01
};

TO_EC_STR_PARAM(secp192r1_r);

static const u8 secp192r1_r_square[] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01
};

TO_EC_STR_PARAM(secp192r1_r_square);

static const u8 secp192r1_mpinv[] = {
	0x01
};

TO_EC_STR_PARAM(secp192r1_mpinv);

static const u8 secp192r1_p_shift[] = {
	0x00
};

TO_EC_STR_PARAM(secp192r1_p_shift);

static const u8 secp192r1_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

TO_EC_STR_PARAM(secp192r1_p_normalized);

static const u8 secp192r1_p_reciprocal[] = {
	0x00
};

TO_EC_STR_PARAM(secp192r1_p_reciprocal);

#elif (WORD_BYTES == 2)		/* 16-bit words */
static const u8 secp192r1_r[] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01
};

TO_EC_STR_PARAM(secp192r1_r);

static const u8 secp192r1_r_square[] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01
};

TO_EC_STR_PARAM(secp192r1_r_square);

static const u8 secp192r1_mpinv[] = {
	0x01
};

TO_EC_STR_PARAM(secp192r1_mpinv);

static const u8 secp192r1_p_shift[] = {
	0x00
};

TO_EC_STR_PARAM(secp192r1_p_shift);

static const u8 secp192r1_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

TO_EC_STR_PARAM(secp192r1_p_normalized);

static const u8 secp192r1_p_reciprocal[] = {
	0x00
};

TO_EC_STR_PARAM(secp192r1_p_reciprocal);

#else /* unknown word size */
#error "Unsupported word size"
#endif

static const u8 secp192r1_a[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfc
};

TO_EC_STR_PARAM(secp192r1_a);

static const u8 secp192r1_b[] = {
	0x64, 0x21, 0x05, 0x19, 0xe5, 0x9c, 0x80, 0xe7,
	0x0f, 0xa7, 0xe9, 0xab, 0x72, 0x24, 0x30, 0x49,
	0xfe, 0xb8, 0xde, 0xec, 0xc1, 0x46, 0xb9, 0xb1
};

TO_EC_STR_PARAM(secp192r1_b);

#define CURVE_SECP192R1_CURVE_ORDER_BITLEN 192
static const u8 secp192r1_curve_order[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0x99, 0xde, 0xf8, 0x36,
	0x14, 0x6b, 0xc9, 0xb1, 0xb4, 0xd2, 0x28, 0x31
};

TO_EC_STR_PARAM(secp192r1_curve_order);

static const u8 secp192r1_gx[] = {
	0x18, 0x8d, 0xa8, 0x0e, 0xb0, 0x30, 0x90, 0xf6,
	0x7c, 0xbf, 0x20, 0xeb, 0x43, 0xa1, 0x88, 0x00,
	0xf4, 0xff, 0x0a, 0xfd, 0x82, 0xff, 0x10, 0x12
};

TO_EC_STR_PARAM(secp192r1_gx);

static const u8 secp192r1_gy[] = {
	0x07, 0x19, 0x2b, 0x95, 0xff, 0xc8, 0xda, 0x78,
	0x63, 0x10, 0x11, 0xed, 0x6b, 0x24, 0xcd, 0xd5,
	0x73, 0xf9, 0x77, 0xa1, 0x1e, 0x79, 0x48, 0x11
};

TO_EC_STR_PARAM(secp192r1_gy);

static const u8 secp192r1_gz[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
};

TO_EC_STR_PARAM(secp192r1_gz);

static const u8 secp192r1_gen_order[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0x99, 0xde, 0xf8, 0x36,
	0x14, 0x6b, 0xc9, 0xb1, 0xb4, 0xd2, 0x28, 0x31
};

TO_EC_STR_PARAM(secp192r1_gen_order);

#define CURVE_SECP192R1_Q_BITLEN 192
static const u8 secp192r1_gen_order_bitlen[] = {
	0xc0
};

TO_EC_STR_PARAM(secp192r1_gen_order_bitlen);

static const u8 secp192r1_cofactor[] = {
	0x01
};

TO_EC_STR_PARAM(secp192r1_cofactor);

static const u8 secp192r1_alpha_montgomery[] = {
	0x00,
};

TO_EC_STR_PARAM_FIXED_SIZE(secp192r1_alpha_montgomery, 0);

static const u8 secp192r1_gamma_montgomery[] = {
	0x00,
};

TO_EC_STR_PARAM_FIXED_SIZE(secp192r1_gamma_montgomery, 0);

static const u8 secp192r1_alpha_edwards[] = {
	0x00,
};

TO_EC_STR_PARAM_FIXED_SIZE(secp192r1_alpha_edwards, 0);

static const u8 secp192r1_name[] = "SECP192R1";
TO_EC_STR_PARAM(secp192r1_name);

static const u8 secp192r1_oid[] = "1.2.840.10045.3.1.1";
TO_EC_STR_PARAM(secp192r1_oid);

static const ec_str_params secp192r1_str_params = {
	.p = &secp192r1_p_str_param,
	.p_bitlen = &secp192r1_p_bitlen_str_param,
	.r = &secp192r1_r_str_param,
	.r_square = &secp192r1_r_square_str_param,
	.mpinv = &secp192r1_mpinv_str_param,
	.p_shift = &secp192r1_p_shift_str_param,
	.p_normalized = &secp192r1_p_normalized_str_param,
	.p_reciprocal = &secp192r1_p_reciprocal_str_param,
	.a = &secp192r1_a_str_param,
	.b = &secp192r1_b_str_param,
	.curve_order = &secp192r1_curve_order_str_param,
	.gx = &secp192r1_gx_str_param,
	.gy = &secp192r1_gy_str_param,
	.gz = &secp192r1_gz_str_param,
	.gen_order = &secp192r1_gen_order_str_param,
	.gen_order_bitlen = &secp192r1_gen_order_bitlen_str_param,
	.cofactor = &secp192r1_cofactor_str_param,
        .alpha_montgomery = &secp192r1_alpha_montgomery_str_param,
        .gamma_montgomery = &secp192r1_gamma_montgomery_str_param,
        .alpha_edwards = &secp192r1_alpha_edwards_str_param,
	.oid = &secp192r1_oid_str_param,
	.name = &secp192r1_name_str_param,
};

/*
 * Compute max bit length of all curves for p and q
 */
#ifndef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN    0
#endif
#if (CURVES_MAX_P_BIT_LEN < CURVE_SECP192R1_P_BITLEN)
#undef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN CURVE_SECP192R1_P_BITLEN
#endif
#ifndef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN    0
#endif
#if (CURVES_MAX_Q_BIT_LEN < CURVE_SECP192R1_Q_BITLEN)
#undef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN CURVE_SECP192R1_Q_BITLEN
#endif
#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN    0
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_SECP192R1_CURVE_ORDER_BITLEN)
#undef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_SECP192R1_CURVE_ORDER_BITLEN
#endif

#endif /* __EC_PARAMS_SECP192R1_H__ */

#endif /* WITH_CURVE_SECP192R1 */
