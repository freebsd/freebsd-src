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
 */
#include <sys/param.h>
#include <sys/types.h>
#include <sys/signalvar.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "thr_private.h"
#include "pthread_md.h"

/* Prototypes: */
static void	build_siginfo(siginfo_t *info, int signo);
/* static void	thr_sig_add(struct pthread *pthread, int sig, siginfo_t *info); */
static void	thr_sig_check_state(struct pthread *pthread, int sig);
static struct pthread *thr_sig_find(struct kse *curkse, int sig,
		    siginfo_t *info);
static void	handle_special_signals(struct kse *curkse, int sig);
static void	thr_sigframe_add(struct pthread *thread, int sig,
		    siginfo_t *info);
static void	thr_sigframe_restore(struct pthread *thread,
		    struct pthread_sigframe *psf);
static void	thr_sigframe_save(struct pthread *thread,
		    struct pthread_sigframe *psf);
static void	thr_sig_invoke_handler(struct pthread *, int sig,
		    siginfo_t *info, ucontext_t *ucp);

/* #define DEBUG_SIGNAL */
#ifdef DEBUG_SIGNAL
#define DBG_MSG		stdout_debug
#else
#define DBG_MSG(x...)
#endif

/*
 * Signal setup and delivery.
 *
 * 1) Delivering signals to threads in the same KSE.
 *    These signals are sent by upcall events and are set in the
 *    km_sigscaught field of the KSE mailbox.  Since these signals
 *    are received while operating on the KSE stack, they can be
 *    delivered either by using signalcontext() to add a stack frame
 *    to the target thread's stack, or by adding them in the thread's
 *    pending set and having the thread run them down after it 
 * 2) Delivering signals to threads in other KSEs/KSEGs.
 * 3) Delivering signals to threads in critical regions.
 * 4) Delivering signals to threads after they change their signal masks.
 *
 * Methods of delivering signals.
 *
 *   1) Add a signal frame to the thread's saved context.
 *   2) Add the signal to the thread structure, mark the thread as
 *  	having signals to handle, and let the thread run them down
 *  	after it resumes from the KSE scheduler.
 *
 * Problem with 1).  You can't do this to a running thread or a
 * thread in a critical region.
 *
 * Problem with 2).  You can't do this to a thread that doesn't
 * yield in some way (explicitly enters the scheduler).  A thread
 * blocked in the kernel or a CPU hungry thread will not see the
 * signal without entering the scheduler.
 *
 * The solution is to use both 1) and 2) to deliver signals:
 *
 *   o Thread in critical region - use 2).  When the thread
 *     leaves the critical region it will check to see if it
 *     has pending signals and run them down.
 *
 *   o Thread enters scheduler explicitly - use 2).  The thread
 *     can check for pending signals after it returns from the
 *     the scheduler.
 *
 *   o Thread is running and not current thread - use 2).  When the
 *     thread hits a condition specified by one of the other bullets,
 *     the signal will be delivered.
 *
 *   o Thread is running and is current thread (e.g., the thread
 *     has just changed its signal mask and now sees that it has
 *     pending signals) - just run down the pending signals.
 *
 *   o Thread is swapped out due to quantum expiration - use 1)
 *
 *   o Thread is blocked in kernel - kse_thr_wakeup() and then
 *     use 1)
 */

/*
 * Rules for selecting threads for signals received:
 *
 *   1) If the signal is a sychronous signal, it is delivered to
 *      the generating (current thread).  If the thread has the
 *      signal masked, it is added to the threads pending signal
 *      set until the thread unmasks it.
 *
 *   2) A thread in sigwait() where the signal is in the thread's
 *      waitset.
 *
 *   3) A thread in sigsuspend() where the signal is not in the
 *      thread's suspended signal mask.
 *
 *   4) Any thread (first found/easiest to deliver) that has the
 *      signal unmasked.
 */

/*
 * This signal handler only delivers asynchronous signals.
 * This must be called with upcalls disabled and without
 * holding any locks.
 */
