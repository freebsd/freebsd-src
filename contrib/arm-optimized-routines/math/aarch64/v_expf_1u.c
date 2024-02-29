/*
 * Single-precision vector e^x function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "v_math.h"

static const float Poly[] = {
  /*  maxerr: 0.36565 +0.5 ulp.  */
  0x1.6a6000p-10f,
  0x1.12718ep-7f,
  0x1.555af0p-5f,
  0x1.555430p-3f,
  0x1.fffff4p-2f,
};
#define C0 v_f32 (Poly[0])
#define C1 v_f32 (Poly[1])
#define C2 v_f32 (Poly[2])
#define C3 v_f32 (Poly[3])
#define C4 v_f32 (Poly[4])

#define Shift v_f32 (0x1.8p23f)
#define InvLn2 v_f32 (0x1.715476p+0f)
#define Ln2hi v_f32 (0x1.62e4p-1f)
#define Ln2lo v_f32 (0x1.7f7d1cp-20f)

static float32x4_t VPCS_ATTR NOINLINE
specialcase (float32x4_t poly, float32x4_t n, uint32x4_t e, float32x4_t absn)
{
  /* 2^n may overflow, break it up into s1*s2.  */
  uint32x4_t b = (n <= v_f32 (0.0f)) & v_u32 (0x83000000);
  float32x4_t s1 = vreinterpretq_f32_u32 (v_u32 (0x7f000000) + b);
  float32x4_t s2 = vreinterpretq_f32_u32 (e - b);
  uint32x4_t cmp = absn > v_f32 (192.0f);
  float32x4_t r1 = s1 * s1;
  float32x4_t r0 = poly * s1 * s2;
  return vreinterpretq_f32_u32 ((cmp & vreinterpretq_u32_f32 (r1))
				| (~cmp & vreinterpretq_u32_f32 (r0)));
}

float32x4_t VPCS_ATTR
_ZGVnN4v_expf_1u (float32x4_t x)
{
  float32x4_t n, r, scale, poly, absn, z;
  uint32x4_t cmp, e;

  /* exp(x) = 2^n * poly(r), with poly(r) in [1/sqrt(2),sqrt(2)]
     x = ln2*n + r, with r in [-ln2/2, ln2/2].  */
#if 1
  z = vfmaq_f32 (Shift, x, InvLn2);
  n = z - Shift;
  r = vfmaq_f32 (x, n, -Ln2hi);
  r = vfmaq_f32 (r, n, -Ln2lo);
  e = vreinterpretq_u32_f32 (z) << 23;
#else
  z = x * InvLn2;
  n = vrndaq_f32 (z);
  r = vfmaq_f32 (x, n, -Ln2hi);
  r = vfmaq_f32 (r, n, -Ln2lo);
  e = vreinterpretq_u32_s32 (vcvtaq_s32_f32 (z)) << 23;
#endif
  scale = vreinterpretq_f32_u32 (e + v_u32 (0x3f800000));
  absn = vabsq_f32 (n);
  cmp = absn > v_f32 (126.0f);
  poly = vfmaq_f32 (C1, C0, r);
  poly = vfmaq_f32 (C2, poly, r);
  poly = vfmaq_f32 (C3, poly, r);
  poly = vfmaq_f32 (C4, poly, r);
  poly = vfmaq_f32 (v_f32 (1.0f), poly, r);
  poly = vfmaq_f32 (v_f32 (1.0f), poly, r);
  if (unlikely (v_any_u32 (cmp)))
    return specialcase (poly, n, e, absn);
  return scale * poly;
}
