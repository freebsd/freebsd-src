/* 
 * Copyright (C) 2001 Julian Elischer <julian@freebsd.org>.
 *  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible 
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/filedesc.h>
#include <sys/tty.h>
#include <sys/signalvar.h>
#include <sys/sx.h>
#include <sys/user.h>
#include <sys/jail.h>
#include <sys/kse.h>
#include <sys/ktr.h>
#include <sys/ucontext.h>

#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/pmap.h>
#include <vm/uma.h>
#include <vm/vm_map.h>

#include <machine/frame.h>

/*
 * KSEGRP related storage.
 */
static uma_zone_t ksegrp_zone;
static uma_zone_t kse_zone;
static uma_zone_t thread_zone;

/* DEBUG ONLY */
SYSCTL_NODE(_kern, OID_AUTO, threads, CTLFLAG_RW, 0, "thread allocation");
static int oiks_debug = 1;	/* 0 disable, 1 printf, 2 enter debugger */
SYSCTL_INT(_kern_threads, OID_AUTO, oiks, CTLFLAG_RW,
	&oiks_debug, 0, "OIKS thread debug");

static int max_threads_per_proc = 6;
SYSCTL_INT(_kern_threads, OID_AUTO, max_per_proc, CTLFLAG_RW,
	&max_threads_per_proc, 0, "Limit on threads per proc");

#define RANGEOF(type, start, end) (offsetof(type, end) - offsetof(type, start))

struct threadqueue zombie_threads = TAILQ_HEAD_INITIALIZER(zombie_threads);
struct mtx zombie_thread_lock;
MTX_SYSINIT(zombie_thread_lock, &zombie_thread_lock,
    "zombie_thread_lock", MTX_SPIN);

/*
 * Pepare a thread for use.
 */
static void
thread_ctor(void *mem, int size, void *arg)
{
	struct thread	*td;

	KASSERT((size == sizeof(struct thread)),
	    ("size mismatch: %d != %d\n", size, (int)sizeof(struct thread)));

	td = (struct thread *)mem;
	td->td_state = TDS_INACTIVE;
	td->td_flags |= TDF_UNBOUND;
}

/*
 * Reclaim a thread after use.
 */
static void
thread_dtor(void *mem, int size, void *arg)
{
	struct thread	*td;

	KASSERT((size == sizeof(struct thread)),
	    ("size mismatch: %d != %d\n", size, (int)sizeof(struct thread)));

	td = (struct thread *)mem;

#ifdef INVARIANTS
	/* Verify that this thread is in a safe state to free. */
	switch (td->td_state) {
	case TDS_INHIBITED:
	case TDS_RUNNING:
	case TDS_CAN_RUN:
	case TDS_RUNQ:
		/*
		 * We must never unlink a thread that is in one of
		 * these states, because it is currently active.
		 */
		panic("bad state for thread unlinking");
		/* NOTREACHED */
	case TDS_INACTIVE:
		break;
	default:
		panic("bad thread state");
		/* NOTREACHED */
	}
#endif
}

/*
 * Initialize type-stable parts of a thread (when newly created).
 */
static void
thread_init(void *mem, int size)
{
	struct thread	*td;

	KASSERT((size == sizeof(struct thread)),
	    ("size mismatch: %d != %d\n", size, (int)sizeof(struct thread)));

	td = (struct thread *)mem;
	mtx_lock(&Giant);
	pmap_new_thread(td, 0);
	mtx_unlock(&Giant);
	cpu_thread_setup(td);
}

/*
 * Tear down type-stable parts of a thread (just before being discarded).
 */
static void
thread_fini(void *mem, int size)
{
	struct thread	*td;

	KASSERT((size == sizeof(struct thread)),
	    ("size mismatch: %d != %d\n", size, (int)sizeof(struct thread)));

	td = (struct thread *)mem;
	pmap_dispose_thread(td);
}

/*
 * Fill a ucontext_t with a thread's context information.
 *
 * This is an analogue to getcontext(3).
 */
