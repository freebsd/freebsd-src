/*
 * Double-precision sinh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask 0x7fffffffffffffff
#define Half 0x3fe0000000000000
#define OFlowBound                                                             \
  0x40862e42fefa39f0 /* 0x1.62e42fefa39fp+9, above which using expm1 results   \
			in NaN.  */

double
__exp_dd (double, double);

/* Approximation for double-precision sinh(x) using expm1.
   sinh(x) = (exp(x) - exp(-x)) / 2.
   The greatest observed error is 2.57 ULP:
   __v_sinh(0x1.9fb1d49d1d58bp-2) got 0x1.ab34e59d678dcp-2
				 want 0x1.ab34e59d678d9p-2.  */
double
sinh (double x)
{
  uint64_t ix = asuint64 (x);
  uint64_t iax = ix & AbsMask;
  double ax = asdouble (iax);
  uint64_t sign = ix & ~AbsMask;
  double halfsign = asdouble (Half | sign);

  if (unlikely (iax >= OFlowBound))
    {
      /* Special values and overflow.  */
      if (unlikely (iax > 0x7ff0000000000000))
	return __math_invalidf (x);
      /* expm1 overflows a little before sinh. We have to fill this
	 gap by using a different algorithm, in this case we use a
	 double-precision exp helper. For large x sinh(x) is dominated
	 by exp(x), however we cannot compute exp without overflow
	 either. We use the identity: exp(a) = (exp(a / 2)) ^ 2
	 to compute sinh(x) ~= (exp(|x| / 2)) ^ 2 / 2    for x > 0
			    ~= (exp(|x| / 2)) ^ 2 / -2   for x < 0.  */
      double e = __exp_dd (ax / 2, 0);
      return (e * halfsign) * e;
    }

  /* Use expm1f to retain acceptable precision for small numbers.
     Let t = e^(|x|) - 1.  */
  double t = expm1 (ax);
  /* Then sinh(x) = (t + t / (t + 1)) / 2   for x > 0
		    (t + t / (t + 1)) / -2  for x < 0.  */
  return (t + t / (t + 1)) * halfsign;
}

PL_SIG (S, D, 1, sinh, -10.0, 10.0)
PL_TEST_ULP (sinh, 2.08)
PL_TEST_INTERVAL (sinh, 0, 0x1p-51, 100)
PL_TEST_INTERVAL (sinh, -0, -0x1p-51, 100)
PL_TEST_INTERVAL (sinh, 0x1p-51, 0x1.62e42fefa39fp+9, 100000)
PL_TEST_INTERVAL (sinh, -0x1p-51, -0x1.62e42fefa39fp+9, 100000)
PL_TEST_INTERVAL (sinh, 0x1.62e42fefa39fp+9, inf, 1000)
PL_TEST_INTERVAL (sinh, -0x1.62e42fefa39fp+9, -inf, 1000)
