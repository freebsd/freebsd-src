#include <libecc/lib_ecc_config.h>
#ifdef WITH_CURVE_GOST_R3410_2012_256_PARAMSETA

#ifndef __EC_PARAMS_GOST_R3410_2012_256_PARAMSETA_H__
#define __EC_PARAMS_GOST_R3410_2012_256_PARAMSETA_H__
#include <libecc/curves/known/ec_params_external.h>
static const u8 gost_R3410_2012_256_paramSetA_p[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x97,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_p);

#define CURVE_GOST_R3410_2012_256_PARAMSETA_P_BITLEN 256
static const u8 gost_R3410_2012_256_paramSetA_p_bitlen[] = {
	0x01, 0x00,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_p_bitlen);

#if (WORD_BYTES == 8)     /* 64-bit words */
static const u8 gost_R3410_2012_256_paramSetA_r[] = {
	0x02, 0x69,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_r);

static const u8 gost_R3410_2012_256_paramSetA_r_square[] = {
	0x05, 0xcf, 0x11,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_r_square);

static const u8 gost_R3410_2012_256_paramSetA_mpinv[] = {
	0x46, 0xf3, 0x23, 0x44, 0x75, 0xd5, 0xad, 0xd9,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_mpinv);

static const u8 gost_R3410_2012_256_paramSetA_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_p_shift);

static const u8 gost_R3410_2012_256_paramSetA_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x97,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_p_normalized);

static const u8 gost_R3410_2012_256_paramSetA_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_p_reciprocal);

#elif (WORD_BYTES == 4)   /* 32-bit words */
static const u8 gost_R3410_2012_256_paramSetA_r[] = {
	0x02, 0x69,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_r);

static const u8 gost_R3410_2012_256_paramSetA_r_square[] = {
	0x05, 0xcf, 0x11,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_r_square);

static const u8 gost_R3410_2012_256_paramSetA_mpinv[] = {
	0x75, 0xd5, 0xad, 0xd9,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_mpinv);

static const u8 gost_R3410_2012_256_paramSetA_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_p_shift);

static const u8 gost_R3410_2012_256_paramSetA_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x97,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_p_normalized);

static const u8 gost_R3410_2012_256_paramSetA_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_p_reciprocal);

#elif (WORD_BYTES == 2)   /* 16-bit words */
static const u8 gost_R3410_2012_256_paramSetA_r[] = {
	0x02, 0x69,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_r);

static const u8 gost_R3410_2012_256_paramSetA_r_square[] = {
	0x05, 0xcf, 0x11,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_r_square);

static const u8 gost_R3410_2012_256_paramSetA_mpinv[] = {
	0xad, 0xd9,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_mpinv);

static const u8 gost_R3410_2012_256_paramSetA_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_p_shift);

static const u8 gost_R3410_2012_256_paramSetA_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x97,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_p_normalized);

static const u8 gost_R3410_2012_256_paramSetA_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_p_reciprocal);

#else                     /* unknown word size */
#error "Unsupported word size"
#endif

static const u8 gost_R3410_2012_256_paramSetA_a[] = {
	0xc2, 0x17, 0x3f, 0x15, 0x13, 0x98, 0x16, 0x73,
	0xaf, 0x48, 0x92, 0xc2, 0x30, 0x35, 0xa2, 0x7c,
	0xe2, 0x5e, 0x20, 0x13, 0xbf, 0x95, 0xaa, 0x33,
	0xb2, 0x2c, 0x65, 0x6f, 0x27, 0x7e, 0x73, 0x35,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_a);

static const u8 gost_R3410_2012_256_paramSetA_b[] = {
	0x29, 0x5f, 0x9b, 0xae, 0x74, 0x28, 0xed, 0x9c,
	0xcc, 0x20, 0xe7, 0xc3, 0x59, 0xa9, 0xd4, 0x1a,
	0x22, 0xfc, 0xcd, 0x91, 0x08, 0xe1, 0x7b, 0xf7,
	0xba, 0x93, 0x37, 0xa6, 0xf8, 0xae, 0x95, 0x13,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_b);

#define CURVE_GOST_R3410_2012_256_PARAMSETA_CURVE_ORDER_BITLEN 257
static const u8 gost_R3410_2012_256_paramSetA_curve_order[] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x3f, 0x63, 0x37, 0x7f, 0x21, 0xed, 0x98,
	0xd7, 0x04, 0x56, 0xbd, 0x55, 0xb0, 0xd8, 0x31,
	0x9c,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_curve_order);

static const u8 gost_R3410_2012_256_paramSetA_gx[] = {
	0x91, 0xe3, 0x84, 0x43, 0xa5, 0xe8, 0x2c, 0x0d,
	0x88, 0x09, 0x23, 0x42, 0x57, 0x12, 0xb2, 0xbb,
	0x65, 0x8b, 0x91, 0x96, 0x93, 0x2e, 0x02, 0xc7,
	0x8b, 0x25, 0x82, 0xfe, 0x74, 0x2d, 0xaa, 0x28,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_gx);

