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
#ifdef WITH_CURVE_BRAINPOOLP256R1

#ifndef __EC_PARAMS_BRAINPOOLP256R1_H__
#define __EC_PARAMS_BRAINPOOLP256R1_H__
#include "ec_params_external.h"

static const u8 brainpoolp256r1_p[] = {
	0xA9, 0xFB, 0x57, 0xDB, 0xA1, 0xEE, 0xA9, 0xBC,
	0x3E, 0x66, 0x0A, 0x90, 0x9D, 0x83, 0x8D, 0x72,
	0x6E, 0x3B, 0xF6, 0x23, 0xD5, 0x26, 0x20, 0x28,
	0x20, 0x13, 0x48, 0x1D, 0x1F, 0x6E, 0x53, 0x77
};

TO_EC_STR_PARAM(brainpoolp256r1_p);

#define CURVE_BRAINPOOLP256R1_P_BITLEN 256
static const u8 brainpoolp256r1_p_bitlen[] = { 0x01, 0x00 };

TO_EC_STR_PARAM(brainpoolp256r1_p_bitlen);

static const u8 brainpoolp256r1_r[] = {
	0x56, 0x04, 0xa8, 0x24, 0x5e, 0x11, 0x56, 0x43,
	0xc1, 0x99, 0xf5, 0x6f, 0x62, 0x7c, 0x72, 0x8d,
	0x91, 0xc4, 0x09, 0xdc, 0x2a, 0xd9, 0xdf, 0xd7,
	0xdf, 0xec, 0xb7, 0xe2, 0xe0, 0x91, 0xac, 0x89
};

TO_EC_STR_PARAM(brainpoolp256r1_r);

static const u8 brainpoolp256r1_r_square[] = {
	0x47, 0x17, 0xaa, 0x21, 0xe5, 0x95, 0x7f, 0xa8,
	0xa1, 0xec, 0xda, 0xcd, 0x6b, 0x1a, 0xc8, 0x07,
	0x5c, 0xce, 0x4c, 0x26, 0x61, 0x4d, 0x4f, 0x4d,
	0x8c, 0xfe, 0xdf, 0x7b, 0xa6, 0x46, 0x5b, 0x6c
};

TO_EC_STR_PARAM(brainpoolp256r1_r_square);

static const u8 brainpoolp256r1_mpinv[] = {
	0xc6, 0xa7, 0x55, 0x90, 0xce, 0xfd, 0x89, 0xb9
};

TO_EC_STR_PARAM(brainpoolp256r1_mpinv);

static const u8 brainpoolp256r1_p_shift[] = {
	0x00
};

TO_EC_STR_PARAM(brainpoolp256r1_p_shift);

#if (WORD_BYTES == 8)		/* 64-bit words */
static const u8 brainpoolp256r1_p_reciprocal[] = {
	0x81, 0x8c, 0x11, 0x31, 0xa1, 0xc5, 0x5b, 0x7e
};
#elif (WORD_BYTES == 4)		/* 32-bit words */
static const u8 brainpoolp256r1_p_reciprocal[] = {
	0x81, 0x8c, 0x11, 0x31
};
#elif (WORD_BYTES == 2)		/* 16-bit words */
static const u8 brainpoolp256r1_p_reciprocal[] = {
	0x81, 0x8c
};
#else /* unknown word size */
#error "Unsupported word size"
#endif
TO_EC_STR_PARAM(brainpoolp256r1_p_reciprocal);

static const u8 brainpoolp256r1_a[] = {
	0x7D, 0x5A, 0x09, 0x75, 0xFC, 0x2C, 0x30, 0x57, 0xEE, 0xF6, 0x75, 0x30,
	0x41, 0x7A, 0xFF, 0xE7, 0xFB, 0x80, 0x55, 0xC1, 0x26, 0xDC, 0x5C, 0x6C,
	0xE9, 0x4A, 0x4B, 0x44, 0xF3, 0x30, 0xB5, 0xD9
};

TO_EC_STR_PARAM(brainpoolp256r1_a);

static const u8 brainpoolp256r1_b[] = {
	0x26, 0xDC, 0x5C, 0x6C, 0xE9, 0x4A, 0x4B, 0x44, 0xF3, 0x30, 0xB5, 0xD9,
	0xBB, 0xD7, 0x7C, 0xBF, 0x95, 0x84, 0x16, 0x29, 0x5C, 0xF7, 0xE1, 0xCE,
	0x6B, 0xCC, 0xDC, 0x18, 0xFF, 0x8C, 0x07, 0xB6
};

TO_EC_STR_PARAM(brainpoolp256r1_b);

#define CURVE_BRAINPOOLP256R1_CURVE_ORDER_BITLEN 256
static const u8 brainpoolp256r1_curve_order[] = {
	0xA9, 0xFB, 0x57, 0xDB, 0xA1, 0xEE, 0xA9, 0xBC, 0x3E, 0x66, 0x0A, 0x90,
	0x9D, 0x83, 0x8D, 0x71, 0x8C, 0x39, 0x7A, 0xA3, 0xB5, 0x61, 0xA6, 0xF7,
	0x90, 0x1E, 0x0E, 0x82, 0x97, 0x48, 0x56, 0xA7
};

TO_EC_STR_PARAM(brainpoolp256r1_curve_order);

