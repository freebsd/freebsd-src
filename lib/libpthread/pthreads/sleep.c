/* ==== sleep.c ============================================================
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@mit.edu
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
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : Condition cariable functions.
 *
 *  1.00 93/12/28 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <unistd.h>

struct pthread * pthread_sleep = NULL;
semaphore	sleep_semaphore = SEMAPHORE_CLEAR;


#include	<sys/time.h>
#include	<stdio.h>

/* ==========================================================================
 * machdep_start_timer()
 */
int machdep_start_timer(struct itimerval *start_time_val)
{
	setitimer(ITIMER_REAL, start_time_val, NULL);
	return(OK);
}

/* ==========================================================================
 * machdep_stop_timer()
 */
struct itimerval stop_time_val = { { 0, 0 }, { 0, 0 } };
int machdep_stop_timer(struct itimerval * current)
{
	setitimer(ITIMER_REAL, &stop_time_val, current);
	return(OK);
}

/* ==========================================================================
 * machdep_sub_timer()
 *
 * formula is: new -= current;
 */
static inline void machdep_sub_timer(struct itimerval * new,
  struct itimerval * current)
{
	new->it_value.tv_usec -= current->it_value.tv_usec;
	if (new->it_value.tv_usec < 0) {
		new->it_value.tv_usec += 1000000;
		new->it_value.tv_sec--;
	}
	new->it_value.tv_sec -= current->it_value.tv_sec;
}


/* ==========================================================================
 * sleep_wakeup()
 *
 * This routine is called by the interrupt handler. It cannot call
 * pthread_yield() thenrfore it returns NOTOK to inform the handler
 * that it will have to be called at a later time.
 */
int sleep_wakeup()
{
	struct pthread *pthread_sleep_next;
	struct itimerval current_time;
	semaphore *lock, *plock;

	/* Lock sleep queue */
	lock = &(sleep_semaphore);
	if (SEMAPHORE_TEST_AND_SET(lock)) {
		return(NOTOK);
	}
	if (pthread_sleep) {

		plock = &(pthread_sleep->lock);
		if (SEMAPHORE_TEST_AND_SET(plock)) {
			SEMAPHORE_RESET(lock);
			return(NOTOK);
		}

		/* return remaining time */
		machdep_stop_timer(&current_time);
		pthread_sleep->time_sec = current_time.it_value.tv_sec;
		pthread_sleep->time_usec = current_time.it_value.tv_usec;

		if (pthread_sleep_next = pthread_sleep->sll) {
			pthread_sleep_next->time_usec += current_time.it_value.tv_usec;
			current_time.it_value.tv_usec = pthread_sleep_next->time_usec;
			pthread_sleep_next->time_sec += current_time.it_value.tv_sec;
			current_time.it_value.tv_sec = pthread_sleep_next->time_sec;
			machdep_start_timer(&current_time);
		}

		/* Clean up removed thread and start it runnng again. */
		pthread_sleep->state = PS_RUNNING;
		pthread_sleep->sll = NULL;
		SEMAPHORE_RESET(plock);

		/* Set top of queue to next queue item */
		pthread_sleep = pthread_sleep_next;
	}
	SEMAPHORE_RESET(lock);
	return(OK);
}

/* ==========================================================================
 * sleep()
 */
unsigned int sleep(unsigned int seconds)
{
	struct pthread *pthread_sleep_current, *pthread_sleep_prev;
	struct itimerval current_time, new_time;
	semaphore *lock, *plock;

	if (seconds) {
		/* Lock current thread */
		plock = &(pthread_run->lock);
		while (SEMAPHORE_TEST_AND_SET(plock)) {
			pthread_yield();
		}

		/* Set new_time timer value */
		new_time.it_value.tv_usec = 0;
		new_time.it_value.tv_sec = seconds;

		/* Lock sleep queue */
		lock = &(sleep_semaphore);
		while (SEMAPHORE_TEST_AND_SET(lock)) {
			pthread_yield();
		}

		/* any threads? */
		if (pthread_sleep_current = pthread_sleep) {
			
			machdep_stop_timer(&current_time);

			/* Is remaining time left <= new thread time */
			if (current_time.it_value.tv_sec < new_time.it_value.tv_sec) { 
				machdep_sub_timer(&new_time, &current_time);
				machdep_start_timer(&current_time);

				while (pthread_sleep_current->sll) {
					pthread_sleep_prev =  pthread_sleep_current;
					pthread_sleep_current = pthread_sleep_current->sll;
					current_time.it_value.tv_sec = pthread_sleep_current->time_sec;
					
					if ((current_time.it_value.tv_sec > new_time.it_value.tv_sec) ||
					  ((current_time.it_value.tv_sec == new_time.it_value.tv_sec) &&
					  (current_time.it_value.tv_usec > new_time.it_value.tv_usec))) {
						pthread_run->time_usec = new_time.it_value.tv_usec;
						pthread_run->time_sec = new_time.it_value.tv_sec;
						machdep_sub_timer(&current_time, &new_time);
						pthread_run->sll = pthread_sleep_current;
						pthread_sleep_prev->sll = pthread_run;

						/* Unlock sleep mutex */
						SEMAPHORE_RESET(lock);
	
						/* Reschedule thread */
						reschedule(PS_SLEEP_WAIT);

						return(pthread_run->time_sec);
					}
					machdep_sub_timer(&new_time, &current_time);

				} 

				/* No more threads in queue, attach pthread_run to end of list */
				pthread_sleep_current->sll = pthread_run;
				pthread_run->sll = NULL;

			} else {
				/* Start timer and enqueue thread */
				machdep_start_timer(&new_time);
				current_time.it_value.tv_sec -= new_time.it_value.tv_sec;
				pthread_run->sll = pthread_sleep_current;
				pthread_sleep = pthread_run;
			}
		} else {
			/* Start timer and enqueue thread */
			machdep_start_timer(&new_time);
			pthread_sleep = pthread_run;
			pthread_run->sll = NULL;
		}

		pthread_run->time_usec = new_time.it_value.tv_usec;
		pthread_run->time_sec = new_time.it_value.tv_sec;

		/* Unlock sleep mutex */
		SEMAPHORE_RESET(lock);
	
		/* Reschedule thread */
		reschedule(PS_SLEEP_WAIT);

	}
	return(pthread_run->time_sec);
}

