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
 */
void
userret(p, frame, oticks)
	struct proc *p;
	struct trapframe *frame;
	u_quad_t oticks;
{
	int sig;

	PROC_LOCK(p);
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);

	mtx_lock_spin(&sched_lock);
	PROC_UNLOCK_NOSWITCH(p);
	p->p_pri.pri_level = p->p_pri.pri_user;
	if (resched_wanted(p)) {
		/*
		 * Since we are curproc, a clock interrupt could
		 * change our priority without changing run queues
		 * (the running process is not kept on a run queue).
		 * If this happened after we setrunqueue ourselves but
		 * before we switch()'ed, we might not be on the queue
		 * indicated by our priority.
		 */
		DROP_GIANT_NOSWITCH();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		mtx_unlock_spin(&sched_lock);
		PICKUP_GIANT();
		PROC_LOCK(p);
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
		mtx_lock_spin(&sched_lock);
		PROC_UNLOCK_NOSWITCH(p);
	}

	/*
	 * Charge system time if profiling.
	 */
	if (p->p_sflag & PS_PROFIL) {
		mtx_unlock_spin(&sched_lock);
		addupc_task(p, TRAPF_PC(frame),
			    (u_int)(p->p_sticks - oticks) * psratio);
	} else
		mtx_unlock_spin(&sched_lock);
}

/*
 * Process an asynchronous software trap.
 * This is relatively easy.
 */
void
ast(framep)
	struct trapframe *framep;
{
	struct proc *p = CURPROC;
	u_quad_t sticks;
#if defined(DEV_NPX) && !defined(SMP)
	int ucode;
#endif

	KASSERT(TRAPF_USERMODE(framep), ("ast in kernel mode"));

	/*
	 * We check for a pending AST here rather than in the assembly as
	 * acquiring and releasing mutexes in assembly is not fun.
	 */
	mtx_lock_spin(&sched_lock);
	if (!(astpending(p) || resched_wanted(p))) {
		mtx_unlock_spin(&sched_lock);
		return;
	}

	sticks = p->p_sticks;
	p->p_frame = framep;

	astoff(p);
	cnt.v_soft++;
	mtx_intr_enable(&sched_lock);
	if (p->p_sflag & PS_OWEUPC) {
		p->p_sflag &= ~PS_OWEUPC;
		mtx_unlock_spin(&sched_lock);
		mtx_lock(&Giant);
		addupc_task(p, p->p_stats->p_prof.pr_addr,
			    p->p_stats->p_prof.pr_ticks);
		mtx_lock_spin(&sched_lock);
	}
	if (p->p_sflag & PS_ALRMPEND) {
		p->p_sflag &= ~PS_ALRMPEND;
		mtx_unlock_spin(&sched_lock);
		PROC_LOCK(p);
		psignal(p, SIGVTALRM);
		PROC_UNLOCK(p);
		mtx_lock_spin(&sched_lock);
	}
#if defined(DEV_NPX) && !defined(SMP)
	if (PCPU_GET(curpcb)->pcb_flags & PCB_NPXTRAP) {
		PCPU_GET(curpcb)->pcb_flags &= ~PCB_NPXTRAP;
		mtx_unlock_spin(&sched_lock);
		ucode = npxtrap();
		if (ucode != -1) {
			if (!mtx_owned(&Giant))
				mtx_lock(&Giant);
			trapsignal(p, SIGFPE, ucode);
		}
		mtx_lock_spin(&sched_lock);
	}
#endif
	if (p->p_sflag & PS_PROFPEND) {
		p->p_sflag &= ~PS_PROFPEND;
		mtx_unlock_spin(&sched_lock);
		PROC_LOCK(p);
		psignal(p, SIGPROF);
		PROC_UNLOCK(p);
	} else
		mtx_unlock_spin(&sched_lock);

	userret(p, framep, sticks);

	if (mtx_owned(&Giant))
		mtx_unlock(&Giant);
}
