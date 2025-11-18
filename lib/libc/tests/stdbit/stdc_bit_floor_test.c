/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define FUNCSTEM stdc_bit_floor
#define MKREFFUNC(name, type)						\
	static type 							\
	name(type value) 						\
	{ 								\
		type floor = 1;						\
									\
		if (value == 0)						\
			return (0);					\
									\
		while (value != 1) {					\
			floor <<= 1;					\
			value >>= 1;					\
		}							\
									\
		return (floor);						\
	}

#include "stdbit-test-framework.c"
