/*
 * Single-precision vector powf function.
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_defs.h"
#include "test_sig.h"

#define Min v_u32 (0x00800000)
#define Max v_u32 (0x7f800000)
#define Thresh v_u32 (0x7f000000) /* Max - Min.  */
#define MantissaMask v_u32 (0x007fffff)

#define A d->log2_poly
#define C d->exp2f_poly

/* 2.6 ulp ~ 0.5 + 2^24 (128*Ln2*relerr_log2 + relerr_exp2).  */
#define Off v_u32 (0x3f35d000)

#define V_POWF_LOG2_TABLE_BITS 5
#define V_EXP2F_TABLE_BITS 5
#define Log2IdxMask ((1 << V_POWF_LOG2_TABLE_BITS) - 1)
#define Scale ((double) (1 << V_EXP2F_TABLE_BITS))

static const struct data
{
  struct
  {
    double invc, logc;
  } log2_tab[1 << V_POWF_LOG2_TABLE_BITS];
  float64x2_t log2_poly[4];
  uint64_t exp2f_tab[1 << V_EXP2F_TABLE_BITS];
  float64x2_t exp2f_poly[3];
} data = {
  .log2_tab = {{0x1.6489890582816p+0, -0x1.e960f97b22702p-2 * Scale},
	       {0x1.5cf19b35e3472p+0, -0x1.c993406cd4db6p-2 * Scale},
	       {0x1.55aac0e956d65p+0, -0x1.aa711d9a7d0f3p-2 * Scale},
	       {0x1.4eb0022977e01p+0, -0x1.8bf37bacdce9bp-2 * Scale},
	       {0x1.47fcccda1dd1fp+0, -0x1.6e13b3519946ep-2 * Scale},
	       {0x1.418ceabab68c1p+0, -0x1.50cb8281e4089p-2 * Scale},
	       {0x1.3b5c788f1edb3p+0, -0x1.341504a237e2bp-2 * Scale},
	       {0x1.3567de48e9c9ap+0, -0x1.17eaab624ffbbp-2 * Scale},
	       {0x1.2fabc80fd19bap+0, -0x1.f88e708f8c853p-3 * Scale},
	       {0x1.2a25200ce536bp+0, -0x1.c24b6da113914p-3 * Scale},
	       {0x1.24d108e0152e3p+0, -0x1.8d02ee397cb1dp-3 * Scale},
	       {0x1.1facd8ab2fbe1p+0, -0x1.58ac1223408b3p-3 * Scale},
	       {0x1.1ab614a03efdfp+0, -0x1.253e6fd190e89p-3 * Scale},
	       {0x1.15ea6d03af9ffp+0, -0x1.e5641882c12ffp-4 * Scale},
	       {0x1.1147b994bb776p+0, -0x1.81fea712926f7p-4 * Scale},
	       {0x1.0ccbf650593aap+0, -0x1.203e240de64a3p-4 * Scale},
	       {0x1.0875408477302p+0, -0x1.8029b86a78281p-5 * Scale},
	       {0x1.0441d42a93328p+0, -0x1.85d713190fb9p-6 * Scale},
	       {0x1p+0, 0x0p+0 * Scale},
	       {0x1.f1d006c855e86p-1, 0x1.4c1cc07312997p-5 * Scale},
	       {0x1.e28c3341aa301p-1, 0x1.5e1848ccec948p-4 * Scale},
	       {0x1.d4bdf9aa64747p-1, 0x1.04cfcb7f1196fp-3 * Scale},
	       {0x1.c7b45a24e5803p-1, 0x1.582813d463c21p-3 * Scale},
	       {0x1.bb5f5eb2ed60ap-1, 0x1.a936fa68760ccp-3 * Scale},
	       {0x1.afb0bff8fe6b4p-1, 0x1.f81bc31d6cc4ep-3 * Scale},
	       {0x1.a49badf7ab1f5p-1, 0x1.2279a09fae6b1p-2 * Scale},
	       {0x1.9a14a111fc4c9p-1, 0x1.47ec0b6df5526p-2 * Scale},
	       {0x1.901131f5b2fdcp-1, 0x1.6c71762280f1p-2 * Scale},
	       {0x1.8687f73f6d865p-1, 0x1.90155070798dap-2 * Scale},
	       {0x1.7d7067eb77986p-1, 0x1.b2e23b1d3068cp-2 * Scale},
	       {0x1.74c2c1cf97b65p-1, 0x1.d4e21b0daa86ap-2 * Scale},
	       {0x1.6c77f37cff2a1p-1, 0x1.f61e2a2f67f3fp-2 * Scale},},
  .log2_poly = { /* rel err: 1.5 * 2^-30.  */
		 V2 (-0x1.6ff5daa3b3d7cp-2 * Scale),
		 V2 (0x1.ec81d03c01aebp-2 * Scale),
		 V2 (-0x1.71547bb43f101p-1 * Scale),
		 V2 (0x1.7154764a815cbp0 * Scale)},
  .exp2f_tab = {0x3ff0000000000000, 0x3fefd9b0d3158574, 0x3fefb5586cf9890f,
		0x3fef9301d0125b51, 0x3fef72b83c7d517b, 0x3fef54873168b9aa,
		0x3fef387a6e756238, 0x3fef1e9df51fdee1, 0x3fef06fe0a31b715,
		0x3feef1a7373aa9cb, 0x3feedea64c123422, 0x3feece086061892d,
		0x3feebfdad5362a27, 0x3feeb42b569d4f82, 0x3feeab07dd485429,
		0x3feea47eb03a5585, 0x3feea09e667f3bcd, 0x3fee9f75e8ec5f74,
		0x3feea11473eb0187, 0x3feea589994cce13, 0x3feeace5422aa0db,
		0x3feeb737b0cdc5e5, 0x3feec49182a3f090, 0x3feed503b23e255d,
		0x3feee89f995ad3ad, 0x3feeff76f2fb5e47, 0x3fef199bdd85529c,
		0x3fef3720dcef9069, 0x3fef5818dcfba487, 0x3fef7c97337b9b5f,
		0x3fefa4afa2a490da, 0x3fefd0765b6e4540,},
  .exp2f_poly = { /* rel err: 1.69 * 2^-34.  */
		  V2 (0x1.c6af84b912394p-5 / Scale / Scale / Scale),
		  V2 (0x1.ebfce50fac4f3p-3 / Scale / Scale),
		  V2 (0x1.62e42ff0c52d6p-1 / Scale)}};

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, float32x4_t ret, uint32x4_t cmp)
{
  return v_call2_f32 (powf, x, y, ret, cmp);
}