void
thread_getcontext(struct thread *td, ucontext_t *uc)
{

/*
 * XXX this is declared in a MD include file, i386/include/ucontext.h but
 * is used in MI code.
 */
#ifdef __i386__
	get_mcontext(td, &uc->uc_mcontext);
#endif
	uc->uc_sigmask = td->td_proc->p_sigmask;
}

/*
 * Set a thread's context from a ucontext_t.
 *
 * This is an analogue to setcontext(3).
 */
int
thread_setcontext(struct thread *td, ucontext_t *uc)
{
	int ret;

/*
 * XXX this is declared in a MD include file, i386/include/ucontext.h but
 * is used in MI code.
 */
#ifdef __i386__
	ret = set_mcontext(td, &uc->uc_mcontext);
#else
	ret = ENOSYS;
#endif
	if (ret == 0) {
		SIG_CANTMASK(uc->uc_sigmask);
		PROC_LOCK(td->td_proc);
		td->td_proc->p_sigmask = uc->uc_sigmask;
		PROC_UNLOCK(td->td_proc);
	}
	return (ret);
}

/*
 * Initialize global thread allocation resources.
 */
void
threadinit(void)
{

#ifndef __ia64__
	thread_zone = uma_zcreate("THREAD", sizeof (struct thread),
	    thread_ctor, thread_dtor, thread_init, thread_fini,
	    UMA_ALIGN_CACHE, 0);
#else
	/*
	 * XXX the ia64 kstack allocator is really lame and is at the mercy
	 * of contigmallloc().  This hackery is to pre-construct a whole
	 * pile of thread structures with associated kernel stacks early
	 * in the system startup while contigmalloc() still works. Once we
	 * have them, keep them.  Sigh.
	 */
	thread_zone = uma_zcreate("THREAD", sizeof (struct thread),
	    thread_ctor, thread_dtor, thread_init, thread_fini,
	    UMA_ALIGN_CACHE, UMA_ZONE_NOFREE);
	uma_prealloc(thread_zone, 512);		/* XXX arbitary */
#endif
	ksegrp_zone = uma_zcreate("KSEGRP", sizeof (struct ksegrp),
	    NULL, NULL, NULL, NULL,
	    UMA_ALIGN_CACHE, 0);
	kse_zone = uma_zcreate("KSE", sizeof (struct kse),
	    NULL, NULL, NULL, NULL,
	    UMA_ALIGN_CACHE, 0);
}

/*
 * Stash an embarasingly extra thread into the zombie thread queue.
 */
void
thread_stash(struct thread *td)
{
	mtx_lock_spin(&zombie_thread_lock);
	TAILQ_INSERT_HEAD(&zombie_threads, td, td_runq);
	mtx_unlock_spin(&zombie_thread_lock);
}

/*
 * Reap zombie threads.
 */
void
thread_reap(void)
{
	struct thread *td_reaped;

	/*
	 * don't even bother to lock if none at this instant
	 * We really don't care about the next instant..
	 */
	if (!TAILQ_EMPTY(&zombie_threads)) {
		mtx_lock_spin(&zombie_thread_lock);
		while (!TAILQ_EMPTY(&zombie_threads)) {
			td_reaped = TAILQ_FIRST(&zombie_threads);
			TAILQ_REMOVE(&zombie_threads, td_reaped, td_runq);
			mtx_unlock_spin(&zombie_thread_lock);
			thread_free(td_reaped);
			mtx_lock_spin(&zombie_thread_lock);
		}
		mtx_unlock_spin(&zombie_thread_lock);
	}
}

/*
 * Allocate a ksegrp.
 */
struct ksegrp *
ksegrp_alloc(void)
{
	return (uma_zalloc(ksegrp_zone, M_WAITOK));
}

/*
 * Allocate a kse.
 */
struct kse *
kse_alloc(void)
{
	return (uma_zalloc(kse_zone, M_WAITOK));
}

/*
 * Allocate a thread.
 */
struct thread *
thread_alloc(void)
{
	thread_reap(); /* check if any zombies to get */
	return (uma_zalloc(thread_zone, M_WAITOK));
}

/*
 * Deallocate a ksegrp.
 */
