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
#include <setjmp.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <fcntl.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Static function prototype definitions: */
static void 
_thread_kern_poll(int wait_reqd);

static void
dequeue_signals(void);

static inline void
thread_run_switch_hook(pthread_t thread_out, pthread_t thread_in);

void
_thread_kern_sched(ucontext_t * scp)
{
#ifndef	__alpha__
	char           *fdata;
#endif
	pthread_t       pthread, pthread_h = NULL;
	struct itimerval itimer;
	struct timespec ts, ts1;
	struct timeval  tv, tv1;
	int		set_timer = 0;

	/*
	 * Flag the pthread kernel as executing scheduler code
	 * to avoid a scheduler signal from interrupting this
	 * execution and calling the scheduler again.
	 */
	_thread_kern_in_sched = 1;

	/* Check if this function was called from the signal handler: */
	if (scp != NULL) {
		/*
		 * Copy the signal context to the current thread's jump
		 * buffer: 
		 */
		memcpy(&_thread_run->saved_sigcontext, scp, sizeof(_thread_run->saved_sigcontext));

#ifndef	__alpha__
		/* Point to the floating point data in the running thread: */
		fdata = _thread_run->saved_fp;

		/* Save the floating point data: */
__asm__("fnsave %0": :"m"(*fdata));
#endif

		/* Flag the signal context as the last state saved: */
		_thread_run->sig_saved = 1;
	}
	/* Save the state of the current thread: */
	else if (setjmp(_thread_run->saved_jmp_buf) != 0) {
		/*
		 * This point is reached when a longjmp() is called to
		 * restore the state of a thread. 
		 *
		 * This is the normal way out of the scheduler.
		 */
		_thread_kern_in_sched = 0;

		if (((_thread_run->cancelflags & PTHREAD_AT_CANCEL_POINT) == 0) &&
		    ((_thread_run->cancelflags & PTHREAD_CANCEL_ASYNCHRONOUS) != 0)) {
			/* 
			 * Cancellations override signals.
			 *
			 * Stick a cancellation point at the start of
			 * each async-cancellable thread's resumption.
			 *
			 * We allow threads woken at cancel points to do their
			 * own checks.
			 */
			pthread_testcancel();
		}

		/*
		 * Check for undispatched signals due to calls to
		 * pthread_kill().
		 */
		if (SIGNOTEMPTY(_thread_run->sigpend))
			_dispatch_signals();

		if (_sched_switch_hook != NULL) {
			/* Run the installed switch hook: */
			thread_run_switch_hook(_last_user_thread, _thread_run);
		}

		return;
	} else
		/* Flag the jump buffer was the last state saved: */
		_thread_run->sig_saved = 0;

	/* If the currently running thread is a user thread, save it: */
	if ((_thread_run->flags & PTHREAD_FLAGS_PRIVATE) == 0)
		_last_user_thread = _thread_run;

	/*
	 * Enter a scheduling loop that finds the next thread that is
	 * ready to run. This loop completes when there are no more threads
	 * in the global list or when a thread has its state restored by
	 * either a sigreturn (if the state was saved as a sigcontext) or a
	 * longjmp (if the state was saved by a setjmp). 
	 */
	while (!(TAILQ_EMPTY(&_thread_list))) {
		/* Get the current time of day: */
		gettimeofday(&tv, NULL);
		TIMEVAL_TO_TIMESPEC(&tv, &ts);

		/*
		 * Protect the scheduling queues from access by the signal
		 * handler.
		 */
		_queue_signals = 1;

		if (_thread_run != &_thread_kern_thread) {

			/*
			 * This thread no longer needs to yield the CPU.
			 */
			_thread_run->yield_on_sig_undefer = 0;
	
			/*
			 * Save the current time as the time that the thread
			 * became inactive: 
			 */
			_thread_run->last_inactive.tv_sec = tv.tv_sec;
			_thread_run->last_inactive.tv_usec = tv.tv_usec;
	
			/*
			 * Place the currently running thread into the
			 * appropriate queue(s).
			 */
			switch (_thread_run->state) {
			case PS_DEAD:
			case PS_STATE_MAX: /* to silence -Wall */
			case PS_SUSPENDED:
				/*
				 * Dead and suspended threads are not placed
				 * in any queue:
				 */
				break;

			case PS_RUNNING:
				/*
				 * Runnable threads can't be placed in the
				 * priority queue until after waiting threads
				 * are polled (to preserve round-robin
				 * scheduling).
				 */
				if ((_thread_run->slice_usec != -1) &&
				    (_thread_run->attr.sched_policy != SCHED_FIFO)) {
					/*
					 * Accumulate the number of microseconds that
					 * this thread has run for:
					 */
					_thread_run->slice_usec +=
					    (_thread_run->last_inactive.tv_sec -
					    _thread_run->last_active.tv_sec) * 1000000 +
					    _thread_run->last_inactive.tv_usec -
					    _thread_run->last_active.tv_usec;
	
					/* Check for time quantum exceeded: */
					if (_thread_run->slice_usec > TIMESLICE_USEC)
						_thread_run->slice_usec = -1;
				}
				break;

			/*
			 * States which do not depend on file descriptor I/O
			 * operations or timeouts: 
			 */
			case PS_DEADLOCK:
			case PS_FDLR_WAIT:
			case PS_FDLW_WAIT:
			case PS_FILE_WAIT:
			case PS_JOIN:
			case PS_MUTEX_WAIT:
			case PS_SIGSUSPEND:
			case PS_SIGTHREAD:
			case PS_SIGWAIT:
			case PS_WAIT_WAIT:
				/* No timeouts for these states: */
				_thread_run->wakeup_time.tv_sec = -1;
				_thread_run->wakeup_time.tv_nsec = -1;

				/* Restart the time slice: */
				_thread_run->slice_usec = -1;

				/* Insert into the waiting queue: */
				PTHREAD_WAITQ_INSERT(_thread_run);
				break;

			/* States which can timeout: */
			case PS_COND_WAIT:
			case PS_SLEEP_WAIT:
				/* Restart the time slice: */
				_thread_run->slice_usec = -1;

				/* Insert into the waiting queue: */
				PTHREAD_WAITQ_INSERT(_thread_run);
				break;
	
			/* States that require periodic work: */
			case PS_SPINBLOCK:
				/* No timeouts for this state: */
				_thread_run->wakeup_time.tv_sec = -1;
				_thread_run->wakeup_time.tv_nsec = -1;

				/* Increment spinblock count: */
				_spinblock_count++;

				/* fall through */
			case PS_FDR_WAIT:
			case PS_FDW_WAIT:
			case PS_POLL_WAIT:
			case PS_SELECT_WAIT:
				/* Restart the time slice: */
				_thread_run->slice_usec = -1;
	
				/* Insert into the waiting queue: */
				PTHREAD_WAITQ_INSERT(_thread_run);
	
				/* Insert into the work queue: */
				PTHREAD_WORKQ_INSERT(_thread_run);
				break;
			}
		}

		/* Unprotect the scheduling queues: */
		_queue_signals = 0;

		/*
		 * Poll file descriptors to update the state of threads
		 * waiting on file I/O where data may be available: 
		 */
		_thread_kern_poll(0);

		/* Protect the scheduling queues: */
		_queue_signals = 1;

		/*
		 * Wake up threads that have timedout.  This has to be
		 * done after polling in case a thread does a poll or
		 * select with zero time.
		 */
		PTHREAD_WAITQ_SETACTIVE();
		while (((pthread = TAILQ_FIRST(&_waitingq)) != NULL) &&
		    (pthread->wakeup_time.tv_sec != -1) &&
		    (((pthread->wakeup_time.tv_sec == 0) &&
		    (pthread->wakeup_time.tv_nsec == 0)) ||
		    (pthread->wakeup_time.tv_sec < ts.tv_sec) ||
		    ((pthread->wakeup_time.tv_sec == ts.tv_sec) &&
		    (pthread->wakeup_time.tv_nsec <= ts.tv_nsec)))) {
			switch (pthread->state) {
			case PS_POLL_WAIT:
			case PS_SELECT_WAIT:
				/* Return zero file descriptors ready: */
				pthread->data.poll_data->nfds = 0;
				/* fall through */
			default:
				/*
				 * Remove this thread from the waiting queue
				 * (and work queue if necessary) and place it
				 * in the ready queue.
				 */
				PTHREAD_WAITQ_CLEARACTIVE();
				if (pthread->flags & PTHREAD_FLAGS_IN_WORKQ)
					PTHREAD_WORKQ_REMOVE(pthread);
				PTHREAD_NEW_STATE(pthread, PS_RUNNING);
				PTHREAD_WAITQ_SETACTIVE();
				break;
			}
			/*
			 * Flag the timeout in the thread structure:
			 */
			pthread->timeout = 1;
		}
		PTHREAD_WAITQ_CLEARACTIVE();

		/*
		 * Check if there is a current runnable thread that isn't
		 * already in the ready queue:
		 */
		if ((_thread_run != &_thread_kern_thread) &&
		    (_thread_run->state == PS_RUNNING) &&
		    ((_thread_run->flags & PTHREAD_FLAGS_IN_PRIOQ) == 0)) {
			if (_thread_run->slice_usec == -1) {
				/*
				 * The thread exceeded its time
				 * quantum or it yielded the CPU;
				 * place it at the tail of the
				 * queue for its priority.
				 */
				PTHREAD_PRIOQ_INSERT_TAIL(_thread_run);
			} else {
				/*
				 * The thread hasn't exceeded its
				 * interval.  Place it at the head
				 * of the queue for its priority.
				 */
				PTHREAD_PRIOQ_INSERT_HEAD(_thread_run);
			}
		}

		/*
		 * Get the highest priority thread in the ready queue.
		 */
		pthread_h = PTHREAD_PRIOQ_FIRST();

		/* Check if there are no threads ready to run: */
		if (pthread_h == NULL) {
			/*
			 * Lock the pthread kernel by changing the pointer to
			 * the running thread to point to the global kernel
			 * thread structure: 
			 */
			_thread_run = &_thread_kern_thread;

			/* Unprotect the scheduling queues: */
			_queue_signals = 0;

			/*
			 * There are no threads ready to run, so wait until
			 * something happens that changes this condition: 
			 */
			_thread_kern_poll(1);
		} else {
			/* Remove the thread from the ready queue: */
			PTHREAD_PRIOQ_REMOVE(pthread_h);

			/* Get first thread on the waiting list: */
			pthread = TAILQ_FIRST(&_waitingq);

			/* Check to see if there is more than one thread: */
			if (pthread_h != TAILQ_FIRST(&_thread_list) ||
			    TAILQ_NEXT(pthread_h, tle) != NULL)
				set_timer = 1;
			else
				set_timer = 0;

			/* Unprotect the scheduling queues: */
			_queue_signals = 0;

			/*
			 * Check for signals queued while the scheduling
			 * queues were protected:
			 */
			while (_sigq_check_reqd != 0) {
				/* Clear before handling queued signals: */
				_sigq_check_reqd = 0;

				/* Protect the scheduling queues again: */
				_queue_signals = 1;

				dequeue_signals();

				/*
				 * Check for a higher priority thread that
				 * became runnable due to signal handling.
				 */
				if (((pthread = PTHREAD_PRIOQ_FIRST()) != NULL) &&
				    (pthread->active_priority > pthread_h->active_priority)) {
					/*
					 * Insert the lower priority thread
					 * at the head of its priority list:
					 */
					PTHREAD_PRIOQ_INSERT_HEAD(pthread_h);

					/* Remove the thread from the ready queue: */
					PTHREAD_PRIOQ_REMOVE(pthread);

					/* There's a new thread in town: */
					pthread_h = pthread;
				}

				/* Get first thread on the waiting list: */
				pthread = TAILQ_FIRST(&_waitingq);

				/*
				 * Check to see if there is more than one
				 * thread:
				 */
				if (pthread_h != TAILQ_FIRST(&_thread_list) ||
				    TAILQ_NEXT(pthread_h, tle) != NULL)
					set_timer = 1;
				else
					set_timer = 0;

				/* Unprotect the scheduling queues: */
				_queue_signals = 0;
			}

			/* Make the selected thread the current thread: */
			_thread_run = pthread_h;

			/*
			 * Save the current time as the time that the thread
			 * became active: 
			 */
			_thread_run->last_active.tv_sec = tv.tv_sec;
			_thread_run->last_active.tv_usec = tv.tv_usec;

			/*
			 * Define the maximum time before a scheduling signal
			 * is required: 
			 */
			itimer.it_value.tv_sec = 0;
			itimer.it_value.tv_usec = TIMESLICE_USEC;

			/*
			 * The interval timer is not reloaded when it
			 * times out. The interval time needs to be
			 * calculated every time. 
			 */
			itimer.it_interval.tv_sec = 0;
			itimer.it_interval.tv_usec = 0;

			/* Get first thread on the waiting list: */
			if ((pthread != NULL) &&
			    (pthread->wakeup_time.tv_sec != -1)) {
				/*
				 * Calculate the time until this thread
				 * is ready, allowing for the clock
				 * resolution: 
				 */
				ts1.tv_sec = pthread->wakeup_time.tv_sec
				    - ts.tv_sec;
				ts1.tv_nsec = pthread->wakeup_time.tv_nsec
				    - ts.tv_nsec + _clock_res_nsec;

				/*
				 * Check for underflow of the nanosecond field:
				 */
				while (ts1.tv_nsec < 0) {
					/*
					 * Allow for the underflow of the
					 * nanosecond field: 
					 */
					ts1.tv_sec--;
					ts1.tv_nsec += 1000000000;
				}
				/*
				 * Check for overflow of the nanosecond field: 
				 */
				while (ts1.tv_nsec >= 1000000000) {
					/*
					 * Allow for the overflow of the
					 * nanosecond field: 
					 */
					ts1.tv_sec++;
					ts1.tv_nsec -= 1000000000;
				}
				/*
				 * Convert the timespec structure to a
				 * timeval structure: 
				 */
				TIMESPEC_TO_TIMEVAL(&tv1, &ts1);

				/*
				 * Check if the thread will be ready
				 * sooner than the earliest ones found
				 * so far: 
				 */
				if (timercmp(&tv1, &itimer.it_value, <)) {
					/*
					 * Update the time value: 
					 */
					itimer.it_value.tv_sec = tv1.tv_sec;
					itimer.it_value.tv_usec = tv1.tv_usec;
				}
			}

			/*
			 * Check if this thread is running for the first time
			 * or running again after using its full time slice
			 * allocation: 
			 */
			if (_thread_run->slice_usec == -1) {
				/* Reset the accumulated time slice period: */
				_thread_run->slice_usec = 0;
			}

			/* Check if there is more than one thread: */
			if (set_timer != 0) {
				/*
				 * Start the interval timer for the
				 * calculated time interval: 
				 */
				if (setitimer(_ITIMER_SCHED_TIMER, &itimer, NULL) != 0) {
					/*
					 * Cannot initialise the timer, so
					 * abort this process: 
					 */
					PANIC("Cannot set scheduling timer");
				}
			}

			/*
			 * Check if this thread is being continued from a
			 * longjmp() out of a signal handler:
			 */
			if ((_thread_run->jmpflags & JMPFLAGS_LONGJMP) != 0) {
				_thread_run->jmpflags = 0;
				__longjmp(_thread_run->nested_jmp.jmp,
				    _thread_run->longjmp_val);
			}
			/*
			 * Check if this thread is being continued from a
			 * _longjmp() out of a signal handler:
			 */
			else if ((_thread_run->jmpflags & JMPFLAGS__LONGJMP) !=
			    0) {
				_thread_run->jmpflags = 0;
				___longjmp(_thread_run->nested_jmp.jmp,
				    _thread_run->longjmp_val);
			}
			/*
			 * Check if this thread is being continued from a
			 * siglongjmp() out of a signal handler:
			 */
			else if ((_thread_run->jmpflags & JMPFLAGS_SIGLONGJMP)
			    != 0) {
				_thread_run->jmpflags = 0;
				__siglongjmp(
				    _thread_run->nested_jmp.sigjmp,
				    _thread_run->longjmp_val);
			}
			/* Check if a signal context was saved: */
			else if (_thread_run->sig_saved == 1) {
#ifndef	__alpha__
				/*
				 * Point to the floating point data in the
				 * running thread: 
				 */
				fdata = _thread_run->saved_fp;

				/* Restore the floating point state: */
		__asm__("frstor %0": :"m"(*fdata));
#endif
				/*
				 * Do a sigreturn to restart the thread that
				 * was interrupted by a signal: 
				 */
				_thread_kern_in_sched = 0;

				/*
				 * If we had a context switch, run any
				 * installed switch hooks.
				 */
				if ((_sched_switch_hook != NULL) &&
				    (_last_user_thread != _thread_run)) {
					thread_run_switch_hook(_last_user_thread,
					    _thread_run);
				}
				_thread_sys_sigreturn(&_thread_run->saved_sigcontext);
			} else {
				/*
				 * Do a longjmp to restart the thread that
				 * was context switched out (by a longjmp to
				 * a different thread): 
				 */
				__longjmp(_thread_run->saved_jmp_buf, 1);
			}

			/* This point should not be reached. */
			PANIC("Thread has returned from sigreturn or longjmp");
		}
	}

	/* There are no more threads, so exit this process: */
	exit(0);
}

