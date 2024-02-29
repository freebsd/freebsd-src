/*
 * Core approximation for double-precision vector sincos
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "poly_advsimd_f64.h"

static const struct v_sincos_data
{
  float64x2_t sin_poly[7], cos_poly[6], pio2[3];
  float64x2_t inv_pio2, shift, range_val;
} v_sincos_data = {
  .inv_pio2 = V2 (0x1.45f306dc9c882p-1),
  .pio2 = { V2 (0x1.921fb50000000p+0), V2 (0x1.110b460000000p-26),
	    V2 (0x1.1a62633145c07p-54) },
  .shift = V2 (0x1.8p52),
  .sin_poly = { /* Computed using Remez in [-pi/2, pi/2].  */
	        V2 (-0x1.555555555547bp-3), V2 (0x1.1111111108a4dp-7),
		V2 (-0x1.a01a019936f27p-13), V2 (0x1.71de37a97d93ep-19),
		V2 (-0x1.ae633919987c6p-26), V2 (0x1.60e277ae07cecp-33),
		V2 (-0x1.9e9540300a1p-41) },
  .cos_poly = { /* Computed using Remez in [-pi/4, pi/4].  */
	        V2 (0x1.555555555554cp-5), V2 (-0x1.6c16c16c1521fp-10),
		V2 (0x1.a01a019cbf62ap-16), V2 (-0x1.27e4f812b681ep-22),
		V2 (0x1.1ee9f152a57cdp-29), V2 (-0x1.8fb131098404bp-37) },
  .range_val = V2 (0x1p23), };

static inline uint64x2_t
check_ge_rangeval (float64x2_t x, const struct v_sincos_data *d)
{
  return vcagtq_f64 (x, d->range_val);
}

/* Double-precision vector function allowing calculation of both sin and cos in
   one function call, using shared argument reduction and separate polynomials.
   Largest observed error is for sin, 3.22 ULP:
   v_sincos_sin (0x1.d70eef40f39b1p+12) got -0x1.ffe9537d5dbb7p-3
				       want -0x1.ffe9537d5dbb4p-3.  */
static inline float64x2x2_t
v_sincos_inline (float64x2_t x, const struct v_sincos_data *d)
{
  /* q = nearest integer to 2 * x / pi.  */
  float64x2_t q = vsubq_f64 (vfmaq_f64 (d->shift, x, d->inv_pio2), d->shift);
  int64x2_t n = vcvtq_s64_f64 (q);

  /* Use q to reduce x to r in [-pi/4, pi/4], by:
     r = x - q * pi/2, in extended precision.  */
  float64x2_t r = x;
  r = vfmsq_f64 (r, q, d->pio2[0]);
  r = vfmsq_f64 (r, q, d->pio2[1]);
  r = vfmsq_f64 (r, q, d->pio2[2]);

  float64x2_t r2 = r * r, r3 = r2 * r, r4 = r2 * r2;

  /* Approximate sin(r) ~= r + r^3 * poly_sin(r^2).  */
  float64x2_t s = v_pw_horner_6_f64 (r2, r4, d->sin_poly);
  s = vfmaq_f64 (r, r3, s);

  /* Approximate cos(r) ~= 1 - (r^2)/2 + r^4 * poly_cos(r^2).  */
  float64x2_t c = v_pw_horner_5_f64 (r2, r4, d->cos_poly);
  c = vfmaq_f64 (v_f64 (-0.5), r2, c);
  c = vfmaq_f64 (v_f64 (1), r2, c);

  /* If odd quadrant, swap cos and sin.  */
  uint64x2_t swap = vtstq_s64 (n, v_s64 (1));
  float64x2_t ss = vbslq_f64 (swap, c, s);
  float64x2_t cc = vbslq_f64 (swap, s, c);

  /* Fix signs according to quadrant.
     ss = asdouble(asuint64(ss) ^ ((n       & 2) << 62))
     cc = asdouble(asuint64(cc) & (((n + 1) & 2) << 62)).  */
  uint64x2_t sin_sign
      = vshlq_n_u64 (vandq_u64 (vreinterpretq_u64_s64 (n), v_u64 (2)), 62);
  uint64x2_t cos_sign = vshlq_n_u64 (
      vandq_u64 (vreinterpretq_u64_s64 (vaddq_s64 (n, v_s64 (1))), v_u64 (2)),
      62);
  ss = vreinterpretq_f64_u64 (
      veorq_u64 (vreinterpretq_u64_f64 (ss), sin_sign));
  cc = vreinterpretq_f64_u64 (
      veorq_u64 (vreinterpretq_u64_f64 (cc), cos_sign));

  return (float64x2x2_t){ ss, cc };
}
