/*
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
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
 *
 * $FreeBSD$
 */

/*
 *	The proverbial page-out daemon.
 */

#include "opt_vm.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <sys/lock.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>
#include <vm/vm_extern.h>

#include <machine/mutex.h>

/*
 * System initialization
 */

/* the kernel process "vm_pageout"*/
static void vm_pageout __P((void));
static int vm_pageout_clean __P((vm_page_t));
static int vm_pageout_scan __P((void));
static int vm_pageout_free_page_calc __P((vm_size_t count));
struct proc *pageproc;

static struct kproc_desc page_kp = {
	"pagedaemon",
	vm_pageout,
	&pageproc
};
SYSINIT(pagedaemon, SI_SUB_KTHREAD_PAGE, SI_ORDER_FIRST, kproc_start, &page_kp)

#if !defined(NO_SWAPPING)
/* the kernel process "vm_daemon"*/
static void vm_daemon __P((void));
static struct	proc *vmproc;

static struct kproc_desc vm_kp = {
	"vmdaemon",
	vm_daemon,
	&vmproc
};
SYSINIT(vmdaemon, SI_SUB_KTHREAD_VM, SI_ORDER_FIRST, kproc_start, &vm_kp)
#endif


int vm_pages_needed=0;		/* Event on which pageout daemon sleeps */
int vm_pageout_deficit=0;	/* Estimated number of pages deficit */
int vm_pageout_pages_needed=0;	/* flag saying that the pageout daemon needs pages */

#if !defined(NO_SWAPPING)
static int vm_pageout_req_swapout;	/* XXX */
static int vm_daemon_needed;
#endif
extern int vm_swap_size;
static int vm_pageout_stats_max=0, vm_pageout_stats_interval = 0;
static int vm_pageout_full_stats_interval = 0;
static int vm_pageout_stats_free_max=0, vm_pageout_algorithm_lru=0;
static int defer_swap_pageouts=0;
static int disable_swap_pageouts=0;

static int max_page_launder=100;
static int vm_pageout_actcmp=0;
#if defined(NO_SWAPPING)
static int vm_swap_enabled=0;
static int vm_swap_idle_enabled=0;
#else
static int vm_swap_enabled=1;
static int vm_swap_idle_enabled=0;
#endif

SYSCTL_INT(_vm, VM_PAGEOUT_ALGORITHM, pageout_algorithm,
	CTLFLAG_RW, &vm_pageout_algorithm_lru, 0, "LRU page mgmt");

SYSCTL_INT(_vm, OID_AUTO, pageout_stats_max,
	CTLFLAG_RW, &vm_pageout_stats_max, 0, "Max pageout stats scan length");

SYSCTL_INT(_vm, OID_AUTO, pageout_full_stats_interval,
	CTLFLAG_RW, &vm_pageout_full_stats_interval, 0, "Interval for full stats scan");

SYSCTL_INT(_vm, OID_AUTO, pageout_stats_interval,
	CTLFLAG_RW, &vm_pageout_stats_interval, 0, "Interval for partial stats scan");

SYSCTL_INT(_vm, OID_AUTO, pageout_stats_free_max,
	CTLFLAG_RW, &vm_pageout_stats_free_max, 0, "Not implemented");

#if defined(NO_SWAPPING)
SYSCTL_INT(_vm, VM_SWAPPING_ENABLED, swap_enabled,
	CTLFLAG_RD, &vm_swap_enabled, 0, "");
SYSCTL_INT(_vm, OID_AUTO, swap_idle_enabled,
	CTLFLAG_RD, &vm_swap_idle_enabled, 0, "");
#else
SYSCTL_INT(_vm, VM_SWAPPING_ENABLED, swap_enabled,
	CTLFLAG_RW, &vm_swap_enabled, 0, "Enable entire process swapout");
SYSCTL_INT(_vm, OID_AUTO, swap_idle_enabled,
	CTLFLAG_RW, &vm_swap_idle_enabled, 0, "Allow swapout on idle criteria");
#endif

SYSCTL_INT(_vm, OID_AUTO, defer_swapspace_pageouts,
	CTLFLAG_RW, &defer_swap_pageouts, 0, "Give preference to dirty pages in mem");

SYSCTL_INT(_vm, OID_AUTO, disable_swapspace_pageouts,
	CTLFLAG_RW, &disable_swap_pageouts, 0, "Disallow swapout of dirty pages");

SYSCTL_INT(_vm, OID_AUTO, max_page_launder,
	CTLFLAG_RW, &max_page_launder, 0, "Maximum number of pages to clean per pass");
SYSCTL_INT(_vm, OID_AUTO, vm_pageout_actcmp,
	CTLFLAG_RD, &vm_pageout_actcmp, 0, "pagedaemon agressiveness");


#define VM_PAGEOUT_PAGE_COUNT 16
int vm_pageout_page_count = VM_PAGEOUT_PAGE_COUNT;

int vm_page_max_wired;		/* XXX max # of wired pages system-wide */

#if !defined(NO_SWAPPING)
typedef void freeer_fcn_t __P((vm_map_t, vm_object_t, vm_pindex_t, int));
static void vm_pageout_map_deactivate_pages __P((vm_map_t, vm_pindex_t));
static freeer_fcn_t vm_pageout_object_deactivate_pages;
static void vm_req_vmdaemon __P((void));
#endif
static void vm_pageout_page_stats(void);

/*
 * vm_pageout_clean:
 *
 * Clean the page and remove it from the laundry.
 * 
 * We set the busy bit to cause potential page faults on this page to
 * block.  Note the careful timing, however, the busy bit isn't set till
 * late and we cannot do anything that will mess with the page.
 */

