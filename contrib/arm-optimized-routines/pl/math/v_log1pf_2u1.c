/*
 * Single-precision vector log(1+x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "poly_advsimd_f32.h"

const static struct data
{
  float32x4_t poly[8], ln2;
  uint32x4_t tiny_bound, minus_one, four, thresh;
  int32x4_t three_quarters;
} data = {
  .poly = { /* Generated using FPMinimax in [-0.25, 0.5]. First two coefficients
	       (1, -0.5) are not stored as they can be generated more
	       efficiently.  */
	    V4 (0x1.5555aap-2f), V4 (-0x1.000038p-2f), V4 (0x1.99675cp-3f),
	    V4 (-0x1.54ef78p-3f), V4 (0x1.28a1f4p-3f), V4 (-0x1.0da91p-3f),
	    V4 (0x1.abcb6p-4f), V4 (-0x1.6f0d5ep-5f) },
  .ln2 = V4 (0x1.62e43p-1f),
  .tiny_bound = V4 (0x34000000), /* asuint32(0x1p-23). ulp=0.5 at 0x1p-23.  */
  .thresh = V4 (0x4b800000), /* asuint32(INFINITY) - tiny_bound.  */
  .minus_one = V4 (0xbf800000),
  .four = V4 (0x40800000),
  .three_quarters = V4 (0x3f400000)
};

static inline float32x4_t
eval_poly (float32x4_t m, const float32x4_t *p)
{
  /* Approximate log(1+m) on [-0.25, 0.5] using split Estrin scheme.  */
  float32x4_t p_12 = vfmaq_f32 (v_f32 (-0.5), m, p[0]);
  float32x4_t p_34 = vfmaq_f32 (p[1], m, p[2]);
  float32x4_t p_56 = vfmaq_f32 (p[3], m, p[4]);
  float32x4_t p_78 = vfmaq_f32 (p[5], m, p[6]);

  float32x4_t m2 = vmulq_f32 (m, m);
  float32x4_t p_02 = vfmaq_f32 (m, m2, p_12);
  float32x4_t p_36 = vfmaq_f32 (p_34, m2, p_56);
  float32x4_t p_79 = vfmaq_f32 (p_78, m2, p[7]);

  float32x4_t m4 = vmulq_f32 (m2, m2);
  float32x4_t p_06 = vfmaq_f32 (p_02, m4, p_36);
  return vfmaq_f32 (p_06, m4, vmulq_f32 (m4, p_79));
}

static float32x4_t NOINLINE VPCS_ATTR
special_case (float32x4_t x, float32x4_t y, uint32x4_t special)
{
  return v_call_f32 (log1pf, x, y, special);
}

/* Vector log1pf approximation using polynomial on reduced interval. Accuracy
   is roughly 2.02 ULP:
   log1pf(0x1.21e13ap-2) got 0x1.fe8028p-3 want 0x1.fe802cp-3.  */
VPCS_ATTR float32x4_t V_NAME_F1 (log1p) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t ia = vreinterpretq_u32_f32 (vabsq_f32 (x));
  uint32x4_t special_cases
      = vorrq_u32 (vcgeq_u32 (vsubq_u32 (ia, d->tiny_bound), d->thresh),
		   vcgeq_u32 (ix, d->minus_one));
  float32x4_t special_arg = x;

#if WANT_SIMD_EXCEPT
  if (unlikely (v_any_u32 (special_cases)))
    /* Side-step special lanes so fenv exceptions are not triggered
       inadvertently.  */
    x = v_zerofy_f32 (x, special_cases);
#endif

  /* With x + 1 = t * 2^k (where t = m + 1 and k is chosen such that m
			   is in [-0.25, 0.5]):
     log1p(x) = log(t) + log(2^k) = log1p(m) + k*log(2).

     We approximate log1p(m) with a polynomial, then scale by
     k*log(2). Instead of doing this directly, we use an intermediate
     scale factor s = 4*k*log(2) to ensure the scale is representable
     as a normalised fp32 number.  */

  float32x4_t m = vaddq_f32 (x, v_f32 (1.0f));

  /* Choose k to scale x to the range [-1/4, 1/2].  */
  int32x4_t k
      = vandq_s32 (vsubq_s32 (vreinterpretq_s32_f32 (m), d->three_quarters),
		   v_s32 (0xff800000));
  uint32x4_t ku = vreinterpretq_u32_s32 (k);

  /* Scale x by exponent manipulation.  */
  float32x4_t m_scale
      = vreinterpretq_f32_u32 (vsubq_u32 (vreinterpretq_u32_f32 (x), ku));

  /* Scale up to ensure that the scale factor is representable as normalised
     fp32 number, and scale m down accordingly.  */
  float32x4_t s = vreinterpretq_f32_u32 (vsubq_u32 (d->four, ku));
  m_scale = vaddq_f32 (m_scale, vfmaq_f32 (v_f32 (-1.0f), v_f32 (0.25f), s));

  /* Evaluate polynomial on the reduced interval.  */
  float32x4_t p = eval_poly (m_scale, d->poly);

  /* The scale factor to be applied back at the end - by multiplying float(k)
     by 2^-23 we get the unbiased exponent of k.  */
  float32x4_t scale_back = vcvtq_f32_s32 (vshrq_n_s32 (k, 23));

  /* Apply the scaling back.  */
  float32x4_t y = vfmaq_f32 (p, scale_back, d->ln2);

  if (unlikely (v_any_u32 (special_cases)))
    return special_case (special_arg, y, special_cases);
  return y;
}

PL_SIG (V, F, 1, log1p, -0.9, 10.0)
PL_TEST_ULP (V_NAME_F1 (log1p), 1.53)
PL_TEST_EXPECT_FENV (V_NAME_F1 (log1p), WANT_SIMD_EXCEPT)
PL_TEST_SYM_INTERVAL (V_NAME_F1 (log1p), 0.0, 0x1p-23, 30000)
PL_TEST_SYM_INTERVAL (V_NAME_F1 (log1p), 0x1p-23, 1, 50000)
PL_TEST_INTERVAL (V_NAME_F1 (log1p), 1, inf, 50000)
PL_TEST_INTERVAL (V_NAME_F1 (log1p), -1.0, -inf, 1000)
