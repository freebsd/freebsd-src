/*
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
#include <sys/sysent.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/ucontext.h>
#include <sys/thr.h>

#include <machine/frame.h>

/*
 * Back end support functions.
 */

void
thr_exit1(void)
{
	struct ksegrp *kg;
	struct thread *td;
	struct kse *ke;
	struct proc *p;

	td = curthread;
	p = td->td_proc;
	kg = td->td_ksegrp;
	ke = td->td_kse;

	mtx_assert(&sched_lock, MA_OWNED);
	PROC_LOCK_ASSERT(p, MA_OWNED);
	KASSERT(!mtx_owned(&Giant), ("dying thread owns giant"));

	/*
	 * Shutting down last thread in the proc.  This will actually
	 * call exit() in the trampoline when it returns.
	 */
	if (p->p_numthreads == 1) {
		PROC_UNLOCK(p);
		return;
	}

	/*
	 * XXX Undelivered process wide signals should be reposted to the
	 * proc.
	 */

	/* Clean up cpu resources. */
	cpu_thread_exit(td);

	/* XXX make thread_unlink() */
	TAILQ_REMOVE(&p->p_threads, td, td_plist);
	p->p_numthreads--;
	TAILQ_REMOVE(&kg->kg_threads, td, td_kglist);
	kg->kg_numthreads--;

	ke->ke_state = KES_UNQUEUED;
	ke->ke_thread = NULL;
	kse_unlink(ke);
	sched_exit_kse(TAILQ_NEXT(ke, ke_kglist), ke);

	/*
	 * If we were stopped while waiting for all threads to exit and this
	 * is the last thread wakeup the exiting thread.
	 */
	if (P_SHOULDSTOP(p) == P_STOPPED_SINGLE)
		if (p->p_numthreads == 1)
			thread_unsuspend_one(p->p_singlethread);

	PROC_UNLOCK(p);
	td->td_kse = NULL;
	td->td_state = TDS_INACTIVE;
#if 0
	td->td_proc = NULL;
#endif
	td->td_ksegrp = NULL;
	td->td_last_kse = NULL;
	sched_exit_thread(TAILQ_NEXT(td, td_kglist), td);
	thread_stash(td);

#if !defined(__alpha__) && !defined(__powerpc__) 
	cpu_throw(td, choosethread());
#else
	cpu_throw();
#endif
}

#define	RANGEOF(type, start, end) (offsetof(type, end) - offsetof(type, start))

/*
 * System call interface.
 */
int
thr_create(struct thread *td, struct thr_create_args *uap)
    /* ucontext_t *ctx, thr_id_t *id, int flags */
{
	struct kse *ke0;
	struct thread *td0;
	ucontext_t ctx;
	int error;

	if ((error = copyin(uap->ctx, &ctx, sizeof(ctx))))
		return (error);

	/* Initialize our td. */
	td0 = thread_alloc();

	/*
	 * Try the copyout as soon as we allocate the td so we don't have to
	 * tear things down in a failure case below.
	 */
	if ((error = copyout(&td0, uap->id, sizeof(thr_id_t)))) {
		thread_free(td0);
		return (error);
	}

	bzero(&td0->td_startzero,
	    (unsigned)RANGEOF(struct thread, td_startzero, td_endzero));
	bcopy(&td->td_startcopy, &td0->td_startcopy,
	    (unsigned) RANGEOF(struct thread, td_startcopy, td_endcopy));

	td0->td_proc = td->td_proc;
	PROC_LOCK(td->td_proc);
	td0->td_sigmask = td->td_sigmask;
	PROC_UNLOCK(td->td_proc);
	td0->td_ucred = crhold(td->td_ucred);

	/* Initialize our kse structure. */
	ke0 = kse_alloc();
	bzero(&ke0->ke_startzero,
	    RANGEOF(struct kse, ke_startzero, ke_endzero));

	/* Set up our machine context. */
	cpu_set_upcall(td0, td);
	error = set_mcontext(td0, &ctx.uc_mcontext);
	if (error != 0) {
		kse_free(ke0);
		thread_free(td0);
		goto out;
	} 

	/* Link the thread and kse into the ksegrp and make it runnable. */
	mtx_lock_spin(&sched_lock);

	thread_link(td0, td->td_ksegrp);
	kse_link(ke0, td->td_ksegrp);

	/* Bind this thread and kse together. */
	td0->td_kse = ke0;
	ke0->ke_thread = td0;

	sched_fork_kse(td->td_kse, ke0);
	sched_fork_thread(td, td0);

	TD_SET_CAN_RUN(td0);
	if ((uap->flags & THR_SUSPENDED) == 0)
		setrunqueue(td0);

	mtx_unlock_spin(&sched_lock);

out:
	return (error);
}

int
thr_self(struct thread *td, struct thr_self_args *uap)
    /* thr_id_t *id */
{
	int error;

	if ((error = copyout(&td, uap->id, sizeof(thr_id_t))))
		return (error);

	return (0);
}

int
thr_exit(struct thread *td, struct thr_exit_args *uap)
    /* NULL */
{
	struct proc *p;

	p = td->td_proc;

	PROC_LOCK(p);
	mtx_lock_spin(&sched_lock);

	/*
	 * This unlocks proc and doesn't return unless this is the last
	 * thread.
	 */
	thr_exit1();
	mtx_unlock_spin(&sched_lock);

	return (0);
}

int
thr_kill(struct thread *td, struct thr_kill_args *uap)
    /* thr_id_t id, int sig */
{
	struct thread *ttd;
	struct proc *p;
	int error;

	p = td->td_proc;
	error = 0;

	PROC_LOCK(p);

	FOREACH_THREAD_IN_PROC(p, ttd)
		if (ttd == uap->id)
			break;

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

	/*
	 * We need a way to force this to go into this thread's siglist.
	 * Until then blocked signals will go to the proc.
	 */
	tdsignal(ttd, uap->sig);
out:
	PROC_UNLOCK(p);

	return (error);
}
