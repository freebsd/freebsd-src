/*-
 * Copyright (c) 2003, Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/smp.h>
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/ucontext.h>
#include <sys/thr.h>
#include <sys/umtx.h>
#include <sys/limits.h>

#include <machine/frame.h>

extern int max_threads_per_proc;
extern int max_groups_per_proc;

SYSCTL_DECL(_kern_threads);
static int thr_scope = 0;
SYSCTL_INT(_kern_threads, OID_AUTO, thr_scope, CTLFLAG_RW,
	&thr_scope, 0, "sys or proc scope scheduling");

static int thr_concurrency = 0;
SYSCTL_INT(_kern_threads, OID_AUTO, thr_concurrency, CTLFLAG_RW,
	&thr_concurrency, 0, "a concurrency value if not default");

static int create_thread(struct thread *td, mcontext_t *ctx,
			 void (*start_func)(void *), void *arg,
			 char *stack_base, size_t stack_size,
			 char *tls_base,
			 long *child_tid, long *parent_tid,
			 int flags);

/*
 * System call interface.
 */
int
thr_create(struct thread *td, struct thr_create_args *uap)
    /* ucontext_t *ctx, long *id, int flags */
{
	ucontext_t ctx;
	int error;

	if ((error = copyin(uap->ctx, &ctx, sizeof(ctx))))
		return (error);

	error = create_thread(td, &ctx.uc_mcontext, NULL, NULL,
		NULL, 0, NULL, uap->id, NULL, uap->flags);
	return (error);
}

int
thr_new(struct thread *td, struct thr_new_args *uap)
    /* struct thr_param * */
{
	struct thr_param param;
	int error;

	if (uap->param_size < sizeof(param))
		return (EINVAL);
	if ((error = copyin(uap->param, &param, sizeof(param))))
		return (error);
	error = create_thread(td, NULL, param.start_func, param.arg,
		param.stack_base, param.stack_size, param.tls_base,
		param.child_tid, param.parent_tid, param.flags);
	return (error);
}

static int
create_thread(struct thread *td, mcontext_t *ctx,
	    void (*start_func)(void *), void *arg,
	    char *stack_base, size_t stack_size,
	    char *tls_base,
	    long *child_tid, long *parent_tid,
	    int flags)
{
	stack_t stack;
	struct thread *newtd;
	struct ksegrp *kg, *newkg;
	struct proc *p;
	long id;
	int error, scope_sys, linkkg;

	error = 0;
	p = td->td_proc;
	kg = td->td_ksegrp;

	/* Have race condition but it is cheap. */
	if ((p->p_numksegrps >= max_groups_per_proc) ||
	    (p->p_numthreads >= max_threads_per_proc)) {
		return (EPROCLIM);
	}

	/* Check PTHREAD_SCOPE_SYSTEM */
	scope_sys = (flags & THR_SYSTEM_SCOPE) != 0;

	/* sysctl overrides user's flag */
	if (thr_scope == 1)
		scope_sys = 0;
	else if (thr_scope == 2)
		scope_sys = 1;

	/* Initialize our td and new ksegrp.. */
	newtd = thread_alloc();

	/*
	 * Try the copyout as soon as we allocate the td so we don't
	 * have to tear things down in a failure case below.
	 * Here we copy out tid to two places, one for child and one
	 * for parent, because pthread can create a detached thread,
	 * if parent wants to safely access child tid, it has to provide 
	 * its storage, because child thread may exit quickly and
	 * memory is freed before parent thread can access it.
	 */
	id = newtd->td_tid;
	if ((child_tid != NULL &&
	    (error = copyout(&id, child_tid, sizeof(long)))) ||
	    (parent_tid != NULL &&
	    (error = copyout(&id, parent_tid, sizeof(long))))) {
	    	thread_free(newtd);
		return (error);
	}
	bzero(&newtd->td_startzero,
	    __rangeof(struct thread, td_startzero, td_endzero));
	bcopy(&td->td_startcopy, &newtd->td_startcopy,
	    __rangeof(struct thread, td_startcopy, td_endcopy));
	newtd->td_proc = td->td_proc;
	newtd->td_ucred = crhold(td->td_ucred);

	cpu_set_upcall(newtd, td);

	if (ctx != NULL) { /* old way to set user context */
		error = set_mcontext(newtd, ctx);
		if (error != 0) {
			thread_free(newtd);
			crfree(td->td_ucred);
			return (error);
		}
	} else {
		/* Set up our machine context. */
		stack.ss_sp = stack_base;
		stack.ss_size = stack_size;
		/* Set upcall address to user thread entry function. */
		cpu_set_upcall_kse(newtd, start_func, arg, &stack);
		/* Setup user TLS address and TLS pointer register. */
		error = cpu_set_user_tls(newtd, tls_base);
		if (error != 0) {
			thread_free(newtd);
			crfree(td->td_ucred);
			return (error);
		}
	}

	if ((td->td_proc->p_flag & P_HADTHREADS) == 0) {
		/* Treat initial thread as it has PTHREAD_SCOPE_PROCESS. */
		p->p_procscopegrp = kg;
		mtx_lock_spin(&sched_lock);
		sched_set_concurrency(kg,
		    thr_concurrency ? thr_concurrency : (2*mp_ncpus));
		mtx_unlock_spin(&sched_lock);
	}

	linkkg = 0;
	if (scope_sys) {
		linkkg = 1;
		newkg = ksegrp_alloc();
		bzero(&newkg->kg_startzero,
		    __rangeof(struct ksegrp, kg_startzero, kg_endzero));
		bcopy(&kg->kg_startcopy, &newkg->kg_startcopy,
		    __rangeof(struct ksegrp, kg_startcopy, kg_endcopy));
		sched_init_concurrency(newkg);
		PROC_LOCK(td->td_proc);
	} else {
		/*
		 * Try to create a KSE group which will be shared
		 * by all PTHREAD_SCOPE_PROCESS threads.
		 */
retry:
		PROC_LOCK(td->td_proc);
		if ((newkg = p->p_procscopegrp) == NULL) {
			PROC_UNLOCK(p);
			newkg = ksegrp_alloc();
			bzero(&newkg->kg_startzero,
			    __rangeof(struct ksegrp, kg_startzero, kg_endzero));
			bcopy(&kg->kg_startcopy, &newkg->kg_startcopy,
			    __rangeof(struct ksegrp, kg_startcopy, kg_endcopy));
			PROC_LOCK(p);
			if (p->p_procscopegrp == NULL) {
				p->p_procscopegrp = newkg;
				sched_init_concurrency(newkg);
				sched_set_concurrency(newkg,
				    thr_concurrency ? thr_concurrency : (2*mp_ncpus));
				linkkg = 1;
			} else {
				PROC_UNLOCK(p);
				ksegrp_free(newkg);
				goto retry;
			}
		}
	}

	td->td_proc->p_flag |= P_HADTHREADS;
	newtd->td_sigmask = td->td_sigmask;
	mtx_lock_spin(&sched_lock);
	if (linkkg)
		ksegrp_link(newkg, p);
	thread_link(newtd, newkg);
	PROC_UNLOCK(p);

	/* let the scheduler know about these things. */
	if (linkkg)
		sched_fork_ksegrp(td, newkg);
	sched_fork_thread(td, newtd);
	TD_SET_CAN_RUN(newtd);
	/* if ((flags & THR_SUSPENDED) == 0) */
		setrunqueue(newtd, SRQ_BORING);
	mtx_unlock_spin(&sched_lock);

	return (error);
}

