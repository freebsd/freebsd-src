/*
 * Single-precision SVE sin(x) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "pl_sig.h"
#include "pl_test.h"

#if SV_SUPPORTED

#define A3 (sv_f32 (__sv_sinf_data.coeffs[3]))
#define A5 (sv_f32 (__sv_sinf_data.coeffs[2]))
#define A7 (sv_f32 (__sv_sinf_data.coeffs[1]))
#define A9 (sv_f32 (__sv_sinf_data.coeffs[0]))

#define NegPi1 (sv_f32 (-0x1.921fb6p+1f))
#define NegPi2 (sv_f32 (0x1.777a5cp-24f))
#define NegPi3 (sv_f32 (0x1.ee59dap-49f))
#define RangeVal (sv_f32 (0x1p20f))
#define InvPi (sv_f32 (0x1.45f306p-2f))
#define Shift (sv_f32 (0x1.8p+23f))
#define AbsMask (0x7fffffff)

static NOINLINE sv_f32_t
__sv_sinf_specialcase (sv_f32_t x, sv_f32_t y, svbool_t cmp)
{
  return sv_call_f32 (sinf, x, y, cmp);
}

/* A fast SVE implementation of sinf.
   Maximum error: 1.89 ULPs.
   This maximum error is achieved at multiple values in [-2^18, 2^18]
   but one example is:
   __sv_sinf(0x1.9247a4p+0) got 0x1.fffff6p-1 want 0x1.fffffap-1.  */
sv_f32_t
__sv_sinf_x (sv_f32_t x, const svbool_t pg)
{
  sv_f32_t n, r, r2, y;
  sv_u32_t sign, odd;
  svbool_t cmp;

  r = sv_as_f32_u32 (svand_n_u32_x (pg, sv_as_u32_f32 (x), AbsMask));
  sign = svand_n_u32_x (pg, sv_as_u32_f32 (x), ~AbsMask);
  cmp = svcmpge_u32 (pg, sv_as_u32_f32 (r), sv_as_u32_f32 (RangeVal));

  /* n = rint(|x|/pi).  */
  n = sv_fma_f32_x (pg, InvPi, r, Shift);
  odd = svlsl_n_u32_x (pg, sv_as_u32_f32 (n), 31);
  n = svsub_f32_x (pg, n, Shift);

  /* r = |x| - n*pi  (range reduction into -pi/2 .. pi/2).  */
  r = sv_fma_f32_x (pg, NegPi1, n, r);
  r = sv_fma_f32_x (pg, NegPi2, n, r);
  r = sv_fma_f32_x (pg, NegPi3, n, r);

  /* sin(r) approx using a degree 9 polynomial from the Taylor series
     expansion. Note that only the odd terms of this are non-zero.  */
  r2 = svmul_f32_x (pg, r, r);
  y = sv_fma_f32_x (pg, A9, r2, A7);
  y = sv_fma_f32_x (pg, y, r2, A5);
  y = sv_fma_f32_x (pg, y, r2, A3);
  y = sv_fma_f32_x (pg, svmul_f32_x (pg, y, r2), r, r);

  /* sign = y^sign^odd.  */
  y = sv_as_f32_u32 (
    sveor_u32_x (pg, sv_as_u32_f32 (y), sveor_u32_x (pg, sign, odd)));

  /* No need to pass pg to specialcase here since cmp is a strict subset,
     guaranteed by the cmpge above.  */
  if (unlikely (svptest_any (pg, cmp)))
    return __sv_sinf_specialcase (x, y, cmp);
  return y;
}

PL_ALIAS (__sv_sinf_x, _ZGVsMxv_sinf)

PL_SIG (SV, F, 1, sin, -3.1, 3.1)
PL_TEST_ULP (__sv_sinf, 1.40)
PL_TEST_INTERVAL (__sv_sinf, 0, 0xffff0000, 10000)
PL_TEST_INTERVAL (__sv_sinf, 0x1p-4, 0x1p4, 500000)
#endif
