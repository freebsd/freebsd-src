/*
 * Double-precision vector erfc(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  uint64x2_t offset, table_scale;
  float64x2_t max, shift;
  float64x2_t p20, p40, p41, p51;
  double p42, p52;
  double qr5[2], qr6[2], qr7[2], qr8[2], qr9[2];
#if WANT_SIMD_EXCEPT
  float64x2_t uflow_bound;
#endif
} data = {
  /* Set an offset so the range of the index used for lookup is 3487, and it
     can be clamped using a saturated add on an offset index.
     Index offset is 0xffffffffffffffff - asuint64(shift) - 3487.  */
  .offset = V2 (0xbd3ffffffffff260),
  .table_scale = V2 (0x37f0000000000000 << 1), /* asuint64 (2^-128) << 1.  */
  .max = V2 (0x1.b3ep+4),		       /* 3487/128.  */
  .shift = V2 (0x1p45),
  .p20 = V2 (0x1.5555555555555p-2),  /* 1/3, used to compute 2/3 and 1/6.  */
  .p40 = V2 (-0x1.999999999999ap-4), /* 1/10.  */
  .p41 = V2 (-0x1.999999999999ap-2), /* 2/5.  */
  .p42 = 0x1.1111111111111p-3,	     /* 2/15.  */
  .p51 = V2 (-0x1.c71c71c71c71cp-3), /* 2/9.  */
  .p52 = 0x1.6c16c16c16c17p-5,	     /* 2/45.  */
  /* Qi = (i+1) / i, Ri = -2 * i / ((i+1)*(i+2)), for i = 5, ..., 9.  */
  .qr5 = { 0x1.3333333333333p0, -0x1.e79e79e79e79ep-3 },
  .qr6 = { 0x1.2aaaaaaaaaaabp0, -0x1.b6db6db6db6dbp-3 },
  .qr7 = { 0x1.2492492492492p0, -0x1.8e38e38e38e39p-3 },
  .qr8 = { 0x1.2p0, -0x1.6c16c16c16c17p-3 },
  .qr9 = { 0x1.1c71c71c71c72p0, -0x1.4f2094f2094f2p-3 },
#if WANT_SIMD_EXCEPT
  .uflow_bound = V2 (0x1.a8b12fc6e4892p+4),
#endif
};

#define TinyBound 0x4000000000000000 /* 0x1p-511 << 1.  */
#define Off 0xfffffffffffff260	     /* 0xffffffffffffffff - 3487.  */

struct entry
{
  float64x2_t erfc;
  float64x2_t scale;
};

static inline struct entry
lookup (uint64x2_t i)
{
  struct entry e;
  float64x2_t e1
      = vld1q_f64 (&__v_erfc_data.tab[vgetq_lane_u64 (i, 0) - Off].erfc);
  float64x2_t e2
      = vld1q_f64 (&__v_erfc_data.tab[vgetq_lane_u64 (i, 1) - Off].erfc);
  e.erfc = vuzp1q_f64 (e1, e2);
  e.scale = vuzp2q_f64 (e1, e2);
  return e;
}

#if WANT_SIMD_EXCEPT
static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, uint64x2_t cmp)
{
  return v_call_f64 (erfc, x, y, cmp);
}
#endif

/* Optimized double-precision vector erfc(x).
   Approximation based on series expansion near x rounded to
   nearest multiple of 1/128.

   Let d = x - r, and scale = 2 / sqrt(pi) * exp(-r^2). For x near r,

   erfc(x) ~ erfc(r) - scale * d * poly(r, d), with

   poly(r, d) = 1 - r d + (2/3 r^2 - 1/3) d^2 - r (1/3 r^2 - 1/2) d^3
		+ (2/15 r^4 - 2/5 r^2 + 1/10) d^4
		- r * (2/45 r^4 - 2/9 r^2 + 1/6) d^5
		+ p6(r) d^6 + ... + p10(r) d^10

   Polynomials p6(r) to p10(r) are computed using recurrence relation

   2(i+1)p_i + 2r(i+2)p_{i+1} + (i+2)(i+3)p_{i+2} = 0,
   with p0 = 1, and p1(r) = -r.

   Values of erfc(r) and scale are read from lookup tables. Stored values
   are scaled to avoid hitting the subnormal range.

   Note that for x < 0, erfc(x) = 2.0 - erfc(-x).

   Maximum measured error: 1.71 ULP
   V_NAME_D1 (erfc)(0x1.46cfe976733p+4) got 0x1.e15fcbea3e7afp-608
				       want 0x1.e15fcbea3e7adp-608.  */
