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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
static uma_zone_t upcall_zone;

/* DEBUG ONLY */
SYSCTL_NODE(_kern, OID_AUTO, threads, CTLFLAG_RW, 0, "thread allocation");
static int thread_debug = 0;
SYSCTL_INT(_kern_threads, OID_AUTO, debug, CTLFLAG_RW,
	&thread_debug, 0, "thread debug");

static int max_threads_per_proc = 150;
SYSCTL_INT(_kern_threads, OID_AUTO, max_threads_per_proc, CTLFLAG_RW,
	&max_threads_per_proc, 0, "Limit on threads per proc");

static int max_groups_per_proc = 50;
SYSCTL_INT(_kern_threads, OID_AUTO, max_groups_per_proc, CTLFLAG_RW,
	&max_groups_per_proc, 0, "Limit on thread groups per proc");

static int max_threads_hits;
SYSCTL_INT(_kern_threads, OID_AUTO, max_threads_hits, CTLFLAG_RD,
	&max_threads_hits, 0, "");

static int virtual_cpu;

#define RANGEOF(type, start, end) (offsetof(type, end) - offsetof(type, start))

TAILQ_HEAD(, thread) zombie_threads = TAILQ_HEAD_INITIALIZER(zombie_threads);
TAILQ_HEAD(, kse) zombie_kses = TAILQ_HEAD_INITIALIZER(zombie_kses);
TAILQ_HEAD(, ksegrp) zombie_ksegrps = TAILQ_HEAD_INITIALIZER(zombie_ksegrps);
TAILQ_HEAD(, kse_upcall) zombie_upcalls = 
	TAILQ_HEAD_INITIALIZER(zombie_upcalls);
struct mtx kse_zombie_lock;
MTX_SYSINIT(kse_zombie_lock, &kse_zombie_lock, "kse zombie lock", MTX_SPIN);

static void kse_purge(struct proc *p, struct thread *td);
static void kse_purge_group(struct thread *td);
static int thread_update_usr_ticks(struct thread *td, int user);
static void thread_alloc_spare(struct thread *td, struct thread *spare);

static int
sysctl_kse_virtual_cpu(SYSCTL_HANDLER_ARGS)
{
	int error, new_val;
	int def_val;

#ifdef SMP
	def_val = mp_ncpus;
#else
	def_val = 1;
#endif
	if (virtual_cpu == 0)
		new_val = def_val;
	else
		new_val = virtual_cpu;
	error = sysctl_handle_int(oidp, &new_val, 0, req);
        if (error != 0 || req->newptr == NULL)
		return (error);
	if (new_val < 0)
		return (EINVAL);
	virtual_cpu = new_val;
	return (0);
}

/* DEBUG ONLY */
SYSCTL_PROC(_kern_threads, OID_AUTO, virtual_cpu, CTLTYPE_INT|CTLFLAG_RW,
	0, sizeof(virtual_cpu), sysctl_kse_virtual_cpu, "I",
	"debug virtual cpus");

/*
 * Prepare a thread for use.
 */
static void
thread_ctor(void *mem, int size, void *arg)
{
	struct thread	*td;

	td = (struct thread *)mem;
	td->td_state = TDS_INACTIVE;
	td->td_oncpu	= NOCPU;
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
 * KSE is linked into kse group.
 */
void
kse_link(struct kse *ke, struct ksegrp *kg)
{
	struct proc *p = kg->kg_proc;

	TAILQ_INSERT_HEAD(&kg->kg_kseq, ke, ke_kglist);
	kg->kg_kses++;
	ke->ke_state	= KES_UNQUEUED;
	ke->ke_proc	= p;
	ke->ke_ksegrp	= kg;
	ke->ke_thread	= NULL;
	ke->ke_oncpu	= NOCPU;
	ke->ke_flags	= 0;
}

void
kse_unlink(struct kse *ke)
{
	struct ksegrp *kg;

	mtx_assert(&sched_lock, MA_OWNED);
	kg = ke->ke_ksegrp;
	TAILQ_REMOVE(&kg->kg_kseq, ke, ke_kglist);
	if (ke->ke_state == KES_IDLE) {
		TAILQ_REMOVE(&kg->kg_iq, ke, ke_kgrlist);
		kg->kg_idle_kses--;
	}
	if (--kg->kg_kses == 0)
		ksegrp_unlink(kg);
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
	TAILQ_INIT(&kg->kg_iq);		/* all idle kses in ksegrp */
	TAILQ_INIT(&kg->kg_upcalls);	/* all upcall structure in ksegrp */
	kg->kg_proc = p;
	/*
	 * the following counters are in the -zero- section
	 * and may not need clearing
	 */
	kg->kg_numthreads = 0;
	kg->kg_runnable   = 0;
	kg->kg_kses       = 0;
	kg->kg_runq_kses  = 0; /* XXXKSE change name */
	kg->kg_idle_kses  = 0;
	kg->kg_numupcalls = 0;
	/* link it in now that it's consistent */
	p->p_numksegrps++;
	TAILQ_INSERT_HEAD(&p->p_ksegrps, kg, kg_ksegrp);
}

void
ksegrp_unlink(struct ksegrp *kg)
{
	struct proc *p;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT((kg->kg_numthreads == 0), ("ksegrp_unlink: residual threads"));
	KASSERT((kg->kg_kses == 0), ("ksegrp_unlink: residual kses"));
	KASSERT((kg->kg_numupcalls == 0), ("ksegrp_unlink: residual upcalls"));

	p = kg->kg_proc;
	TAILQ_REMOVE(&p->p_ksegrps, kg, kg_ksegrp);
	p->p_numksegrps--;
	/*
	 * Aggregate stats from the KSE
	 */
	ksegrp_stash(kg);
}

struct kse_upcall *
upcall_alloc(void)
{
	struct kse_upcall *ku;

	ku = uma_zalloc(upcall_zone, M_WAITOK);
	bzero(ku, sizeof(*ku));
	return (ku);
}

void
upcall_free(struct kse_upcall *ku)
{

	uma_zfree(upcall_zone, ku);
}

void
upcall_link(struct kse_upcall *ku, struct ksegrp *kg)
{

	mtx_assert(&sched_lock, MA_OWNED);
	TAILQ_INSERT_TAIL(&kg->kg_upcalls, ku, ku_link);
	ku->ku_ksegrp = kg;
	kg->kg_numupcalls++;
}

void
upcall_unlink(struct kse_upcall *ku)
{
	struct ksegrp *kg = ku->ku_ksegrp;

	mtx_assert(&sched_lock, MA_OWNED);
	KASSERT(ku->ku_owner == NULL, ("%s: have owner", __func__));
	TAILQ_REMOVE(&kg->kg_upcalls, ku, ku_link); 
	kg->kg_numupcalls--;
	upcall_stash(ku);
}

void
upcall_remove(struct thread *td)
{

	if (td->td_upcall) {
		td->td_upcall->ku_owner = NULL;
		upcall_unlink(td->td_upcall);
		td->td_upcall = 0;
	} 
}

/*
 * For a newly created process,
 * link up all the structures and its initial threads etc.
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

/*
struct kse_thr_interrupt_args {
	struct kse_thr_mailbox * tmbx;
};
*/
int
kse_thr_interrupt(struct thread *td, struct kse_thr_interrupt_args *uap)
{
	struct proc *p;
	struct thread *td2;

	p = td->td_proc;
	if (!(p->p_flag & P_THREADED) || (uap->tmbx == NULL))
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
			return (0);
		}
	}
	mtx_unlock_spin(&sched_lock);
	return (ESRCH);
}

