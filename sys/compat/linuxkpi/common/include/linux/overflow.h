/*-
 * Copyright (c) 2020 The FreeBSD Foundation
 *
 * This software was developed by Emmanuel Vadot under sponsorship
 * from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef __LINUXKPI_LINUX_OVERFLOW_H__
#define	__LINUXKPI_LINUX_OVERFLOW_H__

#include <sys/stdint.h>
#include <sys/types.h>

#ifndef	__has_builtin
#define	__has_builtin(x)	0
#endif

#if __has_builtin(__builtin_add_overflow)
#define check_add_overflow(a, b, c)		\
	__builtin_add_overflow(a, b, c)
#else
#error "Compiler does not support __builtin_add_overflow"
#endif

#if __has_builtin(__builtin_mul_overflow)
#define check_mul_overflow(a, b, c)	\
	__builtin_mul_overflow(a, b, c)

static inline size_t
array_size(size_t x, size_t y)
{
	size_t retval;

	if (__builtin_mul_overflow(x, y, &retval))
		retval = SIZE_MAX;
	return (retval);
}
#else
#error "Compiler does not support __builtin_mul_overflow"
#endif

#endif	/* __LINUXKPI_LINUX_OVERFLOW_H__ */
