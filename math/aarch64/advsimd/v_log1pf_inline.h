/*
 * Helper for single-precision routines which calculate log(1 + x) and do not
 * need special-case handling
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef MATH_V_LOG1PF_INLINE_H
#define MATH_V_LOG1PF_INLINE_H

#include "v_math.h"
#include "v_poly_f32.h"

struct v_log1pf_data
{
  uint32x4_t four;
  int32x4_t three_quarters;
  float c0, c3, c5, c7;
  float32x4_t c4, c6, c1, c2, ln2;
};

/* Polynomial generated using FPMinimax in [-0.25, 0.5]. First two coefficients
   (1, -0.5) are not stored as they can be generated more efficiently.  */
#define V_LOG1PF_CONSTANTS_TABLE                                              \
  {                                                                           \
    .c0 = 0x1.5555aap-2f, .c1 = V4 (-0x1.000038p-2f),                         \
    .c2 = V4 (0x1.99675cp-3f), .c3 = -0x1.54ef78p-3f,                         \
    .c4 = V4 (0x1.28a1f4p-3f), .c5 = -0x1.0da91p-3f,                          \
    .c6 = V4 (0x1.abcb6p-4f), .c7 = -0x1.6f0d5ep-5f,                          \
    .ln2 = V4 (0x1.62e43p-1f), .four = V4 (0x40800000),                       \
    .three_quarters = V4 (0x3f400000)                                         \
  }

static inline float32x4_t
eval_poly (float32x4_t m, const struct v_log1pf_data *d)
{
  /* Approximate log(1+m) on [-0.25, 0.5] using pairwise Horner.  */
  float32x4_t c0357 = vld1q_f32 (&d->c0);
  float32x4_t q = vfmaq_laneq_f32 (v_f32 (-0.5), m, c0357, 0);
  float32x4_t m2 = vmulq_f32 (m, m);
  float32x4_t p67 = vfmaq_laneq_f32 (d->c6, m, c0357, 3);
  float32x4_t p45 = vfmaq_laneq_f32 (d->c4, m, c0357, 2);
  float32x4_t p23 = vfmaq_laneq_f32 (d->c2, m, c0357, 1);
  float32x4_t p = vfmaq_f32 (p45, m2, p67);
  p = vfmaq_f32 (p23, m2, p);
  p = vfmaq_f32 (d->c1, m, p);
  p = vmulq_f32 (m2, p);
  p = vfmaq_f32 (m, m2, p);
  return vfmaq_f32 (p, m2, q);
}

static inline float32x4_t
log1pf_inline (float32x4_t x, const struct v_log1pf_data *d)
{
  /* Helper for calculating log(x + 1).  */

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

  /* Scale up to ensure that the scale factor is representable as normalised
     fp32 number, and scale m down accordingly.  */
  float32x4_t s = vreinterpretq_f32_u32 (vsubq_u32 (d->four, ku));

  /* Scale x by exponent manipulation.  */
  float32x4_t m_scale
      = vreinterpretq_f32_u32 (vsubq_u32 (vreinterpretq_u32_f32 (x), ku));
  m_scale = vaddq_f32 (m_scale, vfmaq_f32 (v_f32 (-1.0f), v_f32 (0.25f), s));

  /* Evaluate polynomial on the reduced interval.  */
  float32x4_t p = eval_poly (m_scale, d);

  /* The scale factor to be applied back at the end - by multiplying float(k)
     by 2^-23 we get the unbiased exponent of k.  */
  float32x4_t scale_back = vmulq_f32 (vcvtq_f32_s32 (k), v_f32 (0x1.0p-23f));

  /* Apply the scaling back.  */
  return vfmaq_f32 (p, scale_back, d->ln2);
}

#endif //  MATH_V_LOG1PF_INLINE_H
