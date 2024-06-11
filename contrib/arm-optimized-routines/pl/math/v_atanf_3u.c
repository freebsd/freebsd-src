/*
 * Single-precision vector atan(x) function.
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "poly_advsimd_f32.h"

static const struct data
{
  float32x4_t poly[8];
  float32x4_t pi_over_2;
} data = {
  /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on
     [2**-128, 1.0].
     Generated using fpminimax between FLT_MIN and 1.  */
  .poly = { V4 (-0x1.55555p-2f), V4 (0x1.99935ep-3f), V4 (-0x1.24051ep-3f),
	    V4 (0x1.bd7368p-4f), V4 (-0x1.491f0ep-4f), V4 (0x1.93a2c0p-5f),
	    V4 (-0x1.4c3c60p-6f), V4 (0x1.01fd88p-8f) },
  .pi_over_2 = V4 (0x1.921fb6p+0f),
};

#define SignMask v_u32 (0x80000000)

#define P(i) d->poly[i]

#define TinyBound 0x30800000 /* asuint(0x1p-30).  */
#define BigBound 0x4e800000  /* asuint(0x1p30).  */

#if WANT_SIMD_EXCEPT
static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, uint32x4_t special)
{
  return v_call_f32 (atanf, x, y, special);
}
#endif

/* Fast implementation of vector atanf based on
   atan(x) ~ shift + z + z^3 * P(z^2) with reduction to [0,1]
   using z=-1/x and shift = pi/2. Maximum observed error is 2.9ulps:
   _ZGVnN4v_atanf (0x1.0468f6p+0) got 0x1.967f06p-1 want 0x1.967fp-1.  */
float32x4_t VPCS_ATTR V_NAME_F1 (atan) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);

  /* Small cases, infs and nans are supported by our approximation technique,
     but do not set fenv flags correctly. Only trigger special case if we need
     fenv.  */
  uint32x4_t ix = vreinterpretq_u32_f32 (x);
  uint32x4_t sign = vandq_u32 (ix, SignMask);

#if WANT_SIMD_EXCEPT
  uint32x4_t ia = vandq_u32 (ix, v_u32 (0x7ff00000));
  uint32x4_t special = vcgtq_u32 (vsubq_u32 (ia, v_u32 (TinyBound)),
				  v_u32 (BigBound - TinyBound));
  /* If any lane is special, fall back to the scalar routine for all lanes.  */
  if (unlikely (v_any_u32 (special)))
    return special_case (x, x, v_u32 (-1));
#endif

  /* Argument reduction:
     y := arctan(x) for x < 1
     y := pi/2 + arctan(-1/x) for x > 1
     Hence, use z=-1/a if x>=1, otherwise z=a.  */
  uint32x4_t red = vcagtq_f32 (x, v_f32 (1.0));
  /* Avoid dependency in abs(x) in division (and comparison).  */
  float32x4_t z = vbslq_f32 (red, vdivq_f32 (v_f32 (1.0f), x), x);
  float32x4_t shift = vreinterpretq_f32_u32 (
      vandq_u32 (red, vreinterpretq_u32_f32 (d->pi_over_2)));
  /* Use absolute value only when needed (odd powers of z).  */
  float32x4_t az = vbslq_f32 (
      SignMask, vreinterpretq_f32_u32 (vandq_u32 (SignMask, red)), z);

  /* Calculate the polynomial approximation.
     Use 2-level Estrin scheme for P(z^2) with deg(P)=7. However,
     a standard implementation using z8 creates spurious underflow
     in the very last fma (when z^8 is small enough).
     Therefore, we split the last fma into a mul and an fma.
     Horner and single-level Estrin have higher errors that exceed
     threshold.  */
  float32x4_t z2 = vmulq_f32 (z, z);
  float32x4_t z4 = vmulq_f32 (z2, z2);

  float32x4_t y = vfmaq_f32 (
      v_pairwise_poly_3_f32 (z2, z4, d->poly), z4,
      vmulq_f32 (z4, v_pairwise_poly_3_f32 (z2, z4, d->poly + 4)));

  /* y = shift + z * P(z^2).  */
  y = vaddq_f32 (vfmaq_f32 (az, y, vmulq_f32 (z2, az)), shift);

  /* y = atan(x) if x>0, -atan(-x) otherwise.  */
  y = vreinterpretq_f32_u32 (veorq_u32 (vreinterpretq_u32_f32 (y), sign));

  return y;
}

PL_SIG (V, F, 1, atan, -10.0, 10.0)
PL_TEST_ULP (V_NAME_F1 (atan), 2.5)
PL_TEST_EXPECT_FENV (V_NAME_F1 (atan), WANT_SIMD_EXCEPT)
PL_TEST_SYM_INTERVAL (V_NAME_F1 (atan), 0, 0x1p-30, 5000)
PL_TEST_SYM_INTERVAL (V_NAME_F1 (atan), 0x1p-30, 1, 40000)
PL_TEST_SYM_INTERVAL (V_NAME_F1 (atan), 1, 0x1p30, 40000)
PL_TEST_SYM_INTERVAL (V_NAME_F1 (atan), 0x1p30, inf, 1000)
