/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define FUNCSTEM stdc_first_leading_zero
#define MKREFFUNC(name, type)						\
	static unsigned							\
	name(type value) 						\
	{ 								\
		type bit = 1;						\
		unsigned pos = 1;					\
									\
		value = ~value;						\
		if (value == 0)						\
			return (0);					\
									\
		while ((type)(bit << 1) != 0)				\
			bit <<= 1;					\
									\
		while ((bit & value) == 0) {				\
			bit >>= 1;					\
			pos++;						\
		}							\
									\
		return (pos);						\
	}

#include "stdbit-test-framework.c"
