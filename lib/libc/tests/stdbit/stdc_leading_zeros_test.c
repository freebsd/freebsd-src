/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define FUNCSTEM stdc_leading_zeros
#define MKREFFUNC(name, type)						\
	static unsigned							\
	name(type value) 						\
	{ 								\
		type bit = 1;						\
		unsigned count = 0;					\
									\
		while ((type)(bit << 1) != 0)				\
			bit <<= 1;					\
									\
		while (bit != 0 && (bit & value) == 0) {		\
			bit >>= 1;					\
			count++;					\
		}							\
									\
		return (count);						\
	}

#include "stdbit-test-framework.c"
