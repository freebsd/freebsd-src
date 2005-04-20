/*-
 * Copyright (c) 1994 John Dyson
 * Copyright (c) 2001 Matt Dillon
 *
 * All Rights Reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: @(#)vm_machdep.c	7.3 (Berkeley) 5/13/91
 *	Utah $Hdr: vm_machdep.c 1.16.1.1 89/06/23$
 * from: FreeBSD: .../i386/vm_machdep.c,v 1.165 2001/07/04 23:27:04 dillon
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <opt_sched.h>

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
#include <sys/unistd.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

SYSCTL_DECL(_vm_stats_misc);

static int cnt_prezero;
SYSCTL_INT(_vm_stats_misc, OID_AUTO, cnt_prezero, CTLFLAG_RD,
    &cnt_prezero, 0, "");

static int idlezero_enable_default = 1;
TUNABLE_INT("vm.idlezero_enable", &idlezero_enable_default);
/* Defer setting the enable flag until the kthread is running. */
static int idlezero_enable = 0;
SYSCTL_INT(_vm, OID_AUTO, idlezero_enable, CTLFLAG_RW, &idlezero_enable, 0, "");

static int idlezero_maxrun = 16;
SYSCTL_INT(_vm, OID_AUTO, idlezero_maxrun, CTLFLAG_RW, &idlezero_maxrun, 0, "");
TUNABLE_INT("vm.idlezero_maxrun", &idlezero_maxrun);

/*
 * Implement the pre-zeroed page mechanism.
 */

#define ZIDLE_LO(v)	((v) * 2 / 3)
#define ZIDLE_HI(v)	((v) * 4 / 5)

static boolean_t wakeup_needed = FALSE;
static int zero_state;

static int
vm_page_zero_check(void)
{

	if (!idlezero_enable)
		return (0);
	/*
	 * Attempt to maintain approximately 1/2 of our free pages in a
	 * PG_ZERO'd state.   Add some hysteresis to (attempt to) avoid
	 * generally zeroing a page when the system is near steady-state.
	 * Otherwise we might get 'flutter' during disk I/O / IPC or 
	 * fast sleeps.  We also do not want to be continuously zeroing
	 * pages because doing so may flush our L1 and L2 caches too much.
	 */
	if (zero_state && vm_page_zero_count >= ZIDLE_LO(cnt.v_free_count))
		return (0);
	if (vm_page_zero_count >= ZIDLE_HI(cnt.v_free_count))
		return (0);
	return (1);
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
	return (1);
}

/* Called by vm_page_free to hint that a new page is available. */
void
vm_page_zero_idle_wakeup(void)
{

	mtx_assert(&vm_page_queue_mtx, MA_OWNED);
	if (wakeup_needed && vm_page_zero_check()) {
		wakeup_needed = FALSE;
		wakeup(&zero_state);
	}
}

static void
vm_pagezero(void __unused *arg)
{
	struct thread *td;

	td = curthread;
	idlezero_enable = idlezero_enable_default;

	for (;;) {
		if (vm_page_zero_check()) {
			vm_page_zero_idle();
#ifndef PREEMPTION
			if (sched_runnable()) {
				mtx_lock_spin(&sched_lock);
				mi_switch(SW_VOL, NULL);
				mtx_unlock_spin(&sched_lock);
			}
#endif
		} else {
			vm_page_lock_queues();
			wakeup_needed = TRUE;
			msleep(&zero_state, &vm_page_queue_mtx,
			    PDROP | td->td_priority, "pgzero", hz * 300);
		}
	}
}

static struct proc *pagezero_proc;

static void
pagezero_start(void __unused *arg)
{
	int error;
	struct thread *td;

	error = kthread_create(vm_pagezero, NULL, &pagezero_proc, RFSTOPPED, 0,
	    "pagezero");
	if (error)
		panic("pagezero_start: error %d\n", error);
	/*
	 * We're an idle task, don't count us in the load.
	 */
	PROC_LOCK(pagezero_proc);
	pagezero_proc->p_flag |= P_NOLOAD;
	PROC_UNLOCK(pagezero_proc);
	mtx_lock_spin(&sched_lock);
	td = FIRST_THREAD_IN_PROC(pagezero_proc);
	setrunqueue(td, SRQ_BORING);
	td->td_ksegrp->kg_user_pri = PRI_MAX_IDLE;
	sched_class(td->td_ksegrp, PRI_IDLE);
	sched_prio(td, PRI_MAX_IDLE);
	mtx_unlock_spin(&sched_lock);
}
SYSINIT(pagezero, SI_SUB_KTHREAD_VM, SI_ORDER_ANY, pagezero_start, NULL)
