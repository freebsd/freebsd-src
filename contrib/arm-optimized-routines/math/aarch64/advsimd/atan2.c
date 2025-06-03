/*
 * Double-precision vector atan2(x) function.
 *
 * Copyright (c) 2021-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float64x2_t c0, c2, c4, c6, c8, c10, c12, c14, c16, c18;
  float64x2_t pi_over_2;
  double c1, c3, c5, c7, c9, c11, c13, c15, c17, c19;
  uint64x2_t zeroinfnan, minustwo;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
	      [2**-1022, 1.0].  */
  .c0 = V2 (-0x1.5555555555555p-2),
  .c1 = 0x1.99999999996c1p-3,
  .c2 = V2 (-0x1.2492492478f88p-3),
  .c3 = 0x1.c71c71bc3951cp-4,
  .c4 = V2 (-0x1.745d160a7e368p-4),
  .c5 = 0x1.3b139b6a88ba1p-4,
  .c6 = V2 (-0x1.11100ee084227p-4),
  .c7 = 0x1.e1d0f9696f63bp-5,
  .c8 = V2 (-0x1.aebfe7b418581p-5),
  .c9 = 0x1.842dbe9b0d916p-5,
  .c10 = V2 (-0x1.5d30140ae5e99p-5),
  .c11 = 0x1.338e31eb2fbbcp-5,
  .c12 = V2 (-0x1.00e6eece7de8p-5),
  .c13 = 0x1.860897b29e5efp-6,
  .c14 = V2 (-0x1.0051381722a59p-6),
  .c15 = 0x1.14e9dc19a4a4ep-7,
  .c16 = V2 (-0x1.d0062b42fe3bfp-9),
  .c17 = 0x1.17739e210171ap-10,
  .c18 = V2 (-0x1.ab24da7be7402p-13),
  .c19 = 0x1.358851160a528p-16,
  .pi_over_2 = V2 (0x1.921fb54442d18p+0),
  .zeroinfnan = V2 (2 * 0x7ff0000000000000ul - 1),
  .minustwo = V2 (0xc000000000000000),
};

#define SignMask v_u64 (0x8000000000000000)

/* Special cases i.e. 0, infinity, NaN (fall back to scalar calls).  */
static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t y, float64x2_t x, float64x2_t ret,
	      uint64x2_t sign_xy, uint64x2_t cmp)
{
  /* Account for the sign of x and y.  */
  ret = vreinterpretq_f64_u64 (
      veorq_u64 (vreinterpretq_u64_f64 (ret), sign_xy));
  return v_call2_f64 (atan2, y, x, ret, cmp);
}

/* Returns 1 if input is the bit representation of 0, infinity or nan.  */
static inline uint64x2_t
zeroinfnan (uint64x2_t i, const struct data *d)
{
  /* (2 * i - 1) >= (2 * asuint64 (INFINITY) - 1).  */
  return vcgeq_u64 (vsubq_u64 (vaddq_u64 (i, i), v_u64 (1)), d->zeroinfnan);
}

/* Fast implementation of vector atan2.
   Maximum observed error is 2.8 ulps:
   _ZGVnN2vv_atan2 (0x1.9651a429a859ap+5, 0x1.953075f4ee26p+5)
	got 0x1.92d628ab678ccp-1
       want 0x1.92d628ab678cfp-1.  */
