/*
 * Coefficients for single-precision e^x - 1 function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* Generated using fpminimax, see tools/expm1f.sollya for details.  */
const float __expm1f_poly[] = {0x1.fffffep-2, 0x1.5554aep-3, 0x1.555736p-5,
			       0x1.12287cp-7, 0x1.6b55a2p-10};