static inline float64x2_t
ylogx_core (const struct data *d, float64x2_t iz, float64x2_t k,
	    float64x2_t invc, float64x2_t logc, float64x2_t y)
{

  /* log2(x) = log1p(z/c-1)/ln2 + log2(c) + k.  */
  float64x2_t r = vfmaq_f64 (v_f64 (-1.0), iz, invc);
  float64x2_t y0 = vaddq_f64 (logc, k);

  /* Polynomial to approximate log1p(r)/ln2.  */
  float64x2_t logx = vfmaq_f64 (A[1], r, A[0]);
  logx = vfmaq_f64 (A[2], logx, r);
  logx = vfmaq_f64 (A[3], logx, r);
  logx = vfmaq_f64 (y0, logx, r);

  return vmulq_f64 (logx, y);
}

static inline float64x2_t
log2_lookup (const struct data *d, uint32_t i)
{
  return vld1q_f64 (
      &d->log2_tab[(i >> (23 - V_POWF_LOG2_TABLE_BITS)) & Log2IdxMask].invc);
}

static inline uint64x1_t
exp2f_lookup (const struct data *d, uint64_t i)
{
  return vld1_u64 (&d->exp2f_tab[i % (1 << V_EXP2F_TABLE_BITS)]);
}

static inline float32x2_t
powf_core (const struct data *d, float64x2_t ylogx)
{
  /* N*x = k + r with r in [-1/2, 1/2].  */
  float64x2_t kd = vrndnq_f64 (ylogx);
  int64x2_t ki = vcvtaq_s64_f64 (ylogx);
  float64x2_t r = vsubq_f64 (ylogx, kd);

  /* exp2(x) = 2^(k/N) * 2^r ~= s * (C0*r^3 + C1*r^2 + C2*r + 1).  */
  uint64x2_t t = vcombine_u64 (exp2f_lookup (d, vgetq_lane_s64 (ki, 0)),
			       exp2f_lookup (d, vgetq_lane_s64 (ki, 1)));
  t = vaddq_u64 (
      t, vreinterpretq_u64_s64 (vshlq_n_s64 (ki, 52 - V_EXP2F_TABLE_BITS)));
  float64x2_t s = vreinterpretq_f64_u64 (t);
  float64x2_t p = vfmaq_f64 (C[1], r, C[0]);
  p = vfmaq_f64 (C[2], r, p);
  p = vfmaq_f64 (s, p, vmulq_f64 (s, r));
  return vcvt_f32_f64 (p);
}