static int
vm_pageout_clean(m)
	vm_page_t m;
{
	register vm_object_t object;
	vm_page_t mc[2*vm_pageout_page_count];
	int pageout_count;
	int ib, is, page_base;
	vm_pindex_t pindex = m->pindex;

	object = m->object;

	/*
	 * It doesn't cost us anything to pageout OBJT_DEFAULT or OBJT_SWAP
	 * with the new swapper, but we could have serious problems paging
	 * out other object types if there is insufficient memory.  
	 *
	 * Unfortunately, checking free memory here is far too late, so the
	 * check has been moved up a procedural level.
	 */

	/*
	 * Don't mess with the page if it's busy, held, or special
	 */
	if ((m->hold_count != 0) ||
	    ((m->busy != 0) || (m->flags & (PG_BUSY|PG_UNMANAGED)))) {
		return 0;
	}

	mc[vm_pageout_page_count] = m;
	pageout_count = 1;
	page_base = vm_pageout_page_count;
	ib = 1;
	is = 1;

	/*
	 * Scan object for clusterable pages.
	 *
	 * We can cluster ONLY if: ->> the page is NOT
	 * clean, wired, busy, held, or mapped into a
	 * buffer, and one of the following:
	 * 1) The page is inactive, or a seldom used
	 *    active page.
	 * -or-
	 * 2) we force the issue.
	 *
	 * During heavy mmap/modification loads the pageout
	 * daemon can really fragment the underlying file
	 * due to flushing pages out of order and not trying
	 * align the clusters (which leave sporatic out-of-order
	 * holes).  To solve this problem we do the reverse scan
	 * first and attempt to align our cluster, then do a 
	 * forward scan if room remains.
	 */

more:
	while (ib && pageout_count < vm_pageout_page_count) {
		vm_page_t p;

		if (ib > pindex) {
			ib = 0;
			break;
		}

		if ((p = vm_page_lookup(object, pindex - ib)) == NULL) {
			ib = 0;
			break;
		}
		if (((p->queue - p->pc) == PQ_CACHE) ||
		    (p->flags & (PG_BUSY|PG_UNMANAGED)) || p->busy) {
			ib = 0;
			break;
		}
		vm_page_test_dirty(p);
		if ((p->dirty & p->valid) == 0 ||
		    p->queue != PQ_INACTIVE ||
		    p->wire_count != 0 ||
		    p->hold_count != 0) {
			ib = 0;
			break;
		}
		mc[--page_base] = p;
		++pageout_count;
		++ib;
		/*
		 * alignment boundry, stop here and switch directions.  Do
		 * not clear ib.
		 */
		if ((pindex - (ib - 1)) % vm_pageout_page_count == 0)
			break;
	}

	while (pageout_count < vm_pageout_page_count && 
	    pindex + is < object->size) {
		vm_page_t p;

		if ((p = vm_page_lookup(object, pindex + is)) == NULL)
			break;
		if (((p->queue - p->pc) == PQ_CACHE) ||
		    (p->flags & (PG_BUSY|PG_UNMANAGED)) || p->busy) {
			break;
		}
		vm_page_test_dirty(p);
		if ((p->dirty & p->valid) == 0 ||
		    p->queue != PQ_INACTIVE ||
		    p->wire_count != 0 ||
		    p->hold_count != 0) {
			break;
		}
		mc[page_base + pageout_count] = p;
		++pageout_count;
		++is;
	}

	/*
	 * If we exhausted our forward scan, continue with the reverse scan
	 * when possible, even past a page boundry.  This catches boundry
	 * conditions.
	 */
	if (ib && pageout_count < vm_pageout_page_count)
		goto more;

	/*
	 * we allow reads during pageouts...
	 */
	return vm_pageout_flush(&mc[page_base], pageout_count, 0);
}

/*
 * vm_pageout_flush() - launder the given pages
 *
 *	The given pages are laundered.  Note that we setup for the start of
 *	I/O ( i.e. busy the page ), mark it read-only, and bump the object
 *	reference count all in here rather then in the parent.  If we want
 *	the parent to do more sophisticated things we may have to change
 *	the ordering.
 */

int
vm_pageout_flush(mc, count, flags)
	vm_page_t *mc;
	int count;
	int flags;
{
	register vm_object_t object;
	int pageout_status[count];
	int numpagedout = 0;
	int i;

	/*
	 * Initiate I/O.  Bump the vm_page_t->busy counter and
	 * mark the pages read-only.
	 *
	 * We do not have to fixup the clean/dirty bits here... we can
	 * allow the pager to do it after the I/O completes.
	 */

	for (i = 0; i < count; i++) {
		KASSERT(mc[i]->valid == VM_PAGE_BITS_ALL && mc[i]->dirty == VM_PAGE_BITS_ALL, ("vm_pageout_flush page %p index %d/%d: partially dirty page", mc[i], i, count));
		vm_page_io_start(mc[i]);
		vm_page_protect(mc[i], VM_PROT_READ);
	}

	object = mc[0]->object;
	vm_object_pip_add(object, count);

	vm_pager_put_pages(object, mc, count,
	    (flags | ((object == kernel_object) ? OBJPC_SYNC : 0)),
	    pageout_status);

	for (i = 0; i < count; i++) {
		vm_page_t mt = mc[i];

		switch (pageout_status[i]) {
		case VM_PAGER_OK:
			numpagedout++;
			break;
		case VM_PAGER_PEND:
			numpagedout++;
			break;
		case VM_PAGER_BAD:
			/*
			 * Page outside of range of object. Right now we
			 * essentially lose the changes by pretending it
			 * worked.
			 */
			pmap_clear_modify(mt);
			vm_page_undirty(mt);
			break;
		case VM_PAGER_ERROR:
		case VM_PAGER_FAIL:
			/*
			 * If page couldn't be paged out, then reactivate the
			 * page so it doesn't clog the inactive list.  (We
			 * will try paging out it again later).
			 */
			vm_page_activate(mt);
			break;
		case VM_PAGER_AGAIN:
			break;
		}

		/*
		 * If the operation is still going, leave the page busy to
		 * block all other accesses. Also, leave the paging in
		 * progress indicator set so that we don't attempt an object
		 * collapse.
		 */
		if (pageout_status[i] != VM_PAGER_PEND) {
			vm_object_pip_wakeup(object);
			vm_page_io_finish(mt);
			if (!vm_page_count_severe() || !vm_page_try_to_cache(mt))
				vm_page_protect(mt, VM_PROT_READ);
		}
	}
	return numpagedout;
}

