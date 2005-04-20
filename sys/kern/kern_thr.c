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

#include <machine/frame.h>

extern int max_threads_per_proc;
extern int max_groups_per_proc;

SYSCTL_DECL(_kern_threads);
static int thr_scope_sys = 0;
SYSCTL_INT(_kern_threads, OID_AUTO, thr_scope_sys, CTLFLAG_RW,
	&thr_scope_sys, 0, "sys or proc scope scheduling");

static int thr_concurrency = 0;
SYSCTL_INT(_kern_threads, OID_AUTO, thr_concurrency, CTLFLAG_RW,
	&thr_concurrency, 0, "a concurrency value if not default");

/*
 * System call interface.
 */
int
thr_create(struct thread *td, struct thr_create_args *uap)
    /* ucontext_t *ctx, long *id, int flags */
{
	struct thread *newtd;
	ucontext_t ctx;
	long id;
	int error;
	struct ksegrp *kg, *newkg;
	struct proc *p;
	int scope_sys;

	p = td->td_proc;
	kg = td->td_ksegrp;
	if ((error = copyin(uap->ctx, &ctx, sizeof(ctx))))
		return (error);

	/* Have race condition but it is cheap */
	if ((p->p_numksegrps >= max_groups_per_proc) ||
	    (p->p_numthreads >= max_threads_per_proc)) {
		return (EPROCLIM);
	}

	/*
	 * Use a local copy to close a race against the user
	 * changing thr_scope_sys.
	 */
	scope_sys = thr_scope_sys;

	/* Initialize our td and new ksegrp.. */
	newtd = thread_alloc();
	if (scope_sys)
		newkg = ksegrp_alloc();
	else
		newkg = kg;
	/*
	 * Try the copyout as soon as we allocate the td so we don't have to
	 * tear things down in a failure case below.
	 */
	id = newtd->td_tid;
	if ((error = copyout(&id, uap->id, sizeof(long)))) {
		if (scope_sys)
			ksegrp_free(newkg);
		thread_free(newtd);
		return (error);
	}

	bzero(&newtd->td_startzero,
	    __rangeof(struct thread, td_startzero, td_endzero));
	bcopy(&td->td_startcopy, &newtd->td_startcopy,
	    __rangeof(struct thread, td_startcopy, td_endcopy));

	if (scope_sys) {
		bzero(&newkg->kg_startzero,
		    __rangeof(struct ksegrp, kg_startzero, kg_endzero));
		bcopy(&kg->kg_startcopy, &newkg->kg_startcopy,
		    __rangeof(struct ksegrp, kg_startcopy, kg_endcopy));
	}

	newtd->td_proc = td->td_proc;
	newtd->td_ucred = crhold(td->td_ucred);

	/* Set up our machine context. */
	cpu_set_upcall(newtd, td);
	error = set_mcontext(newtd, &ctx.uc_mcontext);
	if (error != 0) {
		if (scope_sys)
			ksegrp_free(newkg);
		thread_free(newtd);
		crfree(td->td_ucred);
		goto out;
	}

	/* Link the thread and kse into the ksegrp and make it runnable. */
	PROC_LOCK(td->td_proc);
	if (scope_sys) {
			sched_init_concurrency(newkg);
	} else {
		if ((td->td_proc->p_flag & P_HADTHREADS) == 0) {
			sched_set_concurrency(kg,
			    thr_concurrency ? thr_concurrency : (2*mp_ncpus));
		}
	}
			
	td->td_proc->p_flag |= P_HADTHREADS; 
	newtd->td_sigmask = td->td_sigmask;
	mtx_lock_spin(&sched_lock);
	if (scope_sys)
		ksegrp_link(newkg, p);
	thread_link(newtd, newkg);
	mtx_unlock_spin(&sched_lock);
	PROC_UNLOCK(p);

	/* let the scheduler know about these things. */
	mtx_lock_spin(&sched_lock);
	if (scope_sys)
		sched_fork_ksegrp(td, newkg);
	sched_fork_thread(td, newtd);

	TD_SET_CAN_RUN(newtd);
	if ((uap->flags & THR_SUSPENDED) == 0)
		setrunqueue(newtd, SRQ_BORING);

	mtx_unlock_spin(&sched_lock);

out:
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
	if ((void *)uap->state != NULL)
		suword((void *)uap->state, 1);

	PROC_LOCK(p);
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
	FOREACH_THREAD_IN_PROC(p, ttd) {
		if (ttd->td_tid == uap->id)
			break;
	}
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
	tdsignal(ttd, uap->sig, SIGTARGET_TD);
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
	mtx_lock_spin(&sched_lock);
	td->td_flags &= ~TDF_THRWAKEUP;
	mtx_unlock_spin(&sched_lock);
	PROC_UNLOCK(td->td_proc);
	return (error == EWOULDBLOCK ? ETIMEDOUT : error);
}

int
thr_wake(struct thread *td, struct thr_wake_args *uap)
	/* long id */
{
	struct thread *ttd;

	PROC_LOCK(td->td_proc);
	FOREACH_THREAD_IN_PROC(td->td_proc, ttd) {
		if (ttd->td_tid == uap->id)
			break;
	}
	if (ttd == NULL) {
		PROC_UNLOCK(td->td_proc);
		return (ESRCH);
	}
	mtx_lock_spin(&sched_lock);
	ttd->td_flags |= TDF_THRWAKEUP;
	mtx_unlock_spin(&sched_lock);
	wakeup_one((void *)ttd);
	PROC_UNLOCK(td->td_proc);
	return (0);
}
