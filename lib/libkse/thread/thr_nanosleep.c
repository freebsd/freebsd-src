/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
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
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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
 *
 * $FreeBSD$
 */
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include "thr_private.h"

__weak_reference(__nanosleep, nanosleep);

int
_nanosleep(const struct timespec *time_to_sleep,
    struct timespec *time_remaining)
{
	struct pthread	*curthread = _get_curthread();
	int             ret = 0;
	struct timespec ts, ts1;
	struct timespec remaining_time;

	/* Check if the time to sleep is legal: */
	if ((time_to_sleep == NULL) || (time_to_sleep->tv_sec < 0) ||
	    (time_to_sleep->tv_nsec < 0) ||
	    (time_to_sleep->tv_nsec >= 1000000000)) {
		/* Return an EINVAL error : */
		errno = EINVAL;
		ret = -1;
	} else {
		if (!_kse_isthreaded() ||
		    (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM))
			return (__sys_nanosleep(time_to_sleep, time_remaining));
			
		KSE_GET_TOD(curthread->kse, &ts);

		/* Calculate the time for the current thread to wake up: */
		TIMESPEC_ADD(&curthread->wakeup_time, &ts, time_to_sleep);

		THR_LOCK_SWITCH(curthread);
		curthread->interrupted = 0;
		THR_SET_STATE(curthread, PS_SLEEP_WAIT);

		/* Reschedule the current thread to sleep: */
		_thr_sched_switch_unlocked(curthread);

		/* Calculate the remaining time to sleep: */
		KSE_GET_TOD(curthread->kse, &ts1);
		remaining_time.tv_sec = time_to_sleep->tv_sec
		    + ts.tv_sec - ts1.tv_sec;
		remaining_time.tv_nsec = time_to_sleep->tv_nsec
		    + ts.tv_nsec - ts1.tv_nsec;

		/* Check if the nanosecond field has underflowed: */
		if (remaining_time.tv_nsec < 0) {
			/* Handle the underflow: */
			remaining_time.tv_sec -= 1;
			remaining_time.tv_nsec += 1000000000;
		}
		/* Check if the nanosecond field has overflowed: */
		else if (remaining_time.tv_nsec >= 1000000000) {
			/* Handle the overflow: */
			remaining_time.tv_sec += 1;
			remaining_time.tv_nsec -= 1000000000;
		}

		/* Check if the sleep was longer than the required time: */
		if (remaining_time.tv_sec < 0) {
			/* Reset the time left: */
			remaining_time.tv_sec = 0;
			remaining_time.tv_nsec = 0;
		}

		/* Check if the time remaining is to be returned: */
		if (time_remaining != NULL) {
			/* Return the actual time slept: */
			time_remaining->tv_sec = remaining_time.tv_sec;
			time_remaining->tv_nsec = remaining_time.tv_nsec;
		}

		/* Check if the sleep was interrupted: */
		if (curthread->interrupted) {
			/* Return an EINTR error : */
			errno = EINTR;
			ret = -1;
		}
	}
	return (ret);
}

int
__nanosleep(const struct timespec *time_to_sleep,
    struct timespec *time_remaining)
{
	struct pthread *curthread = _get_curthread();
	int		ret;

	_thr_enter_cancellation_point(curthread);
	ret = _nanosleep(time_to_sleep, time_remaining);
	_thr_leave_cancellation_point(curthread);

	return (ret);
}
