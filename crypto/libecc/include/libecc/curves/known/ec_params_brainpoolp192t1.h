#include <libecc/lib_ecc_config.h>
#ifdef WITH_CURVE_BRAINPOOLP192T1

#ifndef __EC_PARAMS_BRAINPOOLP192T1_H__
#define __EC_PARAMS_BRAINPOOLP192T1_H__
#include <libecc/curves/known/ec_params_external.h>
static const u8 brainpoolp192t1_p[] = {
	0xc3, 0x02, 0xf4, 0x1d, 0x93, 0x2a, 0x36, 0xcd,
	0xa7, 0xa3, 0x46, 0x30, 0x93, 0xd1, 0x8d, 0xb7,
	0x8f, 0xce, 0x47, 0x6d, 0xe1, 0xa8, 0x62, 0x97,
};

TO_EC_STR_PARAM(brainpoolp192t1_p);

#define CURVE_BRAINPOOLP192T1_P_BITLEN 192
static const u8 brainpoolp192t1_p_bitlen[] = {
	0xc0,
};

TO_EC_STR_PARAM(brainpoolp192t1_p_bitlen);

#if (WORD_BYTES == 8)     /* 64-bit words */
static const u8 brainpoolp192t1_r[] = {
	0x3c, 0xfd, 0x0b, 0xe2, 0x6c, 0xd5, 0xc9, 0x32,
	0x58, 0x5c, 0xb9, 0xcf, 0x6c, 0x2e, 0x72, 0x48,
	0x70, 0x31, 0xb8, 0x92, 0x1e, 0x57, 0x9d, 0x69,
};

TO_EC_STR_PARAM(brainpoolp192t1_r);

static const u8 brainpoolp192t1_r_square[] = {
	0xb6, 0x22, 0x51, 0x26, 0xee, 0xd3, 0x4f, 0x10,
	0x33, 0xbf, 0x48, 0x46, 0x02, 0xc3, 0xfe, 0x69,
	0xe2, 0x47, 0x4c, 0x69, 0x72, 0xc7, 0xb2, 0x1a,
};

TO_EC_STR_PARAM(brainpoolp192t1_r_square);

static const u8 brainpoolp192t1_mpinv[] = {
	0xe0, 0x84, 0x96, 0xdb, 0x56, 0xa2, 0xc2, 0xd9,
};

TO_EC_STR_PARAM(brainpoolp192t1_mpinv);

static const u8 brainpoolp192t1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(brainpoolp192t1_p_shift);

static const u8 brainpoolp192t1_p_normalized[] = {
	0xc3, 0x02, 0xf4, 0x1d, 0x93, 0x2a, 0x36, 0xcd,
	0xa7, 0xa3, 0x46, 0x30, 0x93, 0xd1, 0x8d, 0xb7,
	0x8f, 0xce, 0x47, 0x6d, 0xe1, 0xa8, 0x62, 0x97,
};

TO_EC_STR_PARAM(brainpoolp192t1_p_normalized);

static const u8 brainpoolp192t1_p_reciprocal[] = {
	0x50, 0x0f, 0xea, 0x39, 0xff, 0x17, 0x28, 0xc8,
};

TO_EC_STR_PARAM(brainpoolp192t1_p_reciprocal);

#elif (WORD_BYTES == 4)   /* 32-bit words */
static const u8 brainpoolp192t1_r[] = {
	0x3c, 0xfd, 0x0b, 0xe2, 0x6c, 0xd5, 0xc9, 0x32,
	0x58, 0x5c, 0xb9, 0xcf, 0x6c, 0x2e, 0x72, 0x48,
	0x70, 0x31, 0xb8, 0x92, 0x1e, 0x57, 0x9d, 0x69,
};

TO_EC_STR_PARAM(brainpoolp192t1_r);

static const u8 brainpoolp192t1_r_square[] = {
	0xb6, 0x22, 0x51, 0x26, 0xee, 0xd3, 0x4f, 0x10,
	0x33, 0xbf, 0x48, 0x46, 0x02, 0xc3, 0xfe, 0x69,
	0xe2, 0x47, 0x4c, 0x69, 0x72, 0xc7, 0xb2, 0x1a,
};

TO_EC_STR_PARAM(brainpoolp192t1_r_square);

static const u8 brainpoolp192t1_mpinv[] = {
	0x56, 0xa2, 0xc2, 0xd9,
};

TO_EC_STR_PARAM(brainpoolp192t1_mpinv);

static const u8 brainpoolp192t1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(brainpoolp192t1_p_shift);

