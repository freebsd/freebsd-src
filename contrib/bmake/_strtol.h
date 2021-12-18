/* $NetBSD: _strtol.h,v 1.11 2017/07/06 21:08:44 joerg Exp $ */

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Original version ID:
 * NetBSD: src/lib/libc/locale/_wcstol.h,v 1.2 2003/08/07 16:43:03 agc Exp
 */

/*
 * function template for strtol, strtoll and strtoimax.
 *
 * parameters:
 *	_FUNCNAME : function name
 *      __INT     : return type
 *      __INT_MIN : lower limit of the return type
 *      __INT_MAX : upper limit of the return type
 */
#if defined(_KERNEL) || defined(_STANDALONE) || defined(HAVE_NBTOOL_CONFIG_H) || defined(BCS_ONLY)
__INT
_FUNCNAME(const char *nptr, char **endptr, int base)
#else
#include <locale.h>
#include "setlocale_local.h"
#define INT_FUNCNAME_(pre, name, post)	pre ## name ## post
#define INT_FUNCNAME(pre, name, post)	INT_FUNCNAME_(pre, name, post)

static __INT
INT_FUNCNAME(_int_, _FUNCNAME, _l)(const char *nptr, char **endptr,
				   int base, locale_t loc)
#endif
{
	const char *s;
	__INT acc, cutoff;
	unsigned char c;
	int i, neg, any, cutlim;

	_DIAGASSERT(nptr != NULL);
	/* endptr may be NULL */

	/* check base value */
	if (base && (base < 2 || base > 36)) {
#if !defined(_KERNEL) && !defined(_STANDALONE)
		errno = EINVAL;
		if (endptr != NULL)
			/* LINTED interface specification */
			*endptr = __UNCONST(nptr);
		return 0;
#else
		panic("%s: invalid base %d", __func__, base);
#endif
	}

	/*
	 * Skip white space and pick up leading +/- sign if any.
	 * If base is 0, allow 0x for hex and 0 for octal, else
	 * assume decimal; if base is already 16, allow 0x.
	 */
	s = nptr;
#if defined(_KERNEL) || defined(_STANDALONE) || \
    defined(HAVE_NBTOOL_CONFIG_H) || defined(BCS_ONLY)
	do {
		c = *s++;
	} while (isspace(c));
#else
	do {
		c = *s++;
	} while (isspace_l(c, loc));
#endif
	if (c == '-') {
		neg = 1;
		c = *s++;
	} else {
		neg = 0;
		if (c == '+')
			c = *s++;
	}
	if ((base == 0 || base == 16) &&
	    c == '0' && (*s == 'x' || *s == 'X') &&
	    ((s[1] >= '0' && s[1] <= '9') ||
	     (s[1] >= 'a' && s[1] <= 'f') ||
	     (s[1] >= 'A' && s[1] <= 'F'))) {
		c = s[1];
		s += 2;
		base = 16;
#if 0
	} else if ((base == 0 || base == 2) &&
	    c == '0' && (*s == 'b' || *s == 'B') &&
	    (s[1] >= '0' && s[1] <= '1')) {
		c = s[1];
		s += 2;
		base = 2;
#endif
	} else if (base == 0)
		base = (c == '0' ? 8 : 10);

	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for longs is
	 * [-2147483648..2147483647] and the input base is 10,
	 * cutoff will be set to 214748364 and cutlim to either
	 * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
	 * a value > 214748364, or equal but the next digit is > 7 (or 8),
	 * the number is too big, and we will return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	cutoff = (__INT)(neg ? __INT_MIN : __INT_MAX);
	cutlim = (int)(cutoff % base);
	cutoff /= base;
	if (neg) {
		if (cutlim > 0) {
			cutlim -= base;
			cutoff += 1;
		}
		cutlim = -cutlim;
	}
	for (acc = 0, any = 0;; c = *s++) {
		if (c >= '0' && c <= '9')
			i = c - '0';
		else if (c >= 'a' && c <= 'z')
			i = (c - 'a') + 10;
		else if (c >= 'A' && c <= 'Z')
			i = (c - 'A') + 10;
		else
			break;
		if (i >= base)
			break;
		if (any < 0)
			continue;
		if (neg) {
			if (acc < cutoff || (acc == cutoff && i > cutlim)) {
				acc = __INT_MIN;
#if !defined(_KERNEL) && !defined(_STANDALONE)
				any = -1;
				errno = ERANGE;
#else
				any = 0;
				break;
#endif
			} else {
				any = 1;
				acc *= base;
				acc -= i;
			}
		} else {
			if (acc > cutoff || (acc == cutoff && i > cutlim)) {
				acc = __INT_MAX;
#if !defined(_KERNEL) && !defined(_STANDALONE)
				any = -1;
				errno = ERANGE;
#else
				any = 0;
				break;
#endif
			} else {
				any = 1;
				acc *= base;
				acc += i;
			}
		}
	}
	if (endptr != NULL)
		/* LINTED interface specification */
		*endptr = __UNCONST(any ? s - 1 : nptr);
	return(acc);
}

#if !defined(_KERNEL) && !defined(_STANDALONE) && \
    !defined(HAVE_NBTOOL_CONFIG_H) && !defined(BCS_ONLY)
__INT
_FUNCNAME(const char *nptr, char **endptr, int base)
{
	return INT_FUNCNAME(_int_, _FUNCNAME, _l)(nptr, endptr, base, _current_locale());
}

__INT
INT_FUNCNAME(, _FUNCNAME, _l)(const char *nptr, char **endptr, int base, locale_t loc)
{
	return INT_FUNCNAME(_int_, _FUNCNAME, _l)(nptr, endptr, base, loc);
}
#endif
