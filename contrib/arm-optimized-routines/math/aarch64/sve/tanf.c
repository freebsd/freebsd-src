/*
 * Single-precision vector tan(x) function.
 *
 * Copyright (c) 2020-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float pio2_1, pio2_2, pio2_3, invpio2;
  float c1, c3, c5;
  float c0, c2, c4, range_val, shift;
} data = {
  /* Coefficients generated using:
     poly = fpminimax((tan(sqrt(x))-sqrt(x))/x^(3/2),
		      deg,
		      [|single ...|],
		      [a*a;b*b]);
     optimize relative error
     final prec : 23 bits
     deg : 5
     a : 0x1p-126 ^ 2
     b : ((pi) / 0x1p2) ^ 2
     dirty rel error: 0x1.f7c2e4p-25
     dirty abs error: 0x1.f7c2ecp-25.  */
  .c0 = 0x1.55555p-2,	      .c1 = 0x1.11166p-3,
  .c2 = 0x1.b88a78p-5,	      .c3 = 0x1.7b5756p-6,
  .c4 = 0x1.4ef4cep-8,	      .c5 = 0x1.0e1e74p-7,

  .pio2_1 = 0x1.921fb6p+0f,   .pio2_2 = -0x1.777a5cp-25f,
  .pio2_3 = -0x1.ee59dap-50f, .invpio2 = 0x1.45f306p-1f,
  .range_val = 0x1p15f,	      .shift = 0x1.8p+23f
};

static svfloat32_t NOINLINE
special_case (svfloat32_t x, svfloat32_t y, svbool_t cmp)
{
  return sv_call_f32 (tanf, x, y, cmp);
}

/* Fast implementation of SVE tanf.
   Maximum error is 3.45 ULP:
   SV_NAME_F1 (tan)(-0x1.e5f0cap+13) got 0x1.ff9856p-1
				    want 0x1.ff9850p-1.  */
svfloat32_t SV_NAME_F1 (tan) (svfloat32_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  svfloat32_t odd_coeffs = svld1rq (svptrue_b32 (), &d->c1);
  svfloat32_t pi_vals = svld1rq (svptrue_b32 (), &d->pio2_1);

  /* n = rint(x/(pi/2)).  */
  svfloat32_t n = svrintn_x (pg, svmul_lane (x, pi_vals, 3));
  /* n is already a signed integer, simply convert it.  */
  svint32_t in = svcvt_s32_x (pg, n);
  /* Determine if x lives in an interval, where |tan(x)| grows to infinity.  */
  svint32_t alt = svand_x (pg, in, 1);
  svbool_t pred_alt = svcmpne (pg, alt, 0);
  /* r = x - n * (pi/2)  (range reduction into 0 .. pi/4).  */
  svfloat32_t r;
  r = svmls_lane (x, n, pi_vals, 0);
  r = svmls_lane (r, n, pi_vals, 1);
  r = svmls_lane (r, n, pi_vals, 2);

  /* If x lives in an interval, where |tan(x)|
     - is finite, then use a polynomial approximation of the form
       tan(r) ~ r + r^3 * P(r^2) = r + r * r^2 * P(r^2).
     - grows to infinity then use symmetries of tangent and the identity
       tan(r) = cotan(pi/2 - r) to express tan(x) as 1/tan(-r). Finally, use
       the same polynomial approximation of tan as above.  */

  /* Perform additional reduction if required.  */
  svfloat32_t z = svneg_m (r, pred_alt, r);

  /* Evaluate polynomial approximation of tangent on [-pi/4, pi/4],
     using Estrin on z^2.  */
  svfloat32_t z2 = svmul_x (svptrue_b32 (), r, r);
  svfloat32_t p01 = svmla_lane (sv_f32 (d->c0), z2, odd_coeffs, 0);
  svfloat32_t p23 = svmla_lane (sv_f32 (d->c2), z2, odd_coeffs, 1);
  svfloat32_t p45 = svmla_lane (sv_f32 (d->c4), z2, odd_coeffs, 2);

  svfloat32_t z4 = svmul_x (pg, z2, z2);
  svfloat32_t p = svmla_x (pg, p01, z4, p23);

  svfloat32_t z8 = svmul_x (pg, z4, z4);
  p = svmla_x (pg, p, z8, p45);

  svfloat32_t y = svmla_x (pg, z, p, svmul_x (pg, z, z2));

  /* No need to pass pg to specialcase here since cmp is a strict subset,
     guaranteed by the cmpge above.  */

  /* Determine whether input is too large to perform fast regression.  */
  svbool_t cmp = svacge (pg, x, d->range_val);
  if (unlikely (svptest_any (pg, cmp)))
    return special_case (x, svdivr_x (pg, y, 1.0f), cmp);

  svfloat32_t inv_y = svdivr_x (pg, y, 1.0f);
  return svsel (pred_alt, inv_y, y);
}

TEST_SIG (SV, F, 1, tan, -3.1, 3.1)
TEST_ULP (SV_NAME_F1 (tan), 2.96)
TEST_DISABLE_FENV (SV_NAME_F1 (tan))
TEST_INTERVAL (SV_NAME_F1 (tan), -0.0, -0x1p126, 100)
TEST_INTERVAL (SV_NAME_F1 (tan), 0x1p-149, 0x1p-126, 4000)
TEST_INTERVAL (SV_NAME_F1 (tan), 0x1p-126, 0x1p-23, 50000)
TEST_INTERVAL (SV_NAME_F1 (tan), 0x1p-23, 0.7, 50000)
TEST_INTERVAL (SV_NAME_F1 (tan), 0.7, 1.5, 50000)
TEST_INTERVAL (SV_NAME_F1 (tan), 1.5, 100, 50000)
TEST_INTERVAL (SV_NAME_F1 (tan), 100, 0x1p17, 50000)
TEST_INTERVAL (SV_NAME_F1 (tan), 0x1p17, inf, 50000)
CLOSE_SVE_ATTR
