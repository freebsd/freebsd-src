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
#ifdef WITH_CURVE_BRAINPOOLP192R1

#ifndef __EC_PARAMS_BRAINPOOLP192R1_H__
#define __EC_PARAMS_BRAINPOOLP192R1_H__
#include <libecc/curves/known/ec_params_external.h>
static const u8 brainpoolp192r1_p[] = {
	0xc3, 0x02, 0xf4, 0x1d, 0x93, 0x2a, 0x36, 0xcd,
	0xa7, 0xa3, 0x46, 0x30, 0x93, 0xd1, 0x8d, 0xb7,
	0x8f, 0xce, 0x47, 0x6d, 0xe1, 0xa8, 0x62, 0x97,
};

TO_EC_STR_PARAM(brainpoolp192r1_p);

#define CURVE_BRAINPOOLP192R1_P_BITLEN 192
static const u8 brainpoolp192r1_p_bitlen[] = {
	0xc0,
};

TO_EC_STR_PARAM(brainpoolp192r1_p_bitlen);

#if (WORD_BYTES == 8)     /* 64-bit words */
static const u8 brainpoolp192r1_r[] = {
	0x3c, 0xfd, 0x0b, 0xe2, 0x6c, 0xd5, 0xc9, 0x32,
	0x58, 0x5c, 0xb9, 0xcf, 0x6c, 0x2e, 0x72, 0x48,
	0x70, 0x31, 0xb8, 0x92, 0x1e, 0x57, 0x9d, 0x69,
};

TO_EC_STR_PARAM(brainpoolp192r1_r);

static const u8 brainpoolp192r1_r_square[] = {
	0xb6, 0x22, 0x51, 0x26, 0xee, 0xd3, 0x4f, 0x10,
	0x33, 0xbf, 0x48, 0x46, 0x02, 0xc3, 0xfe, 0x69,
	0xe2, 0x47, 0x4c, 0x69, 0x72, 0xc7, 0xb2, 0x1a,
};

TO_EC_STR_PARAM(brainpoolp192r1_r_square);

static const u8 brainpoolp192r1_mpinv[] = {
	0xe0, 0x84, 0x96, 0xdb, 0x56, 0xa2, 0xc2, 0xd9,
};

TO_EC_STR_PARAM(brainpoolp192r1_mpinv);

static const u8 brainpoolp192r1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(brainpoolp192r1_p_shift);

static const u8 brainpoolp192r1_p_normalized[] = {
	0xc3, 0x02, 0xf4, 0x1d, 0x93, 0x2a, 0x36, 0xcd,
	0xa7, 0xa3, 0x46, 0x30, 0x93, 0xd1, 0x8d, 0xb7,
	0x8f, 0xce, 0x47, 0x6d, 0xe1, 0xa8, 0x62, 0x97,
};

TO_EC_STR_PARAM(brainpoolp192r1_p_normalized);

static const u8 brainpoolp192r1_p_reciprocal[] = {
	0x50, 0x0f, 0xea, 0x39, 0xff, 0x17, 0x28, 0xc8,
};

TO_EC_STR_PARAM(brainpoolp192r1_p_reciprocal);

#elif (WORD_BYTES == 4)   /* 32-bit words */
static const u8 brainpoolp192r1_r[] = {
	0x3c, 0xfd, 0x0b, 0xe2, 0x6c, 0xd5, 0xc9, 0x32,
	0x58, 0x5c, 0xb9, 0xcf, 0x6c, 0x2e, 0x72, 0x48,
	0x70, 0x31, 0xb8, 0x92, 0x1e, 0x57, 0x9d, 0x69,
};

TO_EC_STR_PARAM(brainpoolp192r1_r);

static const u8 brainpoolp192r1_r_square[] = {
	0xb6, 0x22, 0x51, 0x26, 0xee, 0xd3, 0x4f, 0x10,
	0x33, 0xbf, 0x48, 0x46, 0x02, 0xc3, 0xfe, 0x69,
	0xe2, 0x47, 0x4c, 0x69, 0x72, 0xc7, 0xb2, 0x1a,
};

TO_EC_STR_PARAM(brainpoolp192r1_r_square);

static const u8 brainpoolp192r1_mpinv[] = {
	0x56, 0xa2, 0xc2, 0xd9,
};

TO_EC_STR_PARAM(brainpoolp192r1_mpinv);

static const u8 brainpoolp192r1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(brainpoolp192r1_p_shift);

static const u8 brainpoolp192r1_p_normalized[] = {
	0xc3, 0x02, 0xf4, 0x1d, 0x93, 0x2a, 0x36, 0xcd,
	0xa7, 0xa3, 0x46, 0x30, 0x93, 0xd1, 0x8d, 0xb7,
	0x8f, 0xce, 0x47, 0x6d, 0xe1, 0xa8, 0x62, 0x97,
};

TO_EC_STR_PARAM(brainpoolp192r1_p_normalized);

