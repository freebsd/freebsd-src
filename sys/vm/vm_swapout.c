/*-
 * SPDX-License-Identifier: (BSD-4-Clause AND MIT-CMU)
 *
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 * Copyright (c) 2005 Yahoo! Technologies Norway AS
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
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
 *	from: @(#)vm_pageout.c	7.4 (Berkeley) 5/7/91
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kstack_pages.h"
#include "opt_kstack_max_pages.h"
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/limits.h>
#include <sys/kernel.h>
#include <sys/eventhandler.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/mount.h>
#include <sys/racct.h>
#include <sys/resourcevar.h>
#include <sys/refcount.h>
#include <sys/sched.h>
#include <sys/sdt.h>
#include <sys/signalvar.h>
#include <sys/smp.h>
#include <sys/time.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/rwlock.h>
#include <sys/sx.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>
#include <vm/swap_pager.h>
#include <vm/vm_extern.h>
#include <vm/uma.h>

/* the kernel process "vm_daemon" */
static void vm_daemon(void);
static struct proc *vmproc;

static struct kproc_desc vm_kp = {
	"vmdaemon",
	vm_daemon,
	&vmproc
};
SYSINIT(vmdaemon, SI_SUB_KTHREAD_VM, SI_ORDER_FIRST, kproc_start, &vm_kp);

static int vm_swap_enabled = 1;
static int vm_swap_idle_enabled = 0;

SYSCTL_INT(_vm, VM_SWAPPING_ENABLED, swap_enabled, CTLFLAG_RW,
    &vm_swap_enabled, 0,
    "Enable entire process swapout");
SYSCTL_INT(_vm, OID_AUTO, swap_idle_enabled, CTLFLAG_RW,
    &vm_swap_idle_enabled, 0,
    "Allow swapout on idle criteria");

/*
 * Swap_idle_threshold1 is the guaranteed swapped in time for a process
 */
static int swap_idle_threshold1 = 2;
SYSCTL_INT(_vm, OID_AUTO, swap_idle_threshold1, CTLFLAG_RW,
    &swap_idle_threshold1, 0,
    "Guaranteed swapped in time for a process");

/*
 * Swap_idle_threshold2 is the time that a process can be idle before
 * it will be swapped out, if idle swapping is enabled.
 */
static int swap_idle_threshold2 = 10;
SYSCTL_INT(_vm, OID_AUTO, swap_idle_threshold2, CTLFLAG_RW,
    &swap_idle_threshold2, 0,
    "Time before a process will be swapped out");

static int vm_daemon_timeout = 0;
SYSCTL_INT(_vm, OID_AUTO, vmdaemon_timeout, CTLFLAG_RW,
    &vm_daemon_timeout, 0,
    "Time between vmdaemon runs");

static int vm_pageout_req_swapout;	/* XXX */
static int vm_daemon_needed;
static struct mtx vm_daemon_mtx;
/* Allow for use by vm_pageout before vm_daemon is initialized. */
MTX_SYSINIT(vm_daemon, &vm_daemon_mtx, "vm daemon", MTX_DEF);

static int swapped_cnt;
static int swap_inprogress;	/* Pending swap-ins done outside swapper. */
static int last_swapin;

static void swapclear(struct proc *);
static int swapout(struct proc *);
static void vm_swapout_map_deactivate_pages(vm_map_t, long);
static void vm_swapout_object_deactivate(pmap_t, vm_object_t, long);
static void swapout_procs(int action);
static void vm_req_vmdaemon(int req);
static void vm_thread_swapout(struct thread *td);

static void
vm_swapout_object_deactivate_page(pmap_t pmap, vm_page_t m, bool unmap)
{

	/*
	 * Ignore unreclaimable wired pages.  Repeat the check after busying
	 * since a busy holder may wire the page.
	 */
	if (vm_page_wired(m) || !vm_page_tryxbusy(m))
		return;

	if (vm_page_wired(m) || !pmap_page_exists_quick(pmap, m)) {
		vm_page_xunbusy(m);
		return;
	}
	if (!pmap_is_referenced(m)) {
		if (!vm_page_active(m))
			(void)vm_page_try_remove_all(m);
		else if (unmap && vm_page_try_remove_all(m))
			vm_page_deactivate(m);
	}
	vm_page_xunbusy(m);
}

