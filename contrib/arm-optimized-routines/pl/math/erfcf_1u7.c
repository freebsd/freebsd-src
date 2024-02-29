/*
 * Single-precision erfc(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define Shift 0x1p17f
#define OneThird 0x1.555556p-2f
#define TwoThird 0x1.555556p-1f

#define TwoOverFifteen 0x1.111112p-3f
#define TwoOverFive 0x1.99999ap-2f
#define Tenth 0x1.99999ap-4f

#define SignMask 0x7fffffff

/* Fast erfcf approximation based on series expansion near x rounded to
   nearest multiple of 1/64.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erfc(x) ~ erfc(r) - scale * d * poly(r, d), with

   poly(r, d) = 1 - r d + (2/3 r^2 - 1/3) d^2 - r (1/3 r^2 - 1/2) d^3
		+ (2/15 r^4 - 2/5 r^2 + 1/10) d^4

   Values of erfc(r) and scale are read from lookup tables. Stored values
   are scaled to avoid hitting the subnormal range.

   Note that for x < 0, erfc(x) = 2.0 - erfc(-x).

   Maximum error: 1.63 ULP (~1.0 ULP for x < 0.0).
   erfcf(0x1.1dbf7ap+3) got 0x1.f51212p-120
		       want 0x1.f51216p-120.  */
float
erfcf (float x)
{
  /* Get top words and sign.  */
  uint32_t ix = asuint (x);
  uint32_t ia = ix & SignMask;
  uint32_t sign = ix & ~SignMask;

  /* |x| < 0x1.0p-26 => accurate to 0.5 ULP (top12(0x1p-26) = 0x328).  */
  if (unlikely (ia < 0x32800000))
    return 1.0f - x; /* Small case.  */

  /* For |x| < 10.0625, the following approximation holds.  */
  if (likely (ia < 0x41210000))
    {
      /* Lookup erfc(r) and scale(r) in tables, e.g. set erfc(r) to 1 and scale
	 to 2/sqrt(pi), when x reduced to r = 0.  */
      float a = asfloat (ia);
      float z = a + Shift;
      uint32_t i = asuint (z) - asuint (Shift);
      float r = z - Shift;

      /* These values are scaled by 2^-47.  */
      float erfcr = __erfcf_data.tab[i].erfc;
      float scale = __erfcf_data.tab[i].scale;

      /* erfc(x) ~ erfc(r) - scale * d * poly (r, d).  */
      float d = a - r;
      float d2 = d * d;
      float r2 = r * r;
      float p1 = -r;
      float p2 = fmaf (TwoThird, r2, -OneThird);
      float p3 = -r * fmaf (OneThird, r2, -0.5f);
      float p4 = fmaf (fmaf (TwoOverFifteen, r2, -TwoOverFive), r2, Tenth);
      float y = fmaf (p4, d, p3);
      y = fmaf (y, d, p2);
      y = fmaf (y, d, p1);
      y = fmaf (-fmaf (y, d2, d), scale, erfcr);
      /* Handle sign and scale back in a single fma.  */
      float off = asfloat (sign >> 1);
      float fac = asfloat (asuint (0x1p-47f) | sign);
      y = fmaf (y, fac, off);
      /* The underflow exception needs to be signaled explicitly when
	 result gets into subormnal range.  */
      if (x >= 0x1.2639cp+3f)
	force_eval_float (opt_barrier_float (0x1p-123f) * 0x1p-123f);
      return y;
    }

  /* erfcf(nan)=nan, erfcf(+inf)=0 and erfcf(-inf)=2.  */
  if (unlikely (ia >= 0x7f800000))
    return asfloat (sign >> 1) + 1.0f / x; /* Special cases.  */

  /* Above this threshold erfcf is constant and needs to raise underflow
     exception for positive x.  */
  return sign ? 2.0f : __math_uflowf (0);
}

PL_SIG (S, F, 1, erfc, -4.0, 10.0)
PL_TEST_ULP (erfcf, 1.14)
PL_TEST_SYM_INTERVAL (erfcf, 0, 0x1p-26, 40000)
PL_TEST_INTERVAL (erfcf, 0x1p-26, 10.0625, 40000)
PL_TEST_INTERVAL (erfcf, -0x1p-26, -4.0, 40000)
PL_TEST_INTERVAL (erfcf, 10.0625, inf, 40000)
PL_TEST_INTERVAL (erfcf, -4.0, -inf, 40000)
