/*-
 * Copyright (c) 2000, All rights reserved.  See /usr/src/COPYRIGHT
 *
 * $FreeBSD$
 */

#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/smp.h>
#include <sys/unistd.h>
#ifdef KTRACE
#include <sys/uio.h>
#include <sys/ktrace.h>
#endif

static void idle_setup(void *dummy);
SYSINIT(idle_setup, SI_SUB_SCHED_IDLE, SI_ORDER_FIRST, idle_setup, NULL)

static void idle_proc(void *dummy);

/*
 * Setup per-cpu idle process contexts.  The AP's shouldn't be running or
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
	int error;

#ifdef SMP
	SLIST_FOREACH(pc, &cpuhead, pc_allcpu) {
		error = kthread_create(idle_proc, NULL, &p,
		    RFSTOPPED | RFHIGHPID, "idle: cpu%d", pc->pc_cpuid);
		pc->pc_idlethread = &p->p_thread;
		if (pc->pc_curthread == NULL)
			pc->pc_curthread = pc->pc_idlethread;
#else
		error = kthread_create(idle_proc, NULL, &p,
		    RFSTOPPED | RFHIGHPID, "idle");
		PCPU_SET(idlethread, &p->p_thread);
#endif
		if (error)
			panic("idle_setup: kthread_create error %d\n", error);

		p->p_flag |= P_NOLOAD;
		p->p_stat = SRUN;
#ifdef SMP
	}
#endif
}

/*
 * idle process context
 */
static void
idle_proc(void *dummy)
{
#ifdef DIAGNOSTIC
	int count;
#endif

	for (;;) {
		mtx_assert(&Giant, MA_NOTOWNED);

#ifdef DIAGNOSTIC
		count = 0;

		while (count >= 0 && procrunnable() == 0) {
#else
		while (procrunnable() == 0) {
#endif
		/*
		 * This is a good place to put things to be done in
		 * the background, including sanity checks.
		 */

#ifdef DIAGNOSTIC
			if (count++ < 0)
				CTR0(KTR_PROC, "idle_proc: timed out waiting"
				    " for a process");
#endif

#ifdef __i386__
			cpu_idle();
#endif
		}

		mtx_lock_spin(&sched_lock);
		curproc->p_stats->p_ru.ru_nvcsw++;
		mi_switch();
		mtx_unlock_spin(&sched_lock);
	}
}