void
_thr_sig_dispatch(struct kse *curkse, int sig, siginfo_t *info)
{
	struct pthread *thread;

	DBG_MSG(">>> _thr_sig_dispatch(%d)\n", sig);

	/* Some signals need special handling: */
	handle_special_signals(curkse, sig);
	DBG_MSG("dispatch sig:%d\n", sig);
	while ((thread = thr_sig_find(curkse, sig, info)) != NULL) {
		/*
		 * Setup the target thread to receive the signal:
		 */
		DBG_MSG("Got signal %d, selecting thread %p\n", sig, thread);
		KSE_SCHED_LOCK(curkse, thread->kseg);
		if ((thread->state == PS_DEAD) ||
		    (thread->state == PS_DEADLOCK) ||
		    THR_IS_EXITING(thread) || THR_IS_SUSPENDED(thread)) {
			KSE_SCHED_UNLOCK(curkse, thread->kseg);
			_thr_ref_delete(NULL, thread);
		} else if (sigismember(&thread->tmbx.tm_context.uc_sigmask,
		    sig)) {
			KSE_SCHED_UNLOCK(curkse, thread->kseg);
			_thr_ref_delete(NULL, thread);
		}
		else {
			_thr_sig_add(thread, sig, info);
			KSE_SCHED_UNLOCK(curkse, thread->kseg);
			_thr_ref_delete(NULL, thread);
			break;
		}
	}
}

void
_thr_sig_handler(int sig, siginfo_t *info, ucontext_t *ucp)
{
	__siginfohandler_t *sigfunc;
	struct kse *curkse;

	curkse = _get_curkse();
	if ((curkse == NULL) || ((curkse->k_flags & KF_STARTED) == 0)) {
		/* Upcalls are not yet started; just call the handler. */
		sigfunc = _thread_sigact[sig - 1].sa_sigaction;
		ucp->uc_sigmask = _thr_proc_sigmask;
		if (((__sighandler_t *)sigfunc != SIG_DFL) &&
		    ((__sighandler_t *)sigfunc != SIG_IGN) &&
		    (sigfunc != (__siginfohandler_t *)_thr_sig_handler)) {
			if (((_thread_sigact[sig - 1].sa_flags & SA_SIGINFO)
			    != 0) || (info == NULL))
				(*(sigfunc))(sig, info, ucp);
			else
				(*(sigfunc))(sig, (siginfo_t *)info->si_code,
				    ucp);
		}
	}
	else {
		/* Nothing. */
		DBG_MSG("Got signal %d\n", sig);
		sigaddset(&curkse->k_mbx.km_sigscaught, sig);
		ucp->uc_sigmask = _thr_proc_sigmask;
	}
}

static void
thr_sig_invoke_handler(struct pthread *curthread, int sig, siginfo_t *info,
    ucontext_t *ucp)
{
	void (*sigfunc)(int, siginfo_t *, void *);
	sigset_t saved_mask;
	int saved_seqno;

	/* Invoke the signal handler without going through the scheduler:
	 */
	DBG_MSG("Got signal %d, calling handler for current thread %p\n",
	    sig, curthread);

	/*
	 * Setup the threads signal mask.
	 *
	 * The mask is changed in the thread's active signal mask
	 * (in the context) and not in the base signal mask because
	 * a thread is allowed to change its signal mask within a
	 * signal handler.  If it does, the signal mask restored
	 * after the handler should be the same as that set by the
	 * thread during the handler, not the original mask from
	 * before calling the handler.  The thread could also
	 * modify the signal mask in the context and expect this
	 * mask to be used.
	 */
	THR_SCHED_LOCK(curthread, curthread);
	saved_mask = curthread->tmbx.tm_context.uc_sigmask;
	saved_seqno = curthread->sigmask_seqno;
	SIGSETOR(curthread->tmbx.tm_context.uc_sigmask,
	    _thread_sigact[sig - 1].sa_mask);
	sigaddset(&curthread->tmbx.tm_context.uc_sigmask, sig);
	THR_SCHED_UNLOCK(curthread, curthread);

	/*
	 * Check that a custom handler is installed and if
	 * the signal is not blocked:
	 */
	sigfunc = _thread_sigact[sig - 1].sa_sigaction;
	ucp->uc_sigmask = _thr_proc_sigmask;
	if (((__sighandler_t *)sigfunc != SIG_DFL) &&
	    ((__sighandler_t *)sigfunc != SIG_IGN)) {
		if (((_thread_sigact[sig - 1].sa_flags & SA_SIGINFO) != 0) ||
		    (info == NULL))
			(*(sigfunc))(sig, info, ucp);
		else
			(*(sigfunc))(sig, (siginfo_t *)info->si_code, ucp);
	}

	/*
	 * Restore the thread's signal mask.
	 */
	if (saved_seqno == curthread->sigmask_seqno)
		curthread->tmbx.tm_context.uc_sigmask = saved_mask;
	else
		curthread->tmbx.tm_context.uc_sigmask = curthread->sigmask;
}

