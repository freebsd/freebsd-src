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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <stdio.h>
#include <errno.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
nanosleep(struct timespec * time_to_sleep, struct timespec * time_remaining)
{
	int             ret = 0;
	struct timespec current_time;
	struct timespec current_time1;
	struct timespec remaining_time;
	struct timeval  tv;

	/* Check if the time to sleep is legal: */
#if	defined(__FreeBSD__)
	if (time_to_sleep == NULL || time_to_sleep->ts_nsec < 0 || time_to_sleep->ts_nsec > 1000000000) {
#else
	if (time_to_sleep == NULL || time_to_sleep->tv_nsec < 0 || time_to_sleep->tv_nsec > 1000000000) {
#endif
		/* Return an EINVAL error : */
		errno = EINVAL;
		ret = -1;
	} else {
		/* Get the current time: */
		gettimeofday(&tv, NULL);
		TIMEVAL_TO_TIMESPEC(&tv, &current_time);

		/* Calculate the time for the current thread to wake up: */
#if	defined(__FreeBSD__)
		_thread_run->wakeup_time.ts_sec = current_time.ts_sec + time_to_sleep->ts_sec;
		_thread_run->wakeup_time.ts_nsec = current_time.ts_nsec + time_to_sleep->ts_nsec;
#else
		_thread_run->wakeup_time.tv_sec = current_time.tv_sec + time_to_sleep->tv_sec;
		_thread_run->wakeup_time.tv_nsec = current_time.tv_nsec + time_to_sleep->tv_nsec;
#endif

		/* Check if the nanosecond field has overflowed: */
#if	defined(__FreeBSD__)
		if (_thread_run->wakeup_time.ts_nsec >= 1000000000) {
#else
		if (_thread_run->wakeup_time.tv_nsec >= 1000000000) {
#endif
			/* Wrap the nanosecond field: */
#if	defined(__FreeBSD__)
			_thread_run->wakeup_time.ts_sec += 1;
			_thread_run->wakeup_time.ts_nsec -= 1000000000;
#else
			_thread_run->wakeup_time.tv_sec += 1;
			_thread_run->wakeup_time.tv_nsec -= 1000000000;
#endif
		}

		/* Reschedule the current thread to sleep: */
		_thread_kern_sched_state(PS_SLEEP_WAIT, __FILE__, __LINE__);

		/* Get the current time: */
		gettimeofday(&tv, NULL);
		TIMEVAL_TO_TIMESPEC(&tv, &current_time1);

		/* Calculate the remaining time to sleep: */
#if	defined(__FreeBSD__)
		remaining_time.ts_sec = time_to_sleep->ts_sec + current_time.ts_sec - current_time1.ts_sec;
		remaining_time.ts_nsec = time_to_sleep->ts_nsec + current_time.ts_nsec - current_time1.ts_nsec;
#else
		remaining_time.tv_sec = time_to_sleep->tv_sec + current_time.tv_sec - current_time1.tv_sec;
		remaining_time.tv_nsec = time_to_sleep->tv_nsec + current_time.tv_nsec - current_time1.tv_nsec;
#endif

		/* Check if the nanosecond field has underflowed: */
#if	defined(__FreeBSD__)
		if (remaining_time.ts_nsec < 0) {
#else
		if (remaining_time.tv_nsec < 0) {
#endif
			/* Handle the underflow: */
#if	defined(__FreeBSD__)
			remaining_time.ts_sec -= 1;
			remaining_time.ts_nsec += 1000000000;
#else
			remaining_time.tv_sec -= 1;
			remaining_time.tv_nsec += 1000000000;
#endif
		}

		/* Check if the nanosecond field has overflowed: */
#if	defined(__FreeBSD__)
		if (remaining_time.ts_nsec >= 1000000000) {
#else
		if (remaining_time.tv_nsec >= 1000000000) {
#endif
			/* Handle the overflow: */
#if	defined(__FreeBSD__)
			remaining_time.ts_sec += 1;
			remaining_time.ts_nsec -= 1000000000;
#else
			remaining_time.tv_sec += 1;
			remaining_time.tv_nsec -= 1000000000;
#endif
		}

		/* Check if the sleep was longer than the required time: */
#if	defined(__FreeBSD__)
		if (remaining_time.ts_sec < 0) {
#else
		if (remaining_time.tv_sec < 0) {
#endif
			/* Reset the time left: */
#if	defined(__FreeBSD__)
			remaining_time.ts_sec = 0;
			remaining_time.ts_nsec = 0;
#else
			remaining_time.tv_sec = 0;
			remaining_time.tv_nsec = 0;
#endif
		}

		/* Check if the time remaining is to be returned: */
		if (time_remaining != NULL) {
			/* Return the actual time slept: */
#if	defined(__FreeBSD__)
			time_remaining->ts_sec = remaining_time.ts_sec;
			time_remaining->ts_nsec = remaining_time.ts_nsec;
#else
			time_remaining->tv_sec = remaining_time.tv_sec;
			time_remaining->tv_nsec = remaining_time.tv_nsec;
#endif
		}

		/* Check if the entire sleep was not completed: */
#if	defined(__FreeBSD__)
		if (remaining_time.ts_nsec != 0 || remaining_time.ts_sec != 0) {
#else
		if (remaining_time.tv_nsec != 0 || remaining_time.tv_sec != 0) {
#endif
			/* Return an EINTR error : */
			errno = EINTR;
			ret = -1;
		}
	}
	return (ret);
}
#endif