/*
 *	vm_swapout_object_deactivate
 *
 *	Deactivate enough pages to satisfy the inactive target
 *	requirements.
 *
 *	The object and map must be locked.
 */
static void
vm_swapout_object_deactivate(pmap_t pmap, vm_object_t first_object,
    long desired)
{
	vm_object_t backing_object, object;
	vm_page_t m;
	bool unmap;

	VM_OBJECT_ASSERT_LOCKED(first_object);
	if ((first_object->flags & OBJ_FICTITIOUS) != 0)
		return;
	for (object = first_object;; object = backing_object) {
		if (pmap_resident_count(pmap) <= desired)
			goto unlock_return;
		VM_OBJECT_ASSERT_LOCKED(object);
		if ((object->flags & OBJ_UNMANAGED) != 0 ||
		    blockcount_read(&object->paging_in_progress) > 0)
			goto unlock_return;

		unmap = true;
		if (object->shadow_count > 1)
			unmap = false;

		/*
		 * Scan the object's entire memory queue.
		 */
		TAILQ_FOREACH(m, &object->memq, listq) {
			if (pmap_resident_count(pmap) <= desired)
				goto unlock_return;
			if (should_yield())
				goto unlock_return;
			vm_swapout_object_deactivate_page(pmap, m, unmap);
		}
		if ((backing_object = object->backing_object) == NULL)
			goto unlock_return;
		VM_OBJECT_RLOCK(backing_object);
		if (object != first_object)
			VM_OBJECT_RUNLOCK(object);
	}
unlock_return:
	if (object != first_object)
		VM_OBJECT_RUNLOCK(object);
}

/*
 * deactivate some number of pages in a map, try to do it fairly, but
 * that is really hard to do.
 */
static void
vm_swapout_map_deactivate_pages(vm_map_t map, long desired)
{
	vm_map_entry_t tmpe;
	vm_object_t obj, bigobj;
	int nothingwired;

	if (!vm_map_trylock_read(map))
		return;

	bigobj = NULL;
	nothingwired = TRUE;

	/*
	 * first, search out the biggest object, and try to free pages from
	 * that.
	 */
	VM_MAP_ENTRY_FOREACH(tmpe, map) {
		if ((tmpe->eflags & MAP_ENTRY_IS_SUB_MAP) == 0) {
			obj = tmpe->object.vm_object;
			if (obj != NULL && VM_OBJECT_TRYRLOCK(obj)) {
				if (obj->shadow_count <= 1 &&
				    (bigobj == NULL ||
				     bigobj->resident_page_count <
				     obj->resident_page_count)) {
					if (bigobj != NULL)
						VM_OBJECT_RUNLOCK(bigobj);
					bigobj = obj;
				} else
					VM_OBJECT_RUNLOCK(obj);
			}
		}
		if (tmpe->wired_count > 0)
			nothingwired = FALSE;
	}

	if (bigobj != NULL) {
		vm_swapout_object_deactivate(map->pmap, bigobj, desired);
		VM_OBJECT_RUNLOCK(bigobj);
	}
	/*
	 * Next, hunt around for other pages to deactivate.  We actually
	 * do this search sort of wrong -- .text first is not the best idea.
	 */
	VM_MAP_ENTRY_FOREACH(tmpe, map) {
		if (pmap_resident_count(vm_map_pmap(map)) <= desired)
			break;
		if ((tmpe->eflags & MAP_ENTRY_IS_SUB_MAP) == 0) {
			obj = tmpe->object.vm_object;
			if (obj != NULL) {
				VM_OBJECT_RLOCK(obj);
				vm_swapout_object_deactivate(map->pmap, obj,
				    desired);
				VM_OBJECT_RUNLOCK(obj);
			}
		}
	}

	/*
	 * Remove all mappings if a process is swapped out, this will free page
	 * table pages.
	 */
	if (desired == 0 && nothingwired) {
		pmap_remove(vm_map_pmap(map), vm_map_min(map),
		    vm_map_max(map));
	}

	vm_map_unlock_read(map);
}

