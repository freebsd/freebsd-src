/*-
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
#include <sys/imgact.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/smp.h>
#include <sys/syscallsubr.h>
#include <sys/sysproto.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/sleepqueue.h>
#include <sys/kse.h>
#include <sys/ktr.h>
#include <vm/uma.h>

/*
 * KSEGRP related storage.
 */
static uma_zone_t upcall_zone;

/* DEBUG ONLY */
extern int virtual_cpu;
extern int thread_debug;

extern int max_threads_per_proc;
extern int max_groups_per_proc;
extern int max_threads_hits;
extern struct mtx kse_zombie_lock;


TAILQ_HEAD(, kse_upcall) zombie_upcalls =
	TAILQ_HEAD_INITIALIZER(zombie_upcalls);

static int thread_update_usr_ticks(struct thread *td);
static void thread_alloc_spare(struct thread *td);

struct kse_upcall *
upcall_alloc(void)
{
	struct kse_upcall *ku;

	ku = uma_zalloc(upcall_zone, M_WAITOK | M_ZERO);
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

	mtx_assert(&sched_lock, MA_OWNED);
	if (td->td_upcall != NULL) {
		td->td_upcall->ku_owner = NULL;
		upcall_unlink(td->td_upcall);
		td->td_upcall = NULL;
	}
}

#ifndef _SYS_SYSPROTO_H_
struct kse_switchin_args {
	struct kse_thr_mailbox *tmbx;
	int flags;
};
#endif

int
kse_switchin(struct thread *td, struct kse_switchin_args *uap)
{
	struct kse_thr_mailbox tmbx;
	struct kse_upcall *ku;
	int error;

	if ((ku = td->td_upcall) == NULL || TD_CAN_UNBIND(td))
		return (EINVAL);
	error = (uap->tmbx == NULL) ? EINVAL : 0;
	if (!error)
		error = copyin(uap->tmbx, &tmbx, sizeof(tmbx));
	if (!error && (uap->flags & KSE_SWITCHIN_SETTMBX))
		error = (suword(&ku->ku_mailbox->km_curthread,
			 (long)uap->tmbx) != 0 ? EINVAL : 0);
	if (!error)
		error = set_mcontext(td, &tmbx.tm_context.uc_mcontext);
	if (!error) {
		suword32(&uap->tmbx->tm_lwp, td->td_tid);
		if (uap->flags & KSE_SWITCHIN_SETTMBX) {
			td->td_mailbox = uap->tmbx;
			td->td_pflags |= TDP_CAN_UNBIND;
		}
		if (td->td_proc->p_flag & P_TRACED) {
			if (tmbx.tm_dflags & TMDF_SSTEP)
				ptrace_single_step(td);
			else
				ptrace_clear_single_step(td);
			if (tmbx.tm_dflags & TMDF_SUSPEND) {
				mtx_lock_spin(&sched_lock);
				/* fuword can block, check again */
				if (td->td_upcall)
					ku->ku_flags |= KUF_DOUPCALL;
				mtx_unlock_spin(&sched_lock);
			}
		}
	}
	return ((error == 0) ? EJUSTRETURN : error);
}

