/*-
 * Copyright (c) 2001 Mike Barcroft <mike@FreeBSD.org>
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#ifndef _SYS_STDINT_H_
#define _SYS_STDINT_H_

#include <sys/cdefs.h>

#include <machine/ansi.h>
#include <machine/limits.h>

typedef	__int8_t		int8_t;
typedef	__int16_t		int16_t;
typedef	__int32_t		int32_t;
typedef	__int64_t		int64_t;

typedef	__uint8_t		uint8_t;
typedef	__uint16_t		uint16_t;
typedef	__uint32_t		uint32_t;
typedef	__uint64_t		uint64_t;

typedef	__int_least8_t		int_least8_t;
typedef	__int_least16_t		int_least16_t;
typedef	__int_least32_t		int_least32_t;
typedef	__int_least64_t		int_least64_t;

typedef	__uint_least8_t		uint_least8_t;
typedef	__uint_least16_t	uint_least16_t;
typedef	__uint_least32_t	uint_least32_t;
typedef	__uint_least64_t	uint_least64_t;

typedef	__int_fast8_t		int_fast8_t;
typedef	__int_fast16_t		int_fast16_t;
typedef	__int_fast32_t		int_fast32_t;
typedef	__int_fast64_t		int_fast64_t;

typedef	__uint_fast8_t		uint_fast8_t;
typedef	__uint_fast16_t		uint_fast16_t;
typedef	__uint_fast32_t		uint_fast32_t;
typedef	__uint_fast64_t		uint_fast64_t;

typedef	__intmax_t		intmax_t;
typedef	__uintmax_t		uintmax_t;

typedef	__intptr_t		intptr_t;
typedef	__uintptr_t		uintptr_t;

#if !defined(__cplusplus) || defined(__STDC_CONSTANT_MACROS)
#define	INT8_C(c)	__INT8_C(c)
#define	INT16_C(c)	__INT16_C(c)
#define	INT32_C(c)	__INT32_C(c)
#define	INT64_C(c)	__INT64_C(c)

#define	UINT8_C(c)	__UINT8_C(c)
#define	UINT16_C(c)	__UINT16_C(c)
#define	UINT32_C(c)	__UINT32_C(c)
#define	UINT64_C(c)	__UINT64_C(c)

#define	INTMAX_C(c)	__INTMAX_C(c)
#define	UINTMAX_C(c)	__UINTMAX_C(c)
#endif /* !defined(__cplusplus) || defined(__STDC_CONSTANT_MACROS) */

#endif /* !_SYS_STDINT_H_ */
