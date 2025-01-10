/*
 * Coefficients for double-precision e^x - 1 function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* Generated using fpminimax, see tools/expm1.sollya for details.  */
const double __expm1_poly[] = { 0x1p-1,
				0x1.5555555555559p-3,
				0x1.555555555554bp-5,
				0x1.111111110f663p-7,
				0x1.6c16c16c1b5f3p-10,
				0x1.a01a01affa35dp-13,
				0x1.a01a018b4ecbbp-16,
				0x1.71ddf82db5bb4p-19,
				0x1.27e517fc0d54bp-22,
				0x1.af5eedae67435p-26,
				0x1.1f143d060a28ap-29 };
