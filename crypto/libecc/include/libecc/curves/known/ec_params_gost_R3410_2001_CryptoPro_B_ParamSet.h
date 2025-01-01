#include <libecc/lib_ecc_config.h>
#ifdef WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET

#ifndef __EC_PARAMS_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET_H__
#define __EC_PARAMS_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET_H__
#include <libecc/curves/known/ec_params_external.h>
static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_p[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x99,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_p);

#define CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET_P_BITLEN 256
static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_p_bitlen[] = {
	0x01, 0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_p_bitlen);

#if (WORD_BYTES == 8)     /* 64-bit words */
static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_r[] = {
	0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf3, 0x67,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_r);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_r_square[] = {
	0x02, 0x7a, 0xcd, 0xc4,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_r_square);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_mpinv[] = {
	0xbd, 0x66, 0x7a, 0xb8, 0xa3, 0x34, 0x78, 0x57,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_mpinv);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_p_shift);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_p_normalized[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x99,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_p_normalized);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_p_reciprocal[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_p_reciprocal);

#elif (WORD_BYTES == 4)   /* 32-bit words */
static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_r[] = {
	0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf3, 0x67,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_r);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_r_square[] = {
	0x02, 0x7a, 0xcd, 0xc4,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_r_square);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_mpinv[] = {
	0xa3, 0x34, 0x78, 0x57,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_mpinv);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_p_shift);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_p_normalized[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x99,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_p_normalized);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_p_reciprocal[] = {
	0xff, 0xff, 0xff, 0xff,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_p_reciprocal);

#elif (WORD_BYTES == 2)   /* 16-bit words */
static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_r[] = {
	0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf3, 0x67,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_r);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_r_square[] = {
	0x02, 0x7a, 0xcd, 0xc4,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_r_square);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_mpinv[] = {
	0x78, 0x57,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_mpinv);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_p_shift);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_p_normalized[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x99,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_p_normalized);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_p_reciprocal[] = {
	0xff, 0xff,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_p_reciprocal);

#else                     /* unknown word size */
#error "Unsupported word size"
#endif

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_a[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x96,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_a);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_b[] = {
	0x3e, 0x1a, 0xf4, 0x19, 0xa2, 0x69, 0xa5, 0xf8,
	0x66, 0xa7, 0xd3, 0xc2, 0x5c, 0x3d, 0xf8, 0x0a,
	0xe9, 0x79, 0x25, 0x93, 0x73, 0xff, 0x2b, 0x18,
	0x2f, 0x49, 0xd4, 0xce, 0x7e, 0x1b, 0xbc, 0x8b,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_b);

#define CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET_CURVE_ORDER_BITLEN 256
static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_curve_order[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x5f, 0x70, 0x0c, 0xff, 0xf1, 0xa6, 0x24, 0xe5,
	0xe4, 0x97, 0x16, 0x1b, 0xcc, 0x8a, 0x19, 0x8f,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_curve_order);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_gx[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_gx);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_gy[] = {
	0x3f, 0xa8, 0x12, 0x43, 0x59, 0xf9, 0x66, 0x80,
	0xb8, 0x3d, 0x1c, 0x3e, 0xb2, 0xc0, 0x70, 0xe5,
	0xc5, 0x45, 0xc9, 0x85, 0x8d, 0x03, 0xec, 0xfb,
	0x74, 0x4b, 0xf8, 0xd7, 0x17, 0x71, 0x7e, 0xfc,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_gy);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_gz[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_gz);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_gen_order[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x5f, 0x70, 0x0c, 0xff, 0xf1, 0xa6, 0x24, 0xe5,
	0xe4, 0x97, 0x16, 0x1b, 0xcc, 0x8a, 0x19, 0x8f,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_gen_order);

