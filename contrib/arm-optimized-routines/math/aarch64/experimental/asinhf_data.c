/*
 * Coefficients for single-precision asinh(x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* Approximate asinhf(x) directly in [2^-12, 1]. See for tools/asinhf.sollya
   for these coeffs were generated.  */
const struct asinhf_data __asinhf_data
    = { .coeffs = { -0x1.9b16fap-19f, -0x1.552baap-3f, -0x1.4e572ap-11f,
		    0x1.3a81dcp-4f, 0x1.65bbaap-10f, -0x1.057f1p-4f,
		    0x1.6c1d46p-5f, -0x1.4cafe8p-7f } };
