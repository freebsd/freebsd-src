/*
 * Double-precision vector sin function.
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "test_defs.h"
#include "test_sig.h"
#include "mathlib.h"
#include "v_math.h"

static const struct data
{
  float64x2_t poly[7];
  float64x2_t range_val, inv_pi, pi_1, pi_2, pi_3;
} data = {
  .poly = { V2 (-0x1.555555555547bp-3), V2 (0x1.1111111108a4dp-7),
	    V2 (-0x1.a01a019936f27p-13), V2 (0x1.71de37a97d93ep-19),
	    V2 (-0x1.ae633919987c6p-26), V2 (0x1.60e277ae07cecp-33),
	    V2 (-0x1.9e9540300a1p-41) },

  .range_val = V2 (0x1p23),
  .inv_pi = V2 (0x1.45f306dc9c883p-2),
  .pi_1 = V2 (0x1.921fb54442d18p+1),
  .pi_2 = V2 (0x1.1a62633145c06p-53),
  .pi_3 = V2 (0x1.c1cd129024e09p-106),
};

#if WANT_SIMD_EXCEPT
/* asuint64(0x1p-253)), below which multiply by inv_pi underflows.  */
# define TinyBound v_u64 (0x3020000000000000)
/* RangeVal - TinyBound.  */
# define Thresh v_u64 (0x1160000000000000)
#endif

#define C(i) d->poly[i]

static float64x2_t VPCS_ATTR NOINLINE
special_case (float64x2_t x, float64x2_t y, uint64x2_t odd, uint64x2_t cmp)
{
  y = vreinterpretq_f64_u64 (veorq_u64 (vreinterpretq_u64_f64 (y), odd));
  return v_call_f64 (sin, x, y, cmp);
}

/* Vector (AdvSIMD) sin approximation.
   Maximum observed error in [-pi/2, pi/2], where argument is not reduced,
   is 2.87 ULP:
   _ZGVnN2v_sin (0x1.921d5c6a07142p+0) got 0x1.fffffffa7dc02p-1
				      want 0x1.fffffffa7dc05p-1
   Maximum observed error in the entire non-special domain ([-2^23, 2^23])
   is 3.22 ULP:
   _ZGVnN2v_sin (0x1.5702447b6f17bp+22) got 0x1.ffdcd125c84fbp-3
				       want 0x1.ffdcd125c84f8p-3.  */
float64x2_t VPCS_ATTR V_NAME_D1 (sin) (float64x2_t x)
{
  const struct data *d = ptr_barrier (&data);
  float64x2_t n, r, r2, r3, r4, y, t1, t2, t3;
  uint64x2_t odd, cmp;

#if WANT_SIMD_EXCEPT
  /* Detect |x| <= TinyBound or |x| >= RangeVal. If fenv exceptions are to be
     triggered correctly, set any special lanes to 1 (which is neutral w.r.t.
     fenv). These lanes will be fixed by special-case handler later.  */
  uint64x2_t ir = vreinterpretq_u64_f64 (vabsq_f64 (x));
  cmp = vcgeq_u64 (vsubq_u64 (ir, TinyBound), Thresh);
  r = vreinterpretq_f64_u64 (vbicq_u64 (vreinterpretq_u64_f64 (x), cmp));
#else
  r = x;
  cmp = vcageq_f64 (x, d->range_val);
#endif

  /* n = rint(|x|/pi).  */
  n = vrndaq_f64 (vmulq_f64 (r, d->inv_pi));
  odd = vshlq_n_u64 (vreinterpretq_u64_s64 (vcvtq_s64_f64 (n)), 63);

  /* r = |x| - n*pi  (range reduction into -pi/2 .. pi/2).  */
  r = vfmsq_f64 (r, d->pi_1, n);
  r = vfmsq_f64 (r, d->pi_2, n);
  r = vfmsq_f64 (r, d->pi_3, n);

  /* sin(r) poly approx.  */
  r2 = vmulq_f64 (r, r);
  r3 = vmulq_f64 (r2, r);
  r4 = vmulq_f64 (r2, r2);

  t1 = vfmaq_f64 (C (4), C (5), r2);
  t2 = vfmaq_f64 (C (2), C (3), r2);
  t3 = vfmaq_f64 (C (0), C (1), r2);

  y = vfmaq_f64 (t1, C (6), r4);
  y = vfmaq_f64 (t2, y, r4);
  y = vfmaq_f64 (t3, y, r4);
  y = vfmaq_f64 (r, y, r3);

  if (unlikely (v_any_u64 (cmp)))
    return special_case (x, y, odd, cmp);
  return vreinterpretq_f64_u64 (veorq_u64 (vreinterpretq_u64_f64 (y), odd));
}

TEST_SIG (V, D, 1, sin, -3.1, 3.1)
TEST_ULP (V_NAME_D1 (sin), 3.0)
TEST_DISABLE_FENV_IF_NOT (V_NAME_D1 (sin), WANT_SIMD_EXCEPT)
TEST_SYM_INTERVAL (V_NAME_D1 (sin), 0, 0x1p23, 500000)
TEST_SYM_INTERVAL (V_NAME_D1 (sin), 0x1p23, inf, 10000)
