/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 *
 *	$NetBSD: clock_subr.h,v 1.7 2000/10/03 13:41:07 tsutsui Exp $
 *
 * $FreeBSD: src/sys/sys/clock.h,v 1.7.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _SYS_CLOCK_H_
#define _SYS_CLOCK_H_

#ifdef _KERNEL		/* No user serviceable parts */

/*
 * Kernel to clock driver interface.
 */
void	inittodr(time_t base);
void	resettodr(void);
void	startrtclock(void);

extern int	disable_rtc_set;

/*
 * Timezone info from settimeofday(2), usually not used
 */
extern int tz_minuteswest;
extern int tz_dsttime;

int utc_offset(void);

/*
 * Structure to hold the values typically reported by time-of-day clocks.
 * This can be passed to the generic conversion functions to be converted
 * to a struct timespec.
 */
struct clocktime {
	int	year;			/* year (4 digit year) */
	int	mon;			/* month (1 - 12) */
	int	day;			/* day (1 - 31) */
	int	hour;			/* hour (0 - 23) */
	int	min;			/* minute (0 - 59) */
	int	sec;			/* second (0 - 59) */
	int	dow;			/* day of week (0 - 6; 0 = Sunday) */
	long	nsec;			/* nano seconds */
};

int clock_ct_to_ts(struct clocktime *, struct timespec *);
void clock_ts_to_ct(struct timespec *, struct clocktime *);
void clock_register(device_t, long);

/*
 * BCD to decimal and decimal to BCD.
 */
#define	FROMBCD(x)	bcd2bin(x)
#define	TOBCD(x)	bin2bcd(x)

/* Some handy constants. */
#define SECDAY		(24 * 60 * 60)
#define SECYR		(SECDAY * 365)

/* Traditional POSIX base year */
#define	POSIX_BASE_YEAR	1970

void timespec2fattime(struct timespec *tsp, int utc, u_int16_t *ddp, u_int16_t *dtp, u_int8_t *dhp);
void fattime2timespec(unsigned dd, unsigned dt, unsigned dh, int utc, struct timespec *tsp);

#endif /* _KERNEL */

#endif /* !_SYS_CLOCK_H_ */
