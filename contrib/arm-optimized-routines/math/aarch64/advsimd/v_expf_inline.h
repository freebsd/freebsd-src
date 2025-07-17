/*
 * Helper for single-precision routines which calculate exp(ax) and do not
 * need special-case handling
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef MATH_V_EXPF_INLINE_H
#define MATH_V_EXPF_INLINE_H

#include "v_math.h"

struct v_expf_data
{
  float ln2_hi, ln2_lo, c0, c2;
  float32x4_t inv_ln2, c1, c3, c4;
  /* asuint(1.0f).  */
  uint32x4_t exponent_bias;
};

/* maxerr: 1.45358 +0.5 ulp.  */
#define V_EXPF_DATA                                                           \
  {                                                                           \
    .c0 = 0x1.0e4020p-7f, .c1 = V4 (0x1.573e2ep-5f), .c2 = 0x1.555e66p-3f,    \
    .c3 = V4 (0x1.fffdb6p-2f), .c4 = V4 (0x1.ffffecp-1f),                     \
    .ln2_hi = 0x1.62e4p-1f, .ln2_lo = 0x1.7f7d1cp-20f,                        \
    .inv_ln2 = V4 (0x1.715476p+0f), .exponent_bias = V4 (0x3f800000),         \
  }

static inline float32x4_t
v_expf_inline (float32x4_t x, const struct v_expf_data *d)
{
  /* Helper routine for calculating exp(ax).
     Copied from v_expf.c, with all special-case handling removed - the
     calling routine should handle special values if required.  */

  /* exp(ax) = 2^n (1 + poly(r)), with 1 + poly(r) in [1/sqrt(2),sqrt(2)]
     ax = ln2*n + r, with r in [-ln2/2, ln2/2].  */
  float32x4_t ax = vabsq_f32 (x);
  float32x4_t ln2_c02 = vld1q_f32 (&d->ln2_hi);
  float32x4_t n = vrndaq_f32 (vmulq_f32 (ax, d->inv_ln2));
  float32x4_t r = vfmsq_laneq_f32 (ax, n, ln2_c02, 0);
  r = vfmsq_laneq_f32 (r, n, ln2_c02, 1);
  uint32x4_t e = vshlq_n_u32 (vreinterpretq_u32_s32 (vcvtq_s32_f32 (n)), 23);
  float32x4_t scale = vreinterpretq_f32_u32 (vaddq_u32 (e, d->exponent_bias));

  /* Custom order-4 Estrin avoids building high order monomial.  */
  float32x4_t r2 = vmulq_f32 (r, r);
  float32x4_t p = vfmaq_laneq_f32 (d->c1, r, ln2_c02, 2);
  float32x4_t q = vfmaq_laneq_f32 (d->c3, r, ln2_c02, 3);
  q = vfmaq_f32 (q, p, r2);
  p = vmulq_f32 (d->c4, r);
  float32x4_t poly = vfmaq_f32 (p, q, r2);
  return vfmaq_f32 (scale, poly, scale);
}

#endif // MATH_V_EXPF_INLINE_H
