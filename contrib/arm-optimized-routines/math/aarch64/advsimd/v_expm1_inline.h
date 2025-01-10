/*
 * Helper for double-precision routines which calculate exp(x) - 1 and do not
 * need special-case handling
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef MATH_V_EXPM1_INLINE_H
#define MATH_V_EXPM1_INLINE_H

#include "v_math.h"

struct v_expm1_data
{
  float64x2_t c2, c4, c6, c8;
  float64x2_t invln2;
  int64x2_t exponent_bias;
  double c1, c3, c5, c7, c9, c10;
  double ln2[2];
};

/* Generated using fpminimax, with degree=12 in [log(2)/2, log(2)/2].  */
#define V_EXPM1_DATA                                                          \
  {                                                                           \
    .c1 = 0x1.5555555555559p-3, .c2 = V2 (0x1.555555555554bp-5),              \
    .c3 = 0x1.111111110f663p-7, .c4 = V2 (0x1.6c16c16c1b5f3p-10),             \
    .c5 = 0x1.a01a01affa35dp-13, .c6 = V2 (0x1.a01a018b4ecbbp-16),            \
    .c7 = 0x1.71ddf82db5bb4p-19, .c8 = V2 (0x1.27e517fc0d54bp-22),            \
    .c9 = 0x1.af5eedae67435p-26, .c10 = 0x1.1f143d060a28ap-29,                \
    .ln2 = { 0x1.62e42fefa39efp-1, 0x1.abc9e3b39803fp-56 },                   \
    .invln2 = V2 (0x1.71547652b82fep0),                                       \
    .exponent_bias = V2 (0x3ff0000000000000),                                 \
  }

static inline float64x2_t
expm1_inline (float64x2_t x, const struct v_expm1_data *d)
{
  /* Helper routine for calculating exp(x) - 1.  */

  float64x2_t ln2 = vld1q_f64 (&d->ln2[0]);

  /* Reduce argument to smaller range:
     Let i = round(x / ln2)
     and f = x - i * ln2, then f is in [-ln2/2, ln2/2].
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where 2^i is exact because i is an integer.  */
  float64x2_t n = vrndaq_f64 (vmulq_f64 (x, d->invln2));
  int64x2_t i = vcvtq_s64_f64 (n);
  float64x2_t f = vfmsq_laneq_f64 (x, n, ln2, 0);
  f = vfmsq_laneq_f64 (f, n, ln2, 1);

  /* Approximate expm1(f) using polynomial.
     Taylor expansion for expm1(x) has the form:
	 x + ax^2 + bx^3 + cx^4 ....
     So we calculate the polynomial P(f) = a + bf + cf^2 + ...
     and assemble the approximation expm1(f) ~= f + f^2 * P(f).  */
  float64x2_t f2 = vmulq_f64 (f, f);
  float64x2_t f4 = vmulq_f64 (f2, f2);
  float64x2_t lane_consts_13 = vld1q_f64 (&d->c1);
  float64x2_t lane_consts_57 = vld1q_f64 (&d->c5);
  float64x2_t lane_consts_910 = vld1q_f64 (&d->c9);
  float64x2_t p01 = vfmaq_laneq_f64 (v_f64 (0.5), f, lane_consts_13, 0);
  float64x2_t p23 = vfmaq_laneq_f64 (d->c2, f, lane_consts_13, 1);
  float64x2_t p45 = vfmaq_laneq_f64 (d->c4, f, lane_consts_57, 0);
  float64x2_t p67 = vfmaq_laneq_f64 (d->c6, f, lane_consts_57, 1);
  float64x2_t p03 = vfmaq_f64 (p01, f2, p23);
  float64x2_t p47 = vfmaq_f64 (p45, f2, p67);
  float64x2_t p89 = vfmaq_laneq_f64 (d->c8, f, lane_consts_910, 0);
  float64x2_t p = vfmaq_laneq_f64 (p89, f2, lane_consts_910, 1);
  p = vfmaq_f64 (p47, f4, p);
  p = vfmaq_f64 (p03, f4, p);

  p = vfmaq_f64 (f, f2, p);

  /* Assemble the result.
     expm1(x) ~= 2^i * (p + 1) - 1
     Let t = 2^i.  */
  int64x2_t u = vaddq_s64 (vshlq_n_s64 (i, 52), d->exponent_bias);
  float64x2_t t = vreinterpretq_f64_s64 (u);

  /* expm1(x) ~= p * t + (t - 1).  */
  return vfmaq_f64 (vsubq_f64 (t, v_f64 (1.0)), p, t);
}

#endif // MATH_V_EXPM1_INLINE_H