void
ksegrp_free(struct ksegrp *td)
{
	uma_zfree(ksegrp_zone, td);
}

/*
 * Deallocate a kse.
 */
void
kse_free(struct kse *td)
{
	uma_zfree(kse_zone, td);
}

/*
 * Deallocate a thread.
 */
void
thread_free(struct thread *td)
{
	uma_zfree(thread_zone, td);
}

/*
 * Store the thread context in the UTS's mailbox.
 * then add the mailbox at the head of a list we are building in user space.
 * The list is anchored in the ksegrp structure.
 */
int
thread_export_context(struct thread *td)
{
	struct proc *p;
	struct ksegrp *kg;
	uintptr_t mbx;
	void *addr;
	int error;
	ucontext_t uc;

	p = td->td_proc;
	kg = td->td_ksegrp;

	/* Export the user/machine context. */
#if 0
	addr = (caddr_t)td->td_mailbox +
	    offsetof(struct kse_thr_mailbox, tm_context);
#else /* if user pointer arithmetic is valid in the kernel */
		addr = (void *)(&td->td_mailbox->tm_context);
#endif
	error = copyin(addr, &uc, sizeof(ucontext_t));
	if (error == 0) {
		thread_getcontext(td, &uc);
		error = copyout(&uc, addr, sizeof(ucontext_t));

	}
	if (error) {
		PROC_LOCK(p);
		psignal(p, SIGSEGV);
		PROC_UNLOCK(p);
		return (error);
	}
	/* get address in latest mbox of list pointer */
#if 0
	addr = (caddr_t)td->td_mailbox
	    + offsetof(struct kse_thr_mailbox , tm_next);
#else /* if user pointer arithmetic is valid in the kernel */
	addr = (void *)(&td->td_mailbox->tm_next);
#endif
	/*
	 * Put the saved address of the previous first
	 * entry into this one
	 */
	for (;;) {
		mbx = (uintptr_t)kg->kg_completed;
		if (suword(addr, mbx)) {
			PROC_LOCK(p);
			psignal(p, SIGSEGV);
			PROC_UNLOCK(p);
			return (EFAULT);
		}
		PROC_LOCK(p);
		if (mbx == (uintptr_t)kg->kg_completed) {
			kg->kg_completed = td->td_mailbox;
			PROC_UNLOCK(p);
			break;
		}
		PROC_UNLOCK(p);
	}
	return (0);
}

/*
 * Take the list of completed mailboxes for this KSEGRP and put them on this
 * KSE's mailbox as it's the next one going up.
 */
static int
thread_link_mboxes(struct ksegrp *kg, struct kse *ke)
{
	struct proc *p = kg->kg_proc;
	void *addr;
	uintptr_t mbx;

#if 0
	addr = (caddr_t)ke->ke_mailbox
	    + offsetof(struct kse_mailbox, km_completed);
#else /* if user pointer arithmetic is valid in the kernel */
		addr = (void *)(&ke->ke_mailbox->km_completed);
#endif
	for (;;) {
		mbx = (uintptr_t)kg->kg_completed;
		if (suword(addr, mbx)) {
			PROC_LOCK(p);
			psignal(p, SIGSEGV);
			PROC_UNLOCK(p);
			return (EFAULT);
		}
		/* XXXKSE could use atomic CMPXCH here */
		PROC_LOCK(p);
		if (mbx == (uintptr_t)kg->kg_completed) {
			kg->kg_completed = NULL;
			PROC_UNLOCK(p);
			break;
		}
		PROC_UNLOCK(p);
	}
	return (0);
}

/*
 * Discard the current thread and exit from its context.
 *
 * Because we can't free a thread while we're operating under its context,
 * push the current thread into our KSE's ke_tdspare slot, freeing the
 * thread that might be there currently. Because we know that only this
 * processor will run our KSE, we needn't worry about someone else grabbing
 * our context before we do a cpu_throw.
 */