#define CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET_Q_BITLEN 256
static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_gen_order_bitlen[] = {
	0x01, 0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_gen_order_bitlen);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_cofactor[] = {
	0x01,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_cofactor);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_alpha_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2001_CryptoPro_B_ParamSet_alpha_montgomery, 0);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_gamma_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2001_CryptoPro_B_ParamSet_gamma_montgomery, 0);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_alpha_edwards[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2001_CryptoPro_B_ParamSet_alpha_edwards, 0);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_name[] = "GOST_R3410_2001_CRYPTOPRO_B_PARAMSET";
TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_name);

static const u8 gost_R3410_2001_CryptoPro_B_ParamSet_oid[] = "1.2.643.2.2.35.2";
TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_B_ParamSet_oid);

static const ec_str_params gost_R3410_2001_CryptoPro_B_ParamSet_str_params = {
	.p = &gost_R3410_2001_CryptoPro_B_ParamSet_p_str_param,
	.p_bitlen = &gost_R3410_2001_CryptoPro_B_ParamSet_p_bitlen_str_param,
	.r = &gost_R3410_2001_CryptoPro_B_ParamSet_r_str_param,
	.r_square = &gost_R3410_2001_CryptoPro_B_ParamSet_r_square_str_param,
	.mpinv = &gost_R3410_2001_CryptoPro_B_ParamSet_mpinv_str_param,
	.p_shift = &gost_R3410_2001_CryptoPro_B_ParamSet_p_shift_str_param,
	.p_normalized = &gost_R3410_2001_CryptoPro_B_ParamSet_p_normalized_str_param,
	.p_reciprocal = &gost_R3410_2001_CryptoPro_B_ParamSet_p_reciprocal_str_param,
	.a = &gost_R3410_2001_CryptoPro_B_ParamSet_a_str_param,
	.b = &gost_R3410_2001_CryptoPro_B_ParamSet_b_str_param,
	.curve_order = &gost_R3410_2001_CryptoPro_B_ParamSet_curve_order_str_param,
	.gx = &gost_R3410_2001_CryptoPro_B_ParamSet_gx_str_param,
	.gy = &gost_R3410_2001_CryptoPro_B_ParamSet_gy_str_param,
	.gz = &gost_R3410_2001_CryptoPro_B_ParamSet_gz_str_param,
	.gen_order = &gost_R3410_2001_CryptoPro_B_ParamSet_gen_order_str_param,
	.gen_order_bitlen = &gost_R3410_2001_CryptoPro_B_ParamSet_gen_order_bitlen_str_param,
	.cofactor = &gost_R3410_2001_CryptoPro_B_ParamSet_cofactor_str_param,
	.alpha_montgomery = &gost_R3410_2001_CryptoPro_B_ParamSet_alpha_montgomery_str_param,
	.gamma_montgomery = &gost_R3410_2001_CryptoPro_B_ParamSet_gamma_montgomery_str_param,
	.alpha_edwards = &gost_R3410_2001_CryptoPro_B_ParamSet_alpha_edwards_str_param,
	.oid = &gost_R3410_2001_CryptoPro_B_ParamSet_oid_str_param,
	.name = &gost_R3410_2001_CryptoPro_B_ParamSet_name_str_param,
};

/*
 * Compute max bit length of all curves for p and q
 */
#ifndef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN    0
#endif
#if (CURVES_MAX_P_BIT_LEN < CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET_P_BITLEN)
#undef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET_P_BITLEN
#endif
#ifndef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN    0
#endif
#if (CURVES_MAX_Q_BIT_LEN < CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET_Q_BITLEN)
#undef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET_Q_BITLEN
#endif
#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN    0
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET_CURVE_ORDER_BITLEN)
#undef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET_CURVE_ORDER_BITLEN
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
#if (MAX_CURVE_OID_LEN < 17)
#undef MAX_CURVE_OID_LEN
#define MAX_CURVE_OID_LEN 17
#endif
#if (MAX_CURVE_NAME_LEN < 52)
#undef MAX_CURVE_NAME_LEN
#define MAX_CURVE_NAME_LEN 52
#endif

#endif /* __EC_PARAMS_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET_H__ */

#endif /* WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_B_PARAMSET */