/*
 * Swap out requests
 */
#define VM_SWAP_NORMAL 1
#define VM_SWAP_IDLE 2

void
vm_swapout_run(void)
{

	if (vm_swap_enabled)
		vm_req_vmdaemon(VM_SWAP_NORMAL);
}

/*
 * Idle process swapout -- run once per second when pagedaemons are
 * reclaiming pages.
 */
void
vm_swapout_run_idle(void)
{
	static long lsec;

	if (!vm_swap_idle_enabled || time_second == lsec)
		return;
	vm_req_vmdaemon(VM_SWAP_IDLE);
	lsec = time_second;
}

static void
vm_req_vmdaemon(int req)
{
	static int lastrun = 0;

	mtx_lock(&vm_daemon_mtx);
	vm_pageout_req_swapout |= req;
	if ((ticks > (lastrun + hz)) || (ticks < lastrun)) {
		wakeup(&vm_daemon_needed);
		lastrun = ticks;
	}
	mtx_unlock(&vm_daemon_mtx);
}

static void
vm_daemon(void)
{
	struct rlimit rsslim;
	struct proc *p;
	struct thread *td;
	struct vmspace *vm;
	int breakout, swapout_flags, tryagain, attempts;
#ifdef RACCT
	uint64_t rsize, ravailable;

	if (racct_enable && vm_daemon_timeout == 0)
		vm_daemon_timeout = hz;
#endif

	while (TRUE) {
		mtx_lock(&vm_daemon_mtx);
		msleep(&vm_daemon_needed, &vm_daemon_mtx, PPAUSE, "psleep",
		    vm_daemon_timeout);
		swapout_flags = vm_pageout_req_swapout;
		vm_pageout_req_swapout = 0;
		mtx_unlock(&vm_daemon_mtx);
		if (swapout_flags != 0) {
			/*
			 * Drain the per-CPU page queue batches as a deadlock
			 * avoidance measure.
			 */
			if ((swapout_flags & VM_SWAP_NORMAL) != 0)
				vm_page_pqbatch_drain();
			swapout_procs(swapout_flags);
		}

		/*
		 * scan the processes for exceeding their rlimits or if
		 * process is swapped out -- deactivate pages
		 */
		tryagain = 0;
		attempts = 0;
again:
		attempts++;
		sx_slock(&allproc_lock);
		FOREACH_PROC_IN_SYSTEM(p) {
			vm_pindex_t limit, size;

			/*
			 * if this is a system process or if we have already
			 * looked at this process, skip it.
			 */
			PROC_LOCK(p);
			if (p->p_state != PRS_NORMAL ||
			    p->p_flag & (P_INEXEC | P_SYSTEM | P_WEXIT)) {
				PROC_UNLOCK(p);
				continue;
			}
			/*
			 * if the process is in a non-running type state,
			 * don't touch it.
			 */
			breakout = 0;
			FOREACH_THREAD_IN_PROC(p, td) {
				thread_lock(td);
				if (!TD_ON_RUNQ(td) &&
				    !TD_IS_RUNNING(td) &&
				    !TD_IS_SLEEPING(td) &&
				    !TD_IS_SUSPENDED(td)) {
					thread_unlock(td);
					breakout = 1;
					break;
				}
				thread_unlock(td);
			}
			if (breakout) {
				PROC_UNLOCK(p);
				continue;
			}
			/*
			 * get a limit
			 */
			lim_rlimit_proc(p, RLIMIT_RSS, &rsslim);
			limit = OFF_TO_IDX(
			    qmin(rsslim.rlim_cur, rsslim.rlim_max));

			/*
			 * let processes that are swapped out really be
			 * swapped out set the limit to nothing (will force a
			 * swap-out.)
			 */
			if ((p->p_flag & P_INMEM) == 0)
				limit = 0;	/* XXX */
			vm = vmspace_acquire_ref(p);
			_PHOLD_LITE(p);
			PROC_UNLOCK(p);
			if (vm == NULL) {
				PRELE(p);
				continue;
			}
			sx_sunlock(&allproc_lock);

			size = vmspace_resident_count(vm);
			if (size >= limit) {
				vm_swapout_map_deactivate_pages(
				    &vm->vm_map, limit);
				size = vmspace_resident_count(vm);
			}
#ifdef RACCT
			if (racct_enable) {
				rsize = IDX_TO_OFF(size);
				PROC_LOCK(p);
				if (p->p_state == PRS_NORMAL)
					racct_set(p, RACCT_RSS, rsize);
				ravailable = racct_get_available(p, RACCT_RSS);
				PROC_UNLOCK(p);
				if (rsize > ravailable) {
					/*
					 * Don't be overly aggressive; this
					 * might be an innocent process,
					 * and the limit could've been exceeded
					 * by some memory hog.  Don't try
					 * to deactivate more than 1/4th
					 * of process' resident set size.
					 */
					if (attempts <= 8) {
						if (ravailable < rsize -
						    (rsize / 4)) {
							ravailable = rsize -
							    (rsize / 4);
						}
					}
					vm_swapout_map_deactivate_pages(
					    &vm->vm_map,
					    OFF_TO_IDX(ravailable));
					/* Update RSS usage after paging out. */
					size = vmspace_resident_count(vm);
					rsize = IDX_TO_OFF(size);
					PROC_LOCK(p);
					if (p->p_state == PRS_NORMAL)
						racct_set(p, RACCT_RSS, rsize);
					PROC_UNLOCK(p);
					if (rsize > ravailable)
						tryagain = 1;
				}
			}
#endif
			vmspace_free(vm);
			sx_slock(&allproc_lock);
			PRELE(p);
		}
		sx_sunlock(&allproc_lock);
		if (tryagain != 0 && attempts <= 10) {
			maybe_yield();
			goto again;
		}
	}
}

