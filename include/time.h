/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)time.h	8.3 (Berkeley) 1/21/94
 */

#ifndef _TIME_H_
#define	_TIME_H_

#include <machine/ansi.h>

#ifndef _ANSI_SOURCE
/*
 * Frequency of the clock ticks reported by times().  Deprecated - use
 * sysconf(_SC_CLK_TCK) instead.
 */
#define	CLK_TCK		_BSD_CLK_TCK_
#endif

/* Frequency of the clock ticks reported by clock().  */
#define	CLOCKS_PER_SEC	_BSD_CLOCKS_PER_SEC_

#ifndef	NULL
#define	NULL	0
#endif

#ifdef	_BSD_CLOCK_T_
typedef	_BSD_CLOCK_T_	clock_t;
#undef	_BSD_CLOCK_T_
#endif

#ifdef	_BSD_TIME_T_
typedef	_BSD_TIME_T_	time_t;
#undef	_BSD_TIME_T_
#endif

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
/*
 * New in POSIX 1003.1b-1993.
 */
#ifdef	_BSD_CLOCKID_T_
typedef	_BSD_CLOCKID_T_	clockid_t;
#undef	_BSD_CLOCKID_T_
#endif

#ifdef	_BSD_TIMER_T_
typedef	_BSD_TIMER_T_	timer_t;
#undef	_BSD_TIMER_T_
#endif

#ifndef _TIMESPEC_DECLARED
#define _TIMESPEC_DECLARED
struct timespec {
	time_t	tv_sec;		/* seconds */
	long	tv_nsec;	/* and nanoseconds */
};
#endif
#endif /* Neither ANSI nor POSIX */

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

#ifdef	_THREAD_SAFE
int asctime_r __P((const struct tm *, char *, int));
int ctime_r __P((const time_t *, char *, int));
struct tm *gmtime_r __P((const time_t *, struct tm *));
struct tm *localtime_r __P((const time_t *, struct tm *));
#endif

#ifndef _ANSI_SOURCE
void tzset __P((void));
#endif /* not ANSI */

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
char *timezone __P((int, int));
void tzsetwall __P((void));
time_t timelocal __P((struct tm * const));
time_t timegm __P((struct tm * const));

/* Introduced in POSIX 1003.1b-1993, not part of 1003.1-1990. */
int clock_getres __P((clockid_t, struct timespec *));
int clock_gettime __P((clockid_t, struct timespec *));
int clock_settime __P((clockid_t, const struct timespec *));
int nanosleep __P((const struct timespec *, struct timespec *));
#endif /* neither ANSI nor POSIX */
__END_DECLS

#endif /* !_TIME_H_ */
