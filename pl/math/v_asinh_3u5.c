/*
 * Double-precision vector asinh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "estrin.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

#define OneTop 0x3ff	/* top12(asuint64(1.0f)).  */
#define HugeBound 0x5fe /* top12(asuint64(0x1p511)).  */
#define TinyBound 0x3e5 /* top12(asuint64(0x1p-26)).  */
#define AbsMask v_u64 (0x7fffffffffffffff)
#define C(i) v_f64 (__asinh_data.poly[i])

/* Constants & data for log.  */
#define OFF 0x3fe6000000000000
#define Ln2 v_f64 (0x1.62e42fefa39efp-1)
#define A(i) v_f64 (__sv_log_data.poly[i])
#define T(i) __log_data.tab[i]
#define N (1 << LOG_TABLE_BITS)

static NOINLINE v_f64_t
special_case (v_f64_t x, v_f64_t y, v_u64_t special)
{
  return v_call_f64 (asinh, x, y, special);
}

struct entry
{
  v_f64_t invc;
  v_f64_t logc;
};

static inline struct entry
lookup (v_u64_t i)
{
  struct entry e;
#ifdef SCALAR
  e.invc = T (i).invc;
  e.logc = T (i).logc;
#else
  e.invc[0] = T (i[0]).invc;
  e.logc[0] = T (i[0]).logc;
  e.invc[1] = T (i[1]).invc;
  e.logc[1] = T (i[1]).logc;
#endif
  return e;
}

static inline v_f64_t
log_inline (v_f64_t x)
{
  /* Double-precision vector log, copied from math/v_log.c with some cosmetic
     modification and special-cases removed. See that file for details of the
     algorithm used.  */
  v_u64_t ix = v_as_u64_f64 (x);
  v_u64_t tmp = ix - OFF;
  v_u64_t i = (tmp >> (52 - LOG_TABLE_BITS)) % N;
  v_s64_t k = v_as_s64_u64 (tmp) >> 52;
  v_u64_t iz = ix - (tmp & 0xfffULL << 52);
  v_f64_t z = v_as_f64_u64 (iz);
  struct entry e = lookup (i);
  v_f64_t r = v_fma_f64 (z, e.invc, v_f64 (-1.0));
  v_f64_t kd = v_to_f64_s64 (k);
  v_f64_t hi = v_fma_f64 (kd, Ln2, e.logc + r);
  v_f64_t r2 = r * r;
  v_f64_t y = v_fma_f64 (A (3), r, A (2));
  v_f64_t p = v_fma_f64 (A (1), r, A (0));
  y = v_fma_f64 (A (4), r2, y);
  y = v_fma_f64 (y, r2, p);
  y = v_fma_f64 (y, r2, hi);
  return y;
}

/* Double-precision implementation of vector asinh(x).
   asinh is very sensitive around 1, so it is impractical to devise a single
   low-cost algorithm which is sufficiently accurate on a wide range of input.
   Instead we use two different algorithms:
   asinh(x) = sign(x) * log(|x| + sqrt(x^2 + 1)      if |x| >= 1
	    = sign(x) * (|x| + |x|^3 * P(x^2))       otherwise
   where log(x) is an optimized log approximation, and P(x) is a polynomial
   shared with the scalar routine. The greatest observed error 3.29 ULP, in
   |x| >= 1:
   __v_asinh(0x1.2cd9d717e2c9bp+0) got 0x1.ffffcfd0e234fp-1
				  want 0x1.ffffcfd0e2352p-1.  */
