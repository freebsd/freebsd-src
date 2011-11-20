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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeffc.h>

extern struct ffclock_estimate ffclock_estimate;
extern struct bintime ffclock_boottime;

/*
 * Feed-forward clock absolute time. This should be the preferred way to read
 * the feed-forward clock for "wall-clock" type time. The flags allow to compose
 * various flavours of absolute time (e.g. with or without leap seconds taken
 * into account). If valid pointers are provided, the ffcounter value and an
 * upper bound on clock error associated with the bintime are provided.
 * NOTE: use ffclock_convert_abs() to differ the conversion of a ffcounter value
 * read earlier.
 */
void
ffclock_abstime(ffcounter *ffcount, struct bintime *bt,
    struct bintime *error_bound, uint32_t flags)
{
	struct ffclock_estimate cest;
	ffcounter ffc;
	ffcounter update_ffcount;
	ffcounter ffdelta_error;

	/* Get counter and corresponding time. */
	if ((flags & FFCLOCK_FAST) == FFCLOCK_FAST)
		ffclock_last_tick(&ffc, bt, flags);
	else {
		ffclock_read_counter(&ffc);
		ffclock_convert_abs(ffc, bt, flags);
	}

	/* Current ffclock estimate, use update_ffcount as generation number. */
	do {
		update_ffcount = ffclock_estimate.update_ffcount;
		bcopy(&ffclock_estimate, &cest, sizeof(struct ffclock_estimate));
	} while (update_ffcount != ffclock_estimate.update_ffcount);

	/*
	 * Leap second adjustment. Total as seen by synchronisation algorithm
	 * since it started. cest.leapsec_next is the ffcounter prediction of
	 * when the next leapsecond occurs.
	 */
	if ((flags & FFCLOCK_LEAPSEC) == FFCLOCK_LEAPSEC) {
		bt->sec -= cest.leapsec_total;
		if (ffc > cest.leapsec_next)
			bt->sec -= cest.leapsec;
	}

	/* Boot time adjustment, for uptime/monotonic clocks. */
	if ((flags & FFCLOCK_UPTIME) == FFCLOCK_UPTIME) {
		bintime_sub(bt, &ffclock_boottime);
	}

	/* Compute error bound if a valid pointer has been passed. */
	if (error_bound) {
		ffdelta_error = ffc - cest.update_ffcount;
		ffclock_convert_diff(ffdelta_error, error_bound);
		/* 18446744073709 = int(2^64/1e12), err_bound_rate in [ps/s] */
		bintime_mul(error_bound, cest.errb_rate *
		    (uint64_t)18446744073709LL);
		/* 18446744073 = int(2^64 / 1e9), since err_abs in [ns] */
		bintime_addx(error_bound, cest.errb_abs *
		    (uint64_t)18446744073LL);
	}

	if (ffcount)
		*ffcount = ffc;
}

/*
 * Feed-forward difference clock. This should be the preferred way to convert a
 * time interval in ffcounter values into a time interval in seconds. If a valid
 * pointer is passed, an upper bound on the error in computing the time interval
 * in seconds is provided.
 */
void
ffclock_difftime(ffcounter ffdelta, struct bintime *bt,
    struct bintime *error_bound)
{
	ffcounter update_ffcount;
	uint32_t err_rate;

	ffclock_convert_diff(ffdelta, bt);

	if (error_bound) {
		do {
			update_ffcount = ffclock_estimate.update_ffcount;
			err_rate = ffclock_estimate.errb_rate;
		} while (update_ffcount != ffclock_estimate.update_ffcount);

		ffclock_convert_diff(ffdelta, error_bound);
		/* 18446744073709 = int(2^64/1e12), err_bound_rate in [ps/s] */
		bintime_mul(error_bound, err_rate * (uint64_t)18446744073709LL);
	}
}
