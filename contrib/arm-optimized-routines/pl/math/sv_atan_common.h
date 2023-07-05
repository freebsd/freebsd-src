/*
 * Double-precision polynomial evaluation function for SVE atan(x) and
 * atan2(y,x).
 *
 * Copyright (c) 2021-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"
#include "sv_math.h"

#define P(i) sv_f64 (__atan_poly_data.poly[i])

/* Polynomial used in fast SVE atan(x) and atan2(y,x) implementations
   The order 19 polynomial P approximates (atan(sqrt(x))-sqrt(x))/x^(3/2).  */
static inline sv_f64_t
__sv_atan_common (svbool_t pg, svbool_t red, sv_f64_t z, sv_f64_t az,
		  sv_f64_t shift)
{
  /* Use full Estrin scheme for P(z^2) with deg(P)=19.  */
  sv_f64_t z2 = svmul_f64_x (pg, z, z);

  /* Level 1.  */
  sv_f64_t P_1_0 = sv_fma_f64_x (pg, P (1), z2, P (0));
  sv_f64_t P_3_2 = sv_fma_f64_x (pg, P (3), z2, P (2));
  sv_f64_t P_5_4 = sv_fma_f64_x (pg, P (5), z2, P (4));
  sv_f64_t P_7_6 = sv_fma_f64_x (pg, P (7), z2, P (6));
  sv_f64_t P_9_8 = sv_fma_f64_x (pg, P (9), z2, P (8));
  sv_f64_t P_11_10 = sv_fma_f64_x (pg, P (11), z2, P (10));
  sv_f64_t P_13_12 = sv_fma_f64_x (pg, P (13), z2, P (12));
  sv_f64_t P_15_14 = sv_fma_f64_x (pg, P (15), z2, P (14));
  sv_f64_t P_17_16 = sv_fma_f64_x (pg, P (17), z2, P (16));
  sv_f64_t P_19_18 = sv_fma_f64_x (pg, P (19), z2, P (18));

  /* Level 2.  */
  sv_f64_t x2 = svmul_f64_x (pg, z2, z2);
  sv_f64_t P_3_0 = sv_fma_f64_x (pg, P_3_2, x2, P_1_0);
  sv_f64_t P_7_4 = sv_fma_f64_x (pg, P_7_6, x2, P_5_4);
  sv_f64_t P_11_8 = sv_fma_f64_x (pg, P_11_10, x2, P_9_8);
  sv_f64_t P_15_12 = sv_fma_f64_x (pg, P_15_14, x2, P_13_12);
  sv_f64_t P_19_16 = sv_fma_f64_x (pg, P_19_18, x2, P_17_16);

  /* Level 3.  */
  sv_f64_t x4 = svmul_f64_x (pg, x2, x2);
  sv_f64_t P_7_0 = sv_fma_f64_x (pg, P_7_4, x4, P_3_0);
  sv_f64_t P_15_8 = sv_fma_f64_x (pg, P_15_12, x4, P_11_8);

  /* Level 4.  */
  sv_f64_t x8 = svmul_f64_x (pg, x4, x4);
  sv_f64_t y = sv_fma_f64_x (pg, P_19_16, x8, P_15_8);
  y = sv_fma_f64_x (pg, y, x8, P_7_0);

  /* Finalize. y = shift + z + z^3 * P(z^2).  */
  sv_f64_t z3 = svmul_f64_x (pg, z2, az);
  y = sv_fma_f64_x (pg, y, z3, az);

  /* Apply shift as indicated by `red` predicate.  */
  y = svadd_f64_m (red, y, shift);

  return y;
}
