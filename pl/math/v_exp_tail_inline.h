/*
 * Double-precision vector e^(x+tail) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#ifndef PL_MATH_V_EXP_TAIL_INLINE_H
#define PL_MATH_V_EXP_TAIL_INLINE_H

#include "v_math.h"
#include "poly_advsimd_f64.h"

#ifndef WANT_V_EXP_TAIL_SPECIALCASE
#error                                                                         \
  "Cannot use v_exp_tail_inline.h without specifying whether you need the special case computation."
#endif

#define N (1 << V_EXP_TAIL_TABLE_BITS)

static const struct data
{
  float64x2_t poly[4];
#if WANT_V_EXP_TAIL_SPECIALCASE
  float64x2_t big_bound, huge_bound;
#endif
  float64x2_t shift, invln2, ln2_hi, ln2_lo;
} data = {
#if WANT_V_EXP_TAIL_SPECIALCASE
  .big_bound = V2 (704.0),
  .huge_bound = V2 (1280.0 * N),
#endif
  .shift = V2 (0x1.8p52),
  .invln2 = V2 (0x1.71547652b82fep8),  /* N/ln2.  */
  .ln2_hi = V2 (0x1.62e42fefa39efp-9), /* ln2/N.  */
  .ln2_lo = V2 (0x1.abc9e3b39803f3p-64),
  .poly = { V2 (1.0), V2 (0x1.fffffffffffd4p-2), V2 (0x1.5555571d6b68cp-3),
	    V2 (0x1.5555576a59599p-5) },
};

static inline uint64x2_t
lookup_sbits (uint64x2_t i)
{
  return (uint64x2_t){__v_exp_tail_data[i[0]], __v_exp_tail_data[i[1]]};
}

#if WANT_V_EXP_TAIL_SPECIALCASE
#define SpecialOffset v_u64 (0x6000000000000000) /* 0x1p513.  */
/* The following 2 bias when combined form the exponent bias:
   SpecialBias1 - SpecialBias2 = asuint64(1.0).  */
#define SpecialBias1 v_u64 (0x7000000000000000) /* 0x1p769.  */
#define SpecialBias2 v_u64 (0x3010000000000000) /* 0x1p-254.  */
static float64x2_t VPCS_ATTR
v_exp_tail_special_case (float64x2_t s, float64x2_t y, float64x2_t n,
			 const struct data *d)
{
  /* 2^(n/N) may overflow, break it up into s1*s2.  */
  uint64x2_t b = vandq_u64 (vclezq_f64 (n), SpecialOffset);
  float64x2_t s1 = vreinterpretq_f64_u64 (vsubq_u64 (SpecialBias1, b));
  float64x2_t s2 = vreinterpretq_f64_u64 (
    vaddq_u64 (vsubq_u64 (vreinterpretq_u64_f64 (s), SpecialBias2), b));
  uint64x2_t oflow = vcagtq_f64 (n, d->huge_bound);
  float64x2_t r0 = vmulq_f64 (vfmaq_f64 (s2, y, s2), s1);
  float64x2_t r1 = vmulq_f64 (s1, s1);
  return vbslq_f64 (oflow, r1, r0);
}
#endif

static inline float64x2_t VPCS_ATTR
v_exp_tail_inline (float64x2_t x, float64x2_t xtail)
{
  const struct data *d = ptr_barrier (&data);
#if WANT_V_EXP_TAIL_SPECIALCASE
  uint64x2_t special = vcgtq_f64 (vabsq_f64 (x), d->big_bound);
#endif
  /* n = round(x/(ln2/N)).  */
  float64x2_t z = vfmaq_f64 (d->shift, x, d->invln2);
  uint64x2_t u = vreinterpretq_u64_f64 (z);
  float64x2_t n = vsubq_f64 (z, d->shift);

  /* r = x - n*ln2/N.  */
  float64x2_t r = x;
  r = vfmsq_f64 (r, d->ln2_hi, n);
  r = vfmsq_f64 (r, d->ln2_lo, n);

  uint64x2_t e = vshlq_n_u64 (u, 52 - V_EXP_TAIL_TABLE_BITS);
  uint64x2_t i = vandq_u64 (u, v_u64 (N - 1));

  /* y = tail + exp(r) - 1 ~= r + C1 r^2 + C2 r^3 + C3 r^4, using Horner.  */
  float64x2_t y = v_horner_3_f64 (r, d->poly);
  y = vfmaq_f64 (xtail, y, r);

  /* s = 2^(n/N).  */
  u = lookup_sbits (i);
  float64x2_t s = vreinterpretq_f64_u64 (vaddq_u64 (u, e));

#if WANT_V_EXP_TAIL_SPECIALCASE
  if (unlikely (v_any_u64 (special)))
    return v_exp_tail_special_case (s, y, n, d);
#endif
  return vfmaq_f64 (s, y, s);
}
#endif // PL_MATH_V_EXP_TAIL_INLINE_H
