/*
 * Double-precision vector pow function.
 *
 * Copyright (c) 2020, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "mathlib.h"
#include "v_math.h"
#if V_SUPPORTED

VPCS_ATTR
v_f64_t
V_NAME(pow) (v_f64_t x, v_f64_t y)
{
  v_f64_t z;
  for (int lane = 0; lane < v_lanes64 (); lane++)
    {
      f64_t sx = v_get_f64 (x, lane);
      f64_t sy = v_get_f64 (y, lane);
      f64_t sz = pow (sx, sy);
      v_set_f64 (&z, lane, sz);
    }
  return z;
}
VPCS_ALIAS
#endif
