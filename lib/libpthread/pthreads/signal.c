/* ==== signal.c ============================================================
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
 * Description : Queue functions.
 *
 *  1.00 93/07/21 proven
 *      -Started coding this file.
 */

#include <pthread.h>
#include <signal.h>

/*
 * Time which select in fd_kern_wait() will sleep.
 * If there are no threads to run we sleep for an hour or until
 * we get an interrupt or an fd thats awakens. To make sure we
 * don't miss an interrupt this variable gets reset too zero in
 * sig_handler_real().
 */
struct timeval __fd_kern_wait_timeout = { 0, 0 };

/*
 * Global for user-kernel lock, and blocked signals
 */
static volatile	sigset_t sig_to_tryagain;
static volatile	sigset_t sig_to_process;
static volatile	int kernel_lock = 0;
static volatile	int	sig_count = 0;

static void sig_handler(int signal);
static void set_thread_timer();
void sig_prevent(void);
void sig_resume(void);

/* ==========================================================================
 * context_switch()
 *
 * This routine saves the current state of the running thread gets
 * the next thread to run and restores it's state. To allow different
 * processors to work with this routine, I allow the machdep_restore_state()
 * to either return or have it return from machdep_save_state with a value
 * other than 0, this is for implementations which use setjmp/longjmp. 
 */
static void context_switch()
{
	struct pthread **current, *next, *last;
	semaphore *lock;
	int count;

	/* save state of current thread */
	if (machdep_save_state()) {
		return;
	}

	last = pthread_run;
	if (pthread_run = pthread_queue_deq(&pthread_current_queue)) {
		/* restore state of new current thread */
		machdep_restore_state();
		return;
	}

	/* Poll all the kernel fds */
	fd_kern_poll();

context_switch_reschedule:;
	/*
	 * Go through the reschedule list once, this is the only place
	 * that goes through the queue without using the queue routines.
	 *
	 * But first delete the current queue.
	 */
	pthread_queue_init(&pthread_current_queue);
	current = &(pthread_link_list);
	count = 0;

	while (*current) {
		switch((*current)->state) {
		case PS_RUNNING:
			pthread_queue_enq(&pthread_current_queue, *current);
			current = &((*current)->pll);
			count++;
			break;
		case PS_DEAD:
			/* Cleanup thread, unless we're using the stack */
			if (((*current)->flags & PF_DETACHED) && (*current != last)) {
				next = (*current)->pll;
				lock = &((*current)->lock);
				if (SEMAPHORE_TEST_AND_SET(lock)) {
					/* Couldn't cleanup this time, try again later */
					current = &((*current)->pll);
				} else {
					if (!((*current)->attr.stackaddr_attr)) {
						free (machdep_pthread_cleanup(&((*current)->machdep_data)));
					}
					free (*current);
					*current = next;
				}
			} else {
				current = &((*current)->pll);
			}
			break;
		default:
			/* Should be on a different queue. Ignore. */
			current = &((*current)->pll);
			count++;
			break;
		}
	}

	/* Are there any threads to run */
	if (pthread_run = pthread_queue_deq(&pthread_current_queue)) {
        /* restore state of new current thread */
		machdep_restore_state();
        return;
    }

	/* Are there any threads at all */
	if (count) {
		/*
		 * Do a wait, timeout is set to a hour unless we get an interrupt
		 * before the select in wich case it polls and returns. 
		 */
		fd_kern_wait();

		/* Check for interrupts, but ignore SIGVTALR */
		sigdelset(&sig_to_process, SIGVTALRM); 

		if (sig_to_process) {
			/* Process interrupts */
			sig_handler(0); 
		}

		goto context_switch_reschedule;

	}
	exit(0);
}

/* ==========================================================================
 * sig_handler_pause()
 * 
 * Wait until a signal is sent to the process.
 */
void sig_handler_pause()
{
	sigset_t sig_to_block, sig_to_pause;

	sigfillset(&sig_to_block);
	sigemptyset(&sig_to_pause);
	sigprocmask(SIG_BLOCK, &sig_to_block, NULL);
	if (!sig_to_process) {
		sigsuspend(&sig_to_pause);
	}
	sigprocmask(SIG_UNBLOCK, &sig_to_block, NULL);
}

/* ==========================================================================
 * context_switch_done()
 *
 * This routine does all the things that are necessary after a context_switch()
 * calls the machdep_restore_state(). DO NOT put this in the context_switch()
 * routine because sometimes the machdep_restore_state() doesn't return
 * to context_switch() but instead ends up in machdep_thread_start() or
 * some such routine, which will need to call this routine and
 * sig_check_and_resume().
 */
void context_switch_done()
{
	sigdelset(&sig_to_process, SIGVTALRM);
	set_thread_timer();
}

/* ==========================================================================
 * set_thread_timer()
 *
 * Assums kernel is locked.
 */
