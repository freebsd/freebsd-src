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
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/filedesc.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/sx.h>
#include <sys/tty.h>
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
static int thread_debug = 0;
SYSCTL_INT(_kern_threads, OID_AUTO, debug, CTLFLAG_RW,
	&thread_debug, 0, "thread debug");

static int max_threads_per_proc = 30;
SYSCTL_INT(_kern_threads, OID_AUTO, max_threads_per_proc, CTLFLAG_RW,
	&max_threads_per_proc, 0, "Limit on threads per proc");

static int max_groups_per_proc = 5;
SYSCTL_INT(_kern_threads, OID_AUTO, max_groups_per_proc, CTLFLAG_RW,
	&max_groups_per_proc, 0, "Limit on thread groups per proc");

#define RANGEOF(type, start, end) (offsetof(type, end) - offsetof(type, start))

struct threadqueue zombie_threads = TAILQ_HEAD_INITIALIZER(zombie_threads);
TAILQ_HEAD(, kse) zombie_kses = TAILQ_HEAD_INITIALIZER(zombie_kses);
TAILQ_HEAD(, ksegrp) zombie_ksegrps = TAILQ_HEAD_INITIALIZER(zombie_ksegrps);
struct mtx zombie_thread_lock;
MTX_SYSINIT(zombie_thread_lock, &zombie_thread_lock,
    "zombie_thread_lock", MTX_SPIN);

static void kse_purge(struct proc *p, struct thread *td);

/*
 * Prepare a thread for use.
 */
static void
thread_ctor(void *mem, int size, void *arg)
{
	struct thread	*td;

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

	td = (struct thread *)mem;
	mtx_lock(&Giant);
	pmap_new_thread(td, 0);
	mtx_unlock(&Giant);
	cpu_thread_setup(td);
	td->td_sched = (struct td_sched *)&td[1];
}

/*
 * Tear down type-stable parts of a thread (just before being discarded).
 */
static void
thread_fini(void *mem, int size)
{
	struct thread	*td;

	td = (struct thread *)mem;
	pmap_dispose_thread(td);
}
/*
 * Initialize type-stable parts of a kse (when newly created).
 */
static void
kse_init(void *mem, int size)
{
	struct kse	*ke;

	ke = (struct kse *)mem;
	ke->ke_sched = (struct ke_sched *)&ke[1];
}
/*
 * Initialize type-stable parts of a ksegrp (when newly created).
 */
static void
ksegrp_init(void *mem, int size)
{
	struct ksegrp	*kg;

	kg = (struct ksegrp *)mem;
	kg->kg_sched = (struct kg_sched *)&kg[1];
}

/* 
 * KSE is linked onto the idle queue.
 */
void
kse_link(struct kse *ke, struct ksegrp *kg)
{
	struct proc *p = kg->kg_proc;

	TAILQ_INSERT_HEAD(&kg->kg_kseq, ke, ke_kglist);
	kg->kg_kses++;
	ke->ke_state = KES_UNQUEUED;
	ke->ke_proc	= p;
	ke->ke_ksegrp	= kg;
	ke->ke_owner	= NULL;
	ke->ke_thread	= NULL;
	ke->ke_oncpu = NOCPU;
}

void
kse_unlink(struct kse *ke)
{
	struct ksegrp *kg;

	mtx_assert(&sched_lock, MA_OWNED);
	kg = ke->ke_ksegrp;

	TAILQ_REMOVE(&kg->kg_kseq, ke, ke_kglist);
	if (--kg->kg_kses == 0) {
			ksegrp_unlink(kg);
	}
	/*
	 * Aggregate stats from the KSE
	 */
	kse_stash(ke);
}

void
ksegrp_link(struct ksegrp *kg, struct proc *p)
{

	TAILQ_INIT(&kg->kg_threads);
	TAILQ_INIT(&kg->kg_runq);	/* links with td_runq */
	TAILQ_INIT(&kg->kg_slpq);	/* links with td_runq */
	TAILQ_INIT(&kg->kg_kseq);	/* all kses in ksegrp */
	TAILQ_INIT(&kg->kg_lq);		/* loan kses in ksegrp */
	kg->kg_proc	= p;
/* the following counters are in the -zero- section and may not need clearing */
	kg->kg_numthreads = 0;
	kg->kg_runnable = 0;
	kg->kg_kses = 0;
	kg->kg_loan_kses = 0;
	kg->kg_runq_kses = 0; /* XXXKSE change name */
/* link it in now that it's consistent */
	p->p_numksegrps++;
	TAILQ_INSERT_HEAD(&p->p_ksegrps, kg, kg_ksegrp);
}

void
ksegrp_unlink(struct ksegrp *kg)
{
	struct proc *p;

	mtx_assert(&sched_lock, MA_OWNED);
	p = kg->kg_proc;
	KASSERT(((kg->kg_numthreads == 0) && (kg->kg_kses == 0)),
	    ("kseg_unlink: residual threads or KSEs"));
	TAILQ_REMOVE(&p->p_ksegrps, kg, kg_ksegrp);
	p->p_numksegrps--;
	/*
	 * Aggregate stats from the KSE
	 */
	ksegrp_stash(kg);
}