int
thr_self(struct thread *td, struct thr_self_args *uap)
    /* long *id */
{
	long id;
	int error;

	id = td->td_tid;
	if ((error = copyout(&id, uap->id, sizeof(long))))
		return (error);

	return (0);
}

int
thr_exit(struct thread *td, struct thr_exit_args *uap)
    /* long *state */
{
	struct proc *p;

	p = td->td_proc;

	/* Signal userland that it can free the stack. */
	if ((void *)uap->state != NULL) {
		suword((void *)uap->state, 1);
		kern_umtx_wake(td, uap->state, INT_MAX);
	}

	PROC_LOCK(p);
	sigqueue_flush(&td->td_sigqueue);
	mtx_lock_spin(&sched_lock);

	/*
	 * Shutting down last thread in the proc.  This will actually
	 * call exit() in the trampoline when it returns.
	 */
	if (p->p_numthreads != 1) {
		thread_exit();
		/* NOTREACHED */
	}
	mtx_unlock_spin(&sched_lock);
	PROC_UNLOCK(p);
	return (0);
}

int
thr_kill(struct thread *td, struct thr_kill_args *uap)
    /* long id, int sig */
{
	struct thread *ttd;
	struct proc *p;
	int error;

	p = td->td_proc;
	error = 0;
	PROC_LOCK(p);
	ttd = thread_find(p, uap->id);
	if (ttd == NULL) {
		error = ESRCH;
		goto out;
	}
	if (uap->sig == 0)
		goto out;
	if (!_SIG_VALID(uap->sig)) {
		error = EINVAL;
		goto out;
	}
	tdsignal(p, ttd, uap->sig, NULL);
out:
	PROC_UNLOCK(p);
	return (error);
}

int
thr_suspend(struct thread *td, struct thr_suspend_args *uap)
	/* const struct timespec *timeout */
{
	struct timespec ts;
	struct timeval	tv;
	int error;
	int hz;

	hz = 0;
	error = 0;
	if (uap->timeout != NULL) {
		error = copyin((const void *)uap->timeout, (void *)&ts,
		    sizeof(struct timespec));
		if (error != 0)
			return (error);
		if (ts.tv_nsec < 0 || ts.tv_nsec > 1000000000)
			return (EINVAL);
		if (ts.tv_sec == 0 && ts.tv_nsec == 0)
			return (ETIMEDOUT);
		TIMESPEC_TO_TIMEVAL(&tv, &ts);
		hz = tvtohz(&tv);
	}
	PROC_LOCK(td->td_proc);
	if ((td->td_flags & TDF_THRWAKEUP) == 0)
		error = msleep((void *)td, &td->td_proc->p_mtx,
		    td->td_priority | PCATCH, "lthr", hz);
	if (td->td_flags & TDF_THRWAKEUP) {
		mtx_lock_spin(&sched_lock);
		td->td_flags &= ~TDF_THRWAKEUP;
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(td->td_proc);
		return (0);
	}
	PROC_UNLOCK(td->td_proc);
	if (error == EWOULDBLOCK)
		error = ETIMEDOUT;
	else if (error == ERESTART) {
		if (hz != 0)
			error = EINTR;
	}
	return (error);
}

int
thr_wake(struct thread *td, struct thr_wake_args *uap)
	/* long id */
{
	struct proc *p;
	struct thread *ttd;

	p = td->td_proc;
	PROC_LOCK(p);
	ttd = thread_find(p, uap->id);
	if (ttd == NULL) {
		PROC_UNLOCK(p);
		return (ESRCH);
	}
	mtx_lock_spin(&sched_lock);
	ttd->td_flags |= TDF_THRWAKEUP;
	mtx_unlock_spin(&sched_lock);
	wakeup((void *)ttd);
	PROC_UNLOCK(p);
	return (0);
}
