/*
 * Double-precision atanh(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "poly_scalar_f64.h"
#include "test_sig.h"
#include "test_defs.h"

#define AbsMask 0x7fffffffffffffff
#define Half 0x3fe0000000000000
#define One 0x3ff0000000000000
#define Ln2Hi 0x1.62e42fefa3800p-1
#define Ln2Lo 0x1.ef35793c76730p-45
#define OneMHfRt2Top                                                          \
  0x00095f62 /* top32(asuint64(1)) - top32(asuint64(sqrt(2)/2)).  */
#define OneTop12 0x3ff
#define HfRt2Top 0x3fe6a09e /* top32(asuint64(sqrt(2)/2)).  */
#define BottomMask 0xffffffff

static inline double
log1p_inline (double x)
{
  /* Helper for calculating log(1 + x) using order-18 polynomial on a reduced
     interval. Copied from log1p_2u.c, with no special-case handling. See that
     file for details of the algorithm.  */
  double m = x + 1;
  uint64_t mi = asuint64 (m);

  /* Decompose x + 1 into (f + 1) * 2^k, with k chosen such that f is in
     [sqrt(2)/2, sqrt(2)].  */
  uint32_t u = (mi >> 32) + OneMHfRt2Top;
  int32_t k = (int32_t) (u >> 20) - OneTop12;
  uint32_t utop = (u & 0x000fffff) + HfRt2Top;
  uint64_t u_red = ((uint64_t) utop << 32) | (mi & BottomMask);
  double f = asdouble (u_red) - 1;

  /* Correction term for round-off in f.  */
  double cm = (x - (m - 1)) / m;

  /* Approximate log1p(f) with polynomial.  */
  double f2 = f * f;
  double f4 = f2 * f2;
  double f8 = f4 * f4;
  double p = fma (
      f, estrin_18_f64 (f, f2, f4, f8, f8 * f8, __log1p_data.coeffs) * f, f);

  /* Recombine log1p(x) = k*log2 + log1p(f) + c/m.  */
  double kd = k;
  double y = fma (Ln2Lo, kd, cm);
  return y + fma (Ln2Hi, kd, p);
}

/* Approximation for double-precision inverse tanh(x), using a simplified
   version of log1p. Greatest observed error is 3.00 ULP:
   atanh(0x1.e58f3c108d714p-4) got 0x1.e7da77672a647p-4
			      want 0x1.e7da77672a64ap-4.  */
double
atanh (double x)
{
  uint64_t ix = asuint64 (x);
  uint64_t sign = ix & ~AbsMask;
  uint64_t ia = ix & AbsMask;

  if (unlikely (ia == One))
    return __math_divzero (sign >> 32);

  if (unlikely (ia > One))
    return __math_invalid (x);

  double halfsign = asdouble (Half | sign);
  double ax = asdouble (ia);
  return halfsign * log1p_inline ((2 * ax) / (1 - ax));
}

TEST_SIG (S, D, 1, atanh, -1.0, 1.0)
TEST_ULP (atanh, 3.00)
TEST_SYM_INTERVAL (atanh, 0, 0x1p-23, 10000)
TEST_SYM_INTERVAL (atanh, 0x1p-23, 1, 90000)
TEST_SYM_INTERVAL (atanh, 1, inf, 100)