static const u8 brainpoolp192r1_p_reciprocal[] = {
	0x50, 0x0f, 0xea, 0x39,
};

TO_EC_STR_PARAM(brainpoolp192r1_p_reciprocal);

#elif (WORD_BYTES == 2)   /* 16-bit words */
static const u8 brainpoolp192r1_r[] = {
	0x3c, 0xfd, 0x0b, 0xe2, 0x6c, 0xd5, 0xc9, 0x32,
	0x58, 0x5c, 0xb9, 0xcf, 0x6c, 0x2e, 0x72, 0x48,
	0x70, 0x31, 0xb8, 0x92, 0x1e, 0x57, 0x9d, 0x69,
};

TO_EC_STR_PARAM(brainpoolp192r1_r);

static const u8 brainpoolp192r1_r_square[] = {
	0xb6, 0x22, 0x51, 0x26, 0xee, 0xd3, 0x4f, 0x10,
	0x33, 0xbf, 0x48, 0x46, 0x02, 0xc3, 0xfe, 0x69,
	0xe2, 0x47, 0x4c, 0x69, 0x72, 0xc7, 0xb2, 0x1a,
};

TO_EC_STR_PARAM(brainpoolp192r1_r_square);

static const u8 brainpoolp192r1_mpinv[] = {
	0xc2, 0xd9,
};

TO_EC_STR_PARAM(brainpoolp192r1_mpinv);

static const u8 brainpoolp192r1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(brainpoolp192r1_p_shift);

static const u8 brainpoolp192r1_p_normalized[] = {
	0xc3, 0x02, 0xf4, 0x1d, 0x93, 0x2a, 0x36, 0xcd,
	0xa7, 0xa3, 0x46, 0x30, 0x93, 0xd1, 0x8d, 0xb7,
	0x8f, 0xce, 0x47, 0x6d, 0xe1, 0xa8, 0x62, 0x97,
};

TO_EC_STR_PARAM(brainpoolp192r1_p_normalized);

static const u8 brainpoolp192r1_p_reciprocal[] = {
	0x50, 0x0f,
};

TO_EC_STR_PARAM(brainpoolp192r1_p_reciprocal);

#else                     /* unknown word size */
#error "Unsupported word size"
#endif

static const u8 brainpoolp192r1_a[] = {
	0x6a, 0x91, 0x17, 0x40, 0x76, 0xb1, 0xe0, 0xe1,
	0x9c, 0x39, 0xc0, 0x31, 0xfe, 0x86, 0x85, 0xc1,
	0xca, 0xe0, 0x40, 0xe5, 0xc6, 0x9a, 0x28, 0xef,
};

TO_EC_STR_PARAM(brainpoolp192r1_a);

static const u8 brainpoolp192r1_b[] = {
	0x46, 0x9a, 0x28, 0xef, 0x7c, 0x28, 0xcc, 0xa3,
	0xdc, 0x72, 0x1d, 0x04, 0x4f, 0x44, 0x96, 0xbc,
	0xca, 0x7e, 0xf4, 0x14, 0x6f, 0xbf, 0x25, 0xc9,
};

TO_EC_STR_PARAM(brainpoolp192r1_b);

#define CURVE_BRAINPOOLP192R1_CURVE_ORDER_BITLEN 192
static const u8 brainpoolp192r1_curve_order[] = {
	0xc3, 0x02, 0xf4, 0x1d, 0x93, 0x2a, 0x36, 0xcd,
	0xa7, 0xa3, 0x46, 0x2f, 0x9e, 0x9e, 0x91, 0x6b,
	0x5b, 0xe8, 0xf1, 0x02, 0x9a, 0xc4, 0xac, 0xc1,
};

TO_EC_STR_PARAM(brainpoolp192r1_curve_order);

static const u8 brainpoolp192r1_gx[] = {
	0xc0, 0xa0, 0x64, 0x7e, 0xaa, 0xb6, 0xa4, 0x87,
	0x53, 0xb0, 0x33, 0xc5, 0x6c, 0xb0, 0xf0, 0x90,
	0x0a, 0x2f, 0x5c, 0x48, 0x53, 0x37, 0x5f, 0xd6,
};

TO_EC_STR_PARAM(brainpoolp192r1_gx);

static const u8 brainpoolp192r1_gy[] = {
	0x14, 0xb6, 0x90, 0x86, 0x6a, 0xbd, 0x5b, 0xb8,
	0x8b, 0x5f, 0x48, 0x28, 0xc1, 0x49, 0x00, 0x02,
	0xe6, 0x77, 0x3f, 0xa2, 0xfa, 0x29, 0x9b, 0x8f,
};

TO_EC_STR_PARAM(brainpoolp192r1_gy);