/*
 * Allow a thread's kernel stack to be paged out.
 */
static void
vm_thread_swapout(struct thread *td)
{
	vm_page_t m;
	vm_offset_t kaddr;
	vm_pindex_t pindex;
	int i, pages;

	cpu_thread_swapout(td);
	kaddr = td->td_kstack;
	pages = td->td_kstack_pages;
	pindex = atop(kaddr - VM_MIN_KERNEL_ADDRESS);
	pmap_qremove(kaddr, pages);
	VM_OBJECT_WLOCK(kstack_object);
	for (i = 0; i < pages; i++) {
		m = vm_page_lookup(kstack_object, pindex + i);
		if (m == NULL)
			panic("vm_thread_swapout: kstack already missing?");
		vm_page_dirty(m);
		vm_page_xunbusy_unchecked(m);
		vm_page_unwire(m, PQ_LAUNDRY);
	}
	VM_OBJECT_WUNLOCK(kstack_object);
}

/*
 * Bring the kernel stack for a specified thread back in.
 */
static void
vm_thread_swapin(struct thread *td, int oom_alloc)
{
	vm_page_t ma[KSTACK_MAX_PAGES];
	vm_offset_t kaddr;
	int a, count, i, j, pages, rv;

	kaddr = td->td_kstack;
	pages = td->td_kstack_pages;
	vm_thread_stack_back(td->td_domain.dr_policy, kaddr, ma, pages,
	    oom_alloc);
	for (i = 0; i < pages;) {
		vm_page_assert_xbusied(ma[i]);
		if (vm_page_all_valid(ma[i])) {
			i++;
			continue;
		}
		vm_object_pip_add(kstack_object, 1);
		for (j = i + 1; j < pages; j++)
			if (vm_page_all_valid(ma[j]))
				break;
		VM_OBJECT_WLOCK(kstack_object);
		rv = vm_pager_has_page(kstack_object, ma[i]->pindex, NULL, &a);
		VM_OBJECT_WUNLOCK(kstack_object);
		KASSERT(rv == 1, ("%s: missing page %p", __func__, ma[i]));
		count = min(a + 1, j - i);
		rv = vm_pager_get_pages(kstack_object, ma + i, count, NULL, NULL);
		KASSERT(rv == VM_PAGER_OK, ("%s: cannot get kstack for proc %d",
		    __func__, td->td_proc->p_pid));
		vm_object_pip_wakeup(kstack_object);
		i += count;
	}
	pmap_qenter(kaddr, ma, pages);
	cpu_thread_swapin(td);
}

