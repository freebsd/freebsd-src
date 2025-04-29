/*
 * Helpers for evaluating polynomials on double-precision SVE input, using
 * various schemes.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef MATH_POLY_SVE_F64_H
#define MATH_POLY_SVE_F64_H

#include <arm_sve.h>

/* Wrap SVE f64 helpers: evaluation of some scheme/order has form:
   sv_[scheme]_[order]_f64_x.  */
#define VTYPE svfloat64_t
#define STYPE double
#define VWRAP(f) sv_##f##_f64_x
#define DUP svdup_f64
#include "sv_poly_generic.h"
#undef DUP
#undef VWRAP
#undef STYPE
#undef VTYPE

#endif