/*
 * Find a thread that can handle the signal.  This must be called
 * with upcalls disabled.
 */
struct pthread *
thr_sig_find(struct kse *curkse, int sig, siginfo_t *info)
{
	int		handler_installed;
	struct pthread	*pthread;
	struct pthread	*suspended_thread, *signaled_thread;

	DBG_MSG("Looking for thread to handle signal %d\n", sig);

	handler_installed = (_thread_sigact[sig - 1].sa_handler != SIG_IGN) &&
	    (_thread_sigact[sig - 1].sa_handler != SIG_DFL);

	/* Check if the signal requires a dump of thread information: */
	if (sig == SIGINFO) {
		/* Dump thread information to file: */
		_thread_dump_info();
	}
	/*
	 * Enter a loop to look for threads that have the signal
	 * unmasked.  POSIX specifies that a thread in a sigwait
	 * will get the signal over any other threads.  Second
	 * preference will be threads in in a sigsuspend.  Third
	 * preference will be the current thread.  If none of the
	 * above, then the signal is delivered to the first thread
	 * that is found.  Note that if a custom handler is not
	 * installed, the signal only affects threads in sigwait.
	 */
	suspended_thread = NULL;
	signaled_thread = NULL;

	KSE_LOCK_ACQUIRE(curkse, &_thread_list_lock);
	TAILQ_FOREACH(pthread, &_thread_list, tle) {
		/* Take the scheduling lock. */
		KSE_SCHED_LOCK(curkse, pthread->kseg);
		if ((pthread->state == PS_SIGWAIT) &&
		    sigismember(pthread->data.sigwait, sig)) {
			/*
			 * Return the signal number and make the
			 * thread runnable.
			 */
			pthread->signo = sig;
			_thr_setrunnable_unlocked(pthread);

			KSE_SCHED_UNLOCK(curkse, pthread->kseg);

			/*
			 * POSIX doesn't doesn't specify which thread
			 * will get the signal if there are multiple
			 * waiters, so we give it to the first thread
			 * we find.
			 *
			 * Do not attempt to deliver this signal
			 * to other threads and do not add the signal
			 * to the process pending set.
			 */
			KSE_LOCK_RELEASE(curkse, &_thread_list_lock);
			DBG_MSG("Waking thread %p in sigwait with signal %d\n",
			    pthread, sig);
			return (NULL);
		}
		else if ((pthread->state == PS_DEAD) ||
		    (pthread->state == PS_DEADLOCK) ||
		    THR_IS_EXITING(pthread) || THR_IS_SUSPENDED(pthread))
			;	/* Skip this thread. */
		else if ((handler_installed != 0) &&
		    !sigismember(&pthread->tmbx.tm_context.uc_sigmask, sig)) {
			if (pthread->state == PS_SIGSUSPEND) {
				if (suspended_thread == NULL) {
					suspended_thread = pthread;
					suspended_thread->refcount++;
				}
			} else if (signaled_thread == NULL) {
				signaled_thread = pthread;
				signaled_thread->refcount++;
			}		
		}
		KSE_SCHED_UNLOCK(curkse, pthread->kseg);
	}
	KSE_LOCK_RELEASE(curkse, &_thread_list_lock);

	/*
	 * Only perform wakeups and signal delivery if there is a
	 * custom handler installed:
	 */
	if (handler_installed == 0) {
		/*
		 * There is no handler installed; nothing to do here.
		 */
	} else if (suspended_thread == NULL &&
	    signaled_thread == NULL) {
		/*
		 * Add it to the set of signals pending
		 * on the process:
		 */
		KSE_LOCK_ACQUIRE(curkse, &_thread_signal_lock);
		if (!sigismember(&_thr_proc_sigpending, sig)) {
			sigaddset(&_thr_proc_sigpending, sig);
			if (info == NULL)
				build_siginfo(&_thr_proc_siginfo[sig], sig);
			else
				memcpy(&_thr_proc_siginfo[sig], info,
				    sizeof(*info));
		}
		KSE_LOCK_RELEASE(curkse, &_thread_signal_lock);
	} else {
		/*
		 * We only deliver the signal to one thread;
		 * give preference to the suspended thread:
		 */
		if (suspended_thread != NULL) {
			pthread = suspended_thread;
			_thr_ref_delete(NULL, signaled_thread);
		} else
			pthread = signaled_thread;
		return (pthread);
	}
	return (NULL);
}

