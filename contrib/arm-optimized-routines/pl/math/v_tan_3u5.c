/*
 * Double-precision vector tan(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "estrin.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#define MHalfPiHi v_f64 (__v_tan_data.neg_half_pi_hi)
#define MHalfPiLo v_f64 (__v_tan_data.neg_half_pi_lo)
#define TwoOverPi v_f64 (0x1.45f306dc9c883p-1)
#define Shift v_f64 (0x1.8p52)
#define AbsMask 0x7fffffffffffffff
#define RangeVal 0x4160000000000000  /* asuint64(2^23).  */
#define TinyBound 0x3e50000000000000 /* asuint64(2^-26).  */
#define C(i) v_f64 (__v_tan_data.poly[i])

/* Special cases (fall back to scalar calls).  */
VPCS_ATTR
NOINLINE static v_f64_t
specialcase (v_f64_t x)
{
  return v_call_f64 (tan, x, x, v_u64 (-1));
}

/* Vector approximation for double-precision tan.
   Maximum measured error is 3.48 ULP:
   __v_tan(0x1.4457047ef78d8p+20) got -0x1.f6ccd8ecf7dedp+37
				 want -0x1.f6ccd8ecf7deap+37.   */
VPCS_ATTR
v_f64_t V_NAME (tan) (v_f64_t x)
{
  v_u64_t iax = v_as_u64_f64 (x) & AbsMask;

  /* Our argument reduction cannot calculate q with sufficient accuracy for very
     large inputs. Fall back to scalar routine for all lanes if any are too
     large, or Inf/NaN. If fenv exceptions are expected, also fall back for tiny
     input to avoid underflow. Note pl does not supply a scalar double-precision
     tan, so the fallback will be statically linked from the system libm.  */
#if WANT_SIMD_EXCEPT
  if (unlikely (v_any_u64 (iax - TinyBound > RangeVal - TinyBound)))
#else
  if (unlikely (v_any_u64 (iax > RangeVal)))
#endif
    return specialcase (x);

  /* q = nearest integer to 2 * x / pi.  */
  v_f64_t q = v_fma_f64 (x, TwoOverPi, Shift) - Shift;
  v_s64_t qi = v_to_s64_f64 (q);

  /* Use q to reduce x to r in [-pi/4, pi/4], by:
     r = x - q * pi/2, in extended precision.  */
  v_f64_t r = x;
  r = v_fma_f64 (q, MHalfPiHi, r);
  r = v_fma_f64 (q, MHalfPiLo, r);
  /* Further reduce r to [-pi/8, pi/8], to be reconstructed using double angle
     formula.  */
  r = r * 0.5;

  /* Approximate tan(r) using order 8 polynomial.
     tan(x) is odd, so polynomial has the form:
     tan(x) ~= x + C0 * x^3 + C1 * x^5 + C3 * x^7 + ...
     Hence we first approximate P(r) = C1 + C2 * r^2 + C3 * r^4 + ...
     Then compute the approximation by:
     tan(r) ~= r + r^3 * (C0 + r^2 * P(r)).  */
  v_f64_t r2 = r * r, r4 = r2 * r2, r8 = r4 * r4;
  /* Use offset version of Estrin wrapper to evaluate from C1 onwards.  */
  v_f64_t p = ESTRIN_7_ (r2, r4, r8, C, 1);
  p = v_fma_f64 (p, r2, C (0));
  p = v_fma_f64 (r2, p * r, r);

  /* Recombination uses double-angle formula:
     tan(2x) = 2 * tan(x) / (1 - (tan(x))^2)
     and reciprocity around pi/2:
     tan(x) = 1 / (tan(pi/2 - x))
     to assemble result using change-of-sign and conditional selection of
     numerator/denominator, dependent on odd/even-ness of q (hence quadrant). */
  v_f64_t n = v_fma_f64 (p, p, v_f64 (-1));
  v_f64_t d = p * 2;

  v_u64_t use_recip = v_cond_u64 ((v_as_u64_s64 (qi) & 1) == 0);

  return v_sel_f64 (use_recip, -d, n) / v_sel_f64 (use_recip, n, d);
}
VPCS_ALIAS

PL_SIG (V, D, 1, tan, -3.1, 3.1)
PL_TEST_ULP (V_NAME (tan), 2.99)
PL_TEST_EXPECT_FENV (V_NAME (tan), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME (tan), 0, TinyBound, 5000)
PL_TEST_INTERVAL (V_NAME (tan), TinyBound, RangeVal, 100000)
PL_TEST_INTERVAL (V_NAME (tan), RangeVal, inf, 5000)
PL_TEST_INTERVAL (V_NAME (tan), -0, -TinyBound, 5000)
PL_TEST_INTERVAL (V_NAME (tan), -TinyBound, -RangeVal, 100000)
PL_TEST_INTERVAL (V_NAME (tan), -RangeVal, -inf, 5000)
#endif