void
_thread_kern_sched_state(enum pthread_state state, char *fname, int lineno)
{
	/*
	 * Flag the pthread kernel as executing scheduler code
	 * to avoid a scheduler signal from interrupting this
	 * execution and calling the scheduler again.
	 */
	_thread_kern_in_sched = 1;

	/*
	 * Prevent the signal handler from fiddling with this thread
	 * before its state is set and is placed into the proper queue.
	 */
	_queue_signals = 1;

	/* Change the state of the current thread: */
	_thread_run->state = state;
	_thread_run->fname = fname;
	_thread_run->lineno = lineno;

	/* Schedule the next thread that is ready: */
	_thread_kern_sched(NULL);
	return;
}

void
_thread_kern_sched_state_unlock(enum pthread_state state,
    spinlock_t *lock, char *fname, int lineno)
{
	/*
	 * Flag the pthread kernel as executing scheduler code
	 * to avoid a scheduler signal from interrupting this
	 * execution and calling the scheduler again.
	 */
	_thread_kern_in_sched = 1;

	/*
	 * Prevent the signal handler from fiddling with this thread
	 * before its state is set and it is placed into the proper
	 * queue(s).
	 */
	_queue_signals = 1;

	/* Change the state of the current thread: */
	_thread_run->state = state;
	_thread_run->fname = fname;
	_thread_run->lineno = lineno;

	_SPINUNLOCK(lock);

	/* Schedule the next thread that is ready: */
	_thread_kern_sched(NULL);
	return;
}

