/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
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
 *
 */
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <pthread.h>
#include "thr_private.h"

/* #define DEBUG_THREAD_KERN */
#ifdef DEBUG_THREAD_KERN
#define DBG_MSG		stdout_debug
#else
#define DBG_MSG(x...)
#endif

static int _kern_idle_running = 0;
static struct timeval _kern_idle_timeout;

/* Static function prototype definitions: */
static void
thread_kern_idle(void);

static void
dequeue_signals(void);

static inline void
thread_run_switch_hook(pthread_t thread_out, pthread_t thread_in);

/* Static variables: */
static int	last_tick = 0;

void
_thread_kern_sched(void)
{
	struct timespec	ts;
	struct timeval	tv;
	struct pthread	*curthread = _get_curthread();
	unsigned int	current_tick;

	/* Get the current time of day. */
	GET_CURRENT_TOD(tv);
	TIMEVAL_TO_TIMESPEC(&tv, &ts);
	current_tick = _sched_ticks;

	/*
	 * Enter a critical section.
	 */
	_thread_kern_kse_mailbox.km_curthread = NULL;

	/*
	 * If this thread is becoming inactive, make note of the
	 * time.
	 */
	if (curthread->state != PS_RUNNING) {
		/*
		 * Save the current time as the time that the
		 * thread became inactive:
		 */
		curthread->last_inactive = (long)current_tick;
		if (curthread->last_inactive <
		    curthread->last_active) {
			/* Account for a rollover: */
			curthread->last_inactive =+
			    UINT_MAX + 1;
		}
	}

	/*
	 * Place this thread into the appropriate queue(s).
	 */
	switch (curthread->state) {
	case PS_DEAD:
	case PS_STATE_MAX: /* XXX: silences -Wall */
	case PS_SUSPENDED:
		/* Dead or suspended threads are not placed in any queue. */
		break;
	case PS_RUNNING:
		/*
		 * Save the current time as the time that the
		 * thread became inactive:
		 */
		current_tick = _sched_ticks;
		curthread->last_inactive = (long)current_tick;
		if (curthread->last_inactive <
		    curthread->last_active) {
			/* Account for a rollover: */
			curthread->last_inactive =+ UINT_MAX + 1;
		}

		if ((curthread->slice_usec != -1) &&
		   (curthread->attr.sched_policy != SCHED_FIFO)) {
			/*
			 * Accumulate the number of microseconds for
			 * which the current thread has run:
			 */
			curthread->slice_usec +=
			    (curthread->last_inactive -
			    curthread->last_active) *
			    (long)_clock_res_usec;
			/* Check for time quantum exceeded: */
			if (curthread->slice_usec > TIMESLICE_USEC)
				curthread->slice_usec = -1;
		}

		if (curthread->slice_usec == -1) {
			/*
			 * The thread exceeded its time
			 * quantum or it yielded the CPU;
			 * place it at the tail of the
			 * queue for its priority.
			 */
			PTHREAD_PRIOQ_INSERT_TAIL(curthread);
		} else {
			/*
			 * The thread hasn't exceeded its
			 * interval.  Place it at the head
			 * of the queue for its priority.
			 */
			PTHREAD_PRIOQ_INSERT_HEAD(curthread);
		}
		break;
	case PS_SPINBLOCK:
		/* Increment spinblock count. */
		_spinblock_count++;
		/*FALLTHROUGH*/
	case PS_DEADLOCK:
	case PS_JOIN:
	case PS_MUTEX_WAIT:
	case PS_WAIT_WAIT:
		/* No timeouts for these states. */
		curthread->wakeup_time.tv_sec = -1;
		curthread->wakeup_time.tv_nsec = -1;

		/* Restart the time slice. */
		curthread->slice_usec = -1;

		/* Insert into the waiting queue. */
		PTHREAD_WAITQ_INSERT(curthread);
		break;

	case PS_COND_WAIT:
	case PS_SLEEP_WAIT:
		/* These states can timeout. */
		/* Restart the time slice. */
		curthread->slice_usec = -1;

		/* Insert into the waiting queue. */
		PTHREAD_WAITQ_INSERT(curthread);
		break;
	}

	/* Switch into the scheduler's context. */
	DBG_MSG("Calling _thread_enter_uts()\n");
	_thread_enter_uts(&curthread->mailbox, &_thread_kern_kse_mailbox);
	DBG_MSG("Returned from _thread_enter_uts, thread %p\n", curthread);

	/*
	 * This point is reached when _thread_switch() is called
	 * to restore the state of a thread.
	 *
	 * This is the normal way out of the scheduler (for synchronous
	 * switches).
	 */

	/* XXXKSE: Do this inside _thread_kern_scheduler() */
	if (curthread->sig_defer_count == 0) {
		if (((curthread->cancelflags &
		    PTHREAD_AT_CANCEL_POINT) == 0) &&
		    ((curthread->cancelflags &
		    PTHREAD_CANCEL_ASYNCHRONOUS) != 0))
			/*
			 * Stick a cancellation point at the
			 * start of each async-cancellable
			 * thread's resumption.
			 *
			 * We allow threads woken at cancel
			 * points to do their own checks.
			 */
			pthread_testcancel();
	}

	if (_sched_switch_hook != NULL) {
		/* Run the installed switch hook: */
		thread_run_switch_hook(_last_user_thread, curthread);
	}
}

