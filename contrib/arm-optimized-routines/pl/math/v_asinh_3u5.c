/*
 * Double-precision vector asinh(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "poly_advsimd_f64.h"
#include "pl_sig.h"
#include "pl_test.h"

#define A(i) v_f64 (__v_log_data.poly[i])
#define N (1 << V_LOG_TABLE_BITS)

const static struct data
{
  float64x2_t poly[18];
  uint64x2_t off, huge_bound, abs_mask;
  float64x2_t ln2, tiny_bound;
} data = {
  .off = V2 (0x3fe6900900000000),
  .ln2 = V2 (0x1.62e42fefa39efp-1),
  .huge_bound = V2 (0x5fe0000000000000),
  .tiny_bound = V2 (0x1p-26),
  .abs_mask = V2 (0x7fffffffffffffff),
  /* Even terms of polynomial s.t. asinh(x) is approximated by
     asinh(x) ~= x + x^3 * (C0 + C1 * x + C2 * x^2 + C3 * x^3 + ...).
     Generated using Remez, f = (asinh(sqrt(x)) - sqrt(x))/x^(3/2).  */
  .poly = { V2 (-0x1.55555555554a7p-3), V2 (0x1.3333333326c7p-4),
	    V2 (-0x1.6db6db68332e6p-5), V2 (0x1.f1c71b26fb40dp-6),
	    V2 (-0x1.6e8b8b654a621p-6), V2 (0x1.1c4daa9e67871p-6),
	    V2 (-0x1.c9871d10885afp-7), V2 (0x1.7a16e8d9d2ecfp-7),
	    V2 (-0x1.3ddca533e9f54p-7), V2 (0x1.0becef748dafcp-7),
	    V2 (-0x1.b90c7099dd397p-8), V2 (0x1.541f2bb1ffe51p-8),
	    V2 (-0x1.d217026a669ecp-9), V2 (0x1.0b5c7977aaf7p-9),
	    V2 (-0x1.e0f37daef9127p-11), V2 (0x1.388b5fe542a6p-12),
	    V2 (-0x1.021a48685e287p-14), V2 (0x1.93d4ba83d34dap-18) },
};

static float64x2_t NOINLINE VPCS_ATTR
special_case (float64x2_t x, float64x2_t y, uint64x2_t special)
{
  return v_call_f64 (asinh, x, y, special);
}

struct entry
{
  float64x2_t invc;
  float64x2_t logc;
};

static inline struct entry
lookup (uint64x2_t i)
{
  float64x2_t e0 = vld1q_f64 (
      &__v_log_data.table[(i[0] >> (52 - V_LOG_TABLE_BITS)) & (N - 1)].invc);
  float64x2_t e1 = vld1q_f64 (
      &__v_log_data.table[(i[1] >> (52 - V_LOG_TABLE_BITS)) & (N - 1)].invc);
  return (struct entry){ vuzp1q_f64 (e0, e1), vuzp2q_f64 (e0, e1) };
}

