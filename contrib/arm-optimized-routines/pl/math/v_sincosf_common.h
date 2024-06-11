/*
 * Core approximation for single-precision vector sincos
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"

const static struct v_sincosf_data
{
  float32x4_t poly_sin[3], poly_cos[3], pio2[3], inv_pio2, shift, range_val;
} v_sincosf_data = {
  .poly_sin = { /* Generated using Remez, odd coeffs only, in [-pi/4, pi/4].  */
	        V4 (-0x1.555546p-3), V4 (0x1.11076p-7), V4 (-0x1.994eb4p-13) },
  .poly_cos = { /* Generated using Remez, even coeffs only, in [-pi/4, pi/4].  */
	        V4 (0x1.55554ap-5), V4 (-0x1.6c0c1ap-10), V4 (0x1.99e0eep-16) },
  .pio2 = { V4 (0x1.921fb6p+0f), V4 (-0x1.777a5cp-25f), V4 (-0x1.ee59dap-50f) },
  .inv_pio2 = V4 (0x1.45f306p-1f),
  .shift = V4 (0x1.8p23),
  .range_val = V4 (0x1p20),
};

static inline uint32x4_t
check_ge_rangeval (float32x4_t x, const struct v_sincosf_data *d)
{
  return vcagtq_f32 (x, d->range_val);
}

/* Single-precision vector function allowing calculation of both sin and cos in
   one function call, using shared argument reduction and separate low-order
   polynomials.
   Worst-case error for sin is 1.67 ULP:
   v_sincosf_sin(0x1.c704c4p+19) got 0x1.fff698p-5 want 0x1.fff69cp-5
   Worst-case error for cos is 1.81 ULP:
   v_sincosf_cos(0x1.e506fp+19) got -0x1.ffec6ep-6 want -0x1.ffec72p-6.  */
static inline float32x4x2_t
v_sincosf_inline (float32x4_t x, const struct v_sincosf_data *d)
{
  /* n = rint ( x / (pi/2) ).  */
  float32x4_t shift = d->shift;
  float32x4_t q = vfmaq_f32 (shift, x, d->inv_pio2);
  q = vsubq_f32 (q, shift);
  int32x4_t n = vcvtq_s32_f32 (q);

  /* Reduce x such that r is in [ -pi/4, pi/4 ].  */
  float32x4_t r = x;
  r = vfmsq_f32 (r, q, d->pio2[0]);
  r = vfmsq_f32 (r, q, d->pio2[1]);
  r = vfmsq_f32 (r, q, d->pio2[2]);

  /* Approximate sin(r) ~= r + r^3 * poly_sin(r^2).  */
  float32x4_t r2 = vmulq_f32 (r, r), r3 = vmulq_f32 (r, r2);
  float32x4_t s = vfmaq_f32 (d->poly_sin[1], r2, d->poly_sin[2]);
  s = vfmaq_f32 (d->poly_sin[0], r2, s);
  s = vfmaq_f32 (r, r3, s);

  /* Approximate cos(r) ~= 1 - (r^2)/2 + r^4 * poly_cos(r^2).  */
  float32x4_t r4 = vmulq_f32 (r2, r2);
  float32x4_t p = vfmaq_f32 (d->poly_cos[1], r2, d->poly_cos[2]);
  float32x4_t c = vfmaq_f32 (v_f32 (-0.5), r2, d->poly_cos[0]);
  c = vfmaq_f32 (c, r4, p);
  c = vfmaq_f32 (v_f32 (1), c, r2);

  /* If odd quadrant, swap cos and sin.  */
  uint32x4_t swap = vtstq_u32 (vreinterpretq_u32_s32 (n), v_u32 (1));
  float32x4_t ss = vbslq_f32 (swap, c, s);
  float32x4_t cc = vbslq_f32 (swap, s, c);

  /* Fix signs according to quadrant.
     ss = asfloat(asuint(ss) ^ ((n       & 2) << 30))
     cc = asfloat(asuint(cc) & (((n + 1) & 2) << 30)).  */
  uint32x4_t sin_sign
      = vshlq_n_u32 (vandq_u32 (vreinterpretq_u32_s32 (n), v_u32 (2)), 30);
  uint32x4_t cos_sign = vshlq_n_u32 (
      vandq_u32 (vreinterpretq_u32_s32 (vaddq_s32 (n, v_s32 (1))), v_u32 (2)),
      30);
  ss = vreinterpretq_f32_u32 (
      veorq_u32 (vreinterpretq_u32_f32 (ss), sin_sign));
  cc = vreinterpretq_f32_u32 (
      veorq_u32 (vreinterpretq_u32_f32 (cc), cos_sign));

  return (float32x4x2_t){ ss, cc };
}
