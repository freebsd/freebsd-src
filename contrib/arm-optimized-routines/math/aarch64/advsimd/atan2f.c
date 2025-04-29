/*
 * Single-precision vector atan2(x) function.
 *
 * Copyright (c) 2021-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "test_sig.h"
#include "test_defs.h"

static const struct data
{
  float32x4_t c0, pi_over_2, c4, c6, c2;
  float c1, c3, c5, c7;
  uint32x4_t comp_const;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
     [2**-128, 1.0].
     Generated using fpminimax between FLT_MIN and 1.  */
  .c0 = V4 (-0x1.55555p-2f),	    .c1 = 0x1.99935ep-3f,
  .c2 = V4 (-0x1.24051ep-3f),	    .c3 = 0x1.bd7368p-4f,
  .c4 = V4 (-0x1.491f0ep-4f),	    .c5 = 0x1.93a2c0p-5f,
  .c6 = V4 (-0x1.4c3c60p-6f),	    .c7 = 0x1.01fd88p-8f,
  .pi_over_2 = V4 (0x1.921fb6p+0f), .comp_const = V4 (2 * 0x7f800000lu - 1),
};

#define SignMask v_u32 (0x80000000)

/* Special cases i.e. 0, infinity and nan (fall back to scalar calls).  */
static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t y, float32x4_t x, float32x4_t ret,
	      uint32x4_t sign_xy, uint32x4_t cmp)
{
  /* Account for the sign of y.  */
  ret = vreinterpretq_f32_u32 (
      veorq_u32 (vreinterpretq_u32_f32 (ret), sign_xy));
  return v_call2_f32 (atan2f, y, x, ret, cmp);
}

/* Returns 1 if input is the bit representation of 0, infinity or nan.  */
static inline uint32x4_t
zeroinfnan (uint32x4_t i, const struct data *d)
{
  /* 2 * i - 1 >= 2 * 0x7f800000lu - 1.  */
  return vcgeq_u32 (vsubq_u32 (vmulq_n_u32 (i, 2), v_u32 (1)), d->comp_const);
}

/* Fast implementation of vector atan2f. Maximum observed error is
   2.95 ULP in [0x1.9300d6p+6 0x1.93c0c6p+6] x [0x1.8c2dbp+6 0x1.8cea6p+6]:
   _ZGVnN4vv_atan2f (0x1.93836cp+6, 0x1.8cae1p+6) got 0x1.967f06p-1
						 want 0x1.967f00p-1.  */
float32x4_t VPCS_ATTR NOINLINE V_NAME_F2 (atan2) (float32x4_t y, float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t iy = vreinterpretq_u32_f32 (y);

  uint32x4_t special_cases
      = vorrq_u32 (zeroinfnan (ix, d), zeroinfnan (iy, d));

  uint32x4_t sign_x = vandq_u32 (ix, SignMask);
  uint32x4_t sign_y = vandq_u32 (iy, SignMask);
  uint32x4_t sign_xy = veorq_u32 (sign_x, sign_y);

  float32x4_t ax = vabsq_f32 (x);
  float32x4_t ay = vabsq_f32 (y);

  uint32x4_t pred_xlt0 = vcltzq_f32 (x);
  uint32x4_t pred_aygtax = vcgtq_f32 (ay, ax);

  /* Set up z for call to atanf.  */
  float32x4_t n = vbslq_f32 (pred_aygtax, vnegq_f32 (ax), ay);
  float32x4_t q = vbslq_f32 (pred_aygtax, ay, ax);
  float32x4_t z = vdivq_f32 (n, q);

  /* Work out the correct shift.  */
  float32x4_t shift = vreinterpretq_f32_u32 (
      vandq_u32 (pred_xlt0, vreinterpretq_u32_f32 (v_f32 (-2.0f))));
  shift = vbslq_f32 (pred_aygtax, vaddq_f32 (shift, v_f32 (1.0f)), shift);
  shift = vmulq_f32 (shift, d->pi_over_2);

  /* Calculate the polynomial approximation.
     Use 2-level Estrin scheme for P(z^2) with deg(P)=7. However,
     a standard implementation using z8 creates spurious underflow
     in the very last fma (when z^8 is small enough).
     Therefore, we split the last fma into a mul and an fma.
     Horner and single-level Estrin have higher errors that exceed
     threshold.  */
  float32x4_t z2 = vmulq_f32 (z, z);
  float32x4_t z4 = vmulq_f32 (z2, z2);

  float32x4_t c1357 = vld1q_f32 (&d->c1);
  float32x4_t p01 = vfmaq_laneq_f32 (d->c0, z2, c1357, 0);
  float32x4_t p23 = vfmaq_laneq_f32 (d->c2, z2, c1357, 1);
  float32x4_t p45 = vfmaq_laneq_f32 (d->c4, z2, c1357, 2);
  float32x4_t p67 = vfmaq_laneq_f32 (d->c6, z2, c1357, 3);
  float32x4_t p03 = vfmaq_f32 (p01, z4, p23);
  float32x4_t p47 = vfmaq_f32 (p45, z4, p67);

  float32x4_t ret = vfmaq_f32 (p03, z4, vmulq_f32 (z4, p47));

  /* y = shift + z * P(z^2).  */
  ret = vaddq_f32 (vfmaq_f32 (z, ret, vmulq_f32 (z2, z)), shift);

  if (unlikely (v_any_u32 (special_cases)))
    {
      return special_case (y, x, ret, sign_xy, special_cases);
    }

  /* Account for the sign of y.  */
  return vreinterpretq_f32_u32 (
      veorq_u32 (vreinterpretq_u32_f32 (ret), sign_xy));
}

HALF_WIDTH_ALIAS_F2 (atan2)

/* Arity of 2 means no mathbench entry emitted. See test/mathbench_funcs.h.  */
TEST_SIG (V, F, 2, atan2)
TEST_DISABLE_FENV (V_NAME_F2 (atan2))
TEST_ULP (V_NAME_F2 (atan2), 2.46)
TEST_INTERVAL (V_NAME_F2 (atan2), -10.0, 10.0, 50000)
TEST_INTERVAL (V_NAME_F2 (atan2), -1.0, 1.0, 40000)
TEST_INTERVAL (V_NAME_F2 (atan2), 0.0, 1.0, 40000)
TEST_INTERVAL (V_NAME_F2 (atan2), 1.0, 100.0, 40000)
TEST_INTERVAL (V_NAME_F2 (atan2), 1e6, 1e32, 40000)