static inline float64x2_t
log_inline (float64x2_t x, const struct data *d)
{
  /* Double-precision vector log, copied from ordinary vector log with some
     cosmetic modification and special-cases removed.  */
  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  uint64x2_t tmp = vsubq_u64 (ix, d->off);
  int64x2_t k = vshrq_n_s64 (vreinterpretq_s64_u64 (tmp), 52);
  uint64x2_t iz
      = vsubq_u64 (ix, vandq_u64 (tmp, vdupq_n_u64 (0xfffULL << 52)));
  float64x2_t z = vreinterpretq_f64_u64 (iz);
  struct entry e = lookup (tmp);
  float64x2_t r = vfmaq_f64 (v_f64 (-1.0), z, e.invc);
  float64x2_t kd = vcvtq_f64_s64 (k);
  float64x2_t hi = vfmaq_f64 (vaddq_f64 (e.logc, r), kd, d->ln2);
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t y = vfmaq_f64 (A (2), A (3), r);
  float64x2_t p = vfmaq_f64 (A (0), A (1), r);
  y = vfmaq_f64 (y, A (4), r2);
  y = vfmaq_f64 (p, y, r2);
  y = vfmaq_f64 (hi, y, r2);
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
VPCS_ATTR float64x2_t V_NAME_D1 (asinh) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  float64x2_t ax = vabsq_f64 (x);
  uint64x2_t iax = vreinterpretq_u64_f64 (ax);

  uint64x2_t gt1 = vcgeq_f64 (ax, v_f64 (1));
  uint64x2_t special = vcgeq_u64 (iax, d->huge_bound);

#if WANT_SIMD_EXCEPT
  uint64x2_t tiny = vcltq_f64 (ax, d->tiny_bound);
  special = vorrq_u64 (special, tiny);
#endif

  /* Option 1: |x| >= 1.
     Compute asinh(x) according by asinh(x) = log(x + sqrt(x^2 + 1)).
     If WANT_SIMD_EXCEPT is enabled, sidestep special values, which will
     overflow, by setting special lanes to 1. These will be fixed later.  */
  float64x2_t option_1 = v_f64 (0);
  if (likely (v_any_u64 (gt1)))
    {
#if WANT_SIMD_EXCEPT
      float64x2_t xm = v_zerofy_f64 (ax, special);
#else
      float64x2_t xm = ax;
#endif
      option_1 = log_inline (
	  vaddq_f64 (xm, vsqrtq_f64 (vfmaq_f64 (v_f64 (1), xm, xm))), d);
    }

  /* Option 2: |x| < 1.
     Compute asinh(x) using a polynomial.
     If WANT_SIMD_EXCEPT is enabled, sidestep special lanes, which will
     overflow, and tiny lanes, which will underflow, by setting them to 0. They
     will be fixed later, either by selecting x or falling back to the scalar
     special-case. The largest observed error in this region is 1.47 ULPs:
     __v_asinh(0x1.fdfcd00cc1e6ap-1) got 0x1.c1d6bf874019bp-1
				    want 0x1.c1d6bf874019cp-1.  */
  float64x2_t option_2 = v_f64 (0);
  if (likely (v_any_u64 (vceqzq_u64 (gt1))))
    {
#if WANT_SIMD_EXCEPT
      ax = v_zerofy_f64 (ax, vorrq_u64 (tiny, gt1));
#endif
      float64x2_t x2 = vmulq_f64 (ax, ax), x3 = vmulq_f64 (ax, x2),
		  z2 = vmulq_f64 (x2, x2), z4 = vmulq_f64 (z2, z2),
		  z8 = vmulq_f64 (z4, z4), z16 = vmulq_f64 (z8, z8);
      float64x2_t p = v_estrin_17_f64 (x2, z2, z4, z8, z16, d->poly);
      option_2 = vfmaq_f64 (ax, p, x3);
#if WANT_SIMD_EXCEPT
      option_2 = vbslq_f64 (tiny, x, option_2);
#endif
    }

  /* Choose the right option for each lane.  */
  float64x2_t y = vbslq_f64 (gt1, option_1, option_2);
  /* Copy sign.  */
  y = vbslq_f64 (d->abs_mask, y, x);

  if (unlikely (v_any_u64 (special)))
    return special_case (x, y, special);
  return y;
}

PL_SIG (V, D, 1, asinh, -10.0, 10.0)
PL_TEST_ULP (V_NAME_D1 (asinh), 2.80)
PL_TEST_EXPECT_FENV (V_NAME_D1 (asinh), WANT_SIMD_EXCEPT)
/* Test vector asinh 3 times, with control lane < 1, > 1 and special.
   Ensures the v_sel is choosing the right option in all cases.  */
#define V_ASINH_INTERVAL(lo, hi, n)                                           \
  PL_TEST_SYM_INTERVAL_C (V_NAME_D1 (asinh), lo, hi, n, 0.5)                  \
  PL_TEST_SYM_INTERVAL_C (V_NAME_D1 (asinh), lo, hi, n, 2)                    \
  PL_TEST_SYM_INTERVAL_C (V_NAME_D1 (asinh), lo, hi, n, 0x1p600)
V_ASINH_INTERVAL (0, 0x1p-26, 50000)
V_ASINH_INTERVAL (0x1p-26, 1, 50000)
V_ASINH_INTERVAL (1, 0x1p511, 50000)
V_ASINH_INTERVAL (0x1p511, inf, 40000)
