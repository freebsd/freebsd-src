/*
 * Data used in single-precision sin(x) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* Polynomial coefficients for approximating sin(x) in single
   precision. These are the non-zero coefficients from the
   degree 9 Taylor series expansion of sin.  */

const struct sv_sinf_data __sv_sinf_data = {.coeffs = {
					      0x1.5b2e76p-19f,
					      -0x1.9f42eap-13f,
					      0x1.110df4p-7f,
					      -0x1.555548p-3f,
					    }};
