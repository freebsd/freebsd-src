/*
 * Double-precision vector erf(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  float64x2_t third;
  float64x2_t tenth, two_over_five, two_over_fifteen;
  float64x2_t two_over_nine, two_over_fortyfive;
  float64x2_t max, shift;
#if WANT_SIMD_EXCEPT
  float64x2_t tiny_bound, huge_bound, scale_minus_one;
#endif
} data = {
  .third = V2 (0x1.5555555555556p-2), /* used to compute 2/3 and 1/6 too.  */
  .two_over_fifteen = V2 (0x1.1111111111111p-3),
  .tenth = V2 (-0x1.999999999999ap-4),
  .two_over_five = V2 (-0x1.999999999999ap-2),
  .two_over_nine = V2 (-0x1.c71c71c71c71cp-3),
  .two_over_fortyfive = V2 (0x1.6c16c16c16c17p-5),
  .max = V2 (5.9921875), /* 6 - 1/128.  */
  .shift = V2 (0x1p45),
#if WANT_SIMD_EXCEPT
  .huge_bound = V2 (0x1p205),
  .tiny_bound = V2 (0x1p-226),
  .scale_minus_one = V2 (0x1.06eba8214db69p-3), /* 2/sqrt(pi) - 1.0.  */
#endif
};

#define AbsMask 0x7fffffffffffffff

struct entry
{
  float64x2_t erf;
  float64x2_t scale;
};

static inline struct entry
lookup (uint64x2_t i)
{
  struct entry e;
  float64x2_t e1 = vld1q_f64 ((float64_t *) (__erf_data.tab + i[0])),
	      e2 = vld1q_f64 ((float64_t *) (__erf_data.tab + i[1]));
  e.erf = vuzp1q_f64 (e1, e2);
  e.scale = vuzp2q_f64 (e1, e2);
  return e;
}

/* Double-precision implementation of vector erf(x).
   Approximation based on series expansion near x rounded to
   nearest multiple of 1/128.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erf(x) ~ erf(r) + scale * d * [
       + 1
       - r d
       + 1/3 (2 r^2 - 1) d^2
       - 1/6 (r (2 r^2 - 3)) d^3
       + 1/30 (4 r^4 - 12 r^2 + 3) d^4
       - 1/90 (4 r^4 - 20 r^2 + 15) d^5
     ]

   Maximum measure error: 2.29 ULP
   V_NAME_D1 (erf)(-0x1.00003c924e5d1p-8) got -0x1.20dd59132ebadp-8
					 want -0x1.20dd59132ebafp-8.  */
float64x2_t VPCS_ATTR V_NAME_D1 (erf) (float64x2_t x)
{
  const struct data *dat = ptr_barrier (&data);

  float64x2_t a = vabsq_f64 (x);
  /* Reciprocal conditions that do not catch NaNs so they can be used in BSLs
     to return expected results.  */
  uint64x2_t a_le_max = vcleq_f64 (a, dat->max);
  uint64x2_t a_gt_max = vcgtq_f64 (a, dat->max);

#if WANT_SIMD_EXCEPT
  /* |x| huge or tiny.  */
  uint64x2_t cmp1 = vcgtq_f64 (a, dat->huge_bound);
  uint64x2_t cmp2 = vcltq_f64 (a, dat->tiny_bound);
  uint64x2_t cmp = vorrq_u64 (cmp1, cmp2);
  /* If any lanes are special, mask them with 1 for small x or 8 for large
     values and retain a copy of a to allow special case handler to fix special
     lanes later. This is only necessary if fenv exceptions are to be triggered
     correctly.  */
  if (unlikely (v_any_u64 (cmp)))
    {
      a = vbslq_f64 (cmp1, v_f64 (8.0), a);
      a = vbslq_f64 (cmp2, v_f64 (1.0), a);
    }
#endif

  /* Set r to multiple of 1/128 nearest to |x|.  */
  float64x2_t shift = dat->shift;
  float64x2_t z = vaddq_f64 (a, shift);

  /* Lookup erf(r) and scale(r) in table, without shortcut for small values,
     but with saturated indices for large values and NaNs in order to avoid
     segfault.  */
  uint64x2_t i
      = vsubq_u64 (vreinterpretq_u64_f64 (z), vreinterpretq_u64_f64 (shift));
  i = vbslq_u64 (a_le_max, i, v_u64 (768));
  struct entry e = lookup (i);

  float64x2_t r = vsubq_f64 (z, shift);

  /* erf(x) ~ erf(r) + scale * d * poly (r, d).  */
  float64x2_t d = vsubq_f64 (a, r);
  float64x2_t d2 = vmulq_f64 (d, d);
  float64x2_t r2 = vmulq_f64 (r, r);

  /* poly (d, r) = 1 + p1(r) * d + p2(r) * d^2 + ... + p5(r) * d^5.  */
  float64x2_t p1 = r;
  float64x2_t p2
      = vfmsq_f64 (dat->third, r2, vaddq_f64 (dat->third, dat->third));
  float64x2_t p3 = vmulq_f64 (r, vfmaq_f64 (v_f64 (-0.5), r2, dat->third));
  float64x2_t p4 = vfmaq_f64 (dat->two_over_five, r2, dat->two_over_fifteen);
  p4 = vfmsq_f64 (dat->tenth, r2, p4);
  float64x2_t p5 = vfmaq_f64 (dat->two_over_nine, r2, dat->two_over_fortyfive);
  p5 = vmulq_f64 (r, vfmaq_f64 (vmulq_f64 (v_f64 (0.5), dat->third), r2, p5));

  float64x2_t p34 = vfmaq_f64 (p3, d, p4);
  float64x2_t p12 = vfmaq_f64 (p1, d, p2);
  float64x2_t y = vfmaq_f64 (p34, d2, p5);
  y = vfmaq_f64 (p12, d2, y);

  y = vfmaq_f64 (e.erf, e.scale, vfmsq_f64 (d, d2, y));

  /* Solves the |x| = inf and NaN cases.  */
  y = vbslq_f64 (a_gt_max, v_f64 (1.0), y);

  /* Copy sign.  */
  y = vbslq_f64 (v_u64 (AbsMask), y, x);

#if WANT_SIMD_EXCEPT
  if (unlikely (v_any_u64 (cmp2)))
    {
      /* Neutralise huge values of x before fixing small values.  */
      x = vbslq_f64 (cmp1, v_f64 (1.0), x);
      /* Fix tiny values that trigger spurious underflow.  */
      return vbslq_f64 (cmp2, vfmaq_f64 (x, dat->scale_minus_one, x), y);
    }
#endif
  return y;
}

PL_SIG (V, D, 1, erf, -6.0, 6.0)
PL_TEST_ULP (V_NAME_D1 (erf), 1.79)
PL_TEST_EXPECT_FENV (V_NAME_D1 (erf), WANT_SIMD_EXCEPT)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (erf), 0, 5.9921875, 40000)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (erf), 5.9921875, inf, 40000)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (erf), 0, inf, 40000)