static void
build_siginfo(siginfo_t *info, int signo)
{
	bzero(info, sizeof(*info));
	info->si_signo = signo;
	info->si_pid = _thr_pid;
}

/*
 * This is called by a thread when it has pending signals to deliver.
 * It should only be called from the context of the thread.
 */
void
_thr_sig_rundown(struct pthread *curthread, ucontext_t *ucp,
    struct pthread_sigframe *psf)
{
	struct pthread_sigframe psf_save;
	sigset_t sigset;
	int i;

	THR_SCHED_LOCK(curthread, curthread);
	memcpy(&sigset, &curthread->sigpend, sizeof(sigset));
	sigemptyset(&curthread->sigpend);
	if (psf != NULL) {
		memcpy(&psf_save, psf, sizeof(*psf));
		SIGSETOR(sigset, psf_save.psf_sigset);
		sigemptyset(&psf->psf_sigset);
	}
	THR_SCHED_UNLOCK(curthread, curthread);

	/* Check the threads previous state: */
	if ((psf != NULL) && (psf_save.psf_state != PS_RUNNING)) {
		/*
		 * Do a little cleanup handling for those threads in
		 * queues before calling the signal handler.  Signals
		 * for these threads are temporarily blocked until
		 * after cleanup handling.
		 */
		switch (psf_save.psf_state) {
		case PS_COND_WAIT:
			_cond_wait_backout(curthread);
			psf_save.psf_state = PS_RUNNING;
			break;
	
		case PS_MUTEX_WAIT:
			_mutex_lock_backout(curthread);
			psf_save.psf_state = PS_RUNNING;
			break;
	
		default:
			break;
		}
	}
	/*
	 * Lower the priority before calling the handler in case
	 * it never returns (longjmps back):
	 */
	curthread->active_priority &= ~THR_SIGNAL_PRIORITY;

	for (i = 1; i < NSIG; i++) {
		if (sigismember(&sigset, i) != 0) {
			/* Call the handler: */
			thr_sig_invoke_handler(curthread, i,
			    &curthread->siginfo[i], ucp);
		}
	}

	THR_SCHED_LOCK(curthread, curthread);
	if (psf != NULL)
		thr_sigframe_restore(curthread, &psf_save);
	/* Restore the signal mask. */
	curthread->tmbx.tm_context.uc_sigmask = curthread->sigmask;
	THR_SCHED_UNLOCK(curthread, curthread);
	_thr_sig_check_pending(curthread);
}

/*
 * This checks pending signals for the current thread.  It should be
 * called whenever a thread changes its signal mask.  Note that this
 * is called from a thread (using its stack).
 *
 * XXX - We might want to just check to see if there are pending
 *       signals for the thread here, but enter the UTS scheduler
 *       to actually install the signal handler(s).
 */
