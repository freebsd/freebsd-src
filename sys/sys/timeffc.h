/*-
 * Copyright (c) 2011 The University of Melbourne
 * All rights reserved.
 *
 * This software was developed by Julien Ridoux at the University of Melbourne
 * under sponsorship from the FreeBSD Foundation.
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

#ifndef _SYS_TIMEFF_H_
#define _SYS_TIMEFF_H_

#include <sys/_ffcounter.h>

/*
 * Feed-forward clock estimate
 * Holds time mark as a ffcounter and conversion to bintime based on current
 * timecounter period and offset estimate passed by the synchronization daemon.
 * Provides time of last daemon update, clock status and bound on error.
 */
struct ffclock_estimate {
	struct bintime	update_time;	/* Time of last estimates update. */
	ffcounter	update_ffcount;	/* Counter value at last update. */
	ffcounter	leapsec_next;	/* Counter value of next leap second. */
	uint64_t	period;		/* Estimate of counter period. */
	uint32_t	errb_abs;	/* Bound on absolute clock error [ns]. */
	uint32_t	errb_rate;	/* Bound on counter rate error [ps/s]. */
	uint32_t	status;		/* Clock status. */
	int16_t		leapsec_total;	/* All leap seconds seen so far. */
	int8_t		leapsec;	/* Next leap second (in {-1,0,1}). */
};

#if __BSD_VISIBLE
#ifdef _KERNEL

/*
 * Index into the sysclocks array for obtaining the ASCII name of a particular
 * sysclock.
 */
#define	SYSCLOCK_FBCK	0
#define	SYSCLOCK_FFWD	1

/*
 * Parameters of counter characterisation required by feed-forward algorithms.
 */
#define	FFCLOCK_SKM_SCALE	1024

/*
 * Feed-forward clock status
 */
#define	FFCLOCK_STA_UNSYNC	1
#define	FFCLOCK_STA_WARMUP	2

/*
 * Clock flags to select how the feed-forward counter is converted to absolute
 * time by ffclock_convert_abs().
 * FAST:    do not read the hardware counter, return feed-forward clock time
 *          at last tick. The time returned has the resolution of the kernel
 *          tick (1/hz [s]).
 * LERP:    linear interpolation of ffclock time to guarantee monotonic time.
 * LEAPSEC: include leap seconds.
 * UPTIME:  removes time of boot.
 */
#define	FFCLOCK_FAST		1
#define	FFCLOCK_LERP		2
#define	FFCLOCK_LEAPSEC		4
#define	FFCLOCK_UPTIME		8

/* Resets feed-forward clock from RTC */
void ffclock_reset_clock(struct timespec *ts);

/*
 * Return the current value of the feed-forward clock counter. Essential to
 * measure time interval in counter units. If a fast timecounter is used by the
 * system, may also allow fast but accurate timestamping.
 */
void ffclock_read_counter(ffcounter *ffcount);

/*
 * Retrieve feed-forward counter value and time of last kernel tick. This
 * accepts the FFCLOCK_LERP flag.
 */
void ffclock_last_tick(ffcounter *ffcount, struct bintime *bt, uint32_t flags);

/*
 * Low level routines to convert a counter timestamp into absolute time and a
 * counter timestamp interval into an interval in seconds. The absolute time
 * conversion accepts the FFCLOCK_LERP flag.
 */
void ffclock_convert_abs(ffcounter ffcount, struct bintime *bt, uint32_t flags);
void ffclock_convert_diff(ffcounter ffdelta, struct bintime *bt);

/*
 * Feed-forward clock routines.
 *
 * These functions rely on the timecounters and ffclock_estimates stored in
 * fftimehands. Note that the error_bound parameter is not the error of the
 * clock but an upper bound on the error of the absolute time or time interval
 * returned.
 *
 * ffclock_abstime(): retrieves current time as counter value and convert this
 *     timestamp in seconds. The value (in seconds) of the converted timestamp
 *     depends on the flags passed: for a given counter value, different
 *     conversions are possible. Different clock models can be selected by
 *     combining flags (for example (FFCLOCK_LERP|FFCLOCK_UPTIME) produces
 *     linearly interpolated uptime).
 * ffclock_difftime(): computes a time interval in seconds based on an interval
 *     measured in ffcounter units. This should be the preferred way to measure
 *     small time intervals very accurately.
 */
void ffclock_abstime(ffcounter *ffcount, struct bintime *bt,
    struct bintime *error_bound, uint32_t flags);
void ffclock_difftime(ffcounter ffdelta, struct bintime *bt,
    struct bintime *error_bound);

/*
 * Wrapper routines to return current absolute time using the feed-forward
 * clock. These functions are named after those defined in <sys/time.h>, which
 * contains a description of the original ones.
 */
void ffclock_bintime(struct bintime *bt);
void ffclock_nanotime(struct timespec *tsp);
void ffclock_microtime(struct timeval *tvp);

void ffclock_getbintime(struct bintime *bt);
void ffclock_getnanotime(struct timespec *tsp);
void ffclock_getmicrotime(struct timeval *tvp);

void ffclock_binuptime(struct bintime *bt);
void ffclock_nanouptime(struct timespec *tsp);
void ffclock_microuptime(struct timeval *tvp);

void ffclock_getbinuptime(struct bintime *bt);
void ffclock_getnanouptime(struct timespec *tsp);
void ffclock_getmicrouptime(struct timeval *tvp);

/*
 * Wrapper routines to convert a time interval specified in ffcounter units into
 * seconds using the current feed-forward clock estimates.
 */
void ffclock_bindifftime(ffcounter ffdelta, struct bintime *bt);
void ffclock_nanodifftime(ffcounter ffdelta, struct timespec *tsp);
void ffclock_microdifftime(ffcounter ffdelta, struct timeval *tvp);

#else /* !_KERNEL */

/* Feed-Forward Clock system calls. */
__BEGIN_DECLS
int ffclock_getcounter(ffcounter *ffcount);
int ffclock_getestimate(struct ffclock_estimate *cest);
int ffclock_setestimate(struct ffclock_estimate *cest);
__END_DECLS

#endif /* _KERNEL */
#endif /* __BSD_VISIBLE */
#endif /* _SYS_TIMEFF_H_ */
