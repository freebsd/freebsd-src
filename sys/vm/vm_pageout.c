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
 * $Id: vm_pageout.c,v 1.85 1996/09/08 20:44:48 dyson Exp $
 */

/*
 *	The proverbial page-out daemon.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/vnode.h>
#include <sys/vmmeter.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_prot.h>
#include <vm/lock.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_pageout.h>
#include <vm/vm_kern.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>
#include <vm/vm_extern.h>

/*
 * System initialization
 */

/* the kernel process "vm_pageout"*/
static void vm_pageout __P((void));
static int vm_pageout_clean __P((vm_page_t, int));
static int vm_pageout_scan __P((void));
static int vm_pageout_free_page_calc __P((vm_size_t count));
struct proc *pageproc;

static struct kproc_desc page_kp = {
	"pagedaemon",
	vm_pageout,
	&pageproc
};
SYSINIT_KT(pagedaemon, SI_SUB_KTHREAD_PAGE, SI_ORDER_FIRST, kproc_start, &page_kp)

#if !defined(NO_SWAPPING)
/* the kernel process "vm_daemon"*/
static void vm_daemon __P((void));
static struct	proc *vmproc;

static struct kproc_desc vm_kp = {
	"vmdaemon",
	vm_daemon,
	&vmproc
};
SYSINIT_KT(vmdaemon, SI_SUB_KTHREAD_VM, SI_ORDER_FIRST, kproc_start, &vm_kp)
#endif


int vm_pages_needed;		/* Event on which pageout daemon sleeps */

int vm_pageout_pages_needed;	/* flag saying that the pageout daemon needs pages */

extern int npendingio;
#if !defined(NO_SWAPPING)
static int vm_pageout_req_swapout;	/* XXX */
static int vm_daemon_needed;
#endif
extern int nswiodone;
extern int vm_swap_size;
extern int vfs_update_wakeup;
int vm_pageout_algorithm_lru=0;
#if defined(NO_SWAPPING)
int vm_swapping_enabled=0;
#else
int vm_swapping_enabled=1;
#endif

SYSCTL_INT(_vm, VM_PAGEOUT_ALGORITHM, pageout_algorithm,
	CTLFLAG_RW, &vm_pageout_algorithm_lru, 0, "");

#if defined(NO_SWAPPING)
SYSCTL_INT(_vm, VM_SWAPPING_ENABLED, swapping_enabled,
	CTLFLAG_RD, &vm_swapping_enabled, 0, "");
#else
SYSCTL_INT(_vm, VM_SWAPPING_ENABLED, swapping_enabled,
	CTLFLAG_RW, &vm_swapping_enabled, 0, "");
#endif

#define MAXLAUNDER (cnt.v_page_count > 1800 ? 32 : 16)

#define VM_PAGEOUT_PAGE_COUNT 16
int vm_pageout_page_count = VM_PAGEOUT_PAGE_COUNT;

int vm_page_max_wired;		/* XXX max # of wired pages system-wide */

#if !defined(NO_SWAPPING)
typedef void freeer_fcn_t __P((vm_map_t, vm_object_t, vm_pindex_t, int));
static void vm_pageout_map_deactivate_pages __P((vm_map_t, vm_pindex_t));
static freeer_fcn_t vm_pageout_object_deactivate_pages;
static void vm_req_vmdaemon __P((void));
#endif

/*
 * vm_pageout_clean:
 *
 * Clean the page and remove it from the laundry.
 * 
 * We set the busy bit to cause potential page faults on this page to
 * block.
 * 
 * And we set pageout-in-progress to keep the object from disappearing
 * during pageout.  This guarantees that the page won't move from the
 * inactive queue.  (However, any other page on the inactive queue may
 * move!)
 */
