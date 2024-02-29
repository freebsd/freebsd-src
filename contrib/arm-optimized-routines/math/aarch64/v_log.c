/*
 * Double-precision vector log(x) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "v_math.h"

static const struct data
{
  uint64x2_t min_norm;
  uint32x4_t special_bound;
  float64x2_t poly[5];
  float64x2_t ln2;
  uint64x2_t sign_exp_mask;
} data = {
  /* Worst-case error: 1.17 + 0.5 ulp.
     Rel error: 0x1.6272e588p-56 in [ -0x1.fc1p-9 0x1.009p-8 ].  */
  .poly = { V2 (-0x1.ffffffffffff7p-2), V2 (0x1.55555555170d4p-2),
	    V2 (-0x1.0000000399c27p-2), V2 (0x1.999b2e90e94cap-3),
	    V2 (-0x1.554e550bd501ep-3) },
  .ln2 = V2 (0x1.62e42fefa39efp-1),
  .min_norm = V2 (0x0010000000000000),
  .special_bound = V4 (0x7fe00000), /* asuint64(inf) - min_norm.  */
  .sign_exp_mask = V2 (0xfff0000000000000)
};

#define A(i) d->poly[i]
#define N (1 << V_LOG_TABLE_BITS)
#define IndexMask (N - 1)
#define Off v_u64 (0x3fe6900900000000)

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
  uint64_t i0 = (i[0] >> (52 - V_LOG_TABLE_BITS)) & IndexMask;
  uint64_t i1 = (i[1] >> (52 - V_LOG_TABLE_BITS)) & IndexMask;
  float64x2_t e0 = vld1q_f64 (&__v_log_data.table[i0].invc);
  float64x2_t e1 = vld1q_f64 (&__v_log_data.table[i1].invc);
  e.invc = vuzp1q_f64 (e0, e1);
  e.logc = vuzp2q_f64 (e0, e1);
  return e;
}

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, float64x2_t hi, float64x2_t r2,
	      uint32x2_t cmp)
{
  return v_call_f64 (log, x, vfmaq_f64 (hi, y, r2), vmovl_u32 (cmp));
}

float64x2_t VPCS_ATTR V_NAME_D1 (log) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);
  float64x2_t z, r, r2, p, y, kd, hi;
  uint64x2_t ix, iz, tmp;
  uint32x2_t cmp;
  int64x2_t k;
  struct entry e;

  ix = vreinterpretq_u64_f64 (x);
  cmp = vcge_u32 (vsubhn_u64 (ix, d->min_norm),
		  vget_low_u32 (d->special_bound));

  /* x = 2^k z; where z is in range [Off,2*Off) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  tmp = vsubq_u64 (ix, Off);
  k = vshrq_n_s64 (vreinterpretq_s64_u64 (tmp), 52); /* arithmetic shift.  */
  iz = vsubq_u64 (ix, vandq_u64 (tmp, d->sign_exp_mask));
  z = vreinterpretq_f64_u64 (iz);
  e = lookup (tmp);

  /* log(x) = log1p(z/c-1) + log(c) + k*Ln2.  */
  r = vfmaq_f64 (v_f64 (-1.0), z, e.invc);
  kd = vcvtq_f64_s64 (k);

  /* hi = r + log(c) + k*Ln2.  */
  hi = vfmaq_f64 (vaddq_f64 (e.logc, r), kd, d->ln2);
  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  r2 = vmulq_f64 (r, r);
  y = vfmaq_f64 (A (2), A (3), r);
  p = vfmaq_f64 (A (0), A (1), r);
  y = vfmaq_f64 (y, A (4), r2);
  y = vfmaq_f64 (p, y, r2);

  if (unlikely (v_any_u32h (cmp)))
    return special_case (x, y, hi, r2, cmp);
  return vfmaq_f64 (hi, y, r2);
}
