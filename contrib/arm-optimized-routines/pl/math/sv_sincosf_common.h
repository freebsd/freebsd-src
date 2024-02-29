/*
 * Core approximation for single-precision vector sincos
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"

const static struct sv_sincosf_data
{
  float poly_sin[3], poly_cos[3], pio2[3], inv_pio2, shift, range_val;
} sv_sincosf_data = {
  .poly_sin = { /* Generated using Remez, odd coeffs only, in [-pi/4, pi/4].  */
	        -0x1.555546p-3, 0x1.11076p-7, -0x1.994eb4p-13 },
  .poly_cos = { /* Generated using Remez, even coeffs only, in [-pi/4, pi/4].  */
	        0x1.55554ap-5, -0x1.6c0c1ap-10, 0x1.99e0eep-16 },
  .pio2 = { 0x1.921fb6p+0f, -0x1.777a5cp-25f, -0x1.ee59dap-50f },
  .inv_pio2 = 0x1.45f306p-1f,
  .shift = 0x1.8p23,
  .range_val = 0x1p20
};

static inline svbool_t
check_ge_rangeval (svbool_t pg, svfloat32_t x, const struct sv_sincosf_data *d)
{
  svbool_t in_bounds = svaclt (pg, x, d->range_val);
  return svnot_z (pg, in_bounds);
}

/* Single-precision vector function allowing calculation of both sin and cos in
   one function call, using shared argument reduction and separate low-order
   polynomials.
   Worst-case error for sin is 1.67 ULP:
   sv_sincosf_sin(0x1.c704c4p+19) got 0x1.fff698p-5 want 0x1.fff69cp-5
   Worst-case error for cos is 1.81 ULP:
   sv_sincosf_cos(0x1.e506fp+19) got -0x1.ffec6ep-6 want -0x1.ffec72p-6.  */
static inline svfloat32x2_t
sv_sincosf_inline (svbool_t pg, svfloat32_t x, const struct sv_sincosf_data *d)
{
  /* n = rint ( x / (pi/2) ).  */
  svfloat32_t q = svmla_x (pg, sv_f32 (d->shift), x, d->inv_pio2);
  q = svsub_x (pg, q, d->shift);
  svint32_t n = svcvt_s32_x (pg, q);

  /* Reduce x such that r is in [ -pi/4, pi/4 ].  */
  svfloat32_t r = x;
  r = svmls_x (pg, r, q, d->pio2[0]);
  r = svmls_x (pg, r, q, d->pio2[1]);
  r = svmls_x (pg, r, q, d->pio2[2]);

  /* Approximate sin(r) ~= r + r^3 * poly_sin(r^2).  */
  svfloat32_t r2 = svmul_x (pg, r, r), r3 = svmul_x (pg, r, r2);
  svfloat32_t s = svmla_x (pg, sv_f32 (d->poly_sin[1]), r2, d->poly_sin[2]);
  s = svmad_x (pg, r2, s, d->poly_sin[0]);
  s = svmla_x (pg, r, r3, s);

  /* Approximate cos(r) ~= 1 - (r^2)/2 + r^4 * poly_cos(r^2).  */
  svfloat32_t r4 = svmul_x (pg, r2, r2);
  svfloat32_t p = svmla_x (pg, sv_f32 (d->poly_cos[1]), r2, d->poly_cos[2]);
  svfloat32_t c = svmad_x (pg, sv_f32 (d->poly_cos[0]), r2, -0.5);
  c = svmla_x (pg, c, r4, p);
  c = svmad_x (pg, r2, c, 1);

  svuint32_t un = svreinterpret_u32 (n);
  /* If odd quadrant, swap cos and sin.  */
  svbool_t swap = svcmpeq (pg, svlsl_x (pg, un, 31), 0);
  svfloat32_t ss = svsel (swap, s, c);
  svfloat32_t cc = svsel (swap, c, s);

  /* Fix signs according to quadrant.
     ss = asfloat(asuint(ss) ^ ((n       & 2) << 30))
     cc = asfloat(asuint(cc) & (((n + 1) & 2) << 30)).  */
  svuint32_t sin_sign = svlsl_x (pg, svand_x (pg, un, 2), 30);
  svuint32_t cos_sign = svlsl_x (
      pg, svand_x (pg, svreinterpret_u32 (svadd_x (pg, n, 1)), 2), 30);
  ss = svreinterpret_f32 (sveor_x (pg, svreinterpret_u32 (ss), sin_sign));
  cc = svreinterpret_f32 (sveor_x (pg, svreinterpret_u32 (cc), cos_sign));

  return svcreate2 (ss, cc);
}