void
faultin(struct proc *p)
{
	struct thread *td;
	int oom_alloc;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	/*
	 * If another process is swapping in this process,
	 * just wait until it finishes.
	 */
	if (p->p_flag & P_SWAPPINGIN) {
		while (p->p_flag & P_SWAPPINGIN)
			msleep(&p->p_flag, &p->p_mtx, PVM, "faultin", 0);
		return;
	}

	if ((p->p_flag & P_INMEM) == 0) {
		oom_alloc = (p->p_flag & P_WKILLED) != 0 ? VM_ALLOC_SYSTEM :
		    VM_ALLOC_NORMAL;

		/*
		 * Don't let another thread swap process p out while we are
		 * busy swapping it in.
		 */
		++p->p_lock;
		p->p_flag |= P_SWAPPINGIN;
		PROC_UNLOCK(p);
		sx_xlock(&allproc_lock);
		MPASS(swapped_cnt > 0);
		swapped_cnt--;
		if (curthread != &thread0)
			swap_inprogress++;
		sx_xunlock(&allproc_lock);

		/*
		 * We hold no lock here because the list of threads
		 * can not change while all threads in the process are
		 * swapped out.
		 */
		FOREACH_THREAD_IN_PROC(p, td)
			vm_thread_swapin(td, oom_alloc);

		if (curthread != &thread0) {
			sx_xlock(&allproc_lock);
			MPASS(swap_inprogress > 0);
			swap_inprogress--;
			last_swapin = ticks;
			sx_xunlock(&allproc_lock);
		}
		PROC_LOCK(p);
		swapclear(p);
		p->p_swtick = ticks;

		/* Allow other threads to swap p out now. */
		wakeup(&p->p_flag);
		--p->p_lock;
	}
}

/*
 * This swapin algorithm attempts to swap-in processes only if there
 * is enough space for them.  Of course, if a process waits for a long
 * time, it will be swapped in anyway.
 */

static struct proc *
swapper_selector(bool wkilled_only)
{
	struct proc *p, *res;
	struct thread *td;
	int ppri, pri, slptime, swtime;

	sx_assert(&allproc_lock, SA_SLOCKED);
	if (swapped_cnt == 0)
		return (NULL);
	res = NULL;
	ppri = INT_MIN;
	FOREACH_PROC_IN_SYSTEM(p) {
		PROC_LOCK(p);
		if (p->p_state == PRS_NEW || (p->p_flag & (P_SWAPPINGOUT |
		    P_SWAPPINGIN | P_INMEM)) != 0) {
			PROC_UNLOCK(p);
			continue;
		}
		if (p->p_state == PRS_NORMAL && (p->p_flag & P_WKILLED) != 0) {
			/*
			 * A swapped-out process might have mapped a
			 * large portion of the system's pages as
			 * anonymous memory.  There is no other way to
			 * release the memory other than to kill the
			 * process, for which we need to swap it in.
			 */
			return (p);
		}
		if (wkilled_only) {
			PROC_UNLOCK(p);
			continue;
		}
		swtime = (ticks - p->p_swtick) / hz;
		FOREACH_THREAD_IN_PROC(p, td) {
			/*
			 * An otherwise runnable thread of a process
			 * swapped out has only the TDI_SWAPPED bit set.
			 */
			thread_lock(td);
			if (td->td_inhibitors == TDI_SWAPPED) {
				slptime = (ticks - td->td_slptick) / hz;
				pri = swtime + slptime;
				if ((td->td_flags & TDF_SWAPINREQ) == 0)
					pri -= p->p_nice * 8;
				/*
				 * if this thread is higher priority
				 * and there is enough space, then select
				 * this process instead of the previous
				 * selection.
				 */
				if (pri > ppri) {
					res = p;
					ppri = pri;
				}
			}
			thread_unlock(td);
		}
		PROC_UNLOCK(p);
	}

	if (res != NULL)
		PROC_LOCK(res);
	return (res);
}

