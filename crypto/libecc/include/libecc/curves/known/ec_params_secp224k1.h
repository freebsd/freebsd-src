#include <libecc/lib_ecc_config.h>
#ifdef WITH_CURVE_SECP224K1

#ifndef __EC_PARAMS_SECP224K1_H__
#define __EC_PARAMS_SECP224K1_H__
#include <libecc/curves/known/ec_params_external.h>
static const u8 secp224k1_p[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	0xff, 0xff, 0xe5, 0x6d,
};

TO_EC_STR_PARAM(secp224k1_p);

#define CURVE_SECP224K1_P_BITLEN 224
static const u8 secp224k1_p_bitlen[] = {
	0xe0,
};

TO_EC_STR_PARAM(secp224k1_p_bitlen);

#if (WORD_BYTES == 8)     /* 64-bit words */
static const u8 secp224k1_r[] = {
	0x01, 0x00, 0x00, 0x1a, 0x93, 0x00, 0x00, 0x00,
	0x00,
};

TO_EC_STR_PARAM(secp224k1_r);

static const u8 secp224k1_r_square[] = {
	0x01, 0x00, 0x00, 0x35, 0x26, 0x02, 0xc2, 0x30,
	0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,
};

TO_EC_STR_PARAM(secp224k1_r_square);

static const u8 secp224k1_mpinv[] = {
	0x5a, 0x92, 0xa0, 0x0a, 0x19, 0x8d, 0x13, 0x9b,
};

TO_EC_STR_PARAM(secp224k1_mpinv);

static const u8 secp224k1_p_shift[] = {
	0x20,
};

TO_EC_STR_PARAM(secp224k1_p_shift);

static const u8 secp224k1_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	0xff, 0xff, 0xe5, 0x6d, 0x00, 0x00, 0x00, 0x00,
};

TO_EC_STR_PARAM(secp224k1_p_normalized);

static const u8 secp224k1_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(secp224k1_p_reciprocal);

#elif (WORD_BYTES == 4)   /* 32-bit words */
static const u8 secp224k1_r[] = {
	0x01, 0x00, 0x00, 0x1a, 0x93,
};

TO_EC_STR_PARAM(secp224k1_r);

static const u8 secp224k1_r_square[] = {
	0x01, 0x00, 0x00, 0x35, 0x26, 0x02, 0xc2, 0x30,
	0x69,
};

TO_EC_STR_PARAM(secp224k1_r_square);

static const u8 secp224k1_mpinv[] = {
	0x19, 0x8d, 0x13, 0x9b,
};

TO_EC_STR_PARAM(secp224k1_mpinv);

static const u8 secp224k1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(secp224k1_p_shift);

static const u8 secp224k1_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	0xff, 0xff, 0xe5, 0x6d,
};

TO_EC_STR_PARAM(secp224k1_p_normalized);

static const u8 secp224k1_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(secp224k1_p_reciprocal);

#elif (WORD_BYTES == 2)   /* 16-bit words */
static const u8 secp224k1_r[] = {
	0x01, 0x00, 0x00, 0x1a, 0x93,
};

TO_EC_STR_PARAM(secp224k1_r);

static const u8 secp224k1_r_square[] = {
	0x01, 0x00, 0x00, 0x35, 0x26, 0x02, 0xc2, 0x30,
	0x69,
};

TO_EC_STR_PARAM(secp224k1_r_square);

static const u8 secp224k1_mpinv[] = {
	0x13, 0x9b,
};

TO_EC_STR_PARAM(secp224k1_mpinv);

static const u8 secp224k1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(secp224k1_p_shift);

static const u8 secp224k1_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	0xff, 0xff, 0xe5, 0x6d,
};

TO_EC_STR_PARAM(secp224k1_p_normalized);

static const u8 secp224k1_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(secp224k1_p_reciprocal);

#else                     /* unknown word size */
#error "Unsupported word size"
#endif

static const u8 secp224k1_a[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
};

TO_EC_STR_PARAM(secp224k1_a);

static const u8 secp224k1_b[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x05,
};

TO_EC_STR_PARAM(secp224k1_b);

#define CURVE_SECP224K1_CURVE_ORDER_BITLEN 225
static const u8 secp224k1_curve_order[] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xdc,
	0xe8, 0xd2, 0xec, 0x61, 0x84, 0xca, 0xf0, 0xa9,
	0x71, 0x76, 0x9f, 0xb1, 0xf7,
};

TO_EC_STR_PARAM(secp224k1_curve_order);