#if !defined(NO_SWAPPING)
/*
 *	vm_pageout_object_deactivate_pages
 *
 *	deactivate enough pages to satisfy the inactive target
 *	requirements or if vm_page_proc_limit is set, then
 *	deactivate all of the pages in the object and its
 *	backing_objects.
 *
 *	The object and map must be locked.
 */
static void
vm_pageout_object_deactivate_pages(map, object, desired, map_remove_only)
	vm_map_t map;
	vm_object_t object;
	vm_pindex_t desired;
	int map_remove_only;
{
	register vm_page_t p, next;
	int rcount;
	int remove_mode;
	int s;

	if (object->type == OBJT_DEVICE || object->type == OBJT_PHYS)
		return;

	while (object) {
		if (pmap_resident_count(vm_map_pmap(map)) <= desired)
			return;
		if (object->paging_in_progress)
			return;

		remove_mode = map_remove_only;
		if (object->shadow_count > 1)
			remove_mode = 1;
	/*
	 * scan the objects entire memory queue
	 */
		rcount = object->resident_page_count;
		p = TAILQ_FIRST(&object->memq);
		while (p && (rcount-- > 0)) {
			int actcount;
			if (pmap_resident_count(vm_map_pmap(map)) <= desired)
				return;
			next = TAILQ_NEXT(p, listq);
			cnt.v_pdpages++;
			if (p->wire_count != 0 ||
			    p->hold_count != 0 ||
			    p->busy != 0 ||
			    (p->flags & (PG_BUSY|PG_UNMANAGED)) ||
			    !pmap_page_exists(vm_map_pmap(map), p)) {
				p = next;
				continue;
			}

			actcount = pmap_ts_referenced(p);
			if (actcount) {
				vm_page_flag_set(p, PG_REFERENCED);
			} else if (p->flags & PG_REFERENCED) {
				actcount = 1;
			}

			if ((p->queue != PQ_ACTIVE) &&
				(p->flags & PG_REFERENCED)) {
				vm_page_activate(p);
				p->act_count += actcount;
				vm_page_flag_clear(p, PG_REFERENCED);
			} else if (p->queue == PQ_ACTIVE) {
				if ((p->flags & PG_REFERENCED) == 0) {
					p->act_count -= min(p->act_count, ACT_DECLINE);
					if (!remove_mode && (vm_pageout_algorithm_lru || (p->act_count == 0))) {
						vm_page_protect(p, VM_PROT_NONE);
						vm_page_deactivate(p);
					} else {
						s = splvm();
						TAILQ_REMOVE(&vm_page_queues[PQ_ACTIVE].pl, p, pageq);
						TAILQ_INSERT_TAIL(&vm_page_queues[PQ_ACTIVE].pl, p, pageq);
						splx(s);
					}
				} else {
					vm_page_activate(p);
					vm_page_flag_clear(p, PG_REFERENCED);
					if (p->act_count < (ACT_MAX - ACT_ADVANCE))
						p->act_count += ACT_ADVANCE;
					s = splvm();
					TAILQ_REMOVE(&vm_page_queues[PQ_ACTIVE].pl, p, pageq);
					TAILQ_INSERT_TAIL(&vm_page_queues[PQ_ACTIVE].pl, p, pageq);
					splx(s);
				}
			} else if (p->queue == PQ_INACTIVE) {
				vm_page_protect(p, VM_PROT_NONE);
			}
			p = next;
		}
		object = object->backing_object;
	}
	return;
}

/*
 * deactivate some number of pages in a map, try to do it fairly, but
 * that is really hard to do.
 */
static void
vm_pageout_map_deactivate_pages(map, desired)
	vm_map_t map;
	vm_pindex_t desired;
{
	vm_map_entry_t tmpe;
	vm_object_t obj, bigobj;

	if (lockmgr(&map->lock, LK_EXCLUSIVE | LK_NOWAIT, (void *)0, curproc)) {
		return;
	}

	bigobj = NULL;

	/*
	 * first, search out the biggest object, and try to free pages from
	 * that.
	 */
	tmpe = map->header.next;
	while (tmpe != &map->header) {
		if ((tmpe->eflags & MAP_ENTRY_IS_SUB_MAP) == 0) {
			obj = tmpe->object.vm_object;
			if ((obj != NULL) && (obj->shadow_count <= 1) &&
				((bigobj == NULL) ||
				 (bigobj->resident_page_count < obj->resident_page_count))) {
				bigobj = obj;
			}
		}
		tmpe = tmpe->next;
	}

	if (bigobj)
		vm_pageout_object_deactivate_pages(map, bigobj, desired, 0);

	/*
	 * Next, hunt around for other pages to deactivate.  We actually
	 * do this search sort of wrong -- .text first is not the best idea.
	 */
	tmpe = map->header.next;
	while (tmpe != &map->header) {
		if (pmap_resident_count(vm_map_pmap(map)) <= desired)
			break;
		if ((tmpe->eflags & MAP_ENTRY_IS_SUB_MAP) == 0) {
			obj = tmpe->object.vm_object;
			if (obj)
				vm_pageout_object_deactivate_pages(map, obj, desired, 0);
		}
		tmpe = tmpe->next;
	};

	/*
	 * Remove all mappings if a process is swapped out, this will free page
	 * table pages.
	 */
	if (desired == 0)
		pmap_remove(vm_map_pmap(map),
			VM_MIN_ADDRESS, VM_MAXUSER_ADDRESS);
	vm_map_unlock(map);
	return;
}
#endif