#define	SWAPIN_INTERVAL	(MAXSLP * hz / 2)

/*
 * Limit swapper to swap in one non-WKILLED process in MAXSLP/2
 * interval, assuming that there is:
 * - at least one domain that is not suffering from a shortage of free memory;
 * - no parallel swap-ins;
 * - no other swap-ins in the current SWAPIN_INTERVAL.
 */
static bool
swapper_wkilled_only(void)
{

	return (vm_page_count_min_set(&all_domains) || swap_inprogress > 0 ||
	    (u_int)(ticks - last_swapin) < SWAPIN_INTERVAL);
}

void
swapper(void)
{
	struct proc *p;

	for (;;) {
		sx_slock(&allproc_lock);
		p = swapper_selector(swapper_wkilled_only());
		sx_sunlock(&allproc_lock);

		if (p == NULL) {
			tsleep(&proc0, PVM, "swapin", SWAPIN_INTERVAL);
		} else {
			PROC_LOCK_ASSERT(p, MA_OWNED);

			/*
			 * Another process may be bringing or may have
			 * already brought this process in while we
			 * traverse all threads.  Or, this process may
			 * have exited or even being swapped out
			 * again.
			 */
			if (p->p_state == PRS_NORMAL && (p->p_flag & (P_INMEM |
			    P_SWAPPINGOUT | P_SWAPPINGIN)) == 0) {
				faultin(p);
			}
			PROC_UNLOCK(p);
		}
	}
}

/*
 * First, if any processes have been sleeping or stopped for at least
 * "swap_idle_threshold1" seconds, they are swapped out.  If, however,
 * no such processes exist, then the longest-sleeping or stopped
 * process is swapped out.  Finally, and only as a last resort, if
 * there are no sleeping or stopped processes, the longest-resident
 * process is swapped out.
 */
