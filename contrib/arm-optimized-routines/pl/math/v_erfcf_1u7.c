/*
 * Single-precision vector erfc(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"

static const struct data
{
  uint32x4_t offset, table_scale;
  float32x4_t max, shift;
  float32x4_t coeffs, third, two_over_five, tenth;
#if WANT_SIMD_EXCEPT
  float32x4_t uflow_bound;
#endif

} data = {
  /* Set an offset so the range of the index used for lookup is 644, and it can
     be clamped using a saturated add.  */
  .offset = V4 (0xb7fffd7b),	       /* 0xffffffff - asuint(shift) - 644.  */
  .table_scale = V4 (0x28000000 << 1), /* asuint (2^-47) << 1.  */
  .max = V4 (10.0625f),		       /* 10 + 1/16 = 644/64.  */
  .shift = V4 (0x1p17f),
  /* Store 1/3, 2/3 and 2/15 in a single register for use with indexed muls and
     fmas.  */
  .coeffs = (float32x4_t){ 0x1.555556p-2f, 0x1.555556p-1f, 0x1.111112p-3f, 0 },
  .third = V4 (0x1.555556p-2f),
  .two_over_five = V4 (-0x1.99999ap-2f),
  .tenth = V4 (-0x1.99999ap-4f),
#if WANT_SIMD_EXCEPT
  .uflow_bound = V4 (0x1.2639cp+3f),
#endif
};

#define TinyBound 0x41000000 /* 0x1p-62f << 1.  */
#define Thres 0xbe000000     /* asuint(infinity) << 1 - TinyBound.  */
#define Off 0xfffffd7b	     /* 0xffffffff - 644.  */

struct entry
{
  float32x4_t erfc;
  float32x4_t scale;
};

static inline struct entry
lookup (uint32x4_t i)
{
  struct entry e;
  float64_t t0 = *((float64_t *) (__erfcf_data.tab - Off + i[0]));
  float64_t t1 = *((float64_t *) (__erfcf_data.tab - Off + i[1]));
  float64_t t2 = *((float64_t *) (__erfcf_data.tab - Off + i[2]));
  float64_t t3 = *((float64_t *) (__erfcf_data.tab - Off + i[3]));
  float32x4_t e1 = vreinterpretq_f32_f64 ((float64x2_t){ t0, t1 });
  float32x4_t e2 = vreinterpretq_f32_f64 ((float64x2_t){ t2, t3 });
  e.erfc = vuzp1q_f32 (e1, e2);
  e.scale = vuzp2q_f32 (e1, e2);
  return e;
}

#if WANT_SIMD_EXCEPT
static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, uint32x4_t cmp)
{
  return v_call_f32 (erfcf, x, y, cmp);
}
#endif

/* Optimized single-precision vector erfcf(x).
   Approximation based on series expansion near x rounded to
   nearest multiple of 1/64.
   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erfc(x) ~ erfc(r) - scale * d * poly(r, d), with

   poly(r, d) = 1 - r d + (2/3 r^2 - 1/3) d^2 - r (1/3 r^2 - 1/2) d^3
		+ (2/15 r^4 - 2/5 r^2 + 1/10) d^4

   Values of erfc(r) and scale are read from lookup tables. Stored values
   are scaled to avoid hitting the subnormal range.

   Note that for x < 0, erfc(x) = 2.0 - erfc(-x).
   Maximum error: 1.63 ULP (~1.0 ULP for x < 0.0).
   _ZGVnN4v_erfcf(0x1.1dbf7ap+3) got 0x1.f51212p-120
				want 0x1.f51216p-120.  */