static void
_thread_kern_poll(int wait_reqd)
{
	int             count = 0;
	int             i, found;
	int		kern_pipe_added = 0;
	int             nfds = 0;
	int		timeout_ms = 0;
	struct pthread	*pthread;
	struct timespec ts;
	struct timeval  tv;

	/* Check if the caller wants to wait: */
	if (wait_reqd == 0) {
		timeout_ms = 0;
	}
	else {
		/* Get the current time of day: */
		gettimeofday(&tv, NULL);
		TIMEVAL_TO_TIMESPEC(&tv, &ts);

		_queue_signals = 1;
		pthread = TAILQ_FIRST(&_waitingq);
		_queue_signals = 0;

		if ((pthread == NULL) || (pthread->wakeup_time.tv_sec == -1)) {
			/*
			 * Either there are no threads in the waiting queue,
			 * or there are no threads that can timeout.
			 */
			timeout_ms = INFTIM;
		}
		else {
			/*
			 * Calculate the time left for the next thread to
			 * timeout allowing for the clock resolution:
			 */
			timeout_ms = ((pthread->wakeup_time.tv_sec - ts.tv_sec) *
			    1000) + ((pthread->wakeup_time.tv_nsec - ts.tv_nsec +
			    _clock_res_nsec) / 1000000);
			/*
			 * Don't allow negative timeouts:
			 */
			if (timeout_ms < 0)
				timeout_ms = 0;
		}
	}
			
	/* Protect the scheduling queues: */
	_queue_signals = 1;

	/*
	 * Check to see if the signal queue needs to be walked to look
	 * for threads awoken by a signal while in the scheduler.
	 */
	if (_sigq_check_reqd != 0) {
		/* Reset flag before handling queued signals: */
		_sigq_check_reqd = 0;

		dequeue_signals();
	}

	/*
	 * Check for a thread that became runnable due to a signal:
	 */
	if (PTHREAD_PRIOQ_FIRST() != NULL) {
		/*
		 * Since there is at least one runnable thread,
		 * disable the wait.
		 */
		timeout_ms = 0;
	}

	/*
	 * Form the poll table:
	 */
	nfds = 0;
	if (timeout_ms != 0) {
		/* Add the kernel pipe to the poll table: */
		_thread_pfd_table[nfds].fd = _thread_kern_pipe[0];
		_thread_pfd_table[nfds].events = POLLRDNORM;
		_thread_pfd_table[nfds].revents = 0;
		nfds++;
		kern_pipe_added = 1;
	}

	PTHREAD_WAITQ_SETACTIVE();
	TAILQ_FOREACH(pthread, &_workq, qe) {
		switch (pthread->state) {
		case PS_SPINBLOCK:
			/*
			 * If the lock is available, let the thread run.
			 */
			if (pthread->data.spinlock->access_lock == 0) {
				PTHREAD_WAITQ_CLEARACTIVE();
				PTHREAD_WORKQ_REMOVE(pthread);
				PTHREAD_NEW_STATE(pthread,PS_RUNNING);
				PTHREAD_WAITQ_SETACTIVE();
				/* One less thread in a spinblock state: */
				_spinblock_count--;
				/*
				 * Since there is at least one runnable
				 * thread, disable the wait.
				 */
				timeout_ms = 0;
			}
			break;

		/* File descriptor read wait: */
		case PS_FDR_WAIT:
			/* Limit number of polled files to table size: */
			if (nfds < _thread_dtablesize) {
				_thread_pfd_table[nfds].events = POLLRDNORM;
				_thread_pfd_table[nfds].fd = pthread->data.fd.fd;
				nfds++;
			}
			break;

		/* File descriptor write wait: */
		case PS_FDW_WAIT:
			/* Limit number of polled files to table size: */
			if (nfds < _thread_dtablesize) {
				_thread_pfd_table[nfds].events = POLLWRNORM;
				_thread_pfd_table[nfds].fd = pthread->data.fd.fd;
				nfds++;
			}
			break;

		/* File descriptor poll or select wait: */
		case PS_POLL_WAIT:
		case PS_SELECT_WAIT:
			/* Limit number of polled files to table size: */
			if (pthread->data.poll_data->nfds + nfds <
			    _thread_dtablesize) {
				for (i = 0; i < pthread->data.poll_data->nfds; i++) {
					_thread_pfd_table[nfds + i].fd =
					    pthread->data.poll_data->fds[i].fd;
					_thread_pfd_table[nfds + i].events =
					    pthread->data.poll_data->fds[i].events;
				}
				nfds += pthread->data.poll_data->nfds;
			}
			break;

		/* Other states do not depend on file I/O. */
		default:
			break;
		}
	}
	PTHREAD_WAITQ_CLEARACTIVE();

	/*
	 * Wait for a file descriptor to be ready for read, write, or
	 * an exception, or a timeout to occur: 
	 */
	count = _thread_sys_poll(_thread_pfd_table, nfds, timeout_ms);

	if (kern_pipe_added != 0)
		/*
		 * Remove the pthread kernel pipe file descriptor
		 * from the pollfd table: 
		 */
		nfds = 1;
	else
		nfds = 0;

	/*
	 * Check if it is possible that there are bytes in the kernel
	 * read pipe waiting to be read:
	 */
	if (count < 0 || ((kern_pipe_added != 0) &&
	    (_thread_pfd_table[0].revents & POLLRDNORM))) {
		/*
		 * If the kernel read pipe was included in the
		 * count: 
		 */
		if (count > 0) {
			/* Decrement the count of file descriptors: */
			count--;
		}

		if (_sigq_check_reqd != 0) {
			/* Reset flag before handling signals: */
			_sigq_check_reqd = 0;

			dequeue_signals();
		}
	}

	/*
	 * Check if any file descriptors are ready:
	 */
	if (count > 0) {
		/*
		 * Enter a loop to look for threads waiting on file
		 * descriptors that are flagged as available by the
		 * _poll syscall: 
		 */
		PTHREAD_WAITQ_SETACTIVE();
		TAILQ_FOREACH(pthread, &_workq, qe) {
			switch (pthread->state) {
			case PS_SPINBLOCK:
				/*
				 * If the lock is available, let the thread run.
				 */
				if (pthread->data.spinlock->access_lock == 0) {
					PTHREAD_WAITQ_CLEARACTIVE();
					PTHREAD_WORKQ_REMOVE(pthread);
					PTHREAD_NEW_STATE(pthread,PS_RUNNING);
					PTHREAD_WAITQ_SETACTIVE();

					/*
					 * One less thread in a spinblock state:
					 */
					_spinblock_count--;
				}
				break;

			/* File descriptor read wait: */
			case PS_FDR_WAIT:
				if ((nfds < _thread_dtablesize) &&
				    (_thread_pfd_table[nfds].revents & POLLRDNORM)) {
					PTHREAD_WAITQ_CLEARACTIVE();
					PTHREAD_WORKQ_REMOVE(pthread);
					PTHREAD_NEW_STATE(pthread,PS_RUNNING);
					PTHREAD_WAITQ_SETACTIVE();
				}
				nfds++;
				break;

			/* File descriptor write wait: */
			case PS_FDW_WAIT:
				if ((nfds < _thread_dtablesize) &&
				    (_thread_pfd_table[nfds].revents & POLLWRNORM)) {
					PTHREAD_WAITQ_CLEARACTIVE();
					PTHREAD_WORKQ_REMOVE(pthread);
					PTHREAD_NEW_STATE(pthread,PS_RUNNING);
					PTHREAD_WAITQ_SETACTIVE();
				}
				nfds++;
				break;

			/* File descriptor poll or select wait: */
			case PS_POLL_WAIT:
			case PS_SELECT_WAIT:
				if (pthread->data.poll_data->nfds + nfds <
				    _thread_dtablesize) {
					/*
					 * Enter a loop looking for I/O
					 * readiness:
					 */
					found = 0;
					for (i = 0; i < pthread->data.poll_data->nfds; i++) {
						if (_thread_pfd_table[nfds + i].revents != 0) {
							pthread->data.poll_data->fds[i].revents =
							    _thread_pfd_table[nfds + i].revents;
							found++;
						}
					}

					/* Increment before destroying: */
					nfds += pthread->data.poll_data->nfds;

					if (found != 0) {
						pthread->data.poll_data->nfds = found;
						PTHREAD_WAITQ_CLEARACTIVE();
						PTHREAD_WORKQ_REMOVE(pthread);
						PTHREAD_NEW_STATE(pthread,PS_RUNNING);
						PTHREAD_WAITQ_SETACTIVE();
					}
				}
				else
					nfds += pthread->data.poll_data->nfds;
				break;

			/* Other states do not depend on file I/O. */
			default:
				break;
			}
		}
		PTHREAD_WAITQ_CLEARACTIVE();
	}
	else if (_spinblock_count != 0) {
		/*
		 * Enter a loop to look for threads waiting on a spinlock
		 * that is now available.
		 */
		PTHREAD_WAITQ_SETACTIVE();
		TAILQ_FOREACH(pthread, &_workq, qe) {
			if (pthread->state == PS_SPINBLOCK) {
				/*
				 * If the lock is available, let the thread run.
				 */
				if (pthread->data.spinlock->access_lock == 0) {
					PTHREAD_WAITQ_CLEARACTIVE();
					PTHREAD_WORKQ_REMOVE(pthread);
					PTHREAD_NEW_STATE(pthread,PS_RUNNING);
					PTHREAD_WAITQ_SETACTIVE();

					/*
					 * One less thread in a spinblock state:
					 */
					_spinblock_count--;
				}
			}
		}
		PTHREAD_WAITQ_CLEARACTIVE();
	}

	/* Unprotect the scheduling queues: */
	_queue_signals = 0;

	while (_sigq_check_reqd != 0) {
		/* Handle queued signals: */
		_sigq_check_reqd = 0;

		/* Protect the scheduling queues: */
		_queue_signals = 1;

		dequeue_signals();

		/* Unprotect the scheduling queues: */
		_queue_signals = 0;
	}

	/* Nothing to return. */
	return;
}

