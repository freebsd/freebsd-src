/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define FUNCSTEM stdc_has_single_bit
#define MKREFFUNC(name, type)						\
	static bool 							\
	name(type value) 						\
	{ 								\
		type bit;						\
									\
		for (bit = 1; bit != 0; bit <<= 1)			\
			if (value == bit)				\
				return (true);				\
									\
		return (false);						\
	}

#include "stdbit-test-framework.c"