/*
 * for a newly created process,
 * link up a the structure and its initial threads etc.
 */
void
proc_linkup(struct proc *p, struct ksegrp *kg,
			struct kse *ke, struct thread *td)
{

	TAILQ_INIT(&p->p_ksegrps);	     /* all ksegrps in proc */
	TAILQ_INIT(&p->p_threads);	     /* all threads in proc */
	TAILQ_INIT(&p->p_suspended);	     /* Threads suspended */
	p->p_numksegrps = 0;
	p->p_numthreads = 0;

	ksegrp_link(kg, p);
	kse_link(ke, kg);
	thread_link(td, kg);
}

int
kse_thr_interrupt(struct thread *td, struct kse_thr_interrupt_args *uap)
{
	struct proc *p;
	struct thread *td2;

	p = td->td_proc;
	/* KSE-enabled processes only, please. */
	if (!(p->p_flag & P_KSES))
		return (EINVAL);
	if (uap->tmbx == NULL)
		return (EINVAL);
	mtx_lock_spin(&sched_lock);
	FOREACH_THREAD_IN_PROC(p, td2) {
		if (td2->td_mailbox == uap->tmbx) {
			td2->td_flags |= TDF_INTERRUPT;
			if (TD_ON_SLEEPQ(td2) && (td2->td_flags & TDF_SINTR)) {
				if (td2->td_flags & TDF_CVWAITQ)
					cv_abort(td2);
				else
					abortsleep(td2);
			}	
			mtx_unlock_spin(&sched_lock);
			td->td_retval[0] = 0;
			td->td_retval[1] = 0;
			return (0);
		}
	}
	mtx_unlock_spin(&sched_lock);
	return (ESRCH);
}

int
kse_exit(struct thread *td, struct kse_exit_args *uap)
{
	struct proc *p;
	struct ksegrp *kg;

	p = td->td_proc;
	/* Only UTS can do the syscall */ 
	if (!(p->p_flag & P_KSES) || (td->td_mailbox != NULL))
		return (EINVAL);
	kg = td->td_ksegrp;
	/* serialize killing kse */
	PROC_LOCK(p);
	mtx_lock_spin(&sched_lock);
	if ((kg->kg_kses == 1) && (kg->kg_numthreads > 1)) {
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
		return (EDEADLK);
	}
	if ((p->p_numthreads == 1) && (p->p_numksegrps == 1)) {
		/* XXXSKE what if >1 KSE? check.... */
		p->p_flag &= ~P_KSES;
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
	} else {
		td->td_kse->ke_flags |= KEF_EXIT;
		thread_exit();
		/* NOTREACHED */
	}
	return (0);
}

/*
 * Either becomes an upcall or waits for an awakening event and
 * THEN becomes an upcall. Only error cases return.
 */
int
kse_release(struct thread * td, struct kse_release_args * uap)
{
	struct proc *p;
	struct ksegrp *kg;

	p = td->td_proc;
	kg = td->td_ksegrp;
	/*
	 * kse must have a mailbox ready for upcall, and only UTS can
	 * do the syscall.
	 */
	if (!(p->p_flag & P_KSES) ||
	    (td->td_mailbox != NULL) ||
	    (td->td_kse->ke_mailbox == NULL))
		return (EINVAL);

	PROC_LOCK(p);
	/* Change OURSELF to become an upcall. */
	td->td_flags = TDF_UPCALLING; /* BOUND */
	if (kg->kg_completed == NULL) {
	/* XXXKSE also look for waiting signals etc. */
		/*
		 * The KSE will however be lendable.
		 */
		mtx_lock_spin(&sched_lock);
		TD_SET_IDLE(td);
		PROC_UNLOCK(p);
		p->p_stats->p_ru.ru_nvcsw++;
		mi_switch();
		mtx_unlock_spin(&sched_lock);
	} else {
		PROC_UNLOCK(p);
	}
	return (0);
}

/* struct kse_wakeup_args {
	struct kse_mailbox *mbx;
}; */
int
kse_wakeup(struct thread *td, struct kse_wakeup_args *uap)
{
	struct proc *p;
	struct kse *ke;
	struct ksegrp *kg;
	struct thread *td2;

	p = td->td_proc;
	td2 = NULL;
	/* KSE-enabled processes only, please. */
	if (!(p->p_flag & P_KSES))
		return EINVAL;
	PROC_LOCK(p);
	if (uap->mbx) {
		FOREACH_KSEGRP_IN_PROC(p, kg) {
			FOREACH_KSE_IN_GROUP(kg, ke) {
				if (ke->ke_mailbox != uap->mbx) 
					continue;
				td2 = ke->ke_owner ;
				KASSERT((td2 != NULL),("KSE with no owner"));
				if (!TD_IS_IDLE(td2)) {
					/* Return silently if no longer idle */
					PROC_UNLOCK(p);
				        td->td_retval[0] = 0;
       					td->td_retval[1] = 0;
					return (0);
				}
				break;
			}	
			if (td2) {
				break;
			}
		}
	} else {
		/* 
		 * look for any idle KSE to resurrect.
		 */
		kg = td->td_ksegrp;
		mtx_lock_spin(&sched_lock);
		FOREACH_KSE_IN_GROUP(kg, ke) {
			td2 = ke->ke_owner;
			KASSERT((td2 != NULL),("KSE with no owner2"));
			if (TD_IS_IDLE(td2)) 
				break;
		}
	}
	if (td2) {
		mtx_lock_spin(&sched_lock);
		PROC_UNLOCK(p);
		TD_CLR_IDLE(td2);
		setrunnable(td2);
		mtx_unlock_spin(&sched_lock);
	        td->td_retval[0] = 0;
       		td->td_retval[1] = 0;
		return (0);
	}	
	mtx_unlock_spin(&sched_lock);
	PROC_UNLOCK(p);
	return (ESRCH);
}

