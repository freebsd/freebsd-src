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

/* Prototypes: */
static void	build_siginfo(siginfo_t *info, int signo);
#ifndef SYSTEM_SCOPE_ONLY
static struct pthread *thr_sig_find(struct kse *curkse, int sig,
		    siginfo_t *info);
static void	handle_special_signals(struct kse *curkse, int sig);
#endif
static void	thr_sigframe_add(struct pthread *thread);
static void	thr_sigframe_restore(struct pthread *thread,
		    struct pthread_sigframe *psf);
static void	thr_sigframe_save(struct pthread *thread,
		    struct pthread_sigframe *psf);

#define SA_KILL         0x01            /* terminates process by default */
#define	SA_STOP		0x02
#define	SA_CONT		0x04

static int sigproptbl[NSIG] = {
	SA_KILL,	/* SIGHUP */
	SA_KILL,	/* SIGINT */
	SA_KILL,	/* SIGQUIT */
	SA_KILL,	/* SIGILL */
	SA_KILL,	/* SIGTRAP */
	SA_KILL,	/* SIGABRT */
	SA_KILL,	/* SIGEMT */
	SA_KILL,	/* SIGFPE */
	SA_KILL,	/* SIGKILL */
	SA_KILL,	/* SIGBUS */
	SA_KILL,	/* SIGSEGV */
	SA_KILL,	/* SIGSYS */
	SA_KILL,	/* SIGPIPE */
	SA_KILL,	/* SIGALRM */
	SA_KILL,	/* SIGTERM */
	0,		/* SIGURG */
	SA_STOP,	/* SIGSTOP */
	SA_STOP,	/* SIGTSTP */
	SA_CONT,	/* SIGCONT */
	0,		/* SIGCHLD */
	SA_STOP,	/* SIGTTIN */
	SA_STOP,	/* SIGTTOU */
	0,		/* SIGIO */
	SA_KILL,	/* SIGXCPU */
	SA_KILL,	/* SIGXFSZ */
	SA_KILL,	/* SIGVTALRM */
	SA_KILL,	/* SIGPROF */
	0,		/* SIGWINCH  */
	0,		/* SIGINFO */
	SA_KILL,	/* SIGUSR1 */
	SA_KILL		/* SIGUSR2 */
};

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

#ifndef SYSTEM_SCOPE_ONLY

static void *
sig_daemon(void *arg /* Unused */)
{
	int i;
	kse_critical_t crit;
	struct timespec ts;
	sigset_t set;
	struct kse *curkse;
	struct pthread *curthread = _get_curthread();

	DBG_MSG("signal daemon started(%p)\n", curthread);
	
	curthread->name = strdup("signal thread");
	crit = _kse_critical_enter();
	curkse = _get_curkse();

	/*
	 * Daemon thread is a bound thread and we must be created with
	 * all signals masked
	 */
#if 0	
	SIGFILLSET(set);
	__sys_sigprocmask(SIG_SETMASK, &set, NULL);
#endif	
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
		curkse->k_kcb->kcb_kmbx.km_flags =
		    KMF_NOUPCALL | KMF_NOCOMPLETED | KMF_WAITSIGEVENT;
		kse_release(&ts);
		curkse->k_kcb->kcb_kmbx.km_flags = 0;
		set = curkse->k_kcb->kcb_kmbx.km_sigscaught;
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
	struct kse_mailbox *kmbx;
	struct pthread *thread;

	DBG_MSG(">>> _thr_sig_dispatch(%d)\n", sig);

	/* Some signals need special handling: */
	handle_special_signals(curkse, sig);

	/* Check if the signal requires a dump of thread information: */
	if (sig == SIGINFO) {
		/* Dump thread information to file: */
		_thread_dump_info();
	}

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
			kmbx = _thr_sig_add(thread, sig, info);
			KSE_SCHED_UNLOCK(curkse, thread->kseg);
			_thr_ref_delete(NULL, thread);
			if (kmbx != NULL)
				kse_wakeup(kmbx);
			break;
		}
	}
	DBG_MSG("<<< _thr_sig_dispatch\n");
}

#endif /* ! SYSTEM_SCOPE_ONLY */

