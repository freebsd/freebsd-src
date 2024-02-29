/*
 * Double-precision vector log10(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "poly_advsimd_f64.h"

#define N (1 << V_LOG10_TABLE_BITS)

static const struct data
{
  uint64x2_t min_norm;
  uint32x4_t special_bound;
  float64x2_t poly[5];
  float64x2_t invln10, log10_2, ln2;
  uint64x2_t sign_exp_mask;
} data = {
  /* Computed from log coefficients divided by log(10) then rounded to double
     precision.  */
  .poly = { V2 (-0x1.bcb7b1526e506p-3), V2 (0x1.287a7636be1d1p-3),
	    V2 (-0x1.bcb7b158af938p-4), V2 (0x1.63c78734e6d07p-4),
	    V2 (-0x1.287461742fee4p-4) },
  .ln2 = V2 (0x1.62e42fefa39efp-1),
  .invln10 = V2 (0x1.bcb7b1526e50ep-2),
  .log10_2 = V2 (0x1.34413509f79ffp-2),
  .min_norm = V2 (0x0010000000000000), /* asuint64(0x1p-1022).  */
  .special_bound = V4 (0x7fe00000),    /* asuint64(inf) - min_norm.  */
  .sign_exp_mask = V2 (0xfff0000000000000),
};

#define Off v_u64 (0x3fe6900900000000)
#define IndexMask (N - 1)

#define T(s, i) __v_log10_data.s[i]

struct entry
{
  float64x2_t invc;
  float64x2_t log10c;
};

static inline struct entry
lookup (uint64x2_t i)
{
  struct entry e;
  uint64_t i0 = (i[0] >> (52 - V_LOG10_TABLE_BITS)) & IndexMask;
  uint64_t i1 = (i[1] >> (52 - V_LOG10_TABLE_BITS)) & IndexMask;
  float64x2_t e0 = vld1q_f64 (&__v_log10_data.table[i0].invc);
  float64x2_t e1 = vld1q_f64 (&__v_log10_data.table[i1].invc);
  e.invc = vuzp1q_f64 (e0, e1);
  e.log10c = vuzp2q_f64 (e0, e1);
  return e;
}

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, float64x2_t hi, float64x2_t r2,
	      uint32x2_t special)
{
  return v_call_f64 (log10, x, vfmaq_f64 (hi, r2, y), vmovl_u32 (special));
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
  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  uint32x2_t special = vcge_u32 (vsubhn_u64 (ix, d->min_norm),
				 vget_low_u32 (d->special_bound));

  /* x = 2^k z; where z is in range [OFF,2*OFF) and exact.
     The range is split into N subintervals.
     The ith subinterval contains z and c is near its center.  */
  uint64x2_t tmp = vsubq_u64 (ix, Off);
  int64x2_t k = vshrq_n_s64 (vreinterpretq_s64_u64 (tmp), 52);
  uint64x2_t iz = vsubq_u64 (ix, vandq_u64 (tmp, d->sign_exp_mask));
  float64x2_t z = vreinterpretq_f64_u64 (iz);

  struct entry e = lookup (tmp);

  /* log10(x) = log1p(z/c-1)/log(10) + log10(c) + k*log10(2).  */
  float64x2_t r = vfmaq_f64 (v_f64 (-1.0), z, e.invc);
  float64x2_t kd = vcvtq_f64_s64 (k);

  /* hi = r / log(10) + log10(c) + k*log10(2).
     Constants in v_log10_data.c are computed (in extended precision) as
     e.log10c := e.logc * ivln10.  */
  float64x2_t w = vfmaq_f64 (e.log10c, r, d->invln10);

  /* y = log10(1+r) + n * log10(2).  */
  float64x2_t hi = vfmaq_f64 (w, kd, d->log10_2);

  /* y = r2*(A0 + r*A1 + r2*(A2 + r*A3 + r2*A4)) + hi.  */
  float64x2_t r2 = vmulq_f64 (r, r);
  float64x2_t y = v_pw_horner_4_f64 (r, r2, d->poly);

  if (unlikely (v_any_u32h (special)))
    return special_case (x, y, hi, r2, special);
  return vfmaq_f64 (hi, r2, y);
}

PL_SIG (V, D, 1, log10, 0.01, 11.1)
PL_TEST_ULP (V_NAME_D1 (log10), 1.97)
PL_TEST_EXPECT_FENV_ALWAYS (V_NAME_D1 (log10))
PL_TEST_INTERVAL (V_NAME_D1 (log10), -0.0, -inf, 1000)
PL_TEST_INTERVAL (V_NAME_D1 (log10), 0, 0x1p-149, 1000)
PL_TEST_INTERVAL (V_NAME_D1 (log10), 0x1p-149, 0x1p-126, 4000)
PL_TEST_INTERVAL (V_NAME_D1 (log10), 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (V_NAME_D1 (log10), 0x1p-23, 1.0, 50000)
PL_TEST_INTERVAL (V_NAME_D1 (log10), 1.0, 100, 50000)
PL_TEST_INTERVAL (V_NAME_D1 (log10), 100, inf, 50000)