/*
struct kse_exit_args {
	register_t dummy;
};
*/
int
kse_exit(struct thread *td, struct kse_exit_args *uap)
{
	struct proc *p;
	struct ksegrp *kg;
	struct kse *ke;
	struct kse_upcall *ku, *ku2;
	int    error, count;

	p = td->td_proc;
	if ((ku = td->td_upcall) == NULL || TD_CAN_UNBIND(td))
		return (EINVAL);
	kg = td->td_ksegrp;
	count = 0;
	PROC_LOCK(p);
	mtx_lock_spin(&sched_lock);
	FOREACH_UPCALL_IN_GROUP(kg, ku2) {
		if (ku2->ku_flags & KUF_EXITING)
			count++;
	}
	if ((kg->kg_numupcalls - count) == 1 &&
	    (kg->kg_numthreads > 1)) {
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
		return (EDEADLK);
	}
	ku->ku_flags |= KUF_EXITING;
	mtx_unlock_spin(&sched_lock);
	PROC_UNLOCK(p);
	error = suword(&ku->ku_mailbox->km_flags, ku->ku_mflags|KMF_DONE);
	PROC_LOCK(p);
	if (error)
		psignal(p, SIGSEGV);
	mtx_lock_spin(&sched_lock);
	upcall_remove(td);
	ke = td->td_kse;
	if (p->p_numthreads == 1) {
		kse_purge(p, td);
		p->p_flag &= ~P_THREADED;
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
	} else {
		if (kg->kg_numthreads == 1) { /* Shutdown a group */
			kse_purge_group(td);
			ke->ke_flags |= KEF_EXIT;
		}
		thread_stopped(p);
		thread_exit();
		/* NOTREACHED */
	}
	return (0);
}

/*
 * Either becomes an upcall or waits for an awakening event and
 * then becomes an upcall. Only error cases return.
 */
/*
struct kse_release_args {
	struct timespec *timeout;
};
*/
int
kse_release(struct thread *td, struct kse_release_args *uap)
{
	struct proc *p;
	struct ksegrp *kg;
	struct timespec ts, ts2, ts3, timeout;
	struct timeval tv;
	int error;

	p = td->td_proc;
	kg = td->td_ksegrp;
	if (td->td_upcall == NULL || TD_CAN_UNBIND(td))
		return (EINVAL);
	if (uap->timeout != NULL) {
		if ((error = copyin(uap->timeout, &timeout, sizeof(timeout))))
			return (error);
		getnanouptime(&ts);
		timespecadd(&ts, &timeout);
		TIMESPEC_TO_TIMEVAL(&tv, &timeout);
	}
	mtx_lock_spin(&sched_lock);
	/* Change OURSELF to become an upcall. */
	td->td_flags = TDF_UPCALLING;
#if 0	/* XXX This shouldn't be necessary */
	if (p->p_sflag & PS_NEEDSIGCHK)
		td->td_flags |= TDF_ASTPENDING;
#endif
	mtx_unlock_spin(&sched_lock);
	PROC_LOCK(p);
	while ((td->td_upcall->ku_flags & KUF_DOUPCALL) == 0 &&
	       (kg->kg_completed == NULL)) {
		kg->kg_upsleeps++;
		error = msleep(&kg->kg_completed, &p->p_mtx, PPAUSE|PCATCH,
			"kse_rel", (uap->timeout ? tvtohz(&tv) : 0));
		kg->kg_upsleeps--;
		PROC_UNLOCK(p);
		if (uap->timeout == NULL || error != EWOULDBLOCK)
			return (0);
		getnanouptime(&ts2);
		if (timespeccmp(&ts2, &ts, >=))
			return (0);
		ts3 = ts;
		timespecsub(&ts3, &ts2);
		TIMESPEC_TO_TIMEVAL(&tv, &ts3);
		PROC_LOCK(p);
	}
	PROC_UNLOCK(p);
	return (0);
}