VPCS_ATTR v_f64_t V_NAME (asinh) (v_f64_t x)
{
  v_u64_t ix = v_as_u64_f64 (x);
  v_u64_t iax = ix & AbsMask;
  v_f64_t ax = v_as_f64_u64 (iax);
  v_u64_t top12 = iax >> 52;

  v_u64_t gt1 = v_cond_u64 (top12 >= OneTop);
  v_u64_t special = v_cond_u64 (top12 >= HugeBound);

#if WANT_SIMD_EXCEPT
  v_u64_t tiny = v_cond_u64 (top12 < TinyBound);
  special |= tiny;
#endif

  /* Option 1: |x| >= 1.
     Compute asinh(x) according by asinh(x) = log(x + sqrt(x^2 + 1)).
     If WANT_SIMD_EXCEPT is enabled, sidestep special values, which will
     overflow, by setting special lanes to 1. These will be fixed later.  */
  v_f64_t option_1 = v_f64 (0);
  if (likely (v_any_u64 (gt1)))
    {
#if WANT_SIMD_EXCEPT
      v_f64_t xm = v_sel_f64 (special, v_f64 (1), ax);
#else
      v_f64_t xm = ax;
#endif
      option_1 = log_inline (xm + v_sqrt_f64 (xm * xm + 1));
    }

  /* Option 2: |x| < 1.
     Compute asinh(x) using a polynomial.
     If WANT_SIMD_EXCEPT is enabled, sidestep special lanes, which will
     overflow, and tiny lanes, which will underflow, by setting them to 0. They
     will be fixed later, either by selecting x or falling back to the scalar
     special-case. The largest observed error in this region is 1.47 ULPs:
     __v_asinh(0x1.fdfcd00cc1e6ap-1) got 0x1.c1d6bf874019bp-1
				    want 0x1.c1d6bf874019cp-1.  */
  v_f64_t option_2 = v_f64 (0);
  if (likely (v_any_u64 (~gt1)))
    {
#if WANT_SIMD_EXCEPT
      ax = v_sel_f64 (tiny | gt1, v_f64 (0), ax);
#endif
      v_f64_t x2 = ax * ax;
      v_f64_t z2 = x2 * x2;
      v_f64_t z4 = z2 * z2;
      v_f64_t z8 = z4 * z4;
      v_f64_t p = ESTRIN_17 (x2, z2, z4, z8, z8 * z8, C);
      option_2 = v_fma_f64 (p, x2 * ax, ax);
#if WANT_SIMD_EXCEPT
      option_2 = v_sel_f64 (tiny, x, option_2);
#endif
    }

  /* Choose the right option for each lane.  */
  v_f64_t y = v_sel_f64 (gt1, option_1, option_2);
  /* Copy sign.  */
  y = v_as_f64_u64 (v_bsl_u64 (AbsMask, v_as_u64_f64 (y), ix));

  if (unlikely (v_any_u64 (special)))
    return special_case (x, y, special);
  return y;
}
VPCS_ALIAS

PL_SIG (V, D, 1, asinh, -10.0, 10.0)
PL_TEST_ULP (V_NAME (asinh), 2.80)
PL_TEST_EXPECT_FENV (V_NAME (asinh), WANT_SIMD_EXCEPT)
/* Test vector asinh 3 times, with control lane < 1, > 1 and special.
   Ensures the v_sel is choosing the right option in all cases.  */
#define V_ASINH_INTERVAL(lo, hi, n)                                            \
  PL_TEST_INTERVAL_C (V_NAME (asinh), lo, hi, n, 0.5)                          \
  PL_TEST_INTERVAL_C (V_NAME (asinh), lo, hi, n, 2)                            \
  PL_TEST_INTERVAL_C (V_NAME (asinh), lo, hi, n, 0x1p600)
V_ASINH_INTERVAL (0, 0x1p-26, 50000)
V_ASINH_INTERVAL (0x1p-26, 1, 50000)
V_ASINH_INTERVAL (1, 0x1p511, 50000)
V_ASINH_INTERVAL (0x1p511, inf, 40000)
V_ASINH_INTERVAL (-0, -0x1p-26, 50000)
V_ASINH_INTERVAL (-0x1p-26, -1, 50000)
V_ASINH_INTERVAL (-1, -0x1p511, 50000)
V_ASINH_INTERVAL (-0x1p511, -inf, 40000)
#endif
