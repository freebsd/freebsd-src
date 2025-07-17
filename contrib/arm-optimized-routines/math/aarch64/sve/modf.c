/*
 * Double-precision SVE modf(x, *y) function.
 *
 * Copyright (c) 2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "sv_math.h"
#include "test_sig.h"
#include "test_defs.h"

/* Modf algorithm. Produces exact values in all rounding modes.  */
svfloat64_t SV_NAME_D1_L1 (modf) (svfloat64_t x, double *out_int,
				  const svbool_t pg)
{
  /* Get integer component of x.  */
  svfloat64_t fint_comp = svrintz_x (pg, x);

  svst1_f64 (pg, out_int, fint_comp);

  /* Subtract integer component from input.  */
  svfloat64_t remaining = svsub_f64_x (svptrue_b64 (), x, fint_comp);

  /* Return +0 for integer x.  */
  svbool_t is_integer = svcmpeq (pg, x, fint_comp);
  return svsel (is_integer, sv_f64 (0), remaining);
}

TEST_ULP (_ZGVsMxvl8_modf_frac, 0.0)
TEST_SYM_INTERVAL (_ZGVsMxvl8_modf_frac, 0, 1, 20000)
TEST_SYM_INTERVAL (_ZGVsMxvl8_modf_frac, 1, inf, 20000)

TEST_ULP (_ZGVsMxvl8_modf_int, 0.0)
TEST_SYM_INTERVAL (_ZGVsMxvl8_modf_int, 0, 1, 20000)
TEST_SYM_INTERVAL (_ZGVsMxvl8_modf_int, 1, inf, 20000)
CLOSE_SVE_ATTR