static void set_thread_timer()
{
	static int last_sched_attr = SCHED_RR;

	switch (pthread_run->attr.sched_attr) {
	case SCHED_RR:
		machdep_set_thread_timer(&(pthread_run->machdep_data));
		break;
	case SCHED_FIFO:
		if (last_sched_attr != SCHED_FIFO) {
			machdep_unset_thread_timer();
		}
		break;
	case SCHED_IO:
		if ((last_sched_attr != SCHED_IO) && (!sig_count)) {
			machdep_set_thread_timer(&(pthread_run->machdep_data));
		}
		break;
	default:
		machdep_set_thread_timer(&(pthread_run->machdep_data));
		break;
	} 
    last_sched_attr = pthread_run->attr.sched_attr;
}

/* ==========================================================================
 * sig_handler()
 *
 * Assumes the kernel is locked. 
 */
static void sig_handler(int sig)
{

	/*
	 * First check for old signals, do one pass through and don't
 	 * check any twice.
     */
	if (sig_to_tryagain) {
		if (sigismember(&sig_to_tryagain, SIGALRM)) {
			switch (sleep_wakeup()) {
			case 1:
				/* Do the default action, no threads were sleeping */
			case OK:
				/* Woke up a sleeping thread */
				sigdelset(&sig_to_tryagain, SIGALRM);
				break;
			case NOTOK:
				/* Couldn't get appropriate locks, try again later */
				break;
			}
		} else {
			PANIC();
		}
	}
		
	/*
	 * NOW, process signal that just came in, plus any pending on the
	 * signal mask. All of these must be resolved.
	 */

sig_handler_top:;

	switch(sig) {
	case 0:
		break;
	case SIGVTALRM:
		if (sig_count) {
			sigset_t sigall;

			sig_count = 0;

			/* Unblock all signals */
			sigemptyset(&sigall);
			sigprocmask(SIG_SETMASK, &sigall, NULL); 
		}
		context_switch();
		context_switch_done();
		break;
	case SIGALRM:
		sigdelset(&sig_to_process, SIGALRM);
		switch (sleep_wakeup()) {
		case 1:
			/* Do the default action, no threads were sleeping */
		case OK:
			/* Woke up a sleeping thread */
			break;
		case NOTOK:
			/* Couldn't get appropriate locks, try again later */
			sigaddset(&sig_to_tryagain, SIGALRM);
			break;
		} 
		break;
	default:
		PANIC();
	}

	/* Determine if there are any other signals */
	if (sig_to_process) {
		for (sig = 1; sig <= SIGMAX; sig++) {
			if (sigismember(&sig_to_process, sig)) {
		
				/* goto sig_handler_top */
				goto sig_handler_top;
			}
		}
	}
}

/* ==========================================================================
 * sig_handler_real()
 * 
 * On a multi-processor this would need to use the test and set instruction
 * otherwise the following will work.
 */
void sig_handler_real(int sig)
{
	if (kernel_lock) {
		__fd_kern_wait_timeout.tv_sec = 0;
		sigaddset(&sig_to_process, sig);
		return;
	}
	sig_prevent();
	sig_count++;
	sig_handler(sig);
	sig_resume();
}

/* ==========================================================================
 * sig_handler_fake()
 */
void sig_handler_fake(int sig)
{
	if (kernel_lock) {
		/* Currently this should be impossible */
		PANIC();
	}
	sig_prevent();
	sig_handler(sig);
	sig_resume();
}

/* ==========================================================================
 * reschedule()
 *
 * This routine assumes that the caller is the current pthread, pthread_run
 * and that it has a lock on itself and that it wants to reschedule itself.
 */
void reschedule(enum pthread_state state)
{
	semaphore *plock;

	if (kernel_lock) {
		/* Currently this should be impossible */
		PANIC();
	}
	sig_prevent();
	pthread_run->state = state;
	SEMAPHORE_RESET((plock = &(pthread_run->lock)));
	sig_handler(SIGVTALRM);
	sig_resume();
}

/* ==========================================================================
 * sig_prevent()
 */
void sig_prevent(void)
{
	kernel_lock++;
}

/* ==========================================================================
 * sig_resume()
 */
void sig_resume()
{
	kernel_lock--;
}

/* ==========================================================================
 * sig_check_and_resume()
 */
void sig_check_and_resume()
{
	/* Some routine name that is yet to be determined. */
	
	/* Only bother if we are truely unlocking the kernel */
	while (!(--kernel_lock)) {

		/* Assume sigset_t is not a struct or union */
		if (sig_to_process) {
			kernel_lock++;
			sig_handler(0);
		} else {
			break;
		}
	}
}

/* ==========================================================================
 * sig_init()
 *
 * SIGVTALRM	(NOT POSIX) needed for thread timeslice timeouts.
 *				Since it's not POSIX I will replace it with a 
 *				virtual timer for threads.
 * SIGALRM		(IS POSIX) so some special handling will be
 * 				necessary to fake SIGALRM signals
 */
void sig_init(void)
{
	int sig_to_init[] = { SIGVTALRM, SIGALRM, 0 };
	int i;

	/* Initialize only the necessary signals */

	for (i = 0; sig_to_init[i]; i++) {
		if (signal(sig_to_init[i], sig_handler_real)) {
			PANIC();
		}
	}
}

