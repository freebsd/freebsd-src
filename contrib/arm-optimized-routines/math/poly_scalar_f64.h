/*
 * Helpers for evaluating polynomials on double-precision scalar input, using
 * various schemes.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef MATH_POLY_SCALAR_F64_H
#define MATH_POLY_SCALAR_F64_H

#include <math.h>

/* Wrap scalar f64 helpers: evaluation of some scheme/order has form:
   [scheme]_[order]_f64.  */
#define VTYPE double
#define FMA fma
#define VWRAP(f) f##_f64
#include "poly_generic.h"
#undef VWRAP
#undef FMA
#undef VTYPE

#endif
