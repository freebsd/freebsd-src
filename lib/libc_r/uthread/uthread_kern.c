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
 * $Id: uthread_kern.c,v 1.3.2.3 1998/04/17 11:22:26 tg Exp $
 *
 */
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <fcntl.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Static variables: */
static sigset_t sig_to_block = 0xffffffff;
static sigset_t sig_to_unblock = 0;

/* Static function prototype definitions: */
static void 
_thread_kern_select(int wait_reqd);
static void 
_thread_signal(pthread_t pthread, int sig);

void
_thread_kern_sched(struct sigcontext * scp)
{
#ifndef	__alpha
	char           *fdata;
#endif
	int             i;
	int             prio = -1;
	pthread_t       pthread;
	pthread_t       pthread_h = NULL;
	pthread_t       pthread_nxt = NULL;
	pthread_t       pthread_prv = NULL;
	pthread_t       pthread_s = NULL;
	struct itimerval itimer;
	struct timespec ts;
	struct timespec ts1;
	struct timeval  tv;
	struct timeval  tv1;

	/* Block signals: */
	_thread_kern_sig_block(NULL);

	/* Check if this function was called from the signal handler: */
	if (scp != NULL) {
		/*
		 * Copy the signal context to the current thread's jump
		 * buffer: 
		 */
		memcpy(&_thread_run->saved_sigcontext, scp, sizeof(_thread_run->saved_sigcontext));

#ifndef	__alpha
		/* Point to the floating point data in the running thread: */
		fdata = _thread_run->saved_fp;

		/* Save the floating point data: */
__asm__("fnsave %0": :"m"(*fdata));
#endif

		/* Flag the signal context as the last state saved: */
		_thread_run->sig_saved = 1;
	}
	/* Save the state of the current thread: */
	else if (_thread_sys_setjmp(_thread_run->saved_jmp_buf) != 0) {
		/* Unblock signals (just in case): */
		_thread_kern_sig_unblock(0);

		/*
		 * This point is reached when a longjmp() is called to
		 * restore the state of a thread. 
		 */
		return;
	} else {
		/* Flag the jump buffer was the last state saved: */
		_thread_run->sig_saved = 0;
	}

	/* Point to the first dead thread (if there are any): */
	pthread = _thread_dead;

	/* There is no previous dead thread: */
	pthread_prv = NULL;

	/* Enter a loop to cleanup after dead threads: */
	while (pthread != NULL) {
		/* Save a pointer to the next thread: */
		pthread_nxt = pthread->nxt;

		/* Check if this thread is one which is running: */
		if (pthread == _thread_run || pthread == _thread_initial) {
			/*
			 * Don't destroy the running thread or the initial
			 * thread. 
			 */
			pthread_prv = pthread;
		}
		/*
		 * Check if this thread has detached or if it is a signal
		 * handler thread: 
		 */
		else if (((pthread->attr.flags & PTHREAD_DETACHED) != 0) || pthread->parent_thread != NULL) {
			/* Check if there is no previous dead thread: */
			if (pthread_prv == NULL) {
				/*
				 * The dead thread is at the head of the
				 * list: 
				 */
				_thread_dead = pthread_nxt;
			} else {
				/*
				 * The dead thread is not at the head of the
				 * list: 
				 */
				pthread_prv->nxt = pthread->nxt;
			}

			/*
			 * Check if the stack was not specified by the caller
			 * to pthread_create and has not been destroyed yet: 
			 */
			if (pthread->attr.stackaddr_attr == NULL && pthread->stack != NULL) {
				/* Free the stack of the dead thread: */
				free(pthread->stack);
			}
			/* Free the memory allocated to the thread structure: */
			free(pthread);
		} else {
			/*
			 * This thread has not detached, so do not destroy
			 * it: 
			 */
			pthread_prv = pthread;

			/*
			 * Check if the stack was not specified by the caller
			 * to pthread_create and has not been destroyed yet: 
			 */
			if (pthread->attr.stackaddr_attr == NULL && pthread->stack != NULL) {
				/* Free the stack of the dead thread: */
				free(pthread->stack);

				/*
				 * NULL the stack pointer now that the memory
				 * has been freed: 
				 */
				pthread->stack = NULL;
			}
		}

		/* Point to the next thread: */
		pthread = pthread_nxt;
	}

	/*
	 * Enter a the scheduling loop that finds the next thread that is
	 * ready to run. This loop completes when there are no more threads
	 * in the global list or when a thread has its state restored by
	 * either a sigreturn (if the state was saved as a sigcontext) or a
	 * longjmp (if the state was saved by a setjmp). 
	 */
	while (_thread_link_list != NULL) {
		/* Get the current time of day: */
		gettimeofday(&tv, NULL);
		TIMEVAL_TO_TIMESPEC(&tv, &ts);

		/*
		 * Poll file descriptors to update the state of threads
		 * waiting on file I/O where data may be available: 
		 */
		_thread_kern_select(0);

		/*
		 * Enter a loop to look for sleeping threads that are ready
		 * or threads with pending signals that are no longer
		 * blocked: 
		 */
		for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
			/* Enter a loop to process the sending signals: */
			for (i = 1; i < NSIG; i++) {
				/*
				 * Check if there are no pending signals of
				 * this type: 
				 */
				if (pthread->sigpend[i] == 0) {
				}
				/* Check if this signal type is not masked: */
				else if (sigismember(&pthread->sigmask, i) == 0) {
					/*
					 * Delete the signal from the set of
					 * pending signals for this thread: 
					 */
					pthread->sigpend[i] -= 1;

					/*
					 * Act on the signal for the current
					 * thread: 
					 */
					_thread_signal(pthread, i);
				} else {
					/*
					 * This signal is masked, so make
					 * sure the count does not exceed 1: 
					 */
					pthread->sigpend[i] = 1;
				}
			}

			/* Check if this thread is to timeout: */
			if (pthread->state == PS_COND_WAIT ||
			    pthread->state == PS_SLEEP_WAIT ||
			    pthread->state == PS_FDR_WAIT ||
			    pthread->state == PS_FDW_WAIT ||
			    pthread->state == PS_SELECT_WAIT) {
				/* Check if this thread is to wait forever: */
				if (pthread->wakeup_time.tv_sec == -1) {
				}
				/*
				 * Check if this thread is to wakeup
				 * immediately or if it is past its wakeup
				 * time: 
				 */
				else if ((pthread->wakeup_time.tv_sec == 0 &&
					pthread->wakeup_time.tv_nsec == 0) ||
					 (ts.tv_sec > pthread->wakeup_time.tv_sec) ||
					 ((ts.tv_sec == pthread->wakeup_time.tv_sec) &&
					  (ts.tv_nsec >= pthread->wakeup_time.tv_nsec))) {
					/*
					 * Check if this thread is waiting on
					 * select: 
					 */
					if (pthread->state == PS_SELECT_WAIT) {
						/*
						 * The select has timed out,
						 * so zero the file
						 * descriptor sets: 
						 */
						FD_ZERO(&pthread->data.select_data->readfds);
						FD_ZERO(&pthread->data.select_data->writefds);
						FD_ZERO(&pthread->data.select_data->exceptfds);
						pthread->data.select_data->nfds = 0;
					}
					/*
					 * Return an error as an interrupted
					 * wait: 
					 */
					_thread_seterrno(pthread, EINTR);

					/*
					 * Flag the timeout in the thread
					 * structure: 
					 */
					pthread->timeout = 1;

					/*
					 * Change the threads state to allow
					 * it to be restarted: 
					 */
					PTHREAD_NEW_STATE(pthread,PS_RUNNING);
				}
			}
		}

		/* Check if there is a current thread: */
		if (_thread_run != &_thread_kern_thread) {
			/*
			 * Save the current time as the time that the thread
			 * became inactive: 
			 */
			_thread_run->last_inactive.tv_sec = tv.tv_sec;
			_thread_run->last_inactive.tv_usec = tv.tv_usec;

			/*
			 * Accumulate the number of microseconds that this
			 * thread has run for: 
			 */
			_thread_run->slice_usec += (_thread_run->last_inactive.tv_sec -
				_thread_run->last_active.tv_sec) * 1000000 +
				_thread_run->last_inactive.tv_usec -
				_thread_run->last_active.tv_usec;

			/*
			 * Check if this thread has reached its allocated
			 * time slice period: 
			 */
			if (_thread_run->slice_usec > TIMESLICE_USEC) {
				/*
				 * Flag the allocated time slice period as
				 * up: 
				 */
				_thread_run->slice_usec = -1;
			}
		}
		/* Check if an incremental priority update is required: */
		if (((tv.tv_sec - kern_inc_prio_time.tv_sec) * 1000000 +
		 tv.tv_usec - kern_inc_prio_time.tv_usec) > INC_PRIO_USEC) {
			/*
			 * Enter a loop to look for run-enabled threads that
			 * have not run since the last time that an
			 * incremental priority update was performed: 
			 */
			for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
				/* Check if this thread is unable to run: */
				if (pthread->state != PS_RUNNING) {
				}
				/*
				 * Check if the last time that this thread
				 * was run (as indicated by the last time it
				 * became inactive) is before the time that
				 * the last incremental priority check was
				 * made: 
				 */
				else if (timercmp(&_thread_run->last_inactive, &kern_inc_prio_time, <)) {
					/*
					 * Increment the incremental priority
					 * for this thread in the hope that
					 * it will eventually get a chance to
					 * run: 
					 */
					(pthread->inc_prio)++;
				}
			}

			/* Save the new incremental priority update time: */
			kern_inc_prio_time.tv_sec = tv.tv_sec;
			kern_inc_prio_time.tv_usec = tv.tv_usec;
		}
		/*
		 * Enter a loop to look for the first thread of the highest
		 * priority that is ready to run: 
		 */
		for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
			/* Check if in single-threaded mode: */
			if (_thread_single != NULL) {
				/*
				 * Check if the current thread is
				 * the thread for which single-threaded
				 * mode is enabled:
				 */
				if (pthread == _thread_single) {
					/*
					 * This thread is allowed
					 * to run.
					 */
				} else {
					/*
					 * Walk up the signal handler
                                         * parent thread tree to see
					 * if the current thread is
					 * descended from the thread
					 * for which single-threaded
					 * mode is enabled.
					 */
					pthread_nxt = pthread;
					while(pthread_nxt != NULL &&
						pthread_nxt != _thread_single) {
						pthread_nxt = pthread->parent_thread;
					}
					/*
					 * Check if the current
					 * thread is not descended
					 * from the thread for which
					 * single-threaded mode is
					 * enabled.
					 */
					if (pthread_nxt == NULL)
						/* Ignore this thread. */
						continue;
				}
			}

			/* Check if the current thread is unable to run: */
			if (pthread->state != PS_RUNNING) {
			}
			/*
			 * Check if no run-enabled thread has been seen or if
			 * the current thread has a priority higher than the
			 * highest seen so far: 
			 */
			else if (pthread_h == NULL || (pthread->pthread_priority + pthread->inc_prio) > prio) {
				/*
				 * Save this thread as the highest priority
				 * thread seen so far: 
				 */
				pthread_h = pthread;
				prio = pthread->pthread_priority + pthread->inc_prio;
			}
		}

		/*
		 * Enter a loop to look for a thread that: 1. Is run-enabled.
		 * 2. Has the required agregate priority. 3. Has not been
		 * allocated its allocated time slice. 4. Became inactive
		 * least recently. 
		 */
		for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
			/* Check if in single-threaded mode: */
			if (_thread_single != NULL) {
				/*
				 * Check if the current thread is
				 * the thread for which single-threaded
				 * mode is enabled:
				 */
				if (pthread == _thread_single) {
					/*
					 * This thread is allowed
					 * to run.
					 */
				} else {
					/*
					 * Walk up the signal handler
                                         * parent thread tree to see
					 * if the current thread is
					 * descended from the thread
					 * for which single-threaded
					 * mode is enabled.
					 */
					pthread_nxt = pthread;
					while(pthread_nxt != NULL &&
						pthread_nxt != _thread_single) {
						pthread_nxt = pthread->parent_thread;
					}
					/*
					 * Check if the current
					 * thread is not descended
					 * from the thread for which
					 * single-threaded mode is
					 * enabled.
					 */
					if (pthread_nxt == NULL)
						/* Ignore this thread. */
						continue;
				}
			}

			/* Check if the current thread is unable to run: */
			if (pthread->state != PS_RUNNING) {
				/* Ignore threads that are not ready to run. */
			}
			/*
			 * Check if the current thread as an agregate
			 * priority not equal to the highest priority found
			 * above: 
			 */
			else if ((pthread->pthread_priority + pthread->inc_prio) != prio) {
				/*
				 * Ignore threads which have lower agregate
				 * priority. 
				 */
			}
			/*
			 * Check if the current thread reached its time slice
			 * allocation last time it ran (or if it has not run
			 * yet): 
			 */
			else if (pthread->slice_usec == -1) {
			}
			/*
			 * Check if an eligible thread has not been found
			 * yet, or if the current thread has an inactive time
			 * earlier than the last one seen: 
			 */
			else if (pthread_s == NULL || timercmp(&pthread->last_inactive, &tv1, <)) {
				/*
				 * Save the pointer to the current thread as
				 * the most eligible thread seen so far: 
				 */
				pthread_s = pthread;

				/*
				 * Save the time that the selected thread
				 * became inactive: 
				 */
				tv1.tv_sec = pthread->last_inactive.tv_sec;
				tv1.tv_usec = pthread->last_inactive.tv_usec;
			}
		}

		/*
		 * Check if no thread was selected according to incomplete
		 * time slice allocation: 
		 */
		if (pthread_s == NULL) {
			/*
			 * Enter a loop to look for any other thread that: 1.
			 * Is run-enabled. 2. Has the required agregate
			 * priority. 3. Became inactive least recently. 
			 */
			for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
				/* Check if in single-threaded mode: */
				if (_thread_single != NULL) {
					/*
					 * Check if the current thread is
					 * the thread for which single-threaded
					 * mode is enabled:
					 */
					if (pthread == _thread_single) {
						/*
						 * This thread is allowed
						 * to run.
						 */
					} else {
						/*
						 * Walk up the signal handler
						 * parent thread tree to see
						 * if the current thread is
						 * descended from the thread
						 * for which single-threaded
						 * mode is enabled.
						 */
						pthread_nxt = pthread;
						while(pthread_nxt != NULL &&
							pthread_nxt != _thread_single) {
							pthread_nxt = pthread->parent_thread;
						}
						/*
						 * Check if the current
						 * thread is not descended
						 * from the thread for which
						 * single-threaded mode is
						 * enabled.
						 */
						if (pthread_nxt == NULL)
							/* Ignore this thread. */
							continue;
					}
				}

				/*
				 * Check if the current thread is unable to
				 * run: 
				 */
				if (pthread->state != PS_RUNNING) {
					/*
					 * Ignore threads that are not ready
					 * to run. 
					 */
				}
				/*
				 * Check if the current thread as an agregate
				 * priority not equal to the highest priority
				 * found above: 
				 */
				else if ((pthread->pthread_priority + pthread->inc_prio) != prio) {
					/*
					 * Ignore threads which have lower
					 * agregate priority.   
					 */
				}
				/*
				 * Check if an eligible thread has not been
				 * found yet, or if the current thread has an
				 * inactive time earlier than the last one
				 * seen: 
				 */
				else if (pthread_s == NULL || timercmp(&pthread->last_inactive, &tv1, <)) {
					/*
					 * Save the pointer to the current
					 * thread as the most eligible thread
					 * seen so far: 
					 */
					pthread_s = pthread;

					/*
					 * Save the time that the selected
					 * thread became inactive: 
					 */
					tv1.tv_sec = pthread->last_inactive.tv_sec;
					tv1.tv_usec = pthread->last_inactive.tv_usec;
				}
			}
		}
		/* Check if there are no threads ready to run: */
		if (pthread_s == NULL) {
			/*
			 * Lock the pthread kernel by changing the pointer to
			 * the running thread to point to the global kernel
			 * thread structure: 
			 */
			_thread_run = &_thread_kern_thread;

			/*
			 * There are no threads ready to run, so wait until
			 * something happens that changes this condition: 
			 */
			_thread_kern_select(1);
		} else {
			/* Make the selected thread the current thread: */
			_thread_run = pthread_s;

			/*
			 * Save the current time as the time that the thread
			 * became active: 
			 */
			_thread_run->last_active.tv_sec = tv.tv_sec;
			_thread_run->last_active.tv_usec = tv.tv_usec;

			/*
			 * Check if this thread is running for the first time
			 * or running again after using its full time slice
			 * allocation: 
			 */
			if (_thread_run->slice_usec == -1) {
				/* Reset the accumulated time slice period: */
				_thread_run->slice_usec = 0;
			}
			/*
			 * Reset the incremental priority now that this
			 * thread has been given the chance to run: 
			 */
			_thread_run->inc_prio = 0;

			/* Check if there is more than one thread: */
			if (_thread_run != _thread_link_list || _thread_run->nxt != NULL) {
				/*
				 * Define the maximum time before a SIGVTALRM
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

				/*
				 * Enter a loop to look for threads waiting
				 * for a time: 
				 */
				for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
					/*
					 * Check if this thread is to
					 * timeout: 
					 */
					if (pthread->state == PS_COND_WAIT ||
					  pthread->state == PS_SLEEP_WAIT ||
					    pthread->state == PS_FDR_WAIT ||
					    pthread->state == PS_FDW_WAIT ||
					 pthread->state == PS_SELECT_WAIT) {
						/*
						 * Check if this thread is to
						 * wait forever: 
						 */
						if (pthread->wakeup_time.tv_sec == -1) {
						}
						/*
						 * Check if this thread is to
						 * wakeup immediately: 
						 */
						else if (pthread->wakeup_time.tv_sec == 0 &&
							 pthread->wakeup_time.tv_nsec == 0) {
						}
						/*
						 * Check if the current time
						 * is after the wakeup time: 
						 */
						else if ((ts.tv_sec > pthread->wakeup_time.tv_sec) ||
							 ((ts.tv_sec == pthread->wakeup_time.tv_sec) &&
							  (ts.tv_nsec > pthread->wakeup_time.tv_nsec))) {
						} else {
							/*
							 * Calculate the time
							 * until this thread
							 * is ready, allowing
							 * for the clock
							 * resolution: 
							 */
							ts1.tv_sec = pthread->wakeup_time.tv_sec - ts.tv_sec;
							ts1.tv_nsec = pthread->wakeup_time.tv_nsec - ts.tv_nsec +
								CLOCK_RES_NSEC;

							/*
							 * Check for
							 * underflow of the
							 * nanosecond field: 
							 */
							if (ts1.tv_nsec < 0) {
								/*
								 * Allow for
								 * the
								 * underflow
								 * of the
								 * nanosecond
								 * field: 
								 */
								ts1.tv_sec--;
								ts1.tv_nsec += 1000000000;
							}
							/*
							 * Check for overflow
							 * of the nanosecond
							 * field: 
							 */
							if (ts1.tv_nsec >= 1000000000) {
								/*
								 * Allow for
								 * the
								 * overflow
								 * of the
								 * nanosecond
								 * field: 
								 */
								ts1.tv_sec++;
								ts1.tv_nsec -= 1000000000;
							}
							/*
							 * Convert the
							 * timespec structure
							 * to a timeval
							 * structure: 
							 */
							TIMESPEC_TO_TIMEVAL(&tv, &ts1);

							/*
							 * Check if the
							 * thread will be
							 * ready sooner than
							 * the earliest one
							 * found so far: 
							 */
							if (timercmp(&tv, &itimer.it_value, <)) {
								/*
								 * Update the
								 * time
								 * value: 
								 */
								itimer.it_value.tv_sec = tv.tv_sec;
								itimer.it_value.tv_usec = tv.tv_usec;
							}
						}
					}
				}

				/*
				 * Start the interval timer for the
				 * calculated time interval: 
				 */
				if (setitimer(ITIMER_VIRTUAL, &itimer, NULL) != 0) {
					/*
					 * Cannot initialise the timer, so
					 * abort this process: 
					 */
					PANIC("Cannot set virtual timer");
				}
			}
			/* Check if a signal context was saved: */
			if (_thread_run->sig_saved == 1) {
#ifndef	__alpha
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
				_thread_sys_sigreturn(&_thread_run->saved_sigcontext);
			} else {
				/*
				 * Do a longjmp to restart the thread that
				 * was context switched out (by a longjmp to
				 * a different thread): 
				 */
				_thread_sys_longjmp(_thread_run->saved_jmp_buf, 1);
			}

			/* This point should not be reached. */
			PANIC("Thread has returned from sigreturn or longjmp");
		}
	}

	/* There are no more threads, so exit this process: */
	exit(0);
}