static int
vm_pageout_clean(m, sync)
	vm_page_t m;
	int sync;
{
	register vm_object_t object;
	vm_page_t mc[2*vm_pageout_page_count];
	int pageout_count;
	int i, forward_okay, backward_okay, page_base;
	vm_pindex_t pindex = m->pindex;

	object = m->object;

	/*
	 * If not OBJT_SWAP, additional memory may be needed to do the pageout.
	 * Try to avoid the deadlock.
	 */
	if ((sync != VM_PAGEOUT_FORCE) &&
	    (object->type == OBJT_DEFAULT) &&
	    ((cnt.v_free_count + cnt.v_cache_count) < cnt.v_pageout_free_min))
		return 0;

	/*
	 * Don't mess with the page if it's busy.
	 */
	if ((!sync && m->hold_count != 0) ||
	    ((m->busy != 0) || (m->flags & PG_BUSY)))
		return 0;

	/*
	 * Try collapsing before it's too late.
	 */
	if (!sync && object->backing_object) {
		vm_object_collapse(object);
	}
	mc[vm_pageout_page_count] = m;
	pageout_count = 1;
	page_base = vm_pageout_page_count;
	forward_okay = TRUE;
	if (pindex != 0)
		backward_okay = TRUE;
	else
		backward_okay = FALSE;
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
	 */
	for (i = 1; (i < vm_pageout_page_count) && (forward_okay || backward_okay); i++) {
		vm_page_t p;

		/*
		 * See if forward page is clusterable.
		 */
		if (forward_okay) {
			/*
			 * Stop forward scan at end of object.
			 */
			if ((pindex + i) > object->size) {
				forward_okay = FALSE;
				goto do_backward;
			}
			p = vm_page_lookup(object, pindex + i);
			if (p) {
				if (((p->queue - p->pc) == PQ_CACHE) ||
					(p->flags & PG_BUSY) || p->busy) {
					forward_okay = FALSE;
					goto do_backward;
				}
				vm_page_test_dirty(p);
				if ((p->dirty & p->valid) != 0 &&
				    ((p->queue == PQ_INACTIVE) ||
				     (sync == VM_PAGEOUT_FORCE)) &&
				    (p->wire_count == 0) &&
				    (p->hold_count == 0)) {
					mc[vm_pageout_page_count + i] = p;
					pageout_count++;
					if (pageout_count == vm_pageout_page_count)
						break;
				} else {
					forward_okay = FALSE;
				}
			} else {
				forward_okay = FALSE;
			}
		}
do_backward:
		/*
		 * See if backward page is clusterable.
		 */
		if (backward_okay) {
			/*
			 * Stop backward scan at beginning of object.
			 */
			if ((pindex - i) == 0) {
				backward_okay = FALSE;
			}
			p = vm_page_lookup(object, pindex - i);
			if (p) {
				if (((p->queue - p->pc) == PQ_CACHE) ||
					(p->flags & PG_BUSY) || p->busy) {
					backward_okay = FALSE;
					continue;
				}
				vm_page_test_dirty(p);
				if ((p->dirty & p->valid) != 0 &&
				    ((p->queue == PQ_INACTIVE) ||
				     (sync == VM_PAGEOUT_FORCE)) &&
				    (p->wire_count == 0) &&
				    (p->hold_count == 0)) {
					mc[vm_pageout_page_count - i] = p;
					pageout_count++;
					page_base--;
					if (pageout_count == vm_pageout_page_count)
						break;
				} else {
					backward_okay = FALSE;
				}
			} else {
				backward_okay = FALSE;
			}
		}
	}

	/*
	 * we allow reads during pageouts...
	 */
	for (i = page_base; i < (page_base + pageout_count); i++) {
		mc[i]->flags |= PG_BUSY;
		vm_page_protect(mc[i], VM_PROT_READ);
	}

	return vm_pageout_flush(&mc[page_base], pageout_count, sync);
}

