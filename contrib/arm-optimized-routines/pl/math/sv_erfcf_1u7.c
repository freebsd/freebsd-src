/*
 * Single-precision vector erfc(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  uint32_t off_idx, off_arr;
  float max, shift;
  float third, two_thirds, two_over_fifteen, two_over_five, tenth;
} data = {
  /* Set an offset so the range of the index used for lookup is 644, and it can
     be clamped using a saturated add.  */
  .off_idx = 0xb7fffd7b, /* 0xffffffff - asuint(shift) - 644.  */
  .off_arr = 0xfffffd7b, /* 0xffffffff - 644.  */
  .max = 10.0625f,	 /* 644/64.  */
  .shift = 0x1p17f,
  .third = 0x1.555556p-2f,
  .two_thirds = 0x1.555556p-1f,
  .two_over_fifteen = 0x1.111112p-3f,
  .two_over_five = -0x1.99999ap-2f,
  .tenth = -0x1.99999ap-4f,
};

#define SignMask 0x80000000
#define TableScale 0x28000000 /* 0x1p-47.  */

/* Optimized single-precision vector erfcf(x).
   Approximation based on series expansion near x rounded to
   nearest multiple of 1/64.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erfc(x) ~ erfc(r) - scale * d * poly(r, d), with

   poly(r, d) = 1 - r d + (2/3 r^2 - 1/3) d^2 - r (1/3 r^2 - 1/2) d^3
		+ (2/15 r^4 - 2/5 r^2 + 1/10) d^4

   Values of erfc(r) and scale are read from lookup tables. Stored values
   are scaled to avoid hitting the subnormal range.

   Note that for x < 0, erfc(x) = 2.0 - erfc(-x).

   Maximum error: 1.63 ULP (~1.0 ULP for x < 0.0).
   _ZGVsMxv_erfcf(0x1.1dbf7ap+3) got 0x1.f51212p-120
				want 0x1.f51216p-120.  */
svfloat32_t SV_NAME_F1 (erfc) (svfloat32_t x, const svbool_t pg)
{
  const struct data *dat = ptr_barrier (&data);

  svfloat32_t a = svabs_x (pg, x);

  /* Clamp input at |x| <= 10.0 + 4/64.  */
  a = svmin_x (pg, a, dat->max);

  /* Reduce x to the nearest multiple of 1/64.  */
  svfloat32_t shift = sv_f32 (dat->shift);
  svfloat32_t z = svadd_x (pg, a, shift);

  /* Saturate index for the NaN case.  */
  svuint32_t i = svqadd (svreinterpret_u32 (z), dat->off_idx);

  /* Lookup erfc(r) and 2/sqrt(pi)*exp(-r^2) in tables.  */
  i = svmul_x (pg, i, 2);
  const float32_t *p = &__erfcf_data.tab[0].erfc - 2 * dat->off_arr;
  svfloat32_t erfcr = svld1_gather_index (pg, p, i);
  svfloat32_t scale = svld1_gather_index (pg, p + 1, i);

  /* erfc(x) ~ erfc(r) - scale * d * poly(r, d).  */
  svfloat32_t r = svsub_x (pg, z, shift);
  svfloat32_t d = svsub_x (pg, a, r);
  svfloat32_t d2 = svmul_x (pg, d, d);
  svfloat32_t r2 = svmul_x (pg, r, r);

  svfloat32_t coeffs = svld1rq (svptrue_b32 (), &dat->third);
  svfloat32_t third = svdup_lane (coeffs, 0);

  svfloat32_t p1 = r;
  svfloat32_t p2 = svmls_lane (third, r2, coeffs, 1);
  svfloat32_t p3 = svmul_x (pg, r, svmla_lane (sv_f32 (-0.5), r2, coeffs, 0));
  svfloat32_t p4 = svmla_lane (sv_f32 (dat->two_over_five), r2, coeffs, 2);
  p4 = svmls_x (pg, sv_f32 (dat->tenth), r2, p4);

  svfloat32_t y = svmla_x (pg, p3, d, p4);
  y = svmla_x (pg, p2, d, y);
  y = svmla_x (pg, p1, d, y);

  /* Solves the |x| = inf/nan case.  */
  y = svmls_x (pg, erfcr, scale, svmls_x (pg, d, d2, y));

  /* Offset equals 2.0f if sign, else 0.0f.  */
  svuint32_t sign = svand_x (pg, svreinterpret_u32 (x), SignMask);
  svfloat32_t off = svreinterpret_f32 (svlsr_x (pg, sign, 1));
  /* Handle sign and scale back in a single fma.  */
  svfloat32_t fac = svreinterpret_f32 (svorr_x (pg, sign, TableScale));

  return svmla_x (pg, off, fac, y);
}

PL_SIG (SV, F, 1, erfc, -4.0, 10.0)
PL_TEST_ULP (SV_NAME_F1 (erfc), 1.14)
PL_TEST_SYM_INTERVAL (SV_NAME_F1 (erfc), 0.0, 0x1p-26, 40000)
PL_TEST_INTERVAL (SV_NAME_F1 (erfc), 0x1p-26, 10.0625, 40000)
PL_TEST_INTERVAL (SV_NAME_F1 (erfc), -0x1p-26, -4.0, 40000)
PL_TEST_INTERVAL (SV_NAME_F1 (erfc), 10.0625, inf, 40000)
PL_TEST_INTERVAL (SV_NAME_F1 (erfc), -4.0, -inf, 40000)
