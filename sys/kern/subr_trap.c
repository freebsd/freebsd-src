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

#ifdef __i386__
#include "opt_npx.h"
#endif

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
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
	struct ksegrp *kg = td->td_ksegrp;
	int sig;

	mtx_lock(&Giant);
	PROC_LOCK(p);
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);
	PROC_UNLOCK(p);
	mtx_unlock(&Giant);

	mtx_lock_spin(&sched_lock);
	td->td_priority = kg->kg_user_pri;
	if (ke->ke_flags & KEF_NEEDRESCHED) {
		DROP_GIANT();
		setrunqueue(td);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		mtx_unlock_spin(&sched_lock);
		PICKUP_GIANT();
		mtx_lock(&Giant);
		PROC_LOCK(p);
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
		mtx_unlock(&Giant);
		PROC_UNLOCK(p);
		mtx_lock_spin(&sched_lock);
	}

	/*
	 * Charge system time if profiling.
	 */
	if (p->p_sflag & PS_PROFIL) {
		quad_t ticks;

		ticks = ke->ke_sticks - oticks;
		mtx_unlock_spin(&sched_lock);
		addupc_task(ke, TRAPF_PC(frame), (u_int)ticks * psratio);
	} else
		mtx_unlock_spin(&sched_lock);
}

/*
 * Process an asynchronous software trap.
 * This is relatively easy.
 * This function will return with preemption disabled.
 */
void
ast(framep)
	struct trapframe *framep;
{
	struct thread *td = curthread;
	struct proc *p = td->td_proc;
	struct kse *ke = td->td_kse;
	u_int prticks, sticks;
	critical_t s;
	int sflag;
	int flags;
#if defined(DEV_NPX) && !defined(SMP)
	int ucode;
#endif

	KASSERT(TRAPF_USERMODE(framep), ("ast in kernel mode"));
#ifdef WITNESS
	if (witness_list(td))
		panic("Returning to user mode with mutex(s) held");
#endif
	mtx_assert(&Giant, MA_NOTOWNED);
	prticks = 0;		/* XXX: Quiet warning. */
	s = cpu_critical_enter();
	while ((ke->ke_flags & (KEF_ASTPENDING | KEF_NEEDRESCHED)) != 0) {
		cpu_critical_exit(s);
		td->td_frame = framep;
		/*
		 * This updates the p_sflag's for the checks below in one
		 * "atomic" operation with turning off the astpending flag.
		 * If another AST is triggered while we are handling the
		 * AST's saved in sflag, the astpending flag will be set and
		 * we will loop again.
		 */
		mtx_lock_spin(&sched_lock);
		sticks = ke->ke_sticks;
		sflag = p->p_sflag;
		flags = ke->ke_flags;
		p->p_sflag &= ~(PS_PROFPEND | PS_ALRMPEND);
		ke->ke_flags &= ~(KEF_OWEUPC | KEF_ASTPENDING);
		cnt.v_soft++;
		if (flags & KEF_OWEUPC && sflag & PS_PROFIL) {
			prticks = p->p_stats->p_prof.pr_ticks;
			p->p_stats->p_prof.pr_ticks = 0;
		}
		mtx_unlock_spin(&sched_lock);

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

		userret(td, framep, sticks);
#ifdef DIAGNOSTIC
		cred_free_thread(td);
#endif
		s = cpu_critical_enter();
	}
	mtx_assert(&Giant, MA_NOTOWNED);
	/*
	 * We need to keep interrupts disabled so that if any further AST's
	 * come in, the interrupt they come in on will be delayed until we
	 * finish returning to userland.  We assume that the return to userland
	 * will perform the equivalent of cpu_critical_exit().
	 */
}
