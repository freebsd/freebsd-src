/*
 * Double-precision polynomial coefficients for scalar asinh(x)
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* asinh(x) is odd, and the first term of the Taylor expansion is x, so we can
   approximate the function by x + x^3 * P(x^2), where P(z) has the form:
   C0 + C1 * z + C2 * z^2 + C3 * z^3 + ...
   Note P is evaluated on even powers of x only. See tools/asinh.sollya for the
   algorithm used to generate these coefficients.  */
const struct asinh_data __asinh_data
  = {.poly
     = {-0x1.55555555554a7p-3, 0x1.3333333326c7p-4, -0x1.6db6db68332e6p-5,
	0x1.f1c71b26fb40dp-6, -0x1.6e8b8b654a621p-6, 0x1.1c4daa9e67871p-6,
	-0x1.c9871d10885afp-7, 0x1.7a16e8d9d2ecfp-7, -0x1.3ddca533e9f54p-7,
	0x1.0becef748dafcp-7, -0x1.b90c7099dd397p-8, 0x1.541f2bb1ffe51p-8,
	-0x1.d217026a669ecp-9, 0x1.0b5c7977aaf7p-9, -0x1.e0f37daef9127p-11,
	0x1.388b5fe542a6p-12, -0x1.021a48685e287p-14, 0x1.93d4ba83d34dap-18}};