void
_thread_kern_scheduler(struct kse_mailbox *km)
{
	struct timespec	ts;
	struct timeval	tv;
	pthread_t	td, pthread, pthread_h;
	unsigned int	current_tick;
	struct kse_thr_mailbox	*tm, *p;

	DBG_MSG("entering\n");
	while (!TAILQ_EMPTY(&_thread_list)) {

		/* Get the current time of day. */
		ts = km->km_timeofday;
		TIMESPEC_TO_TIMEVAL(&_sched_tod, &ts);
		current_tick = _sched_ticks;

		/*
		 * Pick up threads that had blocked in the kernel and
		 * have now completed their trap (syscall, vm fault, etc).
		 * These threads were PS_RUNNING (and still are), but they
		 * need to be added to the run queue so that they can be
		 * scheduled again.
		 */
		DBG_MSG("Picking up km_completed\n");
		p = km->km_completed;
		km->km_completed = NULL;	/* XXX: Atomic xchg here. */
		while ((tm = p) != NULL) {
			p = tm->tm_next;
			tm->tm_next = NULL;
			if (tm->tm_udata == NULL) {
				DBG_MSG("\tidle context\n");
				_kern_idle_running = 0;
				continue;
			}
			DBG_MSG("\tmailbox=%p pthread=%p\n", tm, tm->tm_udata);
			PTHREAD_PRIOQ_INSERT_TAIL((pthread_t)tm->tm_udata);
		}

		/* Deliver posted signals. */
		/* XXX: Not yet. */
		DBG_MSG("Picking up signals\n");

		if (_spinblock_count != 0) {
			/*
			 * Enter a loop to look for threads waiting on
			 * a spinlock that is now available.
			 */
			PTHREAD_WAITQ_SETACTIVE();
			TAILQ_FOREACH(pthread, &_workq, qe) {
				if (pthread->state == PS_SPINBLOCK) {
					/*
					 * If the lock is available, let the
					 * thread run.
					 */
					if (pthread->data.spinlock->
					    access_lock == 0) {
						PTHREAD_WAITQ_CLEARACTIVE();
						PTHREAD_WORKQ_REMOVE(pthread);
						PTHREAD_NEW_STATE(pthread,
						    PS_RUNNING);
						PTHREAD_WAITQ_SETACTIVE();

						/*
						 * One less thread in a
						 * spinblock state:
						 */
						_spinblock_count--;
					}
				}
			}
			PTHREAD_WAITQ_CLEARACTIVE();
		}

		/* Wake up threads that have timed out.  */
		DBG_MSG("setactive\n");
		PTHREAD_WAITQ_SETACTIVE();
		DBG_MSG("Picking up timeouts (%x)\n", TAILQ_FIRST(&_waitingq));
		while (((pthread = TAILQ_FIRST(&_waitingq)) != NULL) &&
		    (pthread->wakeup_time.tv_sec != -1) &&
		    (((pthread->wakeup_time.tv_sec == 0) &&
		    (pthread->wakeup_time.tv_nsec == 0)) ||
		    (pthread->wakeup_time.tv_sec < ts.tv_sec) ||
		    ((pthread->wakeup_time.tv_sec == ts.tv_sec) &&
		    (pthread->wakeup_time.tv_nsec <= ts.tv_nsec)))) {
			DBG_MSG("\t...\n");
			/*
			 * Remove this thread from the waiting queue
			 * (and work queue if necessary) and place it
			 * in the ready queue.
			 */
			PTHREAD_WAITQ_CLEARACTIVE();
			if (pthread->flags & PTHREAD_FLAGS_IN_WORKQ)
				PTHREAD_WORKQ_REMOVE(pthread);
			DBG_MSG("\twaking thread\n");
			PTHREAD_NEW_STATE(pthread, PS_RUNNING);
			PTHREAD_WAITQ_SETACTIVE();
			/*
			 * Flag the timeout in the thread structure:
			 */
			pthread->timeout = 1;
		}
		DBG_MSG("clearactive\n");
		PTHREAD_WAITQ_CLEARACTIVE();

		/*
		 * Get the highest priority thread in the ready queue.
		 */
		DBG_MSG("Selecting thread\n");
		pthread_h = PTHREAD_PRIOQ_FIRST();

		/* Check if there are no threads ready to run: */
		if (pthread_h) {
			DBG_MSG("Scheduling thread\n");
			/* Remove the thread from the ready queue: */
			PTHREAD_PRIOQ_REMOVE(pthread_h);

			/* Make the selected thread the current thread: */
			_set_curthread(pthread_h);

			/*
			 * Save the current time as the time that the thread
			 * became active:
			 */
			current_tick = _sched_ticks;
			pthread_h->last_active = (long) current_tick;

			/*
			 * Check if this thread is running for the first time
			 * or running again after using its full time slice
			 * allocation:
			 */
			if (pthread_h->slice_usec == -1) {
				/* Reset the accumulated time slice period: */
				pthread_h->slice_usec = 0;
			}

			/*
			 * If we had a context switch, run any
			 * installed switch hooks.
			 */
			if ((_sched_switch_hook != NULL) &&
			    (_last_user_thread != pthread_h)) {
				thread_run_switch_hook(_last_user_thread,
				    pthread_h);
			}
			/*
			 * Continue the thread at its current frame:
			 */
			_last_user_thread = td;
			DBG_MSG("switch in\n");
			_thread_switch(&pthread_h->mailbox,
			    &_thread_kern_kse_mailbox.km_curthread);
			DBG_MSG("switch out\n");
		} else {
			/*
			 * There is nothing for us to do. Either
			 * yield, or idle until something wakes up.
			 */
			DBG_MSG("No runnable threads, idling.\n");
			if (_kern_idle_running) {
				DBG_MSG("kse_release");
				kse_release();
			}
			_kern_idle_running = 1;
			if ((pthread == NULL) ||
			    (pthread->wakeup_time.tv_sec == -1))
				/*
				 * Nothing is waiting on a timeout, so
				 * idling gains us nothing; spin.
				 */
				continue;
			TIMESPEC_TO_TIMEVAL(&_kern_idle_timeout,
			    &pthread->wakeup_time);
			_thread_switch(&_idle_thr_mailbox,
			    &_thread_kern_kse_mailbox.km_curthread);
		}
		DBG_MSG("looping\n");
	}
	/* There are no threads; exit. */
	DBG_MSG("No threads, exiting.\n");
	exit(0);
}

