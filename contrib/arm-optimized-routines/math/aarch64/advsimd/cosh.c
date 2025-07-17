/*
 * Double-precision vector cosh(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float64x2_t poly[3];
  float64x2_t inv_ln2;
  double ln2[2];
  float64x2_t shift, thres;
  uint64x2_t index_mask, special_bound;
} data = {
  .poly = { V2 (0x1.fffffffffffd4p-2), V2 (0x1.5555571d6b68cp-3),
	    V2 (0x1.5555576a59599p-5), },

  .inv_ln2 = V2 (0x1.71547652b82fep8), /* N/ln2.  */
  /* -ln2/N.  */
  .ln2 = {-0x1.62e42fefa39efp-9, -0x1.abc9e3b39803f3p-64},
  .shift = V2 (0x1.8p+52),
  .thres = V2 (704.0),

  .index_mask = V2 (0xff),
  /* 0x1.6p9, above which exp overflows.  */
  .special_bound = V2 (0x4086000000000000),
};

static float64x2_t NOINLINE VPCS_ATTR
special_case (float64x2_t x, float64x2_t y, uint64x2_t special)
{
  return v_call_f64 (cosh, x, y, special);
}

/* Helper for approximating exp(x). Copied from v_exp_tail, with no
   special-case handling or tail.  */
static inline float64x2_t
exp_inline (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* n = round(x/(ln2/N)).  */
  float64x2_t z = vfmaq_f64 (d->shift, x, d->inv_ln2);
  uint64x2_t u = vreinterpretq_u64_f64 (z);
  float64x2_t n = vsubq_f64 (z, d->shift);

  /* r = x - n*ln2/N.  */
  float64x2_t ln2 = vld1q_f64 (d->ln2);
  float64x2_t r = vfmaq_laneq_f64 (x, n, ln2, 0);
  r = vfmaq_laneq_f64 (r, n, ln2, 1);

  uint64x2_t e = vshlq_n_u64 (u, 52 - V_EXP_TAIL_TABLE_BITS);
  uint64x2_t i = vandq_u64 (u, d->index_mask);

  /* y = tail + exp(r) - 1 ~= r + C1 r^2 + C2 r^3 + C3 r^4.  */
  float64x2_t y = vfmaq_f64 (d->poly[1], d->poly[2], r);
  y = vfmaq_f64 (d->poly[0], y, r);
  y = vmulq_f64 (vfmaq_f64 (v_f64 (1), y, r), r);

  /* s = 2^(n/N).  */
  u = v_lookup_u64 (__v_exp_tail_data, i);
  float64x2_t s = vreinterpretq_f64_u64 (vaddq_u64 (u, e));

  return vfmaq_f64 (s, y, s);
}

/* Approximation for vector double-precision cosh(x) using exp_inline.
   cosh(x) = (exp(x) + exp(-x)) / 2.
   The greatest observed error is in the scalar fall-back region, so is the
   same as the scalar routine, 1.93 ULP:
   _ZGVnN2v_cosh (0x1.628af341989dap+9) got 0x1.fdf28623ef921p+1021
				       want 0x1.fdf28623ef923p+1021.

   The greatest observed error in the non-special region is 1.54 ULP:
   _ZGVnN2v_cosh (0x1.8e205b6ecacf7p+2) got 0x1.f711dcb0c77afp+7
				       want 0x1.f711dcb0c77b1p+7.  */
float64x2_t VPCS_ATTR V_NAME_D1 (cosh) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  float64x2_t ax = vabsq_f64 (x);
  uint64x2_t special
      = vcgtq_u64 (vreinterpretq_u64_f64 (ax), d->special_bound);

  /* Up to the point that exp overflows, we can use it to calculate cosh by
     exp(|x|) / 2 + 1 / (2 * exp(|x|)).  */
  float64x2_t t = exp_inline (ax);
  float64x2_t half_t = vmulq_n_f64 (t, 0.5);
  float64x2_t half_over_t = vdivq_f64 (v_f64 (0.5), t);

  /* Fall back to scalar for any special cases.  */
  if (unlikely (v_any_u64 (special)))
    return special_case (x, vaddq_f64 (half_t, half_over_t), special);

  return vaddq_f64 (half_t, half_over_t);
}

TEST_SIG (V, D, 1, cosh, -10.0, 10.0)
TEST_ULP (V_NAME_D1 (cosh), 1.43)
TEST_DISABLE_FENV_IF_NOT (V_NAME_D1 (cosh), WANT_SIMD_EXCEPT)
TEST_SYM_INTERVAL (V_NAME_D1 (cosh), 0, 0x1.6p9, 100000)
TEST_SYM_INTERVAL (V_NAME_D1 (cosh), 0x1.6p9, inf, 1000)
