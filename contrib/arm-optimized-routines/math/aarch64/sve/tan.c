/*
 * Double-precision SVE tan(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  double c2, c4, c6, c8;
  double poly_1357[4];
  double c0, inv_half_pi;
  double half_pi_hi, half_pi_lo, range_val;
} data = {
  /* Polynomial generated with FPMinimax.  */
  .c2 = 0x1.ba1ba1bb46414p-5,
  .c4 = 0x1.226e5e5ecdfa3p-7,
  .c6 = 0x1.7ea75d05b583ep-10,
  .c8 = 0x1.4e4fd14147622p-12,
  .poly_1357 = { 0x1.1111111110a63p-3, 0x1.664f47e5b5445p-6,
		 0x1.d6c7ddbf87047p-9, 0x1.289f22964a03cp-11 },
  .c0 = 0x1.5555555555556p-2,
  .inv_half_pi = 0x1.45f306dc9c883p-1,
  .half_pi_hi = 0x1.921fb54442d18p0,
  .half_pi_lo = 0x1.1a62633145c07p-54,
  .range_val = 0x1p23,
};

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t p, svfloat64_t q, svbool_t pg,
	      svbool_t special)
{
  svbool_t use_recip = svcmpeq (
      pg, svand_x (pg, svreinterpret_u64 (svcvt_s64_x (pg, q)), 1), 0);

  svfloat64_t n = svmad_x (pg, p, p, -1);
  svfloat64_t d = svmul_x (svptrue_b64 (), p, 2);
  svfloat64_t swap = n;
  n = svneg_m (n, use_recip, d);
  d = svsel (use_recip, swap, d);
  svfloat64_t y = svdiv_x (svnot_z (pg, special), n, d);
  return sv_call_f64 (tan, x, y, special);
}

/* Vector approximation for double-precision tan.
   Maximum measured error is 3.48 ULP:
   _ZGVsMxv_tan(0x1.4457047ef78d8p+20) got -0x1.f6ccd8ecf7dedp+37
				      want -0x1.f6ccd8ecf7deap+37.  */
svfloat64_t SV_NAME_D1 (tan) (svfloat64_t x, svbool_t pg)
{
  const struct data *dat = ptr_barrier (&data);
  svfloat64_t half_pi_c0 = svld1rq (svptrue_b64 (), &dat->c0);
  /* q = nearest integer to 2 * x / pi.  */
  svfloat64_t q = svmul_lane (x, half_pi_c0, 1);
  q = svrinta_x (pg, q);

  /* Use q to reduce x to r in [-pi/4, pi/4], by:
     r = x - q * pi/2, in extended precision.  */
  svfloat64_t r = x;
  svfloat64_t half_pi = svld1rq (svptrue_b64 (), &dat->half_pi_hi);
  r = svmls_lane (r, q, half_pi, 0);
  r = svmls_lane (r, q, half_pi, 1);
  /* Further reduce r to [-pi/8, pi/8], to be reconstructed using double angle
     formula.  */
  r = svmul_x (svptrue_b64 (), r, 0.5);

  /* Approximate tan(r) using order 8 polynomial.
     tan(x) is odd, so polynomial has the form:
     tan(x) ~= x + C0 * x^3 + C1 * x^5 + C3 * x^7 + ...
     Hence we first approximate P(r) = C1 + C2 * r^2 + C3 * r^4 + ...
     Then compute the approximation by:
     tan(r) ~= r + r^3 * (C0 + r^2 * P(r)).  */

  svfloat64_t r2 = svmul_x (svptrue_b64 (), r, r);
  svfloat64_t r4 = svmul_x (svptrue_b64 (), r2, r2);
  svfloat64_t r8 = svmul_x (svptrue_b64 (), r4, r4);
  /* Use offset version coeff array by 1 to evaluate from C1 onwards.  */
  svfloat64_t C_24 = svld1rq (svptrue_b64 (), &dat->c2);
  svfloat64_t C_68 = svld1rq (svptrue_b64 (), &dat->c6);

  /* Use offset version coeff array by 1 to evaluate from C1 onwards.  */
  svfloat64_t p01 = svmla_lane (sv_f64 (dat->poly_1357[0]), r2, C_24, 0);
  svfloat64_t p23 = svmla_lane_f64 (sv_f64 (dat->poly_1357[1]), r2, C_24, 1);
  svfloat64_t p03 = svmla_x (pg, p01, p23, r4);

  svfloat64_t p45 = svmla_lane (sv_f64 (dat->poly_1357[2]), r2, C_68, 0);
  svfloat64_t p67 = svmla_lane (sv_f64 (dat->poly_1357[3]), r2, C_68, 1);
  svfloat64_t p47 = svmla_x (pg, p45, p67, r4);

  svfloat64_t p = svmla_x (pg, p03, p47, r8);

  svfloat64_t z = svmul_x (svptrue_b64 (), p, r);
  z = svmul_x (svptrue_b64 (), r2, z);
  z = svmla_lane (z, r, half_pi_c0, 0);
  p = svmla_x (pg, r, r2, z);

  /* Recombination uses double-angle formula:
     tan(2x) = 2 * tan(x) / (1 - (tan(x))^2)
     and reciprocity around pi/2:
     tan(x) = 1 / (tan(pi/2 - x))
     to assemble result using change-of-sign and conditional selection of
     numerator/denominator dependent on odd/even-ness of q (quadrant).  */

  /* Invert condition to catch NaNs and Infs as well as large values.  */
  svbool_t special = svnot_z (pg, svaclt (pg, x, dat->range_val));

  if (unlikely (svptest_any (pg, special)))
    {
      return special_case (x, p, q, pg, special);
    }
  svbool_t use_recip = svcmpeq (
      pg, svand_x (pg, svreinterpret_u64 (svcvt_s64_x (pg, q)), 1), 0);

  svfloat64_t n = svmad_x (pg, p, p, -1);
  svfloat64_t d = svmul_x (svptrue_b64 (), p, 2);
  svfloat64_t swap = n;
  n = svneg_m (n, use_recip, d);
  d = svsel (use_recip, swap, d);
  return svdiv_x (pg, n, d);
}

TEST_SIG (SV, D, 1, tan, -3.1, 3.1)
TEST_ULP (SV_NAME_D1 (tan), 2.99)
TEST_DISABLE_FENV (SV_NAME_D1 (tan))
TEST_SYM_INTERVAL (SV_NAME_D1 (tan), 0, 0x1p23, 500000)
TEST_SYM_INTERVAL (SV_NAME_D1 (tan), 0x1p23, inf, 5000)
CLOSE_SVE_ATTR
