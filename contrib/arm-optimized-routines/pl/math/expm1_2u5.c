/*
 * Double-precision e^x - 1 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "poly_scalar_f64.h"
#include "math_config.h"
#include "pl_sig.h"
#include "pl_test.h"

#define InvLn2 0x1.71547652b82fep0
#define Ln2hi 0x1.62e42fefa39efp-1
#define Ln2lo 0x1.abc9e3b39803fp-56
#define Shift 0x1.8p52
/* 0x1p-51, below which expm1(x) is within 2 ULP of x.  */
#define TinyBound 0x3cc0000000000000
/* Above which expm1(x) overflows.  */
#define BigBound 0x1.63108c75a1937p+9
/* Below which expm1(x) rounds to 1.  */
#define NegBound -0x1.740bf7c0d927dp+9
#define AbsMask 0x7fffffffffffffff

/* Approximation for exp(x) - 1 using polynomial on a reduced interval.
   The maximum error observed error is 2.17 ULP:
   expm1(0x1.63f90a866748dp-2) got 0x1.a9af56603878ap-2
			      want 0x1.a9af566038788p-2.  */
double
expm1 (double x)
{
  uint64_t ix = asuint64 (x);
  uint64_t ax = ix & AbsMask;

  /* Tiny, +Infinity.  */
  if (ax <= TinyBound || ix == 0x7ff0000000000000)
    return x;

  /* +/-NaN.  */
  if (ax > 0x7ff0000000000000)
    return __math_invalid (x);

  /* Result is too large to be represented as a double.  */
  if (x >= 0x1.63108c75a1937p+9)
    return __math_oflow (0);

  /* Result rounds to -1 in double precision.  */
  if (x <= NegBound)
    return -1;

  /* Reduce argument to smaller range:
     Let i = round(x / ln2)
     and f = x - i * ln2, then f is in [-ln2/2, ln2/2].
     exp(x) - 1 = 2^i * (expm1(f) + 1) - 1
     where 2^i is exact because i is an integer.  */
  double j = fma (InvLn2, x, Shift) - Shift;
  int64_t i = j;
  double f = fma (j, -Ln2hi, x);
  f = fma (j, -Ln2lo, f);

  /* Approximate expm1(f) using polynomial.
     Taylor expansion for expm1(x) has the form:
	 x + ax^2 + bx^3 + cx^4 ....
     So we calculate the polynomial P(f) = a + bf + cf^2 + ...
     and assemble the approximation expm1(f) ~= f + f^2 * P(f).  */
  double f2 = f * f;
  double f4 = f2 * f2;
  double p = fma (f2, estrin_10_f64 (f, f2, f4, f4 * f4, __expm1_poly), f);

  /* Assemble the result, using a slight rearrangement to achieve acceptable
     accuracy.
     expm1(x) ~= 2^i * (p + 1) - 1
     Let t = 2^(i - 1).  */
  double t = ldexp (0.5, i);
  /* expm1(x) ~= 2 * (p * t + (t - 1/2)).  */
  return 2 * fma (p, t, t - 0.5);
}

PL_SIG (S, D, 1, expm1, -9.9, 9.9)
PL_TEST_ULP (expm1, 1.68)
PL_TEST_SYM_INTERVAL (expm1, 0, 0x1p-51, 1000)
PL_TEST_INTERVAL (expm1, 0x1p-51, 0x1.63108c75a1937p+9, 100000)
PL_TEST_INTERVAL (expm1, -0x1p-51, -0x1.740bf7c0d927dp+9, 100000)
PL_TEST_INTERVAL (expm1, 0x1.63108c75a1937p+9, inf, 100)
PL_TEST_INTERVAL (expm1, -0x1.740bf7c0d927dp+9, -inf, 100)
