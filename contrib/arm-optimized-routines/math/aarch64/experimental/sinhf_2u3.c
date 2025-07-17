/*
 * Single-precision sinh(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "math_config.h"
#include "test_sig.h"
#include "test_defs.h"

#define AbsMask 0x7fffffff
#define Half 0x3f000000
/* 0x1.62e43p+6, 2^7*ln2, minimum value for which expm1f overflows.  */
#define Expm1OFlowLimit 0x42b17218
/* 0x1.65a9fap+6, minimum positive value for which sinhf should overflow.  */
#define OFlowLimit 0x42b2d4fd

/* Approximation for single-precision sinh(x) using expm1.
   sinh(x) = (exp(x) - exp(-x)) / 2.
   The maximum error is 2.26 ULP:
   sinhf(0x1.e34a9ep-4) got 0x1.e469ep-4 want 0x1.e469e4p-4.  */
float
sinhf (float x)
{
  uint32_t ix = asuint (x);
  uint32_t iax = ix & AbsMask;
  float ax = asfloat (iax);
  uint32_t sign = ix & ~AbsMask;
  float halfsign = asfloat (Half | sign);

  if (unlikely (iax >= Expm1OFlowLimit))
    {
      /* Special values and overflow.  */
      if (iax >= 0x7fc00001 || iax == 0x7f800000)
	return x;
      if (iax >= 0x7f800000)
	return __math_invalidf (x);
      if (iax >= OFlowLimit)
	return __math_oflowf (sign);

      /* expm1f overflows a little before sinhf, (~88.7 vs ~89.4). We have to
	 fill this gap by using a different algorithm, in this case we use a
	 double-precision exp helper. For large x sinh(x) dominated by exp(x),
	 however we cannot compute exp without overflow either. We use the
	 identity:
	 exp(a) = (exp(a / 2)) ^ 2.
	 to compute sinh(x) ~= (exp(|x| / 2)) ^ 2 / 2    for x > 0
			    ~= (exp(|x| / 2)) ^ 2 / -2   for x < 0.
	 Greatest error in this region is 1.89 ULP:
	 sinhf(0x1.65898cp+6) got 0x1.f00aep+127  want 0x1.f00adcp+127.  */
      float e = expf (ax / 2);
      return (e * halfsign) * e;
    }

  /* Use expm1f to retain acceptable precision for small numbers.
     Let t = e^(|x|) - 1.  */
  float t = expm1f (ax);
  /* Then sinh(x) = (t + t / (t + 1)) / 2   for x > 0
		    (t + t / (t + 1)) / -2  for x < 0.  */
  return (t + t / (t + 1)) * halfsign;
}

TEST_SIG (S, F, 1, sinh, -10.0, 10.0)
TEST_ULP (sinhf, 1.76)
TEST_SYM_INTERVAL (sinhf, 0, 0x1.62e43p+6, 100000)
TEST_SYM_INTERVAL (sinhf, 0x1.62e43p+6, 0x1.65a9fap+6, 100)
TEST_SYM_INTERVAL (sinhf, 0x1.65a9fap+6, inf, 100)
