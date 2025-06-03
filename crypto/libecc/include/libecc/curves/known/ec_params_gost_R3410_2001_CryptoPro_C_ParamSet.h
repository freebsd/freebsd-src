#include <libecc/lib_ecc_config.h>
#ifdef WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET

#ifndef __EC_PARAMS_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET_H__
#define __EC_PARAMS_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET_H__
#include <libecc/curves/known/ec_params_external.h>
static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_p[] = {
	0x9b, 0x9f, 0x60, 0x5f, 0x5a, 0x85, 0x81, 0x07,
	0xab, 0x1e, 0xc8, 0x5e, 0x6b, 0x41, 0xc8, 0xaa,
	0xcf, 0x84, 0x6e, 0x86, 0x78, 0x90, 0x51, 0xd3,
	0x79, 0x98, 0xf7, 0xb9, 0x02, 0x2d, 0x75, 0x9b,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_p);

#define CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET_P_BITLEN 256
static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_p_bitlen[] = {
	0x01, 0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_p_bitlen);

#if (WORD_BYTES == 8)     /* 64-bit words */
static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_r[] = {
	0x64, 0x60, 0x9f, 0xa0, 0xa5, 0x7a, 0x7e, 0xf8,
	0x54, 0xe1, 0x37, 0xa1, 0x94, 0xbe, 0x37, 0x55,
	0x30, 0x7b, 0x91, 0x79, 0x87, 0x6f, 0xae, 0x2c,
	0x86, 0x67, 0x08, 0x46, 0xfd, 0xd2, 0x8a, 0x65,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_r);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_r_square[] = {
	0x80, 0x7a, 0x39, 0x4e, 0xde, 0x09, 0x76, 0x52,
	0x18, 0x63, 0x04, 0x21, 0x28, 0x49, 0xc0, 0x7b,
	0x10, 0x17, 0xbb, 0x39, 0xc2, 0xd3, 0x46, 0xc5,
	0x40, 0x99, 0x73, 0xb4, 0xc4, 0x27, 0xfc, 0xea,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_r_square);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_mpinv[] = {
	0xdf, 0x6e, 0x6c, 0x2c, 0x72, 0x7c, 0x17, 0x6d,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_mpinv);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_p_shift);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_p_normalized[] = {
	0x9b, 0x9f, 0x60, 0x5f, 0x5a, 0x85, 0x81, 0x07,
	0xab, 0x1e, 0xc8, 0x5e, 0x6b, 0x41, 0xc8, 0xaa,
	0xcf, 0x84, 0x6e, 0x86, 0x78, 0x90, 0x51, 0xd3,
	0x79, 0x98, 0xf7, 0xb9, 0x02, 0x2d, 0x75, 0x9b,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_p_normalized);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_p_reciprocal[] = {
	0xa5, 0x1f, 0x17, 0x61, 0x61, 0xf1, 0xd7, 0x34,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_p_reciprocal);

#elif (WORD_BYTES == 4)   /* 32-bit words */
static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_r[] = {
	0x64, 0x60, 0x9f, 0xa0, 0xa5, 0x7a, 0x7e, 0xf8,
	0x54, 0xe1, 0x37, 0xa1, 0x94, 0xbe, 0x37, 0x55,
	0x30, 0x7b, 0x91, 0x79, 0x87, 0x6f, 0xae, 0x2c,
	0x86, 0x67, 0x08, 0x46, 0xfd, 0xd2, 0x8a, 0x65,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_r);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_r_square[] = {
	0x80, 0x7a, 0x39, 0x4e, 0xde, 0x09, 0x76, 0x52,
	0x18, 0x63, 0x04, 0x21, 0x28, 0x49, 0xc0, 0x7b,
	0x10, 0x17, 0xbb, 0x39, 0xc2, 0xd3, 0x46, 0xc5,
	0x40, 0x99, 0x73, 0xb4, 0xc4, 0x27, 0xfc, 0xea,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_r_square);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_mpinv[] = {
	0x72, 0x7c, 0x17, 0x6d,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_mpinv);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_p_shift);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_p_normalized[] = {
	0x9b, 0x9f, 0x60, 0x5f, 0x5a, 0x85, 0x81, 0x07,
	0xab, 0x1e, 0xc8, 0x5e, 0x6b, 0x41, 0xc8, 0xaa,
	0xcf, 0x84, 0x6e, 0x86, 0x78, 0x90, 0x51, 0xd3,
	0x79, 0x98, 0xf7, 0xb9, 0x02, 0x2d, 0x75, 0x9b,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_p_normalized);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_p_reciprocal[] = {
	0xa5, 0x1f, 0x17, 0x61,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_p_reciprocal);