/* struct kse_wakeup_args {
	struct kse_mailbox *mbx;
}; */
int
kse_wakeup(struct thread *td, struct kse_wakeup_args *uap)
{
	struct proc *p;
	struct ksegrp *kg;
	struct kse_upcall *ku;
	struct thread *td2;

	p = td->td_proc;
	td2 = NULL;
	ku = NULL;
	/* KSE-enabled processes only, please. */
	if (!(p->p_flag & P_THREADED))
		return (EINVAL);
	PROC_LOCK(p);
	mtx_lock_spin(&sched_lock);
	if (uap->mbx) {
		FOREACH_KSEGRP_IN_PROC(p, kg) {
			FOREACH_UPCALL_IN_GROUP(kg, ku) {
				if (ku->ku_mailbox == uap->mbx)
					break;
			}
			if (ku)
				break;
		}
	} else {
		kg = td->td_ksegrp;
		if (kg->kg_upsleeps) {
			wakeup_one(&kg->kg_completed);
			mtx_unlock_spin(&sched_lock);
			PROC_UNLOCK(p);
			return (0);
		}
		ku = TAILQ_FIRST(&kg->kg_upcalls);
	}
	if (ku) {
		if ((td2 = ku->ku_owner) == NULL) {
			panic("%s: no owner", __func__);
		} else if (TD_ON_SLEEPQ(td2) &&
		           (td2->td_wchan == &kg->kg_completed)) {
			abortsleep(td2);
		} else {
			ku->ku_flags |= KUF_DOUPCALL;
		}
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
		return (0);
	}
	mtx_unlock_spin(&sched_lock);
	PROC_UNLOCK(p);
	return (ESRCH);
}

/* 
 * No new KSEG: first call: use current KSE, don't schedule an upcall
 * All other situations, do allocate max new KSEs and schedule an upcall.
 */
/* struct kse_create_args {
	struct kse_mailbox *mbx;
	int newgroup;
}; */
int
kse_create(struct thread *td, struct kse_create_args *uap)
{
	struct kse *newke;
	struct ksegrp *newkg;
	struct ksegrp *kg;
	struct proc *p;
	struct kse_mailbox mbx;
	struct kse_upcall *newku;
	int err, ncpus;

	p = td->td_proc;
	if ((err = copyin(uap->mbx, &mbx, sizeof(mbx))))
		return (err);

	/* Too bad, why hasn't kernel always a cpu counter !? */
#ifdef SMP
	ncpus = mp_ncpus;
#else
	ncpus = 1;
#endif
	if (thread_debug && virtual_cpu != 0)
		ncpus = virtual_cpu;

	/* Easier to just set it than to test and set */
	PROC_LOCK(p);
	p->p_flag |= P_THREADED;
	PROC_UNLOCK(p);
	kg = td->td_ksegrp;
	if (uap->newgroup) {
		/* Have race condition but it is cheap */ 
		if (p->p_numksegrps >= max_groups_per_proc) 
			return (EPROCLIM);
		/* 
		 * If we want a new KSEGRP it doesn't matter whether
		 * we have already fired up KSE mode before or not.
		 * We put the process in KSE mode and create a new KSEGRP.
		 */
		newkg = ksegrp_alloc();
		bzero(&newkg->kg_startzero, RANGEOF(struct ksegrp,
		      kg_startzero, kg_endzero));
		bcopy(&kg->kg_startcopy, &newkg->kg_startcopy,
		      RANGEOF(struct ksegrp, kg_startcopy, kg_endcopy));
		mtx_lock_spin(&sched_lock);
		if (p->p_numksegrps >= max_groups_per_proc) {
			mtx_unlock_spin(&sched_lock);
			ksegrp_free(newkg);
			return (EPROCLIM);
		}
		ksegrp_link(newkg, p);
		mtx_unlock_spin(&sched_lock);
	} else {
		newkg = kg;
	}

	/*
	 * Creating upcalls more than number of physical cpu does
	 * not help performance. 
	 */
	if (newkg->kg_numupcalls >= ncpus)
		return (EPROCLIM);

	if (newkg->kg_numupcalls == 0) {
		/*
		 * Initialize KSE group, optimized for MP.
		 * Create KSEs as many as physical cpus, this increases
		 * concurrent even if userland is not MP safe and can only run
		 * on single CPU (for early version of libpthread, it is true).
		 * In ideal world, every physical cpu should execute a thread.
		 * If there is enough KSEs, threads in kernel can be
		 * executed parallel on different cpus with full speed, 
		 * Concurrent in kernel shouldn't be restricted by number of 
		 * upcalls userland provides.
		 * Adding more upcall structures only increases concurrent
		 * in userland.
		 * Highest performance configuration is:
		 * N kses = N upcalls = N phyiscal cpus
		 */
		while (newkg->kg_kses < ncpus) {
			newke = kse_alloc();
			bzero(&newke->ke_startzero, RANGEOF(struct kse,
			      ke_startzero, ke_endzero));
#if 0
			mtx_lock_spin(&sched_lock);
			bcopy(&ke->ke_startcopy, &newke->ke_startcopy,
			      RANGEOF(struct kse, ke_startcopy, ke_endcopy));
			mtx_unlock_spin(&sched_lock);
#endif
			mtx_lock_spin(&sched_lock);
			kse_link(newke, newkg);
			/* Add engine */
			kse_reassign(newke);
			mtx_unlock_spin(&sched_lock);
		}
	}
	newku = upcall_alloc();
	newku->ku_mailbox = uap->mbx;
	newku->ku_func = mbx.km_func;
	bcopy(&mbx.km_stack, &newku->ku_stack, sizeof(stack_t));

	/* For the first call this may not have been set */
	if (td->td_standin == NULL)
		thread_alloc_spare(td, NULL);

	mtx_lock_spin(&sched_lock);
	if (newkg->kg_numupcalls >= ncpus) {
		mtx_unlock_spin(&sched_lock);
		upcall_free(newku);
		return (EPROCLIM);
	}
	upcall_link(newku, newkg);
	if (mbx.km_quantum)
		newkg->kg_upquantum = max(1, mbx.km_quantum/tick);

	/*
	 * Each upcall structure has an owner thread, find which
	 * one owns it.
	 */
	if (uap->newgroup) {
		/* 
		 * Because new ksegrp hasn't thread,
		 * create an initial upcall thread to own it.
		 */
		thread_schedule_upcall(td, newku);
	} else {
		/*
		 * If current thread hasn't an upcall structure,
		 * just assign the upcall to it.
		 */
		if (td->td_upcall == NULL) {
			newku->ku_owner = td;
			td->td_upcall = newku;
		} else {
			/*
			 * Create a new upcall thread to own it.
			 */
			thread_schedule_upcall(td, newku);
		}
	}
	mtx_unlock_spin(&sched_lock);
	return (0);
}

