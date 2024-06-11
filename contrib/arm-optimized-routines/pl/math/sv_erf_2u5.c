/*
 * Double-precision vector erf(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  double third;
  double tenth, two_over_five, two_over_fifteen;
  double two_over_nine, two_over_fortyfive;
  double max, shift;
} data = {
  .third = 0x1.5555555555556p-2, /* used to compute 2/3 and 1/6 too.  */
  .two_over_fifteen = 0x1.1111111111111p-3,
  .tenth = -0x1.999999999999ap-4,
  .two_over_five = -0x1.999999999999ap-2,
  .two_over_nine = -0x1.c71c71c71c71cp-3,
  .two_over_fortyfive = 0x1.6c16c16c16c17p-5,
  .max = 5.9921875, /* 6 - 1/128.  */
  .shift = 0x1p45,
};

#define SignMask (0x8000000000000000)

/* Double-precision implementation of vector erf(x).
   Approximation based on series expansion near x rounded to
   nearest multiple of 1/128.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,
   erf(x) ~ erf(r) + scale * d * [
       + 1
       - r d
       + 1/3 (2 r^2 - 1) d^2
       - 1/6 (r (2 r^2 - 3)) d^3
       + 1/30 (4 r^4 - 12 r^2 + 3) d^4
       - 1/90 (4 r^4 - 20 r^2 + 15) d^5
     ]

   Maximum measure error: 2.29 ULP
   _ZGVsMxv_erf(-0x1.00003c924e5d1p-8) got -0x1.20dd59132ebadp-8
				      want -0x1.20dd59132ebafp-8.  */
svfloat64_t SV_NAME_D1 (erf) (svfloat64_t x, const svbool_t pg)
{
  const struct data *dat = ptr_barrier (&data);

  /* |x| >= 6.0 - 1/128. Opposite conditions except none of them catch NaNs so
     they can be used in lookup and BSLs to yield the expected results.  */
  svbool_t a_ge_max = svacge (pg, x, dat->max);
  svbool_t a_lt_max = svaclt (pg, x, dat->max);

  /* Set r to multiple of 1/128 nearest to |x|.  */
  svfloat64_t a = svabs_x (pg, x);
  svfloat64_t shift = sv_f64 (dat->shift);
  svfloat64_t z = svadd_x (pg, a, shift);
  svuint64_t i
      = svsub_x (pg, svreinterpret_u64 (z), svreinterpret_u64 (shift));

  /* Lookup without shortcut for small values but with predicate to avoid
     segfault for large values and NaNs.  */
  svfloat64_t r = svsub_x (pg, z, shift);
  svfloat64_t erfr = svld1_gather_index (a_lt_max, __sv_erf_data.erf, i);
  svfloat64_t scale = svld1_gather_index (a_lt_max, __sv_erf_data.scale, i);

  /* erf(x) ~ erf(r) + scale * d * poly (r, d).  */
  svfloat64_t d = svsub_x (pg, a, r);
  svfloat64_t d2 = svmul_x (pg, d, d);
  svfloat64_t r2 = svmul_x (pg, r, r);

  /* poly (d, r) = 1 + p1(r) * d + p2(r) * d^2 + ... + p5(r) * d^5.  */
  svfloat64_t p1 = r;
  svfloat64_t third = sv_f64 (dat->third);
  svfloat64_t twothird = svmul_x (pg, third, 2.0);
  svfloat64_t sixth = svmul_x (pg, third, 0.5);
  svfloat64_t p2 = svmls_x (pg, third, r2, twothird);
  svfloat64_t p3 = svmad_x (pg, r2, third, -0.5);
  p3 = svmul_x (pg, r, p3);
  svfloat64_t p4
      = svmla_x (pg, sv_f64 (dat->two_over_five), r2, dat->two_over_fifteen);
  p4 = svmls_x (pg, sv_f64 (dat->tenth), r2, p4);
  svfloat64_t p5
      = svmla_x (pg, sv_f64 (dat->two_over_nine), r2, dat->two_over_fortyfive);
  p5 = svmla_x (pg, sixth, r2, p5);
  p5 = svmul_x (pg, r, p5);

  svfloat64_t p34 = svmla_x (pg, p3, d, p4);
  svfloat64_t p12 = svmla_x (pg, p1, d, p2);
  svfloat64_t y = svmla_x (pg, p34, d2, p5);
  y = svmla_x (pg, p12, d2, y);

  y = svmla_x (pg, erfr, scale, svmls_x (pg, d, d2, y));

  /* Solves the |x| = inf and NaN cases.  */
  y = svsel (a_ge_max, sv_f64 (1.0), y);

  /* Copy sign.  */
  svuint64_t ix = svreinterpret_u64 (x);
  svuint64_t iy = svreinterpret_u64 (y);
  svuint64_t sign = svand_x (pg, ix, SignMask);
  return svreinterpret_f64 (svorr_x (pg, sign, iy));
}

PL_SIG (SV, D, 1, erf, -6.0, 6.0)
PL_TEST_ULP (SV_NAME_D1 (erf), 1.79)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (erf), 0, 5.9921875, 40000)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (erf), 5.9921875, inf, 40000)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (erf), 0, inf, 4000)