VPCS_ATTR
float64x2_t V_NAME_D1 (erfc) (float64x2_t x)
{
  const struct data *dat = ptr_barrier (&data);

#if WANT_SIMD_EXCEPT
  /* |x| < 2^-511. Avoid fabs by left-shifting by 1.  */
  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  uint64x2_t cmp = vcltq_u64 (vaddq_u64 (ix, ix), v_u64 (TinyBound));
  /* x >= ~26.54 (into subnormal case and uflow case). Comparison is done in
     integer domain to avoid raising exceptions in presence of nans.  */
  uint64x2_t uflow = vcgeq_s64 (vreinterpretq_s64_f64 (x),
				vreinterpretq_s64_f64 (dat->uflow_bound));
  cmp = vorrq_u64 (cmp, uflow);
  float64x2_t xm = x;
  /* If any lanes are special, mask them with 0 and retain a copy of x to allow
     special case handler to fix special lanes later. This is only necessary if
     fenv exceptions are to be triggered correctly.  */
  if (unlikely (v_any_u64 (cmp)))
    x = v_zerofy_f64 (x, cmp);
#endif

  float64x2_t a = vabsq_f64 (x);
  a = vminq_f64 (a, dat->max);

  /* Lookup erfc(r) and scale(r) in tables, e.g. set erfc(r) to 0 and scale to
     2/sqrt(pi), when x reduced to r = 0.  */
  float64x2_t shift = dat->shift;
  float64x2_t z = vaddq_f64 (a, shift);

  /* Clamp index to a range of 3487. A naive approach would use a subtract and
     min. Instead we offset the table address and the index, then use a
     saturating add.  */
  uint64x2_t i = vqaddq_u64 (vreinterpretq_u64_f64 (z), dat->offset);

  struct entry e = lookup (i);

  /* erfc(x) ~ erfc(r) - scale * d * poly(r, d).  */
  float64x2_t r = vsubq_f64 (z, shift);
  float64x2_t d = vsubq_f64 (a, r);
  float64x2_t d2 = vmulq_f64 (d, d);
  float64x2_t r2 = vmulq_f64 (r, r);

  float64x2_t p1 = r;
  float64x2_t p2 = vfmsq_f64 (dat->p20, r2, vaddq_f64 (dat->p20, dat->p20));
  float64x2_t p3 = vmulq_f64 (r, vfmaq_f64 (v_f64 (-0.5), r2, dat->p20));
  float64x2_t p42_p52 = vld1q_f64 (&dat->p42);
  float64x2_t p4 = vfmaq_laneq_f64 (dat->p41, r2, p42_p52, 0);
  p4 = vfmsq_f64 (dat->p40, r2, p4);
  float64x2_t p5 = vfmaq_laneq_f64 (dat->p51, r2, p42_p52, 1);
  p5 = vmulq_f64 (r, vfmaq_f64 (vmulq_f64 (v_f64 (0.5), dat->p20), r2, p5));
  /* Compute p_i using recurrence relation:
     p_{i+2} = (p_i + r * Q_{i+1} * p_{i+1}) * R_{i+1}.  */
  float64x2_t qr5 = vld1q_f64 (dat->qr5), qr6 = vld1q_f64 (dat->qr6),
	      qr7 = vld1q_f64 (dat->qr7), qr8 = vld1q_f64 (dat->qr8),
	      qr9 = vld1q_f64 (dat->qr9);
  float64x2_t p6 = vfmaq_f64 (p4, p5, vmulq_laneq_f64 (r, qr5, 0));
  p6 = vmulq_laneq_f64 (p6, qr5, 1);
  float64x2_t p7 = vfmaq_f64 (p5, p6, vmulq_laneq_f64 (r, qr6, 0));
  p7 = vmulq_laneq_f64 (p7, qr6, 1);
  float64x2_t p8 = vfmaq_f64 (p6, p7, vmulq_laneq_f64 (r, qr7, 0));
  p8 = vmulq_laneq_f64 (p8, qr7, 1);
  float64x2_t p9 = vfmaq_f64 (p7, p8, vmulq_laneq_f64 (r, qr8, 0));
  p9 = vmulq_laneq_f64 (p9, qr8, 1);
  float64x2_t p10 = vfmaq_f64 (p8, p9, vmulq_laneq_f64 (r, qr9, 0));
  p10 = vmulq_laneq_f64 (p10, qr9, 1);
  /* Compute polynomial in d using pairwise Horner scheme.  */
  float64x2_t p90 = vfmaq_f64 (p9, d, p10);
  float64x2_t p78 = vfmaq_f64 (p7, d, p8);
  float64x2_t p56 = vfmaq_f64 (p5, d, p6);
  float64x2_t p34 = vfmaq_f64 (p3, d, p4);
  float64x2_t p12 = vfmaq_f64 (p1, d, p2);
  float64x2_t y = vfmaq_f64 (p78, d2, p90);
  y = vfmaq_f64 (p56, d2, y);
  y = vfmaq_f64 (p34, d2, y);
  y = vfmaq_f64 (p12, d2, y);

  y = vfmsq_f64 (e.erfc, e.scale, vfmsq_f64 (d, d2, y));

  /* Offset equals 2.0 if sign, else 0.0.  */
  uint64x2_t sign = vshrq_n_u64 (vreinterpretq_u64_f64 (x), 63);
  float64x2_t off = vreinterpretq_f64_u64 (vshlq_n_u64 (sign, 62));
  /* Copy sign and scale back in a single fma. Since the bit patterns do not
     overlap, then logical or and addition are equivalent here.  */
  float64x2_t fac = vreinterpretq_f64_u64 (
      vsraq_n_u64 (vshlq_n_u64 (sign, 63), dat->table_scale, 1));

#if WANT_SIMD_EXCEPT
  if (unlikely (v_any_u64 (cmp)))
    return special_case (xm, vfmaq_f64 (off, fac, y), cmp);
#endif

  return vfmaq_f64 (off, fac, y);
}

TEST_SIG (V, D, 1, erfc, -6.0, 28.0)
TEST_ULP (V_NAME_D1 (erfc), 1.21)
TEST_DISABLE_FENV_IF_NOT (V_NAME_D1 (erfc), WANT_SIMD_EXCEPT)
TEST_SYM_INTERVAL (V_NAME_D1 (erfc), 0, 0x1p-26, 40000)
TEST_INTERVAL (V_NAME_D1 (erfc), 0x1p-26, 28.0, 40000)
TEST_INTERVAL (V_NAME_D1 (erfc), -0x1p-26, -6.0, 40000)
TEST_INTERVAL (V_NAME_D1 (erfc), 28.0, inf, 40000)
TEST_INTERVAL (V_NAME_D1 (erfc), -6.0, -inf, 40000)