static const u8 brainpoolp192t1_p_normalized[] = {
	0xc3, 0x02, 0xf4, 0x1d, 0x93, 0x2a, 0x36, 0xcd,
	0xa7, 0xa3, 0x46, 0x30, 0x93, 0xd1, 0x8d, 0xb7,
	0x8f, 0xce, 0x47, 0x6d, 0xe1, 0xa8, 0x62, 0x97,
};

TO_EC_STR_PARAM(brainpoolp192t1_p_normalized);

static const u8 brainpoolp192t1_p_reciprocal[] = {
	0x50, 0x0f, 0xea, 0x39,
};

TO_EC_STR_PARAM(brainpoolp192t1_p_reciprocal);

#elif (WORD_BYTES == 2)   /* 16-bit words */
static const u8 brainpoolp192t1_r[] = {
	0x3c, 0xfd, 0x0b, 0xe2, 0x6c, 0xd5, 0xc9, 0x32,
	0x58, 0x5c, 0xb9, 0xcf, 0x6c, 0x2e, 0x72, 0x48,
	0x70, 0x31, 0xb8, 0x92, 0x1e, 0x57, 0x9d, 0x69,
};

TO_EC_STR_PARAM(brainpoolp192t1_r);

static const u8 brainpoolp192t1_r_square[] = {
	0xb6, 0x22, 0x51, 0x26, 0xee, 0xd3, 0x4f, 0x10,
	0x33, 0xbf, 0x48, 0x46, 0x02, 0xc3, 0xfe, 0x69,
	0xe2, 0x47, 0x4c, 0x69, 0x72, 0xc7, 0xb2, 0x1a,
};

TO_EC_STR_PARAM(brainpoolp192t1_r_square);

static const u8 brainpoolp192t1_mpinv[] = {
	0xc2, 0xd9,
};

TO_EC_STR_PARAM(brainpoolp192t1_mpinv);

static const u8 brainpoolp192t1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(brainpoolp192t1_p_shift);

static const u8 brainpoolp192t1_p_normalized[] = {
	0xc3, 0x02, 0xf4, 0x1d, 0x93, 0x2a, 0x36, 0xcd,
	0xa7, 0xa3, 0x46, 0x30, 0x93, 0xd1, 0x8d, 0xb7,
	0x8f, 0xce, 0x47, 0x6d, 0xe1, 0xa8, 0x62, 0x97,
};

TO_EC_STR_PARAM(brainpoolp192t1_p_normalized);

static const u8 brainpoolp192t1_p_reciprocal[] = {
	0x50, 0x0f,
};

TO_EC_STR_PARAM(brainpoolp192t1_p_reciprocal);

#else                     /* unknown word size */
#error "Unsupported word size"
#endif

static const u8 brainpoolp192t1_a[] = {
	0xc3, 0x02, 0xf4, 0x1d, 0x93, 0x2a, 0x36, 0xcd,
	0xa7, 0xa3, 0x46, 0x30, 0x93, 0xd1, 0x8d, 0xb7,
	0x8f, 0xce, 0x47, 0x6d, 0xe1, 0xa8, 0x62, 0x94,
};

TO_EC_STR_PARAM(brainpoolp192t1_a);

static const u8 brainpoolp192t1_b[] = {
	0x13, 0xd5, 0x6f, 0xfa, 0xec, 0x78, 0x68, 0x1e,
	0x68, 0xf9, 0xde, 0xb4, 0x3b, 0x35, 0xbe, 0xc2,
	0xfb, 0x68, 0x54, 0x2e, 0x27, 0x89, 0x7b, 0x79,
};

TO_EC_STR_PARAM(brainpoolp192t1_b);

#define CURVE_BRAINPOOLP192T1_CURVE_ORDER_BITLEN 192
static const u8 brainpoolp192t1_curve_order[] = {
	0xc3, 0x02, 0xf4, 0x1d, 0x93, 0x2a, 0x36, 0xcd,
	0xa7, 0xa3, 0x46, 0x2f, 0x9e, 0x9e, 0x91, 0x6b,
	0x5b, 0xe8, 0xf1, 0x02, 0x9a, 0xc4, 0xac, 0xc1,
};

TO_EC_STR_PARAM(brainpoolp192t1_curve_order);

static const u8 brainpoolp192t1_gx[] = {
	0x3a, 0xe9, 0xe5, 0x8c, 0x82, 0xf6, 0x3c, 0x30,
	0x28, 0x2e, 0x1f, 0xe7, 0xbb, 0xf4, 0x3f, 0xa7,
	0x2c, 0x44, 0x6a, 0xf6, 0xf4, 0x61, 0x81, 0x29,
};

TO_EC_STR_PARAM(brainpoolp192t1_gx);