#elif (WORD_BYTES == 2)   /* 16-bit words */
static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_r[] = {
	0x64, 0x60, 0x9f, 0xa0, 0xa5, 0x7a, 0x7e, 0xf8,
	0x54, 0xe1, 0x37, 0xa1, 0x94, 0xbe, 0x37, 0x55,
	0x30, 0x7b, 0x91, 0x79, 0x87, 0x6f, 0xae, 0x2c,
	0x86, 0x67, 0x08, 0x46, 0xfd, 0xd2, 0x8a, 0x65,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_r);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_r_square[] = {
	0x80, 0x7a, 0x39, 0x4e, 0xde, 0x09, 0x76, 0x52,
	0x18, 0x63, 0x04, 0x21, 0x28, 0x49, 0xc0, 0x7b,
	0x10, 0x17, 0xbb, 0x39, 0xc2, 0xd3, 0x46, 0xc5,
	0x40, 0x99, 0x73, 0xb4, 0xc4, 0x27, 0xfc, 0xea,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_r_square);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_mpinv[] = {
	0x17, 0x6d,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_mpinv);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_p_shift);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_p_normalized[] = {
	0x9b, 0x9f, 0x60, 0x5f, 0x5a, 0x85, 0x81, 0x07,
	0xab, 0x1e, 0xc8, 0x5e, 0x6b, 0x41, 0xc8, 0xaa,
	0xcf, 0x84, 0x6e, 0x86, 0x78, 0x90, 0x51, 0xd3,
	0x79, 0x98, 0xf7, 0xb9, 0x02, 0x2d, 0x75, 0x9b,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_p_normalized);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_p_reciprocal[] = {
	0xa5, 0x1f,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_p_reciprocal);

#else                     /* unknown word size */
#error "Unsupported word size"
#endif

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_a[] = {
	0x9b, 0x9f, 0x60, 0x5f, 0x5a, 0x85, 0x81, 0x07,
	0xab, 0x1e, 0xc8, 0x5e, 0x6b, 0x41, 0xc8, 0xaa,
	0xcf, 0x84, 0x6e, 0x86, 0x78, 0x90, 0x51, 0xd3,
	0x79, 0x98, 0xf7, 0xb9, 0x02, 0x2d, 0x75, 0x98,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_a);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_b[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x5a,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_b);

#define CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET_CURVE_ORDER_BITLEN 256
static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_curve_order[] = {
	0x9b, 0x9f, 0x60, 0x5f, 0x5a, 0x85, 0x81, 0x07,
	0xab, 0x1e, 0xc8, 0x5e, 0x6b, 0x41, 0xc8, 0xaa,
	0x58, 0x2c, 0xa3, 0x51, 0x1e, 0xdd, 0xfb, 0x74,
	0xf0, 0x2f, 0x3a, 0x65, 0x98, 0x98, 0x0b, 0xb9,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_curve_order);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_gx[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_gx);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_gy[] = {
	0x41, 0xec, 0xe5, 0x57, 0x43, 0x71, 0x1a, 0x8c,
	0x3c, 0xbf, 0x37, 0x83, 0xcd, 0x08, 0xc0, 0xee,
	0x4d, 0x4d, 0xc4, 0x40, 0xd4, 0x64, 0x1a, 0x8f,
	0x36, 0x6e, 0x55, 0x0d, 0xfd, 0xb3, 0xbb, 0x67,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_gy);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_gz[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_gz);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_gen_order[] = {
	0x9b, 0x9f, 0x60, 0x5f, 0x5a, 0x85, 0x81, 0x07,
	0xab, 0x1e, 0xc8, 0x5e, 0x6b, 0x41, 0xc8, 0xaa,
	0x58, 0x2c, 0xa3, 0x51, 0x1e, 0xdd, 0xfb, 0x74,
	0xf0, 0x2f, 0x3a, 0x65, 0x98, 0x98, 0x0b, 0xb9,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_gen_order);

