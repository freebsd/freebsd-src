/*
 * Double-precision scalar sincospi function.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "math_config.h"
#include "test_sig.h"
#include "test_defs.h"
#include "poly_scalar_f64.h"

/* Taylor series coefficents for sin(pi * x).
   C2 coefficient (orginally ~=5.16771278) has been split into two parts:
   C2_hi = 4, C2_lo = C2 - C2_hi (~=1.16771278)
   This change in magnitude reduces floating point rounding errors.
   C2_hi is then reintroduced after the polynomial approxmation.  */
const static struct sincospi_data
{
  double poly[10];
} sincospi_data = {
  /* Taylor series coefficents for sin(pi * x).  */
  .poly = { 0x1.921fb54442d184p1, -0x1.2aef39896f94bp0, 0x1.466bc6775ab16p1,
	    -0x1.32d2cce62dc33p-1, 0x1.507834891188ep-4, -0x1.e30750a28c88ep-8,
	    0x1.e8f48308acda4p-12, -0x1.6fc0032b3c29fp-16,
	    0x1.af86ae521260bp-21, -0x1.012a9870eeb7dp-25 },
};

/* Top 12 bits of a double (sign and exponent bits).  */
static inline uint64_t
abstop12 (double x)
{
  return (asuint64 (x) >> 52) & 0x7ff;
}

/* Triages special cases into 4 categories:
     -1 or +1 if iy represents half an integer
       -1 if round(y) is odd.
       +1 if round(y) is even.
     -2 or +2 if iy represents and integer.
       -2 if iy is odd.
       +2 if iy is even.
   The argument is the bit representation of a positive non-zero
   finite floating-point value which is either a half or an integer.  */
static inline int
checkint (uint64_t iy)
{
  int e = iy >> 52;
  if (e > 0x3ff + 52)
    return 2;
  if (iy & ((1ULL << (0x3ff + 52 - e)) - 1))
    {
      if ((iy - 1) & 2)
	return -1;
      else
	return 1;
    }
  if (iy & (1 << (0x3ff + 52 - e)))
    return -2;
  return 2;
}

/* Approximation for scalar double-precision sincospi(x).
   Maximum error for sin: 3.46 ULP:
      sincospif_sin(0x1.3d8a067cd8961p+14) got 0x1.ffe609a279008p-1 want
   0x1.ffe609a27900cp-1.
   Maximum error for cos: 3.66 ULP:
      sincospif_cos(0x1.a0ec6997557eep-24) got 0x1.ffffffffffe59p-1 want
   0x1.ffffffffffe5dp-1.  */
void
arm_math_sincospi (double x, double *out_sin, double *out_cos)
{
  const struct sincospi_data *d = ptr_barrier (&sincospi_data);
  uint64_t sign = asuint64 (x) & 0x8000000000000000;

  if (likely (abstop12 (x) < abstop12 (0x1p51)))
    {
      /* ax = |x| - n (range reduction into -1/2 .. 1/2).  */
      double ar_s = x - rint (x);

      /* We know that cospi(x) = sinpi(0.5 - x)
	 range reduction and offset into sinpi range -1/2 .. 1/2
	 ax = 0.5 - |x - rint(x)|.  */
      double ar_c = 0.5 - fabs (ar_s);

      /* ss = sin(pi * ax).  */
      double ar2_s = ar_s * ar_s;
      double ar2_c = ar_c * ar_c;
      double ar4_s = ar2_s * ar2_s;
      double ar4_c = ar2_c * ar2_c;

      uint64_t cc_sign = ((uint64_t) llrint (x)) << 63;
      uint64_t ss_sign = cc_sign;
      if (ar_s == 0)
	ss_sign = sign;

      double ss = pw_horner_9_f64 (ar2_s, ar4_s, d->poly);
      double cc = pw_horner_9_f64 (ar2_c, ar4_c, d->poly);

      /* As all values are reduced to -1/2 .. 1/2, the result of cos(x)
	 always be positive, therefore, the sign must be introduced
	 based upon if x rounds to odd or even. For sin(x) the sign is
	 copied from x.  */
      *out_sin
	  = asdouble (asuint64 (fma (-4 * ar2_s, ar_s, ss * ar_s)) ^ ss_sign);
      *out_cos
	  = asdouble (asuint64 (fma (-4 * ar2_c, ar_c, cc * ar_c)) ^ cc_sign);
    }
  else
    {
      /* When abs(x) > 0x1p51, the x will be either
	    - Half integer (relevant if abs(x) in [0x1p51, 0x1p52])
	    - Odd integer  (relevant if abs(x) in [0x1p52, 0x1p53])
	    - Even integer (relevant if abs(x) in [0x1p53, inf])
	    - Inf or NaN.  */
      if (abstop12 (x) >= 0x7ff)
	{
	  double inv_result = __math_invalid (x);
	  *out_sin = inv_result;
	  *out_cos = inv_result;
	  return;
	}
      else
	{
	  uint64_t ax = asuint64 (x) & 0x7fffffffffffffff;
	  int m = checkint (ax);
	  /* The case where ax is half integer.  */
	  if (m & 1)
	    {
	      *out_sin = sign ? -m : m;
	      *out_cos = 0;
	      return;
	    }
	  /* The case where ax is integer.  */
	  else
	    {
	      *out_sin = asdouble (sign);
	      *out_cos = m >> 1;
	      return;
	    }
	}
    }
}

#if WANT_TRIGPI_TESTS
TEST_DISABLE_FENV (arm_math_sincospi_sin)
TEST_DISABLE_FENV (arm_math_sincospi_cos)
TEST_ULP (arm_math_sincospi_sin, 2.96)
TEST_ULP (arm_math_sincospi_cos, 3.16)
#  define SINCOS_INTERVAL(lo, hi, n)                                          \
    TEST_SYM_INTERVAL (arm_math_sincospi_sin, lo, hi, n)                      \
    TEST_SYM_INTERVAL (arm_math_sincospi_cos, lo, hi, n)
SINCOS_INTERVAL (0, 0x1p-63, 10000)
SINCOS_INTERVAL (0x1p-63, 0.5, 50000)
SINCOS_INTERVAL (0.5, 0x1p51, 50000)
SINCOS_INTERVAL (0x1p51, inf, 10000)
#endif
