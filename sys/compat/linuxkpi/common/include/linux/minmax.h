/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013-2015 Mellanox Technologies, Ltd.
 * Copyright (c) 2014-2015 François Tigeot
 * Copyright (c) 2015 Hans Petter Selasky <hselasky@FreeBSD.org>
 * Copyright (c) 2016 Matt Macy <mmacy@FreeBSD.org>
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

#ifndef _LINUXKPI_LINUX_MINMAX_H_
#define	_LINUXKPI_LINUX_MINMAX_H_

#include <linux/build_bug.h>
#include <linux/compiler.h>
#include <linux/types.h>

#define	min(x, y)	((x) < (y) ? (x) : (y))
#define	max(x, y)	((x) > (y) ? (x) : (y))

#define	min3(a, b, c)	min(a, min(b, c))
#define	max3(a, b, c)	max(a, max(b, c))

#define min_not_zero(x, y) ({						\
	__typeof(x) __min1 = (x);					\
	__typeof(y) __min2 = (y);					\
	__min1 == 0 ? __min2 : ((__min2 == 0) ? __min1 : min(__min1, __min2));\
})

#define	min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1 : __min2; })

#define	max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1 : __max2; })

#define	clamp_t(type, _x, min, max)	min_t(type, max_t(type, _x, min), max)
#define	clamp(x, lo, hi)		min(max(x, lo), hi)
#define	clamp_val(val, lo, hi)	clamp_t(typeof(val), val, lo, hi)

/* Swap values of a and b */
#define swap(a, b) do {			\
	__typeof(a) _swap_tmp = a;	\
	a = b;				\
	b = _swap_tmp;			\
} while (0)

#endif /* _LINUXKPI_LINUX_MINMAX_H_ */
