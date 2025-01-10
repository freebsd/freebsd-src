/*
 * Double-precision 10^x function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "test_defs.h"
#include "test_sig.h"

#define N (1 << EXP_TABLE_BITS)
#define IndexMask (N - 1)
#define OFlowBound 0x1.34413509f79ffp8 /* log10(DBL_MAX).  */
#define UFlowBound -0x1.5ep+8 /* -350.  */
#define SmallTop 0x3c6 /* top12(0x1p-57).  */
#define BigTop 0x407   /* top12(0x1p8).  */
#define Thresh 0x41    /* BigTop - SmallTop.  */
#define Shift __exp_data.shift
#define C(i) __exp_data.exp10_poly[i]

static double
special_case (uint64_t sbits, double_t tmp, uint64_t ki)
{
  double_t scale, y;

  if ((ki & 0x80000000) == 0)
    {
      /* The exponent of scale might have overflowed by 1.  */
      sbits -= 1ull << 52;
      scale = asdouble (sbits);
      y = 2 * (scale + scale * tmp);
      return check_oflow (eval_as_double (y));
    }

  /* n < 0, need special care in the subnormal range.  */
  sbits += 1022ull << 52;
  scale = asdouble (sbits);
  y = scale + scale * tmp;

  if (y < 1.0)
    {
      /* Round y to the right precision before scaling it into the subnormal
	 range to avoid double rounding that can cause 0.5+E/2 ulp error where
	 E is the worst-case ulp error outside the subnormal range.  So this
	 is only useful if the goal is better than 1 ulp worst-case error.  */
      double_t lo = scale - y + scale * tmp;
      double_t hi = 1.0 + y;
      lo = 1.0 - hi + y + lo;
      y = eval_as_double (hi + lo) - 1.0;
      /* Avoid -0.0 with downward rounding.  */
      if (WANT_ROUNDING && y == 0.0)
	y = 0.0;
      /* The underflow exception needs to be signaled explicitly.  */
      force_eval_double (opt_barrier_double (0x1p-1022) * 0x1p-1022);
    }
  y = 0x1p-1022 * y;

  return check_uflow (y);
}

/* Double-precision 10^x approximation. Largest observed error is ~0.513 ULP.  */
double
exp10 (double x)
{
  uint64_t ix = asuint64 (x);
  uint32_t abstop = (ix >> 52) & 0x7ff;

  if (unlikely (abstop - SmallTop >= Thresh))
    {
      if (abstop - SmallTop >= 0x80000000)
	/* Avoid spurious underflow for tiny x.
	   Note: 0 is common input.  */
	return x + 1;
      if (abstop == 0x7ff)
	return ix == asuint64 (-INFINITY) ? 0.0 : x + 1.0;
      if (x >= OFlowBound)
	return __math_oflow (0);
      if (x < UFlowBound)
	return __math_uflow (0);

      /* Large x is special-cased below.  */
      abstop = 0;
    }

  /* Reduce x: z = x * N / log10(2), k = round(z).  */
  double_t z = __exp_data.invlog10_2N * x;
  double_t kd;
  uint64_t ki;
#if TOINT_INTRINSICS
  kd = roundtoint (z);
  ki = converttoint (z);
#else
  kd = eval_as_double (z + Shift);
  ki = asuint64 (kd);
  kd -= Shift;
#endif

  /* r = x - k * log10(2), r in [-0.5, 0.5].  */
  double_t r = x;
  r = __exp_data.neglog10_2hiN * kd + r;
  r = __exp_data.neglog10_2loN * kd + r;

  /* exp10(x) = 2^(k/N) * 2^(r/N).
     Approximate the two components separately.  */

  /* s = 2^(k/N), using lookup table.  */
  uint64_t e = ki << (52 - EXP_TABLE_BITS);
  uint64_t i = (ki & IndexMask) * 2;
  uint64_t u = __exp_data.tab[i + 1];
  uint64_t sbits = u + e;

  double_t tail = asdouble (__exp_data.tab[i]);

  /* 2^(r/N) ~= 1 + r * Poly(r).  */
  double_t r2 = r * r;
  double_t p = C (0) + r * C (1);
  double_t y = C (2) + r * C (3);
  y = y + r2 * C (4);
  y = p + r2 * y;
  y = tail + y * r;

  if (unlikely (abstop == 0))
    return special_case (sbits, y, ki);

  /* Assemble components:
     y  = 2^(r/N) * 2^(k/N)
       ~= (y + 1) * s.  */
  double_t s = asdouble (sbits);
  return eval_as_double (s * y + s);
}

#if WANT_EXP10_TESTS
TEST_SIG (S, D, 1, exp10, -9.9, 9.9)
TEST_ULP (exp10, 0.02)
TEST_ULP_NONNEAREST (exp10, 0.5)
TEST_SYM_INTERVAL (exp10, 0, 0x1p-47, 5000)
TEST_SYM_INTERVAL (exp10, 0x1p47, 1, 50000)
TEST_INTERVAL (exp10, 1, OFlowBound, 50000)
TEST_INTERVAL (exp10, -1, UFlowBound, 50000)
TEST_INTERVAL (exp10, OFlowBound, inf, 5000)
TEST_INTERVAL (exp10, UFlowBound, -inf, 5000)
#endif
