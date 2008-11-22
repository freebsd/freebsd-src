/*-
 * Copyright (C) 2000-2004 The FreeBSD Project. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/unistd.h>
#ifdef SMP
#include <sys/smp.h>
#endif

static void idle_setup(void *dummy);
SYSINIT(idle_setup, SI_SUB_SCHED_IDLE, SI_ORDER_FIRST, idle_setup, NULL)

static void idle_proc(void *dummy);

/*
 * Set up per-cpu idle process contexts.  The AP's shouldn't be running or
 * accessing their idle processes at this point, so don't bother with
 * locking.
 */
static void
idle_setup(void *dummy)
{
#ifdef SMP
	struct pcpu *pc;
#endif
	struct proc *p;
	struct thread *td;
	int error;

#ifdef SMP
	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		error = kthread_create(idle_proc, NULL, &p,
		    RFSTOPPED | RFHIGHPID, 0, "idle: cpu%d", pc->pc_cpuid);
		pc->pc_idlethread = FIRST_THREAD_IN_PROC(p);
#else
		error = kthread_create(idle_proc, NULL, &p,
		    RFSTOPPED | RFHIGHPID, 0, "idle");
		PCPU_SET(idlethread, FIRST_THREAD_IN_PROC(p));
#endif
		if (error)
			panic("idle_setup: kthread_create error %d\n", error);

		PROC_LOCK(p);
		p->p_flag |= P_NOLOAD;
		mtx_lock_spin(&sched_lock);
		td = FIRST_THREAD_IN_PROC(p);
		TD_SET_CAN_RUN(td);
		td->td_flags |= TDF_IDLETD;
		sched_class(td->td_ksegrp, PRI_IDLE);
		sched_prio(td, PRI_MAX_IDLE);
		mtx_unlock_spin(&sched_lock);
		PROC_UNLOCK(p);
#ifdef SMP
	}
#endif
}

/*
 * The actual idle process.
 */
static void
idle_proc(void *dummy)
{
	struct proc *p;
	struct thread *td;
#ifdef SMP
	cpumask_t mycpu;
#endif

	td = curthread;
	p = td->td_proc;
#ifdef SMP
	mycpu = PCPU_GET(cpumask);
	mtx_lock_spin(&sched_lock);
	idle_cpus_mask |= mycpu;
	mtx_unlock_spin(&sched_lock);
#endif
	for (;;) {
		mtx_assert(&Giant, MA_NOTOWNED);

		while (sched_runnable() == 0)
			cpu_idle();

		mtx_lock_spin(&sched_lock);
#ifdef SMP
		idle_cpus_mask &= ~mycpu;
#endif
		mi_switch(SW_VOL, NULL);
#ifdef SMP
		idle_cpus_mask |= mycpu;
#endif
		mtx_unlock_spin(&sched_lock);
	}
}