/*
struct kse_thr_interrupt_args {
	struct kse_thr_mailbox * tmbx;
	int cmd;
	long data;
};
*/
int
kse_thr_interrupt(struct thread *td, struct kse_thr_interrupt_args *uap)
{
	struct kse_execve_args args;
	struct image_args iargs;
	struct proc *p;
	struct thread *td2;
	struct kse_upcall *ku;
	struct kse_thr_mailbox *tmbx;
	uint32_t flags;
	int error;

	p = td->td_proc;

	if (!(p->p_flag & P_SA))
		return (EINVAL);

	switch (uap->cmd) {
	case KSE_INTR_SENDSIG:
		if (uap->data < 0 || uap->data > _SIG_MAXSIG)
			return (EINVAL);
	case KSE_INTR_INTERRUPT:
	case KSE_INTR_RESTART:
		PROC_LOCK(p);
		mtx_lock_spin(&sched_lock);
		FOREACH_THREAD_IN_PROC(p, td2) {
			if (td2->td_mailbox == uap->tmbx)
				break;
		}
		if (td2 == NULL) {
			mtx_unlock_spin(&sched_lock);
			PROC_UNLOCK(p);
			return (ESRCH);
		}
		if (uap->cmd == KSE_INTR_SENDSIG) {
			if (uap->data > 0) {
				td2->td_flags &= ~TDF_INTERRUPT;
				mtx_unlock_spin(&sched_lock);
				tdsignal(td2, (int)uap->data, SIGTARGET_TD);
			} else {
				mtx_unlock_spin(&sched_lock);
			}
		} else {
			td2->td_flags |= TDF_INTERRUPT | TDF_ASTPENDING;
			if (TD_CAN_UNBIND(td2))
				td2->td_upcall->ku_flags |= KUF_DOUPCALL;
			if (uap->cmd == KSE_INTR_INTERRUPT)
				td2->td_intrval = EINTR;
			else
				td2->td_intrval = ERESTART;
			if (TD_ON_SLEEPQ(td2) && (td2->td_flags & TDF_SINTR))
				sleepq_abort(td2);
			mtx_unlock_spin(&sched_lock);
		}
		PROC_UNLOCK(p);
		break;
	case KSE_INTR_SIGEXIT:
		if (uap->data < 1 || uap->data > _SIG_MAXSIG)
			return (EINVAL);
		PROC_LOCK(p);
		sigexit(td, (int)uap->data);
		break;

	case KSE_INTR_DBSUSPEND:
		/* this sub-function is only for bound thread */
		if (td->td_pflags & TDP_SA)
			return (EINVAL);
		ku = td->td_upcall;
		tmbx = (void *)fuword((void *)&ku->ku_mailbox->km_curthread);
		if (tmbx == NULL || tmbx == (void *)-1)
			return (EINVAL);
		flags = 0;
		while ((p->p_flag & P_TRACED) && !(p->p_flag & P_SINGLE_EXIT)) {
			flags = fuword32(&tmbx->tm_dflags);
			if (!(flags & TMDF_SUSPEND))
				break;
			PROC_LOCK(p);
			mtx_lock_spin(&sched_lock);
			thread_stopped(p);
			thread_suspend_one(td);
			PROC_UNLOCK(p);
			mi_switch(SW_VOL, NULL);
			mtx_unlock_spin(&sched_lock);
		}
		return (0);

	case KSE_INTR_EXECVE:
		error = copyin((void *)uap->data, &args, sizeof(args));
		if (error)
			return (error);
		error = exec_copyin_args(&iargs, args.path, UIO_USERSPACE,
		    args.argv, args.envp);
		if (error == 0)
			error = kern_execve(td, &iargs, NULL);
		exec_free_args(&iargs);
		if (error == 0) {
			PROC_LOCK(p);
			SIGSETOR(td->td_siglist, args.sigpend);
			PROC_UNLOCK(p);
			kern_sigprocmask(td, SIG_SETMASK, &args.sigmask, NULL,
			    0);
		}
		return (error);

	default:
		return (EINVAL);
	}
	return (0);
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
	struct kse_upcall *ku, *ku2;
	int    error, count;

	p = td->td_proc;
	/* 
	 * Ensure that this is only called from the UTS
	 */
	if ((ku = td->td_upcall) == NULL || TD_CAN_UNBIND(td))
		return (EINVAL);

	kg = td->td_ksegrp;
	count = 0;

	/*
	 * Calculate the existing non-exiting upcalls in this ksegroup.
	 * If we are the last upcall but there are still other threads,
	 * then do not exit. We need the other threads to be able to 
	 * complete whatever they are doing.
	 * XXX This relies on the userland knowing what to do if we return.
	 * It may be a better choice to convert ourselves into a kse_release
	 * ( or similar) and wait in the kernel to be needed.
	 */
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

	/* 
	 * Mark the UTS mailbox as having been finished with.
	 * If that fails then just go for a segfault.
	 * XXX need to check it that can be deliverred without a mailbox.
	 */
	error = suword32(&ku->ku_mailbox->km_flags, ku->ku_mflags|KMF_DONE);
	if (!(td->td_pflags & TDP_SA))
		if (suword32(&td->td_mailbox->tm_lwp, 0))
			error = EFAULT;
	PROC_LOCK(p);
	if (error)
		psignal(p, SIGSEGV);
	mtx_lock_spin(&sched_lock);
	upcall_remove(td);
	if (p->p_numthreads != 1) {
		/*
		 * If we are not the last thread, but we are the last
		 * thread in this ksegrp, then by definition this is not
		 * the last group and we need to clean it up as well.
		 * thread_exit will clean up the kseg as needed.
		 */
		thread_stopped(p);
		thread_exit();
		/* NOTREACHED */
	}
	/*
	 * This is the last thread. Just return to the user.
	 * We know that there is only one ksegrp too, as any others
	 * would have been discarded in previous calls to thread_exit().
	 * Effectively we have left threading mode..
	 * The only real thing left to do is ensure that the
	 * scheduler sets out concurrency back to 1 as that may be a
	 * resource leak otherwise.
	 * This is an A[PB]I issue.. what SHOULD we do?
	 * One possibility is to return to the user. It may not cope well.
	 * The other possibility would be to let the process exit.
	 */
	thread_unthread(td);
	mtx_unlock_spin(&sched_lock);
	PROC_UNLOCK(p);
#if 1
	return (0);
#else
	exit1(td, 0);
#endif
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
	struct kse_upcall *ku;
	struct timespec timeout;
	struct timeval tv;
	sigset_t sigset;
	int error;

	p = td->td_proc;
	kg = td->td_ksegrp;
	if ((ku = td->td_upcall) == NULL || TD_CAN_UNBIND(td))
		return (EINVAL);
	if (uap->timeout != NULL) {
		if ((error = copyin(uap->timeout, &timeout, sizeof(timeout))))
			return (error);
		TIMESPEC_TO_TIMEVAL(&tv, &timeout);
	}
	if (td->td_pflags & TDP_SA)
		td->td_pflags |= TDP_UPCALLING;
	else {
		ku->ku_mflags = fuword32(&ku->ku_mailbox->km_flags);
		if (ku->ku_mflags == -1) {
			PROC_LOCK(p);
			sigexit(td, SIGSEGV);
		}
	}
	PROC_LOCK(p);
	if (ku->ku_mflags & KMF_WAITSIGEVENT) {
		/* UTS wants to wait for signal event */
		if (!(p->p_flag & P_SIGEVENT) &&
		    !(ku->ku_flags & KUF_DOUPCALL)) {
			td->td_kflags |= TDK_KSERELSIG;
			error = msleep(&p->p_siglist, &p->p_mtx, PPAUSE|PCATCH,
			    "ksesigwait", (uap->timeout ? tvtohz(&tv) : 0));
			td->td_kflags &= ~(TDK_KSERELSIG | TDK_WAKEUP);
		}
		p->p_flag &= ~P_SIGEVENT;
		sigset = p->p_siglist;
		PROC_UNLOCK(p);
		error = copyout(&sigset, &ku->ku_mailbox->km_sigscaught,
		    sizeof(sigset));
	} else {
		if ((ku->ku_flags & KUF_DOUPCALL) == 0 &&
		    ((ku->ku_mflags & KMF_NOCOMPLETED) ||
		     (kg->kg_completed == NULL))) {
			kg->kg_upsleeps++;
			td->td_kflags |= TDK_KSEREL;
			error = msleep(&kg->kg_completed, &p->p_mtx,
				PPAUSE|PCATCH, "kserel",
				(uap->timeout ? tvtohz(&tv) : 0));
			td->td_kflags &= ~(TDK_KSEREL | TDK_WAKEUP);
			kg->kg_upsleeps--;
		}
		PROC_UNLOCK(p);
	}
	if (ku->ku_flags & KUF_DOUPCALL) {
		mtx_lock_spin(&sched_lock);
		ku->ku_flags &= ~KUF_DOUPCALL;
		mtx_unlock_spin(&sched_lock);
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
	struct ksegrp *kg;
	struct kse_upcall *ku;
	struct thread *td2;

	p = td->td_proc;
	td2 = NULL;
	ku = NULL;
	/* KSE-enabled processes only, please. */
	if (!(p->p_flag & P_SA))
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
			mtx_unlock_spin(&sched_lock);
			wakeup(&kg->kg_completed);
			PROC_UNLOCK(p);
			return (0);
		}
		ku = TAILQ_FIRST(&kg->kg_upcalls);
	}
	if (ku == NULL) {
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
		return (ESRCH);
	}
	if ((td2 = ku->ku_owner) == NULL) {
		mtx_unlock_spin(&sched_lock);
		panic("%s: no owner", __func__);
	} else if (td2->td_kflags & (TDK_KSEREL | TDK_KSERELSIG)) {
		mtx_unlock_spin(&sched_lock);
		if (!(td2->td_kflags & TDK_WAKEUP)) {
			td2->td_kflags |= TDK_WAKEUP;
			if (td2->td_kflags & TDK_KSEREL)
				sleepq_remove(td2, &kg->kg_completed);
			else
				sleepq_remove(td2, &p->p_siglist);
		}
	} else {
		ku->ku_flags |= KUF_DOUPCALL;
		mtx_unlock_spin(&sched_lock);
	}
	PROC_UNLOCK(p);
	return (0);
}

