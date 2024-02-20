/*
 * Single-precision polynomial evaluation function for scalar
 * atan(x) and atan2(y,x).
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef PL_MATH_ATANF_COMMON_H
#define PL_MATH_ATANF_COMMON_H

#include "math_config.h"
#include "poly_scalar_f32.h"

/* Polynomial used in fast atanf(x) and atan2f(y,x) implementations
   The order 7 polynomial P approximates (atan(sqrt(x))-sqrt(x))/x^(3/2).  */
static inline float
eval_poly (float z, float az, float shift)
{
  /* Use 2-level Estrin scheme for P(z^2) with deg(P)=7. However,
     a standard implementation using z8 creates spurious underflow
     in the very last fma (when z^8 is small enough).
     Therefore, we split the last fma into a mul and and an fma.
     Horner and single-level Estrin have higher errors that exceed
     threshold.  */
  float z2 = z * z;
  float z4 = z2 * z2;

  /* Then assemble polynomial.  */
  float y = fmaf (
      z4, z4 * pairwise_poly_3_f32 (z2, z4, __atanf_poly_data.poly + 4),
      pairwise_poly_3_f32 (z2, z4, __atanf_poly_data.poly));
  /* Finalize:
     y = shift + z * P(z^2).  */
  return fmaf (y, z2 * az, az) + shift;
}

#endif // PL_MATH_ATANF_COMMON_H