/*
 * Don't try to be fancy - being fancy can lead to VOP_LOCK's and therefore
 * to vnode deadlocks.  We only do it for OBJT_DEFAULT and OBJT_SWAP objects
 * which we know can be trivially freed.
 */

void
vm_pageout_page_free(vm_page_t m) {
	vm_object_t object = m->object;
	int type = object->type;

	if (type == OBJT_SWAP || type == OBJT_DEFAULT)
		vm_object_reference(object);
	vm_page_busy(m);
	vm_page_protect(m, VM_PROT_NONE);
	vm_page_free(m);
	if (type == OBJT_SWAP || type == OBJT_DEFAULT)
		vm_object_deallocate(object);
}

/*
 *	vm_pageout_scan does the dirty work for the pageout daemon.
 */
static int
vm_pageout_scan()
{
	vm_page_t m, next;
	struct vm_page marker;
	int page_shortage, maxscan, pcount;
	int addl_page_shortage, addl_page_shortage_init;
	int maxlaunder;
	struct proc *p, *bigproc;
	vm_offset_t size, bigsize;
	vm_object_t object;
	int force_wakeup = 0;
	int actcount;
	int vnodes_skipped = 0;
	int s;

	/*
	 * Do whatever cleanup that the pmap code can.
	 */
	pmap_collect();

	addl_page_shortage_init = vm_pageout_deficit;
	vm_pageout_deficit = 0;

	if (max_page_launder == 0)
		max_page_launder = 1;

	/*
	 * Calculate the number of pages we want to either free or move
	 * to the cache.  Be more agressive if we aren't making our target.
	 */

	page_shortage = vm_paging_target() +
		addl_page_shortage_init + vm_pageout_actcmp;

	/*
	 * Figure out how agressively we should flush dirty pages.
	 */
	{
		int factor = vm_pageout_actcmp;

		maxlaunder = cnt.v_inactive_target / 3 + factor;
		if (maxlaunder > max_page_launder + factor)
			maxlaunder = max_page_launder + factor;
	}

	/*
	 * Initialize our marker
	 */
	bzero(&marker, sizeof(marker));
	marker.flags = PG_BUSY | PG_FICTITIOUS | PG_MARKER;
	marker.queue = PQ_INACTIVE;
	marker.wire_count = 1;

	/*
	 * Start scanning the inactive queue for pages we can move to the
	 * cache or free.  The scan will stop when the target is reached or
	 * we have scanned the entire inactive queue.  Note that m->act_count
	 * is not used to form decisions for the inactive queue, only for the
	 * active queue.
	 */

rescan0:
	addl_page_shortage = addl_page_shortage_init;
	maxscan = cnt.v_inactive_count;
	for (m = TAILQ_FIRST(&vm_page_queues[PQ_INACTIVE].pl);
	     m != NULL && maxscan-- > 0 && page_shortage > 0;
	     m = next) {

		cnt.v_pdpages++;

		if (m->queue != PQ_INACTIVE) {
			goto rescan0;
		}

		next = TAILQ_NEXT(m, pageq);

		/*
		 * skip marker pages
		 */
		if (m->flags & PG_MARKER)
			continue;

		if (m->hold_count) {
			s = splvm();
			TAILQ_REMOVE(&vm_page_queues[PQ_INACTIVE].pl, m, pageq);
			TAILQ_INSERT_TAIL(&vm_page_queues[PQ_INACTIVE].pl, m, pageq);
			splx(s);
			addl_page_shortage++;
			continue;
		}
		/*
		 * Dont mess with busy pages, keep in the front of the
		 * queue, most likely are being paged out.
		 */
		if (m->busy || (m->flags & PG_BUSY)) {
			addl_page_shortage++;
			continue;
		}

		/*
		 * If the object is not being used, we ignore previous 
		 * references.
		 */
		if (m->object->ref_count == 0) {
			vm_page_flag_clear(m, PG_REFERENCED);
			pmap_clear_reference(m);

		/*
		 * Otherwise, if the page has been referenced while in the 
		 * inactive queue, we bump the "activation count" upwards, 
		 * making it less likely that the page will be added back to 
		 * the inactive queue prematurely again.  Here we check the 
		 * page tables (or emulated bits, if any), given the upper 
		 * level VM system not knowing anything about existing 
		 * references.
		 */
		} else if (((m->flags & PG_REFERENCED) == 0) &&
			(actcount = pmap_ts_referenced(m))) {
			vm_page_activate(m);
			m->act_count += (actcount + ACT_ADVANCE);
			continue;
		}

		/*
		 * If the upper level VM system knows about any page 
		 * references, we activate the page.  We also set the 
		 * "activation count" higher than normal so that we will less 
		 * likely place pages back onto the inactive queue again.
		 */
		if ((m->flags & PG_REFERENCED) != 0) {
			vm_page_flag_clear(m, PG_REFERENCED);
			actcount = pmap_ts_referenced(m);
			vm_page_activate(m);
			m->act_count += (actcount + ACT_ADVANCE + 1);
			continue;
		}

		/*
		 * If the upper level VM system doesn't know anything about 
		 * the page being dirty, we have to check for it again.  As 
		 * far as the VM code knows, any partially dirty pages are 
		 * fully dirty.
		 */
		if (m->dirty == 0) {
			vm_page_test_dirty(m);
		} else {
			vm_page_dirty(m);
		}

		/*
		 * Invalid pages can be easily freed
		 */
		if (m->valid == 0) {
			vm_pageout_page_free(m);
			cnt.v_dfree++;
			--page_shortage;

		/*
		 * Clean pages can be placed onto the cache queue.  This
		 * effectively frees them.
		 */
		} else if (m->dirty == 0) {
			vm_page_cache(m);
			--page_shortage;

		/*
		 * Dirty pages need to be paged out.  Note that we clean
		 * only a limited number of pages per pagedaemon pass.
		 */
		} else if (maxlaunder > 0) {
			int swap_pageouts_ok;
			struct vnode *vp = NULL;
			struct mount *mp;

			object = m->object;

			if ((object->type != OBJT_SWAP) && (object->type != OBJT_DEFAULT)) {
				swap_pageouts_ok = 1;
			} else {
				swap_pageouts_ok = !(defer_swap_pageouts || disable_swap_pageouts);
				swap_pageouts_ok |= (!disable_swap_pageouts && defer_swap_pageouts &&
				vm_page_count_min());
										
			}

			/*
			 * We don't bother paging objects that are "dead".  
			 * Those objects are in a "rundown" state.
			 */
			if (!swap_pageouts_ok || (object->flags & OBJ_DEAD)) {
				s = splvm();
				TAILQ_REMOVE(&vm_page_queues[PQ_INACTIVE].pl, m, pageq);
				TAILQ_INSERT_TAIL(&vm_page_queues[PQ_INACTIVE].pl, m, pageq);
				splx(s);
				continue;
			}

			/*
			 * Presumably we have sufficient free memory to do
			 * the more sophisticated checks and locking required
			 * for vnodes.
			 *
			 * The object is already known NOT to be dead.  The
			 * vget() may still block, though, because 
			 * VOP_ISLOCKED() doesn't check to see if an inode
			 * (v_data) is associated with the vnode.  If it isn't,
			 * vget() will load in it from disk.  Worse, vget()
			 * may actually get stuck waiting on "inode" if another
			 * process is in the process of bringing the inode in.
			 * This is bad news for us either way.
			 *
			 * So for the moment we check v_data == NULL as a
			 * workaround.  This means that vnodes which do not
			 * use v_data in the way we expect probably will not
			 * wind up being paged out by the pager and it will be
			 * up to the syncer to get them.  That's better then
			 * us blocking here.
			 *
			 * This whole code section is bogus - we need to fix
			 * the vnode pager to handle vm_page_t's without us
			 * having to do any sophisticated VOP tests.
			 */

			if (object->type == OBJT_VNODE) {
				vp = object->handle;

				mp = NULL;
				if (vp->v_type == VREG)
					vn_start_write(vp, &mp, V_NOWAIT);
				if (VOP_ISLOCKED(vp, NULL) ||
				    vp->v_data == NULL ||
				    vget(vp, LK_EXCLUSIVE|LK_NOOBJ, curproc)) {
					vn_finished_write(mp);
					if ((m->queue == PQ_INACTIVE) &&
						(m->hold_count == 0) &&
						(m->busy == 0) &&
						(m->flags & PG_BUSY) == 0) {
						s = splvm();
						TAILQ_REMOVE(&vm_page_queues[PQ_INACTIVE].pl, m, pageq);
						TAILQ_INSERT_TAIL(&vm_page_queues[PQ_INACTIVE].pl, m, pageq);
						splx(s);
					}
					if (object->flags & OBJ_MIGHTBEDIRTY)
						vnodes_skipped++;
					continue;
				}

				/*
				 * The page might have been moved to another
				 * queue during potential blocking in vget()
				 * above.  The page might have been freed and
				 * reused for another vnode.  The object might
				 * have been reused for another vnode.
				 */
				if (m->queue != PQ_INACTIVE ||
				    m->object != object ||
				    object->handle != vp) {
					if (object->flags & OBJ_MIGHTBEDIRTY)
						vnodes_skipped++;
					vput(vp);
					vn_finished_write(mp);
					continue;
				}
	
				/*
				 * The page may have been busied during the
				 * blocking in vput();  We don't move the
				 * page back onto the end of the queue so that
				 * statistics are more correct if we don't.
				 */
				if (m->busy || (m->flags & PG_BUSY)) {
					vput(vp);
					vn_finished_write(mp);
					continue;
				}

				/*
				 * If the page has become held, then skip it
				 */
				if (m->hold_count) {
					s = splvm();
					TAILQ_REMOVE(&vm_page_queues[PQ_INACTIVE].pl, m, pageq);
					TAILQ_INSERT_TAIL(&vm_page_queues[PQ_INACTIVE].pl, m, pageq);
					splx(s);
					if (object->flags & OBJ_MIGHTBEDIRTY)
						vnodes_skipped++;
					vput(vp);
					vn_finished_write(mp);
					continue;
				}
			}

			/*
			 * If a page is dirty, then it is either being washed
			 * (but not yet cleaned) or it is still in the
			 * laundry.  If it is still in the laundry, then we
			 * start the cleaning operation.  maxlaunder nominally
			 * counts I/O cost (seeks) rather then bytes.
			 *
			 * This operation may cluster, invalidating the 'next'
			 * pointer.  To prevent an inordinate number of
			 * restarts we use our marker to remember our place.
			 */
			s = splvm();
			TAILQ_INSERT_AFTER(&vm_page_queues[PQ_INACTIVE].pl, m, &marker, pageq);
			splx(s);
			if (vm_pageout_clean(m) != 0)
				--maxlaunder;
			s = splvm();
			next = TAILQ_NEXT(&marker, pageq);
			TAILQ_REMOVE(&vm_page_queues[PQ_INACTIVE].pl, &marker, pageq);
			splx(s);
			if (vp) {
				vput(vp);
				vn_finished_write(mp);
			}
		}
	}

	/*
	 * If we were not able to meet our target, increase actcmp
	 */

	if (vm_page_count_min()) {
		if (vm_pageout_actcmp < ACT_MAX / 2)
			vm_pageout_actcmp += ACT_ADVANCE;
	} else {
		if (vm_pageout_actcmp < ACT_DECLINE)
			vm_pageout_actcmp = 0;
		else
			vm_pageout_actcmp -= ACT_DECLINE;
	}

	/*
	 * Compute the number of pages we want to try to move from the
	 * active queue to the inactive queue.
	 */

	page_shortage = vm_paging_target() +
		cnt.v_inactive_target - cnt.v_inactive_count;
	page_shortage += addl_page_shortage;
	page_shortage += vm_pageout_actcmp;

	/*
	 * Scan the active queue for things we can deactivate. We nominally
	 * track the per-page activity counter and use it to locate
	 * deactivation candidates.
	 */

	pcount = cnt.v_active_count;
	m = TAILQ_FIRST(&vm_page_queues[PQ_ACTIVE].pl);

	while ((m != NULL) && (pcount-- > 0) && (page_shortage > 0)) {

		/*
		 * This is a consistency check, and should likely be a panic
		 * or warning.
		 */
		if (m->queue != PQ_ACTIVE) {
			break;
		}

		next = TAILQ_NEXT(m, pageq);
		/*
		 * Don't deactivate pages that are busy.
		 */
		if ((m->busy != 0) ||
		    (m->flags & PG_BUSY) ||
		    (m->hold_count != 0)) {
			s = splvm();
			TAILQ_REMOVE(&vm_page_queues[PQ_ACTIVE].pl, m, pageq);
			TAILQ_INSERT_TAIL(&vm_page_queues[PQ_ACTIVE].pl, m, pageq);
			splx(s);
			m = next;
			continue;
		}

		/*
		 * The count for pagedaemon pages is done after checking the
		 * page for eligibility...
		 */
		cnt.v_pdpages++;

		/*
		 * Check to see "how much" the page has been used.
		 */
		actcount = 0;
		if (m->object->ref_count != 0) {
			if (m->flags & PG_REFERENCED) {
				actcount += 1;
			}
			actcount += pmap_ts_referenced(m);
			if (actcount) {
				m->act_count += ACT_ADVANCE + actcount;
				if (m->act_count > ACT_MAX)
					m->act_count = ACT_MAX;
			}
		}

		/*
		 * Since we have "tested" this bit, we need to clear it now.
		 */
		vm_page_flag_clear(m, PG_REFERENCED);

		/*
		 * Only if an object is currently being used, do we use the
		 * page activation count stats.
		 */
		if (actcount && (m->object->ref_count != 0)) {
			s = splvm();
			TAILQ_REMOVE(&vm_page_queues[PQ_ACTIVE].pl, m, pageq);
			TAILQ_INSERT_TAIL(&vm_page_queues[PQ_ACTIVE].pl, m, pageq);
			splx(s);
		} else {
			m->act_count -= min(m->act_count, ACT_DECLINE);
			if (vm_pageout_algorithm_lru ||
			    (m->object->ref_count == 0) || 
			    (m->act_count <= vm_pageout_actcmp)) {
				page_shortage--;
				if (m->object->ref_count == 0) {
					vm_page_protect(m, VM_PROT_NONE);
					if (m->dirty == 0)
						vm_page_cache(m);
					else
						vm_page_deactivate(m);
				} else {
					vm_page_deactivate(m);
				}
			} else {
				s = splvm();
				TAILQ_REMOVE(&vm_page_queues[PQ_ACTIVE].pl, m, pageq);
				TAILQ_INSERT_TAIL(&vm_page_queues[PQ_ACTIVE].pl, m, pageq);
				splx(s);
			}
		}
		m = next;
	}

	s = splvm();

	/*
	 * We try to maintain some *really* free pages, this allows interrupt
	 * code to be guaranteed space.  Since both cache and free queues 
	 * are considered basically 'free', moving pages from cache to free
	 * does not effect other calculations.
	 */

	while (cnt.v_free_count < cnt.v_free_reserved) {
		static int cache_rover = 0;
		m = vm_page_list_find(PQ_CACHE, cache_rover, FALSE);
		if (!m)
			break;
		if ((m->flags & (PG_BUSY|PG_UNMANAGED)) || 
		    m->busy || 
		    m->hold_count || 
		    m->wire_count) {
#ifdef INVARIANTS
			printf("Warning: busy page %p found in cache\n", m);
#endif
			vm_page_deactivate(m);
			continue;
		}
		cache_rover = (cache_rover + PQ_PRIME2) & PQ_L2_MASK;
		vm_pageout_page_free(m);
		cnt.v_dfree++;
	}
	splx(s);

#if !defined(NO_SWAPPING)
	/*
	 * Idle process swapout -- run once per second.
	 */
	if (vm_swap_idle_enabled) {
		static long lsec;
		if (time_second != lsec) {
			vm_pageout_req_swapout |= VM_SWAP_IDLE;
			vm_req_vmdaemon();
			lsec = time_second;
		}
	}
#endif
		
	/*
	 * If we didn't get enough free pages, and we have skipped a vnode
	 * in a writeable object, wakeup the sync daemon.  And kick swapout
	 * if we did not get enough free pages.
	 */
	if (vm_paging_target() > 0) {
		if (vnodes_skipped && vm_page_count_min())
			(void) speedup_syncer();
#if !defined(NO_SWAPPING)
		if (vm_swap_enabled && vm_page_count_target()) {
			vm_req_vmdaemon();
			vm_pageout_req_swapout |= VM_SWAP_NORMAL;
		}
#endif
	}

	/*
	 * make sure that we have swap space -- if we are low on memory and
	 * swap -- then kill the biggest process.
	 */
	if ((vm_swap_size < 64 || swap_pager_full) && vm_page_count_min()) {
		bigproc = NULL;
		bigsize = 0;
		for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
			/*
			 * if this is a system process, skip it
			 */
			if ((p->p_flag & P_SYSTEM) || (p->p_lock > 0) ||
			    (p->p_pid == 1) ||
			    ((p->p_pid < 48) && (vm_swap_size != 0))) {
				continue;
			}
			/*
			 * if the process is in a non-running type state,
			 * don't touch it.
			 */
			if (p->p_stat != SRUN && p->p_stat != SSLEEP) {
				continue;
			}
			/*
			 * get the process size
			 */
			size = vmspace_resident_count(p->p_vmspace);
			/*
			 * if the this process is bigger than the biggest one
			 * remember it.
			 */
			if (size > bigsize) {
				bigproc = p;
				bigsize = size;
			}
		}
		if (bigproc != NULL) {
			killproc(bigproc, "out of swap space");
			bigproc->p_estcpu = 0;
			bigproc->p_nice = PRIO_MIN;
			resetpriority(bigproc);
			wakeup(&cnt.v_free_count);
		}
	}
	return force_wakeup;
}

