/*-
 * Copyright (c) 1990, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *      $Id:$
 */


#ifndef lint
#ifndef NOID
static char	elsieid[] = "@(#)difftime.c	7.4";
#endif /* !defined NOID */
#endif /* !defined lint */

/*LINTLIBRARY*/

#include "private.h"

/*
** Algorithm courtesy Paul Eggert (eggert@twinsun.com).
*/

#ifdef HAVE_LONG_DOUBLE
#define long_double	long double
#endif /* defined HAVE_LONG_DOUBLE */
#ifndef HAVE_LONG_DOUBLE
#define long_double	double
#endif /* !defined HAVE_LONG_DOUBLE */

double
difftime(time1, time0)
const time_t	time1;
const time_t	time0;
{
	time_t	delta;
	time_t	hibit;

	if (sizeof(time_t) < sizeof(double))
		return (double) time1 - (double) time0;
	if (sizeof(time_t) < sizeof(long_double))
		return (long_double) time1 - (long_double) time0;
	if (time1 < time0)
		return -difftime(time0, time1);
	/*
	** As much as possible, avoid loss of precision
	** by computing the difference before converting to double.
	*/
	delta = time1 - time0;
	if (delta >= 0)
		return delta;
	/*
	** Repair delta overflow.
	*/
	hibit = 1;
	while ((hibit <<= 1) > 0)
		continue;
	/*
	** The following expression rounds twice, which means
	** the result may not be the closest to the true answer.
	** For example, suppose time_t is 64-bit signed int,
	** long_double is IEEE 754 double with default rounding,
	** time1 = 9223372036854775807 and time0 = -1536.
	** Then the true difference is 9223372036854777343,
	** which rounds to 9223372036854777856
	** with a total error of 513.
	** But delta overflows to -9223372036854774273,
	** which rounds to -9223372036854774784, and correcting
	** this by subtracting 2 * (long_double) hibit
	** (i.e. by adding 2**64 = 18446744073709551616)
	** yields 9223372036854776832, which
	** rounds to 9223372036854775808
	** with a total error of 1535 instead.
	** This problem occurs only with very large differences.
	** It's too painful to fix this portably.
	** We are not alone in this problem;
	** many C compilers round twice when converting
	** large unsigned types to small floating types,
	** so if time_t is unsigned the "return delta" above
	** has the same double-rounding problem.
	*/
	return delta - 2 * (long_double) hibit;
}
