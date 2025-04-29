/*
 * Helpers for evaluating polynomials on single-precision SVE input, using
 * various schemes.
 *
 * Copyright (c) 2023-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#ifndef MATH_POLY_SVE_F32_H
#define MATH_POLY_SVE_F32_H

#include <arm_sve.h>

/* Wrap SVE f32 helpers: evaluation of some scheme/order has form:
   sv_[scheme]_[order]_f32_x.  */
#define VTYPE svfloat32_t
#define STYPE float
#define VWRAP(f) sv_##f##_f32_x
#define DUP svdup_f32
#include "sv_poly_generic.h"
#undef DUP
#undef VWRAP
#undef STYPE
#undef VTYPE

#endif
