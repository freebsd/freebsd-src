/*
 * Copyright (c) 2025 Robert Clausecker <fuz@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define FUNCSTEM stdc_bit_ceil
#define MKREFFUNC(name, type)						\
	static type 							\
	name(type value) 						\
	{ 								\
		type ceil = 1;						\
									\
		while (ceil < value && ceil != 0)			\
			ceil <<= 1;					\
									\
		return (ceil);						\
	}

#include "stdbit-test-framework.c"
