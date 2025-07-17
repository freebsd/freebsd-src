/*
 * Double-precision polynomial coefficients for vector atan(x) and atan2(y,x).
 *
 * Copyright (c) 2019-2024, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

const struct atan_poly_data __atan_poly_data
    = { .poly = { /* Coefficients of polynomial P such that atan(x)~x+x*P(x^2)
		     on [2**-1022, 1.0]. See atan.sollya for details of how
		     these were generated.  */
		  -0x1.5555555555555p-2,  0x1.99999999996c1p-3,
		  -0x1.2492492478f88p-3,  0x1.c71c71bc3951cp-4,
		  -0x1.745d160a7e368p-4,  0x1.3b139b6a88ba1p-4,
		  -0x1.11100ee084227p-4,  0x1.e1d0f9696f63bp-5,
		  -0x1.aebfe7b418581p-5,  0x1.842dbe9b0d916p-5,
		  -0x1.5d30140ae5e99p-5,  0x1.338e31eb2fbbcp-5,
		  -0x1.00e6eece7de8p-5,	  0x1.860897b29e5efp-6,
		  -0x1.0051381722a59p-6,  0x1.14e9dc19a4a4ep-7,
		  -0x1.d0062b42fe3bfp-9,  0x1.17739e210171ap-10,
		  -0x1.ab24da7be7402p-13, 0x1.358851160a528p-16 } };