void
thread_exit(void)
{
	struct thread *td;
	struct kse *ke;
	struct proc *p;
	struct ksegrp	*kg;

	td = curthread;
	kg = td->td_ksegrp;
	p = td->td_proc;
	ke = td->td_kse;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT(p != NULL, ("thread exiting without a process"));
	KASSERT(ke != NULL, ("thread exiting without a kse"));
	KASSERT(kg != NULL, ("thread exiting without a kse group"));
	PROC_LOCK_ASSERT(p, MA_OWNED);
	CTR1(KTR_PROC, "thread_exit: thread %p", td);
	KASSERT(!mtx_owned(&Giant), ("dying thread owns giant"));

	if (ke->ke_tdspare != NULL) {
		thread_stash(ke->ke_tdspare);
		ke->ke_tdspare = NULL;
	}
	cpu_thread_exit(td);	/* XXXSMP */

	/*
	 * The last thread is left attached to the process
	 * So that the whole bundle gets recycled. Skip
	 * all this stuff.
	 */
	if (p->p_numthreads > 1) {
		/* Reassign this thread's KSE. */
		ke->ke_thread = NULL;
		td->td_kse = NULL;
		ke->ke_state = KES_UNQUEUED;
		if (ke->ke_bound == td)
			ke->ke_bound = NULL;
		kse_reassign(ke);

		/* Unlink this thread from its proc. and the kseg */
		TAILQ_REMOVE(&p->p_threads, td, td_plist);
		p->p_numthreads--;
		TAILQ_REMOVE(&kg->kg_threads, td, td_kglist);
		kg->kg_numthreads--;
		/*
		 * The test below is NOT true if we are the
		 * sole exiting thread. P_STOPPED_SNGL is unset
		 * in exit1() after it is the only survivor.
		 */
		if (P_SHOULDSTOP(p) == P_STOPPED_SINGLE) {
			if (p->p_numthreads == p->p_suspcount) {
				thread_unsuspend_one(p->p_singlethread);
			}
		}
		PROC_UNLOCK(p);
		td->td_state	= TDS_INACTIVE;
		td->td_proc	= NULL;
		td->td_ksegrp	= NULL;
		td->td_last_kse	= NULL;
		ke->ke_tdspare = td;
	} else {
		PROC_UNLOCK(p);
	}

	cpu_throw();
	/* NOTREACHED */
}

/*
 * Link a thread to a process.
 * set up anything that needs to be initialized for it to
 * be used by the process.
 *
 * Note that we do not link to the proc's ucred here.
 * The thread is linked as if running but no KSE assigned.
 */
void
thread_link(struct thread *td, struct ksegrp *kg)
{
	struct proc *p;

	p = kg->kg_proc;
	td->td_state = TDS_INACTIVE;
	td->td_proc	= p;
	td->td_ksegrp	= kg;
	td->td_last_kse	= NULL;

	LIST_INIT(&td->td_contested);
	callout_init(&td->td_slpcallout, 1);
	TAILQ_INSERT_HEAD(&p->p_threads, td, td_plist);
	TAILQ_INSERT_HEAD(&kg->kg_threads, td, td_kglist);
	p->p_numthreads++;
	kg->kg_numthreads++;
	if (oiks_debug && p->p_numthreads > max_threads_per_proc) {
		printf("OIKS %d\n", p->p_numthreads);
		if (oiks_debug > 1)
			Debugger("OIKS");
	}
	td->td_kse	= NULL;
}

/*
 * Create a thread and schedule it for upcall on the KSE given.
 */
