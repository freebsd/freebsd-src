/*
 * Coefficients for single-precision asin(x) function.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* Approximate asin(x) directly in [0x1p-106, 0.25]. See tools/asin.sollya
   for these coeffcients were generated.  */
const double __asin_poly[] = {
  /* Polynomial approximation of  (asin(sqrt(x)) - sqrt(x)) / (x * sqrt(x))
     on [ 0x1p-106, 0x1p-2 ], relative error: 0x1.c3d8e169p-57.  */
  0x1.555555555554ep-3, 0x1.3333333337233p-4,  0x1.6db6db67f6d9fp-5,
  0x1.f1c71fbd29fbbp-6, 0x1.6e8b264d467d6p-6,  0x1.1c5997c357e9dp-6,
  0x1.c86a22cd9389dp-7, 0x1.856073c22ebbep-7,  0x1.fd1151acb6bedp-8,
  0x1.087182f799c1dp-6, -0x1.6602748120927p-7, 0x1.cfa0dd1f9478p-6,
};
