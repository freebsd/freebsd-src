/*
 * Coefficients for single-precision vector e^x function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* Coefficients copied from the polynomial in math/v_expf.c.  */
const float __sv_expf_poly[] = {0x1.0e4020p-7f, 0x1.573e2ep-5f, 0x1.555e66p-3f,
				0x1.fffdb6p-2f, 0x1.ffffecp-1f};
