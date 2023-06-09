/*
 * Single-precision polynomial evaluation function for SVE atan(x) and
 * atan2(y,x).
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef PL_MATH_SV_ATANF_COMMON_H
#define PL_MATH_SV_ATANF_COMMON_H

#include "math_config.h"
#include "sv_math.h"

#define P(i) sv_f32 (__atanf_poly_data.poly[i])

/* Polynomial used in fast SVE atanf(x) and atan2f(y,x) implementations
   The order 7 polynomial P approximates (f(sqrt(x))-sqrt(x))/x^(3/2).  */
static inline sv_f32_t
__sv_atanf_common (svbool_t pg, svbool_t red, sv_f32_t z, sv_f32_t az,
		   sv_f32_t shift)
{
  /* Use full Estrin scheme for P(z^2) with deg(P)=7.  */

  /* First compute square powers of z.  */
  sv_f32_t z2 = svmul_f32_x (pg, z, z);
  sv_f32_t z4 = svmul_f32_x (pg, z2, z2);
  sv_f32_t z8 = svmul_f32_x (pg, z4, z4);

  /* Then assemble polynomial.  */
  sv_f32_t p_4_7 = sv_fma_f32_x (pg, z4, (sv_fma_f32_x (pg, z2, P (7), P (6))),
				 (sv_fma_f32_x (pg, z2, P (5), P (4))));
  sv_f32_t p_0_3 = sv_fma_f32_x (pg, z4, (sv_fma_f32_x (pg, z2, P (3), P (2))),
				 (sv_fma_f32_x (pg, z2, P (1), P (0))));
  sv_f32_t y = sv_fma_f32_x (pg, z8, p_4_7, p_0_3);

  /* Finalize. y = shift + z + z^3 * P(z^2).  */
  sv_f32_t z3 = svmul_f32_x (pg, z2, az);
  y = sv_fma_f32_x (pg, y, z3, az);

  /* Apply shift as indicated by 'red' predicate.  */
  y = svadd_f32_m (red, y, shift);

  return y;
}

#endif // PL_MATH_SV_ATANF_COMMON_H
