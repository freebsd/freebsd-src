/*
 * Double-precision vector log10(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  uint64x2_t off, sign_exp_mask, offset_lower_bound;
  uint32x4_t special_bound;
  double invln10, log10_2;
  double c1, c3;
  float64x2_t c0, c2, c4;
} data = {
  /* Computed from log coefficients divided by log(10) then rounded to double
     precision.  */
  .c0 = V2 (-0x1.bcb7b1526e506p-3),
  .c1 = 0x1.287a7636be1d1p-3,
  .c2 = V2 (-0x1.bcb7b158af938p-4),
  .c3 = 0x1.63c78734e6d07p-4,
  .c4 = V2 (-0x1.287461742fee4p-4),
  .invln10 = 0x1.bcb7b1526e50ep-2,
  .log10_2 = 0x1.34413509f79ffp-2,
  .off = V2 (0x3fe6900900000000),
  .sign_exp_mask = V2 (0xfff0000000000000),
  /* Lower bound is 0x0010000000000000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound - offset (which wraps around).  */
  .offset_lower_bound = V2 (0x0010000000000000 - 0x3fe6900900000000),
  .special_bound = V4 (0x7fe00000), /* asuint64(inf) - 0x0010000000000000.  */
};

#define N (1 << V_LOG10_TABLE_BITS)
#define IndexMask (N - 1)

struct entry
{
  float64x2_t invc;
  float64x2_t log10c;
};

static inline struct entry
lookup (uint64x2_t i)
{
  struct entry e;
  uint64_t i0
      = (vgetq_lane_u64 (i, 0) >> (52 - V_LOG10_TABLE_BITS)) & IndexMask;
  uint64_t i1
      = (vgetq_lane_u64 (i, 1) >> (52 - V_LOG10_TABLE_BITS)) & IndexMask;
  float64x2_t e0 = vld1q_f64 (&__v_log10_data.table[i0].invc);
  float64x2_t e1 = vld1q_f64 (&__v_log10_data.table[i1].invc);
  e.invc = vuzp1q_f64 (e0, e1);
  e.log10c = vuzp2q_f64 (e0, e1);
  return e;
}

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t hi, uint64x2_t u_off, float64x2_t y, float64x2_t r2,
	      uint32x2_t special, const struct data *d)
{
  float64x2_t x = vreinterpretq_f64_u64 (vaddq_u64 (u_off, d->off));
  return v_call_f64 (log10, x, vfmaq_f64 (hi, y, r2), vmovl_u32 (special));
}

/* Fast implementation of double-precision vector log10
   is a slight modification of double-precision vector log.
   Max ULP error: < 2.5 ulp (nearest rounding.)
   Maximum measured at 2.46 ulp for x in [0.96, 0.97]
   _ZGVnN2v_log10(0x1.13192407fcb46p+0) got 0x1.fff6be3cae4bbp-6
				       want 0x1.fff6be3cae4b9p-6.  */
float64x2_t VPCS_ATTR V_NAME_D1 (log10) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* To avoid having to mov x out of the way, keep u after offset has been
     applied, and recover x by adding the offset back in the special-case
     handler.  */
  uint64x2_t u = vreinterpretq_u64_f64 (x);
  uint64x2_t u_off = vsubq_u64 (u, d->off);

  /* x = 2^k z; where z is in range [OFF,2*OFF) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  int64x2_t k = vshrq_n_s64 (vreinterpretq_s64_u64 (u_off), 52);
  uint64x2_t iz = vsubq_u64 (u, vandq_u64 (u_off, d->sign_exp_mask));
  float64x2_t z = vreinterpretq_f64_u64 (iz);

  struct entry e = lookup (u_off);

  uint32x2_t special = vcge_u32 (vsubhn_u64 (u_off, d->offset_lower_bound),
				 vget_low_u32 (d->special_bound));

  /* log10(x) = log1p(z/c-1)/log(10) + log10(c) + k*log10(2).  */
  float64x2_t r = vfmaq_f64 (v_f64 (-1.0), z, e.invc);
  float64x2_t kd = vcvtq_f64_s64 (k);

  /* hi = r / log(10) + log10(c) + k*log10(2).
     Constants in v_log10_data.c are computed (in extended precision) as
     e.log10c := e.logc * invln10.  */
  float64x2_t cte = vld1q_f64 (&d->invln10);
  float64x2_t hi = vfmaq_laneq_f64 (e.log10c, r, cte, 0);

  /* y = log10(1+r) + n * log10(2).  */
  hi = vfmaq_laneq_f64 (hi, kd, cte, 1);

  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t odd_coeffs = vld1q_f64 (&d->c1);
  float64x2_t y = vfmaq_laneq_f64 (d->c2, r, odd_coeffs, 1);
  float64x2_t p = vfmaq_laneq_f64 (d->c0, r, odd_coeffs, 0);
  y = vfmaq_f64 (y, d->c4, r2);
  y = vfmaq_f64 (p, y, r2);

  if (unlikely (v_any_u32h (special)))
    return special_case (hi, u_off, y, r2, special, d);
  return vfmaq_f64 (hi, y, r2);
}

TEST_SIG (V, D, 1, log10, 0.01, 11.1)
TEST_ULP (V_NAME_D1 (log10), 1.97)
TEST_INTERVAL (V_NAME_D1 (log10), -0.0, -inf, 1000)
TEST_INTERVAL (V_NAME_D1 (log10), 0, 0x1p-149, 1000)
TEST_INTERVAL (V_NAME_D1 (log10), 0x1p-149, 0x1p-126, 4000)
TEST_INTERVAL (V_NAME_D1 (log10), 0x1p-126, 0x1p-23, 50000)
TEST_INTERVAL (V_NAME_D1 (log10), 0x1p-23, 1.0, 50000)
TEST_INTERVAL (V_NAME_D1 (log10), 1.0, 100, 50000)
TEST_INTERVAL (V_NAME_D1 (log10), 100, inf, 50000)
