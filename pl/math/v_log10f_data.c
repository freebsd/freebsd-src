/*
 * Coefficients for single-precision vector log10 function.
 *
 * Copyright (c) 2020-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "math_config.h"

const float __v_log10f_poly[] = {
  /* Use order 9 for log10(1+x), i.e. order 8 for log10(1+x)/x, with x in
     [-1/3, 1/3] (offset=2/3). Max. relative error: 0x1.068ee468p-25.  */
  -0x1.bcb79cp-3f, 0x1.2879c8p-3f, -0x1.bcd472p-4f, 0x1.6408f8p-4f,
  -0x1.246f8p-4f,  0x1.f0e514p-5f, -0x1.0fc92cp-4f, 0x1.f5f76ap-5f};
