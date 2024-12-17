#include <libecc/lib_ecc_config.h>
#ifdef WITH_CURVE_GOST_R3410_2001_TESTPARAMSET

#ifndef __EC_PARAMS_GOST_R3410_2001_TESTPARAMSET_H__
#define __EC_PARAMS_GOST_R3410_2001_TESTPARAMSET_H__
#include <libecc/curves/known/ec_params_external.h>
static const u8 gost_R3410_2001_TestParamSet_p[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x31,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_p);

#define CURVE_GOST_R3410_2001_TESTPARAMSET_P_BITLEN 256
static const u8 gost_R3410_2001_TestParamSet_p_bitlen[] = {
	0x01, 0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_p_bitlen);

#if (WORD_BYTES == 8)     /* 64-bit words */
static const u8 gost_R3410_2001_TestParamSet_r[] = {
	0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xcf,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_r);

static const u8 gost_R3410_2001_TestParamSet_r_square[] = {
	0x46, 0x45, 0x84,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_r_square);

static const u8 gost_R3410_2001_TestParamSet_mpinv[] = {
	0xdb, 0xf9, 0x51, 0xd5, 0x88, 0x3b, 0x2b, 0x2f,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_mpinv);

static const u8 gost_R3410_2001_TestParamSet_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_p_shift);

static const u8 gost_R3410_2001_TestParamSet_p_normalized[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x31,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_p_normalized);

static const u8 gost_R3410_2001_TestParamSet_p_reciprocal[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_p_reciprocal);

#elif (WORD_BYTES == 4)   /* 32-bit words */
static const u8 gost_R3410_2001_TestParamSet_r[] = {
	0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xcf,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_r);

static const u8 gost_R3410_2001_TestParamSet_r_square[] = {
	0x46, 0x45, 0x84,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_r_square);

static const u8 gost_R3410_2001_TestParamSet_mpinv[] = {
	0x88, 0x3b, 0x2b, 0x2f,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_mpinv);

static const u8 gost_R3410_2001_TestParamSet_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_p_shift);

static const u8 gost_R3410_2001_TestParamSet_p_normalized[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x31,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_p_normalized);

static const u8 gost_R3410_2001_TestParamSet_p_reciprocal[] = {
	0xff, 0xff, 0xff, 0xff,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_p_reciprocal);

#elif (WORD_BYTES == 2)   /* 16-bit words */
static const u8 gost_R3410_2001_TestParamSet_r[] = {
	0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfb, 0xcf,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_r);

static const u8 gost_R3410_2001_TestParamSet_r_square[] = {
	0x46, 0x45, 0x84,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_r_square);

static const u8 gost_R3410_2001_TestParamSet_mpinv[] = {
	0x2b, 0x2f,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_mpinv);

static const u8 gost_R3410_2001_TestParamSet_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_p_shift);

static const u8 gost_R3410_2001_TestParamSet_p_normalized[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x31,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_p_normalized);

static const u8 gost_R3410_2001_TestParamSet_p_reciprocal[] = {
	0xff, 0xff,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_p_reciprocal);

#else                     /* unknown word size */
#error "Unsupported word size"
#endif

static const u8 gost_R3410_2001_TestParamSet_a[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_a);

static const u8 gost_R3410_2001_TestParamSet_b[] = {
	0x5f, 0xbf, 0xf4, 0x98, 0xaa, 0x93, 0x8c, 0xe7,
	0x39, 0xb8, 0xe0, 0x22, 0xfb, 0xaf, 0xef, 0x40,
	0x56, 0x3f, 0x6e, 0x6a, 0x34, 0x72, 0xfc, 0x2a,
	0x51, 0x4c, 0x0c, 0xe9, 0xda, 0xe2, 0x3b, 0x7e,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_b);

#define CURVE_GOST_R3410_2001_TESTPARAMSET_CURVE_ORDER_BITLEN 256
static const u8 gost_R3410_2001_TestParamSet_curve_order[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x50, 0xfe, 0x8a, 0x18, 0x92, 0x97, 0x61, 0x54,
	0xc5, 0x9c, 0xfc, 0x19, 0x3a, 0xcc, 0xf5, 0xb3,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_curve_order);

static const u8 gost_R3410_2001_TestParamSet_gx[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_gx);

static const u8 gost_R3410_2001_TestParamSet_gy[] = {
	0x08, 0xe2, 0xa8, 0xa0, 0xe6, 0x51, 0x47, 0xd4,
	0xbd, 0x63, 0x16, 0x03, 0x0e, 0x16, 0xd1, 0x9c,
	0x85, 0xc9, 0x7f, 0x0a, 0x9c, 0xa2, 0x67, 0x12,
	0x2b, 0x96, 0xab, 0xbc, 0xea, 0x7e, 0x8f, 0xc8,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_gy);