/*
 * Initialize global thread allocation resources.
 */
void
threadinit(void)
{

	thread_zone = uma_zcreate("THREAD", sched_sizeof_thread(),
	    thread_ctor, thread_dtor, thread_init, thread_fini,
	    UMA_ALIGN_CACHE, 0);
	ksegrp_zone = uma_zcreate("KSEGRP", sched_sizeof_ksegrp(),
	    NULL, NULL, ksegrp_init, NULL,
	    UMA_ALIGN_CACHE, 0);
	kse_zone = uma_zcreate("KSE", sched_sizeof_kse(),
	    NULL, NULL, kse_init, NULL,
	    UMA_ALIGN_CACHE, 0);
	upcall_zone = uma_zcreate("UPCALL", sizeof(struct kse_upcall),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_CACHE, 0);
}

/*
 * Stash an embarasingly extra thread into the zombie thread queue.
 */
void
thread_stash(struct thread *td)
{
	mtx_lock_spin(&kse_zombie_lock);
	TAILQ_INSERT_HEAD(&zombie_threads, td, td_runq);
	mtx_unlock_spin(&kse_zombie_lock);
}

/*
 * Stash an embarasingly extra kse into the zombie kse queue.
 */
void
kse_stash(struct kse *ke)
{
	mtx_lock_spin(&kse_zombie_lock);
	TAILQ_INSERT_HEAD(&zombie_kses, ke, ke_procq);
	mtx_unlock_spin(&kse_zombie_lock);
}

/*
 * Stash an embarasingly extra upcall into the zombie upcall queue.
 */

void
upcall_stash(struct kse_upcall *ku)
{
	mtx_lock_spin(&kse_zombie_lock);
	TAILQ_INSERT_HEAD(&zombie_upcalls, ku, ku_link);
	mtx_unlock_spin(&kse_zombie_lock);
}

/*
 * Stash an embarasingly extra ksegrp into the zombie ksegrp queue.
 */
void
ksegrp_stash(struct ksegrp *kg)
{
	mtx_lock_spin(&kse_zombie_lock);
	TAILQ_INSERT_HEAD(&zombie_ksegrps, kg, kg_ksegrp);
	mtx_unlock_spin(&kse_zombie_lock);
}

/*
 * Reap zombie kse resource.
 */