int
vm_pageout_flush(mc, count, sync)
	vm_page_t *mc;
	int count;
	int sync;
{
	register vm_object_t object;
	int pageout_status[count];
	int anyok = 0;
	int i;

	object = mc[0]->object;
	object->paging_in_progress += count;

	vm_pager_put_pages(object, mc, count,
	    ((sync || (object == kernel_object)) ? TRUE : FALSE),
	    pageout_status);

	for (i = 0; i < count; i++) {
		vm_page_t mt = mc[i];

		switch (pageout_status[i]) {
		case VM_PAGER_OK:
			++anyok;
			break;
		case VM_PAGER_PEND:
			++anyok;
			break;
		case VM_PAGER_BAD:
			/*
			 * Page outside of range of object. Right now we
			 * essentially lose the changes by pretending it
			 * worked.
			 */
			pmap_clear_modify(VM_PAGE_TO_PHYS(mt));
			mt->dirty = 0;
			break;
		case VM_PAGER_ERROR:
		case VM_PAGER_FAIL:
			/*
			 * If page couldn't be paged out, then reactivate the
			 * page so it doesn't clog the inactive list.  (We
			 * will try paging out it again later).
			 */
			if (mt->queue == PQ_INACTIVE)
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
			PAGE_WAKEUP(mt);
		}
	}
	return anyok;
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

	if (object->type == OBJT_DEVICE)
		return;

	while (object) {
		if (vm_map_pmap(map)->pm_stats.resident_count <= desired)
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
			int refcount;
			if (vm_map_pmap(map)->pm_stats.resident_count <= desired)
				return;
			next = TAILQ_NEXT(p, listq);
			cnt.v_pdpages++;
			if (p->wire_count != 0 ||
			    p->hold_count != 0 ||
			    p->busy != 0 ||
			    (p->flags & PG_BUSY) ||
			    !pmap_page_exists(vm_map_pmap(map), VM_PAGE_TO_PHYS(p))) {
				p = next;
				continue;
			}

			refcount = pmap_ts_referenced(VM_PAGE_TO_PHYS(p));
			if (refcount) {
				p->flags |= PG_REFERENCED;
			} else if (p->flags & PG_REFERENCED) {
				refcount = 1;
			}

			if ((p->queue != PQ_ACTIVE) &&
				(p->flags & PG_REFERENCED)) {
				vm_page_activate(p);
				p->act_count += refcount;
				p->flags &= ~PG_REFERENCED;
			} else if (p->queue == PQ_ACTIVE) {
				if ((p->flags & PG_REFERENCED) == 0) {
					p->act_count -= min(p->act_count, ACT_DECLINE);
					if (!remove_mode && (vm_pageout_algorithm_lru || (p->act_count == 0))) {
						vm_page_protect(p, VM_PROT_NONE);
						vm_page_deactivate(p);
					} else {
						s = splvm();
						TAILQ_REMOVE(&vm_page_queue_active, p, pageq);
						TAILQ_INSERT_TAIL(&vm_page_queue_active, p, pageq);
						splx(s);
					}
				} else {
					p->flags &= ~PG_REFERENCED;
					if (p->act_count < (ACT_MAX - ACT_ADVANCE))
						p->act_count += ACT_ADVANCE;
					s = splvm();
					TAILQ_REMOVE(&vm_page_queue_active, p, pageq);
					TAILQ_INSERT_TAIL(&vm_page_queue_active, p, pageq);
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

	vm_map_reference(map);
	if (!lock_try_write(&map->lock)) {
		vm_map_deallocate(map);
		return;
	}

	bigobj = NULL;

	/*
	 * first, search out the biggest object, and try to free pages from
	 * that.
	 */
	tmpe = map->header.next;
	while (tmpe != &map->header) {
		if ((tmpe->is_sub_map == 0) && (tmpe->is_a_map == 0)) {
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
		if (vm_map_pmap(map)->pm_stats.resident_count <= desired)
			break;
		if ((tmpe->is_sub_map == 0) && (tmpe->is_a_map == 0)) {
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
	vm_map_deallocate(map);
	return;
}
#endif

/*
 *	vm_pageout_scan does the dirty work for the pageout daemon.
 */
static int
vm_pageout_scan()
{
	vm_page_t m, next;
	int page_shortage, addl_page_shortage, maxscan, maxlaunder, pcount;
	int pages_freed;
	struct proc *p, *bigproc;
	vm_offset_t size, bigsize;
	vm_object_t object;
	int force_wakeup = 0;
	int vnodes_skipped = 0;
	int s;

	/*
	 * Start scanning the inactive queue for pages we can free. We keep
	 * scanning until we have enough free pages or we have scanned through
	 * the entire queue.  If we encounter dirty pages, we start cleaning
	 * them.
	 */

	pages_freed = 0;
	addl_page_shortage = 0;

	maxlaunder = (cnt.v_inactive_target > MAXLAUNDER) ?
	    MAXLAUNDER : cnt.v_inactive_target;
rescan0:
	maxscan = cnt.v_inactive_count;
	for( m = TAILQ_FIRST(&vm_page_queue_inactive);

		(m != NULL) && (maxscan-- > 0) &&
			((cnt.v_cache_count + cnt.v_free_count) <
			(cnt.v_cache_min + cnt.v_free_target));

		m = next) {

		cnt.v_pdpages++;

		if (m->queue != PQ_INACTIVE) {
			goto rescan0;
		}

		next = TAILQ_NEXT(m, pageq);

		if (m->hold_count) {
			s = splvm();
			TAILQ_REMOVE(&vm_page_queue_inactive, m, pageq);
			TAILQ_INSERT_TAIL(&vm_page_queue_inactive, m, pageq);
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

		if (m->object->ref_count == 0) {
			m->flags &= ~PG_REFERENCED;
			pmap_clear_reference(VM_PAGE_TO_PHYS(m));
		} else if (((m->flags & PG_REFERENCED) == 0) &&
			pmap_ts_referenced(VM_PAGE_TO_PHYS(m))) {
			vm_page_activate(m);
			continue;
		}

		if ((m->flags & PG_REFERENCED) != 0) {
			m->flags &= ~PG_REFERENCED;
			pmap_clear_reference(VM_PAGE_TO_PHYS(m));
			vm_page_activate(m);
			continue;
		}

		if (m->dirty == 0) {
			vm_page_test_dirty(m);
		} else if (m->dirty != 0) {
			m->dirty = VM_PAGE_BITS_ALL;
		}

		if (m->valid == 0) {
			vm_page_protect(m, VM_PROT_NONE);
			vm_page_free(m);
			cnt.v_dfree++;
			++pages_freed;
		} else if (m->dirty == 0) {
			vm_page_cache(m);
			++pages_freed;
		} else if (maxlaunder > 0) {
			int written;
			struct vnode *vp = NULL;

			object = m->object;
			if (object->flags & OBJ_DEAD) {
				s = splvm();
				TAILQ_REMOVE(&vm_page_queue_inactive, m, pageq);
				TAILQ_INSERT_TAIL(&vm_page_queue_inactive, m, pageq);
				splx(s);
				continue;
			}

			if (object->type == OBJT_VNODE) {
				vp = object->handle;
				if (VOP_ISLOCKED(vp) || vget(vp, 1)) {
					if ((m->queue == PQ_INACTIVE) &&
						(m->hold_count == 0) &&
						(m->busy == 0) &&
						(m->flags & PG_BUSY) == 0) {
						s = splvm();
						TAILQ_REMOVE(&vm_page_queue_inactive, m, pageq);
						TAILQ_INSERT_TAIL(&vm_page_queue_inactive, m, pageq);
						splx(s);
					}
					if (object->flags & OBJ_MIGHTBEDIRTY)
						++vnodes_skipped;
					continue;
				}

				/*
				 * The page might have been moved to another queue
				 * during potential blocking in vget() above.
				 */
				if (m->queue != PQ_INACTIVE) {
					if (object->flags & OBJ_MIGHTBEDIRTY)
						++vnodes_skipped;
					vput(vp);
					continue;
				}
	
				/*
				 * The page may have been busied during the blocking in
				 * vput();  We don't move the page back onto the end of
				 * the queue so that statistics are more correct if we don't.
				 */
				if (m->busy || (m->flags & PG_BUSY)) {
					vput(vp);
					continue;
				}

				/*
				 * If the page has become held, then skip it
				 */
				if (m->hold_count) {
					s = splvm();
					TAILQ_REMOVE(&vm_page_queue_inactive, m, pageq);
					TAILQ_INSERT_TAIL(&vm_page_queue_inactive, m, pageq);
					splx(s);
					if (object->flags & OBJ_MIGHTBEDIRTY)
						++vnodes_skipped;
					vput(vp);
					continue;
				}
			}

			/*
			 * If a page is dirty, then it is either being washed
			 * (but not yet cleaned) or it is still in the
			 * laundry.  If it is still in the laundry, then we
			 * start the cleaning operation.
			 */
			written = vm_pageout_clean(m, 0);

			if (vp)
				vput(vp);

			maxlaunder -= written;
		}
	}

	/*
	 * Compute the page shortage.  If we are still very low on memory be
	 * sure that we will move a minimal amount of pages from active to
	 * inactive.
	 */

	page_shortage = (cnt.v_inactive_target + cnt.v_cache_min) -
	    (cnt.v_free_count + cnt.v_inactive_count + cnt.v_cache_count);
	if (page_shortage <= 0) {
		if (pages_freed == 0) {
			page_shortage = cnt.v_free_min - cnt.v_free_count;
		} else {
			page_shortage = 1;
		}
	}
	if (addl_page_shortage) {
		if (page_shortage < 0)
			page_shortage = 0;
		page_shortage += addl_page_shortage;
	}

	pcount = cnt.v_active_count;
	m = TAILQ_FIRST(&vm_page_queue_active);
	while ((m != NULL) && (pcount-- > 0) && (page_shortage > 0)) {
		int refcount;

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
			TAILQ_REMOVE(&vm_page_queue_active, m, pageq);
			TAILQ_INSERT_TAIL(&vm_page_queue_active, m, pageq);
			splx(s);
			m = next;
			continue;
		}

		/*
		 * The count for pagedaemon pages is done after checking the
		 * page for eligbility...
		 */
		cnt.v_pdpages++;

		refcount = 0;
		if (m->object->ref_count != 0) {
			if (m->flags & PG_REFERENCED) {
				refcount += 1;
			}
			refcount += pmap_ts_referenced(VM_PAGE_TO_PHYS(m));
			if (refcount) {
				m->act_count += ACT_ADVANCE + refcount;
				if (m->act_count > ACT_MAX)
					m->act_count = ACT_MAX;
			}
		}

		m->flags &= ~PG_REFERENCED;

		if (refcount && (m->object->ref_count != 0)) {
			s = splvm();
			TAILQ_REMOVE(&vm_page_queue_active, m, pageq);
			TAILQ_INSERT_TAIL(&vm_page_queue_active, m, pageq);
			splx(s);
		} else {
			m->act_count -= min(m->act_count, ACT_DECLINE);
			if (vm_pageout_algorithm_lru ||
				(m->object->ref_count == 0) || (m->act_count == 0)) {
				--page_shortage;
				vm_page_protect(m, VM_PROT_NONE);
				if ((m->dirty == 0) &&
					(m->object->ref_count == 0)) {
					vm_page_cache(m);
				} else {
					vm_page_deactivate(m);
				}
			} else {
				s = splvm();
				TAILQ_REMOVE(&vm_page_queue_active, m, pageq);
				TAILQ_INSERT_TAIL(&vm_page_queue_active, m, pageq);
				splx(s);
			}
		}
		m = next;
	}

	s = splvm();
	/*
	 * We try to maintain some *really* free pages, this allows interrupt
	 * code to be guaranteed space.
	 */
	while (cnt.v_free_count < cnt.v_free_reserved) {
		static int cache_rover = 0;
		m = vm_page_list_find(PQ_CACHE, cache_rover);
		if (!m)
			break;
		cache_rover = (cache_rover + PQ_PRIME2) & PQ_L2_MASK;
		vm_page_free(m);
		cnt.v_dfree++;
	}
	splx(s);

	/*
	 * If we didn't get enough free pages, and we have skipped a vnode
	 * in a writeable object, wakeup the sync daemon.  And kick swapout
	 * if we did not get enough free pages.
	 */
	if ((cnt.v_cache_count + cnt.v_free_count) <
		(cnt.v_free_target + cnt.v_cache_min) ) {
		if (vnodes_skipped &&
		    (cnt.v_cache_count + cnt.v_free_count) < cnt.v_free_min) {
			if (!vfs_update_wakeup) {
				vfs_update_wakeup = 1;
				wakeup(&vfs_update_wakeup);
			}
		}
#if !defined(NO_SWAPPING)
		if (vm_swapping_enabled &&
			(cnt.v_free_count + cnt.v_cache_count < cnt.v_free_target)) {
			vm_req_vmdaemon();
			vm_pageout_req_swapout = 1;
		}
#endif
	}


	/*
	 * make sure that we have swap space -- if we are low on memory and
	 * swap -- then kill the biggest process.
	 */
	if ((vm_swap_size == 0 || swap_pager_full) &&
	    ((cnt.v_free_count + cnt.v_cache_count) < cnt.v_free_min)) {
		bigproc = NULL;
		bigsize = 0;
		for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
			/*
			 * if this is a system process, skip it
			 */
			if ((p->p_flag & P_SYSTEM) || (p->p_pid == 1) ||
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
			size = p->p_vmspace->vm_pmap.pm_stats.resident_count;
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
	cnt.v_free_min += cnt.v_free_reserved;
	return 1;
}


#ifdef unused
int
vm_pageout_free_pages(object, add)
vm_object_t object;
int add;
{
	return vm_pageout_free_page_calc(object->size);
}
#endif

/*
 *	vm_pageout is the high level pageout daemon.
 */
static void
vm_pageout()
{
	(void) spl0();

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
	cnt.v_free_target = 3 * cnt.v_free_min + cnt.v_free_reserved;

	if (cnt.v_free_count > 1024) {
		cnt.v_cache_max = (cnt.v_free_count - 1024) / 2;
		cnt.v_cache_min = (cnt.v_free_count - 1024) / 8;
		cnt.v_inactive_target = 2*cnt.v_cache_min + 192;
	} else {
		cnt.v_cache_min = 0;
		cnt.v_cache_max = 0;
		cnt.v_inactive_target = cnt.v_free_count / 4;
	}

	/* XXX does not really belong here */
	if (vm_page_max_wired == 0)
		vm_page_max_wired = cnt.v_free_count / 3;


	swap_pager_swap_init();
	/*
	 * The pageout daemon is never done, so loop forever.
	 */
	while (TRUE) {
		int inactive_target;
		int s = splvm();
		if (!vm_pages_needed ||
			((cnt.v_free_count + cnt.v_cache_count) > cnt.v_free_min)) {
			vm_pages_needed = 0;
			tsleep(&vm_pages_needed, PVM, "psleep", 0);
		} else if (!vm_pages_needed) {
			tsleep(&vm_pages_needed, PVM, "psleep", hz/10);
		}
		inactive_target =
			(cnt.v_page_count - cnt.v_wire_count) / 4;
		if (inactive_target < 2*cnt.v_free_min)
			inactive_target = 2*cnt.v_free_min;
		cnt.v_inactive_target = inactive_target;
		if (vm_pages_needed)
			cnt.v_pdwakeups++;
		vm_pages_needed = 0;
		splx(s);
		vm_pager_sync();
		vm_pageout_scan();
		vm_pager_sync();
		wakeup(&cnt.v_free_count);
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
	vm_object_t object;
	struct proc *p;

	(void) spl0();

	while (TRUE) {
		tsleep(&vm_daemon_needed, PUSER, "psleep", 0);
		if (vm_pageout_req_swapout) {
			swapout_procs();
			vm_pageout_req_swapout = 0;
		}
		/*
		 * scan the processes for exceeding their rlimits or if
		 * process is swapped out -- deactivate pages
		 */

		for (p = allproc.lh_first; p != 0; p = p->p_list.le_next) {
			quad_t limit;
			vm_offset_t size;

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
			limit = qmin(p->p_rlimit[RLIMIT_RSS].rlim_cur,
			    p->p_rlimit[RLIMIT_RSS].rlim_max);

			/*
			 * let processes that are swapped out really be
			 * swapped out set the limit to nothing (will force a
			 * swap-out.)
			 */
			if ((p->p_flag & P_INMEM) == 0)
				limit = 0;	/* XXX */

			size = p->p_vmspace->vm_pmap.pm_stats.resident_count * PAGE_SIZE;
			if (limit >= 0 && size >= limit) {
				vm_pageout_map_deactivate_pages(&p->p_vmspace->vm_map,
				    (vm_pindex_t)(limit >> PAGE_SHIFT) );
			}
		}

		/*
		 * we remove cached objects that have no RSS...
		 */
restart:
		object = TAILQ_FIRST(&vm_object_cached_list);
		while (object) {
			/*
			 * if there are no resident pages -- get rid of the object
			 */
			if (object->resident_page_count == 0) {
				vm_object_reference(object);
				pager_cache(object, FALSE);
				goto restart;
			}
			object = TAILQ_NEXT(object, cached_list);
		}
	}
}
#endif
