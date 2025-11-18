/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define FUNCSTEM stdc_bit_width
#define MKREFFUNC(name, type)						\
	static unsigned							\
	name(type value) 						\
	{ 								\
		unsigned width = 0;					\
									\
		while (value != 0) {					\
			value >>= 1;					\
			width++;					\
		}							\
									\
		return (width);						\
	}

#include "stdbit-test-framework.c"
