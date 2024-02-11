/*
 * Double-precision log(1+x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "poly_scalar_f64.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define Ln2Hi 0x1.62e42fefa3800p-1
#define Ln2Lo 0x1.ef35793c76730p-45
#define HfRt2Top 0x3fe6a09e /* top32(asuint64(sqrt(2)/2)).  */
#define OneMHfRt2Top                                                           \
  0x00095f62 /* top32(asuint64(1)) - top32(asuint64(sqrt(2)/2)).  */
#define OneTop12 0x3ff
#define BottomMask 0xffffffff
#define OneMHfRt2 0x3fd2bec333018866
#define Rt2MOne 0x3fda827999fcef32
#define AbsMask 0x7fffffffffffffff
#define ExpM63 0x3c00

static inline double
eval_poly (double f)
{
  double f2 = f * f;
  double f4 = f2 * f2;
  double f8 = f4 * f4;
  return estrin_18_f64 (f, f2, f4, f8, f8 * f8, __log1p_data.coeffs);
}

/* log1p approximation using polynomial on reduced interval. Largest
   observed errors are near the lower boundary of the region where k
   is 0.
   Maximum measured error: 1.75ULP.
   log1p(-0x1.2e1aea97b3e5cp-2) got -0x1.65fb8659a2f9p-2
			       want -0x1.65fb8659a2f92p-2.  */
double
log1p (double x)
{
  uint64_t ix = asuint64 (x);
  uint64_t ia = ix & AbsMask;
  uint32_t ia16 = ia >> 48;

  /* Handle special cases first.  */
  if (unlikely (ia16 >= 0x7ff0 || ix >= 0xbff0000000000000
		|| ix == 0x8000000000000000))
    {
      if (ix == 0x8000000000000000 || ix == 0x7ff0000000000000)
	{
	  /* x ==  -0 => log1p(x) =  -0.
	     x == Inf => log1p(x) = Inf.  */
	  return x;
	}
      if (ix == 0xbff0000000000000)
	{
	  /* x == -1 => log1p(x) = -Inf.  */
	  return __math_divzero (-1);
	  ;
	}
      if (ia16 >= 0x7ff0)
	{
	  /* x == +/-NaN => log1p(x) = NaN.  */
	  return __math_invalid (asdouble (ia));
	}
      /* x  <      -1 => log1p(x) =  NaN.
	 x ==    -Inf => log1p(x) =  NaN.  */
      return __math_invalid (x);
    }

  /* With x + 1 = t * 2^k (where t = f + 1 and k is chosen such that f
			   is in [sqrt(2)/2, sqrt(2)]):
     log1p(x) = k*log(2) + log1p(f).

     f may not be representable exactly, so we need a correction term:
     let m = round(1 + x), c = (1 + x) - m.
     c << m: at very small x, log1p(x) ~ x, hence:
     log(1+x) - log(m) ~ c/m.

     We therefore calculate log1p(x) by k*log2 + log1p(f) + c/m.  */

  uint64_t sign = ix & ~AbsMask;
  if (ia <= OneMHfRt2 || (!sign && ia <= Rt2MOne))
    {
      if (unlikely (ia16 <= ExpM63))
	{
	  /* If exponent of x <= -63 then shortcut the polynomial and avoid
	     underflow by just returning x, which is exactly rounded in this
	     region.  */
	  return x;
	}
      /* If x is in [sqrt(2)/2 - 1, sqrt(2) - 1] then we can shortcut all the
	 logic below, as k = 0 and f = x and therefore representable exactly.
	 All we need is to return the polynomial.  */
      return fma (x, eval_poly (x) * x, x);
    }

  /* Obtain correctly scaled k by manipulation in the exponent.  */
  double m = x + 1;
  uint64_t mi = asuint64 (m);
  uint32_t u = (mi >> 32) + OneMHfRt2Top;
  int32_t k = (int32_t) (u >> 20) - OneTop12;

  /* Correction term c/m.  */
  double cm = (x - (m - 1)) / m;

  /* Reduce x to f in [sqrt(2)/2, sqrt(2)].  */
  uint32_t utop = (u & 0x000fffff) + HfRt2Top;
  uint64_t u_red = ((uint64_t) utop << 32) | (mi & BottomMask);
  double f = asdouble (u_red) - 1;

  /* Approximate log1p(x) on the reduced input using a polynomial. Because
     log1p(0)=0 we choose an approximation of the form:
	x + C0*x^2 + C1*x^3 + C2x^4 + ...
     Hence approximation has the form f + f^2 * P(f)
	where P(x) = C0 + C1*x + C2x^2 + ...  */
  double p = fma (f, eval_poly (f) * f, f);

  double kd = k;
  double y = fma (Ln2Lo, kd, cm);
  return y + fma (Ln2Hi, kd, p);
}

PL_SIG (S, D, 1, log1p, -0.9, 10.0)
PL_TEST_ULP (log1p, 1.26)
PL_TEST_SYM_INTERVAL (log1p, 0.0, 0x1p-23, 50000)
PL_TEST_SYM_INTERVAL (log1p, 0x1p-23, 0.001, 50000)
PL_TEST_SYM_INTERVAL (log1p, 0.001, 1.0, 50000)
PL_TEST_SYM_INTERVAL (log1p, 1.0, inf, 5000)