static void
_thread_signal(pthread_t pthread, int sig)
{
	int		done;
	long            l;
	pthread_t       new_pthread;
	struct sigaction act;
	void           *arg;

	/*
	 * Assume that the signal will not be dealt with according
	 * to the thread state:
	 */
	done = 0;

	/* Process according to thread state: */
	switch (pthread->state) {
	/* States which do not change when a signal is trapped: */
	case PS_COND_WAIT:
	case PS_DEAD:
	case PS_FDLR_WAIT:
	case PS_FDLW_WAIT:
	case PS_JOIN:
	case PS_MUTEX_WAIT:
	case PS_RUNNING:
	case PS_STATE_MAX:
	case PS_SIGTHREAD:
	case PS_SUSPENDED:
		/* Nothing to do here. */
		break;

	/* Wait for child: */
	case PS_WAIT_WAIT:
		/* Check if the signal is from a child exiting: */
		if (sig == SIGCHLD) {
			/* Reset the error: */
			_thread_seterrno(pthread, 0);

			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread,PS_RUNNING);
		} else {
			/* Return the 'interrupted' error: */
			_thread_seterrno(pthread, EINTR);

			/* Change the state of the thread to run: */
			PTHREAD_NEW_STATE(pthread,PS_RUNNING);
		}
		pthread->interrupted = 1;
		break;

	/* Waiting on I/O for zero or more file descriptors: */
	case PS_SELECT_WAIT:
		pthread->data.select_data->nfds = -1;

		/* Return the 'interrupted' error: */
		_thread_seterrno(pthread, EINTR);
		pthread->interrupted = 1;

		/* Change the state of the thread to run: */
		PTHREAD_NEW_STATE(pthread,PS_RUNNING);
		break;

	/*
	 * States that are interrupted by the occurrence of a signal
	 * other than the scheduling alarm: 
	 */
	case PS_FDR_WAIT:
	case PS_FDW_WAIT:
	case PS_SLEEP_WAIT:
        case PS_SIGWAIT:
		/* Return the 'interrupted' error: */
		_thread_seterrno(pthread, EINTR);
		pthread->interrupted = 1;

		/* Change the state of the thread to run: */
		PTHREAD_NEW_STATE(pthread,PS_RUNNING);

		/* Return the signal number: */
		pthread->signo = sig;
		break;
	}

	/*
	 * Check if this signal has been dealt with, or is being
	 * ignored:
	 */
	if (done || pthread->act[sig - 1].sa_handler == SIG_IGN) {
		/* Ignore the signal for this thread. */
	}
	/* Check if this signal is to use the default handler: */
	else if (pthread->act[sig - 1].sa_handler == SIG_DFL) {
		/* Process according to signal type: */
		switch (sig) {
		/* Signals which cause core dumps: */
		case SIGQUIT:
		case SIGILL:
		case SIGTRAP:
		case SIGABRT:
		case SIGEMT:
		case SIGFPE:
		case SIGBUS:
		case SIGSEGV:
		case SIGSYS:
			/* Clear the signal action: */
			sigfillset(&act.sa_mask);
			act.sa_handler = SIG_DFL;
			act.sa_flags = SA_RESTART;
			_thread_sys_sigaction(sig, &act, NULL);

			/*
			 * Do a sigreturn back to where the signal was
			 * detected and a core dump should occur: 
			 */
			_thread_sys_sigreturn(&pthread->saved_sigcontext);
			break;

		/*
		 * The following signals should terminate the
		 * process. Do this by clearing the signal action
		 * and then re-throwing the signal.
		 */
		case SIGHUP:
		case SIGINT:
		case SIGPIPE:
		case SIGALRM:
		case SIGTERM:
		case SIGXCPU:
		case SIGXFSZ:
		case SIGVTALRM:
		case SIGUSR1:
		case SIGUSR2:
		/* These signals stop the process. Also re-throw them. */
		case SIGTSTP:
		case SIGTTIN:
		case SIGTTOU:
                        /* Clear the signal action: */
                        sigfillset(&act.sa_mask);
                        act.sa_handler = SIG_DFL;
                        act.sa_flags = SA_RESTART;
                        _thread_sys_sigaction(sig, &act, NULL);
			/* Re-throw to ourselves. */
                        kill(getpid(), sig);
			break;

		case SIGCONT:
			/*
			 * If we get this it means that we were
			 * probably stopped and then continued.
			 * Reset the handler for the SIGTSTP, SIGTTIN
			 * and SIGTTOU signals.
			 */

                	sigfillset(&act.sa_mask);
	                act.sa_handler = (void (*) ()) _thread_sig_handler;
			act.sa_flags = SA_RESTART;

                        /* Initialise the signals for default handling: */
                        if (_thread_sys_sigaction(SIGTSTP, &act, NULL) != 0) {
                                PANIC("Cannot initialise SIGTSTP signal handler");
                        }
                        if (_thread_sys_sigaction(SIGTTIN, &act, NULL) != 0) {
                                PANIC("Cannot initialise SIGTTIN signal handler");
                        }
                        if (_thread_sys_sigaction(SIGTTOU, &act, NULL) != 0) {
                                PANIC("Cannot initialise SIGTTOU signal handler");
                        }
			break;

		/* Default processing for other signals: */
		default:
			/*
			 * ### Default processing is a problem to resolve!     
			 * ### 
			 */
			break;
		}
	} else {
		/*
		 * Cast the signal number as a long and then to a void
		 * pointer. Sigh. This is POSIX. 
		 */
		l = (long) sig;
		arg = (void *) l;

		/* Create a signal handler thread, but don't run it yet: */
		if (_thread_create(&new_pthread, NULL, (void *) pthread->act[sig - 1].sa_handler, arg, pthread) != 0) {
			/*
			 * Error creating signal handler thread, so abort
			 * this process: 
			 */
			PANIC("Cannot create signal handler thread");
		}
	}

	/* Nothing to return. */
	return;
}

