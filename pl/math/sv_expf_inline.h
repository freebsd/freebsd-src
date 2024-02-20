/*
 * SVE helper for single-precision routines which calculate exp(x) and do
 * not need special-case handling
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef PL_MATH_SV_EXPF_INLINE_H
#define PL_MATH_SV_EXPF_INLINE_H

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

struct sv_expf_data
{
  float poly[5];
  float inv_ln2, ln2_hi, ln2_lo, shift;
};

/* Coefficients copied from the polynomial in AdvSIMD variant, reversed for
   compatibility with polynomial helpers. Shift is 1.5*2^17 + 127.  */
#define SV_EXPF_DATA                                                          \
  {                                                                           \
    .poly = { 0x1.ffffecp-1f, 0x1.fffdb6p-2f, 0x1.555e66p-3f, 0x1.573e2ep-5f, \
	      0x1.0e4020p-7f },                                               \
                                                                              \
    .inv_ln2 = 0x1.715476p+0f, .ln2_hi = 0x1.62e4p-1f,                        \
    .ln2_lo = 0x1.7f7d1cp-20f, .shift = 0x1.803f8p17f,                        \
  }

#define C(i) sv_f32 (d->poly[i])

static inline svfloat32_t
expf_inline (svfloat32_t x, const svbool_t pg, const struct sv_expf_data *d)
{
  /* exp(x) = 2^n (1 + poly(r)), with 1 + poly(r) in [1/sqrt(2),sqrt(2)]
     x = ln2*n + r, with r in [-ln2/2, ln2/2].  */

  /* Load some constants in quad-word chunks to minimise memory access.  */
  svfloat32_t c4_invln2_and_ln2 = svld1rq (svptrue_b32 (), &d->poly[4]);

  /* n = round(x/(ln2/N)).  */
  svfloat32_t z = svmla_lane (sv_f32 (d->shift), x, c4_invln2_and_ln2, 1);
  svfloat32_t n = svsub_x (pg, z, d->shift);

  /* r = x - n*ln2/N.  */
  svfloat32_t r = svmls_lane (x, n, c4_invln2_and_ln2, 2);
  r = svmls_lane (r, n, c4_invln2_and_ln2, 3);

  /* scale = 2^(n/N).  */
  svfloat32_t scale = svexpa (svreinterpret_u32_f32 (z));

  /* y = exp(r) - 1 ~= r + C0 r^2 + C1 r^3 + C2 r^4 + C3 r^5 + C4 r^6.  */
  svfloat32_t p12 = svmla_x (pg, C (1), C (2), r);
  svfloat32_t p34 = svmla_lane (C (3), r, c4_invln2_and_ln2, 0);
  svfloat32_t r2 = svmul_f32_x (pg, r, r);
  svfloat32_t p14 = svmla_x (pg, p12, p34, r2);
  svfloat32_t p0 = svmul_f32_x (pg, r, C (0));
  svfloat32_t poly = svmla_x (pg, p0, r2, p14);

  return svmla_x (pg, scale, scale, poly);
}

#endif // PL_MATH_SV_EXPF_INLINE_H