static const u8 brainpoolp192t1_gy[] = {
	0x09, 0x7e, 0x2c, 0x56, 0x67, 0xc2, 0x22, 0x3a,
	0x90, 0x2a, 0xb5, 0xca, 0x44, 0x9d, 0x00, 0x84,
	0xb7, 0xe5, 0xb3, 0xde, 0x7c, 0xcc, 0x01, 0xc9,
};

TO_EC_STR_PARAM(brainpoolp192t1_gy);

static const u8 brainpoolp192t1_gz[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

TO_EC_STR_PARAM(brainpoolp192t1_gz);

static const u8 brainpoolp192t1_gen_order[] = {
	0xc3, 0x02, 0xf4, 0x1d, 0x93, 0x2a, 0x36, 0xcd,
	0xa7, 0xa3, 0x46, 0x2f, 0x9e, 0x9e, 0x91, 0x6b,
	0x5b, 0xe8, 0xf1, 0x02, 0x9a, 0xc4, 0xac, 0xc1,
};

TO_EC_STR_PARAM(brainpoolp192t1_gen_order);

#define CURVE_BRAINPOOLP192T1_Q_BITLEN 192
static const u8 brainpoolp192t1_gen_order_bitlen[] = {
	0xc0,
};

TO_EC_STR_PARAM(brainpoolp192t1_gen_order_bitlen);

static const u8 brainpoolp192t1_cofactor[] = {
	0x01,
};

TO_EC_STR_PARAM(brainpoolp192t1_cofactor);

static const u8 brainpoolp192t1_alpha_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(brainpoolp192t1_alpha_montgomery, 0);

static const u8 brainpoolp192t1_gamma_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(brainpoolp192t1_gamma_montgomery, 0);

static const u8 brainpoolp192t1_alpha_edwards[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(brainpoolp192t1_alpha_edwards, 0);

static const u8 brainpoolp192t1_name[] = "BRAINPOOLP192T1";
TO_EC_STR_PARAM(brainpoolp192t1_name);

static const u8 brainpoolp192t1_oid[] = "1.3.36.3.3.2.8.1.1.4";
TO_EC_STR_PARAM(brainpoolp192t1_oid);

static const ec_str_params brainpoolp192t1_str_params = {
	.p = &brainpoolp192t1_p_str_param,
	.p_bitlen = &brainpoolp192t1_p_bitlen_str_param,
	.r = &brainpoolp192t1_r_str_param,
	.r_square = &brainpoolp192t1_r_square_str_param,
	.mpinv = &brainpoolp192t1_mpinv_str_param,
	.p_shift = &brainpoolp192t1_p_shift_str_param,
	.p_normalized = &brainpoolp192t1_p_normalized_str_param,
	.p_reciprocal = &brainpoolp192t1_p_reciprocal_str_param,
	.a = &brainpoolp192t1_a_str_param,
	.b = &brainpoolp192t1_b_str_param,
	.curve_order = &brainpoolp192t1_curve_order_str_param,
	.gx = &brainpoolp192t1_gx_str_param,
	.gy = &brainpoolp192t1_gy_str_param,
	.gz = &brainpoolp192t1_gz_str_param,
	.gen_order = &brainpoolp192t1_gen_order_str_param,
	.gen_order_bitlen = &brainpoolp192t1_gen_order_bitlen_str_param,
	.cofactor = &brainpoolp192t1_cofactor_str_param,
	.alpha_montgomery = &brainpoolp192t1_alpha_montgomery_str_param,
	.gamma_montgomery = &brainpoolp192t1_gamma_montgomery_str_param,
	.alpha_edwards = &brainpoolp192t1_alpha_edwards_str_param,
	.oid = &brainpoolp192t1_oid_str_param,
	.name = &brainpoolp192t1_name_str_param,
};

/*
 * Compute max bit length of all curves for p and q
 */
#ifndef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN    0
#endif
#if (CURVES_MAX_P_BIT_LEN < CURVE_BRAINPOOLP192T1_P_BITLEN)
#undef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN CURVE_BRAINPOOLP192T1_P_BITLEN
#endif
#ifndef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN    0
#endif
#if (CURVES_MAX_Q_BIT_LEN < CURVE_BRAINPOOLP192T1_Q_BITLEN)
#undef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN CURVE_BRAINPOOLP192T1_Q_BITLEN
#endif
#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN    0
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_BRAINPOOLP192T1_CURVE_ORDER_BITLEN)
#undef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_BRAINPOOLP192T1_CURVE_ORDER_BITLEN
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
#if (MAX_CURVE_NAME_LEN < 29)
#undef MAX_CURVE_NAME_LEN
#define MAX_CURVE_NAME_LEN 29
#endif

#endif /* __EC_PARAMS_BRAINPOOLP192T1_H__ */

#endif /* WITH_CURVE_BRAINPOOLP192T1 */