/*
 * No new KSEG: first call: use current KSE, don't schedule an upcall
 * All other situations, do allocate max new KSEs and schedule an upcall.
 *
 * XXX should be changed so that 'first' behaviour lasts for as long
 * as you have not made a kse in this ksegrp. i.e. as long as we do not have
 * a mailbox..
 */
/* struct kse_create_args {
	struct kse_mailbox *mbx;
	int newgroup;
}; */
int
kse_create(struct thread *td, struct kse_create_args *uap)
{
	struct ksegrp *newkg;
	struct ksegrp *kg;
	struct proc *p;
	struct kse_mailbox mbx;
	struct kse_upcall *newku;
	int err, ncpus, sa = 0, first = 0;
	struct thread *newtd;

	p = td->td_proc;
	kg = td->td_ksegrp;
	if ((err = copyin(uap->mbx, &mbx, sizeof(mbx))))
		return (err);

	ncpus = mp_ncpus;
	if (virtual_cpu != 0)
		ncpus = virtual_cpu;
	/*
	 * If the new UTS mailbox says that this
	 * will be a BOUND lwp, then it had better
	 * have its thread mailbox already there.
	 * In addition, this ksegrp will be limited to
	 * a concurrency of 1. There is more on this later.
	 */
	if (mbx.km_flags & KMF_BOUND) {
		if (mbx.km_curthread == NULL) 
			return (EINVAL);
		ncpus = 1;
	} else {
		sa = TDP_SA;
	}

	PROC_LOCK(p);
	/*
	 * Processes using the other threading model can't
	 * suddenly start calling this one
	 */
	if ((p->p_flag & (P_SA|P_HADTHREADS)) == P_HADTHREADS) {
		PROC_UNLOCK(p);
		return (EINVAL);
	}

	/*
	 * Limit it to NCPU upcall contexts per ksegrp in any case.
	 * There is a small race here as we don't hold proclock
	 * until we inc the ksegrp count, but it's not really a big problem
	 * if we get one too many, but we save a proc lock.
	 */
	if ((!uap->newgroup) && (kg->kg_numupcalls >= ncpus)) {
		PROC_UNLOCK(p);
		return (EPROCLIM);
	}

	if (!(p->p_flag & P_SA)) {
		first = 1;
		p->p_flag |= P_SA|P_HADTHREADS;
	}

	PROC_UNLOCK(p);
	/*
	 * Now pay attention!
	 * If we are going to be bound, then we need to be either
	 * a new group, or the first call ever. In either
	 * case we will be creating (or be) the only thread in a group.
	 * and the concurrency will be set to 1.
	 * This is not quite right, as we may still make ourself 
	 * bound after making other ksegrps but it will do for now.
	 * The library will only try do this much.
	 */
	if (!sa && !(uap->newgroup || first))
		return (EINVAL);

	if (uap->newgroup) {
		newkg = ksegrp_alloc();
		bzero(&newkg->kg_startzero,
		    __rangeof(struct ksegrp, kg_startzero, kg_endzero));
		bcopy(&kg->kg_startcopy, &newkg->kg_startcopy,
		    __rangeof(struct ksegrp, kg_startcopy, kg_endcopy));
		sched_init_concurrency(newkg);
		PROC_LOCK(p);
		if (p->p_numksegrps >= max_groups_per_proc) {
			PROC_UNLOCK(p);
			ksegrp_free(newkg);
			return (EPROCLIM);
		}
		ksegrp_link(newkg, p);
		mtx_lock_spin(&sched_lock);
		sched_fork_ksegrp(td, newkg);
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
	} else {
		/*
		 * We want to make a thread in our own ksegrp.
		 * If we are just the first call, either kind
		 * is ok, but if not then either we must be 
		 * already an upcallable thread to make another,
		 * or a bound thread to make one of those.
		 * Once again, not quite right but good enough for now.. XXXKSE
		 */
		if (!first && ((td->td_pflags & TDP_SA) != sa))
			return (EINVAL);

		newkg = kg;
	}

	/* 
	 * This test is a bit "indirect".
	 * It might simplify things if we made a direct way of testing
	 * if a ksegrp has been worked on before.
	 * In the case of a bound request and the concurrency being set to 
	 * one, the concurrency will already be 1 so it's just inefficient
	 * but not dangerous to call this again. XXX
	 */
	if (newkg->kg_numupcalls == 0) {
		/*
		 * Initialize KSE group with the appropriate
		 * concurrency.
		 *
		 * For a multiplexed group, create as as much concurrency
		 * as the number of physical cpus.
		 * This increases concurrency in the kernel even if the
		 * userland is not MP safe and can only run on a single CPU.
		 * In an ideal world, every physical cpu should execute a
		 * thread.  If there is enough concurrency, threads in the
		 * kernel can be executed parallel on different cpus at
		 * full speed without being restricted by the number of
		 * upcalls the userland provides.
		 * Adding more upcall structures only increases concurrency
		 * in userland.
		 *
		 * For a bound thread group, because there is only one thread
		 * in the group, we only set the concurrency for the group 
		 * to 1.  A thread in this kind of group will never schedule
		 * an upcall when blocked.  This simulates pthread system
		 * scope thread behaviour.
		 */
		sched_set_concurrency(newkg, ncpus);
	}
	/* 
	 * Even bound LWPs get a mailbox and an upcall to hold it.
	 */
	newku = upcall_alloc();
	newku->ku_mailbox = uap->mbx;
	newku->ku_func = mbx.km_func;
	bcopy(&mbx.km_stack, &newku->ku_stack, sizeof(stack_t));

	/*
	 * For the first call this may not have been set.
	 * Of course nor may it actually be needed.
	 */
	if (td->td_standin == NULL)
		thread_alloc_spare(td);

	PROC_LOCK(p);
	mtx_lock_spin(&sched_lock);
	if (newkg->kg_numupcalls >= ncpus) {
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
		upcall_free(newku);
		return (EPROCLIM);
	}

	/*
	 * If we are the first time, and a normal thread,
	 * then transfer all the signals back to the 'process'.
	 * SA threading will make a special thread to handle them.
	 */
	if (first && sa) {
		SIGSETOR(p->p_siglist, td->td_siglist);
		SIGEMPTYSET(td->td_siglist);
		SIGFILLSET(td->td_sigmask);
		SIG_CANTMASK(td->td_sigmask);
	}

	/*
	 * Make the new upcall available to the ksegrp.
	 * It may or may not use it, but it's available.
	 */
	PROC_UNLOCK(p);
	upcall_link(newku, newkg);
	if (mbx.km_quantum)
		newkg->kg_upquantum = max(1, mbx.km_quantum / tick);

	/*
	 * Each upcall structure has an owner thread, find which
	 * one owns it.
	 */
	if (uap->newgroup) {
		/*
		 * Because the new ksegrp hasn't a thread,
		 * create an initial upcall thread to own it.
		 */
		newtd = thread_schedule_upcall(td, newku);
	} else {
		/*
		 * If the current thread hasn't an upcall structure,
		 * just assign the upcall to it.
		 * It'll just return.
		 */
		if (td->td_upcall == NULL) {
			newku->ku_owner = td;
			td->td_upcall = newku;
			newtd = td;
		} else {
			/*
			 * Create a new upcall thread to own it.
			 */
			newtd = thread_schedule_upcall(td, newku);
		}
	}
	mtx_unlock_spin(&sched_lock);

	/*
	 * Let the UTS instance know its LWPID.
	 * It doesn't really care. But the debugger will.
	 */
	suword32(&newku->ku_mailbox->km_lwp, newtd->td_tid);

	/*
	 * In the same manner, if the UTS has a current user thread, 
	 * then it is also running on this LWP so set it as well.
	 * The library could do that of course.. but why not..
	 */
	if (mbx.km_curthread)
		suword32(&mbx.km_curthread->tm_lwp, newtd->td_tid);

	
	if (sa) {
		newtd->td_pflags |= TDP_SA;
	} else {
		newtd->td_pflags &= ~TDP_SA;

		/*
		 * Since a library will use the mailbox pointer to 
		 * identify even a bound thread, and the mailbox pointer
		 * will never be allowed to change after this syscall
		 * for a bound thread, set it here so the library can
		 * find the thread after the syscall returns.
		 */
		newtd->td_mailbox = mbx.km_curthread;

		if (newtd != td) {
			/*
			 * If we did create a new thread then
			 * make sure it goes to the right place
			 * when it starts up, and make sure that it runs 
			 * at full speed when it gets there. 
			 * thread_schedule_upcall() copies all cpu state
			 * to the new thread, so we should clear single step
			 * flag here.
			 */
			cpu_set_upcall_kse(newtd, newku->ku_func,
				newku->ku_mailbox, &newku->ku_stack);
			if (p->p_flag & P_TRACED)
				ptrace_clear_single_step(newtd);
		}
	}
	
	/* 
	 * If we are starting a new thread, kick it off.
	 */
	if (newtd != td) {
		mtx_lock_spin(&sched_lock);
		setrunqueue(newtd, SRQ_BORING);
		mtx_unlock_spin(&sched_lock);
	}
	return (0);
}

