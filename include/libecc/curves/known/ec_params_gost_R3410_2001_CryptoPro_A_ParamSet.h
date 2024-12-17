#include <libecc/lib_ecc_config.h>
#ifdef WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET

#ifndef __EC_PARAMS_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET_H__
#define __EC_PARAMS_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET_H__
#include <libecc/curves/known/ec_params_external.h>
static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_p[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x97,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_p);

#define CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET_P_BITLEN 256
static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_p_bitlen[] = {
	0x01, 0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_p_bitlen);

#if (WORD_BYTES == 8)     /* 64-bit words */
static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_r[] = {
	0x02, 0x69,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_r);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_r_square[] = {
	0x05, 0xcf, 0x11,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_r_square);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_mpinv[] = {
	0x46, 0xf3, 0x23, 0x44, 0x75, 0xd5, 0xad, 0xd9,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_mpinv);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_p_shift);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x97,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_p_normalized);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_p_reciprocal);

#elif (WORD_BYTES == 4)   /* 32-bit words */
static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_r[] = {
	0x02, 0x69,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_r);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_r_square[] = {
	0x05, 0xcf, 0x11,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_r_square);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_mpinv[] = {
	0x75, 0xd5, 0xad, 0xd9,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_mpinv);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_p_shift);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x97,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_p_normalized);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_p_reciprocal);

#elif (WORD_BYTES == 2)   /* 16-bit words */
static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_r[] = {
	0x02, 0x69,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_r);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_r_square[] = {
	0x05, 0xcf, 0x11,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_r_square);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_mpinv[] = {
	0xad, 0xd9,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_mpinv);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_p_shift);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x97,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_p_normalized);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_p_reciprocal);

#else                     /* unknown word size */
#error "Unsupported word size"
#endif

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_a[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfd, 0x94,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_a);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_b[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa6,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_b);

#define CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET_CURVE_ORDER_BITLEN 256
static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_curve_order[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x6c, 0x61, 0x10, 0x70, 0x99, 0x5a, 0xd1, 0x00,
	0x45, 0x84, 0x1b, 0x09, 0xb7, 0x61, 0xb8, 0x93,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_curve_order);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_gx[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_gx);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_gy[] = {
	0x8d, 0x91, 0xe4, 0x71, 0xe0, 0x98, 0x9c, 0xda,
	0x27, 0xdf, 0x50, 0x5a, 0x45, 0x3f, 0x2b, 0x76,
	0x35, 0x29, 0x4f, 0x2d, 0xdf, 0x23, 0xe3, 0xb1,
	0x22, 0xac, 0xc9, 0x9c, 0x9e, 0x9f, 0x1e, 0x14,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_gy);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_gz[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_gz);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_gen_order[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x6c, 0x61, 0x10, 0x70, 0x99, 0x5a, 0xd1, 0x00,
	0x45, 0x84, 0x1b, 0x09, 0xb7, 0x61, 0xb8, 0x93,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_gen_order);

#define CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET_Q_BITLEN 256
static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_gen_order_bitlen[] = {
	0x01, 0x00,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_gen_order_bitlen);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_cofactor[] = {
	0x01,
};

TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_cofactor);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_alpha_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2001_CryptoPro_A_ParamSet_alpha_montgomery, 0);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_gamma_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2001_CryptoPro_A_ParamSet_gamma_montgomery, 0);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_alpha_edwards[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(gost_R3410_2001_CryptoPro_A_ParamSet_alpha_edwards, 0);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_name[] = "GOST_R3410_2001_CRYPTOPRO_A_PARAMSET";
TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_name);

static const u8 gost_R3410_2001_CryptoPro_A_ParamSet_oid[] = "1.2.643.2.2.35.1";
TO_EC_STR_PARAM(gost_R3410_2001_CryptoPro_A_ParamSet_oid);

static const ec_str_params gost_R3410_2001_CryptoPro_A_ParamSet_str_params = {
	.p = &gost_R3410_2001_CryptoPro_A_ParamSet_p_str_param,
	.p_bitlen = &gost_R3410_2001_CryptoPro_A_ParamSet_p_bitlen_str_param,
	.r = &gost_R3410_2001_CryptoPro_A_ParamSet_r_str_param,
	.r_square = &gost_R3410_2001_CryptoPro_A_ParamSet_r_square_str_param,
	.mpinv = &gost_R3410_2001_CryptoPro_A_ParamSet_mpinv_str_param,
	.p_shift = &gost_R3410_2001_CryptoPro_A_ParamSet_p_shift_str_param,
	.p_normalized = &gost_R3410_2001_CryptoPro_A_ParamSet_p_normalized_str_param,
	.p_reciprocal = &gost_R3410_2001_CryptoPro_A_ParamSet_p_reciprocal_str_param,
	.a = &gost_R3410_2001_CryptoPro_A_ParamSet_a_str_param,
	.b = &gost_R3410_2001_CryptoPro_A_ParamSet_b_str_param,
	.curve_order = &gost_R3410_2001_CryptoPro_A_ParamSet_curve_order_str_param,
	.gx = &gost_R3410_2001_CryptoPro_A_ParamSet_gx_str_param,
	.gy = &gost_R3410_2001_CryptoPro_A_ParamSet_gy_str_param,
	.gz = &gost_R3410_2001_CryptoPro_A_ParamSet_gz_str_param,
	.gen_order = &gost_R3410_2001_CryptoPro_A_ParamSet_gen_order_str_param,
	.gen_order_bitlen = &gost_R3410_2001_CryptoPro_A_ParamSet_gen_order_bitlen_str_param,
	.cofactor = &gost_R3410_2001_CryptoPro_A_ParamSet_cofactor_str_param,
	.alpha_montgomery = &gost_R3410_2001_CryptoPro_A_ParamSet_alpha_montgomery_str_param,
	.gamma_montgomery = &gost_R3410_2001_CryptoPro_A_ParamSet_gamma_montgomery_str_param,
	.alpha_edwards = &gost_R3410_2001_CryptoPro_A_ParamSet_alpha_edwards_str_param,
	.oid = &gost_R3410_2001_CryptoPro_A_ParamSet_oid_str_param,
	.name = &gost_R3410_2001_CryptoPro_A_ParamSet_name_str_param,
};

/*
 * Compute max bit length of all curves for p and q
 */
#ifndef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN    0
#endif
#if (CURVES_MAX_P_BIT_LEN < CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET_P_BITLEN)
#undef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET_P_BITLEN
#endif
#ifndef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN    0
#endif
#if (CURVES_MAX_Q_BIT_LEN < CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET_Q_BITLEN)
#undef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET_Q_BITLEN
#endif
#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN    0
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET_CURVE_ORDER_BITLEN)
#undef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET_CURVE_ORDER_BITLEN
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

#endif /* __EC_PARAMS_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET_H__ */

#endif /* WITH_CURVE_GOST_R3410_2001_CRYPTOPRO_A_PARAMSET */
