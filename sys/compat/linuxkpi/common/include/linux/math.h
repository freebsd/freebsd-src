/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013-2015 Mellanox Technologies, Ltd.
 * Copyright (c) 2014-2015 François Tigeot
 * Copyright (c) 2016 Matt Macy <mmacy@FreeBSD.org>
 * Copyright (c) 2019 Johannes Lundberg <johalun@FreeBSD.org>
 * Copyright (c) 2023 Serenity Cyber Security, LLC.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _LINUXKPI_LINUX_MATH_H_
#define	_LINUXKPI_LINUX_MATH_H_

#include <linux/types.h>

/*
 * This looks more complex than it should be. But we need to
 * get the type for the ~ right in round_down (it needs to be
 * as wide as the result!), and we want to evaluate the macro
 * arguments just once each.
 */
#define	__round_mask(x, y)	((__typeof__(x))((y)-1))
#define	round_up(x, y)		((((x)-1) | __round_mask(x, y))+1)
#define	round_down(x, y)	((x) & ~__round_mask(x, y))

#define	DIV_ROUND_UP(x, n)	howmany(x, n)
#define	DIV_ROUND_UP_ULL(x, n)	DIV_ROUND_UP((unsigned long long)(x), (n))
#define	DIV_ROUND_DOWN_ULL(x, n) ((unsigned long long)(x) / (n))

#define	DIV_ROUND_CLOSEST(x, divisor)	(((x) + ((divisor) / 2)) / (divisor))
#define	DIV_ROUND_CLOSEST_ULL(x, divisor) ({		\
	__typeof(divisor) __d = (divisor);		\
	unsigned long long __ret = (x) + (__d) / 2;	\
	__ret /= __d;					\
	__ret;						\
})

#if !defined(LINUXKPI_VERSION) || (LINUXKPI_VERSION >= 60600)
#define abs_diff(x, y) ({		\
	__typeof(x) _x = (x);		\
	__typeof(y) _y = (y);		\
	_x > _y ? _x - _y : _y - _x;	\
})
#endif

static inline uintmax_t
mult_frac(uintmax_t x, uintmax_t multiplier, uintmax_t divisor)
{
	uintmax_t q = (x / divisor);
	uintmax_t r = (x % divisor);

	return ((q * multiplier) + ((r * multiplier) / divisor));
}

#endif /* _LINUXKPI_LINUX_MATH_H_ */