static void
swapout_procs(int action)
{
	struct proc *p;
	struct thread *td;
	int slptime;
	bool didswap, doswap;

	MPASS((action & (VM_SWAP_NORMAL | VM_SWAP_IDLE)) != 0);

	didswap = false;
	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		/*
		 * Filter out not yet fully constructed processes.  Do
		 * not swap out held processes.  Avoid processes which
		 * are system, exiting, execing, traced, already swapped
		 * out or are in the process of being swapped in or out.
		 */
		PROC_LOCK(p);
		if (p->p_state != PRS_NORMAL || p->p_lock != 0 || (p->p_flag &
		    (P_SYSTEM | P_WEXIT | P_INEXEC | P_STOPPED_SINGLE |
		    P_TRACED | P_SWAPPINGOUT | P_SWAPPINGIN | P_INMEM)) !=
		    P_INMEM) {
			PROC_UNLOCK(p);
			continue;
		}

		/*
		 * Further consideration of this process for swap out
		 * requires iterating over its threads.  We release
		 * allproc_lock here so that process creation and
		 * destruction are not blocked while we iterate.
		 *
		 * To later reacquire allproc_lock and resume
		 * iteration over the allproc list, we will first have
		 * to release the lock on the process.  We place a
		 * hold on the process so that it remains in the
		 * allproc list while it is unlocked.
		 */
		_PHOLD_LITE(p);
		sx_sunlock(&allproc_lock);

		/*
		 * Do not swapout a realtime process.
		 * Guarantee swap_idle_threshold1 time in memory.
		 * If the system is under memory stress, or if we are
		 * swapping idle processes >= swap_idle_threshold2,
		 * then swap the process out.
		 */
		doswap = true;
		FOREACH_THREAD_IN_PROC(p, td) {
			thread_lock(td);
			slptime = (ticks - td->td_slptick) / hz;
			if (PRI_IS_REALTIME(td->td_pri_class) ||
			    slptime < swap_idle_threshold1 ||
			    !thread_safetoswapout(td) ||
			    ((action & VM_SWAP_NORMAL) == 0 &&
			    slptime < swap_idle_threshold2))
				doswap = false;
			thread_unlock(td);
			if (!doswap)
				break;
		}
		if (doswap && swapout(p) == 0)
			didswap = true;

		PROC_UNLOCK(p);
		if (didswap) {
			sx_xlock(&allproc_lock);
			swapped_cnt++;
			sx_downgrade(&allproc_lock);
		} else
			sx_slock(&allproc_lock);
		PRELE(p);
	}
	sx_sunlock(&allproc_lock);

	/*
	 * If we swapped something out, and another process needed memory,
	 * then wakeup the sched process.
	 */
	if (didswap)
		wakeup(&proc0);
}

static void
swapclear(struct proc *p)
{
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	FOREACH_THREAD_IN_PROC(p, td) {
		thread_lock(td);
		td->td_flags |= TDF_INMEM;
		td->td_flags &= ~TDF_SWAPINREQ;
		TD_CLR_SWAPPED(td);
		if (TD_CAN_RUN(td)) {
			if (setrunnable(td, 0)) {
#ifdef INVARIANTS
				/*
				 * XXX: We just cleared TDI_SWAPPED
				 * above and set TDF_INMEM, so this
				 * should never happen.
				 */
				panic("not waking up swapper");
#endif
			}
		} else
			thread_unlock(td);
	}
	p->p_flag &= ~(P_SWAPPINGIN | P_SWAPPINGOUT);
	p->p_flag |= P_INMEM;
}

static int
swapout(struct proc *p)
{
	struct thread *td;

	PROC_LOCK_ASSERT(p, MA_OWNED);

	/*
	 * The states of this process and its threads may have changed
	 * by now.  Assuming that there is only one pageout daemon thread,
	 * this process should still be in memory.
	 */
	KASSERT((p->p_flag & (P_INMEM | P_SWAPPINGOUT | P_SWAPPINGIN)) ==
	    P_INMEM, ("swapout: lost a swapout race?"));

	/*
	 * Remember the resident count.
	 */
	p->p_vmspace->vm_swrss = vmspace_resident_count(p->p_vmspace);

	/*
	 * Check and mark all threads before we proceed.
	 */
	p->p_flag &= ~P_INMEM;
	p->p_flag |= P_SWAPPINGOUT;
	FOREACH_THREAD_IN_PROC(p, td) {
		thread_lock(td);
		if (!thread_safetoswapout(td)) {
			thread_unlock(td);
			swapclear(p);
			return (EBUSY);
		}
		td->td_flags &= ~TDF_INMEM;
		TD_SET_SWAPPED(td);
		thread_unlock(td);
	}
	td = FIRST_THREAD_IN_PROC(p);
	++td->td_ru.ru_nswap;
	PROC_UNLOCK(p);

	/*
	 * This list is stable because all threads are now prevented from
	 * running.  The list is only modified in the context of a running
	 * thread in this process.
	 */
	FOREACH_THREAD_IN_PROC(p, td)
		vm_thread_swapout(td);

	PROC_LOCK(p);
	p->p_flag &= ~P_SWAPPINGOUT;
	p->p_swtick = ticks;
	return (0);
}
