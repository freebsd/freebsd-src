/*
 * Data used in single-precision erfc(x) function.
 *
 * Copyright (c) 2019-2023, Arm Limited.
 * SPDX-License-Identifier: MIT OR Apache-2.0 WITH LLVM-exception
 */

#include "math_config.h"

/* Polynomial coefficients for approximating erfc(x)*exp(x*x) in double
   precision. Generated using the Remez algorithm on each interval separately
   (see erfcf.sollya for more detail).  */
const struct erfcf_poly_data __erfcf_poly_data
  = {.poly
     = {{
#if ERFCF_POLY_NCOEFFS == 16
	  0x1.ffffffffe7c59p-1, -0x1.20dd74f8cecc5p0, 0x1.fffffc67a0fbdp-1,
	  -0x1.81270c3ced2d6p-1, 0x1.fffc0c6606e45p-2, -0x1.340a779e8a8e3p-2,
	  0x1.54c1663fc5a01p-3, -0x1.5d468c9269dafp-4, 0x1.4afe6b00df9d5p-5,
	  -0x1.1d22d2720cb91p-6, 0x1.afa399a5761b1p-8, -0x1.113851b5858adp-9,
	  0x1.0f992e4d5c6a4p-11, -0x1.86534d558052ap-14, 0x1.63e537bfb7cd5p-17,
	  -0x1.32712a6275c4dp-21
#endif
	},

	{
#if ERFCF_POLY_NCOEFFS == 16
	  0x1.fea5663f75cd1p-1, -0x1.1cb5a82adf1c4p0, 0x1.e7c8da942d86fp-1,
	  -0x1.547ba0456bac7p-1, 0x1.8a6fc0f4421a4p-2, -0x1.7c14f9301ee58p-3,
	  0x1.2f67c8351577p-4, -0x1.8e733f6d159d9p-6, 0x1.aa6a0ec249067p-8,
	  -0x1.6f4ec45b11f3fp-10, 0x1.f4c00c4b33ba8p-13, -0x1.0795faf7846d2p-15,
	  0x1.9cef9031810ddp-19, -0x1.c4d60c3fecdb6p-23, 0x1.360547ec2229dp-27,
	  -0x1.8ec1581647f9fp-33
#endif
	},

	{
#if ERFCF_POLY_NCOEFFS == 16
	  0x1.dae421147c591p-1, -0x1.c211957a0abfcp-1, 0x1.28a8d87aa1b12p-1,
	  -0x1.224d2a58cbef4p-2, 0x1.b3d45dcaef898p-4, -0x1.ff99d8b33e7a9p-6,
	  0x1.dac66375b99f6p-8, -0x1.5e1786f0f91ap-10, 0x1.9a2588deaec4fp-13,
	  -0x1.7b886b183b235p-16, 0x1.1209e7da8ff82p-19, -0x1.2e5c870c6ed8p-23,
	  0x1.ec6a89422928ep-28, -0x1.16e7d837b61bcp-32, 0x1.88868a73e4b43p-38,
	  -0x1.027034672f11cp-44
#endif
	},

	{
#if ERFCF_POLY_NCOEFFS == 16
	  0x1.8ae320c1bad5ap-1, -0x1.1cdd6aa6929aap-1, 0x1.0e39a7b285f58p-2,
	  -0x1.6fb12a95e351dp-4, 0x1.77dd0649e352cp-6, -0x1.28a9e9560c461p-8,
	  0x1.6f7d7778e9433p-11, -0x1.68363698afe4ap-14, 0x1.17e94cdf35d82p-17,
	  -0x1.5766a817bd3ffp-21, 0x1.48d892094a2c1p-25, -0x1.e1b6511ab6d0bp-30,
	  0x1.04c7b8143f6a4p-34, -0x1.898831961065bp-40, 0x1.71ae8a56142a6p-46,
	  -0x1.45abac612344bp-53
#endif
	}}};