void
_thread_kern_sig_block(int *status)
{
	sigset_t        oset;

	/*
	 * Block all signals so that the process will not be interrupted by
	 * signals: 
	 */
	_thread_sys_sigprocmask(SIG_SETMASK, &sig_to_block, &oset);

	/* Check if the caller wants the current block status returned: */
	if (status != NULL) {
		/* Return the previous signal block status: */
		*status = (oset != 0);
	}
	return;
}

void
_thread_kern_sig_unblock(int status)
{
	sigset_t        oset;

	/*
	 * Check if the caller thinks that signals weren't blocked when it
	 * called _thread_kern_sig_block: 
	 */
	if (status == 0) {
		/*
		 * Unblock all signals so that the process will be
		 * interrupted when a signal occurs: 
		 */
		_thread_sys_sigprocmask(SIG_SETMASK, &sig_to_unblock, &oset);
	}
	return;
}

void
_thread_kern_sched_state(enum pthread_state state, char *fname, int lineno)
{
	/* Change the state of the current thread: */
	_thread_run->state = state;
	_thread_run->fname = fname;
	_thread_run->lineno = lineno;

	/* Schedule the next thread that is ready: */
	_thread_kern_sched(NULL);
	return;
}

static void
_thread_kern_select(int wait_reqd)
{
	char            bufr[128];
	fd_set          fd_set_except;
	fd_set          fd_set_read;
	fd_set          fd_set_write;
	int             count = 0;
	int             count_dec;
	int             found_one;
	int             i;
	int             nfds = -1;
	int             settimeout;
	pthread_t       pthread;
	ssize_t         num;
	struct timespec ts;
	struct timespec ts1;
	struct timeval *p_tv;
	struct timeval  tv;
	struct timeval  tv1;

	/* Zero the file descriptor sets: */
	FD_ZERO(&fd_set_read);
	FD_ZERO(&fd_set_write);
	FD_ZERO(&fd_set_except);

	/* Check if the caller wants to wait: */
	if (wait_reqd) {
		/*
		 * Add the pthread kernel pipe file descriptor to the read
		 * set: 
		 */
		FD_SET(_thread_kern_pipe[0], &fd_set_read);
		nfds = _thread_kern_pipe[0];

		/* Get the current time of day: */
		gettimeofday(&tv, NULL);
		TIMEVAL_TO_TIMESPEC(&tv, &ts);
	}
	/* Initialise the time value structure: */
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	/*
	 * Enter a loop to process threads waiting on either file descriptors
	 * or times: 
	 */
	for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
		/* Assume that this state does not time out: */
		settimeout = 0;

		/* Process according to thread state: */
		switch (pthread->state) {
		/*
		 * States which do not depend on file descriptor I/O
		 * operations or timeouts: 
		 */
		case PS_DEAD:
		case PS_FDLR_WAIT:
		case PS_FDLW_WAIT:
		case PS_JOIN:
		case PS_MUTEX_WAIT:
		case PS_RUNNING:
		case PS_SIGTHREAD:
		case PS_SIGWAIT:
		case PS_STATE_MAX:
		case PS_WAIT_WAIT:
		case PS_SUSPENDED:
			/* Nothing to do here. */
			break;

		/* File descriptor read wait: */
		case PS_FDR_WAIT:
			/* Add the file descriptor to the read set: */
			FD_SET(pthread->data.fd.fd, &fd_set_read);

			/*
			 * Check if this file descriptor is greater than any
			 * of those seen so far: 
			 */
			if (pthread->data.fd.fd > nfds) {
				/* Remember this file descriptor: */
				nfds = pthread->data.fd.fd;
			}
			/* Increment the file descriptor count: */
			count++;

			/* This state can time out: */
			settimeout = 1;
			break;

		/* File descriptor write wait: */
		case PS_FDW_WAIT:
			/* Add the file descriptor to the write set: */
			FD_SET(pthread->data.fd.fd, &fd_set_write);

			/*
			 * Check if this file descriptor is greater than any
			 * of those seen so far: 
			 */
			if (pthread->data.fd.fd > nfds) {
				/* Remember this file descriptor: */
				nfds = pthread->data.fd.fd;
			}
			/* Increment the file descriptor count: */
			count++;

			/* This state can time out: */
			settimeout = 1;
			break;

		/* States that time out: */
		case PS_SLEEP_WAIT:
		case PS_COND_WAIT:
			/* Flag a timeout as required: */
			settimeout = 1;
			break;

		/* Select wait: */
		case PS_SELECT_WAIT:
			/*
			 * Enter a loop to process each file descriptor in
			 * the thread-specific file descriptor sets: 
			 */
			for (i = 0; i < pthread->data.select_data->nfds; i++) {
				/*
				 * Check if this file descriptor is set for
				 * exceptions: 
				 */
				if (FD_ISSET(i, &pthread->data.select_data->exceptfds)) {
					/*
					 * Add the file descriptor to the
					 * exception set: 
					 */
					FD_SET(i, &fd_set_except);

					/*
					 * Increment the file descriptor
					 * count: 
					 */
					count++;

					/*
					 * Check if this file descriptor is
					 * greater than any of those seen so
					 * far: 
					 */
					if (i > nfds) {
						/*
						 * Remember this file
						 * descriptor: 
						 */
						nfds = i;
					}
				}
				/*
				 * Check if this file descriptor is set for
				 * write: 
				 */
				if (FD_ISSET(i, &pthread->data.select_data->writefds)) {
					/*
					 * Add the file descriptor to the
					 * write set: 
					 */
					FD_SET(i, &fd_set_write);

					/*
					 * Increment the file descriptor
					 * count: 
					 */
					count++;

					/*
					 * Check if this file descriptor is
					 * greater than any of those seen so
					 * far: 
					 */
					if (i > nfds) {
						/*
						 * Remember this file
						 * descriptor: 
						 */
						nfds = i;
					}
				}
				/*
				 * Check if this file descriptor is set for
				 * read: 
				 */
				if (FD_ISSET(i, &pthread->data.select_data->readfds)) {
					/*
					 * Add the file descriptor to the
					 * read set: 
					 */
					FD_SET(i, &fd_set_read);

					/*
					 * Increment the file descriptor
					 * count: 
					 */
					count++;

					/*
					 * Check if this file descriptor is
					 * greater than any of those seen so
					 * far: 
					 */
					if (i > nfds) {
						/*
						 * Remember this file
						 * descriptor: 
						 */
						nfds = i;
					}
				}
			}

			/* This state can time out: */
			settimeout = 1;
			break;
		}

		/*
		 * Check if the caller wants to wait and if the thread state
		 * is one that times out: 
		 */
		if (wait_reqd && settimeout) {
			/* Check if this thread wants to wait forever: */
			if (pthread->wakeup_time.tv_sec == -1) {
			}
			/* Check if this thread doesn't want to wait at all: */
			else if (pthread->wakeup_time.tv_sec == 0 &&
				 pthread->wakeup_time.tv_nsec == 0) {
				/* Override the caller's request to wait: */
				wait_reqd = 0;
			} else {
				/*
				 * Calculate the time until this thread is
				 * ready, allowing for the clock resolution: 
				 */
				ts1.tv_sec = pthread->wakeup_time.tv_sec - ts.tv_sec;
				ts1.tv_nsec = pthread->wakeup_time.tv_nsec - ts.tv_nsec +
					CLOCK_RES_NSEC;

				/*
				 * Check for underflow of the nanosecond
				 * field: 
				 */
				if (ts1.tv_nsec < 0) {
					/*
					 * Allow for the underflow of the
					 * nanosecond field: 
					 */
					ts1.tv_sec--;
					ts1.tv_nsec += 1000000000;
				}
				/*
				 * Check for overflow of the nanosecond
				 * field: 
				 */
				if (ts1.tv_nsec >= 1000000000) {
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
				 * Check if no time value has been found yet,
				 * or if the thread will be ready sooner that
				 * the earliest one found so far: 
				 */
				if ((tv.tv_sec == 0 && tv.tv_usec == 0) || timercmp(&tv1, &tv, <)) {
					/* Update the time value: */
					tv.tv_sec = tv1.tv_sec;
					tv.tv_usec = tv1.tv_usec;
				}
			}
		}
	}

	/* Check if the caller wants to wait: */
	if (wait_reqd) {
		/* Check if no threads were found with timeouts: */
		if (tv.tv_sec == 0 && tv.tv_usec == 0) {
			/* Wait forever: */
			p_tv = NULL;
		} else {
			/*
			 * Point to the time value structure which contains
			 * the earliest time that a thread will be ready: 
			 */
			p_tv = &tv;
		}

		/*
		 * Flag the pthread kernel as in a select. This is to avoid
		 * the window between the next statement that unblocks
		 * signals and the select statement which follows. 
		 */
		_thread_kern_in_select = 1;

		/* Unblock all signals: */
		_thread_kern_sig_unblock(0);

		/*
		 * Wait for a file descriptor to be ready for read, write, or
		 * an exception, or a timeout to occur: 
		 */
		count = _thread_sys_select(nfds + 1, &fd_set_read, &fd_set_write, &fd_set_except, p_tv);

		/* Block all signals again: */
		_thread_kern_sig_block(NULL);

		/* Reset the kernel in select flag: */
		_thread_kern_in_select = 0;

		/*
		 * Check if it is possible that there are bytes in the kernel
		 * read pipe waiting to be read: 
		 */
		if (count < 0 || FD_ISSET(_thread_kern_pipe[0], &fd_set_read)) {
			/*
			 * Check if the kernel read pipe was included in the
			 * count: 
			 */
			if (count > 0) {
				/*
				 * Remove the kernel read pipe from the
				 * count: 
				 */
				FD_CLR(_thread_kern_pipe[0], &fd_set_read);

				/* Decrement the count of file descriptors: */
				count--;
			}
			/*
			 * Enter a loop to read (and trash) bytes from the
			 * pthread kernel pipe: 
			 */
			while ((num = _thread_sys_read(_thread_kern_pipe[0], bufr, sizeof(bufr))) > 0) {
				/*
				 * The buffer read contains one byte per
				 * signal and each byte is the signal number.
				 * This data is not used, but the fact that
				 * the signal handler wrote to the pipe *is*
				 * used to cause the _thread_sys_select call
				 * to complete if the signal occurred between
				 * the time when signals were unblocked and
				 * the _thread_sys_select select call being
				 * made. 
				 */
			}
		}
	}
	/* Check if there are file descriptors to poll: */
	else if (count > 0) {
		/*
		 * Point to the time value structure which has been zeroed so
		 * that the call to _thread_sys_select will not wait: 
		 */
		p_tv = &tv;

		/* Poll file descrptors without wait: */
		count = _thread_sys_select(nfds + 1, &fd_set_read, &fd_set_write, &fd_set_except, p_tv);
	}
	/*
	 * Check if the select call was interrupted, or some other error
	 * occurred: 
	 */
	if (count < 0) {
		/* Check if the select call was interrupted: */
		if (errno == EINTR) {
			/*
			 * Interrupted calls are expected. The interrupting
			 * signal will be in the sigpend array. 
			 */
		} else {
			/* This should not occur: */
		}
	}
	/* Check if no file descriptors are ready: */
	else if (count == 0) {
		/* Nothing to do here.                                              */
	} else {
		/*
		 * Enter a loop to look for threads waiting on file
		 * descriptors that are flagged as available by the
		 * _thread_sys_select syscall: 
		 */
		for (pthread = _thread_link_list; pthread != NULL; pthread = pthread->nxt) {
			/* Process according to thread state: */
			switch (pthread->state) {
			/*
			 * States which do not depend on file
			 * descriptor I/O operations: 
			 */
			case PS_RUNNING:
			case PS_COND_WAIT:
			case PS_DEAD:
			case PS_FDLR_WAIT:
			case PS_FDLW_WAIT:
			case PS_JOIN:
			case PS_MUTEX_WAIT:
			case PS_SIGWAIT:
			case PS_SLEEP_WAIT:
			case PS_WAIT_WAIT:
			case PS_SIGTHREAD:
			case PS_STATE_MAX:
			case PS_SUSPENDED:
				/* Nothing to do here. */
				break;

			/* File descriptor read wait: */
			case PS_FDR_WAIT:
				/*
				 * Check if the file descriptor is available
				 * for read: 
				 */
				if (FD_ISSET(pthread->data.fd.fd, &fd_set_read)) {
					/*
					 * Change the thread state to allow
					 * it to read from the file when it
					 * is scheduled next: 
					 */
					pthread->state = PS_RUNNING;
				}
				break;

			/* File descriptor write wait: */
			case PS_FDW_WAIT:
				/*
				 * Check if the file descriptor is available
				 * for write: 
				 */
				if (FD_ISSET(pthread->data.fd.fd, &fd_set_write)) {
					/*
					 * Change the thread state to allow
					 * it to write to the file when it is
					 * scheduled next: 
					 */
					pthread->state = PS_RUNNING;
				}
				break;

			/* Select wait: */
			case PS_SELECT_WAIT:
				/*
				 * Reset the flag that indicates if a file
				 * descriptor is ready for some type of
				 * operation: 
				 */
				count_dec = 0;

				/*
				 * Enter a loop to search though the
				 * thread-specific select file descriptors
				 * for the first descriptor that is ready: 
				 */
				for (i = 0; i < pthread->data.select_data->nfds && count_dec == 0; i++) {
					/*
					 * Check if this file descriptor does
					 * not have an exception: 
					 */
					if (FD_ISSET(i, &pthread->data.select_data->exceptfds) && FD_ISSET(i, &fd_set_except)) {
						/*
						 * Flag this file descriptor
						 * as ready: 
						 */
						count_dec = 1;
					}
					/*
					 * Check if this file descriptor is
					 * not ready for write: 
					 */
					if (FD_ISSET(i, &pthread->data.select_data->writefds) && FD_ISSET(i, &fd_set_write)) {
						/*
						 * Flag this file descriptor
						 * as ready: 
						 */
						count_dec = 1;
					}
					/*
					 * Check if this file descriptor is
					 * not ready for read: 
					 */
					if (FD_ISSET(i, &pthread->data.select_data->readfds) && FD_ISSET(i, &fd_set_read)) {
						/*
						 * Flag this file descriptor
						 * as ready: 
						 */
						count_dec = 1;
					}
				}

				/*
				 * Check if any file descriptors are ready
				 * for the current thread: 
				 */
				if (count_dec) {
					/*
					 * Reset the count of file
					 * descriptors that are ready for
					 * this thread: 
					 */
					found_one = 0;

					/*
					 * Enter a loop to search though the
					 * thread-specific select file
					 * descriptors: 
					 */
					for (i = 0; i < pthread->data.select_data->nfds; i++) {
						/*
						 * Reset the count of
						 * operations for which the
						 * current file descriptor is
						 * ready: 
						 */
						count_dec = 0;

						/*
						 * Check if this file
						 * descriptor is selected for
						 * exceptions: 
						 */
						if (FD_ISSET(i, &pthread->data.select_data->exceptfds)) {
							/*
							 * Check if this file
							 * descriptor has an
							 * exception: 
							 */
							if (FD_ISSET(i, &fd_set_except)) {
								/*
								 * Increment
								 * the count
								 * for this
								 * file: 
								 */
								count_dec++;
							} else {
								/*
								 * Clear the
								 * file
								 * descriptor
								 * in the
								 * thread-spec
								 * ific file
								 * descriptor
								 * set: 
								 */
								FD_CLR(i, &pthread->data.select_data->exceptfds);
							}
						}
						/*
						 * Check if this file
						 * descriptor is selected for
						 * write: 
						 */
						if (FD_ISSET(i, &pthread->data.select_data->writefds)) {
							/*
							 * Check if this file
							 * descriptor is
							 * ready for write: 
							 */
							if (FD_ISSET(i, &fd_set_write)) {
								/*
								 * Increment
								 * the count
								 * for this
								 * file: 
								 */
								count_dec++;
							} else {
								/*
								 * Clear the
								 * file
								 * descriptor
								 * in the
								 * thread-spec
								 * ific file
								 * descriptor
								 * set: 
								 */
								FD_CLR(i, &pthread->data.select_data->writefds);
							}
						}
						/*
						 * Check if this file
						 * descriptor is selected for
						 * read: 
						 */
						if (FD_ISSET(i, &pthread->data.select_data->readfds)) {
							/*
							 * Check if this file
							 * descriptor is
							 * ready for read: 
							 */
							if (FD_ISSET(i, &fd_set_read)) {
								/*
								 * Increment
								 * the count
								 * for this
								 * file: 
								 */
								count_dec++;
							} else {
								/*
								 * Clear the
								 * file
								 * descriptor
								 * in the
								 * thread-spec
								 * ific file
								 * descriptor
								 * set: 
								 */
								FD_CLR(i, &pthread->data.select_data->readfds);
							}
						}
						/*
						 * Check if the current file
						 * descriptor is ready for
						 * any one of the operations: 
						 */
						if (count_dec > 0) {
							/*
							 * Increment the
							 * count of file
							 * descriptors that
							 * are ready for the
							 * current thread: 
							 */
							found_one++;
						}
					}

					/*
					 * Return the number of file
					 * descriptors that are ready: 
					 */
					pthread->data.select_data->nfds = found_one;

					/*
					 * Change the state of the current
					 * thread to run: 
					 */
					pthread->state = PS_RUNNING;
				}
				break;
			}
		}
	}

	/* Nothing to return. */
	return;
}

void
_thread_kern_set_timeout(struct timespec * timeout)
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
#endif
