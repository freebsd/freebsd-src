#include <libecc/lib_ecc_config.h>
#ifdef WITH_CURVE_BIGN256V1

#ifndef __EC_PARAMS_BIGN256V1_H__
#define __EC_PARAMS_BIGN256V1_H__
#include <libecc/curves/known/ec_params_external.h>
static const u8 bign256v1_p[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x43,
};

TO_EC_STR_PARAM(bign256v1_p);

#define CURVE_BIGN256V1_P_BITLEN 256
static const u8 bign256v1_p_bitlen[] = {
	0x01, 0x00,
};

TO_EC_STR_PARAM(bign256v1_p_bitlen);

#if (WORD_BYTES == 8)     /* 64-bit words */
static const u8 bign256v1_r[] = {
	0xbd,
};

TO_EC_STR_PARAM(bign256v1_r);

static const u8 bign256v1_r_square[] = {
	0x8b, 0x89,
};

TO_EC_STR_PARAM(bign256v1_r_square);

static const u8 bign256v1_mpinv[] = {
	0xa5, 0x3f, 0xa9, 0x4f, 0xea, 0x53, 0xfa, 0x95,
};

TO_EC_STR_PARAM(bign256v1_mpinv);

static const u8 bign256v1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(bign256v1_p_shift);

static const u8 bign256v1_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x43,
};

TO_EC_STR_PARAM(bign256v1_p_normalized);

static const u8 bign256v1_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(bign256v1_p_reciprocal);

#elif (WORD_BYTES == 4)   /* 32-bit words */
static const u8 bign256v1_r[] = {
	0xbd,
};

TO_EC_STR_PARAM(bign256v1_r);

static const u8 bign256v1_r_square[] = {
	0x8b, 0x89,
};

TO_EC_STR_PARAM(bign256v1_r_square);

static const u8 bign256v1_mpinv[] = {
	0xea, 0x53, 0xfa, 0x95,
};

TO_EC_STR_PARAM(bign256v1_mpinv);

static const u8 bign256v1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(bign256v1_p_shift);

static const u8 bign256v1_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x43,
};

TO_EC_STR_PARAM(bign256v1_p_normalized);

static const u8 bign256v1_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(bign256v1_p_reciprocal);

#elif (WORD_BYTES == 2)   /* 16-bit words */
static const u8 bign256v1_r[] = {
	0xbd,
};

TO_EC_STR_PARAM(bign256v1_r);

static const u8 bign256v1_r_square[] = {
	0x8b, 0x89,
};

TO_EC_STR_PARAM(bign256v1_r_square);

static const u8 bign256v1_mpinv[] = {
	0xfa, 0x95,
};

TO_EC_STR_PARAM(bign256v1_mpinv);

static const u8 bign256v1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(bign256v1_p_shift);

static const u8 bign256v1_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x43,
};

TO_EC_STR_PARAM(bign256v1_p_normalized);

static const u8 bign256v1_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(bign256v1_p_reciprocal);

#else                     /* unknown word size */
#error "Unsupported word size"
#endif

static const u8 bign256v1_a[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x40,
};

TO_EC_STR_PARAM(bign256v1_a);

static const u8 bign256v1_b[] = {
	0x77, 0xce, 0x6c, 0x15, 0x15, 0xf3, 0xa8, 0xed,
	0xd2, 0xc1, 0x3a, 0xab, 0xe4, 0xd8, 0xfb, 0xbe,
	0x4c, 0xf5, 0x50, 0x69, 0x97, 0x8b, 0x92, 0x53,
	0xb2, 0x2e, 0x7d, 0x6b, 0xd6, 0x9c, 0x03, 0xf1,
};

TO_EC_STR_PARAM(bign256v1_b);

#define CURVE_BIGN256V1_CURVE_ORDER_BITLEN 256
static const u8 bign256v1_curve_order[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xd9, 0x5c, 0x8e, 0xd6, 0x0d, 0xfb, 0x4d, 0xfc,
	0x7e, 0x5a, 0xbf, 0x99, 0x26, 0x3d, 0x66, 0x07,
};

TO_EC_STR_PARAM(bign256v1_curve_order);

