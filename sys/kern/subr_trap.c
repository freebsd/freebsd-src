/*-
 * Copyright (C) 1994, David Greenman
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the University of Utah, and William Jolitz.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)trap.c	7.4 (Berkeley) 5/13/91
 * $FreeBSD$
 */

#include "opt_mac.h"
#ifdef __i386__
#include "opt_npx.h"
#endif

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mac.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/kse.h>
#include <sys/ktr.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>
#include <machine/cpu.h>
#include <machine/pcb.h>

/*
 * Define the code needed before returning to user mode, for
 * trap and syscall.
 *
 * MPSAFE
 */
void
userret(td, frame, oticks)
	struct thread *td;
	struct trapframe *frame;
	u_int oticks;
{
	struct proc *p = td->td_proc;
	struct kse *ke = td->td_kse; 

	CTR3(KTR_SYSC, "userret: thread %p (pid %d, %s)", td, p->p_pid,
            p->p_comm);
#ifdef INVARIANTS
	/* Check that we called signotify() enough. */
	mtx_lock(&Giant);
	PROC_LOCK(p);
	mtx_lock_spin(&sched_lock);
	if (SIGPENDING(p) && ((p->p_sflag & PS_NEEDSIGCHK) == 0 ||
	    (td->td_kse->ke_flags & KEF_ASTPENDING) == 0))
		printf("failed to set signal flags properly for ast()\n");
	mtx_unlock_spin(&sched_lock);
	PROC_UNLOCK(p);
	mtx_unlock(&Giant);
#endif

#ifdef MAC
	mac_thread_userret(td);
#endif

	/*
	 * Let the scheduler adjust our priority etc.
	 */
	sched_userret(td);

	/*
	 * Do special thread processing, e.g. suspension checking, upcall
	 * tweaking and such.
	 */
	if (p->p_flag & P_KSES) {
		thread_userret(td, frame);
		/* printf("KSE thread returned"); */
	}

	/*
	 * Charge system time if profiling.
	 *
	 * XXX should move PS_PROFIL to a place that can obviously be
	 * accessed safely without sched_lock.
	 */
	if (p->p_sflag & PS_PROFIL) {
		quad_t ticks;

		mtx_lock_spin(&sched_lock);
		ticks = ke->ke_sticks - oticks;
		mtx_unlock_spin(&sched_lock);
		addupc_task(ke, TRAPF_PC(frame), (u_int)ticks * psratio);
	}
}

/*
 * Process an asynchronous software trap.
 * This is relatively easy.
 * This function will return with preemption disabled.
 */
void
ast(struct trapframe *framep)
{
	struct thread *td;
	struct proc *p;
	struct kse *ke;
	struct ksegrp *kg;
	struct rlimit *rlim;
	u_int prticks, sticks;
	int sflag;
	int flags;
	int sig;
#if defined(DEV_NPX) && !defined(SMP)
	int ucode;
#endif

	td = curthread;
	p = td->td_proc;
	kg = td->td_ksegrp;

	CTR3(KTR_SYSC, "ast: thread %p (pid %d, %s)", td, p->p_pid,
            p->p_comm);
	KASSERT(TRAPF_USERMODE(framep), ("ast in kernel mode"));
#ifdef WITNESS
	if (witness_list(td))
		panic("Returning to user mode with mutex(s) held");
#endif
	mtx_assert(&Giant, MA_NOTOWNED);
	mtx_assert(&sched_lock, MA_NOTOWNED);
	td->td_frame = framep;

	/*
	 * This updates the p_sflag's for the checks below in one
	 * "atomic" operation with turning off the astpending flag.
	 * If another AST is triggered while we are handling the
	 * AST's saved in sflag, the astpending flag will be set and
	 * ast() will be called again.
	 */
	mtx_lock_spin(&sched_lock);
	ke = td->td_kse;
	sticks = ke->ke_sticks;
	flags = ke->ke_flags;
	sflag = p->p_sflag;
	p->p_sflag &= ~(PS_ALRMPEND | PS_NEEDSIGCHK | PS_PROFPEND | PS_XCPU);
	ke->ke_flags &= ~(KEF_ASTPENDING | KEF_NEEDRESCHED | KEF_OWEUPC);
	cnt.v_soft++;
	prticks = 0;
	if (flags & KEF_OWEUPC && sflag & PS_PROFIL) {
		prticks = p->p_stats->p_prof.pr_ticks;
		p->p_stats->p_prof.pr_ticks = 0;
	}
	mtx_unlock_spin(&sched_lock);
	/*
	 * XXXKSE While the fact that we owe a user profiling
	 * tick is stored per KSE in this code, the statistics
	 * themselves are still stored per process.
	 * This should probably change, by which I mean that
	 * possibly the location of both might change.
	 */

	if (td->td_ucred != p->p_ucred) 
		cred_update_thread(td);
	if (flags & KEF_OWEUPC && sflag & PS_PROFIL)
		addupc_task(ke, p->p_stats->p_prof.pr_addr, prticks);
	if (sflag & PS_ALRMPEND) {
		PROC_LOCK(p);
		psignal(p, SIGVTALRM);
		PROC_UNLOCK(p);
	}
#if defined(DEV_NPX) && !defined(SMP)
	if (PCPU_GET(curpcb)->pcb_flags & PCB_NPXTRAP) {
		atomic_clear_int(&PCPU_GET(curpcb)->pcb_flags,
		    PCB_NPXTRAP);
		ucode = npxtrap();
		if (ucode != -1) {
			trapsignal(p, SIGFPE, ucode);
		}
	}
#endif
	if (sflag & PS_PROFPEND) {
		PROC_LOCK(p);
		psignal(p, SIGPROF);
		PROC_UNLOCK(p);
	}
	if (sflag & PS_XCPU) {
		PROC_LOCK(p);
		rlim = &p->p_rlimit[RLIMIT_CPU];
		if (p->p_runtime.sec >= rlim->rlim_max)
			killproc(p, "exceeded maximum CPU limit");
		else {
			psignal(p, SIGXCPU);
			mtx_lock_spin(&sched_lock);
			if (p->p_cpulimit < rlim->rlim_max)
				p->p_cpulimit += 5;
			mtx_unlock_spin(&sched_lock);
		}
		PROC_UNLOCK(p);
	}
	if (flags & KEF_NEEDRESCHED) {
		mtx_lock_spin(&sched_lock);
		sched_prio(td, kg->kg_user_pri);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		mtx_unlock_spin(&sched_lock);
	}
	if (sflag & PS_NEEDSIGCHK) {
		PROC_LOCK(p);
		while ((sig = cursig(td)) != 0)
			postsig(sig);
		PROC_UNLOCK(p);
	}

	userret(td, framep, sticks);
#ifdef DIAGNOSTIC
	cred_free_thread(td);
#endif
	mtx_assert(&Giant, MA_NOTOWNED);
}