void
_thr_sig_check_pending(struct pthread *curthread)
{
	sigset_t sigset;
	sigset_t pending_process;
	sigset_t pending_thread;
	kse_critical_t crit;
	int i;

	curthread->check_pending = 0;

	/*
	 * Check if there are pending signals for the running
	 * thread or process that aren't blocked:
	 */
	crit = _kse_critical_enter();
	KSE_LOCK_ACQUIRE(curthread->kse, &_thread_signal_lock);
	sigset = _thr_proc_sigpending;
	KSE_LOCK_RELEASE(curthread->kse, &_thread_signal_lock);
	_kse_critical_leave(crit);

	THR_SCHED_LOCK(curthread, curthread);
	SIGSETOR(sigset, curthread->sigpend);
	SIGSETNAND(sigset, curthread->tmbx.tm_context.uc_sigmask);
	if (SIGNOTEMPTY(sigset)) {
		ucontext_t uc;
		volatile int once;

		curthread->check_pending = 0;
		THR_SCHED_UNLOCK(curthread, curthread);

		/*
		 * Split the pending signals into those that were
		 * pending on the process and those that were pending
		 * on the thread.
		 */
		sigfillset(&pending_process);
		sigfillset(&pending_thread);
		for (i = 1; i < NSIG; i++) {
			if (sigismember(&sigset, i) != 0) {
				if (sigismember(&curthread->sigpend, i) != 0) {
					build_siginfo(&curthread->siginfo[i], i);
					sigdelset(&pending_thread, i);
				} else {
					memcpy(&curthread->siginfo[i],
					    &_thr_proc_siginfo[i],
					    sizeof(siginfo_t));
					sigdelset(&pending_process, i);
				}
			}
		}
		/*
		 * Remove any process pending signals that were scheduled
		 * to be delivered from process' pending set.
		 */
		crit = _kse_critical_enter();
		KSE_LOCK_ACQUIRE(curthread->kse, &_thread_signal_lock);
		SIGSETAND(_thr_proc_sigpending, pending_process);
		KSE_LOCK_RELEASE(curthread->kse, &_thread_signal_lock);
		_kse_critical_leave(crit);

		/*
		 * Remove any thread pending signals that were scheduled
		 * to be delivered from thread's pending set.
		 */
		THR_SCHED_LOCK(curthread, curthread);
		SIGSETAND(curthread->sigpend, pending_thread);
		THR_SCHED_UNLOCK(curthread, curthread);

		once = 0;
		THR_GETCONTEXT(&uc);
		if (once == 0) {
			once = 1;
			for (i = 1; i < NSIG; i++) {
				if (sigismember(&sigset, i) != 0) {
					/* Call the handler: */
					thr_sig_invoke_handler(curthread, i,
					    &curthread->siginfo[i], &uc);
				}
			}
		}
	}
	else
		THR_SCHED_UNLOCK(curthread, curthread);
}

/*
 * This must be called with upcalls disabled.
 */
static void
handle_special_signals(struct kse *curkse, int sig)
{
	switch (sig) {
	/*
	 * POSIX says that pending SIGCONT signals are
	 * discarded when one of these signals occurs.
	 */
	case SIGTSTP:
	case SIGTTIN:
	case SIGTTOU:
		KSE_LOCK_ACQUIRE(curkse, &_thread_signal_lock);
		sigdelset(&_thr_proc_sigpending, SIGCONT);
		KSE_LOCK_RELEASE(curkse, &_thread_signal_lock);
		break;

	default:
		break;
	}
}

/*
 * Perform thread specific actions in response to a signal.
 * This function is only called if there is a handler installed
 * for the signal, and if the target thread has the signal
 * unmasked.
 *
 * This must be called with the thread's scheduling lock held.
 */