/*
 * This routine tries to maintain the pseudo LRU active queue,
 * so that during long periods of time where there is no paging,
 * that some statistic accumulation still occurs.  This code
 * helps the situation where paging just starts to occur.
 */
static void
vm_pageout_page_stats()
{
	int s;
	vm_page_t m,next;
	int pcount,tpcount;		/* Number of pages to check */
	static int fullintervalcount = 0;
	int page_shortage;
	int s0;

	page_shortage = 
	    (cnt.v_inactive_target + cnt.v_cache_max + cnt.v_free_min) -
	    (cnt.v_free_count + cnt.v_inactive_count + cnt.v_cache_count);

	if (page_shortage <= 0)
		return;

	s0 = splvm();

	pcount = cnt.v_active_count;
	fullintervalcount += vm_pageout_stats_interval;
	if (fullintervalcount < vm_pageout_full_stats_interval) {
		tpcount = (vm_pageout_stats_max * cnt.v_active_count) / cnt.v_page_count;
		if (pcount > tpcount)
			pcount = tpcount;
	} else {
		fullintervalcount = 0;
	}

	m = TAILQ_FIRST(&vm_page_queues[PQ_ACTIVE].pl);
	while ((m != NULL) && (pcount-- > 0)) {
		int actcount;

		if (m->queue != PQ_ACTIVE) {
			break;
		}

		next = TAILQ_NEXT(m, pageq);
		/*
		 * Don't deactivate pages that are busy.
		 */
		if ((m->busy != 0) ||
		    (m->flags & PG_BUSY) ||
		    (m->hold_count != 0)) {
			s = splvm();
			TAILQ_REMOVE(&vm_page_queues[PQ_ACTIVE].pl, m, pageq);
			TAILQ_INSERT_TAIL(&vm_page_queues[PQ_ACTIVE].pl, m, pageq);
			splx(s);
			m = next;
			continue;
		}

		actcount = 0;
		if (m->flags & PG_REFERENCED) {
			vm_page_flag_clear(m, PG_REFERENCED);
			actcount += 1;
		}

		actcount += pmap_ts_referenced(m);
		if (actcount) {
			m->act_count += ACT_ADVANCE + actcount;
			if (m->act_count > ACT_MAX)
				m->act_count = ACT_MAX;
			s = splvm();
			TAILQ_REMOVE(&vm_page_queues[PQ_ACTIVE].pl, m, pageq);
			TAILQ_INSERT_TAIL(&vm_page_queues[PQ_ACTIVE].pl, m, pageq);
			splx(s);
		} else {
			if (m->act_count == 0) {
				/*
				 * We turn off page access, so that we have more accurate
				 * RSS stats.  We don't do this in the normal page deactivation
				 * when the system is loaded VM wise, because the cost of
				 * the large number of page protect operations would be higher
				 * than the value of doing the operation.
				 */
				vm_page_protect(m, VM_PROT_NONE);
				vm_page_deactivate(m);
			} else {
				m->act_count -= min(m->act_count, ACT_DECLINE);
				s = splvm();
				TAILQ_REMOVE(&vm_page_queues[PQ_ACTIVE].pl, m, pageq);
				TAILQ_INSERT_TAIL(&vm_page_queues[PQ_ACTIVE].pl, m, pageq);
				splx(s);
			}
		}

		m = next;
	}
	splx(s0);
}

