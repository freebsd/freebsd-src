/*
 * Coefficients and helpers for double-precision vector tan(x) function.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "math_config.h"

const struct v_tan_data __v_tan_data
  = {.neg_half_pi_hi = -0x1.921fb54442d18p0,
     .neg_half_pi_lo = -0x1.1a62633145c07p-54,
     .poly
     = {0x1.5555555555556p-2, 0x1.1111111110a63p-3, 0x1.ba1ba1bb46414p-5,
	0x1.664f47e5b5445p-6, 0x1.226e5e5ecdfa3p-7, 0x1.d6c7ddbf87047p-9,
	0x1.7ea75d05b583ep-10, 0x1.289f22964a03cp-11, 0x1.4e4fd14147622p-12}};
