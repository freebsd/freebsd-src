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
static void	thr_sig_check_state(struct pthread *pthread, int sig);
static struct pthread *thr_sig_find(struct kse *curkse, int sig,
		    siginfo_t *info);
static void	handle_special_signals(struct kse *curkse, int sig);
static void	thr_sigframe_add(struct pthread *thread);
static void	thr_sigframe_restore(struct pthread *thread,
		    struct pthread_sigframe *psf);
static void	thr_sigframe_save(struct pthread *thread,
		    struct pthread_sigframe *psf);

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

static void *
sig_daemon(void *arg /* Unused */)
{
	int i;
	kse_critical_t crit;
	struct timespec ts;
	sigset_t set;
	struct kse *curkse;
	struct pthread *curthread = _get_curthread();

	DBG_MSG("signal daemon started\n");
	
	curthread->name = strdup("signal thread");
	crit = _kse_critical_enter();
	curkse = _get_curkse();
	SIGFILLSET(set);
	__sys_sigprocmask(SIG_SETMASK, &set, NULL);
	__sys_sigpending(&set);
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	while (1) {
		KSE_LOCK_ACQUIRE(curkse, &_thread_signal_lock);
		_thr_proc_sigpending = set;
		KSE_LOCK_RELEASE(curkse, &_thread_signal_lock);
		for (i = 1; i <= _SIG_MAXSIG; i++) {
			if (SIGISMEMBER(set, i) != 0)
				_thr_sig_dispatch(curkse, i,
				    NULL /* no siginfo */);
		}
		ts.tv_sec = 30;
		ts.tv_nsec = 0;
		curkse->k_mbx.km_flags =
		    KMF_NOUPCALL | KMF_NOCOMPLETED | KMF_WAITSIGEVENT;
		kse_release(&ts);
		curkse->k_mbx.km_flags = 0;
		set = curkse->k_mbx.km_sigscaught;
	}
	return (0);
}

/* Utility function to create signal daemon thread */
int
_thr_start_sig_daemon(void)
{
	pthread_attr_t attr;
	sigset_t sigset, oldset;
	
	SIGFILLSET(sigset);
	pthread_sigmask(SIG_SETMASK, &sigset, &oldset);
	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	/* sigmask will be inherited */
	if (pthread_create(&_thr_sig_daemon, &attr, sig_daemon, NULL))
		PANIC("can not create signal daemon thread!\n");
	pthread_attr_destroy(&attr);
	pthread_sigmask(SIG_SETMASK, &oldset, NULL);
	return (0);
}

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
		} else if (SIGISMEMBER(thread->sigmask, sig)) {
			KSE_SCHED_UNLOCK(curkse, thread->kseg);
			_thr_ref_delete(NULL, thread);
		} else {
			_thr_sig_add(thread, sig, info);
			KSE_SCHED_UNLOCK(curkse, thread->kseg);
			_thr_ref_delete(NULL, thread);
			break;
		}
	}
	DBG_MSG("<<< _thr_sig_dispatch\n");
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
		if (((__sighandler_t *)sigfunc != SIG_DFL) &&
		    ((__sighandler_t *)sigfunc != SIG_IGN) &&
		    (sigfunc != (__siginfohandler_t *)_thr_sig_handler)) {
			if (((_thread_sigact[sig - 1].sa_flags & SA_SIGINFO)
			    != 0) || (info == NULL))
				(*(sigfunc))(sig, info, ucp);
			else
				(*(sigfunc))(sig,
				    (siginfo_t*)(intptr_t)info->si_code, ucp);
		}
	}
	else {
		/* Nothing. */
		DBG_MSG("Got signal %d\n", sig);
		/* XXX Bound thread will fall into this... */
	}
}

