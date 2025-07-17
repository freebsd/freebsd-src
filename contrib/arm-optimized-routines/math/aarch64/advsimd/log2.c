/*
 * Double-precision vector log2 function.
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
  float64x2_t c0, c2;
  double c1, c3, invln2, c4;
} data = {
  /* Each coefficient was generated to approximate log(r) for |r| < 0x1.fp-9
     and N = 128, then scaled by log2(e) in extended precision and rounded back
     to double precision.  */
  .c0 = V2 (-0x1.71547652b8300p-1),
  .c1 = 0x1.ec709dc340953p-2,
  .c2 = V2 (-0x1.71547651c8f35p-2),
  .c3 = 0x1.2777ebe12dda5p-2,
  .c4 = -0x1.ec738d616fe26p-3,
  .invln2 = 0x1.71547652b82fep0,
  .off = V2 (0x3fe6900900000000),
  .sign_exp_mask = V2 (0xfff0000000000000),
  /* Lower bound is 0x0010000000000000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound - offset (which wraps around).  */
  .offset_lower_bound = V2 (0x0010000000000000 - 0x3fe6900900000000),
  .special_bound = V4 (0x7fe00000), /* asuint64(inf) - asuint64(0x1p-1022).  */
};

#define N (1 << V_LOG2_TABLE_BITS)
#define IndexMask (N - 1)

struct entry
{
  float64x2_t invc;
  float64x2_t log2c;
};

static inline struct entry
lookup (uint64x2_t i)
{
  struct entry e;
  uint64_t i0
      = (vgetq_lane_u64 (i, 0) >> (52 - V_LOG2_TABLE_BITS)) & IndexMask;
  uint64_t i1
      = (vgetq_lane_u64 (i, 1) >> (52 - V_LOG2_TABLE_BITS)) & IndexMask;
  float64x2_t e0 = vld1q_f64 (&__v_log2_data.table[i0].invc);
  float64x2_t e1 = vld1q_f64 (&__v_log2_data.table[i1].invc);
  e.invc = vuzp1q_f64 (e0, e1);
  e.log2c = vuzp2q_f64 (e0, e1);
  return e;
}

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t hi, uint64x2_t u_off, float64x2_t y, float64x2_t r2,
	      uint32x2_t special, const struct data *d)
{
  float64x2_t x = vreinterpretq_f64_u64 (vaddq_u64 (u_off, d->off));
  return v_call_f64 (log2, x, vfmaq_f64 (hi, y, r2), vmovl_u32 (special));
}

/* Double-precision vector log2 routine. Implements the same algorithm as
   vector log10, with coefficients and table entries scaled in extended
   precision. The maximum observed error is 2.58 ULP:
   _ZGVnN2v_log2(0x1.0b556b093869bp+0) got 0x1.fffb34198d9dap-5
				      want 0x1.fffb34198d9ddp-5.  */
float64x2_t VPCS_ATTR V_NAME_D1 (log2) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* To avoid having to mov x out of the way, keep u after offset has been
     applied, and recover x by adding the offset back in the special-case
     handler.  */
  uint64x2_t u = vreinterpretq_u64_f64 (x);
  uint64x2_t u_off = vsubq_u64 (u, d->off);

  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  int64x2_t k = vshrq_n_s64 (vreinterpretq_s64_u64 (u_off), 52);
  uint64x2_t iz = vsubq_u64 (u, vandq_u64 (u_off, d->sign_exp_mask));
  float64x2_t z = vreinterpretq_f64_u64 (iz);

  struct entry e = lookup (u_off);

  uint32x2_t special = vcge_u32 (vsubhn_u64 (u_off, d->offset_lower_bound),
				 vget_low_u32 (d->special_bound));

  /* log2(x) = log1p(z/c-1)/log(2) + log2(c) + k.  */
  float64x2_t r = vfmaq_f64 (v_f64 (-1.0), z, e.invc);
  float64x2_t kd = vcvtq_f64_s64 (k);

  float64x2_t invln2_and_c4 = vld1q_f64 (&d->invln2);
  float64x2_t hi
      = vfmaq_laneq_f64 (vaddq_f64 (e.log2c, kd), r, invln2_and_c4, 0);

  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t odd_coeffs = vld1q_f64 (&d->c1);
  float64x2_t y = vfmaq_laneq_f64 (d->c2, r, odd_coeffs, 1);
  float64x2_t p = vfmaq_laneq_f64 (d->c0, r, odd_coeffs, 0);
  y = vfmaq_laneq_f64 (y, r2, invln2_and_c4, 1);
  y = vfmaq_f64 (p, r2, y);

  if (unlikely (v_any_u32h (special)))
    return special_case (hi, u_off, y, r2, special, d);
  return vfmaq_f64 (hi, y, r2);
}

TEST_SIG (V, D, 1, log2, 0.01, 11.1)
TEST_ULP (V_NAME_D1 (log2), 2.09)
TEST_INTERVAL (V_NAME_D1 (log2), -0.0, -0x1p126, 100)
TEST_INTERVAL (V_NAME_D1 (log2), 0x1p-149, 0x1p-126, 4000)
TEST_INTERVAL (V_NAME_D1 (log2), 0x1p-126, 0x1p-23, 50000)
TEST_INTERVAL (V_NAME_D1 (log2), 0x1p-23, 1.0, 50000)
TEST_INTERVAL (V_NAME_D1 (log2), 1.0, 100, 50000)
TEST_INTERVAL (V_NAME_D1 (log2), 100, inf, 50000)