struct thread *
thread_schedule_upcall(struct thread *td, struct kse *ke)
{
	struct thread *td2;

	mtx_assert(&sched_lock, MA_OWNED);
	if (ke->ke_tdspare != NULL) {
		td2 = ke->ke_tdspare;
		ke->ke_tdspare = NULL;
	} else {
		mtx_unlock_spin(&sched_lock);
		td2 = thread_alloc();
		mtx_lock_spin(&sched_lock);
	}
	CTR3(KTR_PROC, "thread_schedule_upcall: thread %p (pid %d, %s)",
	     td, td->td_proc->p_pid, td->td_proc->p_comm);
	bzero(&td2->td_startzero,
	    (unsigned)RANGEOF(struct thread, td_startzero, td_endzero));
	bcopy(&td->td_startcopy, &td2->td_startcopy,
	    (unsigned) RANGEOF(struct thread, td_startcopy, td_endcopy));
	thread_link(td2, ke->ke_ksegrp);
	cpu_set_upcall(td2, td->td_pcb);
	bcopy(td->td_frame, td2->td_frame, sizeof(struct trapframe));
	/*
	 * The user context for this thread is selected when we choose
	 * a KSE and return to userland on it. All we need do here is
	 * note that the thread exists in order to perform an upcall.
	 *
	 * Since selecting a KSE to perform the upcall involves locking
	 * that KSE's context to our upcall, its best to wait until the
	 * last possible moment before grabbing a KSE. We do this in
	 * userret().
	 */
	td2->td_ucred = crhold(td->td_ucred);
	td2->td_flags = TDF_UNBOUND|TDF_UPCALLING;
	TD_SET_CAN_RUN(td2);
	setrunqueue(td2);
	return (td2);
}

/*
 * Schedule an upcall to notify a KSE process recieved signals.
 *
 * XXX - Modifying a sigset_t like this is totally bogus.
 */
struct thread *
signal_upcall(struct proc *p, int sig)
{
	struct thread *td, *td2;
	struct kse *ke;
	sigset_t ss;
	int error;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	td = FIRST_THREAD_IN_PROC(p);
	ke = td->td_kse;
	PROC_UNLOCK(p);
	error = copyin(&ke->ke_mailbox->km_sigscaught, &ss, sizeof(sigset_t));
	PROC_LOCK(p);
	if (error)
		return (NULL);
	SIGADDSET(ss, sig);
	PROC_UNLOCK(p);
	error = copyout(&ss, &ke->ke_mailbox->km_sigscaught, sizeof(sigset_t));
	PROC_LOCK(p);
	if (error)
		return (NULL);
	mtx_lock_spin(&sched_lock);
	td2 = thread_schedule_upcall(td, ke);
	mtx_unlock_spin(&sched_lock);
	return (td2);
}

/*
 * Consider whether or not an upcall should be made, and update the
 * TDF_UPCALLING flag appropriately.
 *
 * This function is called when the current thread had been bound to a user
 * thread that performed a syscall that blocked, and is now returning.
 * Got that? syscall -> msleep -> wakeup -> syscall_return -> us.
 *
 * This thread will be returned to the UTS in its mailbox as a completed
 * thread.  We need to decide whether or not to perform an upcall now,
 * or simply queue the thread for later.
 *
 * XXXKSE Future enhancement: We could also return back to
 * the thread if we haven't had to do an upcall since then.
 * If the KSE's copy is == the thread's copy, and there are
 * no other completed threads.
 */
static int
thread_consider_upcalling(struct thread *td)
{
	struct proc *p;
	struct ksegrp *kg;
	int error;

	/*
	 * Save the thread's context, and link it
	 * into the KSEGRP's list of completed threads.
	 */
	error = thread_export_context(td);
	td->td_flags &= ~TDF_UNBOUND;
	td->td_mailbox = NULL;
	if (error)
		/*
		 * Failing to do the KSE operation just defaults
		 * back to synchonous operation, so just return from
		 * the syscall.
		 */
		return (error);

	/*
	 * Decide whether to perform an upcall now.
	 */
	/* Make sure there are no other threads waiting to run. */
	p = td->td_proc;
	kg = td->td_ksegrp;
	PROC_LOCK(p);
	mtx_lock_spin(&sched_lock);
	/* bogus test, ok for testing though */
	if (TAILQ_FIRST(&kg->kg_runq) && 
	    (TAILQ_LAST(&kg->kg_runq, threadqueue) 
		!= kg->kg_last_assigned)) {
		/*
		 * Another thread in this KSEG needs to run.
		 * Switch to it instead of performing an upcall,
		 * abondoning this thread.  Perform the upcall
		 * later; discard this thread for now.
		 *
		 * XXXKSE - As for the other threads to run;
		 * we COULD rush through all the threads
		 * in this KSEG at this priority, or we
		 * could throw the ball back into the court
		 * and just run the highest prio kse available.
		 * What is OUR priority?  The priority of the highest
		 * sycall waiting to be returned?
		 * For now, just let another KSE run (easiest).
		 */
		thread_exit(); /* Abandon current thread. */
		/* NOTREACHED */
	} 
	/*
	 * Perform an upcall now.
	 *
	 * XXXKSE - Assumes we are going to userland, and not
	 * nested in the kernel.
	 */
	td->td_flags |= TDF_UPCALLING;
	mtx_unlock_spin(&sched_lock);
	PROC_UNLOCK(p);
	return (0);
}

