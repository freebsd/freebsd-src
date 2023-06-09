/*
 * Single-precision vector tan(x) function.
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "v_math.h"
#include "estrinf.h"
#include "pl_sig.h"
#include "pl_test.h"

#if V_SUPPORTED

/* Constants.  */
#define NegPio2_1 (v_f32 (-0x1.921fb6p+0f))
#define NegPio2_2 (v_f32 (0x1.777a5cp-25f))
#define NegPio2_3 (v_f32 (0x1.ee59dap-50f))
#define InvPio2 (v_f32 (0x1.45f306p-1f))
#define RangeVal (0x47000000)  /* asuint32(0x1p15f).  */
#define TinyBound (0x30000000) /* asuint32 (0x1p-31).  */
#define Shift (v_f32 (0x1.8p+23f))
#define AbsMask (v_u32 (0x7fffffff))

#define poly(i) v_f32 (__tanf_poly_data.poly_tan[i])

/* Special cases (fall back to scalar calls).  */
VPCS_ATTR
NOINLINE static v_f32_t
specialcase (v_f32_t x, v_f32_t y, v_u32_t cmp)
{
  return v_call_f32 (tanf, x, y, cmp);
}

/* Use a full Estrin scheme to evaluate polynomial.  */
static inline v_f32_t
eval_poly (v_f32_t z)
{
  v_f32_t z2 = z * z;
#if WANT_SIMD_EXCEPT
  /* Tiny z (<= 0x1p-31) will underflow when calculating z^4. If fp exceptions
     are to be triggered correctly, sidestep this by fixing such lanes to 0.  */
  v_u32_t will_uflow = v_cond_u32 ((v_as_u32_f32 (z) & AbsMask) <= TinyBound);
  if (unlikely (v_any_u32 (will_uflow)))
    z2 = v_sel_f32 (will_uflow, v_f32 (0), z2);
#endif
  v_f32_t z4 = z2 * z2;
  return ESTRIN_5 (z, z2, z4, poly);
}

/* Fast implementation of Neon tanf.
   Maximum error is 3.45 ULP:
   __v_tanf(-0x1.e5f0cap+13) got 0x1.ff9856p-1
			    want 0x1.ff9850p-1.  */
VPCS_ATTR
v_f32_t V_NAME (tanf) (v_f32_t x)
{
  v_f32_t special_arg = x;
  v_u32_t ix = v_as_u32_f32 (x);
  v_u32_t iax = ix & AbsMask;

  /* iax >= RangeVal means x, if not inf or NaN, is too large to perform fast
     regression.  */
#if WANT_SIMD_EXCEPT
  /* If fp exceptions are to be triggered correctly, also special-case tiny
     input, as this will load to overflow later. Fix any special lanes to 1 to
     prevent any exceptions being triggered.  */
  v_u32_t special = v_cond_u32 (iax - TinyBound >= RangeVal - TinyBound);
  if (unlikely (v_any_u32 (special)))
    x = v_sel_f32 (special, v_f32 (1.0f), x);
#else
  /* Otherwise, special-case large and special values.  */
  v_u32_t special = v_cond_u32 (iax >= RangeVal);
#endif

  /* n = rint(x/(pi/2)).  */
  v_f32_t q = v_fma_f32 (InvPio2, x, Shift);
  v_f32_t n = q - Shift;
  /* n is representable as a signed integer, simply convert it.  */
  v_s32_t in = v_round_s32 (n);
  /* Determine if x lives in an interval, where |tan(x)| grows to infinity.  */
  v_s32_t alt = in & 1;
  v_u32_t pred_alt = (alt != 0);

  /* r = x - n * (pi/2)  (range reduction into -pi./4 .. pi/4).  */
  v_f32_t r;
  r = v_fma_f32 (NegPio2_1, n, x);
  r = v_fma_f32 (NegPio2_2, n, r);
  r = v_fma_f32 (NegPio2_3, n, r);

  /* If x lives in an interval, where |tan(x)|
     - is finite, then use a polynomial approximation of the form
       tan(r) ~ r + r^3 * P(r^2) = r + r * r^2 * P(r^2).
     - grows to infinity then use symmetries of tangent and the identity
       tan(r) = cotan(pi/2 - r) to express tan(x) as 1/tan(-r). Finally, use
       the same polynomial approximation of tan as above.  */

  /* Perform additional reduction if required.  */
  v_f32_t z = v_sel_f32 (pred_alt, -r, r);

  /* Evaluate polynomial approximation of tangent on [-pi/4, pi/4].  */
  v_f32_t z2 = r * r;
  v_f32_t p = eval_poly (z2);
  v_f32_t y = v_fma_f32 (z * z2, p, z);

  /* Compute reciprocal and apply if required.  */
  v_f32_t inv_y = v_div_f32 (v_f32 (1.0f), y);
  y = v_sel_f32 (pred_alt, inv_y, y);

  /* Fast reduction does not handle the x = -0.0 case well,
     therefore it is fixed here.  */
  y = v_sel_f32 (x == v_f32 (-0.0), x, y);

  if (unlikely (v_any_u32 (special)))
    return specialcase (special_arg, y, special);
  return y;
}
VPCS_ALIAS

PL_SIG (V, F, 1, tan, -3.1, 3.1)
PL_TEST_ULP (V_NAME (tanf), 2.96)
PL_TEST_EXPECT_FENV (V_NAME (tanf), WANT_SIMD_EXCEPT)
PL_TEST_INTERVAL (V_NAME (tanf), -0.0, -0x1p126, 100)
PL_TEST_INTERVAL (V_NAME (tanf), 0x1p-149, 0x1p-126, 4000)
PL_TEST_INTERVAL (V_NAME (tanf), 0x1p-126, 0x1p-23, 50000)
PL_TEST_INTERVAL (V_NAME (tanf), 0x1p-23, 0.7, 50000)
PL_TEST_INTERVAL (V_NAME (tanf), 0.7, 1.5, 50000)
PL_TEST_INTERVAL (V_NAME (tanf), 1.5, 100, 50000)
PL_TEST_INTERVAL (V_NAME (tanf), 100, 0x1p17, 50000)
PL_TEST_INTERVAL (V_NAME (tanf), 0x1p17, inf, 50000)
#endif
