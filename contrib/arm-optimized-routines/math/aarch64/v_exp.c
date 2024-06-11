/*
 * Double-precision vector e^x function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "v_math.h"

#define N (1 << V_EXP_TABLE_BITS)
#define IndexMask (N - 1)

const static volatile struct
{
  float64x2_t poly[3];
  float64x2_t inv_ln2, ln2_hi, ln2_lo, shift;
#if !WANT_SIMD_EXCEPT
  float64x2_t special_bound, scale_thresh;
#endif
} data = {
  /* maxerr: 1.88 +0.5 ulp
     rel error: 1.4337*2^-53
     abs error: 1.4299*2^-53 in [ -ln2/256, ln2/256 ].  */
  .poly = { V2 (0x1.ffffffffffd43p-2), V2 (0x1.55555c75adbb2p-3),
	    V2 (0x1.55555da646206p-5) },
#if !WANT_SIMD_EXCEPT
  .scale_thresh = V2 (163840.0), /* 1280.0 * N.  */
  .special_bound = V2 (704.0),
#endif
  .inv_ln2 = V2 (0x1.71547652b82fep7), /* N/ln2.  */
  .ln2_hi = V2 (0x1.62e42fefa39efp-8), /* ln2/N.  */
  .ln2_lo = V2 (0x1.abc9e3b39803f3p-63),
  .shift = V2 (0x1.8p+52)
};

#define C(i) data.poly[i]
#define Tab __v_exp_data

#if WANT_SIMD_EXCEPT

# define TinyBound v_u64 (0x2000000000000000) /* asuint64 (0x1p-511).  */
# define BigBound v_u64 (0x4080000000000000) /* asuint64 (0x1p9).  */
# define SpecialBound v_u64 (0x2080000000000000) /* BigBound - TinyBound.  */

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, uint64x2_t cmp)
{
  /* If fenv exceptions are to be triggered correctly, fall back to the scalar
     routine to special lanes.  */
  return v_call_f64 (exp, x, y, cmp);
}

#else

# define SpecialOffset v_u64 (0x6000000000000000) /* 0x1p513.  */
/* SpecialBias1 + SpecialBias1 = asuint(1.0).  */
# define SpecialBias1 v_u64 (0x7000000000000000) /* 0x1p769.  */
# define SpecialBias2 v_u64 (0x3010000000000000) /* 0x1p-254.  */

static inline float64x2_t VPCS_ATTR
special_case (float64x2_t s, float64x2_t y, float64x2_t n)
{
  /* 2^(n/N) may overflow, break it up into s1*s2.  */
  uint64x2_t b = vandq_u64 (vcltzq_f64 (n), SpecialOffset);
  float64x2_t s1 = vreinterpretq_f64_u64 (vsubq_u64 (SpecialBias1, b));
  float64x2_t s2 = vreinterpretq_f64_u64 (
      vaddq_u64 (vsubq_u64 (vreinterpretq_u64_f64 (s), SpecialBias2), b));
  uint64x2_t cmp = vcagtq_f64 (n, data.scale_thresh);
  float64x2_t r1 = vmulq_f64 (s1, s1);
  float64x2_t r0 = vmulq_f64 (vfmaq_f64 (s2, y, s2), s1);
  return vbslq_f64 (cmp, r1, r0);
}

#endif

float64x2_t VPCS_ATTR V_NAME_D1 (exp) (float64x2_t x)
{
  float64x2_t n, r, r2, s, y, z;
  uint64x2_t cmp, u, e;

#if WANT_SIMD_EXCEPT
  /* If any lanes are special, mask them with 1 and retain a copy of x to allow
     special_case to fix special lanes later. This is only necessary if fenv
     exceptions are to be triggered correctly.  */
  float64x2_t xm = x;
  uint64x2_t iax = vreinterpretq_u64_f64 (vabsq_f64 (x));
  cmp = vcgeq_u64 (vsubq_u64 (iax, TinyBound), SpecialBound);
  if (unlikely (v_any_u64 (cmp)))
    x = vbslq_f64 (cmp, v_f64 (1), x);
#else
  cmp = vcagtq_f64 (x, data.special_bound);
#endif

  /* n = round(x/(ln2/N)).  */
  z = vfmaq_f64 (data.shift, x, data.inv_ln2);
  u = vreinterpretq_u64_f64 (z);
  n = vsubq_f64 (z, data.shift);

  /* r = x - n*ln2/N.  */
  r = x;
  r = vfmsq_f64 (r, data.ln2_hi, n);
  r = vfmsq_f64 (r, data.ln2_lo, n);

  e = vshlq_n_u64 (u, 52 - V_EXP_TABLE_BITS);

  /* y = exp(r) - 1 ~= r + C0 r^2 + C1 r^3 + C2 r^4.  */
  r2 = vmulq_f64 (r, r);
  y = vfmaq_f64 (C (0), C (1), r);
  y = vfmaq_f64 (y, C (2), r2);
  y = vfmaq_f64 (r, y, r2);

  /* s = 2^(n/N).  */
  u = (uint64x2_t){ Tab[u[0] & IndexMask], Tab[u[1] & IndexMask] };
  s = vreinterpretq_f64_u64 (vaddq_u64 (u, e));

  if (unlikely (v_any_u64 (cmp)))
#if WANT_SIMD_EXCEPT
    return special_case (xm, vfmaq_f64 (s, y, s), cmp);
#else
    return special_case (s, y, n);
#endif

  return vfmaq_f64 (s, y, s);
}