#define CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET_Q_BITLEN 256
static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_gen_order_bitlen[] = {
	0x01, 0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_gen_order_bitlen);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_cofactor[] = {
	0x01,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_cofactor);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_alpha_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2001_CryptoPro_C_ParamSet_alpha_montgomery, 0);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_gamma_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2001_CryptoPro_C_ParamSet_gamma_montgomery, 0);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_alpha_edwards[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2001_CryptoPro_C_ParamSet_alpha_edwards, 0);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_name[] = "GOST_R3410_2001_CRYPTOPRO_C_PARAMSET";
TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_name);

static const u8 gost_R3410_2001_CryptoPro_C_ParamSet_oid[] = "1.2.643.2.2.35.3";
TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_C_ParamSet_oid);

static const ec_str_params gost_R3410_2001_CryptoPro_C_ParamSet_str_params = {
	.p = &gost_R3410_2001_CryptoPro_C_ParamSet_p_str_param,
	.p_bitlen = &gost_R3410_2001_CryptoPro_C_ParamSet_p_bitlen_str_param,
	.r = &gost_R3410_2001_CryptoPro_C_ParamSet_r_str_param,
	.r_square = &gost_R3410_2001_CryptoPro_C_ParamSet_r_square_str_param,
	.mpinv = &gost_R3410_2001_CryptoPro_C_ParamSet_mpinv_str_param,
	.p_shift = &gost_R3410_2001_CryptoPro_C_ParamSet_p_shift_str_param,
	.p_normalized = &gost_R3410_2001_CryptoPro_C_ParamSet_p_normalized_str_param,
	.p_reciprocal = &gost_R3410_2001_CryptoPro_C_ParamSet_p_reciprocal_str_param,
	.a = &gost_R3410_2001_CryptoPro_C_ParamSet_a_str_param,
	.b = &gost_R3410_2001_CryptoPro_C_ParamSet_b_str_param,
	.curve_order = &gost_R3410_2001_CryptoPro_C_ParamSet_curve_order_str_param,
	.gx = &gost_R3410_2001_CryptoPro_C_ParamSet_gx_str_param,
	.gy = &gost_R3410_2001_CryptoPro_C_ParamSet_gy_str_param,
	.gz = &gost_R3410_2001_CryptoPro_C_ParamSet_gz_str_param,
	.gen_order = &gost_R3410_2001_CryptoPro_C_ParamSet_gen_order_str_param,
	.gen_order_bitlen = &gost_R3410_2001_CryptoPro_C_ParamSet_gen_order_bitlen_str_param,
	.cofactor = &gost_R3410_2001_CryptoPro_C_ParamSet_cofactor_str_param,
	.alpha_montgomery = &gost_R3410_2001_CryptoPro_C_ParamSet_alpha_montgomery_str_param,
	.gamma_montgomery = &gost_R3410_2001_CryptoPro_C_ParamSet_gamma_montgomery_str_param,
	.alpha_edwards = &gost_R3410_2001_CryptoPro_C_ParamSet_alpha_edwards_str_param,
	.oid = &gost_R3410_2001_CryptoPro_C_ParamSet_oid_str_param,
	.name = &gost_R3410_2001_CryptoPro_C_ParamSet_name_str_param,
};

/*
 * Compute max bit length of all curves for p and q
 */
#ifndef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN    0
#endif
#if (CURVES_MAX_P_BIT_LEN < CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET_P_BITLEN)
#undef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET_P_BITLEN
#endif
#ifndef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN    0
#endif
#if (CURVES_MAX_Q_BIT_LEN < CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET_Q_BITLEN)
#undef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET_Q_BITLEN
#endif
#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN    0
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET_CURVE_ORDER_BITLEN)
#undef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET_CURVE_ORDER_BITLEN
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

#endif /* __EC_PARAMS_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET_H__ */

#endif /* WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_C_PARAMSET */