/*
 * The extra work we go through if we are a threaded process when we
 * return to userland.
 *
 * If we are a KSE process and returning to user mode, check for
 * extra work to do before we return (e.g. for more syscalls
 * to complete first).  If we were in a critical section, we should
 * just return to let it finish. Same if we were in the UTS (in
 * which case the mailbox's context's busy indicator will be set).
 * The only traps we suport will have set the mailbox.
 * We will clear it here.
 */
int
thread_userret(struct thread *td, struct trapframe *frame)
{
	int error;
	int unbound;
	struct kse *ke;

	if (td->td_kse->ke_bound) {
		thread_export_context(td);
		PROC_LOCK(td->td_proc);
		mtx_lock_spin(&sched_lock);
		thread_exit();
	}

	/* Make the thread bound from now on, but remember what it was. */
	unbound = td->td_flags & TDF_UNBOUND;
	td->td_flags &= ~TDF_UNBOUND;
	/*
	 * Ensure that we have a spare thread available.
	 */
	ke = td->td_kse;
	if (ke->ke_tdspare == NULL) {
		mtx_lock(&Giant);
		ke->ke_tdspare = thread_alloc();
		mtx_unlock(&Giant);
	}
	/*
	 * Originally bound threads need no additional work.
	 */
	if (unbound == 0)
		return (0);
	error = 0;
	/*
	 * Decide whether or not we should perform an upcall now.
	 */
	if (((td->td_flags & TDF_UPCALLING) == 0) && unbound) {
		/* if we have other threads to run we will not return */
		if ((error = thread_consider_upcalling(td)))
			return (error); /* coundn't go async , just go sync. */
	}
	if (td->td_flags & TDF_UPCALLING) {
		/*
		 * There is no more work to do and we are going to ride
		 * this thead/KSE up to userland as an upcall.
		 */
		CTR3(KTR_PROC, "userret: upcall thread %p (pid %d, %s)",
		    td, td->td_proc->p_pid, td->td_proc->p_comm);

		/*
		 * Set user context to the UTS.
		 */
		cpu_set_upcall_kse(td, ke);

		/*
		 * Put any completed mailboxes on this KSE's list.
		 */
		error = thread_link_mboxes(td->td_ksegrp, ke);
		if (error)
			goto bad;

		/*
		 * Set state and mailbox.
		 */
		td->td_flags &= ~TDF_UPCALLING;
#if 0
		error = suword((caddr_t)ke->ke_mailbox +
		    offsetof(struct kse_mailbox, km_curthread),
		    0);
#else	/* if user pointer arithmetic is ok in the kernel */
		error = suword((caddr_t)&ke->ke_mailbox->km_curthread, 0);
#endif
		if (error)
			goto bad;
	}
	/*
	 * Stop any chance that we may be separated from
	 * the KSE we are currently on. This is "biting the bullet",
	 * we are committing to go to user space as as this KSE here.
	 */
	return (error);
bad:
	/*
	 * Things are going to be so screwed we should just kill the process.
 	 * how do we do that?
	 */
	 panic ("thread_userret.. need to kill proc..... how?");
}

/*
 * Enforce single-threading.
 *
 * Returns 1 if the caller must abort (another thread is waiting to
 * exit the process or similar). Process is locked!
 * Returns 0 when you are successfully the only thread running.
 * A process has successfully single threaded in the suspend mode when
 * There are no threads in user mode. Threads in the kernel must be
 * allowed to continue until they get to the user boundary. They may even
 * copy out their return values and data before suspending. They may however be
 * accellerated in reaching the user boundary as we will wake up
 * any sleeping threads that are interruptable. (PCATCH).
 */