static __inline int
sigprop(int sig)
{

	if (sig > 0 && sig < NSIG)
                return (sigproptbl[_SIG_IDX(sig)]);
        return (0);
}

void
_thr_sig_handler(int sig, siginfo_t *info, ucontext_t *ucp)
{
	__siginfohandler_t *sigfunc;
	struct pthread *curthread;
	struct kse *curkse;
	struct sigaction act;
	int sa_flags, err_save, intr_save, timeout_save;

	DBG_MSG(">>> _thr_sig_handler(%d)\n", sig);

	curthread = _get_curthread();
	if (curthread == NULL)
		PANIC("No current thread.\n");
	if (!(curthread->attr.flags & PTHREAD_SCOPE_SYSTEM))
		PANIC("Thread is not system scope.\n");
	if (curthread->flags & THR_FLAGS_EXITING)
		return;
	curkse = _get_curkse();
	/*
	 * If thread is in critical region or if thread is on
	 * the way of state transition, then latch signal into buffer.
	 */
	if (_kse_in_critical() || THR_IN_CRITICAL(curthread) ||
	    (curthread->state != PS_RUNNING && curthread->curframe == NULL)) {
		DBG_MSG(">>> _thr_sig_handler(%d) in critical\n", sig);
		curthread->siginfo[sig-1] = *info;
		curthread->check_pending = 1;
		curkse->k_sigseqno++;
		SIGADDSET(curthread->sigpend, sig);
		/* 
		 * If the kse is on the way to idle itself, but
		 * we have signal ready, we should prevent it
		 * to sleep, kernel will latch the wakeup request,
		 * so kse_release will return from kernel immediately.
		 */
		if (KSE_IS_IDLE(curkse))
			kse_wakeup(&curkse->k_kcb->kcb_kmbx);
		return;
	}

	/* It is now safe to invoke signal handler */
	err_save = errno;
	timeout_save = curthread->timeout;
	intr_save = curthread->interrupted;
	/* Get a fresh copy of signal mask, for thread dump only */
	curthread->sigmask = ucp->uc_sigmask;
	_kse_critical_enter();
	KSE_LOCK_ACQUIRE(curkse, &_thread_signal_lock);
	sigfunc = _thread_sigact[sig - 1].sa_sigaction;
	sa_flags = _thread_sigact[sig - 1].sa_flags & SA_SIGINFO;
	if (sa_flags & SA_RESETHAND) {
		act.sa_handler = SIG_DFL;
		act.sa_flags = SA_RESTART;
		SIGEMPTYSET(act.sa_mask);
		__sys_sigaction(sig, &act, NULL);
		__sys_sigaction(sig, NULL, &_thread_sigact[sig - 1]);
	}
	KSE_LOCK_RELEASE(curkse, &_thread_signal_lock);
	_kse_critical_leave(&curthread->tcb->tcb_tmbx);

	/* Now invoke real handler */
	if (((__sighandler_t *)sigfunc != SIG_DFL) &&
	    ((__sighandler_t *)sigfunc != SIG_IGN) && 
	    (sigfunc != (__siginfohandler_t *)_thr_sig_handler)) {
		if ((sa_flags & SA_SIGINFO) != 0 || info == NULL)
			(*(sigfunc))(sig, info, ucp);
		else
			(*(sigfunc))(sig, (siginfo_t*)(intptr_t)info->si_code,
				 ucp);
	} else {
		if ((__sighandler_t *)sigfunc == SIG_DFL) {
			if (sigprop(sig) & SA_KILL) {
				if (_kse_isthreaded())
					kse_thr_interrupt(NULL,
						 KSE_INTR_SIGEXIT, sig);
				else
					kill(getpid(), sig);
			}
#ifdef NOTYET
			else if (sigprop(sig) & SA_STOP)
				kse_thr_interrupt(NULL, KSE_INTR_JOBSTOP, sig);
#endif
		}
	}
	errno = err_save;
	curthread->timeout = timeout_save;
	curthread->interrupted = intr_save;
	_kse_critical_enter();
	curthread->sigmask = ucp->uc_sigmask;
	_kse_critical_leave(&curthread->tcb->tcb_tmbx);
	DBG_MSG("<<< _thr_sig_handler(%d)\n", sig);
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
		act.sa_handler = SIG_DFL;
		act.sa_flags = SA_RESTART;
		SIGEMPTYSET(act.sa_mask);
		__sys_sigaction(sig, &act, NULL);
		__sys_sigaction(sig, NULL, &_thread_sigact[sig - 1]);
	}
	KSE_LOCK_RELEASE(curkse, &_thread_signal_lock);
	KSE_SCHED_UNLOCK(curkse, curkse->k_kseg);
	/*
	 * We are processing buffered signals, synchronize working
	 * signal mask into kernel.
	 */
	if (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM)
		__sys_sigprocmask(SIG_SETMASK, &curthread->sigmask, NULL);
	_kse_critical_leave(&curthread->tcb->tcb_tmbx);
	ucp->uc_sigmask = sigmask;
	if (((__sighandler_t *)sigfunc != SIG_DFL) &&
	    ((__sighandler_t *)sigfunc != SIG_IGN)) {
		if ((sa_flags & SA_SIGINFO) != 0 || info == NULL)
			(*(sigfunc))(sig, info, ucp);
		else
			(*(sigfunc))(sig, (siginfo_t*)(intptr_t)info->si_code,
			    ucp);
	} else {
		if ((__sighandler_t *)sigfunc == SIG_DFL) {
			if (sigprop(sig) & SA_KILL) {
				if (_kse_isthreaded())
					kse_thr_interrupt(NULL,
						 KSE_INTR_SIGEXIT, sig);
				else
					kill(getpid(), sig);
			}
#ifdef NOTYET
			else if (sigprop(sig) & SA_STOP)
				kse_thr_interrupt(NULL, KSE_INTR_JOBSTOP, sig);
#endif
		}
	}

	_kse_critical_enter();
	/* Don't trust after critical leave/enter */
	curkse = _get_curkse();

	/*
	 * Restore the thread's signal mask.
	 */
	curthread->sigmask = ucp->uc_sigmask;
	if (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM)
		__sys_sigprocmask(SIG_SETMASK, &ucp->uc_sigmask, NULL);
	KSE_SCHED_LOCK(curkse, curkse->k_kseg);
	KSE_LOCK_ACQUIRE(curkse, &_thread_signal_lock);
	
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
	SIGDELSET(_thr_proc_sigpending, sig);
	if (__sys_sigtimedwait(&sigset, siginfo, &ts) > 0)
		return (sig);
	return (0);
}

