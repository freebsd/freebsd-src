/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)time.h	5.12 (Berkeley) 3/9/91
 */

#ifndef _TIME_H_
#define	_TIME_H_

#include <machine/ansi.h>

#ifndef	NULL
#define	NULL	0
#endif

#ifdef	_CLOCK_T_
typedef	_CLOCK_T_	clock_t;
#undef	_CLOCK_T_
#endif

#ifdef	_TIME_T_
typedef	_TIME_T_	time_t;
#undef	_TIME_T_
#endif

#ifdef	_SIZE_T_
typedef	_SIZE_T_	size_t;
#undef	_SIZE_T_
#endif

struct tm {
	int	tm_sec;		/* seconds after the minute [0-60] */
	int	tm_min;		/* minutes after the hour [0-59] */
	int	tm_hour;	/* hours since midnight [0-23] */
	int	tm_mday;	/* day of the month [1-31] */
	int	tm_mon;		/* months since January [0-11] */
	int	tm_year;	/* years since 1900 */
	int	tm_wday;	/* days since Sunday [0-6] */
	int	tm_yday;	/* days since January 1 [0-365] */
	int	tm_isdst;	/* Daylight Savings Time flag */
	long	tm_gmtoff;	/* offset from CUT in seconds */
	char	*tm_zone;	/* timezone abbreviation */
};

#include <sys/cdefs.h>

__BEGIN_DECLS
char *asctime __P((const struct tm *));
clock_t clock __P((void));
char *ctime __P((const time_t *));
double difftime __P((time_t, time_t));
struct tm *gmtime __P((const time_t *));
struct tm *localtime __P((const time_t *));
time_t mktime __P((struct tm *));
size_t strftime __P((char *, size_t, const char *, const struct tm *));
time_t time __P((time_t *));

#ifndef _ANSI_SOURCE
void tzset __P((void));
#endif /* not ANSI */

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
char *timezone __P((int, int));
void tzsetwall __P((void));
#endif /* neither ANSI nor POSIX */
__END_DECLS

#endif /* !_TIME_H_ */
