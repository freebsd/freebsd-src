/*-
 * Copyright (c) 2000, All rights reserved.  See /usr/src/COPYRIGHT
 *
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
		if (pc->pc_curthread == NULL) {
			pc->pc_curthread = pc->pc_idlethread;
			pc->pc_idlethread->td_critnest = 0;
		}
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
		p->p_state = PRS_NORMAL;
		td = FIRST_THREAD_IN_PROC(p);
		td->td_state = TDS_CAN_RUN;
		td->td_flags |= TDF_IDLETD;
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

	td = curthread;
	p = td->td_proc;
	for (;;) {
		mtx_assert(&Giant, MA_NOTOWNED);

		while (sched_runnable() == 0)
			cpu_idle();

		mtx_lock_spin(&sched_lock);
		td->td_state = TDS_CAN_RUN;
		p->p_stats->p_ru.ru_nvcsw++;
		mi_switch();
		mtx_unlock_spin(&sched_lock);
	}
}
