/*-
 * Copyright (c) 1994 John Dyson
 * Copyright (c) 2001 Matt Dillon
 *
 * All rights reserved.  Terms for use and redistribution
 * are covered by the BSD Copyright as found in /usr/src/COPYRIGHT.
 *
 *	from: @(#)vm_machdep.c	7.3 (Berkeley) 5/13/91
 *	Utah $Hdr: vm_machdep.c 1.16.1.1 89/06/23$
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/vmmeter.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/kthread.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

SYSCTL_DECL(_vm_stats_misc);

static int cnt_prezero;
SYSCTL_INT(_vm_stats_misc, OID_AUTO,
	cnt_prezero, CTLFLAG_RD, &cnt_prezero, 0, "");

static int idlezero_enable = 1;
SYSCTL_INT(_vm, OID_AUTO, idlezero_enable, CTLFLAG_RW, &idlezero_enable, 0, "");
TUNABLE_INT("vm.idlezero_enable", &idlezero_enable);

static int idlezero_maxrun = 16;
SYSCTL_INT(_vm, OID_AUTO, idlezero_maxrun, CTLFLAG_RW, &idlezero_maxrun, 0, "");
TUNABLE_INT("vm.idlezero_maxrun", &idlezero_maxrun);

/*
 * Implement the pre-zeroed page mechanism.
 */

#define ZIDLE_LO(v)	((v) * 2 / 3)
#define ZIDLE_HI(v)	((v) * 4 / 5)

static int zero_state;

static int
vm_page_zero_check(void)
{

	if (!idlezero_enable)
		return 0;
	/*
	 * Attempt to maintain approximately 1/2 of our free pages in a
	 * PG_ZERO'd state.   Add some hysteresis to (attempt to) avoid
	 * generally zeroing a page when the system is near steady-state.
	 * Otherwise we might get 'flutter' during disk I/O / IPC or 
	 * fast sleeps.  We also do not want to be continuously zeroing
	 * pages because doing so may flush our L1 and L2 caches too much.
	 */
	if (zero_state && vm_page_zero_count >= ZIDLE_LO(cnt.v_free_count))
		return 0;
	if (vm_page_zero_count >= ZIDLE_HI(cnt.v_free_count))
		return 0;
	return 1;
}

static int
vm_page_zero_idle(void)
{
	static int free_rover;
	vm_page_t m;

	mtx_lock_spin(&vm_page_queue_free_mtx);
	zero_state = 0;
	m = vm_pageq_find(PQ_FREE, free_rover, FALSE);
	if (m != NULL && (m->flags & PG_ZERO) == 0) {
		vm_pageq_remove_nowakeup(m);
		mtx_unlock_spin(&vm_page_queue_free_mtx);
		pmap_zero_page_idle(m);
		mtx_lock_spin(&vm_page_queue_free_mtx);
		m->flags |= PG_ZERO;
		vm_pageq_enqueue(PQ_FREE + m->pc, m);
		++vm_page_zero_count;
		++cnt_prezero;
		if (vm_page_zero_count >= ZIDLE_HI(cnt.v_free_count))
			zero_state = 1;
	}
	free_rover = (free_rover + PQ_PRIME2) & PQ_L2_MASK;
	mtx_unlock_spin(&vm_page_queue_free_mtx);
	return 1;
}


/* Called by vm_page_free to hint that a new page is available */
void
vm_page_zero_idle_wakeup(void)
{

	if (idlezero_enable && vm_page_zero_check())
		wakeup(&zero_state);
}

static void
vm_pagezero(void)
{
	struct thread *td;
	struct proc *p;
	struct rtprio rtp;
	int pages = 0;
	int pri;

	td = curthread;
	p = td->td_proc;
	rtp.prio = RTP_PRIO_MAX;
	rtp.type = RTP_PRIO_IDLE;
	mtx_lock_spin(&sched_lock);
	rtp_to_pri(&rtp, td->td_ksegrp);
	pri = td->td_priority;
	mtx_unlock_spin(&sched_lock);
	PROC_LOCK(p);
	p->p_flag |= P_NOLOAD;
	PROC_UNLOCK(p);

	for (;;) {
		if (vm_page_zero_check()) {
			pages += vm_page_zero_idle();
			if (pages > idlezero_maxrun || sched_runnable()) {
				mtx_lock_spin(&sched_lock);
				td->td_proc->p_stats->p_ru.ru_nvcsw++;
				mi_switch();
				mtx_unlock_spin(&sched_lock);
				pages = 0;
			}
		} else {
			tsleep(&zero_state, pri, "pgzero", hz * 300);
			pages = 0;
		}
	}
}

static struct proc *pagezero;
static struct kproc_desc pagezero_kp = {
	 "pagezero",
	 vm_pagezero,
	 &pagezero
};
SYSINIT(pagezero, SI_SUB_KTHREAD_VM, SI_ORDER_ANY, kproc_start, &pagezero_kp)
