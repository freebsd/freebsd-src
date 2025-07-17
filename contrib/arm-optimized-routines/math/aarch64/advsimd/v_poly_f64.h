/*
 * Helpers for evaluating polynomials on double-precision AdvSIMD input, using
 * various schemes.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef MATH_POLY_ADVSIMD_F64_H
#define MATH_POLY_ADVSIMD_F64_H

#include <arm_neon.h>

/* Wrap AdvSIMD f64 helpers: evaluation of some scheme/order has form:
   v_[scheme]_[order]_f64.  */
#define VTYPE float64x2_t
#define FMA(x, y, z) vfmaq_f64 (z, x, y)
#define VWRAP(f) v_##f##_f64
#include "poly_generic.h"
#undef VWRAP
#undef FMA
#undef VTYPE

#endif
