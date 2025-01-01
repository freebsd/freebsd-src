#include <libecc/lib_ecc_config.h>
#ifdef WITH_CURVE_SECP192K1

#ifndef __EC_PARAMS_SECP192K1_H__
#define __EC_PARAMS_SECP192K1_H__
#include <libecc/curves/known/ec_params_external.h>
static const u8 secp192k1_p[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xee, 0x37,
};

TO_EC_STR_PARAM(secp192k1_p);

#define CURVE_SECP192K1_P_BITLEN 192
static const u8 secp192k1_p_bitlen[] = {
	0xc0,
};

TO_EC_STR_PARAM(secp192k1_p_bitlen);

#if (WORD_BYTES == 8)     /* 64-bit words */
static const u8 secp192k1_r[] = {
	0x01, 0x00, 0x00, 0x11, 0xc9,
};

TO_EC_STR_PARAM(secp192k1_r);

static const u8 secp192k1_r_square[] = {
	0x01, 0x00, 0x00, 0x23, 0x92, 0x01, 0x3c, 0x4f,
	0xd1,
};

TO_EC_STR_PARAM(secp192k1_r_square);

static const u8 secp192k1_mpinv[] = {
	0xf2, 0x7a, 0xe5, 0x5b, 0x74, 0x46, 0xd8, 0x79,
};

TO_EC_STR_PARAM(secp192k1_mpinv);

static const u8 secp192k1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(secp192k1_p_shift);

static const u8 secp192k1_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xee, 0x37,
};

TO_EC_STR_PARAM(secp192k1_p_normalized);

static const u8 secp192k1_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(secp192k1_p_reciprocal);

#elif (WORD_BYTES == 4)   /* 32-bit words */
static const u8 secp192k1_r[] = {
	0x01, 0x00, 0x00, 0x11, 0xc9,
};

TO_EC_STR_PARAM(secp192k1_r);

static const u8 secp192k1_r_square[] = {
	0x01, 0x00, 0x00, 0x23, 0x92, 0x01, 0x3c, 0x4f,
	0xd1,
};

TO_EC_STR_PARAM(secp192k1_r_square);

static const u8 secp192k1_mpinv[] = {
	0x74, 0x46, 0xd8, 0x79,
};

TO_EC_STR_PARAM(secp192k1_mpinv);

static const u8 secp192k1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(secp192k1_p_shift);

static const u8 secp192k1_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xee, 0x37,
};

TO_EC_STR_PARAM(secp192k1_p_normalized);

static const u8 secp192k1_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(secp192k1_p_reciprocal);

#elif (WORD_BYTES == 2)   /* 16-bit words */
static const u8 secp192k1_r[] = {
	0x01, 0x00, 0x00, 0x11, 0xc9,
};

TO_EC_STR_PARAM(secp192k1_r);

static const u8 secp192k1_r_square[] = {
	0x01, 0x00, 0x00, 0x23, 0x92, 0x01, 0x3c, 0x4f,
	0xd1,
};

TO_EC_STR_PARAM(secp192k1_r_square);

static const u8 secp192k1_mpinv[] = {
	0xd8, 0x79,
};

TO_EC_STR_PARAM(secp192k1_mpinv);

static const u8 secp192k1_p_shift[] = {
	0x00,
};

TO_EC_STR_PARAM(secp192k1_p_shift);

static const u8 secp192k1_p_normalized[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xee, 0x37,
};

TO_EC_STR_PARAM(secp192k1_p_normalized);

static const u8 secp192k1_p_reciprocal[] = {
	0x00,
};

TO_EC_STR_PARAM(secp192k1_p_reciprocal);

#else                     /* unknown word size */
#error "Unsupported word size"
#endif

