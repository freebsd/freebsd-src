/*	$NetBSD: humanize_number.c,v 1.14 2008/04/28 20:22:59 martin Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999, 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center, by Luke Mewburn and by Tomas Svensson.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <libutil.h>

int
humanize_number(char *buf, size_t len, int64_t quotient,
    const char *suffix, int scale, int flags)
{
	const char *prefixes, *sep;
	int	i, r, remainder, maxscale, s1, s2, sign;
	int64_t	divisor, max;
	size_t	baselen;

	assert(buf != NULL);
	assert(suffix != NULL);
	assert(scale >= 0);

	remainder = 0;

	if (flags & HN_DIVISOR_1000) {
		/* SI for decimal multiplies */
		divisor = 1000;
		if (flags & HN_B)
			prefixes = "B\0k\0M\0G\0T\0P\0E";
		else
			prefixes = "\0\0k\0M\0G\0T\0P\0E";
	} else {
		/*
		 * binary multiplies
		 * XXX IEC 60027-2 recommends Ki, Mi, Gi...
		 */
		divisor = 1024;
		if (flags & HN_B)
			prefixes = "B\0K\0M\0G\0T\0P\0E";
		else
			prefixes = "\0\0K\0M\0G\0T\0P\0E";
	}

#define	SCALE2PREFIX(scale)	(&prefixes[(scale) << 1])
	maxscale = 7;

	if (scale >= maxscale &&
	    (scale & (HN_AUTOSCALE | HN_GETSCALE)) == 0)
		return (-1);

	if (buf == NULL || suffix == NULL)
		return (-1);

	if (len > 0)
		buf[0] = '\0';
	if (quotient < 0) {
		sign = -1;
		quotient = -quotient;
		baselen = 3;		/* sign, digit, prefix */
	} else {
		sign = 1;
		baselen = 2;		/* digit, prefix */
	}
	if (flags & HN_NOSPACE)
		sep = "";
	else {
		sep = " ";
		baselen++;
	}
	baselen += strlen(suffix);

	/* Check if enough room for `x y' + suffix + `\0' */
	if (len < baselen + 1)
		return (-1);

	if (scale & (HN_AUTOSCALE | HN_GETSCALE)) {
		/* See if there is additional columns can be used. */
		for (max = 1, i = len - baselen; i-- > 0;)
			max *= 10;

		/*
		 * Divide the number until it fits the given column.
		 * If there will be an overflow by the rounding below,
		 * divide once more.
		 */
		for (i = 0;
		    (quotient >= max || (quotient == max - 1 && remainder >= 950)) &&
		    i < maxscale; i++) {
			remainder = quotient % divisor;
			quotient /= divisor;
		}

		if (scale & HN_GETSCALE)
			return (i);
	} else {
		for (i = 0; i < scale && i < maxscale; i++) {
			remainder = quotient % divisor;
			quotient /= divisor;
		}
	}

	/* If a value <= 9.9 after rounding and ... */
	if (quotient <= 9 && remainder < 950 && i > 0 && flags & HN_DECIMAL) {
		/* baselen + \0 + .N */
		if (len < baselen + 1 + 2)
			return (-1);
		s1 = (int)quotient + ((remainder + 50) / 1000);
		s2 = ((remainder + 50) / 100) % 10;
		r = snprintf(buf, len, "%d%s%d%s%s%s",
		    sign * s1, localeconv()->decimal_point, s2,
		    sep, SCALE2PREFIX(i), suffix);
	} else
		r = snprintf(buf, len, "%" PRId64 "%s%s%s",
		    sign * (quotient + (remainder + 50) / 1000),
		    sep, SCALE2PREFIX(i), suffix);

	return (r);
}