int
thread_single(int force_exit)
{
	struct thread *td;
	struct thread *td2;
	struct proc *p;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	KASSERT((td != NULL), ("curthread is NULL"));

	if ((p->p_flag & P_KSES) == 0)
		return (0);

	/* Is someone already single threading? */
	if (p->p_singlethread) 
		return (1);

	if (force_exit == SINGLE_EXIT)
		p->p_flag |= P_SINGLE_EXIT;
	else
		p->p_flag &= ~P_SINGLE_EXIT;
	p->p_flag |= P_STOPPED_SINGLE;
	p->p_singlethread = td;
	while ((p->p_numthreads - p->p_suspcount) != 1) {
		mtx_lock_spin(&sched_lock);
		FOREACH_THREAD_IN_PROC(p, td2) {
			if (td2 == td)
				continue;
			if (TD_IS_INHIBITED(td2)) {
				if (TD_IS_SUSPENDED(td2)) {
					if (force_exit == SINGLE_EXIT) {
						thread_unsuspend_one(td2);
					}
				}
				if ( TD_IS_SLEEPING(td2)) {
					if (td2->td_flags & TDF_CVWAITQ)
						cv_waitq_remove(td2);
					else
						unsleep(td2);
					break;
				}
				if (TD_CAN_RUN(td2))
					setrunqueue(td2);
			}
		}
		/*
		 * Wake us up when everyone else has suspended.
		 * In the mean time we suspend as well.
		 */
		thread_suspend_one(td);
		mtx_unlock(&Giant);
		PROC_UNLOCK(p);
		mi_switch();
		mtx_unlock_spin(&sched_lock);
		mtx_lock(&Giant);
		PROC_LOCK(p);
	}
	return (0);
}

/*
 * Called in from locations that can safely check to see
 * whether we have to suspend or at least throttle for a
 * single-thread event (e.g. fork).
 *
 * Such locations include userret().
 * If the "return_instead" argument is non zero, the thread must be able to
 * accept 0 (caller may continue), or 1 (caller must abort) as a result.
 *
 * The 'return_instead' argument tells the function if it may do a
 * thread_exit() or suspend, or whether the caller must abort and back
 * out instead.
 *
 * If the thread that set the single_threading request has set the
 * P_SINGLE_EXIT bit in the process flags then this call will never return
 * if 'return_instead' is false, but will exit.
 *
 * P_SINGLE_EXIT | return_instead == 0| return_instead != 0
 *---------------+--------------------+---------------------
 *       0       | returns 0          |   returns 0 or 1
 *               | when ST ends       |   immediatly
 *---------------+--------------------+---------------------
 *       1       | thread exits       |   returns 1
 *               |                    |  immediatly
 * 0 = thread_exit() or suspension ok,
 * other = return error instead of stopping the thread.
 *
 * While a full suspension is under effect, even a single threading
 * thread would be suspended if it made this call (but it shouldn't).
 * This call should only be made from places where
 * thread_exit() would be safe as that may be the outcome unless 
 * return_instead is set.
 */