static int
vm_pageout_free_page_calc(count)
vm_size_t count;
{
	if (count < cnt.v_page_count)
		 return 0;
	/*
	 * free_reserved needs to include enough for the largest swap pager
	 * structures plus enough for any pv_entry structs when paging.
	 */
	if (cnt.v_page_count > 1024)
		cnt.v_free_min = 4 + (cnt.v_page_count - 1024) / 200;
	else
		cnt.v_free_min = 4;
	cnt.v_pageout_free_min = (2*MAXBSIZE)/PAGE_SIZE +
		cnt.v_interrupt_free_min;
	cnt.v_free_reserved = vm_pageout_page_count +
		cnt.v_pageout_free_min + (count / 768) + PQ_L2_SIZE;
	cnt.v_free_severe = cnt.v_free_min / 2;
	cnt.v_free_min += cnt.v_free_reserved;
	cnt.v_free_severe += cnt.v_free_reserved;
	return 1;
}


/*
 *	vm_pageout is the high level pageout daemon.
 */
static void
vm_pageout()
{

	mtx_enter(&Giant, MTX_DEF);

	/*
	 * Initialize some paging parameters.
	 */

	cnt.v_interrupt_free_min = 2;
	if (cnt.v_page_count < 2000)
		vm_pageout_page_count = 8;

	vm_pageout_free_page_calc(cnt.v_page_count);
	/*
	 * free_reserved needs to include enough for the largest swap pager
	 * structures plus enough for any pv_entry structs when paging.
	 */
	if (cnt.v_free_count > 6144)
		cnt.v_free_target = 3 * cnt.v_free_min + cnt.v_free_reserved;
	else
		cnt.v_free_target = 2 * cnt.v_free_min + cnt.v_free_reserved;

	if (cnt.v_free_count > 2048) {
		cnt.v_cache_min = cnt.v_free_target;
		cnt.v_cache_max = 2 * cnt.v_cache_min;
		cnt.v_inactive_target = (3 * cnt.v_free_target) / 2;
	} else {
		cnt.v_cache_min = 0;
		cnt.v_cache_max = 0;
		cnt.v_inactive_target = cnt.v_free_count / 4;
	}
	if (cnt.v_inactive_target > cnt.v_free_count / 3)
		cnt.v_inactive_target = cnt.v_free_count / 3;

	/* XXX does not really belong here */
	if (vm_page_max_wired == 0)
		vm_page_max_wired = cnt.v_free_count / 3;

	if (vm_pageout_stats_max == 0)
		vm_pageout_stats_max = cnt.v_free_target;

	/*
	 * Set interval in seconds for stats scan.
	 */
	if (vm_pageout_stats_interval == 0)
		vm_pageout_stats_interval = 5;
	if (vm_pageout_full_stats_interval == 0)
		vm_pageout_full_stats_interval = vm_pageout_stats_interval * 4;
	

	/*
	 * Set maximum free per pass
	 */
	if (vm_pageout_stats_free_max == 0)
		vm_pageout_stats_free_max = 5;

	max_page_launder = (cnt.v_page_count > 1800 ? 32 : 16);

	curproc->p_flag |= P_BUFEXHAUST;
	swap_pager_swap_init();
	/*
	 * The pageout daemon is never done, so loop forever.
	 */
	while (TRUE) {
		int error;
		int s = splvm();

		/*
		 * If we have enough free memory, wakeup waiters.  Do
		 * not clear vm_pages_needed until we reach our target,
		 * otherwise we may be woken up over and over again and
		 * waste a lot of cpu.
		 */
		if (vm_pages_needed && !vm_page_count_min()) {
			if (vm_paging_needed() <= 0)
				vm_pages_needed = 0;
			wakeup(&cnt.v_free_count);
		}
		if (vm_pages_needed) {
			/*
			 * Still not done, sleep a bit and go again
			 */
			tsleep(&vm_pages_needed, PVM, "psleep", hz/2);
		} else {
			/*
			 * Good enough, sleep & handle stats
			 */
			error = tsleep(&vm_pages_needed,
				PVM, "psleep", vm_pageout_stats_interval * hz);
			if (error && !vm_pages_needed) {
				if (vm_pageout_actcmp > 0)
					--vm_pageout_actcmp;
				splx(s);
				vm_pageout_page_stats();
				continue;
			}
		}

		if (vm_pages_needed)
			cnt.v_pdwakeups++;
		splx(s);
		vm_pageout_scan();
		vm_pageout_deficit = 0;
	}
}