/*
 * Initialize global thread allocation resources.
 */
void
kseinit(void)
{

	upcall_zone = uma_zcreate("UPCALL", sizeof(struct kse_upcall),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_CACHE, 0);
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
 * Reap zombie kse resource.
 */
void
kse_GC(void)
{
	struct kse_upcall *ku_first, *ku_next;

	/*
	 * Don't even bother to lock if none at this instant,
	 * we really don't care about the next instant..
	 */
	if (!TAILQ_EMPTY(&zombie_upcalls)) {
		mtx_lock_spin(&kse_zombie_lock);
		ku_first = TAILQ_FIRST(&zombie_upcalls);
		if (ku_first)
			TAILQ_INIT(&zombie_upcalls);
		mtx_unlock_spin(&kse_zombie_lock);
		while (ku_first) {
			ku_next = TAILQ_NEXT(ku_first, ku_link);
			upcall_free(ku_first);
			ku_first = ku_next;
		}
	}
}

/*
 * Store the thread context in the UTS's mailbox.
 * then add the mailbox at the head of a list we are building in user space.
 * The list is anchored in the ksegrp structure.
 */
int
thread_export_context(struct thread *td, int willexit)
{
	struct proc *p;
	struct ksegrp *kg;
	uintptr_t mbx;
	void *addr;
	int error = 0, sig;
	mcontext_t mc;

	p = td->td_proc;
	kg = td->td_ksegrp;

	/*
	 * Post sync signal, or process SIGKILL and SIGSTOP.
	 * For sync signal, it is only possible when the signal is not
	 * caught by userland or process is being debugged.
	 */
	PROC_LOCK(p);
	if (td->td_flags & TDF_NEEDSIGCHK) {
		mtx_lock_spin(&sched_lock);
		td->td_flags &= ~TDF_NEEDSIGCHK;
		mtx_unlock_spin(&sched_lock);
		mtx_lock(&p->p_sigacts->ps_mtx);
		while ((sig = cursig(td)) != 0)
			postsig(sig);
		mtx_unlock(&p->p_sigacts->ps_mtx);
	}
	if (willexit)
		SIGFILLSET(td->td_sigmask);
	PROC_UNLOCK(p);

	/* Export the user/machine context. */
	get_mcontext(td, &mc, 0);
	addr = (void *)(&td->td_mailbox->tm_context.uc_mcontext);
	error = copyout(&mc, addr, sizeof(mcontext_t));
	if (error)
		goto bad;

	addr = (caddr_t)(&td->td_mailbox->tm_lwp);
	if (suword32(addr, 0)) {
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
	sigexit(td, SIGILL);
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

	if (!(td->td_pflags & TDP_SA))
		return (0);
	if (user) {
		/* Current always do via ast() */
		mtx_lock_spin(&sched_lock);
		td->td_flags |= TDF_ASTPENDING;
		mtx_unlock_spin(&sched_lock);
		td->td_uuticks++;
	} else if (td->td_mailbox != NULL)
		td->td_usticks++;
	return (0);
}

/*
 * Export state clock ticks for userland
 */
static int
thread_update_usr_ticks(struct thread *td)
{
	struct proc *p = td->td_proc;
	caddr_t addr;
	u_int uticks;

	if (td->td_mailbox == NULL)
		return (-1);

	if ((uticks = td->td_uuticks) != 0) {
		td->td_uuticks = 0;
		addr = (caddr_t)&td->td_mailbox->tm_uticks;
		if (suword32(addr, uticks+fuword32(addr)))
			goto error;
	}
	if ((uticks = td->td_usticks) != 0) {
		td->td_usticks = 0;
		addr = (caddr_t)&td->td_mailbox->tm_sticks;
		if (suword32(addr, uticks+fuword32(addr)))
			goto error;
	}
	return (0);

error:
	PROC_LOCK(p);
	psignal(p, SIGSEGV);
	PROC_UNLOCK(p);
	return (-2);
}

/*
 * This function is intended to be used to initialize a spare thread
 * for upcall. Initialize thread's large data area outside sched_lock
 * for thread_schedule_upcall(). The crhold is also here to get it out
 * from the schedlock as it has a mutex op itself.
 * XXX BUG.. we need to get the cr ref after the thread has 
 * checked and chenged its own, not 6 months before...  
 */
void
thread_alloc_spare(struct thread *td)
{
	struct thread *spare;

	if (td->td_standin)
		return;
	spare = thread_alloc();
	td->td_standin = spare;
	bzero(&spare->td_startzero,
	    __rangeof(struct thread, td_startzero, td_endzero));
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
	/*
	 * Bzero already done in thread_alloc_spare() because we can't
	 * do the crhold here because we are in schedlock already.
	 */
	bcopy(&td->td_startcopy, &td2->td_startcopy,
	    __rangeof(struct thread, td_startcopy, td_endcopy));
	thread_link(td2, ku->ku_ksegrp);
	/* inherit parts of blocked thread's context as a good template */
	cpu_set_upcall(td2, td);
	/* Let the new thread become owner of the upcall */
	ku->ku_owner   = td2;
	td2->td_upcall = ku;
	td2->td_flags  = 0;
	td2->td_pflags = TDP_SA|TDP_UPCALLING;
	td2->td_state  = TDS_CAN_RUN;
	td2->td_inhibitors = 0;
	SIGFILLSET(td2->td_sigmask);
	SIG_CANTMASK(td2->td_sigmask);
	sched_fork_thread(td, td2);
	return (td2);	/* bogus.. should be a void function */
}

/*
 * It is only used when thread generated a trap and process is being
 * debugged.
 */
void
thread_signal_add(struct thread *td, int sig)
{
	struct proc *p;
	siginfo_t siginfo;
	struct sigacts *ps;
	int error;

	p = td->td_proc;
	PROC_LOCK_ASSERT(p, MA_OWNED);
	ps = p->p_sigacts;
	mtx_assert(&ps->ps_mtx, MA_OWNED);

	cpu_thread_siginfo(sig, 0, &siginfo);
	mtx_unlock(&ps->ps_mtx);
	SIGADDSET(td->td_sigmask, sig);
	PROC_UNLOCK(p);
	error = copyout(&siginfo, &td->td_mailbox->tm_syncsig, sizeof(siginfo));
	if (error) {
		PROC_LOCK(p);
		sigexit(td, SIGSEGV);
	}
	PROC_LOCK(p);
	mtx_lock(&ps->ps_mtx);
}
#include "opt_sched.h"
struct thread *
thread_switchout(struct thread *td, int flags, struct thread *nextthread)
{
	struct kse_upcall *ku;
	struct thread *td2;

	mtx_assert(&sched_lock, MA_OWNED);

	/*
	 * If the outgoing thread is in threaded group and has never
	 * scheduled an upcall, decide whether this is a short
	 * or long term event and thus whether or not to schedule
	 * an upcall.
	 * If it is a short term event, just suspend it in
	 * a way that takes its KSE with it.
	 * Select the events for which we want to schedule upcalls.
	 * For now it's just sleep or if thread is suspended but
	 * process wide suspending flag is not set (debugger
	 * suspends thread).
	 * XXXKSE eventually almost any inhibition could do.
	 */
	if (TD_CAN_UNBIND(td) && (td->td_standin) &&
	    (TD_ON_SLEEPQ(td) || (TD_IS_SUSPENDED(td) &&
	     !P_SHOULDSTOP(td->td_proc)))) {
		/*
		 * Release ownership of upcall, and schedule an upcall
		 * thread, this new upcall thread becomes the owner of
		 * the upcall structure. It will be ahead of us in the
		 * run queue, so as we are stopping, it should either
		 * start up immediatly, or at least before us if
		 * we release our slot.
		 */
		ku = td->td_upcall;
		ku->ku_owner = NULL;
		td->td_upcall = NULL;
		td->td_pflags &= ~TDP_CAN_UNBIND;
		td2 = thread_schedule_upcall(td, ku);
		if (flags & SW_INVOL || nextthread) {
			setrunqueue(td2, SRQ_YIELDING);
		} else {
			/* Keep up with reality.. we have one extra thread 
			 * in the picture.. and it's 'running'.
			 */
			return td2;
		}
	}
	return (nextthread);
}

/*
 * Setup done on the thread when it enters the kernel.
 */
void
thread_user_enter(struct thread *td)
{
	struct proc *p = td->td_proc;
	struct ksegrp *kg;
	struct kse_upcall *ku;
	struct kse_thr_mailbox *tmbx;
	uint32_t flags;

	/*
	 * First check that we shouldn't just abort. we
	 * can suspend it here or just exit.
	 */
	if (__predict_false(P_SHOULDSTOP(p))) {
		PROC_LOCK(p);
		thread_suspend_check(0);
		PROC_UNLOCK(p);
	}

	if (!(td->td_pflags & TDP_SA))
		return;

	/*
	 * If we are doing a syscall in a KSE environment,
	 * note where our mailbox is.
	 */

	kg = td->td_ksegrp;
	ku = td->td_upcall;

	KASSERT(ku != NULL, ("no upcall owned"));
	KASSERT(ku->ku_owner == td, ("wrong owner"));
	KASSERT(!TD_CAN_UNBIND(td), ("can unbind"));

	if (td->td_standin == NULL)
		thread_alloc_spare(td);
	ku->ku_mflags = fuword32((void *)&ku->ku_mailbox->km_flags);
	tmbx = (void *)fuword((void *)&ku->ku_mailbox->km_curthread);
	if ((tmbx == NULL) || (tmbx == (void *)-1L) ||
	    (ku->ku_mflags & KMF_NOUPCALL)) {
		td->td_mailbox = NULL;
	} else {
		flags = fuword32(&tmbx->tm_flags);
		/*
		 * On some architectures, TP register points to thread
		 * mailbox but not points to kse mailbox, and userland
		 * can not atomically clear km_curthread, but can
		 * use TP register, and set TMF_NOUPCALL in thread
		 * flag	to indicate a critical region.
		 */
		if (flags & TMF_NOUPCALL) {
			td->td_mailbox = NULL;
		} else {
			td->td_mailbox = tmbx;
			td->td_pflags |= TDP_CAN_UNBIND;
			if (__predict_false(p->p_flag & P_TRACED)) {
				flags = fuword32(&tmbx->tm_dflags);
				if (flags & TMDF_SUSPEND) {
					mtx_lock_spin(&sched_lock);
					/* fuword can block, check again */
					if (td->td_upcall)
						ku->ku_flags |= KUF_DOUPCALL;
					mtx_unlock_spin(&sched_lock);
				}
			}
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
	struct kse_upcall *ku;
	struct ksegrp *kg, *kg2;
	struct proc *p;
	struct timespec ts;
	int error = 0, upcalls, uts_crit;

	/* Nothing to do with bound thread */
	if (!(td->td_pflags & TDP_SA))
		return (0);

	/*
	 * Update stat clock count for userland
	 */
	if (td->td_mailbox != NULL) {
		thread_update_usr_ticks(td);
		uts_crit = 0;
	} else {
		uts_crit = 1;
	}

	p = td->td_proc;
	kg = td->td_ksegrp;
	ku = td->td_upcall;

	/*
	 * Optimisation:
	 * This thread has not started any upcall.
	 * If there is no work to report other than ourself,
	 * then it can return direct to userland.
	 */
	if (TD_CAN_UNBIND(td)) {
		td->td_pflags &= ~TDP_CAN_UNBIND;
		if ((td->td_flags & TDF_NEEDSIGCHK) == 0 &&
		    (kg->kg_completed == NULL) &&
		    (ku->ku_flags & KUF_DOUPCALL) == 0 &&
		    (kg->kg_upquantum && ticks < kg->kg_nextupcall)) {
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
		thread_export_context(td, 0);
		/*
		 * There is something to report, and we own an upcall
		 * structure, we can go to userland.
		 * Turn ourself into an upcall thread.
		 */
		td->td_pflags |= TDP_UPCALLING;
	} else if (td->td_mailbox && (ku == NULL)) {
		thread_export_context(td, 1);
		PROC_LOCK(p);
		if (kg->kg_upsleeps)
			wakeup(&kg->kg_completed);
		WITNESS_WARN(WARN_PANIC, &p->p_mtx.mtx_object,
		    "thread exiting in userret");
		mtx_lock_spin(&sched_lock);
		thread_stopped(p);
		thread_exit();
		/* NOTREACHED */
	}

	KASSERT(ku != NULL, ("upcall is NULL"));
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
			    "maxthreads", 0)) {
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

	if (td->td_pflags & TDP_UPCALLING) {
		uts_crit = 0;
		kg->kg_nextupcall = ticks + kg->kg_upquantum;
		/*
		 * There is no more work to do and we are going to ride
		 * this thread up to userland as an upcall.
		 * Do the last parts of the setup needed for the upcall.
		 */
		CTR3(KTR_PROC, "userret: upcall thread %p (pid %d, %s)",
		    td, td->td_proc->p_pid, td->td_proc->p_comm);

		td->td_pflags &= ~TDP_UPCALLING;
		if (ku->ku_flags & KUF_DOUPCALL) {
			mtx_lock_spin(&sched_lock);
			ku->ku_flags &= ~KUF_DOUPCALL;
			mtx_unlock_spin(&sched_lock);
		}
		/*
		 * Set user context to the UTS
		 */
		if (!(ku->ku_mflags & KMF_NOUPCALL)) {
			cpu_set_upcall_kse(td, ku->ku_func, ku->ku_mailbox,
				&ku->ku_stack);
			if (p->p_flag & P_TRACED)
				ptrace_clear_single_step(td);
			error = suword32(&ku->ku_mailbox->km_lwp,
					td->td_tid);
			if (error)
				goto out;
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
		PROC_LOCK(p);
		psignal(p, SIGSEGV);
		PROC_UNLOCK(p);
	} else {
		/*
		 * Optimisation:
		 * Ensure that we have a spare thread available,
		 * for when we re-enter the kernel.
		 */
		if (td->td_standin == NULL)
			thread_alloc_spare(td);
	}

	ku->ku_mflags = 0;
	td->td_mailbox = NULL;
	td->td_usticks = 0;
	return (error);	/* go sync */
}

/*
 * called after ptrace resumed a process, force all
 * virtual CPUs to schedule upcall for SA process,
 * because debugger may have changed something in userland,
 * we should notice UTS as soon as possible.
 */
void
thread_continued(struct proc *p)
{
	struct ksegrp *kg;
	struct kse_upcall *ku;
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);
	KASSERT(P_SHOULDSTOP(p), ("process not stopped"));

	if (!(p->p_flag & P_SA))
		return;

	if (p->p_flag & P_TRACED) {
		FOREACH_KSEGRP_IN_PROC(p, kg) {
			td = TAILQ_FIRST(&kg->kg_threads);
			if (td == NULL)
				continue;
			/* not a SA group, nothing to do */
			if (!(td->td_pflags & TDP_SA))
				continue;
			FOREACH_UPCALL_IN_GROUP(kg, ku) {
				mtx_lock_spin(&sched_lock);
				ku->ku_flags |= KUF_DOUPCALL;
				mtx_unlock_spin(&sched_lock);
				wakeup(&kg->kg_completed);
			}
		}
	}
}