/* Must be called with signal lock and schedule lock held in order */
static void
thr_sig_invoke_handler(struct pthread *curthread, int sig, siginfo_t *info,
    ucontext_t *ucp)
{
	void (*sigfunc)(int, siginfo_t *, void *);
	sigset_t sigmask;
	int sa_flags;
	struct sigaction act;
	struct kse *curkse;

	/*
	 * Invoke the signal handler without going through the scheduler:
	 */
	DBG_MSG("Got signal %d, calling handler for current thread %p\n",
	    sig, curthread);

	if (!_kse_in_critical())
		PANIC("thr_sig_invoke_handler without in critical\n");
	curkse = _get_curkse();
	/*
	 * Check that a custom handler is installed and if
	 * the signal is not blocked:
	 */
	sigfunc = _thread_sigact[sig - 1].sa_sigaction;
	sa_flags = _thread_sigact[sig - 1].sa_flags & SA_SIGINFO;
	sigmask = curthread->sigmask;
	SIGSETOR(curthread->sigmask, _thread_sigact[sig - 1].sa_mask);
	if (!(sa_flags & (SA_NODEFER | SA_RESETHAND)))
		SIGADDSET(curthread->sigmask, sig);
	if ((sig != SIGILL) && (sa_flags & SA_RESETHAND)) {
		if (_thread_dfl_count[sig - 1] == 0) {
			act.sa_handler = SIG_DFL;
			act.sa_flags = SA_RESTART;
			SIGEMPTYSET(act.sa_mask);
			__sys_sigaction(sig, &act, NULL);
			__sys_sigaction(sig, NULL, &_thread_sigact[sig - 1]);
		}
	}
	KSE_LOCK_RELEASE(curkse, &_thread_signal_lock);
	KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
	_kse_critical_leave(&curthread->tmbx);
	ucp->uc_sigmask = sigmask;

	if (((__sighandler_t *)sigfunc != SIG_DFL) &&
	    ((__sighandler_t *)sigfunc != SIG_IGN)) {
		if ((sa_flags & SA_SIGINFO) != 0 || info == NULL)
			(*(sigfunc))(sig, info, ucp);
		else
			(*(sigfunc))(sig, (siginfo_t*)(intptr_t)info->si_code,
			    ucp);
	} else {
		/* XXX
		 * TODO: exit process if signal would kill it. 
		 */
#ifdef NOTYET
			if (sigprop(sig) & SA_KILL)
				kse_sigexit(sig);
#endif
	}
	_kse_critical_enter();
	/* Don't trust after critical leave/enter */
	curkse = _get_curkse();
	KSE_SCHED_LOCK(curkse, curkse->k_kseg);
	KSE_LOCK_ACQUIRE(curkse, &_thread_signal_lock);
	/*
	 * Restore the thread's signal mask.
	 */
	curthread->sigmask = ucp->uc_sigmask;

	DBG_MSG("Got signal %d, handler returned %p\n", sig, curthread);
}

int
_thr_getprocsig(int sig, siginfo_t *siginfo)
{
	kse_critical_t crit;
	struct kse *curkse;
	int ret;

	DBG_MSG(">>> _thr_getprocsig\n");

	crit = _kse_critical_enter();
	curkse = _get_curkse();
	KSE_LOCK_ACQUIRE(curkse, &_thread_signal_lock);
	ret = _thr_getprocsig_unlocked(sig, siginfo);
	KSE_LOCK_RELEASE(curkse, &_thread_signal_lock);
	_kse_critical_leave(crit);

	DBG_MSG("<<< _thr_getprocsig\n");
	return (ret);
}

int
_thr_getprocsig_unlocked(int sig, siginfo_t *siginfo)
{
	sigset_t sigset;
	struct timespec ts;

	/* try to retrieve signal from kernel */
	SIGEMPTYSET(sigset);
	SIGADDSET(sigset, sig);
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	if (__sys_sigtimedwait(&sigset, siginfo, &ts) > 0) {
		SIGDELSET(_thr_proc_sigpending, sig);
		return (sig);
	}
	return (0);
}