void
pagedaemon_wakeup()
{
	if (!vm_pages_needed && curproc != pageproc) {
		vm_pages_needed++;
		wakeup(&vm_pages_needed);
	}
}

#if !defined(NO_SWAPPING)
static void
vm_req_vmdaemon()
{
	static int lastrun = 0;

	if ((ticks > (lastrun + hz)) || (ticks < lastrun)) {
		wakeup(&vm_daemon_needed);
		lastrun = ticks;
	}
}

static void
vm_daemon()
{
	struct proc *p;

	mtx_enter(&Giant, MTX_DEF);

	while (TRUE) {
		tsleep(&vm_daemon_needed, PPAUSE, "psleep", 0);
		if (vm_pageout_req_swapout) {
			swapout_procs(vm_pageout_req_swapout);
			vm_pageout_req_swapout = 0;
		}
		/*
		 * scan the processes for exceeding their rlimits or if
		 * process is swapped out -- deactivate pages
		 */

		for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
			vm_pindex_t limit, size;

			/*
			 * if this is a system process or if we have already
			 * looked at this process, skip it.
			 */
			if (p->p_flag & (P_SYSTEM | P_WEXIT)) {
				continue;
			}
			/*
			 * if the process is in a non-running type state,
			 * don't touch it.
			 */
			if (p->p_stat != SRUN && p->p_stat != SSLEEP) {
				continue;
			}
			/*
			 * get a limit
			 */
			limit = OFF_TO_IDX(
			    qmin(p->p_rlimit[RLIMIT_RSS].rlim_cur,
				p->p_rlimit[RLIMIT_RSS].rlim_max));

			/*
			 * let processes that are swapped out really be
			 * swapped out set the limit to nothing (will force a
			 * swap-out.)
			 */
			if ((p->p_flag & P_INMEM) == 0)
				limit = 0;	/* XXX */

			size = vmspace_resident_count(p->p_vmspace);
			if (limit >= 0 && size >= limit) {
				vm_pageout_map_deactivate_pages(
				    &p->p_vmspace->vm_map, limit);
			}
		}
	}
}
#endif