float64x2_t VPCS_ATTR V_NAME_D2 (atan2) (float64x2_t y, float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint64x2_t ix = vreinterpretq_u64_f64 (x);
  uint64x2_t iy = vreinterpretq_u64_f64 (y);

  uint64x2_t special_cases
      = vorrq_u64 (zeroinfnan (ix, d), zeroinfnan (iy, d));

  uint64x2_t sign_x = vandq_u64 (ix, SignMask);
  uint64x2_t sign_y = vandq_u64 (iy, SignMask);
  uint64x2_t sign_xy = veorq_u64 (sign_x, sign_y);

  float64x2_t ax = vabsq_f64 (x);
  float64x2_t ay = vabsq_f64 (y);

  uint64x2_t pred_xlt0 = vcltzq_f64 (x);
  uint64x2_t pred_aygtax = vcagtq_f64 (y, x);

  /* Set up z for call to atan.  */
  float64x2_t n = vbslq_f64 (pred_aygtax, vnegq_f64 (ax), ay);
  float64x2_t q = vbslq_f64 (pred_aygtax, ay, ax);
  float64x2_t z = vdivq_f64 (n, q);

  /* Work out the correct shift.  */
  float64x2_t shift
      = vreinterpretq_f64_u64 (vandq_u64 (pred_xlt0, d->minustwo));
  shift = vbslq_f64 (pred_aygtax, vaddq_f64 (shift, v_f64 (1.0)), shift);
  shift = vmulq_f64 (shift, d->pi_over_2);

  /* Calculate the polynomial approximation.
     Use split Estrin scheme for P(z^2) with deg(P)=19. Use split instead of
     full scheme to avoid underflow in x^16.
     The order 19 polynomial P approximates
     (atan(sqrt(x))-sqrt(x))/x^(3/2).  */
  float64x2_t z2 = vmulq_f64 (z, z);
  float64x2_t x2 = vmulq_f64 (z2, z2);
  float64x2_t x4 = vmulq_f64 (x2, x2);
  float64x2_t x8 = vmulq_f64 (x4, x4);

  float64x2_t c13 = vld1q_f64 (&d->c1);
  float64x2_t c57 = vld1q_f64 (&d->c5);
  float64x2_t c911 = vld1q_f64 (&d->c9);
  float64x2_t c1315 = vld1q_f64 (&d->c13);
  float64x2_t c1719 = vld1q_f64 (&d->c17);

  /* estrin_7.  */
  float64x2_t p01 = vfmaq_laneq_f64 (d->c0, z2, c13, 0);
  float64x2_t p23 = vfmaq_laneq_f64 (d->c2, z2, c13, 1);
  float64x2_t p03 = vfmaq_f64 (p01, x2, p23);

  float64x2_t p45 = vfmaq_laneq_f64 (d->c4, z2, c57, 0);
  float64x2_t p67 = vfmaq_laneq_f64 (d->c6, z2, c57, 1);
  float64x2_t p47 = vfmaq_f64 (p45, x2, p67);

  float64x2_t p07 = vfmaq_f64 (p03, x4, p47);

  /* estrin_11.  */
  float64x2_t p89 = vfmaq_laneq_f64 (d->c8, z2, c911, 0);
  float64x2_t p1011 = vfmaq_laneq_f64 (d->c10, z2, c911, 1);
  float64x2_t p811 = vfmaq_f64 (p89, x2, p1011);

  float64x2_t p1213 = vfmaq_laneq_f64 (d->c12, z2, c1315, 0);
  float64x2_t p1415 = vfmaq_laneq_f64 (d->c14, z2, c1315, 1);
  float64x2_t p1215 = vfmaq_f64 (p1213, x2, p1415);

  float64x2_t p1617 = vfmaq_laneq_f64 (d->c16, z2, c1719, 0);
  float64x2_t p1819 = vfmaq_laneq_f64 (d->c18, z2, c1719, 1);
  float64x2_t p1619 = vfmaq_f64 (p1617, x2, p1819);

  float64x2_t p815 = vfmaq_f64 (p811, x4, p1215);
  float64x2_t p819 = vfmaq_f64 (p815, x8, p1619);

  float64x2_t ret = vfmaq_f64 (p07, p819, x8);

  /* Finalize. y = shift + z + z^3 * P(z^2).  */
  ret = vfmaq_f64 (z, ret, vmulq_f64 (z2, z));
  ret = vaddq_f64 (ret, shift);

  if (unlikely (v_any_u64 (special_cases)))
    return special_case (y, x, ret, sign_xy, special_cases);

  /* Account for the sign of x and y.  */
  ret = vreinterpretq_f64_u64 (
      veorq_u64 (vreinterpretq_u64_f64 (ret), sign_xy));

  return ret;
}

/* Arity of 2 means no mathbench entry emitted. See test/mathbench_funcs.h.  */
TEST_SIG (V, D, 2, atan2)
// TODO tighten this once __v_atan2 is fixed
TEST_ULP (V_NAME_D2 (atan2), 2.9)
TEST_DISABLE_FENV (V_NAME_D2 (atan2))
TEST_INTERVAL (V_NAME_D2 (atan2), -10.0, 10.0, 50000)
TEST_INTERVAL (V_NAME_D2 (atan2), -1.0, 1.0, 40000)
TEST_INTERVAL (V_NAME_D2 (atan2), 0.0, 1.0, 40000)
TEST_INTERVAL (V_NAME_D2 (atan2), 1.0, 100.0, 40000)
TEST_INTERVAL (V_NAME_D2 (atan2), 1e6, 1e32, 40000)