void
_thr_sig_add(struct pthread *pthread, int sig, siginfo_t *info)
{
	int	restart;
	int	suppress_handler = 0;

	if (pthread->curframe == NULL) {
		/*
		 * This thread is active.  Just add it to the
		 * thread's pending set.
		 */
		sigaddset(&pthread->sigpend, sig);
		pthread->check_pending = 1;
		if (info == NULL)
			build_siginfo(&pthread->siginfo[sig], sig);
		else if (info != &pthread->siginfo[sig])
			memcpy(&pthread->siginfo[sig], info,
			    sizeof(*info));
		if ((pthread->blocked != 0) && !THR_IN_CRITICAL(pthread))
			kse_thr_interrupt(&pthread->tmbx /* XXX - restart?!?! */);
	}
	else {
		restart = _thread_sigact[sig - 1].sa_flags & SA_RESTART;

		/* Make sure this signal isn't still in the pending set: */
		sigdelset(&pthread->sigpend, sig);

		/*
		 * Process according to thread state:
		 */
		switch (pthread->state) {
		/*
		 * States which do not change when a signal is trapped:
		 */
		case PS_DEAD:
		case PS_DEADLOCK:
		case PS_LOCKWAIT:
		case PS_SUSPENDED:
		case PS_STATE_MAX:
			/*
			 * You can't call a signal handler for threads in these
			 * states.
			 */
			suppress_handler = 1;
			break;

		/*
		 * States which do not need any cleanup handling when signals
		 * occur:
		 */
		case PS_RUNNING:
			/*
			 * Remove the thread from the queue before changing its
			 * priority:
			 */
			if ((pthread->flags & THR_FLAGS_IN_RUNQ) != 0)
				THR_RUNQ_REMOVE(pthread);
			break;

		/*
		 * States which cannot be interrupted but still require the
		 * signal handler to run:
		 */
		case PS_COND_WAIT:
		case PS_MUTEX_WAIT:
			/*
			 * Remove the thread from the wait queue.  It will
			 * be added back to the wait queue once all signal
			 * handlers have been invoked.
			 */
			KSE_WAITQ_REMOVE(pthread->kse, pthread);
			break;

		case PS_SLEEP_WAIT:
			/*
			 * Unmasked signals always cause sleep to terminate
			 * early regardless of SA_RESTART:
			 */
			pthread->interrupted = 1;
			KSE_WAITQ_REMOVE(pthread->kse, pthread);
			THR_SET_STATE(pthread, PS_RUNNING);
			break;

		case PS_JOIN:
		case PS_SIGSUSPEND:
			KSE_WAITQ_REMOVE(pthread->kse, pthread);
			THR_SET_STATE(pthread, PS_RUNNING);
			break;

		case PS_SIGWAIT:
			/*
			 * The signal handler is not called for threads in
			 * SIGWAIT.
			 */
			suppress_handler = 1;
			/* Wake up the thread if the signal is blocked. */
			if (sigismember(pthread->data.sigwait, sig)) {
				/* Return the signal number: */
				pthread->signo = sig;

				/* Make the thread runnable: */
				_thr_setrunnable_unlocked(pthread);
			} else
				/* Increment the pending signal count. */
				sigaddset(&pthread->sigpend, sig);
			break;
		}

		if (suppress_handler == 0) {
			/*
			 * Setup a signal frame and save the current threads
			 * state:
			 */
			thr_sigframe_add(pthread, sig, info);

			if (pthread->state != PS_RUNNING)
				THR_SET_STATE(pthread, PS_RUNNING);

			/*
			 * The thread should be removed from all scheduling
			 * queues at this point.  Raise the priority and
			 * place the thread in the run queue.  It is also
			 * possible for a signal to be sent to a suspended
			 * thread, mostly via pthread_kill().  If a thread
			 * is suspended, don't insert it into the priority
			 * queue; just set its state to suspended and it
			 * will run the signal handler when it is resumed.
			 */
			pthread->active_priority |= THR_SIGNAL_PRIORITY;
			if ((pthread->flags & THR_FLAGS_IN_RUNQ) == 0)
				THR_RUNQ_INSERT_TAIL(pthread);
		}
	}
}

static void
thr_sig_check_state(struct pthread *pthread, int sig)
{
	/*
	 * Process according to thread state:
	 */
	switch (pthread->state) {
	/*
	 * States which do not change when a signal is trapped:
	 */
	case PS_RUNNING:
	case PS_LOCKWAIT:
	case PS_MUTEX_WAIT:
	case PS_COND_WAIT:
	case PS_JOIN:
	case PS_SUSPENDED:
	case PS_DEAD:
	case PS_DEADLOCK:
	case PS_STATE_MAX:
		break;

	case PS_SIGWAIT:
		/* Wake up the thread if the signal is blocked. */
		if (sigismember(pthread->data.sigwait, sig)) {
			/* Return the signal number: */
			pthread->signo = sig;

			/* Change the state of the thread to run: */
			_thr_setrunnable_unlocked(pthread);
		} else
			/* Increment the pending signal count. */
			sigaddset(&pthread->sigpend, sig);
		break;

	case PS_SIGSUSPEND:
	case PS_SLEEP_WAIT:
		/*
		 * Remove the thread from the wait queue and make it
		 * runnable:
		 */
		_thr_setrunnable_unlocked(pthread);

		/* Flag the operation as interrupted: */
		pthread->interrupted = 1;
		break;
	}
}

/*
 * Send a signal to a specific thread (ala pthread_kill):
 */
