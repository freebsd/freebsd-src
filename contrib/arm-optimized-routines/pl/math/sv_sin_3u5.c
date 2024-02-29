/*
 * Double-precision SVE sin(x) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  double inv_pi, pi_1, pi_2, pi_3, shift, range_val;
  double poly[7];
} data = {
  .poly = { -0x1.555555555547bp-3, 0x1.1111111108a4dp-7, -0x1.a01a019936f27p-13,
            0x1.71de37a97d93ep-19, -0x1.ae633919987c6p-26,
            0x1.60e277ae07cecp-33, -0x1.9e9540300a1p-41, },

  .inv_pi = 0x1.45f306dc9c883p-2,
  .pi_1 = 0x1.921fb54442d18p+1,
  .pi_2 = 0x1.1a62633145c06p-53,
  .pi_3 = 0x1.c1cd129024e09p-106,
  .shift = 0x1.8p52,
  .range_val = 0x1p23,
};

#define C(i) sv_f64 (d->poly[i])

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t y, svbool_t cmp)
{
  return sv_call_f64 (sin, x, y, cmp);
}

/* A fast SVE implementation of sin.
   Maximum observed error in [-pi/2, pi/2], where argument is not reduced,
   is 2.87 ULP:
   _ZGVsMxv_sin (0x1.921d5c6a07142p+0) got 0x1.fffffffa7dc02p-1
				      want 0x1.fffffffa7dc05p-1
   Maximum observed error in the entire non-special domain ([-2^23, 2^23])
   is 3.22 ULP:
   _ZGVsMxv_sin (0x1.5702447b6f17bp+22) got 0x1.ffdcd125c84fbp-3
				       want 0x1.ffdcd125c84f8p-3.  */
svfloat64_t SV_NAME_D1 (sin) (svfloat64_t x, const svbool_t pg)
{
  const struct data *d = ptr_barrier (&data);

  /* Load some values in quad-word chunks to minimise memory access.  */
  const svbool_t ptrue = svptrue_b64 ();
  svfloat64_t shift = sv_f64 (d->shift);
  svfloat64_t inv_pi_and_pi1 = svld1rq (ptrue, &d->inv_pi);
  svfloat64_t pi2_and_pi3 = svld1rq (ptrue, &d->pi_2);

  /* n = rint(|x|/pi).  */
  svfloat64_t n = svmla_lane (shift, x, inv_pi_and_pi1, 0);
  svuint64_t odd = svlsl_x (pg, svreinterpret_u64 (n), 63);
  n = svsub_x (pg, n, shift);

  /* r = |x| - n*(pi/2)  (range reduction into -pi/2 .. pi/2).  */
  svfloat64_t r = x;
  r = svmls_lane (r, n, inv_pi_and_pi1, 1);
  r = svmls_lane (r, n, pi2_and_pi3, 0);
  r = svmls_lane (r, n, pi2_and_pi3, 1);

  /* sin(r) poly approx.  */
  svfloat64_t r2 = svmul_x (pg, r, r);
  svfloat64_t r3 = svmul_x (pg, r2, r);
  svfloat64_t r4 = svmul_x (pg, r2, r2);

  svfloat64_t t1 = svmla_x (pg, C (4), C (5), r2);
  svfloat64_t t2 = svmla_x (pg, C (2), C (3), r2);
  svfloat64_t t3 = svmla_x (pg, C (0), C (1), r2);

  svfloat64_t y = svmla_x (pg, t1, C (6), r4);
  y = svmla_x (pg, t2, y, r4);
  y = svmla_x (pg, t3, y, r4);
  y = svmla_x (pg, r, y, r3);

  svbool_t cmp = svacle (pg, x, d->range_val);
  cmp = svnot_z (pg, cmp);
  if (unlikely (svptest_any (pg, cmp)))
    return special_case (x,
			 svreinterpret_f64 (sveor_z (
			     svnot_z (pg, cmp), svreinterpret_u64 (y), odd)),
			 cmp);

  /* Copy sign.  */
  return svreinterpret_f64 (sveor_z (pg, svreinterpret_u64 (y), odd));
}

PL_SIG (SV, D, 1, sin, -3.1, 3.1)
PL_TEST_ULP (SV_NAME_D1 (sin), 2.73)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (sin), 0, 0x1p23, 1000000)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (sin), 0x1p23, inf, 10000)
