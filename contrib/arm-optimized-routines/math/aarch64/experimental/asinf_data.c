/*
 * Coefficients for single-precision asin(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* Approximate asinf(x) directly in [0x1p-24, 0.25]. See for tools/asinf.sollya
   for these coeffs were generated.  */
const float __asinf_poly[] = {
  /* Polynomial approximation of  (asin(sqrt(x)) - sqrt(x)) / (x * sqrt(x))  on
     [ 0x1p-24 0x1p-2 ] order = 4 rel error: 0x1.00a23bbp-29 .  */
  0x1.55555ep-3, 0x1.33261ap-4, 0x1.70d7dcp-5, 0x1.b059dp-6, 0x1.3af7d8p-5,
};
