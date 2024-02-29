/*
 * Single-precision vector cbrt(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "pl_sig.h"
#include "pl_test.h"
#include "poly_advsimd_f32.h"

const static struct data
{
  float32x4_t poly[4], one_third;
  float table[5];
} data = {
  .poly = { /* Very rough approximation of cbrt(x) in [0.5, 1], generated with
               FPMinimax.  */
	    V4 (0x1.c14e96p-2), V4 (0x1.dd2d3p-1), V4 (-0x1.08e81ap-1),
	    V4 (0x1.2c74c2p-3) },
  .table = { /* table[i] = 2^((i - 2) / 3).  */
	    0x1.428a3p-1, 0x1.965feap-1, 0x1p0, 0x1.428a3p0, 0x1.965feap0 },
  .one_third = V4 (0x1.555556p-2f),
};

#define SignMask v_u32 (0x80000000)
#define SmallestNormal v_u32 (0x00800000)
#define Thresh vdup_n_u16 (0x7f00) /* asuint(INFINITY) - SmallestNormal.  */
#define MantissaMask v_u32 (0x007fffff)
#define HalfExp v_u32 (0x3f000000)

static float32x4_t VPCS_ATTR NOINLINE
special_case (float32x4_t x, float32x4_t y, uint16x4_t special)
{
  return v_call_f32 (cbrtf, x, y, vmovl_u16 (special));
}

static inline float32x4_t
shifted_lookup (const float *table, int32x4_t i)
{
  return (float32x4_t){ table[i[0] + 2], table[i[1] + 2], table[i[2] + 2],
			table[i[3] + 2] };
}

/* Approximation for vector single-precision cbrt(x) using Newton iteration
   with initial guess obtained by a low-order polynomial. Greatest error
   is 1.64 ULP. This is observed for every value where the mantissa is
   0x1.85a2aa and the exponent is a multiple of 3, for example:
   _ZGVnN4v_cbrtf(0x1.85a2aap+3) got 0x1.267936p+1
				want 0x1.267932p+1.  */
VPCS_ATTR float32x4_t V_NAME_F1 (cbrt) (float32x4_t x)
{
  const struct data *d = ptr_barrier (&data);
  uint32x4_t iax = vreinterpretq_u32_f32 (vabsq_f32 (x));

  /* Subnormal, +/-0 and special values.  */
  uint16x4_t special = vcge_u16 (vsubhn_u32 (iax, SmallestNormal), Thresh);

  /* Decompose |x| into m * 2^e, where m is in [0.5, 1.0]. This is a vector
     version of frexpf, which gets subnormal values wrong - these have to be
     special-cased as a result.  */
  float32x4_t m = vbslq_f32 (MantissaMask, x, v_f32 (0.5));
  int32x4_t e
      = vsubq_s32 (vreinterpretq_s32_u32 (vshrq_n_u32 (iax, 23)), v_s32 (126));

  /* p is a rough approximation for cbrt(m) in [0.5, 1.0]. The better this is,
     the less accurate the next stage of the algorithm needs to be. An order-4
     polynomial is enough for one Newton iteration.  */
  float32x4_t p = v_pairwise_poly_3_f32 (m, vmulq_f32 (m, m), d->poly);

  float32x4_t one_third = d->one_third;
  float32x4_t two_thirds = vaddq_f32 (one_third, one_third);

  /* One iteration of Newton's method for iteratively approximating cbrt.  */
  float32x4_t m_by_3 = vmulq_f32 (m, one_third);
  float32x4_t a
      = vfmaq_f32 (vdivq_f32 (m_by_3, vmulq_f32 (p, p)), two_thirds, p);

  /* Assemble the result by the following:

     cbrt(x) = cbrt(m) * 2 ^ (e / 3).

     We can get 2 ^ round(e / 3) using ldexp and integer divide, but since e is
     not necessarily a multiple of 3 we lose some information.

     Let q = 2 ^ round(e / 3), then t = 2 ^ (e / 3) / q.

     Then we know t = 2 ^ (i / 3), where i is the remainder from e / 3, which
     is an integer in [-2, 2], and can be looked up in the table T. Hence the
     result is assembled as:

     cbrt(x) = cbrt(m) * t * 2 ^ round(e / 3) * sign.  */
  float32x4_t ef = vmulq_f32 (vcvtq_f32_s32 (e), one_third);
  int32x4_t ey = vcvtq_s32_f32 (ef);
  int32x4_t em3 = vsubq_s32 (e, vmulq_s32 (ey, v_s32 (3)));

  float32x4_t my = shifted_lookup (d->table, em3);
  my = vmulq_f32 (my, a);

  /* Vector version of ldexpf.  */
  float32x4_t y
      = vreinterpretq_f32_s32 (vshlq_n_s32 (vaddq_s32 (ey, v_s32 (127)), 23));
  y = vmulq_f32 (y, my);

  if (unlikely (v_any_u16h (special)))
    return special_case (x, vbslq_f32 (SignMask, x, y), special);

  /* Copy sign.  */
  return vbslq_f32 (SignMask, x, y);
}

PL_SIG (V, F, 1, cbrt, -10.0, 10.0)
PL_TEST_ULP (V_NAME_F1 (cbrt), 1.15)
PL_TEST_EXPECT_FENV_ALWAYS (V_NAME_F1 (cbrt))
PL_TEST_SYM_INTERVAL (V_NAME_F1 (cbrt), 0, inf, 1000000)