void
_thread_kern_sched_state(enum pthread_state state, char *fname, int lineno)
{
	struct pthread	*curthread = _get_curthread();

	/*
	 * Flag the pthread kernel as executing scheduler code
	 * to avoid an upcall from interrupting this execution
	 * and calling the scheduler again.
	 */
	_thread_kern_kse_mailbox.km_curthread = NULL;

	/* Change the state of the current thread: */
	curthread->state = state;
	curthread->fname = fname;
	curthread->lineno = lineno;

	/* Schedule the next thread that is ready: */
	_thread_kern_sched();
}

void
_thread_kern_sched_state_unlock(enum pthread_state state,
    spinlock_t *lock, char *fname, int lineno)
{
	struct pthread	*curthread = _get_curthread();

	/*
	 * Flag the pthread kernel as executing scheduler code
	 * to avoid an upcall from interrupting this execution
	 * and calling the scheduler again.
	 */
	_thread_kern_kse_mailbox.km_curthread = NULL;

	/* Change the state of the current thread: */
	curthread->state = state;
	curthread->fname = fname;
	curthread->lineno = lineno;

	_SPINUNLOCK(lock);

	/* Schedule the next thread that is ready: */
	_thread_kern_sched();
}

/*
 * Block until the next timeout.
 */
void
_thread_kern_idle(void)
{
	struct timespec ts;
	struct timeval timeout;

	for (;;) {
		timersub(&_kern_idle_timeout, &_sched_tod, &timeout);
		TIMEVAL_TO_TIMESPEC(&timeout, &ts);
		__sys_nanosleep(&ts, NULL);
	}
}