void
_thread_kern_set_timeout(const struct timespec * timeout)
{
	struct timespec current_time;
	struct timeval  tv;

	/* Reset the timeout flag for the running thread: */
	_thread_run->timeout = 0;

	/* Check if the thread is to wait forever: */
	if (timeout == NULL) {
		/*
		 * Set the wakeup time to something that can be recognised as
		 * different to an actual time of day: 
		 */
		_thread_run->wakeup_time.tv_sec = -1;
		_thread_run->wakeup_time.tv_nsec = -1;
	}
	/* Check if no waiting is required: */
	else if (timeout->tv_sec == 0 && timeout->tv_nsec == 0) {
		/* Set the wake up time to 'immediately': */
		_thread_run->wakeup_time.tv_sec = 0;
		_thread_run->wakeup_time.tv_nsec = 0;
	} else {
		/* Get the current time: */
		gettimeofday(&tv, NULL);
		TIMEVAL_TO_TIMESPEC(&tv, &current_time);

		/* Calculate the time for the current thread to wake up: */
		_thread_run->wakeup_time.tv_sec = current_time.tv_sec + timeout->tv_sec;
		_thread_run->wakeup_time.tv_nsec = current_time.tv_nsec + timeout->tv_nsec;

		/* Check if the nanosecond field needs to wrap: */
		if (_thread_run->wakeup_time.tv_nsec >= 1000000000) {
			/* Wrap the nanosecond field: */
			_thread_run->wakeup_time.tv_sec += 1;
			_thread_run->wakeup_time.tv_nsec -= 1000000000;
		}
	}
	return;
}