VPCS_ATTR
float32x4_t V_NAME_F1 (erfc) (float32x4_t x)
{
  const struct data *dat = ptr_barrier (&data);

#if WANT_SIMD_EXCEPT
  /* |x| < 2^-62. Avoid fabs by left-shifting by 1.  */
  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t cmp = vcltq_u32 (vaddq_u32 (ix, ix), v_u32 (TinyBound));
  /* x >= ~9.19 (into subnormal case and uflow case). Comparison is done in
     integer domain to avoid raising exceptions in presence of nans.  */
  uint32x4_t uflow = vcgeq_s32 (vreinterpretq_s32_f32 (x),
				vreinterpretq_s32_f32 (dat->uflow_bound));
  cmp = vorrq_u32 (cmp, uflow);
  float32x4_t xm = x;
  /* If any lanes are special, mask them with 0 and retain a copy of x to allow
     special case handler to fix special lanes later. This is only necessary if
     fenv exceptions are to be triggered correctly.  */
  if (unlikely (v_any_u32 (cmp)))
    x = v_zerofy_f32 (x, cmp);
#endif

  float32x4_t a = vabsq_f32 (x);
  a = vminq_f32 (a, dat->max);

  /* Lookup erfc(r) and scale(r) in tables, e.g. set erfc(r) to 0 and scale to
     2/sqrt(pi), when x reduced to r = 0.  */
  float32x4_t shift = dat->shift;
  float32x4_t z = vaddq_f32 (a, shift);

  /* Clamp index to a range of 644. A naive approach would use a subtract and
     min. Instead we offset the table address and the index, then use a
     saturating add.  */
  uint32x4_t i = vqaddq_u32 (vreinterpretq_u32_f32 (z), dat->offset);

  struct entry e = lookup (i);

  /* erfc(x) ~ erfc(r) - scale * d * poly(r, d).  */
  float32x4_t r = vsubq_f32 (z, shift);
  float32x4_t d = vsubq_f32 (a, r);
  float32x4_t d2 = vmulq_f32 (d, d);
  float32x4_t r2 = vmulq_f32 (r, r);

  float32x4_t p1 = r;
  float32x4_t p2 = vfmsq_laneq_f32 (dat->third, r2, dat->coeffs, 1);
  float32x4_t p3
      = vmulq_f32 (r, vfmaq_laneq_f32 (v_f32 (-0.5), r2, dat->coeffs, 0));
  float32x4_t p4 = vfmaq_laneq_f32 (dat->two_over_five, r2, dat->coeffs, 2);
  p4 = vfmsq_f32 (dat->tenth, r2, p4);

  float32x4_t y = vfmaq_f32 (p3, d, p4);
  y = vfmaq_f32 (p2, d, y);
  y = vfmaq_f32 (p1, d, y);
  y = vfmsq_f32 (e.erfc, e.scale, vfmsq_f32 (d, d2, y));

  /* Offset equals 2.0f if sign, else 0.0f.  */
  uint32x4_t sign = vshrq_n_u32 (vreinterpretq_u32_f32 (x), 31);
  float32x4_t off = vreinterpretq_f32_u32 (vshlq_n_u32 (sign, 30));
  /* Copy sign and scale back in a single fma. Since the bit patterns do not
     overlap, then logical or and addition are equivalent here.  */
  float32x4_t fac = vreinterpretq_f32_u32 (
      vsraq_n_u32 (vshlq_n_u32 (sign, 31), dat->table_scale, 1));

#if WANT_SIMD_EXCEPT
  if (unlikely (v_any_u32 (cmp)))
    return special_case (xm, vfmaq_f32 (off, fac, y), cmp);
#endif

  return vfmaq_f32 (off, fac, y);
}

PL_SIG (V, F, 1, erfc, -4.0, 10.0)
PL_TEST_ULP (V_NAME_F1 (erfc), 1.14)
PL_TEST_SYM_INTERVAL (V_NAME_F1 (erfc), 0, 0x1p-26, 40000)
PL_TEST_INTERVAL (V_NAME_F1 (erfc), 0x1p-26, 10.0625, 40000)
PL_TEST_INTERVAL (V_NAME_F1 (erfc), -0x1p-26, -4.0, 40000)
PL_TEST_INTERVAL (V_NAME_F1 (erfc), 10.0625, inf, 40000)
PL_TEST_INTERVAL (V_NAME_F1 (erfc), -4.0, -inf, 40000)