/*
 * Find a thread that can handle the signal.  This must be called
 * with upcalls disabled.
 */
struct pthread *
thr_sig_find(struct kse *curkse, int sig, siginfo_t *info)
{
	struct pthread	*pthread;
	struct pthread	*suspended_thread, *signaled_thread;
	siginfo_t si;

	DBG_MSG("Looking for thread to handle signal %d\n", sig);

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
		if (pthread == _thr_sig_daemon)
			continue;
#ifdef NOTYET
		/* Signal delivering to bound thread is done by kernel */
		if (pthread->attr.flags & PTHREAD_SCOPE_SYSTEM)
			continue;
#endif

		/* Take the scheduling lock. */
		KSE_SCHED_LOCK(curkse, pthread->kseg);
		if ((pthread->state == PS_DEAD)		||
		    (pthread->state == PS_DEADLOCK)	||
		    THR_IS_EXITING(pthread)		||
		    THR_IS_SUSPENDED(pthread)		||
		    SIGISMEMBER(pthread->sigmask, sig)) {
			; /* Skip this thread. */
		} else if (pthread->state == PS_SIGWAIT) {
			/*
			 * retrieve signal from kernel, if it is job control
			 * signal, and sigaction is SIG_DFL, then we will
			 * be stopped in kernel, we hold lock here, but that 
			 * does not matter, because that's job control, and
			 * whole process should be stopped.
			 */
			if (_thr_getprocsig(sig, &si)) {
				DBG_MSG("Waking thread %p in sigwait"
					" with signal %d\n", pthread, sig);
				/*  where to put siginfo ? */
				*(pthread->data.sigwaitinfo) = si;
				pthread->sigmask = pthread->oldsigmask;
				_thr_setrunnable_unlocked(pthread);
			}
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
			return (NULL);
		} else  {
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

	if (suspended_thread != NULL) {
		pthread = suspended_thread;
		if (signaled_thread)
			_thr_ref_delete(NULL, signaled_thread);
	} else if (signaled_thread) {
		pthread = signaled_thread;
	} else {
		pthread = NULL;
	}
	return (pthread);
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
	siginfo_t siginfo;
	int i;
	kse_critical_t crit;
	struct kse *curkse;

	DBG_MSG(">>> thr_sig_rundown %p\n", curthread);
	/* Check the threads previous state: */
	if ((psf != NULL) && (psf->psf_valid != 0)) {
		/*
		 * Do a little cleanup handling for those threads in
		 * queues before calling the signal handler.  Signals
		 * for these threads are temporarily blocked until
		 * after cleanup handling.
		 */
		switch (psf->psf_state) {
		case PS_COND_WAIT:
			_cond_wait_backout(curthread);
			psf->psf_state = PS_RUNNING;
			break;
	
		case PS_MUTEX_WAIT:
			_mutex_lock_backout(curthread);
			psf->psf_state = PS_RUNNING;
			break;
	
		case PS_RUNNING:
			break;

		default:
			psf->psf_state = PS_RUNNING;
			break;
		}
		/* XXX see comment in thr_sched_switch_unlocked */
		curthread->critical_count--;
	}

	/*
	 * Lower the priority before calling the handler in case
	 * it never returns (longjmps back):
	 */
	crit = _kse_critical_enter();
	curkse = _get_curkse();
	KSE_SCHED_LOCK(curkse, curkse->k_kseg);
	KSE_LOCK_ACQUIRE(curkse, &_thread_signal_lock);
	curthread->active_priority &= ~THR_SIGNAL_PRIORITY;

	while (1) {
		for (i = 1; i <= _SIG_MAXSIG; i++) {
			if (SIGISMEMBER(curthread->sigmask, i))
				continue;
			if (SIGISMEMBER(curthread->sigpend, i)) {
				SIGDELSET(curthread->sigpend, i);
				siginfo = curthread->siginfo[i-1];
				break;
			}
			if (SIGISMEMBER(_thr_proc_sigpending, i)) {
				if (_thr_getprocsig_unlocked(i, &siginfo))
					break;
			}
		}
		if (i <= _SIG_MAXSIG)
			thr_sig_invoke_handler(curthread, i, &siginfo, ucp);
		else
			break;
	}

	if (psf != NULL && psf->psf_valid != 0)
		thr_sigframe_restore(curthread, psf);
	curkse = _get_curkse();
	KSE_LOCK_RELEASE(curkse, &_thread_signal_lock);
	KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
	_kse_critical_leave(&curthread->tmbx);

	DBG_MSG("<<< thr_sig_rundown %p\n", curthread);
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
	ucontext_t uc;
	volatile int once;

	if (THR_IN_CRITICAL(curthread))
		return;

	once = 0;
	THR_GETCONTEXT(&uc);
	if (once == 0) {
		once = 1;
		curthread->check_pending = 0;
		_thr_sig_rundown(curthread, &uc, NULL);
	}
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
		SIGDELSET(_thr_proc_sigpending, SIGCONT);
		KSE_LOCK_RELEASE(curkse, &_thread_signal_lock);
		break;
	case SIGCONT:
		KSE_LOCK_ACQUIRE(curkse, &_thread_signal_lock);
		SIGDELSET(_thr_proc_sigpending, SIGTSTP);
		SIGDELSET(_thr_proc_sigpending, SIGTTIN);
		SIGDELSET(_thr_proc_sigpending, SIGTTOU);
		KSE_LOCK_RELEASE(curkse, &_thread_signal_lock);
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
	int	fromproc = 0;
	struct  pthread *curthread = _get_curthread();
	struct	kse *curkse;
	siginfo_t siginfo;

	DBG_MSG(">>> _thr_sig_add\n");

	curkse = _get_curkse();
	restart = _thread_sigact[sig - 1].sa_flags & SA_RESTART;
	fromproc = (curthread == _thr_sig_daemon);

	if (pthread->state == PS_DEAD || pthread->state == PS_DEADLOCK ||
	    pthread->state == PS_STATE_MAX)
	    	return; /* return false */

#ifdef NOTYET
	if ((pthread->attrs.flags & PTHREAD_SCOPE_SYSTEM) != 0) {
		if (!fromproc)
			kse_thr_interrupt(&pthread->tmbx, 0, sig);
		return;
	}
#endif

	if (pthread->curframe == NULL ||
	    SIGISMEMBER(pthread->sigmask, sig) ||
	    THR_IN_CRITICAL(pthread)) {
		/* thread is running or signal was being masked */
		if (!fromproc) {
			SIGADDSET(pthread->sigpend, sig);
			if (info == NULL)
				build_siginfo(&pthread->siginfo[sig-1], sig);
			else if (info != &pthread->siginfo[sig-1])
				memcpy(&pthread->siginfo[sig-1], info,
					 sizeof(*info));
		} else {
			if (!_thr_getprocsig(sig, &pthread->siginfo[sig-1]))
				return;
			SIGADDSET(pthread->sigpend, sig);
		}
		if (!SIGISMEMBER(pthread->sigmask, sig)) {
			pthread->check_pending = 1;
			if (pthread->blocked != 0 && !THR_IN_CRITICAL(pthread))
				kse_thr_interrupt(&pthread->tmbx,
					 restart ? -2 : -1);
		}
	}
	else {
		/* if process signal not exists, just return */
		if (fromproc) {
			if (!_thr_getprocsig(sig, &siginfo))
				return;
			info = &siginfo;
		}
		/*
		 * Process according to thread state:
		 */
		switch (pthread->state) {
		case PS_DEAD:
		case PS_DEADLOCK:
		case PS_STATE_MAX:
			return;	/* XXX return false */
		case PS_LOCKWAIT:
		case PS_SUSPENDED:
			/*
			 * You can't call a signal handler for threads in these
			 * states.
			 */
			suppress_handler = 1;
			break;
		case PS_RUNNING:
			if ((pthread->flags & THR_FLAGS_IN_RUNQ)) {
				THR_RUNQ_REMOVE(pthread);
				pthread->active_priority |= THR_SIGNAL_PRIORITY;
				THR_RUNQ_INSERT_TAIL(pthread);
			} else {
				/* Possible not in RUNQ and has curframe ? */
				pthread->active_priority |= THR_SIGNAL_PRIORITY;
			}
			suppress_handler = 1;
			break;
		/*
		 * States which cannot be interrupted but still require the
		 * signal handler to run:
		 */
		case PS_COND_WAIT:
		case PS_MUTEX_WAIT:
			break;

		case PS_SLEEP_WAIT:
			/*
			 * Unmasked signals always cause sleep to terminate
			 * early regardless of SA_RESTART:
			 */
			pthread->interrupted = 1;
			break;

		case PS_JOIN:
			break;

		case PS_SIGSUSPEND:
			pthread->interrupted = 1;
			break;

		case PS_SIGWAIT:
			if (info == NULL)
				build_siginfo(&pthread->siginfo[sig-1], sig);
			else if (info != &pthread->siginfo[sig-1])
				memcpy(&pthread->siginfo[sig-1], info,
					sizeof(*info));
			/*
			 * The signal handler is not called for threads in
			 * SIGWAIT.
			 */
			suppress_handler = 1;
			/* Wake up the thread if the signal is not blocked. */
			if (!SIGISMEMBER(pthread->sigmask, sig)) {
				/* Return the signal number: */
				*(pthread->data.sigwaitinfo) = pthread->siginfo[sig-1];
				pthread->sigmask = pthread->oldsigmask;
				/* Make the thread runnable: */
				_thr_setrunnable_unlocked(pthread);
			} else {
				/* Increment the pending signal count. */
				SIGADDSET(pthread->sigpend, sig);
				pthread->check_pending = 1;
			}
		
			return;
		}

		SIGADDSET(pthread->sigpend, sig);
		if (info == NULL)
			build_siginfo(&pthread->siginfo[sig-1], sig);
		else if (info != &pthread->siginfo[sig-1])
			memcpy(&pthread->siginfo[sig-1], info, sizeof(*info));

		if (suppress_handler == 0) {
			/*
			 * Setup a signal frame and save the current threads
			 * state:
			 */
			thr_sigframe_add(pthread);
			if (pthread->flags & THR_FLAGS_IN_RUNQ)
				THR_RUNQ_REMOVE(pthread);
			pthread->active_priority |= THR_SIGNAL_PRIORITY;
			_thr_setrunnable_unlocked(pthread);
		} else {
			pthread->check_pending = 1;
		}
	}

	DBG_MSG("<<< _thr_sig_add\n");
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
		build_siginfo(&pthread->siginfo[sig-1], sig);
		/* Wake up the thread if the signal is blocked. */
		if (!SIGISMEMBER(pthread->sigmask, sig)) {
			/* Return the signal number: */
			*(pthread->data.sigwaitinfo) = pthread->siginfo[sig-1];
			pthread->sigmask = pthread->oldsigmask;
			/* Change the state of the thread to run: */
			_thr_setrunnable_unlocked(pthread);
		} else {
			/* Increment the pending signal count. */
			SIGADDSET(pthread->sigpend, sig);
			pthread->check_pending = 1;
		}
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

#ifdef NOTYET
	if ((pthread->attr.flags & PTHREAD_SCOPE_SYSTEM) == 0) {
		kse_thr_interrupt(&pthread->tmbx, sig);
		return;
	}
#endif
	/* Lock the scheduling queue of the target thread. */
	THR_SCHED_LOCK(curthread, pthread);

	/* Check for signals whose actions are SIG_DFL: */
	if (_thread_sigact[sig - 1].sa_handler == SIG_DFL) {
		/*
		 * Check to see if a temporary signal handler is
		 * installed for sigwaiters:
		 */
		if (_thread_dfl_count[sig - 1] == 0) {
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
		_thr_sig_add(pthread, sig, NULL);
		THR_SCHED_UNLOCK(curthread, pthread);
		/* XXX
		 * If thread sent signal to itself, check signals now.
		 * It is not really needed, _kse_critical_leave should
		 * have already checked signals.
		 */
		if (pthread == curthread && curthread->check_pending)
			_thr_sig_check_pending(curthread);
	} else  {
		THR_SCHED_UNLOCK(curthread, pthread);
	}
}

static void
thr_sigframe_add(struct pthread *thread)
{
	if (thread->curframe == NULL)
		PANIC("Thread doesn't have signal frame ");

	if (thread->curframe->psf_valid == 0) {
		thread->curframe->psf_valid = 1;
		/*
		 * Multiple signals can be added to the same signal
		 * frame.  Only save the thread's state the first time.
		 */
		thr_sigframe_save(thread, thread->curframe);
	}
}

static void
thr_sigframe_restore(struct pthread *thread, struct pthread_sigframe *psf)
{
	if (psf->psf_valid == 0)
		PANIC("invalid pthread_sigframe\n");
	thread->flags = psf->psf_flags;
	thread->interrupted = psf->psf_interrupted;
	thread->signo = psf->psf_signo;
	thread->state = psf->psf_state;
	thread->data = psf->psf_wait_data;
	thread->wakeup_time = psf->psf_wakeup_time;
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
}

void
_thr_signal_init(void)
{
	sigset_t sigset;
	struct sigaction act;
	int i;

	SIGFILLSET(sigset);
	__sys_sigprocmask(SIG_SETMASK, &sigset, &_thr_initial->sigmask);
	/* Enter a loop to get the existing signal status: */
	for (i = 1; i <= _SIG_MAXSIG; i++) {
		/* Check for signals which cannot be trapped: */
		if (i == SIGKILL || i == SIGSTOP) {
		}

		/* Get the signal handler details: */
		else if (__sys_sigaction(i, NULL,
			    &_thread_sigact[i - 1]) != 0) {
			/*
			 * Abort this process if signal
			 * initialisation fails:
			 */
			PANIC("Cannot read signal handler info");
		}
	}
	/*
	 * Install the signal handler for SIGINFO.  It isn't
	 * really needed, but it is nice to have for debugging
	 * purposes.
	 */
	_thread_sigact[SIGINFO - 1].sa_flags = SA_SIGINFO | SA_RESTART;
	SIGEMPTYSET(act.sa_mask);
	act.sa_flags = SA_SIGINFO | SA_RESTART;
	act.sa_sigaction = (__siginfohandler_t *)&_thr_sig_handler;
	if (__sys_sigaction(SIGINFO, &act, NULL) != 0) {
		/*
		 * Abort this process if signal initialisation fails:
		 */
		PANIC("Cannot initialize signal handler");
	}
}

void
_thr_signal_deinit(void)
{
	sigset_t tmpmask, oldmask;
	int i;

	SIGFILLSET(tmpmask);
	SIG_CANTMASK(tmpmask);
	__sys_sigprocmask(SIG_SETMASK, &tmpmask, &oldmask);
	/* Enter a loop to get the existing signal status: */
	for (i = 1; i <= _SIG_MAXSIG; i++) {
		/* Check for signals which cannot be trapped: */
		if (i == SIGKILL || i == SIGSTOP) {
		}

		/* Set the signal handler details: */
		else if (__sys_sigaction(i, &_thread_sigact[i - 1], NULL) != 0) {
			/*
			 * Abort this process if signal
			 * initialisation fails:
			 */
			PANIC("Cannot set signal handler info");
		}
	}
	__sys_sigprocmask(SIG_SETMASK, &oldmask, NULL);
}

