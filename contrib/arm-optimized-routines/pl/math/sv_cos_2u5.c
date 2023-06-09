/*
 * Double-precision SVE cos(x) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define InvPio2 (sv_f64 (0x1.45f306dc9c882p-1))
#define NegPio2_1 (sv_f64 (-0x1.921fb50000000p+0))
#define NegPio2_2 (sv_f64 (-0x1.110b460000000p-26))
#define NegPio2_3 (sv_f64 (-0x1.1a62633145c07p-54))
/* Original shift used in Neon cos,
   plus a contribution to set the bit #0 of q
   as expected by trigonometric instructions.  */
#define Shift (sv_f64 (0x1.8000000000001p52))
#define RangeVal (sv_f64 (0x1p23))
#define AbsMask (0x7fffffffffffffff)

static NOINLINE sv_f64_t
__sv_cos_specialcase (sv_f64_t x, sv_f64_t y, svbool_t cmp)
{
  return sv_call_f64 (cos, x, y, cmp);
}

/* A fast SVE implementation of cos based on trigonometric
   instructions (FTMAD, FTSSEL, FTSMUL).
   Maximum measured error: 2.108 ULPs.
   __sv_cos(0x1.9b0ba158c98f3p+7) got -0x1.fddd4c65c7f07p-3
				 want -0x1.fddd4c65c7f05p-3.  */
sv_f64_t
__sv_cos_x (sv_f64_t x, const svbool_t pg)
{
  sv_f64_t n, r, r2, y;
  svbool_t cmp;

  r = sv_as_f64_u64 (svand_n_u64_x (pg, sv_as_u64_f64 (x), AbsMask));
  cmp = svcmpge_u64 (pg, sv_as_u64_f64 (r), sv_as_u64_f64 (RangeVal));

  /* n = rint(|x|/(pi/2)).  */
  sv_f64_t q = sv_fma_f64_x (pg, InvPio2, r, Shift);
  n = svsub_f64_x (pg, q, Shift);

  /* r = |x| - n*(pi/2)  (range reduction into -pi/4 .. pi/4).  */
  r = sv_fma_f64_x (pg, NegPio2_1, n, r);
  r = sv_fma_f64_x (pg, NegPio2_2, n, r);
  r = sv_fma_f64_x (pg, NegPio2_3, n, r);

  /* cos(r) poly approx.  */
  r2 = svtsmul_f64 (r, sv_as_u64_f64 (q));
  y = sv_f64 (0.0);
  y = svtmad_f64 (y, r2, 7);
  y = svtmad_f64 (y, r2, 6);
  y = svtmad_f64 (y, r2, 5);
  y = svtmad_f64 (y, r2, 4);
  y = svtmad_f64 (y, r2, 3);
  y = svtmad_f64 (y, r2, 2);
  y = svtmad_f64 (y, r2, 1);
  y = svtmad_f64 (y, r2, 0);

  /* Final multiplicative factor: 1.0 or x depending on bit #0 of q.  */
  sv_f64_t f = svtssel_f64 (r, sv_as_u64_f64 (q));
  /* Apply factor.  */
  y = svmul_f64_x (pg, f, y);

  /* No need to pass pg to specialcase here since cmp is a strict subset,
     guaranteed by the cmpge above.  */
  if (unlikely (svptest_any (pg, cmp)))
    return __sv_cos_specialcase (x, y, cmp);
  return y;
}

PL_ALIAS (__sv_cos_x, _ZGVsMxv_cos)

PL_SIG (SV, D, 1, cos, -3.1, 3.1)
PL_TEST_ULP (__sv_cos, 1.61)
PL_TEST_INTERVAL (__sv_cos, 0, 0xffff0000, 10000)
PL_TEST_INTERVAL (__sv_cos, 0x1p-4, 0x1p4, 500000)
#endif
