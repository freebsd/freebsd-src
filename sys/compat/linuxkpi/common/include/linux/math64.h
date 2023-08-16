/*-
 * Copyright (c) 2007 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2014-2015 Mellanox Technologies, Ltd. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUXKPI_LINUX_MATH64_H
#define	_LINUXKPI_LINUX_MATH64_H

#include <sys/stdint.h>

#define	do_div(n, base) ({			\
	uint32_t __base = (base);		\
	uint32_t __rem;				\
	__rem = ((uint64_t)(n)) % __base;	\
	(n) = ((uint64_t)(n)) / __base;		\
	__rem;					\
})

static inline uint64_t
div64_u64_rem(uint64_t dividend, uint64_t divisor, uint64_t *remainder)
{

	*remainder = dividend % divisor;
	return (dividend / divisor);
}

static inline int64_t
div64_s64(int64_t dividend, int64_t divisor)
{

	return (dividend / divisor);
}

static inline uint64_t
div64_u64(uint64_t dividend, uint64_t divisor)
{

	return (dividend / divisor);
}

static inline uint64_t
div_u64_rem(uint64_t dividend, uint32_t divisor, uint32_t *remainder)
{

	*remainder = dividend % divisor;
	return (dividend / divisor);
}

static inline int64_t
div_s64(int64_t dividend, int32_t divisor)
{

	return (dividend / divisor);
}

static inline uint64_t
div_u64(uint64_t dividend, uint32_t divisor)
{

	return (dividend / divisor);
}

static inline uint64_t
mul_u32_u32(uint32_t a, uint32_t b)
{

	return ((uint64_t)a * b);
}

static inline uint64_t
div64_u64_round_up(uint64_t dividend, uint64_t divisor)
{
	return ((dividend + divisor - 1) / divisor);
}

#define	DIV64_U64_ROUND_UP(...) \
	div64_u64_round_up(__VA_ARGS__)

static inline uint64_t
mul_u64_u32_div(uint64_t x, uint32_t y, uint32_t div)
{
	const uint64_t rem = x % div;

	return ((x / div) * y + (rem * y) / div);
}

static inline uint64_t
mul_u64_u32_shr(uint64_t x, uint32_t y, unsigned int shift)
{
	uint32_t hi, lo;
	hi = x >> 32;
	lo = x & 0xffffffff;

	return (mul_u32_u32(lo, y) >> shift) +
		(mul_u32_u32(hi, y) << (32 - shift));
}

#endif /* _LINUXKPI_LINUX_MATH64_H */
