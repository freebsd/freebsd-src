/*
 * Coefficients and table entries for double-precision cbrt(x).
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

const struct cbrt_data __cbrt_data
  = {.poly = { /* Coefficients for very rough approximation of cbrt(x) in [0.5, 1].
                  See cbrt.sollya for details of generation.  */
	      0x1.c14e8ee44767p-2, 0x1.dd2d3f99e4c0ep-1, -0x1.08e83026b7e74p-1, 0x1.2c74eaa3ba428p-3},
     .table = { /* table[i] = 2^((i - 2) / 3).  */
	         0x1.428a2f98d728bp-1, 0x1.965fea53d6e3dp-1, 0x1p0, 0x1.428a2f98d728bp0, 0x1.965fea53d6e3dp0}};
