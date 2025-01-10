/*
 * Single-precision polynomial coefficients for vector atan(x) and atan2(y,x).
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* Coefficients of polynomial P such that atan(x)~x+x*P(x^2) on [2**-128, 1.0].
 */
const struct atanf_poly_data __atanf_poly_data
    = { .poly
	= { /* See atanf.sollya for details of how these were generated.  */
	    -0x1.55555p-2f, 0x1.99935ep-3f, -0x1.24051ep-3f, 0x1.bd7368p-4f,
	    -0x1.491f0ep-4f, 0x1.93a2c0p-5f, -0x1.4c3c60p-6f,
	    0x1.01fd88p-8f } };