static const u8 secp192k1_a[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

TO_EC_STR_PARAM(secp192k1_a);

static const u8 secp192k1_b[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
};

TO_EC_STR_PARAM(secp192k1_b);

#define CURVE_SECP192K1_CURVE_ORDER_BITLEN 192
static const u8 secp192k1_curve_order[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0x26, 0xf2, 0xfc, 0x17,
	0x0f, 0x69, 0x46, 0x6a, 0x74, 0xde, 0xfd, 0x8d,
};

TO_EC_STR_PARAM(secp192k1_curve_order);

static const u8 secp192k1_gx[] = {
	0xdb, 0x4f, 0xf1, 0x0e, 0xc0, 0x57, 0xe9, 0xae,
	0x26, 0xb0, 0x7d, 0x02, 0x80, 0xb7, 0xf4, 0x34,
	0x1d, 0xa5, 0xd1, 0xb1, 0xea, 0xe0, 0x6c, 0x7d,
};

TO_EC_STR_PARAM(secp192k1_gx);

static const u8 secp192k1_gy[] = {
	0x9b, 0x2f, 0x2f, 0x6d, 0x9c, 0x56, 0x28, 0xa7,
	0x84, 0x41, 0x63, 0xd0, 0x15, 0xbe, 0x86, 0x34,
	0x40, 0x82, 0xaa, 0x88, 0xd9, 0x5e, 0x2f, 0x9d,
};

TO_EC_STR_PARAM(secp192k1_gy);

static const u8 secp192k1_gz[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
};

TO_EC_STR_PARAM(secp192k1_gz);

static const u8 secp192k1_gen_order[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0x26, 0xf2, 0xfc, 0x17,
	0x0f, 0x69, 0x46, 0x6a, 0x74, 0xde, 0xfd, 0x8d,
};

TO_EC_STR_PARAM(secp192k1_gen_order);

#define CURVE_SECP192K1_Q_BITLEN 192
static const u8 secp192k1_gen_order_bitlen[] = {
	0xc0,
};

TO_EC_STR_PARAM(secp192k1_gen_order_bitlen);

static const u8 secp192k1_cofactor[] = {
	0x01,
};

TO_EC_STR_PARAM(secp192k1_cofactor);

static const u8 secp192k1_alpha_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(secp192k1_alpha_montgomery, 0);

static const u8 secp192k1_gamma_montgomery[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(secp192k1_gamma_montgomery, 0);

static const u8 secp192k1_alpha_edwards[] = {
	0x00,
};
TO_EC_STR_PARAM_FIXED_SIZE(secp192k1_alpha_edwards, 0);

static const u8 secp192k1_name[] = "SECP192K1";
TO_EC_STR_PARAM(secp192k1_name);

static const u8 secp192k1_oid[] = "1.3.132.0.31";
TO_EC_STR_PARAM(secp192k1_oid);

static const ec_str_params secp192k1_str_params = {
	.p = &secp192k1_p_str_param,
	.p_bitlen = &secp192k1_p_bitlen_str_param,
	.r = &secp192k1_r_str_param,
	.r_square = &secp192k1_r_square_str_param,
	.mpinv = &secp192k1_mpinv_str_param,
	.p_shift = &secp192k1_p_shift_str_param,
	.p_normalized = &secp192k1_p_normalized_str_param,
	.p_reciprocal = &secp192k1_p_reciprocal_str_param,
	.a = &secp192k1_a_str_param,
	.b = &secp192k1_b_str_param,
	.curve_order = &secp192k1_curve_order_str_param,
	.gx = &secp192k1_gx_str_param,
	.gy = &secp192k1_gy_str_param,
	.gz = &secp192k1_gz_str_param,
	.gen_order = &secp192k1_gen_order_str_param,
	.gen_order_bitlen = &secp192k1_gen_order_bitlen_str_param,
	.cofactor = &secp192k1_cofactor_str_param,
	.alpha_montgomery = &secp192k1_alpha_montgomery_str_param,
	.gamma_montgomery = &secp192k1_gamma_montgomery_str_param,
	.alpha_edwards = &secp192k1_alpha_edwards_str_param,
	.oid = &secp192k1_oid_str_param,
	.name = &secp192k1_name_str_param,
};

/*
 * Compute max bit length of all curves for p and q
 */
#ifndef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN    0
#endif
#if (CURVES_MAX_P_BIT_LEN < CURVE_SECP192K1_P_BITLEN)
#undef CURVES_MAX_P_BIT_LEN
#define CURVES_MAX_P_BIT_LEN CURVE_SECP192K1_P_BITLEN
#endif
#ifndef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN    0
#endif
#if (CURVES_MAX_Q_BIT_LEN < CURVE_SECP192K1_Q_BITLEN)
#undef CURVES_MAX_Q_BIT_LEN
#define CURVES_MAX_Q_BIT_LEN CURVE_SECP192K1_Q_BITLEN
#endif
#ifndef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN    0
#endif
#if (CURVES_MAX_CURVE_ORDER_BIT_LEN < CURVE_SECP192K1_CURVE_ORDER_BITLEN)
#undef CURVES_MAX_CURVE_ORDER_BIT_LEN
#define CURVES_MAX_CURVE_ORDER_BIT_LEN CURVE_SECP192K1_CURVE_ORDER_BITLEN
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

#endif /* __EC_PARAMS_SECP192K1_H__ */

#endif /* WITH_CURVE_SECP192K1 */
