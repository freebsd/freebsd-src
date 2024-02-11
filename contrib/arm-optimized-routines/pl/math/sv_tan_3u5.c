/*
 * Double-precision SVE tan(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "poly_sve_f64.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  double poly[9];
  double half_pi_hi, half_pi_lo, inv_half_pi, range_val, shift;
} data = {
  /* Polynomial generated with FPMinimax.  */
  .poly = { 0x1.5555555555556p-2, 0x1.1111111110a63p-3, 0x1.ba1ba1bb46414p-5,
	    0x1.664f47e5b5445p-6, 0x1.226e5e5ecdfa3p-7, 0x1.d6c7ddbf87047p-9,
	    0x1.7ea75d05b583ep-10, 0x1.289f22964a03cp-11,
	    0x1.4e4fd14147622p-12, },
  .half_pi_hi = 0x1.921fb54442d18p0,
  .half_pi_lo = 0x1.1a62633145c07p-54,
  .inv_half_pi = 0x1.45f306dc9c883p-1,
  .range_val = 0x1p23,
  .shift = 0x1.8p52,
};

static svfloat64_t NOINLINE
special_case (svfloat64_t x, svfloat64_t y, svbool_t special)
{
  return sv_call_f64 (tan, x, y, special);
}

/* Vector approximation for double-precision tan.
   Maximum measured error is 3.48 ULP:
   _ZGVsMxv_tan(0x1.4457047ef78d8p+20) got -0x1.f6ccd8ecf7dedp+37
				      want -0x1.f6ccd8ecf7deap+37.  */
svfloat64_t SV_NAME_D1 (tan) (svfloat64_t x, svbool_t pg)
{
  const struct data *dat = ptr_barrier (&data);

  /* Invert condition to catch NaNs and Infs as well as large values.  */
  svbool_t special = svnot_z (pg, svaclt (pg, x, dat->range_val));

  /* q = nearest integer to 2 * x / pi.  */
  svfloat64_t shift = sv_f64 (dat->shift);
  svfloat64_t q = svmla_x (pg, shift, x, dat->inv_half_pi);
  q = svsub_x (pg, q, shift);
  svint64_t qi = svcvt_s64_x (pg, q);

  /* Use q to reduce x to r in [-pi/4, pi/4], by:
     r = x - q * pi/2, in extended precision.  */
  svfloat64_t r = x;
  svfloat64_t half_pi = svld1rq (svptrue_b64 (), &dat->half_pi_hi);
  r = svmls_lane (r, q, half_pi, 0);
  r = svmls_lane (r, q, half_pi, 1);
  /* Further reduce r to [-pi/8, pi/8], to be reconstructed using double angle
     formula.  */
  r = svmul_x (pg, r, 0.5);

  /* Approximate tan(r) using order 8 polynomial.
     tan(x) is odd, so polynomial has the form:
     tan(x) ~= x + C0 * x^3 + C1 * x^5 + C3 * x^7 + ...
     Hence we first approximate P(r) = C1 + C2 * r^2 + C3 * r^4 + ...
     Then compute the approximation by:
     tan(r) ~= r + r^3 * (C0 + r^2 * P(r)).  */
  svfloat64_t r2 = svmul_x (pg, r, r);
  svfloat64_t r4 = svmul_x (pg, r2, r2);
  svfloat64_t r8 = svmul_x (pg, r4, r4);
  /* Use offset version coeff array by 1 to evaluate from C1 onwards.  */
  svfloat64_t p = sv_estrin_7_f64_x (pg, r2, r4, r8, dat->poly + 1);
  p = svmad_x (pg, p, r2, dat->poly[0]);
  p = svmla_x (pg, r, r2, svmul_x (pg, p, r));

  /* Recombination uses double-angle formula:
     tan(2x) = 2 * tan(x) / (1 - (tan(x))^2)
     and reciprocity around pi/2:
     tan(x) = 1 / (tan(pi/2 - x))
     to assemble result using change-of-sign and conditional selection of
     numerator/denominator dependent on odd/even-ness of q (hence quadrant).  */
  svbool_t use_recip
      = svcmpeq (pg, svand_x (pg, svreinterpret_u64 (qi), 1), 0);

  svfloat64_t n = svmad_x (pg, p, p, -1);
  svfloat64_t d = svmul_x (pg, p, 2);
  svfloat64_t swap = n;
  n = svneg_m (n, use_recip, d);
  d = svsel (use_recip, swap, d);
  if (unlikely (svptest_any (pg, special)))
    return special_case (x, svdiv_x (svnot_z (pg, special), n, d), special);
  return svdiv_x (pg, n, d);
}

PL_SIG (SV, D, 1, tan, -3.1, 3.1)
PL_TEST_ULP (SV_NAME_D1 (tan), 2.99)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (tan), 0, 0x1p23, 500000)
PL_TEST_SYM_INTERVAL (SV_NAME_D1 (tan), 0x1p23, inf, 5000)