static const u8 secp224k1_gx[] = {
	0xa1, 0x45, 0x5b, 0x33, 0x4d, 0xf0, 0x99, 0xdf,
	0x30, 0xfc, 0x28, 0xa1, 0x69, 0xa4, 0x67, 0xe9,
	0xe4, 0x70, 0x75, 0xa9, 0x0f, 0x7e, 0x65, 0x0e,
	0xb6, 0xb7, 0xa4, 0x5c,
};

TO_EC_STR_PARAM(secp224k1_gx);

static const u8 secp224k1_gy[] = {
	0x7e, 0x08, 0x9f, 0xed, 0x7f, 0xba, 0x34, 0x42,
	0x82, 0xca, 0xfb, 0xd6, 0xf7, 0xe3, 0x19, 0xf7,
	0xc0, 0xb0, 0xbd, 0x59, 0xe2, 0xca, 0x4b, 0xdb,
	0x55, 0x6d, 0x61, 0xa5,
};

TO_EC_STR_PARAM(secp224k1_gy);

static const u8 secp224k1_gz[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x01,
};

TO_EC_STR_PARAM(secp224k1_gz);

static const u8 secp224k1_gen_order[] = {
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xdc,
	0xe8, 0xd2, 0xec, 0x61, 0x84, 0xca, 0xf0, 0xa9,
	0x71, 0x76, 0x9f, 0xb1, 0xf7,
};

TO_EC_STR_PARAM(secp224k1_gen_order);

#define CURVE_SECP224K1_Q_BITLEN 225
static const u8 secp224k1_gen_order_bitlen[] = {
	0xe1,
};

TO_EC_STR_PARAM(secp224k1_gen_order_bitlen);

static const u8 secp224k1_cofactor[] = {
	0x01,
};

TO_EC_STR_PARAM(secp224k1_cofactor);

static const u8 secp224k1_alpha_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(secp224k1_alpha_montgomery, 0);

static const u8 secp224k1_gamma_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(secp224k1_gamma_montgomery, 0);

static const u8 secp224k1_alpha_edwards[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(secp224k1_alpha_edwards, 0);

static const u8 secp224k1_name[] = "SECP224K1";
TO_EC_STR_PARAM(secp224k1_name);

static const u8 secp224k1_oid[] = "1.3.132.0.32";
TO_EC_STR_PARAM(secp224k1_oid);

static const ec_str_params secp224k1_str_params = {
	.p = &secp224k1_p_str_param,
	.p_bitlen = &secp224k1_p_bitlen_str_param,
	.r = &secp224k1_r_str_param,
	.r_square = &secp224k1_r_square_str_param,
	.mpinv = &secp224k1_mpinv_str_param,
	.p_shift = &secp224k1_p_shift_str_param,
	.p_normalized = &secp224k1_p_normalized_str_param,
	.p_reciprocal = &secp224k1_p_reciprocal_str_param,
	.a = &secp224k1_a_str_param,
	.b = &secp224k1_b_str_param,
	.curve_order = &secp224k1_curve_order_str_param,
	.gx = &secp224k1_gx_str_param,
	.gy = &secp224k1_gy_str_param,
	.gz = &secp224k1_gz_str_param,
	.gen_order = &secp224k1_gen_order_str_param,
	.gen_order_bitlen = &secp224k1_gen_order_bitlen_str_param,
	.cofactor = &secp224k1_cofactor_str_param,
	.alpha_montgomery = &secp224k1_alpha_montgomery_str_param,
	.gamma_montgomery = &secp224k1_gamma_montgomery_str_param,
	.alpha_edwards = &secp224k1_alpha_edwards_str_param,
	.oid = &secp224k1_oid_str_param,
	.name = &secp224k1_name_str_param,
};

/*
 * Compute max bit length of all curves for p and q
 */
#ifndef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN    0
#endif
#if (CURVES_MAX_P_BIT_LEN < CURVE_SECP224K1_P_BITLEN)
#undef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN CURVE_SECP224K1_P_BITLEN
#endif
#ifndef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN    0
#endif
#if (CURVES_MAX_Q_BIT_LEN < CURVE_SECP224K1_Q_BITLEN)
#undef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN CURVE_SECP224K1_Q_BITLEN
#endif
#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN    0
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_SECP224K1_CURVE_ORDER_BITLEN)
#undef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_SECP224K1_CURVE_ORDER_BITLEN
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

#endif /* __EC_PARAMS_SECP224K1_H__ */

#endif /* WITH_CURVE_SECP224K1 */
