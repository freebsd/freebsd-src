/*
 * Double-precision vector 10^x function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"

/* Value of |x| above which scale overflows without special treatment.  */
#define SpecialBound 306.0 /* floor (log10 (2^1023)) - 1.  */
/* Value of n above which scale overflows even with special treatment.  */
#define ScaleBound 163840.0 /* 1280.0 * N.  */

const static struct data
{
  float64x2_t poly[4];
  float64x2_t log10_2, log2_10_hi, log2_10_lo, shift;
#if !WANT_SIMD_EXCEPT
  float64x2_t special_bound, scale_thresh;
#endif
} data = {
  /* Coefficients generated using Remez algorithm.
     rel error: 0x1.5ddf8f28p-54
     abs error: 0x1.5ed266c8p-54 in [ -log10(2)/256, log10(2)/256 ]
     maxerr: 1.14432 +0.5 ulp.  */
  .poly = { V2 (0x1.26bb1bbb5524p1), V2 (0x1.53524c73cecdap1),
	    V2 (0x1.047060efb781cp1), V2 (0x1.2bd76040f0d16p0) },
  .log10_2 = V2 (0x1.a934f0979a371p8),	   /* N/log2(10).  */
  .log2_10_hi = V2 (0x1.34413509f79ffp-9), /* log2(10)/N.  */
  .log2_10_lo = V2 (-0x1.9dc1da994fd21p-66),
  .shift = V2 (0x1.8p+52),
#if !WANT_SIMD_EXCEPT
  .scale_thresh = V2 (ScaleBound),
  .special_bound = V2 (SpecialBound),
#endif
};

#define N (1 << V_EXP_TABLE_BITS)
#define IndexMask v_u64 (N - 1)

#if WANT_SIMD_EXCEPT

# define TinyBound v_u64 (0x2000000000000000) /* asuint64 (0x1p-511).  */
# define BigBound v_u64 (0x4070000000000000)  /* asuint64 (0x1p8).  */
# define Thres v_u64 (0x2070000000000000)     /* BigBound - TinyBound.  */

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, uint64x2_t cmp)
{
  /* If fenv exceptions are to be triggered correctly, fall back to the scalar
     routine for special lanes.  */
  return v_call_f64 (exp10, x, y, cmp);
}

#else

# define SpecialOffset v_u64 (0x6000000000000000) /* 0x1p513.  */
/* SpecialBias1 + SpecialBias1 = asuint(1.0).  */
# define SpecialBias1 v_u64 (0x7000000000000000)  /* 0x1p769.  */
# define SpecialBias2 v_u64 (0x3010000000000000)  /* 0x1p-254.  */

static inline float64x2_t VPCS_ATTR
special_case (float64x2_t s, float64x2_t y, float64x2_t n,
	      const struct data *d)
{
  /* 2^(n/N) may overflow, break it up into s1*s2.  */
  uint64x2_t b = vandq_u64 (vcltzq_f64 (n), SpecialOffset);
  float64x2_t s1 = vreinterpretq_f64_u64 (vsubq_u64 (SpecialBias1, b));
  float64x2_t s2 = vreinterpretq_f64_u64 (
      vaddq_u64 (vsubq_u64 (vreinterpretq_u64_f64 (s), SpecialBias2), b));
  uint64x2_t cmp = vcagtq_f64 (n, d->scale_thresh);
  float64x2_t r1 = vmulq_f64 (s1, s1);
  float64x2_t r0 = vmulq_f64 (vfmaq_f64 (s2, y, s2), s1);
  return vbslq_f64 (cmp, r1, r0);
}

#endif

/* Fast vector implementation of exp10.
   Maximum measured error is 1.64 ulp.
   _ZGVnN2v_exp10(0x1.ccd1c9d82cc8cp+0) got 0x1.f8dab6d7fed0cp+5
				       want 0x1.f8dab6d7fed0ap+5.  */
float64x2_t VPCS_ATTR V_NAME_D1 (exp10) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint64x2_t cmp;
#if WANT_SIMD_EXCEPT
  /* If any lanes are special, mask them with 1 and retain a copy of x to allow
     special_case to fix special lanes later. This is only necessary if fenv
     exceptions are to be triggered correctly.  */
  float64x2_t xm = x;
  uint64x2_t iax = vreinterpretq_u64_f64 (vabsq_f64 (x));
  cmp = vcgeq_u64 (vsubq_u64 (iax, TinyBound), Thres);
  if (unlikely (v_any_u64 (cmp)))
    x = vbslq_f64 (cmp, v_f64 (1), x);
#else
  cmp = vcageq_f64 (x, d->special_bound);
#endif

  /* n = round(x/(log10(2)/N)).  */
  float64x2_t z = vfmaq_f64 (d->shift, x, d->log10_2);
  uint64x2_t u = vreinterpretq_u64_f64 (z);
  float64x2_t n = vsubq_f64 (z, d->shift);

  /* r = x - n*log10(2)/N.  */
  float64x2_t r = x;
  r = vfmsq_f64 (r, d->log2_10_hi, n);
  r = vfmsq_f64 (r, d->log2_10_lo, n);

  uint64x2_t e = vshlq_n_u64 (u, 52 - V_EXP_TABLE_BITS);
  uint64x2_t i = vandq_u64 (u, IndexMask);

  /* y = exp10(r) - 1 ~= C0 r + C1 r^2 + C2 r^3 + C3 r^4.  */
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t p = vfmaq_f64 (d->poly[0], r, d->poly[1]);
  float64x2_t y = vfmaq_f64 (d->poly[2], r, d->poly[3]);
  p = vfmaq_f64 (p, y, r2);
  y = vmulq_f64 (r, p);

  /* s = 2^(n/N).  */
  u = v_lookup_u64 (__v_exp_data, i);
  float64x2_t s = vreinterpretq_f64_u64 (vaddq_u64 (u, e));

  if (unlikely (v_any_u64 (cmp)))
#if WANT_SIMD_EXCEPT
    return special_case (xm, vfmaq_f64 (s, y, s), cmp);
#else
    return special_case (s, y, n, d);
#endif

  return vfmaq_f64 (s, y, s);
}

PL_SIG (S, D, 1, exp10, -9.9, 9.9)
PL_SIG (V, D, 1, exp10, -9.9, 9.9)
PL_TEST_ULP (V_NAME_D1 (exp10), 1.15)
PL_TEST_EXPECT_FENV (V_NAME_D1 (exp10), WANT_SIMD_EXCEPT)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (exp10), 0, SpecialBound, 5000)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (exp10), SpecialBound, ScaleBound, 5000)
PL_TEST_SYM_INTERVAL (V_NAME_D1 (exp10), ScaleBound, inf, 10000)
