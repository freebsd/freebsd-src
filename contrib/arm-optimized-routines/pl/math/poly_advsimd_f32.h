/*
 * Helpers for evaluating polynomials on single-precision AdvSIMD input, using
 * various schemes.
 *
 * Copyright (c) 2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef PL_MATH_POLY_ADVSIMD_F32_H
#define PL_MATH_POLY_ADVSIMD_F32_H

#include <arm_neon.h>

/* Wrap AdvSIMD f32 helpers: evaluation of some scheme/order has form:
   v_[scheme]_[order]_f32.  */
#define VTYPE float32x4_t
#define FMA(x, y, z) vfmaq_f32 (z, x, y)
#define VWRAP(f) v_##f##_f32
#include "poly_generic.h"
#undef VWRAP
#undef FMA
#undef VTYPE

#endif
