/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2009, 2010 Xin LI <delphij@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/limits.h>
#include <sys/types.h>
#include <string.h>

/*
 * Portable strlen() for 32-bit and 64-bit systems.
 *
 * The expression:
 *
 *	((x - 0x01....01) & ~x & 0x80....80)
 *
 * would evaluate to a non-zero value iff any of the bytes in the
 * original word is zero.
 *
 * The algorithm above is found on "Hacker's Delight" by
 * Henry S. Warren, Jr.
 *
 * Note: this leaves performance on the table and each architecture
 * would be best served with a tailor made routine instead.
 */

#if LONG_BIT == 32
static const unsigned long mask01 = 0x01010101;
static const unsigned long mask80 = 0x80808080;
#elif LONG_BIT == 64
static const unsigned long mask01 = 0x0101010101010101;
static const unsigned long mask80 = 0x8080808080808080;
#else
#error Unsupported word size
#endif

#define	LONGPTR_MASK (sizeof(long) - 1)

#if BYTE_ORDER == LITTLE_ENDIAN
#define	FINDZERO __builtin_ctzl
#else
#define	FINDZERO __builtin_clzl
#endif

size_t
strlen(const char *str)
{
	const unsigned long *lp;
	unsigned long mask;
	long va, vb;
	long val;

	lp = (unsigned long *) (uintptr_t) str;
	if ((uintptr_t)lp & LONGPTR_MASK) {
		lp = (__typeof(lp)) ((uintptr_t)lp & ~LONGPTR_MASK);
#if BYTE_ORDER == LITTLE_ENDIAN
		mask = ~(~0UL << (((uintptr_t)str & LONGPTR_MASK) << 3));
#else
		mask = ~(~0UL >> (((uintptr_t)str & LONGPTR_MASK) << 3));
#endif
		val = *lp | mask;
		va = (val - mask01);
		vb = ((~val) & mask80);
		if (va & vb) {
			return ((const char *)lp - str + (FINDZERO(va & vb) >> 3));
		}
		lp++;
	}

	for (; ; lp++) {
		va = (*lp - mask01);
		vb = ((~*lp) & mask80);
		if (va & vb) {
			return ((const char *)lp - str + (FINDZERO(va & vb) >> 3));
		}
	}

	__builtin_unreachable();
	return (0);
}
