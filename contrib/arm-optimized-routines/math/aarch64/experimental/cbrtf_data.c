/*
 * Coefficients and table entries for single-precision cbrt(x).
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

const struct cbrtf_data __cbrtf_data
  = {.poly = { /* Coefficients for very rough approximation of cbrt(x) in [0.5, 1].
                  See cbrtf.sollya for details of generation.  */
	        0x1.c14e96p-2, 0x1.dd2d3p-1, -0x1.08e81ap-1, 0x1.2c74c2p-3},
     .table = { /* table[i] = 2^((i - 2) / 3).  */
	        0x1.428a3p-1, 0x1.965feap-1, 0x1p0, 0x1.428a3p0, 0x1.965feap0}};
