/*
 * Single-precision polynomial evaluation function for scalar and vector
 * atan(x) and atan2(y,x).
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef PL_MATH_ATANF_COMMON_H
#define PL_MATH_ATANF_COMMON_H

#include "math_config.h"
#include "estrinf.h"

#if V_SUPPORTED

#include "v_math.h"

#define FLT_T v_f32_t
#define P(i) v_f32 (__atanf_poly_data.poly[i])

#else

#define FLT_T float
#define P(i) __atanf_poly_data.poly[i]

#endif

/* Polynomial used in fast atanf(x) and atan2f(y,x) implementations
   The order 7 polynomial P approximates (atan(sqrt(x))-sqrt(x))/x^(3/2).  */
static inline FLT_T
eval_poly (FLT_T z, FLT_T az, FLT_T shift)
{
  /* Use 2-level Estrin scheme for P(z^2) with deg(P)=7. However,
     a standard implementation using z8 creates spurious underflow
     in the very last fma (when z^8 is small enough).
     Therefore, we split the last fma into a mul and and an fma.
     Horner and single-level Estrin have higher errors that exceed
     threshold.  */
  FLT_T z2 = z * z;
  FLT_T z4 = z2 * z2;

  /* Then assemble polynomial.  */
  FLT_T y = FMA (z4, z4 * ESTRIN_3_ (z2, z4, P, 4), ESTRIN_3 (z2, z4, P));

  /* Finalize:
     y = shift + z * P(z^2).  */
  return FMA (y, z2 * az, az) + shift;
}

#endif // PL_MATH_ATANF_COMMON_H
