/*
 * Copyright (c) 1993 Martin Birgmeier
 * All rights reserved.
 *
 * You may redistribute unmodified or modified versions of this source
 * code provided that the above copyright notice and this and the
 * following conditions are retained.
 *
 * This software is provided ``as is'', and comes with no warranties
 * of any kind. I shall in no event be liable for anything that happens
 * to anyone/anything when using this software.
 */

#ifndef _RAND48_H_
#define _RAND48_H_

#include <sys/types.h>
#include <math.h>
#include <stdlib.h>

#include "fpmath.h"

#define	RAND48_SEED_0	(0x330e)
#define	RAND48_SEED_1	(0xabcd)
#define	RAND48_SEED_2	(0x1234)
#define	RAND48_MULT_0	(0xe66d)
#define	RAND48_MULT_1	(0xdeec)
#define	RAND48_MULT_2	(0x0005)
#define	RAND48_ADD	(0x000b)

typedef uint64_t uint48;

extern uint48 _rand48_seed;
extern uint48 _rand48_mult;
extern uint48 _rand48_add;

#define	TOUINT48(x, y, z)						\
	((uint48)(x) + (((uint48)(y)) << 16) + (((uint48)(z)) << 32))

#define	RAND48_SEED	TOUINT48(RAND48_SEED_0, RAND48_SEED_1, RAND48_SEED_2)
#define	RAND48_MULT	TOUINT48(RAND48_MULT_0, RAND48_MULT_1, RAND48_MULT_2)

#define	LOADRAND48(l, x) do {						\
	(l) = TOUINT48((x)[0], (x)[1], (x)[2]);				\
} while (0)

#define	STORERAND48(l, x) do {						\
	(x)[0] = (unsigned short)(l);					\
	(x)[1] = (unsigned short)((l) >> 16);				\
	(x)[2] = (unsigned short)((l) >> 32);				\
} while (0)

#define	_DORAND48(l) do {						\
	(l) = (l) * _rand48_mult + _rand48_add;				\
} while (0)

#define	DORAND48(l, x) do {						\
	LOADRAND48(l, x);						\
	_DORAND48(l);							\
	STORERAND48(l, x);						\
} while (0)

#define	ERAND48_BEGIN							\
	union {								\
		union IEEEd2bits ieee;					\
		uint64_t u64;						\
	} u;								\
	int s

/*
 * Optimization for speed: assume doubles are IEEE 754 and use bit fiddling
 * rather than converting to double.  Specifically, clamp the result to 48 bits
 * and convert to a double in [0.0, 1.0) via division by 2^48.  Normalize by
 * shifting the most significant bit into the implicit one position and
 * adjusting the exponent accordingly.  The store to the exponent field
 * overwrites the implicit one.
 */
#define	ERAND48_END(x) do {						\
	u.u64 = ((x) & 0xffffffffffffULL);				\
	if (u.u64 == 0)							\
		return (0.0);						\
	u.u64 <<= 5;							\
	for (s = 0; !(u.u64 & (1LL << 52)); s++, u.u64 <<= 1)		\
		;							\
	u.ieee.bits.exp = 1022 - s;					\
	return (u.ieee.d);						\
} while (0)

#endif /* _RAND48_H_ */