/* 
 * No new KSEG: first call: use current KSE, don't schedule an upcall
 * All other situations, do allocate a new KSE and schedule an upcall on it.
 */
/* struct kse_create_args {
	struct kse_mailbox *mbx;
	int newgroup;
}; */
int
kse_create(struct thread *td, struct kse_create_args *uap)
{
	struct kse *newke;
	struct kse *ke;
	struct ksegrp *newkg;
	struct ksegrp *kg;
	struct proc *p;
	struct kse_mailbox mbx;
	int err;

	p = td->td_proc;
	if ((err = copyin(uap->mbx, &mbx, sizeof(mbx))))
		return (err);

	p->p_flag |= P_KSES; /* easier to just set it than to test and set */
	kg = td->td_ksegrp;
	if (uap->newgroup) {
		if (p->p_numksegrps >= max_groups_per_proc) 
			return (EPROCLIM);
		/* 
		 * If we want a new KSEGRP it doesn't matter whether
		 * we have already fired up KSE mode before or not.
		 * We put the process in KSE mode and create a new KSEGRP
		 * and KSE. If our KSE has not got a mailbox yet then
		 * that doesn't matter, just leave it that way. It will 
		 * ensure that this thread stay BOUND. It's possible
		 * that the call came form a threaded library and the main 
		 * program knows nothing of threads.
		 */
		newkg = ksegrp_alloc();
		bzero(&newkg->kg_startzero, RANGEOF(struct ksegrp,
		      kg_startzero, kg_endzero)); 
		bcopy(&kg->kg_startcopy, &newkg->kg_startcopy,
		      RANGEOF(struct ksegrp, kg_startcopy, kg_endcopy));
		newke = kse_alloc();
	} else {
		/* 
		 * Otherwise, if we have already set this KSE
		 * to have a mailbox, we want to make another KSE here,
		 * but only if there are not already the limit, which 
		 * is 1 per CPU max.
		 * 
		 * If the current KSE doesn't have a mailbox we just use it
		 * and give it one.
		 *
		 * Because we don't like to access
		 * the KSE outside of schedlock if we are UNBOUND,
		 * (because it can change if we are preempted by an interrupt) 
		 * we can deduce it as having a mailbox if we are UNBOUND,
		 * and only need to actually look at it if we are BOUND,
		 * which is safe.
		 */
		if ((td->td_flags & TDF_UNBOUND) || td->td_kse->ke_mailbox) {
			if (thread_debug == 0) { /* if debugging, allow more */
#ifdef SMP
			if (kg->kg_kses > mp_ncpus)
#endif
				return (EPROCLIM);
			}
			newke = kse_alloc();
		} else {
			newke = NULL;
		}
		newkg = NULL;
	}
	if (newke) {
		bzero(&newke->ke_startzero, RANGEOF(struct kse,
		      ke_startzero, ke_endzero));
#if 0
		bcopy(&ke->ke_startcopy, &newke->ke_startcopy,
		      RANGEOF(struct kse, ke_startcopy, ke_endcopy));
#endif
		/* For the first call this may not have been set */
		if (td->td_standin == NULL) {
			td->td_standin = thread_alloc();
		}
		mtx_lock_spin(&sched_lock);
		if (newkg) {
			if (p->p_numksegrps >= max_groups_per_proc) {
				mtx_unlock_spin(&sched_lock);
				ksegrp_free(newkg); 
				kse_free(newke);
				return (EPROCLIM);
			}
			ksegrp_link(newkg, p);
		}
		else
			newkg = kg;
		kse_link(newke, newkg);
		if (p->p_sflag & PS_NEEDSIGCHK)
			newke->ke_flags |= KEF_ASTPENDING;
		newke->ke_mailbox = uap->mbx;
		newke->ke_upcall = mbx.km_func;
		bcopy(&mbx.km_stack, &newke->ke_stack, sizeof(stack_t));
		thread_schedule_upcall(td, newke);
		mtx_unlock_spin(&sched_lock);
	} else {
		/*
		 * If we didn't allocate a new KSE then the we are using
		 * the exisiting (BOUND) kse.
		 */
		ke = td->td_kse;
		ke->ke_mailbox = uap->mbx;
		ke->ke_upcall = mbx.km_func;
		bcopy(&mbx.km_stack, &ke->ke_stack, sizeof(stack_t));
	}
	/*
	 * Fill out the KSE-mode specific fields of the new kse.
	 */

	td->td_retval[0] = 0;
	td->td_retval[1] = 0;
	return (0);
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
	thread_zone = uma_zcreate("THREAD", sched_sizeof_thread(),
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
	thread_zone = uma_zcreate("THREAD", sched_sizeof_thread(),
	    thread_ctor, thread_dtor, thread_init, thread_fini,
	    UMA_ALIGN_CACHE, UMA_ZONE_NOFREE);
	uma_prealloc(thread_zone, 512);		/* XXX arbitary */
#endif
	ksegrp_zone = uma_zcreate("KSEGRP", sched_sizeof_ksegrp(),
	    NULL, NULL, ksegrp_init, NULL,
	    UMA_ALIGN_CACHE, 0);
	kse_zone = uma_zcreate("KSE", sched_sizeof_kse(),
	    NULL, NULL, kse_init, NULL,
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
 * Stash an embarasingly extra kse into the zombie kse queue.
 */
void
kse_stash(struct kse *ke)
{
	mtx_lock_spin(&zombie_thread_lock);
	TAILQ_INSERT_HEAD(&zombie_kses, ke, ke_procq);
	mtx_unlock_spin(&zombie_thread_lock);
}

/*
 * Stash an embarasingly extra ksegrp into the zombie ksegrp queue.
 */
void
ksegrp_stash(struct ksegrp *kg)
{
	mtx_lock_spin(&zombie_thread_lock);
	TAILQ_INSERT_HEAD(&zombie_ksegrps, kg, kg_ksegrp);
	mtx_unlock_spin(&zombie_thread_lock);
}

/*
 * Reap zombie threads.
 */
void
thread_reap(void)
{
	struct thread *td_first, *td_next;
	struct kse *ke_first, *ke_next;
	struct ksegrp *kg_first, * kg_next;

	/*
	 * don't even bother to lock if none at this instant
	 * We really don't care about the next instant..
	 */
	if ((!TAILQ_EMPTY(&zombie_threads))
	    || (!TAILQ_EMPTY(&zombie_kses))
	    || (!TAILQ_EMPTY(&zombie_ksegrps))) {
		mtx_lock_spin(&zombie_thread_lock);
		td_first = TAILQ_FIRST(&zombie_threads);
		ke_first = TAILQ_FIRST(&zombie_kses);
		kg_first = TAILQ_FIRST(&zombie_ksegrps);
		if (td_first)
			TAILQ_INIT(&zombie_threads);
		if (ke_first)
			TAILQ_INIT(&zombie_kses);
		if (kg_first)
			TAILQ_INIT(&zombie_ksegrps);
		mtx_unlock_spin(&zombie_thread_lock);
		while (td_first) {
			td_next = TAILQ_NEXT(td_first, td_runq);
			thread_free(td_first);
			td_first = td_next;
		}
		while (ke_first) {
			ke_next = TAILQ_NEXT(ke_first, ke_procq);
			kse_free(ke_first);
			ke_first = ke_next;
		}
		while (kg_first) {
			kg_next = TAILQ_NEXT(kg_first, kg_ksegrp);
			ksegrp_free(kg_first);
			kg_first = kg_next;
		}
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

	cpu_thread_clean(td);
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
	uint temp;

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
	if (error) 
		goto bad;

	thread_getcontext(td, &uc);
	error = copyout(&uc, addr, sizeof(ucontext_t));
	if (error) 
		goto bad;

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
			error = EFAULT;
			goto bad;
		}
		PROC_LOCK(p);
		if (mbx == (uintptr_t)kg->kg_completed) {
			kg->kg_completed = td->td_mailbox;
			PROC_UNLOCK(p);
			break;
		}
		PROC_UNLOCK(p);
	}
	addr = (caddr_t)td->td_mailbox
		 + offsetof(struct kse_thr_mailbox, tm_sticks);
	temp = fuword(addr) + td->td_usticks;
	if (suword(addr, temp))
		goto bad;
	return (0);

bad:
	PROC_LOCK(p);
	psignal(p, SIGSEGV);
	PROC_UNLOCK(p);
	return (error);
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
 * This function should be called at statclock interrupt time
 */
int
thread_add_ticks_intr(int user, uint ticks)
{
	struct thread *td = curthread;
	struct kse *ke = td->td_kse;
	
	if (ke->ke_mailbox == NULL)
		return -1;
	if (user) {
		/* Current always do via ast() */
		ke->ke_flags |= KEF_ASTPENDING;
		ke->ke_uuticks += ticks;
	} else {
		if (td->td_mailbox != NULL)
			td->td_usticks += ticks;
		else 
			ke->ke_usticks += ticks;
	}
	return 0;
}

static int
thread_update_uticks(void)
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct kse *ke = td->td_kse;
	struct kse_thr_mailbox *tmbx;
	caddr_t addr;
	uint uticks, sticks;

	if (ke->ke_mailbox == NULL)
		return 0;

	uticks = ke->ke_uuticks;
	ke->ke_uuticks = 0;
	sticks = ke->ke_usticks;
	ke->ke_usticks = 0;
#if 0
	tmbx = (void *)fuword((caddr_t)ke->ke_mailbox
	    + offsetof(struct kse_mailbox, km_curthread));
#else /* if user pointer arithmetic is ok in the kernel */
	tmbx = (void *)fuword( (void *)&ke->ke_mailbox->km_curthread);
#endif
	if ((tmbx == NULL) || (tmbx == (void *)-1))
		return 0;
	if (uticks) {
		addr = (caddr_t)tmbx + offsetof(struct kse_thr_mailbox, tm_uticks);
		uticks += fuword(addr);
		if (suword(addr, uticks))
			goto bad;
	}
	if (sticks) {
		addr = (caddr_t)tmbx + offsetof(struct kse_thr_mailbox, tm_sticks);
		sticks += fuword(addr);
		if (suword(addr, sticks))
			goto bad;
	}
	return 0;
bad:
	PROC_LOCK(p);
	psignal(p, SIGSEGV);
	PROC_UNLOCK(p);
	return -1;
}

/*
 * Discard the current thread and exit from its context.
 *
 * Because we can't free a thread while we're operating under its context,
 * push the current thread into our CPU's deadthread holder. This means
 * we needn't worry about someone else grabbing our context before we
 * do a cpu_throw().
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

	if (td->td_standin != NULL) {
		thread_stash(td->td_standin);
		td->td_standin = NULL;
	}

	cpu_thread_exit(td);	/* XXXSMP */

	/*
	 * The last thread is left attached to the process
	 * So that the whole bundle gets recycled. Skip
	 * all this stuff.
	 */
	if (p->p_numthreads > 1) {
		/*
		 * Unlink this thread from its proc and the kseg.
		 * In keeping with the other structs we probably should
		 * have a thread_unlink() that does some of this but it
		 * would only be called from here (I think) so it would
		 * be a waste. (might be useful for proc_fini() as well.)
 		 */
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

		/* Reassign this thread's KSE. */
		ke->ke_state = KES_UNQUEUED;

		/* 
		 * Decide what to do with the KSE attached to this thread.
		 * XXX Possibly kse_reassign should do both cases as it already 
		 * does some of this.
		 */
		if (ke->ke_flags & KEF_EXIT) {
			KASSERT((ke->ke_owner == td),
		    	    ("thread_exit: KSE exiting with non-owner thread"));
			ke->ke_thread = NULL;
			td->td_kse = NULL;
			kse_unlink(ke);
		} else {
			TD_SET_EXITING(td);	/* definitly not runnable */
			kse_reassign(ke);
		}
		PROC_UNLOCK(p);
		td->td_state	= TDS_INACTIVE;
		td->td_proc	= NULL;
		td->td_ksegrp	= NULL;
		td->td_last_kse	= NULL;
		PCPU_SET(deadthread, td);
	} else {
		PROC_UNLOCK(p);
	}
	cpu_throw();
	/* NOTREACHED */
}

/* 
 * Do any thread specific cleanups that may be needed in wait()
 * called with Giant held, proc and schedlock not held.
 */
void
thread_wait(struct proc *p)
{
	struct thread *td;

	KASSERT((p->p_numthreads == 1), ("Muliple threads in wait1()"));
	KASSERT((p->p_numksegrps == 1), ("Muliple ksegrps in wait1()"));
	FOREACH_THREAD_IN_PROC(p, td) {
		if (td->td_standin != NULL) {
			thread_free(td->td_standin);
			td->td_standin = NULL;
		}
		cpu_thread_clean(td);
	}
	thread_reap();	/* check for zombie threads etc. */
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
	td->td_kse	= NULL;
}

void
kse_purge(struct proc *p, struct thread *td)
{
	/* XXXKSE think about this..
		may need to wake up threads on loan queue. */
	struct ksegrp *kg;

 	KASSERT(p->p_numthreads == 1, ("bad thread number"));
	mtx_lock_spin(&sched_lock);
	while ((kg = TAILQ_FIRST(&p->p_ksegrps)) != NULL) {
		TAILQ_REMOVE(&p->p_ksegrps, kg, kg_ksegrp);
		p->p_numksegrps--;
		KASSERT(((kg->kg_kses == 0) && (kg != td->td_ksegrp)) ||
		    ((kg->kg_kses == 1) && (kg == td->td_ksegrp)),
			("wrong kg_kses"));
		if (kg != td->td_ksegrp) {
			ksegrp_stash(kg);
		}
	}
	TAILQ_INSERT_HEAD(&p->p_ksegrps, td->td_ksegrp, kg_ksegrp);
	p->p_numksegrps++;
	mtx_unlock_spin(&sched_lock);
}


/*
 * Create a thread and schedule it for upcall on the KSE given.
 * Use our thread's standin so that we don't have to allocate one.
 */
struct thread *
thread_schedule_upcall(struct thread *td, struct kse *ke)
{
	struct thread *td2;
	int newkse;

	mtx_assert(&sched_lock, MA_OWNED);
	newkse = (ke != td->td_kse);

	/* 
	 * If the owner and kse are BOUND then that thread is planning to
	 * go to userland and upcalls are not expected. So don't make one.
	 * If it is not bound then make it so with the spare thread
	 * anf then borrw back the KSE to allow us to complete some in-kernel
	 * work. When we complete, the Bound thread will have the chance to 
	 * complete. This thread will sleep as planned. Hopefully there will
	 * eventually be un unbound thread that can be converted to an
	 * upcall to report the completion of this thread.
	 */

	if ((td2 = td->td_standin) != NULL) {
		td->td_standin = NULL;
	} else {
		if (newkse)
			panic("no reserve thread when called with a new kse");
		/*
		 * If called from (e.g.) sleep and we do not have
		 * a reserve thread, then we've used it, so do not
		 * create an upcall.
		 */
		return (NULL);
	}
	CTR3(KTR_PROC, "thread_schedule_upcall: thread %p (pid %d, %s)",
	     td2, td->td_proc->p_pid, td->td_proc->p_comm);
	bzero(&td2->td_startzero,
	    (unsigned)RANGEOF(struct thread, td_startzero, td_endzero));
	bcopy(&td->td_startcopy, &td2->td_startcopy,
	    (unsigned) RANGEOF(struct thread, td_startcopy, td_endcopy));
	thread_link(td2, ke->ke_ksegrp);
	cpu_set_upcall(td2, td->td_pcb);

	/*
	 * XXXKSE do we really need this? (default values for the
	 * frame).
	 */
	bcopy(td->td_frame, td2->td_frame, sizeof(struct trapframe));

	/*
	 * Bind the new thread to the KSE,
	 * and if it's our KSE, lend it back to ourself
	 * so we can continue running.
	 */
	td2->td_ucred = crhold(td->td_ucred);
	td2->td_flags = TDF_UPCALLING; /* note: BOUND */
	td2->td_kse = ke;
	td2->td_state = TDS_CAN_RUN;
	td2->td_inhibitors = 0;
	ke->ke_owner = td2;
	/*
	 * If called from kse_reassign(), we are working on the current
	 * KSE so fake that we borrowed it. If called from
	 * kse_create(), don't, as we have a new kse too.
	 */
	if (!newkse) {
		/*
		 * This thread will be scheduled when the current thread
		 * blocks, exits or tries to enter userspace, (which ever
		 * happens first). When that happens the KSe will "revert"
		 * to this thread in a BOUND manner. Since we are called
		 * from msleep() this is going to be "very soon" in nearly
		 * all cases.
		 */
		TD_SET_LOAN(td2);
	} else {
		ke->ke_thread = td2;
		ke->ke_state = KES_THREAD;
		setrunqueue(td2);
	}
	return (td2);	/* bogus.. should be a void function */
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
return (NULL);

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
	if (td->td_standin == NULL)
		td->td_standin = thread_alloc();
	mtx_lock_spin(&sched_lock);
	td2 = thread_schedule_upcall(td, ke); /* Bogus JRE */
	mtx_unlock_spin(&sched_lock);
	return (td2);
}

/*
 * setup done on the thread when it enters the kernel.
 * XXXKSE Presently only for syscalls but eventually all kernel entries.
 */
void
thread_user_enter(struct proc *p, struct thread *td)
{
	struct kse *ke;

	/*
	 * First check that we shouldn't just abort.
	 * But check if we are the single thread first!
	 * XXX p_singlethread not locked, but should be safe.
	 */
	if ((p->p_flag & P_WEXIT) && (p->p_singlethread != td)) {
		PROC_LOCK(p);
		mtx_lock_spin(&sched_lock);
		thread_exit();
		/* NOTREACHED */
	}

	/*
	 * If we are doing a syscall in a KSE environment,
	 * note where our mailbox is. There is always the
	 * possibility that we could do this lazily (in kse_reassign()),
	 * but for now do it every time.
	 */
	ke = td->td_kse;
	td->td_flags &= ~TDF_UNBOUND;
	if (ke->ke_mailbox != NULL) {
#if 0
		td->td_mailbox = (void *)fuword((caddr_t)ke->ke_mailbox
		    + offsetof(struct kse_mailbox, km_curthread));
#else /* if user pointer arithmetic is ok in the kernel */
		td->td_mailbox =
		    (void *)fuword( (void *)&ke->ke_mailbox->km_curthread);
#endif
		if ((td->td_mailbox == NULL) ||
		    (td->td_mailbox == (void *)-1)) {
			td->td_mailbox = NULL;	/* single thread it.. */
			mtx_lock_spin(&sched_lock);
			td->td_flags &= ~(TDF_UNBOUND|TDF_CAN_UNBIND);
			mtx_unlock_spin(&sched_lock);
		} else {
			/* 
			 * when thread limit reached, act like that the thread
			 * has already done an upcall.
			 */
		    	if (p->p_numthreads > max_threads_per_proc) {
				if (td->td_standin != NULL) {
					thread_stash(td->td_standin);
					td->td_standin = NULL;
				}	
			} else {
				if (td->td_standin == NULL)
					td->td_standin = thread_alloc();
			}
			mtx_lock_spin(&sched_lock);
			td->td_flags |= TDF_CAN_UNBIND;
			mtx_unlock_spin(&sched_lock);
			KASSERT((ke->ke_owner == td),
			    ("thread_user_enter: No starting owner "));
			ke->ke_owner = td;
			td->td_usticks = 0;
		}
	}
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
	struct ksegrp *kg;
	struct thread *worktodo;
	struct proc *p;
	struct timespec ts;

	KASSERT((td->td_kse && td->td_kse->ke_thread && td->td_kse->ke_owner),
	    ("thread_userret: bad thread/kse pointers"));
	KASSERT((td == curthread),
	    ("thread_userret: bad thread argument"));


	kg = td->td_ksegrp;
	p = td->td_proc;
	error = 0;
	unbound = TD_IS_UNBOUND(td);

	mtx_lock_spin(&sched_lock);
       	if ((worktodo = kg->kg_last_assigned))
       		worktodo = TAILQ_NEXT(worktodo, td_runq);
       	else
       		worktodo = TAILQ_FIRST(&kg->kg_runq);

	/*
	 * Permanently bound threads never upcall but they may 
	 * loan out their KSE at this point.
	 * Upcalls imply bound.. They also may want to do some Philantropy.
	 * Temporarily bound threads on the other hand either yield
	 * to other work and transform into an upcall, or proceed back to
	 * userland.
	 */

	if (TD_CAN_UNBIND(td)) {
		td->td_flags &= ~(TDF_UNBOUND|TDF_CAN_UNBIND);
		if (!worktodo && (kg->kg_completed == NULL)) {
			/*
			 * This thread has not started any upcall.
			 * If there is no work to report other than
			 * ourself, then it can return direct to userland.
			 */
justreturn:
			mtx_unlock_spin(&sched_lock);
			thread_update_uticks();
			td->td_mailbox = NULL;
			return (0);
		}
		mtx_unlock_spin(&sched_lock);
		error = thread_export_context(td);
		td->td_usticks = 0;
		if (error) {
			/*
			 * As we are not running on a borrowed KSE,
			 * failing to do the KSE operation just defaults
			 * back to synchonous operation, so just return from
			 * the syscall.
			 */
			goto justreturn;
		}
		mtx_lock_spin(&sched_lock);
		/*
		 * Turn ourself into a bound upcall.
		 * We will rely on kse_reassign()
		 * to make us run at a later time.
		 */
		td->td_flags |= TDF_UPCALLING;

		/* there may be more work since we re-locked schedlock */
       		if ((worktodo = kg->kg_last_assigned))
       			worktodo = TAILQ_NEXT(worktodo, td_runq);
       		else
       			worktodo = TAILQ_FIRST(&kg->kg_runq);
	} else if (unbound) {
		/*
		 * We are an unbound thread, looking to
		 * return to user space. There must be another owner
		 * of this KSE.
		 * We are using a borrowed KSE. save state and exit.
		 * kse_reassign() will recycle the kse as needed,
		 */
		mtx_unlock_spin(&sched_lock);
		error = thread_export_context(td);
		td->td_usticks = 0;
		if (error) {
			/*
			 * There is nothing we can do.
			 * We just lose that context. We
			 * probably should note this somewhere and send
			 * the process a signal.
			 */
			PROC_LOCK(td->td_proc);
			psignal(td->td_proc, SIGSEGV);
			mtx_lock_spin(&sched_lock);
			ke = td->td_kse;
			/* possibly upcall with error? */
		} else {
			/*
			 * Don't make an upcall, just exit so that the owner
			 * can get its KSE if it wants it.
			 * Our context is already safely stored for later
			 * use by the UTS.
			 */
			PROC_LOCK(p);
			mtx_lock_spin(&sched_lock);
			ke = td->td_kse;
		}
		/* 
		 * If the owner is idling, we now have something for it
		 * to report, so make it runnable.
		 * If the owner is not an upcall, make an attempt to
		 * ensure that at least one of any IDLED upcalls can
		 * wake up.
		 */
		if (ke->ke_owner->td_flags & TDF_UPCALLING) {
			TD_CLR_IDLE(ke->ke_owner);
		} else {
			FOREACH_KSE_IN_GROUP(kg, ke) {	
				if (TD_IS_IDLE(ke->ke_owner)) {
					TD_CLR_IDLE(ke->ke_owner);
				}
			}
		}
		thread_exit();
	}
	/* 
	 * We ARE going back to userland with this KSE.
	 * We are permanently bound. We may be an upcall.
	 * If an upcall, check for threads that need to borrow the KSE.
	 * Any other thread that comes ready after this missed the boat.
	 */
	ke = td->td_kse;

	/*
	 *  If not upcalling, go back to userspace.
	 * If we are, get the upcall set up.
	 */
	if (td->td_flags & TDF_UPCALLING) {
		if (worktodo)  {
			/* 
			 * force a switch to more urgent 'in kernel'
			 * work. Control will return to this thread
			 * when there is no more work to do.
			 * kse_reassign() will do that for us.
			 */
			TD_SET_LOAN(td);  /* XXXKSE may not be needed */
			p->p_stats->p_ru.ru_nvcsw++;
			mi_switch(); /* kse_reassign() will (re)find worktodo */
		}
		td->td_flags &= ~TDF_UPCALLING;
		mtx_unlock_spin(&sched_lock);

		/* 
		 * There is no more work to do and we are going to ride
		 * this thread/KSE up to userland as an upcall.
		 * Do the last parts of the setup needed for the upcall.
		 */
		CTR3(KTR_PROC, "userret: upcall thread %p (pid %d, %s)",
		    td, td->td_proc->p_pid, td->td_proc->p_comm);

		/*
		 * Set user context to the UTS.
		 * Will use Giant in cpu_thread_clean() because it uses
		 * kmem_free(kernel_map, ...)
		 */
		cpu_set_upcall_kse(td, ke);

		/* 
		 * Unhook the list of completed threads.
		 * anything that completes after this gets to 
		 * come in next time.
		 * Put the list of completed thread mailboxes on
		 * this KSE's mailbox.
		 */
		error = thread_link_mboxes(kg, ke);
		if (error) 
			goto bad;

		/*
		 * Set state and clear the  thread mailbox pointer.
		 * From now on we are just a bound outgoing process.
		 * **Problem** userret is often called several times.
		 * it would be nice if this all happenned only on the first
		 * time through. (the scan for extra work etc.)
		 */
#if 0
		error = suword((caddr_t)ke->ke_mailbox +
		    offsetof(struct kse_mailbox, km_curthread), 0);
#else	/* if user pointer arithmetic is ok in the kernel */
		error = suword((caddr_t)&ke->ke_mailbox->km_curthread, 0);
#endif
		ke->ke_uuticks = ke->ke_usticks = 0;
		if (error) 
			goto bad;
		nanotime(&ts);
		if (copyout(&ts,
		    (caddr_t)&ke->ke_mailbox->km_timeofday, sizeof(ts))) {
			goto bad;
		}
	} else {
		mtx_unlock_spin(&sched_lock);
	}
	/*
	 * Optimisation:
	 * Ensure that we have a spare thread available,
	 * for when we re-enter the kernel.
	 */
	if (td->td_standin == NULL) {
		td->td_standin = thread_alloc();
	}

	thread_update_uticks();
	td->td_mailbox = NULL;
	return (0);

bad:
	/*
	 * Things are going to be so screwed we should just kill the process.
 	 * how do we do that?
	 */
	PROC_LOCK(td->td_proc);
	psignal(td->td_proc, SIGSEGV);
	PROC_UNLOCK(td->td_proc);
	td->td_mailbox = NULL;
	return (error);	/* go sync */
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
	mtx_assert(&Giant, MA_OWNED);
	PROC_LOCK_ASSERT(p, MA_OWNED);
	KASSERT((td != NULL), ("curthread is NULL"));

	if ((p->p_flag & P_KSES) == 0)
		return (0);

	/* Is someone already single threading? */
	if (p->p_singlethread) 
		return (1);

	if (force_exit == SINGLE_EXIT) {
		p->p_flag |= P_SINGLE_EXIT;
		td->td_flags &= ~TDF_UNBOUND;
	} else
		p->p_flag &= ~P_SINGLE_EXIT;
	p->p_flag |= P_STOPPED_SINGLE;
	p->p_singlethread = td;
	/* XXXKSE Which lock protects the below values? */
	while ((p->p_numthreads - p->p_suspcount) != 1) {
		mtx_lock_spin(&sched_lock);
		FOREACH_THREAD_IN_PROC(p, td2) {
			if (td2 == td)
				continue;
			if (TD_IS_INHIBITED(td2)) {
				if (force_exit == SINGLE_EXIT) {
					if (TD_IS_SUSPENDED(td2)) {
						thread_unsuspend_one(td2);
					}
					if (TD_ON_SLEEPQ(td2) &&
					    (td2->td_flags & TDF_SINTR)) {
						if (td2->td_flags & TDF_CVWAITQ)
							cv_abort(td2);
						else
							abortsleep(td2);
					}
					if (TD_IS_IDLE(td2)) {
						TD_CLR_IDLE(td2);
					}
				} else {
					if (TD_IS_SUSPENDED(td2))
						continue;
					/* maybe other inhibitted states too? */
					if (td2->td_inhibitors &
					    (TDI_SLEEPING | TDI_SWAPPED |
					    TDI_LOAN | TDI_IDLE |
					    TDI_EXITING))
						thread_suspend_one(td2);
				}
			}
		}
		/* 
		 * Maybe we suspended some threads.. was it enough? 
		 */
		if ((p->p_numthreads - p->p_suspcount) == 1) {
			mtx_unlock_spin(&sched_lock);
			break;
		}

		/*
		 * Wake us up when everyone else has suspended.
		 * In the mean time we suspend as well.
		 */
		thread_suspend_one(td);
		mtx_unlock(&Giant);
		PROC_UNLOCK(p);
		p->p_stats->p_ru.ru_nvcsw++;
		mi_switch();
		mtx_unlock_spin(&sched_lock);
		mtx_lock(&Giant);
		PROC_LOCK(p);
	}
	if (force_exit == SINGLE_EXIT)
		kse_purge(p, td);
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
	struct kse *ke;
	struct ksegrp *kg;

	td = curthread;
	p = td->td_proc;
	kg = td->td_ksegrp;
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
			/* 
			 * All threads should be exiting
			 * Unless they are the active "singlethread".
			 * destroy un-needed KSEs as we go..
			 * KSEGRPS may implode too as #kses -> 0.
			 */
			ke = td->td_kse;
			if (ke->ke_owner == td &&
			    (kg->kg_kses >= kg->kg_numthreads ))
				ke->ke_flags |= KEF_EXIT;
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
	 * May already be set.. doesn't matter.
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