void
_thread_kern_sig_defer(void)
{
	/* Allow signal deferral to be recursive. */
	_thread_run->sig_defer_count++;
}

void
_thread_kern_sig_undefer(void)
{
	pthread_t pthread;
	int need_resched = 0;

	/*
	 * Perform checks to yield only if we are about to undefer
	 * signals.
	 */
	if (_thread_run->sig_defer_count > 1) {
		/* Decrement the signal deferral count. */
		_thread_run->sig_defer_count--;
	}
	else if (_thread_run->sig_defer_count == 1) {
		/* Reenable signals: */
		_thread_run->sig_defer_count = 0;

		/*
		 * Check if there are queued signals:
		 */
		while (_sigq_check_reqd != 0) {
			/* Defer scheduling while we process queued signals: */
			_thread_run->sig_defer_count = 1;

			/* Clear the flag before checking the signal queue: */
			_sigq_check_reqd = 0;

			/* Dequeue and handle signals: */
			dequeue_signals();

			/*
			 * Avoiding an unnecessary check to reschedule, check
			 * to see if signal handling caused a higher priority
			 * thread to become ready.
			 */
			if ((need_resched == 0) &&
			    (((pthread = PTHREAD_PRIOQ_FIRST()) != NULL) &&
			    (pthread->active_priority > _thread_run->active_priority))) {
				need_resched = 1;
			}

			/* Reenable signals: */
			_thread_run->sig_defer_count = 0;
		}

		/* Yield the CPU if necessary: */
		if (need_resched || _thread_run->yield_on_sig_undefer != 0) {
			_thread_run->yield_on_sig_undefer = 0;
			_thread_kern_sched(NULL);
		}
	}
}

static void
dequeue_signals(void)
{
	char	bufr[128];
	int	i, num;
	pthread_t pthread;

	/*
	 * Enter a loop to read and handle queued signals from the
	 * pthread kernel pipe: 
	 */
	while (((num = _thread_sys_read(_thread_kern_pipe[0], bufr,
	    sizeof(bufr))) > 0) || (num == -1 && errno == EINTR)) {
		/*
		 * The buffer read contains one byte per signal and
		 * each byte is the signal number.
		 */
		for (i = 0; i < num; i++) {
			if ((int) bufr[i] == _SCHED_SIGNAL) {
				/*
				 * Scheduling signals shouldn't ever be
				 * queued; just ignore it for now.
				 */
			}
			else {
				/* Handle this signal: */
				pthread = _thread_sig_handle((int) bufr[i],
				    NULL);
				if (pthread != NULL)
					_thread_sig_deliver(pthread,
					    (int) bufr[i]);
			}
		}
	}
	if ((num < 0) && (errno != EAGAIN)) {
		/*
		 * The only error we should expect is if there is
		 * no data to read.
		 */
		PANIC("Unable to read from thread kernel pipe");
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
#endif