float32x4_t VPCS_ATTR NOINLINE V_NAME_F2 (pow) (float32x4_t x, float32x4_t y)
{
  const struct data *d = ptr_barrier (&data);
  uint32x4_t u = vreinterpretq_u32_f32 (x);
  uint32x4_t cmp = vcgeq_u32 (vsubq_u32 (u, Min), Thresh);
  uint32x4_t tmp = vsubq_u32 (u, Off);
  uint32x4_t top = vbicq_u32 (tmp, MantissaMask);
  float32x4_t iz = vreinterpretq_f32_u32 (vsubq_u32 (u, top));
  int32x4_t k = vshrq_n_s32 (vreinterpretq_s32_u32 (top),
			     23 - V_EXP2F_TABLE_BITS); /* arithmetic shift.  */

  /* Use double precision for each lane: split input vectors into lo and hi
     halves and promote.  */
  float64x2_t tab0 = log2_lookup (d, vgetq_lane_u32 (tmp, 0)),
	      tab1 = log2_lookup (d, vgetq_lane_u32 (tmp, 1)),
	      tab2 = log2_lookup (d, vgetq_lane_u32 (tmp, 2)),
	      tab3 = log2_lookup (d, vgetq_lane_u32 (tmp, 3));

  float64x2_t iz_lo = vcvt_f64_f32 (vget_low_f32 (iz)),
	      iz_hi = vcvt_high_f64_f32 (iz);

  float64x2_t k_lo = vcvtq_f64_s64 (vmovl_s32 (vget_low_s32 (k))),
	      k_hi = vcvtq_f64_s64 (vmovl_high_s32 (k));

  float64x2_t invc_lo = vzip1q_f64 (tab0, tab1),
	      invc_hi = vzip1q_f64 (tab2, tab3),
	      logc_lo = vzip2q_f64 (tab0, tab1),
	      logc_hi = vzip2q_f64 (tab2, tab3);

  float64x2_t y_lo = vcvt_f64_f32 (vget_low_f32 (y)),
	      y_hi = vcvt_high_f64_f32 (y);

  float64x2_t ylogx_lo = ylogx_core (d, iz_lo, k_lo, invc_lo, logc_lo, y_lo);
  float64x2_t ylogx_hi = ylogx_core (d, iz_hi, k_hi, invc_hi, logc_hi, y_hi);

  uint32x4_t ylogx_top = vuzp2q_u32 (vreinterpretq_u32_f64 (ylogx_lo),
				     vreinterpretq_u32_f64 (ylogx_hi));

  cmp = vorrq_u32 (
      cmp, vcgeq_u32 (vandq_u32 (vshrq_n_u32 (ylogx_top, 15), v_u32 (0xffff)),
		      vdupq_n_u32 (asuint64 (126.0 * (1 << V_EXP2F_TABLE_BITS))
				   >> 47)));

  float32x2_t p_lo = powf_core (d, ylogx_lo);
  float32x2_t p_hi = powf_core (d, ylogx_hi);

  if (unlikely (v_any_u32 (cmp)))
    return special_case (x, y, vcombine_f32 (p_lo, p_hi), cmp);
  return vcombine_f32 (p_lo, p_hi);
}

HALF_WIDTH_ALIAS_F2 (pow)

TEST_SIG (V, F, 2, pow)
TEST_ULP (V_NAME_F2 (pow), 2.1)
TEST_DISABLE_FENV (V_NAME_F2 (pow))
TEST_INTERVAL2 (V_NAME_F2 (pow), 0x1p-1, 0x1p1, 0x1p-7, 0x1p7, 50000)
TEST_INTERVAL2 (V_NAME_F2 (pow), 0x1p-1, 0x1p1, -0x1p-7, -0x1p7, 50000)
TEST_INTERVAL2 (V_NAME_F2 (pow), 0x1p-70, 0x1p70, 0x1p-1, 0x1p1, 50000)
TEST_INTERVAL2 (V_NAME_F2 (pow), 0x1p-70, 0x1p70, -0x1p-1, -0x1p1, 50000)
TEST_INTERVAL2 (V_NAME_F2 (pow), 0x1.ep-1, 0x1.1p0, 0x1p8, 0x1p14, 50000)
TEST_INTERVAL2 (V_NAME_F2 (pow), 0x1.ep-1, 0x1.1p0, -0x1p8, -0x1p14, 50000)
