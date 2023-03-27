/*
 * Data used in single-precision log1p(x) function.
 *
 * Copyright (c) 2022-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */
#include "math_config.h"

/* Polynomial coefficients generated using floating-point minimax
   algorithm, see tools/log1pf.sollya for details.  */
const struct log1pf_data __log1pf_data
  = {.coeffs = {-0x1p-1f, 0x1.5555aap-2f, -0x1.000038p-2f, 0x1.99675cp-3f,
		-0x1.54ef78p-3f, 0x1.28a1f4p-3f, -0x1.0da91p-3f, 0x1.abcb6p-4f,
		-0x1.6f0d5ep-5f}};
