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
#include <vm/vm_radix.h>
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

static int vm_daemon_timeout = 0;
SYSCTL_INT(_vm, OID_AUTO, vmdaemon_timeout, CTLFLAG_RW,
    &vm_daemon_timeout, 0,
    "Time between vmdaemon runs");

static int vm_daemon_needed;
static struct mtx vm_daemon_mtx;
/* Allow for use by vm_pageout before vm_daemon is initialized. */
MTX_SYSINIT(vm_daemon, &vm_daemon_mtx, "vm daemon", MTX_DEF);

static void vm_swapout_map_deactivate_pages(vm_map_t, long);
static void vm_swapout_object_deactivate(pmap_t, vm_object_t, long);

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
	struct pctrie_iter pages;
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
		vm_page_iter_init(&pages, object);
		VM_RADIX_FOREACH(m, &pages) {
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

static void
vm_daemon(void)
{
	struct rlimit rsslim;
	struct proc *p;
	struct thread *td;
	struct vmspace *vm;
	int breakout, tryagain, attempts;
	uint64_t rsize, ravailable;

	if (racct_enable && vm_daemon_timeout == 0)
		vm_daemon_timeout = hz;

	while (TRUE) {
		mtx_lock(&vm_daemon_mtx);
		msleep(&vm_daemon_needed, &vm_daemon_mtx, PPAUSE, "psleep",
		    vm_daemon_timeout);
		mtx_unlock(&vm_daemon_mtx);

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

			vm = vmspace_acquire_ref(p);
			_PHOLD(p);
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
