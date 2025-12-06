/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define FUNCSTEM stdc_count_zeros
#define MKREFFUNC(name, type)						\
	static unsigned							\
	name(type value) 						\
	{ 								\
		unsigned count = 0;					\
									\
		value = ~value;						\
		while (value != 0) {					\
			count += value & 1;				\
			value >>= 1;					\
		}							\
									\
		return (count);						\
	}

#include "stdbit-test-framework.c"
