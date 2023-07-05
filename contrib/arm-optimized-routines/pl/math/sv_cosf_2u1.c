/*
 * Single-precision SVE cos(x) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define NegPio2_1 (sv_f32 (-0x1.921fb6p+0f))
#define NegPio2_2 (sv_f32 (0x1.777a5cp-25f))
#define NegPio2_3 (sv_f32 (0x1.ee59dap-50f))
#define RangeVal (sv_f32 (0x1p20f))
#define InvPio2 (sv_f32 (0x1.45f306p-1f))
/* Original shift used in Neon cosf,
   plus a contribution to set the bit #0 of q
   as expected by trigonometric instructions.  */
#define Shift (sv_f32 (0x1.800002p+23f))
#define AbsMask (0x7fffffff)

static NOINLINE sv_f32_t
__sv_cosf_specialcase (sv_f32_t x, sv_f32_t y, svbool_t cmp)
{
  return sv_call_f32 (cosf, x, y, cmp);
}

/* A fast SVE implementation of cosf based on trigonometric
   instructions (FTMAD, FTSSEL, FTSMUL).
   Maximum measured error: 2.06 ULPs.
   __sv_cosf(0x1.dea2f2p+19) got 0x1.fffe7ap-6
			    want 0x1.fffe76p-6.  */
sv_f32_t
__sv_cosf_x (sv_f32_t x, const svbool_t pg)
{
  sv_f32_t n, r, r2, y;
  svbool_t cmp;

  r = sv_as_f32_u32 (svand_n_u32_x (pg, sv_as_u32_f32 (x), AbsMask));
  cmp = svcmpge_u32 (pg, sv_as_u32_f32 (r), sv_as_u32_f32 (RangeVal));

  /* n = rint(|x|/(pi/2)).  */
  sv_f32_t q = sv_fma_f32_x (pg, InvPio2, r, Shift);
  n = svsub_f32_x (pg, q, Shift);

  /* r = |x| - n*(pi/2)  (range reduction into -pi/4 .. pi/4).  */
  r = sv_fma_f32_x (pg, NegPio2_1, n, r);
  r = sv_fma_f32_x (pg, NegPio2_2, n, r);
  r = sv_fma_f32_x (pg, NegPio2_3, n, r);

  /* Final multiplicative factor: 1.0 or x depending on bit #0 of q.  */
  sv_f32_t f = svtssel_f32 (r, sv_as_u32_f32 (q));

  /* cos(r) poly approx.  */
  r2 = svtsmul_f32 (r, sv_as_u32_f32 (q));
  y = sv_f32 (0.0f);
  y = svtmad_f32 (y, r2, 4);
  y = svtmad_f32 (y, r2, 3);
  y = svtmad_f32 (y, r2, 2);
  y = svtmad_f32 (y, r2, 1);
  y = svtmad_f32 (y, r2, 0);

  /* Apply factor.  */
  y = svmul_f32_x (pg, f, y);

  /* No need to pass pg to specialcase here since cmp is a strict subset,
     guaranteed by the cmpge above.  */
  if (unlikely (svptest_any (pg, cmp)))
    return __sv_cosf_specialcase (x, y, cmp);
  return y;
}

PL_ALIAS (__sv_cosf_x, _ZGVsMxv_cosf)

PL_SIG (SV, F, 1, cos, -3.1, 3.1)
PL_TEST_ULP (__sv_cosf, 1.57)
PL_TEST_INTERVAL (__sv_cosf, 0, 0xffff0000, 10000)
PL_TEST_INTERVAL (__sv_cosf, 0x1p-4, 0x1p4, 500000)
#endif
