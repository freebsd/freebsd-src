/*
 * Double-precision vector log(x) function.
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_defs.h"
#include "test_sig.h"

static const struct data
{
  uint64x2_t off, sign_exp_mask, offset_lower_bound;
  uint32x4_t special_bound;
  float64x2_t c0, c2;
  double c1, c3, ln2, c4;
} data = {
  /* Rel error: 0x1.6272e588p-56 in [ -0x1.fc1p-9 0x1.009p-8 ].  */
  .c0 = V2 (-0x1.ffffffffffff7p-2),
  .c1 = 0x1.55555555170d4p-2,
  .c2 = V2 (-0x1.0000000399c27p-2),
  .c3 = 0x1.999b2e90e94cap-3,
  .c4 = -0x1.554e550bd501ep-3,
  .ln2 = 0x1.62e42fefa39efp-1,
  .sign_exp_mask = V2 (0xfff0000000000000),
  .off = V2 (0x3fe6900900000000),
  /* Lower bound is 0x0010000000000000. For
     optimised register use subnormals are detected after offset has been
     subtracted, so lower bound - offset (which wraps around).  */
  .offset_lower_bound = V2 (0x0010000000000000 - 0x3fe6900900000000),
  .special_bound = V4 (0x7fe00000), /* asuint64(inf) -  asuint64(0x1p-126).  */
};

#define N (1 << V_LOG_TABLE_BITS)
#define IndexMask (N - 1)

struct entry
{
  float64x2_t invc;
  float64x2_t logc;
};

static inline struct entry
lookup (uint64x2_t i)
{
  /* Since N is a power of 2, n % N = n & (N - 1).  */
  struct entry e;
  uint64_t i0 = (vgetq_lane_u64 (i, 0) >> (52 - V_LOG_TABLE_BITS)) & IndexMask;
  uint64_t i1 = (vgetq_lane_u64 (i, 1) >> (52 - V_LOG_TABLE_BITS)) & IndexMask;
  float64x2_t e0 = vld1q_f64 (&__v_log_data.table[i0].invc);
  float64x2_t e1 = vld1q_f64 (&__v_log_data.table[i1].invc);
  e.invc = vuzp1q_f64 (e0, e1);
  e.logc = vuzp2q_f64 (e0, e1);
  return e;
}

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t hi, uint64x2_t u_off, float64x2_t y, float64x2_t r2,
	      uint32x2_t special, const struct data *d)
{
  float64x2_t x = vreinterpretq_f64_u64 (vaddq_u64 (u_off, d->off));
  return v_call_f64 (log, x, vfmaq_f64 (hi, y, r2), vmovl_u32 (special));
}

/* Double-precision vector log routine.
   The maximum observed error is 2.17 ULP:
   _ZGVnN2v_log(0x1.a6129884398a3p+0) got 0x1.ffffff1cca043p-2
				     want 0x1.ffffff1cca045p-2.  */
float64x2_t VPCS_ATTR V_NAME_D1 (log) (float64x2_t x)
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

  /* log(x) = log1p(z/c-1) + log(c) + k*Ln2.  */
  float64x2_t r = vfmaq_f64 (v_f64 (-1.0), z, e.invc);
  float64x2_t kd = vcvtq_f64_s64 (k);

  /* hi = r + log(c) + k*Ln2.  */
  float64x2_t ln2_and_c4 = vld1q_f64 (&d->ln2);
  float64x2_t hi = vfmaq_laneq_f64 (vaddq_f64 (e.logc, r), kd, ln2_and_c4, 0);

  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  float64x2_t odd_coeffs = vld1q_f64 (&d->c1);
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t y = vfmaq_laneq_f64 (d->c2, r, odd_coeffs, 1);
  float64x2_t p = vfmaq_laneq_f64 (d->c0, r, odd_coeffs, 0);
  y = vfmaq_laneq_f64 (y, r2, ln2_and_c4, 1);
  y = vfmaq_f64 (p, r2, y);

  if (unlikely (v_any_u32h (special)))
    return special_case (hi, u_off, y, r2, special, d);
  return vfmaq_f64 (hi, y, r2);
}

TEST_SIG (V, D, 1, log, 0.01, 11.1)
TEST_ULP (V_NAME_D1 (log), 1.67)
TEST_DISABLE_FENV_IF_NOT (V_NAME_D1 (log), WANT_SIMD_EXCEPT)
TEST_INTERVAL (V_NAME_D1 (log), 0, 0xffff000000000000, 10000)
TEST_INTERVAL (V_NAME_D1 (log), 0x1p-4, 0x1p4, 400000)
TEST_INTERVAL (V_NAME_D1 (log), 0, inf, 400000)