static const u8 brainpoolp256r1_gx[] = {
	0x8B, 0xD2, 0xAE, 0xB9, 0xCB, 0x7E, 0x57, 0xCB, 0x2C, 0x4B, 0x48, 0x2F,
	0xFC, 0x81, 0xB7, 0xAF, 0xB9, 0xDE, 0x27, 0xE1, 0xE3, 0xBD, 0x23, 0xC2,
	0x3A, 0x44, 0x53, 0xBD, 0x9A, 0xCE, 0x32, 0x62
};

TO_EC_STR_PARAM(brainpoolp256r1_gx);

static const u8 brainpoolp256r1_gy[] = {
	0x54, 0x7E, 0xF8, 0x35, 0xC3, 0xDA, 0xC4, 0xFD, 0x97, 0xF8, 0x46, 0x1A,
	0x14, 0x61, 0x1D, 0xC9, 0xC2, 0x77, 0x45, 0x13, 0x2D, 0xED, 0x8E, 0x54,
	0x5C, 0x1D, 0x54, 0xC7, 0x2F, 0x04, 0x69, 0x97
};

TO_EC_STR_PARAM(brainpoolp256r1_gy);

static const u8 brainpoolp256r1_gz[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
};

TO_EC_STR_PARAM(brainpoolp256r1_gz);

static const u8 brainpoolp256r1_gen_order[] = {
	0xA9, 0xFB, 0x57, 0xDB, 0xA1, 0xEE, 0xA9, 0xBC, 0x3E, 0x66, 0x0A, 0x90,
	0x9D, 0x83, 0x8D, 0x71, 0x8C, 0x39, 0x7A, 0xA3, 0xB5, 0x61, 0xA6, 0xF7,
	0x90, 0x1E, 0x0E, 0x82, 0x97, 0x48, 0x56, 0xA7
};

TO_EC_STR_PARAM(brainpoolp256r1_gen_order);

#define CURVE_BRAINPOOLP256R1_Q_BITLEN 256
static const u8 brainpoolp256r1_gen_order_bitlen[] = { 0x01, 0x00 };

TO_EC_STR_PARAM(brainpoolp256r1_gen_order_bitlen);

static const u8 brainpoolp256r1_cofactor[] = { 0x01 };

TO_EC_STR_PARAM(brainpoolp256r1_cofactor);

static const u8 brainpoolp256r1_alpha_montgomery[] = {
	0x00,
};

TO_EC_STR_PARAM_FIXED_SIZE(brainpoolp256r1_alpha_montgomery, 0);

static const u8 brainpoolp256r1_gamma_montgomery[] = {
	0x00,
};

TO_EC_STR_PARAM_FIXED_SIZE(brainpoolp256r1_gamma_montgomery, 0);

static const u8 brainpoolp256r1_alpha_edwards[] = {
	0x00,
};

TO_EC_STR_PARAM_FIXED_SIZE(brainpoolp256r1_alpha_edwards, 0);

static const u8 brainpoolp256r1_oid[] = "1.3.36.3.3.2.8.1.1.7";
TO_EC_STR_PARAM(brainpoolp256r1_oid);

static const u8 brainpoolp256r1_name[] = "BRAINPOOLP256R1";
TO_EC_STR_PARAM(brainpoolp256r1_name);

static const ec_str_params brainpoolp256r1_str_params = {
	.p = &brainpoolp256r1_p_str_param,
	.p_bitlen = &brainpoolp256r1_p_bitlen_str_param,
	.r = &brainpoolp256r1_r_str_param,
	.r_square = &brainpoolp256r1_r_square_str_param,
	.mpinv = &brainpoolp256r1_mpinv_str_param,
	.p_shift = &brainpoolp256r1_p_shift_str_param,
	.p_normalized = &brainpoolp256r1_p_str_param,
	.p_reciprocal = &brainpoolp256r1_p_reciprocal_str_param,
	.a = &brainpoolp256r1_a_str_param,
	.b = &brainpoolp256r1_b_str_param,
	.curve_order = &brainpoolp256r1_curve_order_str_param,
	.gx = &brainpoolp256r1_gx_str_param,
	.gy = &brainpoolp256r1_gy_str_param,
	.gz = &brainpoolp256r1_gz_str_param,
	.gen_order = &brainpoolp256r1_gen_order_str_param,
	.gen_order_bitlen = &brainpoolp256r1_gen_order_bitlen_str_param,
	.cofactor = &brainpoolp256r1_cofactor_str_param,
	.alpha_montgomery = &brainpoolp256r1_alpha_montgomery_str_param,
	.gamma_montgomery = &brainpoolp256r1_gamma_montgomery_str_param,
	.alpha_edwards = &brainpoolp256r1_alpha_edwards_str_param,
	.oid = &brainpoolp256r1_oid_str_param,
	.name = &brainpoolp256r1_name_str_param,
};

/*
 * Compute max bit length of all curves for p and q
 */
#ifndef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN    0
#endif
#if (CURVES_MAX_P_BIT_LEN < CURVE_BRAINPOOLP256R1_P_BITLEN)
#undef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN CURVE_BRAINPOOLP256R1_P_BITLEN
#endif
#ifndef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN    0
#endif
#if (CURVES_MAX_Q_BIT_LEN < CURVE_BRAINPOOLP256R1_Q_BITLEN)
#undef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN CURVE_BRAINPOOLP256R1_Q_BITLEN
#endif
#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN    0
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_BRAINPOOLP256R1_CURVE_ORDER_BITLEN)
#undef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_BRAINPOOLP256R1_CURVE_ORDER_BITLEN
#endif

#endif /* __EC_PARAMS_BRAINPOOLP256R1_H__ */

#endif /* WITH_CURVE_BRAINPOOLP256R1 */
