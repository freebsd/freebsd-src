/*
 * Double-precision vector tan(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "v_poly_f64.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float64x2_t poly[9];
  double half_pi[2];
  float64x2_t two_over_pi, shift;
#if !WANT_SIMD_EXCEPT
  float64x2_t range_val;
#endif
} data = {
  /* Coefficients generated using FPMinimax.  */
  .poly = { V2 (0x1.5555555555556p-2), V2 (0x1.1111111110a63p-3),
	    V2 (0x1.ba1ba1bb46414p-5), V2 (0x1.664f47e5b5445p-6),
	    V2 (0x1.226e5e5ecdfa3p-7), V2 (0x1.d6c7ddbf87047p-9),
	    V2 (0x1.7ea75d05b583ep-10), V2 (0x1.289f22964a03cp-11),
	    V2 (0x1.4e4fd14147622p-12) },
  .half_pi = { 0x1.921fb54442d18p0, 0x1.1a62633145c07p-54 },
  .two_over_pi = V2 (0x1.45f306dc9c883p-1),
  .shift = V2 (0x1.8p52),
#if !WANT_SIMD_EXCEPT
  .range_val = V2 (0x1p23),
#endif
};

#define RangeVal 0x4160000000000000  /* asuint64(0x1p23).  */
#define TinyBound 0x3e50000000000000 /* asuint64(2^-26).  */
#define Thresh 0x310000000000000     /* RangeVal - TinyBound.  */

/* Special cases (fall back to scalar calls).  */
static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x)
{
  return v_call_f64 (tan, x, x, v_u64 (-1));
}

/* Vector approximation for double-precision tan.
   Maximum measured error is 3.48 ULP:
   _ZGVnN2v_tan(0x1.4457047ef78d8p+20) got -0x1.f6ccd8ecf7dedp+37
				      want -0x1.f6ccd8ecf7deap+37.  */
float64x2_t VPCS_ATTR V_NAME_D1 (tan) (float64x2_t x)
{
  const struct data *dat = ptr_barrier (&data);
  /* Our argument reduction cannot calculate q with sufficient accuracy for
     very large inputs. Fall back to scalar routine for all lanes if any are
     too large, or Inf/NaN. If fenv exceptions are expected, also fall back for
     tiny input to avoid underflow.  */
#if WANT_SIMD_EXCEPT
  uint64x2_t iax = vreinterpretq_u64_f64 (vabsq_f64 (x));
  /* iax - tiny_bound > range_val - tiny_bound.  */
  uint64x2_t special
      = vcgtq_u64 (vsubq_u64 (iax, v_u64 (TinyBound)), v_u64 (Thresh));
  if (unlikely (v_any_u64 (special)))
    return special_case (x);
#endif

  /* q = nearest integer to 2 * x / pi.  */
  float64x2_t q
      = vsubq_f64 (vfmaq_f64 (dat->shift, x, dat->two_over_pi), dat->shift);
  int64x2_t qi = vcvtq_s64_f64 (q);

  /* Use q to reduce x to r in [-pi/4, pi/4], by:
     r = x - q * pi/2, in extended precision.  */
  float64x2_t r = x;
  float64x2_t half_pi = vld1q_f64 (dat->half_pi);
  r = vfmsq_laneq_f64 (r, q, half_pi, 0);
  r = vfmsq_laneq_f64 (r, q, half_pi, 1);
  /* Further reduce r to [-pi/8, pi/8], to be reconstructed using double angle
     formula.  */
  r = vmulq_n_f64 (r, 0.5);

  /* Approximate tan(r) using order 8 polynomial.
     tan(x) is odd, so polynomial has the form:
     tan(x) ~= x + C0 * x^3 + C1 * x^5 + C3 * x^7 + ...
     Hence we first approximate P(r) = C1 + C2 * r^2 + C3 * r^4 + ...
     Then compute the approximation by:
     tan(r) ~= r + r^3 * (C0 + r^2 * P(r)).  */
  float64x2_t r2 = vmulq_f64 (r, r), r4 = vmulq_f64 (r2, r2),
	      r8 = vmulq_f64 (r4, r4);
  /* Offset coefficients to evaluate from C1 onwards.  */
  float64x2_t p = v_estrin_7_f64 (r2, r4, r8, dat->poly + 1);
  p = vfmaq_f64 (dat->poly[0], p, r2);
  p = vfmaq_f64 (r, r2, vmulq_f64 (p, r));

  /* Recombination uses double-angle formula:
     tan(2x) = 2 * tan(x) / (1 - (tan(x))^2)
     and reciprocity around pi/2:
     tan(x) = 1 / (tan(pi/2 - x))
     to assemble result using change-of-sign and conditional selection of
     numerator/denominator, dependent on odd/even-ness of q (hence quadrant).
   */
  float64x2_t n = vfmaq_f64 (v_f64 (-1), p, p);
  float64x2_t d = vaddq_f64 (p, p);

  uint64x2_t no_recip = vtstq_u64 (vreinterpretq_u64_s64 (qi), v_u64 (1));

#if !WANT_SIMD_EXCEPT
  uint64x2_t special = vcageq_f64 (x, dat->range_val);
  if (unlikely (v_any_u64 (special)))
    return special_case (x);
#endif

  return vdivq_f64 (vbslq_f64 (no_recip, n, vnegq_f64 (d)),
		    vbslq_f64 (no_recip, d, n));
}

TEST_SIG (V, D, 1, tan, -3.1, 3.1)
TEST_ULP (V_NAME_D1 (tan), 2.99)
TEST_DISABLE_FENV_IF_NOT (V_NAME_D1 (tan), WANT_SIMD_EXCEPT)
TEST_SYM_INTERVAL (V_NAME_D1 (tan), 0, TinyBound, 5000)
TEST_SYM_INTERVAL (V_NAME_D1 (tan), TinyBound, RangeVal, 100000)
TEST_SYM_INTERVAL (V_NAME_D1 (tan), RangeVal, inf, 5000)