static const u8 bign256v1_gx[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

TO_EC_STR_PARAM(bign256v1_gx);

static const u8 bign256v1_gy[] = {
	0x6b, 0xf7, 0xfc, 0x3c, 0xfb, 0x16, 0xd6, 0x9f,
	0x5c, 0xe4, 0xc9, 0xa3, 0x51, 0xd6, 0x83, 0x5d,
	0x78, 0x91, 0x39, 0x66, 0xc4, 0x08, 0xf6, 0x52,
	0x1e, 0x29, 0xcf, 0x18, 0x04, 0x51, 0x6a, 0x93,
};

TO_EC_STR_PARAM(bign256v1_gy);

static const u8 bign256v1_gz[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

TO_EC_STR_PARAM(bign256v1_gz);

static const u8 bign256v1_gen_order[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xd9, 0x5c, 0x8e, 0xd6, 0x0d, 0xfb, 0x4d, 0xfc,
	0x7e, 0x5a, 0xbf, 0x99, 0x26, 0x3d, 0x66, 0x07,
};

TO_EC_STR_PARAM(bign256v1_gen_order);

#define CURVE_BIGN256V1_Q_BITLEN 256
static const u8 bign256v1_gen_order_bitlen[] = {
	0x01, 0x00,
};

TO_EC_STR_PARAM(bign256v1_gen_order_bitlen);

static const u8 bign256v1_cofactor[] = {
	0x01,
};

TO_EC_STR_PARAM(bign256v1_cofactor);

static const u8 bign256v1_alpha_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(bign256v1_alpha_montgomery, 0);

static const u8 bign256v1_gamma_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(bign256v1_gamma_montgomery, 0);

static const u8 bign256v1_alpha_edwards[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(bign256v1_alpha_edwards, 0);

static const u8 bign256v1_name[] = "BIGN256V1";
TO_EC_STR_PARAM(bign256v1_name);

static const u8 bign256v1_oid[] = "1.2.112.0.2.0.34.101.45.3.1";
TO_EC_STR_PARAM(bign256v1_oid);

static const ec_str_params bign256v1_str_params = {
	.p = &bign256v1_p_str_param,
	.p_bitlen = &bign256v1_p_bitlen_str_param,
	.r = &bign256v1_r_str_param,
	.r_square = &bign256v1_r_square_str_param,
	.mpinv = &bign256v1_mpinv_str_param,
	.p_shift = &bign256v1_p_shift_str_param,
	.p_normalized = &bign256v1_p_normalized_str_param,
	.p_reciprocal = &bign256v1_p_reciprocal_str_param,
	.a = &bign256v1_a_str_param,
	.b = &bign256v1_b_str_param,
	.curve_order = &bign256v1_curve_order_str_param,
	.gx = &bign256v1_gx_str_param,
	.gy = &bign256v1_gy_str_param,
	.gz = &bign256v1_gz_str_param,
	.gen_order = &bign256v1_gen_order_str_param,
	.gen_order_bitlen = &bign256v1_gen_order_bitlen_str_param,
	.cofactor = &bign256v1_cofactor_str_param,
	.alpha_montgomery = &bign256v1_alpha_montgomery_str_param,
	.gamma_montgomery = &bign256v1_gamma_montgomery_str_param,
	.alpha_edwards = &bign256v1_alpha_edwards_str_param,
	.oid = &bign256v1_oid_str_param,
	.name = &bign256v1_name_str_param,
};

/*
 * Compute max bit length of all curves for p and q
 */
#ifndef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN    0
#endif
#if (CURVES_MAX_P_BIT_LEN < CURVE_BIGN256V1_P_BITLEN)
#undef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN CURVE_BIGN256V1_P_BITLEN
#endif
#ifndef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN    0
#endif
#if (CURVES_MAX_Q_BIT_LEN < CURVE_BIGN256V1_Q_BITLEN)
#undef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN CURVE_BIGN256V1_Q_BITLEN
#endif
#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN    0
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_BIGN256V1_CURVE_ORDER_BITLEN)
#undef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_BIGN256V1_CURVE_ORDER_BITLEN
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
#if (MAX_CURVE_NAME_LEN < 23)
#undef MAX_CURVE_NAME_LEN
#define MAX_CURVE_NAME_LEN 23
#endif

#endif /* __EC_PARAMS_BIGN256V1_H__ */

#endif /* WITH_CURVE_BIGN256V1 */
