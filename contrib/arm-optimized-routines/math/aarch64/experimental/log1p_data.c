/*
 * Data used in double-precision log(1+x) function.
 *
 * Copyright (c) 2022-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* Polynomial coefficients generated using Remez algorithm, see
   log1p.sollya for details.  */
const struct log1p_data __log1p_data
    = { .coeffs
	= { -0x1.ffffffffffffbp-2, 0x1.55555555551a9p-2, -0x1.00000000008e3p-2,
	    0x1.9999999a32797p-3, -0x1.555555552fecfp-3, 0x1.249248e071e5ap-3,
	    -0x1.ffffff8bf8482p-4, 0x1.c71c8f07da57ap-4, -0x1.9999ca4ccb617p-4,
	    0x1.7459ad2e1dfa3p-4, -0x1.554d2680a3ff2p-4, 0x1.3b4c54d487455p-4,
	    -0x1.2548a9ffe80e6p-4, 0x1.0f389a24b2e07p-4, -0x1.eee4db15db335p-5,
	    0x1.e95b494d4a5ddp-5, -0x1.15fdf07cb7c73p-4, 0x1.0310b70800fcfp-4,
	    -0x1.cfa7385bdb37ep-6 } };