void
_thr_sig_send(struct pthread *pthread, int sig)
{
	struct pthread *curthread = _get_curthread();

	/* Lock the scheduling queue of the target thread. */
	THR_SCHED_LOCK(curthread, pthread);

	/* Check for signals whose actions are SIG_DFL: */
	if (_thread_sigact[sig - 1].sa_handler == SIG_DFL) {
		/*
		 * Check to see if a temporary signal handler is
		 * installed for sigwaiters:
		 */
		if (_thread_dfl_count[sig] == 0) {
			/*
			 * Deliver the signal to the process if a handler
			 * is not installed:
			 */
			THR_SCHED_UNLOCK(curthread, pthread);
			kill(getpid(), sig);
			THR_SCHED_LOCK(curthread, pthread);
		}
		/*
		 * Assuming we're still running after the above kill(),
		 * make any necessary state changes to the thread:
		 */
		thr_sig_check_state(pthread, sig);
		THR_SCHED_UNLOCK(curthread, pthread);
	}
	/*
	 * Check that the signal is not being ignored:
	 */
	else if (_thread_sigact[sig - 1].sa_handler != SIG_IGN) {
		if (pthread->state == PS_SIGWAIT &&
		    sigismember(pthread->data.sigwait, sig)) {
			/* Return the signal number: */
			pthread->signo = sig;

			/* Change the state of the thread to run: */
			_thr_setrunnable_unlocked(pthread);
			THR_SCHED_UNLOCK(curthread, pthread);
		} else if (sigismember(&pthread->tmbx.tm_context.uc_sigmask, sig)) {
			/* Add the signal to the pending set: */
			sigaddset(&pthread->sigpend, sig);
			THR_SCHED_UNLOCK(curthread, pthread);
		} else if (pthread == curthread) {
			ucontext_t uc;
			siginfo_t info;
			volatile int once;

			THR_SCHED_UNLOCK(curthread, pthread);
			build_siginfo(&info, sig);
			once = 0;
			THR_GETCONTEXT(&uc);
			if (once == 0) {
				once = 1;
				/*
				 * Call the signal handler for the current
				 * thread:
				 */
				thr_sig_invoke_handler(curthread, sig,
				    &info, &uc);
			}
		} else {
			/*
			 * Perform any state changes due to signal
			 * arrival:
			 */
			_thr_sig_add(pthread, sig, NULL);
			THR_SCHED_UNLOCK(curthread, pthread);
		}
	}
}

static void
thr_sigframe_add(struct pthread *thread, int sig, siginfo_t *info)
{
	if (thread->curframe == NULL)
		PANIC("Thread doesn't have signal frame ");

	if (thread->have_signals == 0) {
		/*
		 * Multiple signals can be added to the same signal
		 * frame.  Only save the thread's state the first time.
		 */
		thr_sigframe_save(thread, thread->curframe);
		thread->have_signals = 1;
		thread->flags &= THR_FLAGS_PRIVATE;
	}
	sigaddset(&thread->curframe->psf_sigset, sig);
	if (info == NULL)
		build_siginfo(&thread->siginfo[sig], sig);
	else if (info != &thread->siginfo[sig])
		memcpy(&thread->siginfo[sig], info, sizeof(*info));

	/* Setup the new signal mask. */
	SIGSETOR(thread->tmbx.tm_context.uc_sigmask,
	    _thread_sigact[sig - 1].sa_mask);
	sigaddset(&thread->tmbx.tm_context.uc_sigmask, sig);
}

void
thr_sigframe_restore(struct pthread *thread, struct pthread_sigframe *psf)
{
	thread->flags = psf->psf_flags;
	thread->interrupted = psf->psf_interrupted;
	thread->signo = psf->psf_signo;
	thread->state = psf->psf_state;
	thread->data = psf->psf_wait_data;
	thread->wakeup_time = psf->psf_wakeup_time;
	if (thread->sigmask_seqno == psf->psf_seqno)
		thread->tmbx.tm_context.uc_sigmask = psf->psf_sigmask;
	else
		thread->tmbx.tm_context.uc_sigmask = thread->sigmask;
}

static void
thr_sigframe_save(struct pthread *thread, struct pthread_sigframe *psf)
{
	/* This has to initialize all members of the sigframe. */
	psf->psf_flags =
	    thread->flags & (THR_FLAGS_PRIVATE | THR_FLAGS_IN_TDLIST);
	psf->psf_interrupted = thread->interrupted;
	psf->psf_signo = thread->signo;
	psf->psf_state = thread->state;
	psf->psf_wait_data = thread->data;
	psf->psf_wakeup_time = thread->wakeup_time;
	psf->psf_sigmask = thread->tmbx.tm_context.uc_sigmask;
	psf->psf_seqno = thread->sigmask_seqno;
	sigemptyset(&psf->psf_sigset);
}
