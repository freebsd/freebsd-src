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
_nanosleep(const struct timespec * time_to_sleep,
    struct timespec * time_remaining)
{
	struct pthread	*curthread = _get_curthread();
	int             ret = 0;
	struct timespec current_time;
	struct timespec current_time1;
	struct timespec remaining_time;
	struct timeval  tv;

	/* Check if the time to sleep is legal: */
	if (time_to_sleep == NULL || time_to_sleep->tv_sec < 0 ||
		time_to_sleep->tv_nsec < 0 || time_to_sleep->tv_nsec >= 1000000000) {
		/* Return an EINVAL error : */
		errno = EINVAL;
		ret = -1;
	} else {
		/*
		 * As long as we're going to get the time of day, we
		 * might as well store it in the global time of day:
		 */
		gettimeofday((struct timeval *) &_sched_tod, NULL);
		GET_CURRENT_TOD(tv);
		TIMEVAL_TO_TIMESPEC(&tv, &current_time);

		/* Calculate the time for the current thread to wake up: */
		curthread->wakeup_time.tv_sec = current_time.tv_sec + time_to_sleep->tv_sec;
		curthread->wakeup_time.tv_nsec = current_time.tv_nsec + time_to_sleep->tv_nsec;

		/* Check if the nanosecond field has overflowed: */
		if (curthread->wakeup_time.tv_nsec >= 1000000000) {
			/* Wrap the nanosecond field: */
			curthread->wakeup_time.tv_sec += 1;
			curthread->wakeup_time.tv_nsec -= 1000000000;
		}
		curthread->interrupted = 0;

		/* Reschedule the current thread to sleep: */
		_thread_kern_sched_state(PS_SLEEP_WAIT, __FILE__, __LINE__);

		/*
		 * As long as we're going to get the time of day, we
		 * might as well store it in the global time of day:
		 */
		gettimeofday((struct timeval *) &_sched_tod, NULL);
		GET_CURRENT_TOD(tv);
		TIMEVAL_TO_TIMESPEC(&tv, &current_time1);

		/* Calculate the remaining time to sleep: */
		remaining_time.tv_sec = time_to_sleep->tv_sec + current_time.tv_sec - current_time1.tv_sec;
		remaining_time.tv_nsec = time_to_sleep->tv_nsec + current_time.tv_nsec - current_time1.tv_nsec;

		/* Check if the nanosecond field has underflowed: */
		if (remaining_time.tv_nsec < 0) {
			/* Handle the underflow: */
			remaining_time.tv_sec -= 1;
			remaining_time.tv_nsec += 1000000000;
		}

		/* Check if the nanosecond field has overflowed: */
		if (remaining_time.tv_nsec >= 1000000000) {
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
__nanosleep(const struct timespec * time_to_sleep, struct timespec *
    time_remaining)
{
	int	ret;

	_thread_enter_cancellation_point();
	ret = _nanosleep(time_to_sleep, time_remaining);
	_thread_leave_cancellation_point();

	return ret;
}