void
_thread_kern_set_timeout(const struct timespec * timeout)
{
	struct pthread	*curthread = _get_curthread();
	struct timespec current_time;
	struct timeval  tv;

	/* Reset the timeout flag for the running thread: */
	curthread->timeout = 0;

	/* Check if the thread is to wait forever: */
	if (timeout == NULL) {
		/*
		 * Set the wakeup time to something that can be recognised as
		 * different to an actual time of day:
		 */
		curthread->wakeup_time.tv_sec = -1;
		curthread->wakeup_time.tv_nsec = -1;
	}
	/* Check if no waiting is required: */
	else if (timeout->tv_sec == 0 && timeout->tv_nsec == 0) {
		/* Set the wake up time to 'immediately': */
		curthread->wakeup_time.tv_sec = 0;
		curthread->wakeup_time.tv_nsec = 0;
	} else {
		/* Get the current time: */
		GET_CURRENT_TOD(tv);
		TIMEVAL_TO_TIMESPEC(&tv, &current_time);

		/* Calculate the time for the current thread to wake up: */
		curthread->wakeup_time.tv_sec = current_time.tv_sec + timeout->tv_sec;
		curthread->wakeup_time.tv_nsec = current_time.tv_nsec + timeout->tv_nsec;

		/* Check if the nanosecond field needs to wrap: */
		if (curthread->wakeup_time.tv_nsec >= 1000000000) {
			/* Wrap the nanosecond field: */
			curthread->wakeup_time.tv_sec += 1;
			curthread->wakeup_time.tv_nsec -= 1000000000;
		}
	}
}

void
_thread_kern_sig_defer(void)
{
	struct pthread	*curthread = _get_curthread();

	/* Allow signal deferral to be recursive. */
	curthread->sig_defer_count++;
}

void
_thread_kern_sig_undefer(void)
{
	struct pthread	*curthread = _get_curthread();

	/*
	 * Perform checks to yield only if we are about to undefer
	 * signals.
	 */
	if (curthread->sig_defer_count > 1) {
		/* Decrement the signal deferral count. */
		curthread->sig_defer_count--;
	}
	else if (curthread->sig_defer_count == 1) {
		/* Reenable signals: */
		curthread->sig_defer_count = 0;

		/*
		 * Check for asynchronous cancellation before delivering any
		 * pending signals:
		 */
		if (((curthread->cancelflags & PTHREAD_AT_CANCEL_POINT) == 0) &&
		    ((curthread->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS) != 0))
			pthread_testcancel();
	}
}

static inline void
thread_run_switch_hook(pthread_t thread_out, pthread_t thread_in)
{
	pthread_t tid_out = thread_out;
	pthread_t tid_in = thread_in;

	if ((tid_out != NULL) &&
	    (tid_out->flags & PTHREAD_FLAGS_PRIVATE) != 0)
		tid_out = NULL;
	if ((tid_in != NULL) &&
	    (tid_in->flags & PTHREAD_FLAGS_PRIVATE) != 0)
		tid_in = NULL;

	if ((_sched_switch_hook != NULL) && (tid_out != tid_in)) {
		/* Run the scheduler switch hook: */
		_sched_switch_hook(tid_out, tid_in);
	}
}

struct pthread *
_get_curthread(void)
{
	if (_thread_initial == NULL)
		_thread_init();

	return (_thread_run);
}

void
_set_curthread(struct pthread *newthread)
{
	_thread_run = newthread;
}