int
thread_suspend_check(int return_instead)
{
	struct thread *td;
	struct proc *p;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	while (P_SHOULDSTOP(p)) {
		if (P_SHOULDSTOP(p) == P_STOPPED_SINGLE) {
			KASSERT(p->p_singlethread != NULL,
			    ("singlethread not set"));
			/*
			 * The only suspension in action is a
			 * single-threading. Single threader need not stop.
			 * XXX Should be safe to access unlocked 
			 * as it can only be set to be true by us.
			 */
			if (p->p_singlethread == td)
				return (0);	/* Exempt from stopping. */
		} 
		if (return_instead)
			return (1);

		/*
		 * If the process is waiting for us to exit,
		 * this thread should just suicide.
		 * Assumes that P_SINGLE_EXIT implies P_STOPPED_SINGLE.
		 */
		if ((p->p_flag & P_SINGLE_EXIT) && (p->p_singlethread != td)) {
			mtx_lock_spin(&sched_lock);
			while (mtx_owned(&Giant))
				mtx_unlock(&Giant);
			thread_exit();
		}

		/*
		 * When a thread suspends, it just
		 * moves to the processes's suspend queue
		 * and stays there.
		 *
		 * XXXKSE if TDF_BOUND is true
		 * it will not release it's KSE which might
		 * lead to deadlock if there are not enough KSEs
		 * to complete all waiting threads.
		 * Maybe be able to 'lend' it out again.
		 * (lent kse's can not go back to userland?)
		 * and can only be lent in STOPPED state.
		 */
		mtx_lock_spin(&sched_lock);
		if ((p->p_flag & P_STOPPED_SIG) &&
		    (p->p_suspcount+1 == p->p_numthreads)) {
			mtx_unlock_spin(&sched_lock);
			PROC_LOCK(p->p_pptr);
			if ((p->p_pptr->p_procsig->ps_flag &
				PS_NOCLDSTOP) == 0) {
				psignal(p->p_pptr, SIGCHLD);
			}
			PROC_UNLOCK(p->p_pptr);
			mtx_lock_spin(&sched_lock);
		}
		mtx_assert(&Giant, MA_NOTOWNED);
		thread_suspend_one(td);
		PROC_UNLOCK(p);
		if (P_SHOULDSTOP(p) == P_STOPPED_SINGLE) {
			if (p->p_numthreads == p->p_suspcount) {
				thread_unsuspend_one(p->p_singlethread);
			}
		}
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		mtx_unlock_spin(&sched_lock);
		PROC_LOCK(p);
	}
	return (0);
}

void
thread_suspend_one(struct thread *td)
{
	struct proc *p = td->td_proc;

	mtx_assert(&sched_lock, MA_OWNED);
	p->p_suspcount++;
	TD_SET_SUSPENDED(td);
	TAILQ_INSERT_TAIL(&p->p_suspended, td, td_runq);
	/*
	 * Hack: If we are suspending but are on the sleep queue
	 * then we are in msleep or the cv equivalent. We
	 * want to look like we have two Inhibitors.
	 */
	if (TD_ON_SLEEPQ(td))
		TD_SET_SLEEPING(td);
}

void
thread_unsuspend_one(struct thread *td)
{
	struct proc *p = td->td_proc;

	mtx_assert(&sched_lock, MA_OWNED);
	TAILQ_REMOVE(&p->p_suspended, td, td_runq);
	TD_CLR_SUSPENDED(td);
	p->p_suspcount--;
	setrunnable(td);
}

/*
 * Allow all threads blocked by single threading to continue running.
 */
void
thread_unsuspend(struct proc *p)
{
	struct thread *td;

	mtx_assert(&sched_lock, MA_OWNED);
	PROC_LOCK_ASSERT(p, MA_OWNED);
	if (!P_SHOULDSTOP(p)) {
		while (( td = TAILQ_FIRST(&p->p_suspended))) {
			thread_unsuspend_one(td);
		}
	} else if ((P_SHOULDSTOP(p) == P_STOPPED_SINGLE) &&
	    (p->p_numthreads == p->p_suspcount)) {
		/*
		 * Stopping everything also did the job for the single
		 * threading request. Now we've downgraded to single-threaded,
		 * let it continue.
		 */
		thread_unsuspend_one(p->p_singlethread);
	}
}

void
thread_single_end(void)
{
	struct thread *td;
	struct proc *p;

	td = curthread;
	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	p->p_flag &= ~P_STOPPED_SINGLE;
	p->p_singlethread = NULL;
	/*
	 * If there are other threads they mey now run,
	 * unless of course there is a blanket 'stop order'
	 * on the process. The single threader must be allowed
	 * to continue however as this is a bad place to stop.
	 */
	if ((p->p_numthreads != 1) && (!P_SHOULDSTOP(p))) {
		mtx_lock_spin(&sched_lock);
		while (( td = TAILQ_FIRST(&p->p_suspended))) {
			thread_unsuspend_one(td);
		}
		mtx_unlock_spin(&sched_lock);
	}
}


