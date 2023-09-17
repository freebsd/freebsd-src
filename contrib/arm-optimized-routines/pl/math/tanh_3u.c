/*
 * Double-precision tanh(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "math_config.h"
#include "estrin.h"
#include "pl_sig.h"
#include "pl_test.h"

#define AbsMask 0x7fffffffffffffff
#define InvLn2 0x1.71547652b82fep0
#define Ln2hi 0x1.62e42fefa39efp-1
#define Ln2lo 0x1.abc9e3b39803fp-56
#define Shift 0x1.8p52
#define C(i) __expm1_poly[i]

#define BoringBound 0x403241bf835f9d5f /* asuint64 (0x1.241bf835f9d5fp+4).  */
#define TinyBound 0x3e40000000000000   /* asuint64 (0x1p-27).  */
#define One 0x3ff0000000000000

static inline double
expm1_inline (double x)
{
  /* Helper routine for calculating exp(x) - 1. Copied from expm1_2u5.c, with
     several simplifications:
     - No special-case handling for tiny or special values.
     - Simpler combination of p and t in final stage of the algorithm.
     - Use shift-and-add instead of ldexp to calculate t.  */

  /* Reduce argument: f in [-ln2/2, ln2/2], i is exact.  */
  double j = fma (InvLn2, x, Shift) - Shift;
  int64_t i = j;
  double f = fma (j, -Ln2hi, x);
  f = fma (j, -Ln2lo, f);

  /* Approximate expm1(f) using polynomial.  */
  double f2 = f * f;
  double f4 = f2 * f2;
  double p = fma (f2, ESTRIN_10 (f, f2, f4, f4 * f4, C), f);

  /* t = 2 ^ i.  */
  double t = asdouble ((uint64_t) (i + 1023) << 52);
  /* expm1(x) = p * t + (t - 1).  */
  return fma (p, t, t - 1);
}

/* Approximation for double-precision tanh(x), using a simplified version of
   expm1. The greatest observed error is 2.75 ULP:
   tanh(-0x1.c143c3a44e087p-3) got -0x1.ba31ba4691ab7p-3
			      want -0x1.ba31ba4691ab4p-3.  */
double
tanh (double x)
{
  uint64_t ix = asuint64 (x);
  uint64_t ia = ix & AbsMask;
  uint64_t sign = ix & ~AbsMask;

  if (unlikely (ia > BoringBound))
    {
      if (ia > 0x7ff0000000000000)
	return __math_invalid (x);
      return asdouble (One | sign);
    }

  if (unlikely (ia < TinyBound))
    return x;

  /* tanh(x) = (e^2x - 1) / (e^2x + 1).  */
  double q = expm1_inline (2 * x);
  return q / (q + 2);
}

PL_SIG (S, D, 1, tanh, -10.0, 10.0)
PL_TEST_ULP (tanh, 2.26)
PL_TEST_INTERVAL (tanh, 0, TinyBound, 1000)
PL_TEST_INTERVAL (tanh, -0, -TinyBound, 1000)
PL_TEST_INTERVAL (tanh, TinyBound, BoringBound, 100000)
PL_TEST_INTERVAL (tanh, -TinyBound, -BoringBound, 100000)
PL_TEST_INTERVAL (tanh, BoringBound, inf, 1000)
PL_TEST_INTERVAL (tanh, -BoringBound, -inf, 1000)