static const u8 gost_R3410_2001_TestParamSet_gz[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_gz);

static const u8 gost_R3410_2001_TestParamSet_gen_order[] = {
	0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x50, 0xfe, 0x8a, 0x18, 0x92, 0x97, 0x61, 0x54,
	0xc5, 0x9c, 0xfc, 0x19, 0x3a, 0xcc, 0xf5, 0xb3,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_gen_order);

#define CURVE_GOST_R3410_2001_TESTPARAMSET_Q_BITLEN 256
static const u8 gost_R3410_2001_TestParamSet_gen_order_bitlen[] = {
	0x01, 0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_gen_order_bitlen);

static const u8 gost_R3410_2001_TestParamSet_cofactor[] = {
	0x01,
};

TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_cofactor);

static const u8 gost_R3410_2001_TestParamSet_alpha_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2001_TestParamSet_alpha_montgomery, 0);

static const u8 gost_R3410_2001_TestParamSet_gamma_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2001_TestParamSet_gamma_montgomery, 0);

static const u8 gost_R3410_2001_TestParamSet_alpha_edwards[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2001_TestParamSet_alpha_edwards, 0);

static const u8 gost_R3410_2001_TestParamSet_name[] = "GOST_R3410_2001_TESTPARAMSET";
TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_name);

static const u8 gost_R3410_2001_TestParamSet_oid[] = "1.2.643.2.2.35.0";
TO_EC_STR_PARAM(gost_R3410_2001_TestParamSet_oid);

static const ec_str_params gost_R3410_2001_TestParamSet_str_params = {
	.p = &gost_R3410_2001_TestParamSet_p_str_param,
	.p_bitlen = &gost_R3410_2001_TestParamSet_p_bitlen_str_param,
	.r = &gost_R3410_2001_TestParamSet_r_str_param,
	.r_square = &gost_R3410_2001_TestParamSet_r_square_str_param,
	.mpinv = &gost_R3410_2001_TestParamSet_mpinv_str_param,
	.p_shift = &gost_R3410_2001_TestParamSet_p_shift_str_param,
	.p_normalized = &gost_R3410_2001_TestParamSet_p_normalized_str_param,
	.p_reciprocal = &gost_R3410_2001_TestParamSet_p_reciprocal_str_param,
	.a = &gost_R3410_2001_TestParamSet_a_str_param,
	.b = &gost_R3410_2001_TestParamSet_b_str_param,
	.curve_order = &gost_R3410_2001_TestParamSet_curve_order_str_param,
	.gx = &gost_R3410_2001_TestParamSet_gx_str_param,
	.gy = &gost_R3410_2001_TestParamSet_gy_str_param,
	.gz = &gost_R3410_2001_TestParamSet_gz_str_param,
	.gen_order = &gost_R3410_2001_TestParamSet_gen_order_str_param,
	.gen_order_bitlen = &gost_R3410_2001_TestParamSet_gen_order_bitlen_str_param,
	.cofactor = &gost_R3410_2001_TestParamSet_cofactor_str_param,
	.alpha_montgomery = &gost_R3410_2001_TestParamSet_alpha_montgomery_str_param,
	.gamma_montgomery = &gost_R3410_2001_TestParamSet_gamma_montgomery_str_param,
	.alpha_edwards = &gost_R3410_2001_TestParamSet_alpha_edwards_str_param,
	.oid = &gost_R3410_2001_TestParamSet_oid_str_param,
	.name = &gost_R3410_2001_TestParamSet_name_str_param,
};

/*
 * Compute max bit length of all curves for p and q
 */
#ifndef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN    0
#endif
#if (CURVES_MAX_P_BIT_LEN < CURVE_GOST_R3410_2001_TESTPARAMSET_P_BITLEN)
#undef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN CURVE_GOST_R3410_2001_TESTPARAMSET_P_BITLEN
#endif
#ifndef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN    0
#endif
#if (CURVES_MAX_Q_BIT_LEN < CURVE_GOST_R3410_2001_TESTPARAMSET_Q_BITLEN)
#undef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN CURVE_GOST_R3410_2001_TESTPARAMSET_Q_BITLEN
#endif
#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN    0
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_GOST_R3410_2001_TESTPARAMSET_CURVE_ORDER_BITLEN)
#undef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_GOST_R3410_2001_TESTPARAMSET_CURVE_ORDER_BITLEN
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
#if (MAX_CURVE_NAME_LEN < 44)
#undef MAX_CURVE_NAME_LEN
#define MAX_CURVE_NAME_LEN 44
#endif

#endif /* __EC_PARAMS_GOST_R3410_2001_TESTPARAMSET_H__ */

#endif /* WITH_CURVE_GOST_R3410_2001_TESTPARAMSET */