#ifndef SYSTEM_SCOPE_ONLY
/*
 * Find a thread that can handle the signal.  This must be called
 * with upcalls disabled.
 */
struct pthread *
thr_sig_find(struct kse *curkse, int sig, siginfo_t *info)
{
	struct kse_mailbox *kmbx = NULL;
	struct pthread	*pthread;
	struct pthread	*suspended_thread, *signaled_thread;
	__siginfohandler_t *sigfunc;
	siginfo_t si;

	DBG_MSG("Looking for thread to handle signal %d\n", sig);

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
		/* Signal delivering to bound thread is done by kernel */
		if (pthread->attr.flags & PTHREAD_SCOPE_SYSTEM)
			continue;
		/* Take the scheduling lock. */
		KSE_SCHED_LOCK(curkse, pthread->kseg);
		if ((pthread->state == PS_DEAD)		||
		    (pthread->state == PS_DEADLOCK)	||
		    THR_IS_EXITING(pthread)		||
		    THR_IS_SUSPENDED(pthread)) {
			; /* Skip this thread. */
		} else if (pthread->state == PS_SIGWAIT &&
			   SIGISMEMBER(*(pthread->data.sigwait->waitset), sig)) {
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
				*(pthread->data.sigwait->siginfo) = si;
				kmbx = _thr_setrunnable_unlocked(pthread);
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
			if (kmbx != NULL)
				kse_wakeup(kmbx);
			return (NULL);
		} else if (!SIGISMEMBER(pthread->sigmask, sig)) {
			sigfunc = _thread_sigact[sig - 1].sa_sigaction;
			if ((__sighandler_t *)sigfunc == SIG_DFL) {
				if (sigprop(sig) & SA_KILL) {
					kse_thr_interrupt(NULL,
						 KSE_INTR_SIGEXIT, sig);
					/* Never reach */
				}
			}
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
#endif /* ! SYSTEM_SCOPE_ONLY */

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
	int interrupted = curthread->interrupted;
	int timeout = curthread->timeout;
	siginfo_t siginfo;
	int i;
	kse_critical_t crit;
	struct kse *curkse;
	sigset_t sigmask;

	DBG_MSG(">>> thr_sig_rundown (%p)\n", curthread);
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
		/*
		 * For bound thread, we mask all signals and get a fresh
		 * copy of signal mask from kernel
		 */
		if (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM) {
			SIGFILLSET(sigmask);
			__sys_sigprocmask(SIG_SETMASK, &sigmask,
				 &curthread->sigmask);
		}
		for (i = 1; i <= _SIG_MAXSIG; i++) {
			if (SIGISMEMBER(curthread->sigmask, i))
				continue;
			if (SIGISMEMBER(curthread->sigpend, i)) {
				SIGDELSET(curthread->sigpend, i);
				siginfo = curthread->siginfo[i-1];
				break;
			}
			if (!(curthread->attr.flags & PTHREAD_SCOPE_SYSTEM) 
			    && SIGISMEMBER(_thr_proc_sigpending, i)) {
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
	if (curthread->attr.flags & PTHREAD_SCOPE_SYSTEM)
		__sys_sigprocmask(SIG_SETMASK, &curthread->sigmask, NULL);
	_kse_critical_leave(&curthread->tcb->tcb_tmbx);

	curthread->interrupted = interrupted;
	curthread->timeout = timeout;

	DBG_MSG("<<< thr_sig_rundown (%p)\n", curthread);
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

#ifndef SYSTEM_SCOPE_ONLY
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
#endif /* ! SYSTEM_SCOPE_ONLY */

/*
 * Perform thread specific actions in response to a signal.
 * This function is only called if there is a handler installed
 * for the signal, and if the target thread has the signal
 * unmasked.
 *
 * This must be called with the thread's scheduling lock held.
 */
struct kse_mailbox *
_thr_sig_add(struct pthread *pthread, int sig, siginfo_t *info)
{
	siginfo_t siginfo;
	struct kse *curkse;
	struct kse_mailbox *kmbx = NULL;
	struct pthread *curthread = _get_curthread();
	int	restart;
	int	suppress_handler = 0;
	int	fromproc = 0;
	__sighandler_t *sigfunc;

	DBG_MSG(">>> _thr_sig_add %p (%d)\n", pthread, sig);

	curkse = _get_curkse();
	restart = _thread_sigact[sig - 1].sa_flags & SA_RESTART;
	sigfunc = _thread_sigact[sig - 1].sa_handler;
	fromproc = (curthread == _thr_sig_daemon);

	if (pthread->state == PS_DEAD || pthread->state == PS_DEADLOCK ||
	    pthread->state == PS_STATE_MAX)
	    	return (NULL); /* return false */

	if ((pthread->attr.flags & PTHREAD_SCOPE_SYSTEM) &&
	    (curthread != pthread)) {
	    	PANIC("Please use _thr_send_sig for bound thread");
		return (NULL);
	}

	if (pthread->curframe == NULL ||
	    (pthread->state != PS_SIGWAIT &&
	    SIGISMEMBER(pthread->sigmask, sig)) ||
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
				return (NULL);
			SIGADDSET(pthread->sigpend, sig);
		}
		if (!SIGISMEMBER(pthread->sigmask, sig)) {
			/* A quick path to exit process */
			if (sigfunc == SIG_DFL && sigprop(sig) & SA_KILL) {
				kse_thr_interrupt(NULL, KSE_INTR_SIGEXIT, sig);
				/* Never reach */
			}
			pthread->check_pending = 1;
			if (!(pthread->attr.flags & PTHREAD_SCOPE_SYSTEM) &&
			    (pthread->blocked != 0) && 
			    !THR_IN_CRITICAL(pthread))
				kse_thr_interrupt(&pthread->tcb->tcb_tmbx,
				    restart ? KSE_INTR_RESTART : KSE_INTR_INTERRUPT, 0);
		}
	}
	else {
		/* if process signal not exists, just return */
		if (fromproc) {
			if (!_thr_getprocsig(sig, &siginfo))
				return (NULL);
			info = &siginfo;
		}

		if (pthread->state != PS_SIGWAIT && sigfunc == SIG_DFL &&
		    (sigprop(sig) & SA_KILL)) {
			kse_thr_interrupt(NULL, KSE_INTR_SIGEXIT, sig);
			/* Never reach */
		}

		/*
		 * Process according to thread state:
		 */
		switch (pthread->state) {
		case PS_DEAD:
		case PS_DEADLOCK:
		case PS_STATE_MAX:
			return (NULL);	/* XXX return false */
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
			if (SIGISMEMBER(*(pthread->data.sigwait->waitset), sig)) {
				/* Return the signal number: */
				*(pthread->data.sigwait->siginfo) = pthread->siginfo[sig-1];
				/* Make the thread runnable: */
				kmbx = _thr_setrunnable_unlocked(pthread);
			} else {
				/* Increment the pending signal count. */
				SIGADDSET(pthread->sigpend, sig);
				if (!SIGISMEMBER(pthread->sigmask, sig)) {
					if (sigfunc == SIG_DFL &&
					    sigprop(sig) & SA_KILL) {
						kse_thr_interrupt(NULL,
							 KSE_INTR_SIGEXIT,
							 sig);
						/* Never reach */
					}
					pthread->check_pending = 1;
					pthread->interrupted = 1;
					kmbx = _thr_setrunnable_unlocked(pthread);
				}
			}
			return (kmbx);
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
			kmbx = _thr_setrunnable_unlocked(pthread);
		} else {
			pthread->check_pending = 1;
		}
	}
	return (kmbx);
}

/*
 * Send a signal to a specific thread (ala pthread_kill):
 */
void
_thr_sig_send(struct pthread *pthread, int sig)
{
	struct pthread *curthread = _get_curthread();
	struct kse_mailbox *kmbx;

	if (pthread->attr.flags & PTHREAD_SCOPE_SYSTEM) {
		kse_thr_interrupt(&pthread->tcb->tcb_tmbx, KSE_INTR_SENDSIG, sig);
		return;
	}

	/* Lock the scheduling queue of the target thread. */
	THR_SCHED_LOCK(curthread, pthread);
	if (_thread_sigact[sig - 1].sa_handler != SIG_IGN) {
		kmbx = _thr_sig_add(pthread, sig, NULL);
		THR_SCHED_UNLOCK(curthread, pthread);
		if (kmbx != NULL)
			kse_wakeup(kmbx);
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
	psf->psf_state = thread->state;
	psf->psf_wait_data = thread->data;
	psf->psf_wakeup_time = thread->wakeup_time;
}

void
_thr_signal_init(void)
{
	struct sigaction act;
	__siginfohandler_t *sigfunc;
	int i;
	sigset_t sigset;

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
		/* Intall wrapper if handler was set */
		sigfunc = _thread_sigact[i - 1].sa_sigaction;
		if (((__sighandler_t *)sigfunc) != SIG_DFL &&
		    ((__sighandler_t *)sigfunc) != SIG_IGN) {
		    	act = _thread_sigact[i - 1];
			act.sa_flags |= SA_SIGINFO;
			act.sa_sigaction =
				(__siginfohandler_t *)_thr_sig_handler;
			__sys_sigaction(i, &act, NULL);
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
		__sys_sigprocmask(SIG_SETMASK, &_thr_initial->sigmask, NULL);
		/*
		 * Abort this process if signal initialisation fails:
		 */
		PANIC("Cannot initialize signal handler");
	}
	__sys_sigprocmask(SIG_SETMASK, &_thr_initial->sigmask, NULL);
}

void
_thr_signal_deinit(void)
{
	int i;

	/* Enter a loop to get the existing signal status: */
	for (i = 1; i <= _SIG_MAXSIG; i++) {
		/* Check for signals which cannot be trapped: */
		if (i == SIGKILL || i == SIGSTOP) {
		}

		/* Set the signal handler details: */
		else if (__sys_sigaction(i, &_thread_sigact[i - 1],
			 NULL) != 0) {
			/*
			 * Abort this process if signal
			 * initialisation fails:
			 */
			PANIC("Cannot set signal handler info");
		}
	}
}