void
thread_reap(void)
{
	struct thread *td_first, *td_next;
	struct kse *ke_first, *ke_next;
	struct ksegrp *kg_first, * kg_next;
	struct kse_upcall *ku_first, *ku_next;

	/*
	 * Don't even bother to lock if none at this instant,
	 * we really don't care about the next instant..
	 */
	if ((!TAILQ_EMPTY(&zombie_threads))
	    || (!TAILQ_EMPTY(&zombie_kses))
	    || (!TAILQ_EMPTY(&zombie_ksegrps))
	    || (!TAILQ_EMPTY(&zombie_upcalls))) {
		mtx_lock_spin(&kse_zombie_lock);
		td_first = TAILQ_FIRST(&zombie_threads);
		ke_first = TAILQ_FIRST(&zombie_kses);
		kg_first = TAILQ_FIRST(&zombie_ksegrps);
		ku_first = TAILQ_FIRST(&zombie_upcalls);
		if (td_first)
			TAILQ_INIT(&zombie_threads);
		if (ke_first)
			TAILQ_INIT(&zombie_kses);
		if (kg_first)
			TAILQ_INIT(&zombie_ksegrps);
		if (ku_first)
			TAILQ_INIT(&zombie_upcalls);
		mtx_unlock_spin(&kse_zombie_lock);
		while (td_first) {
			td_next = TAILQ_NEXT(td_first, td_runq);
			if (td_first->td_ucred)
				crfree(td_first->td_ucred);
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
		while (ku_first) {
			ku_next = TAILQ_NEXT(ku_first, ku_link);
			upcall_free(ku_first);
			ku_first = ku_next;
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
	int error,temp;
	mcontext_t mc;

	p = td->td_proc;
	kg = td->td_ksegrp;

	/* Export the user/machine context. */
	get_mcontext(td, &mc, 0);
	addr = (void *)(&td->td_mailbox->tm_context.uc_mcontext);
	error = copyout(&mc, addr, sizeof(mcontext_t));
	if (error)
		goto bad;

	/* Exports clock ticks in kernel mode */
	addr = (caddr_t)(&td->td_mailbox->tm_sticks);
	temp = fuword(addr) + td->td_usticks;
	if (suword(addr, temp)) {
		error = EFAULT;
		goto bad;
	}

	/* Get address in latest mbox of list pointer */
	addr = (void *)(&td->td_mailbox->tm_next);
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
			/*
			 * The thread context may be taken away by
			 * other upcall threads when we unlock
			 * process lock. it's no longer valid to
			 * use it again in any other places.
			 */
			td->td_mailbox = NULL;
			PROC_UNLOCK(p);
			break;
		}
		PROC_UNLOCK(p);
	}
	td->td_usticks = 0;
	return (0);

bad:
	PROC_LOCK(p);
	psignal(p, SIGSEGV);
	PROC_UNLOCK(p);
	/* The mailbox is bad, don't use it */
	td->td_mailbox = NULL;
	td->td_usticks = 0;
	return (error);
}

/*
 * Take the list of completed mailboxes for this KSEGRP and put them on this
 * upcall's mailbox as it's the next one going up.
 */
static int
thread_link_mboxes(struct ksegrp *kg, struct kse_upcall *ku)
{
	struct proc *p = kg->kg_proc;
	void *addr;
	uintptr_t mbx;

	addr = (void *)(&ku->ku_mailbox->km_completed);
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
thread_statclock(int user)
{
	struct thread *td = curthread;
	
	if (td->td_ksegrp->kg_numupcalls == 0)
		return (-1);
	if (user) {
		/* Current always do via ast() */
		mtx_lock_spin(&sched_lock);
		td->td_flags |= (TDF_USTATCLOCK|TDF_ASTPENDING);
		mtx_unlock_spin(&sched_lock);
		td->td_uuticks++;
	} else {
		if (td->td_mailbox != NULL)
			td->td_usticks++;
		else {
			/* XXXKSE
		 	 * We will call thread_user_enter() for every
			 * kernel entry in future, so if the thread mailbox
			 * is NULL, it must be a UTS kernel, don't account
			 * clock ticks for it.
			 */
		}
	}
	return (0);
}

/*
 * Export state clock ticks for userland
 */
static int
thread_update_usr_ticks(struct thread *td, int user)
{
	struct proc *p = td->td_proc;
	struct kse_thr_mailbox *tmbx;
	struct kse_upcall *ku;
	struct ksegrp *kg;
	caddr_t addr;
	uint uticks;

	if ((ku = td->td_upcall) == NULL)
		return (-1);
	
	tmbx = (void *)fuword((void *)&ku->ku_mailbox->km_curthread);
	if ((tmbx == NULL) || (tmbx == (void *)-1))
		return (-1);
	if (user) {
		uticks = td->td_uuticks;
		td->td_uuticks = 0;
		addr = (caddr_t)&tmbx->tm_uticks;
	} else {
		uticks = td->td_usticks;
		td->td_usticks = 0;
		addr = (caddr_t)&tmbx->tm_sticks;
	}
	if (uticks) {
		if (suword(addr, uticks+fuword(addr))) {
			PROC_LOCK(p);
			psignal(p, SIGSEGV);
			PROC_UNLOCK(p);
			return (-2);
		}
	}
	kg = td->td_ksegrp;
	if (kg->kg_upquantum && ticks >= kg->kg_nextupcall) {
		mtx_lock_spin(&sched_lock);
		td->td_upcall->ku_flags |= KUF_DOUPCALL;
		mtx_unlock_spin(&sched_lock);
	}
	return (0);
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
		thread_unlink(td);
		if (p->p_maxthrwaits)
			wakeup(&p->p_numthreads);
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

		/*
		 * Because each upcall structure has an owner thread,
		 * owner thread exits only when process is in exiting
		 * state, so upcall to userland is no longer needed,
		 * deleting upcall structure is safe here.
		 * So when all threads in a group is exited, all upcalls
		 * in the group should be automatically freed.
		 */
		if (td->td_upcall)
			upcall_remove(td);
	
		ke->ke_state = KES_UNQUEUED;
		ke->ke_thread = NULL;
		/* 
		 * Decide what to do with the KSE attached to this thread.
		 */
		if (ke->ke_flags & KEF_EXIT)
			kse_unlink(ke);
		else
			kse_reassign(ke);
		PROC_UNLOCK(p);
		td->td_kse	= NULL;
		td->td_state	= TDS_INACTIVE;
#if 0
		td->td_proc	= NULL;
#endif
		td->td_ksegrp	= NULL;
		td->td_last_kse	= NULL;
		PCPU_SET(deadthread, td);
	} else {
		PROC_UNLOCK(p);
	}
	/* XXX Shouldn't cpu_throw() here. */
	mtx_assert(&sched_lock, MA_OWNED);
#if !defined(__alpha__) && !defined(__powerpc__) 
	cpu_throw(td, choosethread());
#else
	cpu_throw();
#endif
	panic("I'm a teapot!");
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
	td->td_state    = TDS_INACTIVE;
	td->td_proc     = p;
	td->td_ksegrp   = kg;
	td->td_last_kse = NULL;
	td->td_flags    = 0;
	td->td_kse      = NULL;

	LIST_INIT(&td->td_contested);
	callout_init(&td->td_slpcallout, 1);
	TAILQ_INSERT_HEAD(&p->p_threads, td, td_plist);
	TAILQ_INSERT_HEAD(&kg->kg_threads, td, td_kglist);
	p->p_numthreads++;
	kg->kg_numthreads++;
}

void
thread_unlink(struct thread *td)
{      
	struct proc *p = td->td_proc;
	struct ksegrp *kg = td->td_ksegrp;

	mtx_assert(&sched_lock, MA_OWNED);
	TAILQ_REMOVE(&p->p_threads, td, td_plist);
	p->p_numthreads--;
	TAILQ_REMOVE(&kg->kg_threads, td, td_kglist);
	kg->kg_numthreads--;
	/* could clear a few other things here */
} 

/*
 * Purge a ksegrp resource. When a ksegrp is preparing to
 * exit, it calls this function. 
 */
static void
kse_purge_group(struct thread *td)
{
	struct ksegrp *kg;
	struct kse *ke;

	kg = td->td_ksegrp;
 	KASSERT(kg->kg_numthreads == 1, ("%s: bad thread number", __func__));
	while ((ke = TAILQ_FIRST(&kg->kg_iq)) != NULL) {
		KASSERT(ke->ke_state == KES_IDLE,
			("%s: wrong idle KSE state", __func__));
		kse_unlink(ke);
	}
	KASSERT((kg->kg_kses == 1),
		("%s: ksegrp still has %d KSEs", __func__, kg->kg_kses));
	KASSERT((kg->kg_numupcalls == 0),
	        ("%s: ksegrp still has %d upcall datas",
		__func__, kg->kg_numupcalls));
}

/*
 * Purge a process's KSE resource. When a process is preparing to 
 * exit, it calls kse_purge to release any extra KSE resources in 
 * the process.
 */
static void
kse_purge(struct proc *p, struct thread *td)
{
	struct ksegrp *kg;
	struct kse *ke;

 	KASSERT(p->p_numthreads == 1, ("bad thread number"));
	while ((kg = TAILQ_FIRST(&p->p_ksegrps)) != NULL) {
		TAILQ_REMOVE(&p->p_ksegrps, kg, kg_ksegrp);
		p->p_numksegrps--;
		/*
		 * There is no ownership for KSE, after all threads
		 * in the group exited, it is possible that some KSEs 
		 * were left in idle queue, gc them now.
		 */
		while ((ke = TAILQ_FIRST(&kg->kg_iq)) != NULL) {
			KASSERT(ke->ke_state == KES_IDLE,
			   ("%s: wrong idle KSE state", __func__));
			TAILQ_REMOVE(&kg->kg_iq, ke, ke_kgrlist);
			kg->kg_idle_kses--;
			TAILQ_REMOVE(&kg->kg_kseq, ke, ke_kglist);
			kg->kg_kses--;
			kse_stash(ke);
		}
		KASSERT(((kg->kg_kses == 0) && (kg != td->td_ksegrp)) ||
		        ((kg->kg_kses == 1) && (kg == td->td_ksegrp)),
		        ("ksegrp has wrong kg_kses: %d", kg->kg_kses));
		KASSERT((kg->kg_numupcalls == 0),
		        ("%s: ksegrp still has %d upcall datas",
			__func__, kg->kg_numupcalls));
	
		if (kg != td->td_ksegrp)
			ksegrp_stash(kg);
	}
	TAILQ_INSERT_HEAD(&p->p_ksegrps, td->td_ksegrp, kg_ksegrp);
	p->p_numksegrps++;
}

/*
 * This function is intended to be used to initialize a spare thread
 * for upcall. Initialize thread's large data area outside sched_lock
 * for thread_schedule_upcall().
 */
void
thread_alloc_spare(struct thread *td, struct thread *spare)
{
	if (td->td_standin)
		return;
	if (spare == NULL)
		spare = thread_alloc();
	td->td_standin = spare;
	bzero(&spare->td_startzero,
	    (unsigned)RANGEOF(struct thread, td_startzero, td_endzero));
	spare->td_proc = td->td_proc;
	spare->td_ucred = crhold(td->td_ucred);
}

/*
 * Create a thread and schedule it for upcall on the KSE given.
 * Use our thread's standin so that we don't have to allocate one.
 */
struct thread *
thread_schedule_upcall(struct thread *td, struct kse_upcall *ku)
{
	struct thread *td2;

	mtx_assert(&sched_lock, MA_OWNED);

	/* 
	 * Schedule an upcall thread on specified kse_upcall,
	 * the kse_upcall must be free.
	 * td must have a spare thread.
	 */
	KASSERT(ku->ku_owner == NULL, ("%s: upcall has owner", __func__));
	if ((td2 = td->td_standin) != NULL) {
		td->td_standin = NULL;
	} else {
		panic("no reserve thread when scheduling an upcall");
		return (NULL);
	}
	CTR3(KTR_PROC, "thread_schedule_upcall: thread %p (pid %d, %s)",
	     td2, td->td_proc->p_pid, td->td_proc->p_comm);
	bcopy(&td->td_startcopy, &td2->td_startcopy,
	    (unsigned) RANGEOF(struct thread, td_startcopy, td_endcopy));
	thread_link(td2, ku->ku_ksegrp);
	/* inherit blocked thread's context */
	cpu_set_upcall(td2, td);
	/* Let the new thread become owner of the upcall */
	ku->ku_owner   = td2;
	td2->td_upcall = ku;
	td2->td_flags  = TDF_UPCALLING;
#if 0	/* XXX This shouldn't be necessary */
	if (td->td_proc->p_sflag & PS_NEEDSIGCHK)
		td2->td_flags |= TDF_ASTPENDING;
#endif
	td2->td_kse    = NULL;
	td2->td_state  = TDS_CAN_RUN;
	td2->td_inhibitors = 0;
	setrunqueue(td2);
	return (td2);	/* bogus.. should be a void function */
}

void
thread_signal_add(struct thread *td, int sig)
{
	struct kse_upcall *ku;
	struct proc *p;
	sigset_t ss;
	int error;

	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	mtx_assert(&p->p_sigacts->ps_mtx, MA_OWNED);
	td = curthread;
	ku = td->td_upcall;
	mtx_unlock(&p->p_sigacts->ps_mtx);
	PROC_UNLOCK(p);
	error = copyin(&ku->ku_mailbox->km_sigscaught, &ss, sizeof(sigset_t));
	if (error)
		goto error;

	SIGADDSET(ss, sig);

	error = copyout(&ss, &ku->ku_mailbox->km_sigscaught, sizeof(sigset_t));
	if (error)
		goto error;

	PROC_LOCK(p);
	mtx_lock(&p->p_sigacts->ps_mtx);
	return;
error:
	PROC_LOCK(p);
	sigexit(td, SIGILL);
}


/*
 * Schedule an upcall to notify a KSE process recieved signals.
 *
 */
void
thread_signal_upcall(struct thread *td)
{
	mtx_lock_spin(&sched_lock);
	td->td_flags |= TDF_UPCALLING;
	mtx_unlock_spin(&sched_lock);

	return;
}

void
thread_switchout(struct thread *td)
{
	struct kse_upcall *ku;

	mtx_assert(&sched_lock, MA_OWNED);

	/*
	 * If the outgoing thread is in threaded group and has never
	 * scheduled an upcall, decide whether this is a short
	 * or long term event and thus whether or not to schedule
	 * an upcall.
	 * If it is a short term event, just suspend it in
	 * a way that takes its KSE with it.
	 * Select the events for which we want to schedule upcalls.
	 * For now it's just sleep.
	 * XXXKSE eventually almost any inhibition could do.
	 */
	if (TD_CAN_UNBIND(td) && (td->td_standin) && TD_ON_SLEEPQ(td)) {
		/* 
		 * Release ownership of upcall, and schedule an upcall
		 * thread, this new upcall thread becomes the owner of
		 * the upcall structure.
		 */
		ku = td->td_upcall;
		ku->ku_owner = NULL;
		td->td_upcall = NULL; 
		td->td_flags &= ~TDF_CAN_UNBIND;
		thread_schedule_upcall(td, ku);
	}
}

/*
 * Setup done on the thread when it enters the kernel.
 * XXXKSE Presently only for syscalls but eventually all kernel entries.
 */
void
thread_user_enter(struct proc *p, struct thread *td)
{
	struct ksegrp *kg;
	struct kse_upcall *ku;
	struct kse_thr_mailbox *tmbx;

	kg = td->td_ksegrp;

	/*
	 * First check that we shouldn't just abort.
	 * But check if we are the single thread first!
	 */
	PROC_LOCK(p);
	if ((p->p_flag & P_SINGLE_EXIT) && (p->p_singlethread != td)) {
		mtx_lock_spin(&sched_lock);
		thread_stopped(p);
		thread_exit();
		/* NOTREACHED */
	}
	PROC_UNLOCK(p);

	/*
	 * If we are doing a syscall in a KSE environment,
	 * note where our mailbox is. There is always the
	 * possibility that we could do this lazily (in kse_reassign()),
	 * but for now do it every time.
	 */
	kg = td->td_ksegrp;
	if (kg->kg_numupcalls) {
		ku = td->td_upcall;
		KASSERT(ku, ("%s: no upcall owned", __func__));
		KASSERT((ku->ku_owner == td), ("%s: wrong owner", __func__));
		KASSERT(!TD_CAN_UNBIND(td), ("%s: can unbind", __func__));
		ku->ku_mflags = fuword((void *)&ku->ku_mailbox->km_flags);
		tmbx = (void *)fuword((void *)&ku->ku_mailbox->km_curthread);
		if ((tmbx == NULL) || (tmbx == (void *)-1)) {
			td->td_mailbox = NULL;
		} else {
			td->td_mailbox = tmbx;
			if (td->td_standin == NULL)
				thread_alloc_spare(td, NULL);
			mtx_lock_spin(&sched_lock);
			if (ku->ku_mflags & KMF_NOUPCALL)
				td->td_flags &= ~TDF_CAN_UNBIND;
			else
				td->td_flags |= TDF_CAN_UNBIND;
			mtx_unlock_spin(&sched_lock);
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
	int error = 0, upcalls, uts_crit;
	struct kse_upcall *ku;
	struct ksegrp *kg, *kg2;
	struct proc *p;
	struct timespec ts;

	p = td->td_proc;
	kg = td->td_ksegrp;

	/* Nothing to do with non-threaded group/process */
	if (td->td_ksegrp->kg_numupcalls == 0)
		return (0);

	/*
	 * Stat clock interrupt hit in userland, it 
	 * is returning from interrupt, charge thread's
	 * userland time for UTS.
	 */
	if (td->td_flags & TDF_USTATCLOCK) {
		thread_update_usr_ticks(td, 1);
		mtx_lock_spin(&sched_lock);
		td->td_flags &= ~TDF_USTATCLOCK;
		mtx_unlock_spin(&sched_lock);
		if (kg->kg_completed || 
		    (td->td_upcall->ku_flags & KUF_DOUPCALL))
			thread_user_enter(p, td);
	}

	uts_crit = (td->td_mailbox == NULL);
	ku = td->td_upcall;
	/* 
	 * Optimisation:
	 * This thread has not started any upcall.
	 * If there is no work to report other than ourself,
	 * then it can return direct to userland.
	 */
	if (TD_CAN_UNBIND(td)) {
		mtx_lock_spin(&sched_lock);
		td->td_flags &= ~TDF_CAN_UNBIND;
		if ((td->td_flags & TDF_NEEDSIGCHK) == 0 &&
		    (kg->kg_completed == NULL) &&
		    (ku->ku_flags & KUF_DOUPCALL) == 0 &&
		    (kg->kg_upquantum && ticks < kg->kg_nextupcall)) {
			mtx_unlock_spin(&sched_lock);
			thread_update_usr_ticks(td, 0);
			nanotime(&ts);
			error = copyout(&ts,
				(caddr_t)&ku->ku_mailbox->km_timeofday,
				sizeof(ts));
			td->td_mailbox = 0;
			ku->ku_mflags = 0;
			if (error)
				goto out;
			return (0);
		}
		mtx_unlock_spin(&sched_lock);
		error = thread_export_context(td);
		if (error) {
			/*
			 * Failing to do the KSE operation just defaults
			 * back to synchonous operation, so just return from
			 * the syscall.
			 */
			goto out;
		}
		/*
		 * There is something to report, and we own an upcall
		 * strucuture, we can go to userland.
		 * Turn ourself into an upcall thread.
		 */
		mtx_lock_spin(&sched_lock);
		td->td_flags |= TDF_UPCALLING;
		mtx_unlock_spin(&sched_lock);
	} else if (td->td_mailbox && (ku == NULL)) {
		error = thread_export_context(td);
		/* possibly upcall with error? */
		PROC_LOCK(p);
		/*
		 * There are upcall threads waiting for
		 * work to do, wake one of them up.
		 * XXXKSE Maybe wake all of them up. 
		 */
		if (!error && kg->kg_upsleeps)
			wakeup_one(&kg->kg_completed);
		mtx_lock_spin(&sched_lock);
		thread_stopped(p);
		thread_exit();
		/* NOTREACHED */
	}

	KASSERT(TD_CAN_UNBIND(td) == 0, ("can unbind"));

	if (p->p_numthreads > max_threads_per_proc) {
		max_threads_hits++;
		PROC_LOCK(p);
		mtx_lock_spin(&sched_lock);
		p->p_maxthrwaits++;
		while (p->p_numthreads > max_threads_per_proc) {
			upcalls = 0;
			FOREACH_KSEGRP_IN_PROC(p, kg2) {
				if (kg2->kg_numupcalls == 0)
					upcalls++;
				else
					upcalls += kg2->kg_numupcalls;
			}
			if (upcalls >= max_threads_per_proc)
				break;
			mtx_unlock_spin(&sched_lock);
			if (msleep(&p->p_numthreads, &p->p_mtx, PPAUSE|PCATCH,
			    "maxthreads", NULL)) {
				mtx_lock_spin(&sched_lock);
				break;
			} else {
				mtx_lock_spin(&sched_lock);
			}
		}
		p->p_maxthrwaits--;
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
	}

	if (td->td_flags & TDF_UPCALLING) {
		uts_crit = 0;
		kg->kg_nextupcall = ticks+kg->kg_upquantum;
		/* 
		 * There is no more work to do and we are going to ride
		 * this thread up to userland as an upcall.
		 * Do the last parts of the setup needed for the upcall.
		 */
		CTR3(KTR_PROC, "userret: upcall thread %p (pid %d, %s)",
		    td, td->td_proc->p_pid, td->td_proc->p_comm);

		mtx_lock_spin(&sched_lock);
		td->td_flags &= ~TDF_UPCALLING;
		if (ku->ku_flags & KUF_DOUPCALL)
			ku->ku_flags &= ~KUF_DOUPCALL;
		mtx_unlock_spin(&sched_lock);

		/*
		 * Set user context to the UTS
		 */
		if (!(ku->ku_mflags & KMF_NOUPCALL)) {
			cpu_set_upcall_kse(td, ku);
			error = suword(&ku->ku_mailbox->km_curthread, 0);
			if (error)
				goto out;
		}

		/*
		 * Unhook the list of completed threads.
		 * anything that completes after this gets to 
		 * come in next time.
		 * Put the list of completed thread mailboxes on
		 * this KSE's mailbox.
		 */
		if (!(ku->ku_mflags & KMF_NOCOMPLETED) &&
		    (error = thread_link_mboxes(kg, ku)) != 0)
			goto out;
	}
	if (!uts_crit) {
		nanotime(&ts);
		error = copyout(&ts, &ku->ku_mailbox->km_timeofday, sizeof(ts));
	}

out:
	if (error) {
		/*
		 * Things are going to be so screwed we should just kill
		 * the process.
		 * how do we do that?
		 */
		PROC_LOCK(td->td_proc);
		psignal(td->td_proc, SIGSEGV);
		PROC_UNLOCK(td->td_proc);
	} else {
		/*
		 * Optimisation:
		 * Ensure that we have a spare thread available,
		 * for when we re-enter the kernel.
		 */
		if (td->td_standin == NULL)
			thread_alloc_spare(td, NULL);
	}

	ku->ku_mflags = 0;
	/*
	 * Clear thread mailbox first, then clear system tick count.
	 * The order is important because thread_statclock() use 
	 * mailbox pointer to see if it is an userland thread or
	 * an UTS kernel thread.
	 */
	td->td_mailbox = NULL;
	td->td_usticks = 0;
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

	if ((p->p_flag & P_THREADED) == 0 && p->p_numthreads == 1)
		return (0);

	/* Is someone already single threading? */
	if (p->p_singlethread) 
		return (1);

	if (force_exit == SINGLE_EXIT) {
		p->p_flag |= P_SINGLE_EXIT;
	} else
		p->p_flag &= ~P_SINGLE_EXIT;
	p->p_flag |= P_STOPPED_SINGLE;
	mtx_lock_spin(&sched_lock);
	p->p_singlethread = td;
	while ((p->p_numthreads - p->p_suspcount) != 1) {
		FOREACH_THREAD_IN_PROC(p, td2) {
			if (td2 == td)
				continue;
			td2->td_flags |= TDF_ASTPENDING;
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
				} else {
					if (TD_IS_SUSPENDED(td2))
						continue;
					/*
					 * maybe other inhibitted states too?
					 * XXXKSE Is it totally safe to
					 * suspend a non-interruptable thread?
					 */
					if (td2->td_inhibitors &
					    (TDI_SLEEPING | TDI_SWAPPED))
						thread_suspend_one(td2);
				}
			}
		}
		/* 
		 * Maybe we suspended some threads.. was it enough? 
		 */
		if ((p->p_numthreads - p->p_suspcount) == 1)
			break;

		/*
		 * Wake us up when everyone else has suspended.
		 * In the mean time we suspend as well.
		 */
		thread_suspend_one(td);
		DROP_GIANT();
		PROC_UNLOCK(p);
		p->p_stats->p_ru.ru_nvcsw++;
		mi_switch();
		mtx_unlock_spin(&sched_lock);
		PICKUP_GIANT();
		PROC_LOCK(p);
		mtx_lock_spin(&sched_lock);
	}
	if (force_exit == SINGLE_EXIT) { 
		if (td->td_upcall)
			upcall_remove(td);
		kse_purge(p, td);
	}
	mtx_unlock_spin(&sched_lock);
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

		mtx_lock_spin(&sched_lock);
		thread_stopped(p);
		/*
		 * If the process is waiting for us to exit,
		 * this thread should just suicide.
		 * Assumes that P_SINGLE_EXIT implies P_STOPPED_SINGLE.
		 */
		if ((p->p_flag & P_SINGLE_EXIT) && (p->p_singlethread != td)) {
			while (mtx_owned(&Giant))
				mtx_unlock(&Giant);
			if (p->p_flag & P_THREADED)
				thread_exit();
			else
				thr_exit1();
		}

		/*
		 * When a thread suspends, it just
		 * moves to the processes's suspend queue
		 * and stays there.
		 */
		thread_suspend_one(td);
		if (P_SHOULDSTOP(p) == P_STOPPED_SINGLE) {
			if (p->p_numthreads == p->p_suspcount) {
				thread_unsuspend_one(p->p_singlethread);
			}
		}
		DROP_GIANT();
		PROC_UNLOCK(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		mtx_unlock_spin(&sched_lock);
		PICKUP_GIANT();
		PROC_LOCK(p);
	}
	return (0);
}

void
thread_suspend_one(struct thread *td)
{
	struct proc *p = td->td_proc;

	mtx_assert(&sched_lock, MA_OWNED);
	PROC_LOCK_ASSERT(p, MA_OWNED);
	KASSERT(!TD_IS_SUSPENDED(td), ("already suspended"));
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
	PROC_LOCK_ASSERT(p, MA_OWNED);
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
	mtx_lock_spin(&sched_lock);
	p->p_singlethread = NULL;
	/*
	 * If there are other threads they mey now run,
	 * unless of course there is a blanket 'stop order'
	 * on the process. The single threader must be allowed
	 * to continue however as this is a bad place to stop.
	 */
	if ((p->p_numthreads != 1) && (!P_SHOULDSTOP(p))) {
		while (( td = TAILQ_FIRST(&p->p_suspended))) {
			thread_unsuspend_one(td);
		}
	}
	mtx_unlock_spin(&sched_lock);
}


