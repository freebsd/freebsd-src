/*	$NetBSD: humanize_number.c,v 1.5 2003/12/26 11:30:36 simonb Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <libutil.h>

int
humanize_number(char *buf, size_t len, int64_t bytes,
    const char *suffix, int scale, int flags)
{
	const char *prefixes;
	int	i, r;
	int64_t	divisor, max, s1, s2, sign;
	size_t	baselen, suffixlen;

	assert(buf != NULL);
	assert(suffix != NULL);
	assert(scale >= 0);

	if (flags & HN_DIVISOR_1000) {
		/* SI for decimal multiplies */
		divisor = 1000;
		prefixes = " kMGTPE";
	} else {
		/*
		 * binary multiplies
		 * XXX IEC 60027-2 recommends Ki, Mi, Gi...
		 */
		divisor = 1024;
		prefixes = " KMGTPE";
	}

	if ((size_t) scale >= strlen(prefixes) && scale != HN_AUTOSCALE &&
	    scale != HN_GETSCALE)
		return (-1);

	if (buf == NULL || suffix == NULL)
		return (-1);

	if (len > 0)
		buf[0] = '\0';
	if (bytes < 0) {
		sign = -1;
		bytes *= -100;
		baselen = 4;
	} else {
		sign = 1;
		bytes *= 100;
		baselen = 3;
	}

	suffixlen = strlen(suffix);

	/* check if enough room for `x y' + suffix + `\0' */
	if (len < baselen + suffixlen + 1)
		return (-1);

	if (flags & HN_DIVISOR_1000)
		divisor = 1000;
	else
		divisor = 1024;

	max = 100;
	for (i = 0;
	     (size_t) i < len - suffixlen - baselen + ((flags & HN_NOSPACE) ?
	     1 : 0); i++)
		max *= 10;

	if ((scale & HN_AUTOSCALE) || (scale & HN_GETSCALE)) {
		for (i = 0; bytes >= max && prefixes[i + 1]; i++)
			bytes /= divisor;
	} else {
		for (i = 0; i < scale && prefixes[i + 1]; i++)
			bytes /= divisor;
	}

	if (scale & HN_GETSCALE)
		return (i);

	if (bytes < 1000 && flags & HN_DECIMAL) {
		if (len < (baselen + 2 + ((flags & HN_NOSPACE) || (i == 0 &&
		    !(flags & HN_B)) ? 0 : 1)))
			return (-1);
		s1 = bytes / 100;
		if ((s2 = (((bytes % 100) + 5) / 10)) == 10) {
			s1++;
			s2 = 0;
		}
		if (s1 < 10 && i == 0)
			/* Don't ever use .0 for a number less than 10. */
			r = snprintf(buf, len, "%lld%s%c%s",
			    /* LONGLONG */
			    (long long)(sign * s1),
			    (i == 0 && !(flags & HN_B)) || flags & HN_NOSPACE ?
			    "" : " ", (i == 0 && (flags & HN_B)) ? 'B' :
			    prefixes[i], suffix);
		else
			r = snprintf(buf, len, "%lld%s%lld%s%c%s",
			    /* LONGLONG */
			    (long long)(sign * s1),
			    localeconv()->decimal_point,
			    /* LONGLONG */
			    (long long)s2,
			    (i == 0 && !(flags & HN_B)) || flags & HN_NOSPACE ?
			    "" : " ", (i == 0 && (flags & HN_B)) ? 'B' :
			    prefixes[i], suffix);

	} else
		r = snprintf(buf, len, "%lld%s%c%s",
		    /* LONGLONG */
		    (long long)(sign * ((bytes + 50) / 100)),
		    i == 0 || flags & HN_NOSPACE ? "" : " ", (i == 0 &&
		    (flags & HN_B)) ? 'B' : prefixes[i], suffix);

	return (r);
}