static const u8 brainpoolp192r1_gz[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

TO_EC_STR_PARAM(brainpoolp192r1_gz);

static const u8 brainpoolp192r1_gen_order[] = {
	0xc3, 0x02, 0xf4, 0x1d, 0x93, 0x2a, 0x36, 0xcd,
	0xa7, 0xa3, 0x46, 0x2f, 0x9e, 0x9e, 0x91, 0x6b,
	0x5b, 0xe8, 0xf1, 0x02, 0x9a, 0xc4, 0xac, 0xc1,
};

TO_EC_STR_PARAM(brainpoolp192r1_gen_order);

#define CURVE_BRAINPOOLP192R1_Q_BITLEN 192
static const u8 brainpoolp192r1_gen_order_bitlen[] = {
	0xc0,
};

TO_EC_STR_PARAM(brainpoolp192r1_gen_order_bitlen);

static const u8 brainpoolp192r1_cofactor[] = {
	0x01,
};

TO_EC_STR_PARAM(brainpoolp192r1_cofactor);

static const u8 brainpoolp192r1_alpha_montgomery[] = {
	0x00,
};

TO_EC_STR_PARAM_FIXED_SIZE(brainpoolp192r1_alpha_montgomery, 0);

static const u8 brainpoolp192r1_gamma_montgomery[] = {
	0x00,
};

TO_EC_STR_PARAM_FIXED_SIZE(brainpoolp192r1_gamma_montgomery, 0);

static const u8 brainpoolp192r1_alpha_edwards[] = {
	0x00,
};

TO_EC_STR_PARAM_FIXED_SIZE(brainpoolp192r1_alpha_edwards, 0);

static const u8 brainpoolp192r1_name[] = "BRAINPOOLP192R1";
TO_EC_STR_PARAM(brainpoolp192r1_name);

static const u8 brainpoolp192r1_oid[] = "1.3.36.3.3.2.8.1.1.3";
TO_EC_STR_PARAM(brainpoolp192r1_oid);

static const ec_str_params brainpoolp192r1_str_params = {
	.p = &brainpoolp192r1_p_str_param,
	.p_bitlen = &brainpoolp192r1_p_bitlen_str_param,
	.r = &brainpoolp192r1_r_str_param,
	.r_square = &brainpoolp192r1_r_square_str_param,
	.mpinv = &brainpoolp192r1_mpinv_str_param,
	.p_shift = &brainpoolp192r1_p_shift_str_param,
	.p_normalized = &brainpoolp192r1_p_normalized_str_param,
	.p_reciprocal = &brainpoolp192r1_p_reciprocal_str_param,
	.a = &brainpoolp192r1_a_str_param,
	.b = &brainpoolp192r1_b_str_param,
	.curve_order = &brainpoolp192r1_curve_order_str_param,
	.gx = &brainpoolp192r1_gx_str_param,
	.gy = &brainpoolp192r1_gy_str_param,
	.gz = &brainpoolp192r1_gz_str_param,
	.gen_order = &brainpoolp192r1_gen_order_str_param,
	.gen_order_bitlen = &brainpoolp192r1_gen_order_bitlen_str_param,
	.cofactor = &brainpoolp192r1_cofactor_str_param,
	.alpha_montgomery = &brainpoolp192r1_alpha_montgomery_str_param,
	.gamma_montgomery = &brainpoolp192r1_gamma_montgomery_str_param,
	.alpha_edwards = &brainpoolp192r1_alpha_edwards_str_param,
	.oid = &brainpoolp192r1_oid_str_param,
	.name = &brainpoolp192r1_name_str_param,
};

/*
 * Compute max bit length of all curves for p and q
 */
#ifndef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN    0
#endif
#if (CURVES_MAX_P_BIT_LEN < CURVE_BRAINPOOLP192R1_P_BITLEN)
#undef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN CURVE_BRAINPOOLP192R1_P_BITLEN
#endif
#ifndef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN    0
#endif
#if (CURVES_MAX_Q_BIT_LEN < CURVE_BRAINPOOLP192R1_Q_BITLEN)
#undef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN CURVE_BRAINPOOLP192R1_Q_BITLEN
#endif
#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN    0
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_BRAINPOOLP192R1_CURVE_ORDER_BITLEN)
#undef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_BRAINPOOLP192R1_CURVE_ORDER_BITLEN
#endif

/*
 * Compute and adapt max name and oid length
 */
#ifndef MAX_CURVE_OID_LEN
#define MAX_CURVE_OID_LEN 0
#endif
#ifndef MAX_CURVE_NAME_LEN
#define MAX_CURVE_NAME_LEN 0
#endif
#if (MAX_CURVE_OID_LEN < 1)
#undef MAX_CURVE_OID_LEN
#define MAX_CURVE_OID_LEN 1
#endif
#if (MAX_CURVE_NAME_LEN < 28)
#undef MAX_CURVE_NAME_LEN
#define MAX_CURVE_NAME_LEN 28
#endif

#endif /* __EC_PARAMS_BRAINPOOLP192R1_H__ */

#endif /* WITH_CURVE_BRAINPOOLP192R1 */