static const u8 gost_R3410_2012_256_paramSetA_gy[] = {
	0x32, 0x87, 0x94, 0x23, 0xab, 0x1a, 0x03, 0x75,
	0x89, 0x57, 0x86, 0xc4, 0xbb, 0x46, 0xe9, 0x56,
	0x5f, 0xde, 0x0b, 0x53, 0x44, 0x76, 0x67, 0x40,
	0xaf, 0x26, 0x8a, 0xdb, 0x32, 0x32, 0x2e, 0x5c,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_gy);

static const u8 gost_R3410_2012_256_paramSetA_gz[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_gz);

static const u8 gost_R3410_2012_256_paramSetA_gen_order[] = {
	0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x0f, 0xd8, 0xcd, 0xdf, 0xc8, 0x7b, 0x66, 0x35,
	0xc1, 0x15, 0xaf, 0x55, 0x6c, 0x36, 0x0c, 0x67,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_gen_order);

#define CURVE_GOST_R3410_2012_256_PARAMSETA_Q_BITLEN 255
static const u8 gost_R3410_2012_256_paramSetA_gen_order_bitlen[] = {
	0xff,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_gen_order_bitlen);

static const u8 gost_R3410_2012_256_paramSetA_cofactor[] = {
	0x04,
};

TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_cofactor);

static const u8 gost_R3410_2012_256_paramSetA_alpha_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2012_256_paramSetA_alpha_montgomery, 0);

static const u8 gost_R3410_2012_256_paramSetA_gamma_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2012_256_paramSetA_gamma_montgomery, 0);

static const u8 gost_R3410_2012_256_paramSetA_alpha_edwards[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2012_256_paramSetA_alpha_edwards, 0);

static const u8 gost_R3410_2012_256_paramSetA_name[] = "GOST_R3410_2012_256_PARAMSETA";
TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_name);

static const u8 gost_R3410_2012_256_paramSetA_oid[] = "1.2.643.7.1.2.1.1.1";
TO_EC_STR_PARAM(gost_R3410_2012_256_paramSetA_oid);

static const ec_str_params gost_R3410_2012_256_paramSetA_str_params = {
	.p = &gost_R3410_2012_256_paramSetA_p_str_param,
	.p_bitlen = &gost_R3410_2012_256_paramSetA_p_bitlen_str_param,
	.r = &gost_R3410_2012_256_paramSetA_r_str_param,
	.r_square = &gost_R3410_2012_256_paramSetA_r_square_str_param,
	.mpinv = &gost_R3410_2012_256_paramSetA_mpinv_str_param,
	.p_shift = &gost_R3410_2012_256_paramSetA_p_shift_str_param,
	.p_normalized = &gost_R3410_2012_256_paramSetA_p_normalized_str_param,
	.p_reciprocal = &gost_R3410_2012_256_paramSetA_p_reciprocal_str_param,
	.a = &gost_R3410_2012_256_paramSetA_a_str_param,
	.b = &gost_R3410_2012_256_paramSetA_b_str_param,
	.curve_order = &gost_R3410_2012_256_paramSetA_curve_order_str_param,
	.gx = &gost_R3410_2012_256_paramSetA_gx_str_param,
	.gy = &gost_R3410_2012_256_paramSetA_gy_str_param,
	.gz = &gost_R3410_2012_256_paramSetA_gz_str_param,
	.gen_order = &gost_R3410_2012_256_paramSetA_gen_order_str_param,
	.gen_order_bitlen = &gost_R3410_2012_256_paramSetA_gen_order_bitlen_str_param,
	.cofactor = &gost_R3410_2012_256_paramSetA_cofactor_str_param,
	.alpha_montgomery = &gost_R3410_2012_256_paramSetA_alpha_montgomery_str_param,
	.gamma_montgomery = &gost_R3410_2012_256_paramSetA_gamma_montgomery_str_param,
	.alpha_edwards = &gost_R3410_2012_256_paramSetA_alpha_edwards_str_param,
	.oid = &gost_R3410_2012_256_paramSetA_oid_str_param,
	.name = &gost_R3410_2012_256_paramSetA_name_str_param,
};

/*
 * Compute max bit length of all curves for p and q
 */
#ifndef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN    0
#endif
#if (CURVES_MAX_P_BIT_LEN < CURVE_GOST_R3410_2012_256_PARAMSETA_P_BITLEN)
#undef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN CURVE_GOST_R3410_2012_256_PARAMSETA_P_BITLEN
#endif
#ifndef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN    0
#endif
#if (CURVES_MAX_Q_BIT_LEN < CURVE_GOST_R3410_2012_256_PARAMSETA_Q_BITLEN)
#undef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN CURVE_GOST_R3410_2012_256_PARAMSETA_Q_BITLEN
#endif
#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN    0
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_GOST_R3410_2012_256_PARAMSETA_CURVE_ORDER_BITLEN)
#undef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_GOST_R3410_2012_256_PARAMSETA_CURVE_ORDER_BITLEN
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
#if (MAX_CURVE_OID_LEN < 20)
#undef MAX_CURVE_OID_LEN
#define MAX_CURVE_OID_LEN 20
#endif
#if (MAX_CURVE_NAME_LEN < 50)
#undef MAX_CURVE_NAME_LEN
#define MAX_CURVE_NAME_LEN 50
#endif

#endif /* __EC_PARAMS_GOST_R3410_2012_256_PARAMSETA_H__ */

#endif /* WITH_CURVE_GOST_R3410_2012_256_PARAMSETA */
