/*
 * Helpers for evaluating polynomials on siongle-precision scalar input, using
 * various schemes.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef MATH_POLY_SCALAR_F32_H
#define MATH_POLY_SCALAR_F32_H

#include <math.h>

/* Wrap scalar f32 helpers: evaluation of some scheme/order has form:
   [scheme]_[order]_f32.  */
#define VTYPE float
#define FMA fmaf
#define VWRAP(f) f##_f32
#include "poly_generic.h"
#undef VWRAP
#undef FMA
#undef VTYPE

#endif
