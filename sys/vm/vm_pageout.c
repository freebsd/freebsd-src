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
 *	@(#)vm_pageout.c	7.4 (Berkeley) 5/7/91
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
 * $Id: vm_pageout.c,v 1.13 1994/02/10 08:08:37 davidg Exp $
 */

/*
 *	The proverbial page-out daemon.
 */

#include "param.h"

#include "vm.h"
#include "vm_page.h"
#include "vm_pageout.h"
#include "malloc.h"
#include "proc.h"
#include "resource.h"
#include "resourcevar.h"
#include "vmmeter.h"

extern vm_map_t kmem_map;
int	vm_pages_needed;		/* Event on which pageout daemon sleeps */
int	vm_pageout_free_min = 0;	/* Stop pageout to wait for pagers at this free level */

int	vm_pageout_pages_needed = 0;	/* flag saying that the pageout daemon needs pages */
int	vm_page_pagesfreed;

extern int npendingio;
extern int hz;
int	vm_pageout_proc_limit;
extern int nswiodone;

#define MAXREF 32767
#define DEACT_MAX (DEACT_START * 4)
#define MINSCAN 512	/* minimum number of pages to scan in active queue */
			/* set the "clock" hands to be (MINSCAN * 4096) Bytes */
static int minscan;
void vm_pageout_deact_bump(vm_page_t m) ;


/*
 * vm_pageout_clean:
 * 	cleans a vm_page
 */
int
vm_pageout_clean(m, wait) 
	register vm_page_t m;
	int wait;
{
	/*
	 *	Clean the page and remove it from the
	 *	laundry.
	 *
	 *	We set the busy bit to cause
	 *	potential page faults on this page to
	 *	block.
	 *
	 *	And we set pageout-in-progress to keep
	 *	the object from disappearing during
	 *	pageout.  This guarantees that the
	 *	page won't move from the inactive
	 *	queue.  (However, any other page on
	 *	the inactive queue may move!)
	 */

	register vm_object_t	object;
	register vm_pager_t	pager;
	int			pageout_status;

	object = m->object;
	if (!object) {
		printf("pager: object missing\n");
		return 0;
	}

	/*
	 *	Try to collapse the object before
	 *	making a pager for it.  We must
	 *	unlock the page queues first.
	 *	We try to defer the creation of a pager
	 *	until all shadows are not paging.  This
	 *	allows vm_object_collapse to work better and
	 *	helps control swap space size.
	 *	(J. Dyson 11 Nov 93)
	 */

	if (!object->pager &&
		vm_page_free_count < vm_pageout_free_min)
		return 0;

collapseagain:
	if (!object->pager &&
		object->shadow &&
		object->shadow->paging_in_progress)
		return 0;

	if (object->shadow) {
		vm_offset_t offset = m->offset;
		vm_object_collapse(object);
		if (!vm_page_lookup(object, offset))
			return 0;
	}

waitagain:
	if (!wait && (m->flags & PG_BUSY)) {
		return 0;
	} else if (m->flags & PG_BUSY) {
		int s = splhigh();
		m->flags |= PG_WANTED;
		tsleep((caddr_t)m, PVM, "clnslp", 0);
		splx(s);
		goto waitagain;
	}

	m->flags |= PG_BUSY;

	pmap_page_protect(VM_PAGE_TO_PHYS(m), VM_PROT_READ);

	vm_stat.pageouts++;

	object->paging_in_progress++;

	/*
	 *	If there is no pager for the page,
	 *	use the default pager.  If there's
	 *	no place to put the page at the
	 *	moment, leave it in the laundry and
	 *	hope that there will be paging space
	 *	later.
	 */

	if ((pager = object->pager) == NULL) {
		pager = vm_pager_allocate(PG_DFLT, (caddr_t)0,
			object->size, VM_PROT_ALL, 0);
		if (pager != NULL) {
			vm_object_setpager(object, pager, 0, FALSE);
		}
	}
	if ((pager && pager->pg_type == PG_SWAP) || 
		vm_page_free_count >= vm_pageout_free_min) {
		pageout_status = pager ?
			vm_pager_put(pager, m, (((object == kernel_object) || wait) ? TRUE: FALSE)) :
			VM_PAGER_FAIL;
	} else
		pageout_status = VM_PAGER_FAIL;

	switch (pageout_status) {
	case VM_PAGER_OK:
		m->flags &= ~PG_LAUNDRY;
		break;
	case VM_PAGER_PEND:
		m->flags &= ~PG_LAUNDRY;
		break;
	case VM_PAGER_BAD:
		/*
		 * Page outside of range of object.
		 * Right now we essentially lose the
		 * changes by pretending it worked.
		 */
		m->flags &= ~PG_LAUNDRY;
		m->flags |= PG_CLEAN;
		pmap_clear_modify(VM_PAGE_TO_PHYS(m));
		break;
	case VM_PAGER_FAIL:
		/*
		 * If page couldn't be paged out, then
		 * reactivate the page so it doesn't
		 * clog the inactive list.  (We will
		 * try paging out it again later).
		 */
		if ((m->flags & PG_ACTIVE) == 0)
			vm_page_activate(m);
		break;
	case VM_PAGER_TRYAGAIN:
		break;
	}


	/*
	 * If the operation is still going, leave
	 * the page busy to block all other accesses.
	 * Also, leave the paging in progress
	 * indicator set so that we don't attempt an
	 * object collapse.
	 */
	if (pageout_status != VM_PAGER_PEND) {
		if ((m->flags & PG_ACTIVE) == 0 &&
			pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
			vm_page_activate(m);
		}
		PAGE_WAKEUP(m);
		if (--object->paging_in_progress == 0)
			wakeup((caddr_t) object);
	}
	return (pageout_status == VM_PAGER_PEND ||
		pageout_status == VM_PAGER_OK) ? 1 : 0;
}

int
vm_fault_object_deactivate_pages(map, object, dummy)
	vm_map_t map;
	vm_object_t object;
	int dummy;
{
	register vm_page_t	p, next;
	int rcount;
	int s;
	int dcount;
	int count;

	dcount = 0;
	/*
	 * deactivate the pages in the objects shadow
	 */

	if (object->shadow)
		dcount += vm_fault_object_deactivate_pages(map, object->shadow, 0);

	/*
	 * scan the objects memory queue and remove 20% of the active pages
	 */
	rcount = object->resident_page_count;
	count = rcount;
	if (count == 0)
		return dcount;
#define MINOBJWRITE 10
#define OBJDIVISOR 5
	if (count > MINOBJWRITE) {
		count = MINOBJWRITE + ((count - MINOBJWRITE) / OBJDIVISOR);
	}
	p = (vm_page_t) queue_first(&object->memq);
	while ((rcount-- > 0) && !queue_end(&object->memq, (queue_entry_t) p) ) {
		next = (vm_page_t) queue_next(&p->listq);
		vm_page_lock_queues();
		/*
		 * if a page is active, not wired and is in the processes pmap,
		 * then deactivate the page.
		 */
		if ((p->flags & (PG_ACTIVE|PG_BUSY)) == PG_ACTIVE &&
			p->wire_count == 0 &&
			pmap_page_exists(vm_map_pmap(map), VM_PAGE_TO_PHYS(p))) {
			if (!pmap_is_referenced(VM_PAGE_TO_PHYS(p))) {
				vm_page_deactivate(p);
				if ((p->flags & PG_CLEAN) == 0) {
					vm_pageout_clean(p, 0);
				}
				++dcount;
				if (--count <= 0) {
					vm_page_unlock_queues();
					s = splbio();
					while (object->paging_in_progress) {
						tsleep((caddr_t) object,PVM,"vmfobw",0);
					}
					splx(s);
					return dcount;
				}
			} else {
				vm_pageout_deact_bump(p);
				pmap_clear_reference(VM_PAGE_TO_PHYS(p));
				queue_remove(&object->memq, p, vm_page_t, listq);
				queue_enter(&object->memq, p, vm_page_t, listq);
				queue_remove(&vm_page_queue_active, p, vm_page_t, pageq);
				queue_enter(&vm_page_queue_active, p, vm_page_t, pageq);
			}
		/*
		 * if a page is inactive and has been modified, clean it now
		 */
		} else if ((p->flags & (PG_INACTIVE|PG_BUSY)) == PG_INACTIVE) {
			if ((p->flags & PG_CLEAN) &&
				pmap_is_modified(VM_PAGE_TO_PHYS(p)))
				p->flags &= ~PG_CLEAN;

			if ((p->flags & PG_CLEAN) == 0)
				vm_pageout_clean(p, 0);
		}

		vm_page_unlock_queues();
		p = next;
	}
	s = splbio();
	while (object->paging_in_progress) {
		tsleep((caddr_t)object,PVM,"vmfobw",0);
	}
	splx(s);
	return dcount;
}

/*
 *	vm_pageout_object_deactivate_pages
 *
 *	deactivate enough pages to satisfy the inactive target
 *	requirements or if vm_page_proc_limit is set, then
 *	deactivate all of the pages in the object and its
 *	shadows.
 *
 *	The object and map must be locked.
 */
int
vm_pageout_object_deactivate_pages(map, object, count)
	vm_map_t map;
	vm_object_t object;
	int count;
{
	register vm_page_t	p, next;
	int rcount;
	int s;
	int dcount;

	dcount = 0;
	if (count == 0)
		count = 1;

	if (object->shadow) {
		dcount += vm_pageout_object_deactivate_pages(map, object->shadow, count);
	}

	if (object->paging_in_progress)
		return dcount;

	/*
	 * scan the objects entire memory queue
	 */
	rcount = object->resident_page_count;
	p = (vm_page_t) queue_first(&object->memq);
	while ((rcount-- > 0) && !queue_end(&object->memq, (queue_entry_t) p) ) {
		next = (vm_page_t) queue_next(&p->listq);
		vm_page_lock_queues();
		/*
		 * if a page is active, not wired and is in the processes pmap,
		 * then deactivate the page.
		 */
		if ((p->flags & (PG_ACTIVE|PG_BUSY)) == PG_ACTIVE &&
			p->wire_count == 0 &&
			pmap_page_exists(vm_map_pmap(map), VM_PAGE_TO_PHYS(p))) {
			if (!pmap_is_referenced(VM_PAGE_TO_PHYS(p))) {
				if (object->ref_count <= 1)
					vm_page_deactivate(p);
				else
					vm_page_pageout_deactivate(p);
				if (((p->flags & PG_INACTIVE)) &&
					(p->flags & PG_CLEAN) == 0)
					vm_pageout_clean(p, 0);
				/*
				 * see if we are done yet
				 */
				if (p->flags & PG_INACTIVE) {
					--count;
					++dcount;
					if (count <= 0 &&
						vm_page_inactive_count > vm_page_inactive_target) {
							vm_page_unlock_queues();
							return dcount;
					}
				}
				
			} else {
				vm_pageout_deact_bump(p);
				pmap_clear_reference(VM_PAGE_TO_PHYS(p));
				queue_remove(&object->memq, p, vm_page_t, listq);
				queue_enter(&object->memq, p, vm_page_t, listq);
				queue_remove(&vm_page_queue_active, p, vm_page_t, pageq);
				queue_enter(&vm_page_queue_active, p, vm_page_t, pageq);
			}
		/*
		 * if a page is inactive and has been modified, clean it now
		 */
		} else if ((p->flags & (PG_INACTIVE|PG_BUSY)) == PG_INACTIVE) {
			if ((p->flags & PG_CLEAN) &&
				pmap_is_modified(VM_PAGE_TO_PHYS(p)))
				p->flags &= ~PG_CLEAN;

			if ((p->flags & PG_CLEAN) == 0)
				vm_pageout_clean(p, 0);
		}

		vm_page_unlock_queues();
		p = next;
	}
	return dcount;
}


/*
 * deactivate some number of pages in a map, try to do it fairly, but
 * that is really hard to do.
 */

void
vm_pageout_map_deactivate_pages(map, entry, count, freeer)
	vm_map_t map;
	vm_map_entry_t entry;
	int *count;
	int (*freeer)(vm_map_t, vm_object_t, int);
{
	vm_map_t tmpm;
	vm_map_entry_t tmpe;
	vm_object_t obj;
	if (*count <= 0)
		return;
	vm_map_reference(map);
	if (!lock_try_read(&map->lock)) {
		vm_map_deallocate(map);
		return;
	}
	if (entry == 0) {
		tmpe = map->header.next;
		while (tmpe != &map->header && *count > 0) {
			vm_pageout_map_deactivate_pages(map, tmpe, count, freeer);
			tmpe = tmpe->next;
		};
	} else if (entry->is_sub_map || entry->is_a_map) {
		tmpm = entry->object.share_map;
		tmpe = tmpm->header.next;
		while (tmpe != &tmpm->header && *count > 0) {
			vm_pageout_map_deactivate_pages(tmpm, tmpe, count, freeer);
			tmpe = tmpe->next;
		};
	} else if (obj = entry->object.vm_object) {
		*count -= (*freeer)(map, obj, *count);
	}
	lock_read_done(&map->lock);
	vm_map_deallocate(map);
	return;
}

void
vm_fault_free_pages(p)
	struct proc *p;
{
	int overage = 1;
	vm_pageout_map_deactivate_pages(&p->p_vmspace->vm_map,
		(vm_map_entry_t) 0, &overage, vm_fault_object_deactivate_pages);
}

/*
 *	vm_pageout_scan does the dirty work for the pageout daemon.
 */
void
vm_pageout_scan()
{
	vm_page_t	m;
	int		page_shortage, maxscan, maxlaunder;
	int		pages_freed, free, nproc, nbusy;
	vm_page_t	next;
	struct proc	*p;
	vm_object_t	object;
	int		s;

	/*
	 * deactivate objects with ref_counts == 0
	 */
	object = (vm_object_t) queue_first(&vm_object_list);
	while (!queue_end(&vm_object_list, (queue_entry_t) object)) {
		if (object->ref_count == 0)
			vm_object_deactivate_pages(object);
		object = (vm_object_t) queue_next(&object->object_list);
	}

rerun:
#if 1
	/*
	 * next scan the processes for exceeding their rlimits or if process
	 * is swapped out -- deactivate pages 
	 */

rescanproc1a:
	for (p = allproc; p != NULL; p = p->p_nxt)
		p->p_flag &= ~SPAGEDAEMON;

rescanproc1:
	for (p = allproc; p != NULL; p = p->p_nxt) {
		vm_offset_t size;
		int overage;
		vm_offset_t limit;

		/*
		 * if this is a system process or if we have already
		 * looked at this process, skip it.
		 */
		if (p->p_flag & (SSYS|SPAGEDAEMON|SWEXIT)) {
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
		limit = min(p->p_rlimit[RLIMIT_RSS].rlim_cur,
			    p->p_rlimit[RLIMIT_RSS].rlim_max);
			
		/*
		 * let processes that are swapped out really be swapped out
		 * set the limit to nothing (will force a swap-out.)
		 */
		if ((p->p_flag & SLOAD) == 0)
			limit = 0;

		size = p->p_vmspace->vm_pmap.pm_stats.resident_count * NBPG;
		if (size >= limit) {
			overage = (size - limit) / NBPG;
			vm_pageout_map_deactivate_pages(&p->p_vmspace->vm_map,
				(vm_map_entry_t) 0, &overage, vm_pageout_object_deactivate_pages);
			p->p_flag |= SPAGEDAEMON;
			goto rescanproc1;
		}
		p->p_flag |= SPAGEDAEMON;
	}

#if 0
	if (((vm_page_free_count + vm_page_inactive_count) >=
		(vm_page_inactive_target + vm_page_free_target)) &&
		(vm_page_free_count >= vm_page_free_target))
		return;
#endif

#endif

	pages_freed = 0;

	/*
	 *	Start scanning the inactive queue for pages we can free.
	 *	We keep scanning until we have enough free pages or
	 *	we have scanned through the entire queue.  If we
	 *	encounter dirty pages, we start cleaning them.
	 */

	maxlaunder = (vm_page_free_target - vm_page_free_count);
rescan:
	m = (vm_page_t) queue_first(&vm_page_queue_inactive);
	maxscan = vm_page_inactive_count;
	while (maxscan-- > 0) {
		vm_page_t	next;


		if (queue_end(&vm_page_queue_inactive, (queue_entry_t) m)
			|| (vm_page_free_count >= vm_page_free_target)) {
			break;
		}

		next = (vm_page_t) queue_next(&m->pageq);

		/*
		 * dont mess with busy pages
		 */
		if (m->flags & PG_BUSY) {
			queue_remove(&vm_page_queue_inactive, m, vm_page_t, pageq);
			queue_enter(&vm_page_queue_inactive, m, vm_page_t, pageq);
			m = next;
			continue;
		}

		/*
		 * if page is clean and but the page has been referenced,
		 * then reactivate the page, but if we are very low on memory
		 * or the page has not been referenced, then we free it to the
		 * vm system.
		 */
		if (m->flags & PG_CLEAN) {
			if ((vm_page_free_count > vm_pageout_free_min)
				&& pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
				vm_page_activate(m);
				++vm_stat.reactivations;
				m = next;
				continue;
			}
			else {
				pmap_page_protect(VM_PAGE_TO_PHYS(m),
						  VM_PROT_NONE);
				vm_page_free(m);
				++pages_freed;
				m = next;
				continue;
			}
		} else if ((m->flags & PG_LAUNDRY) && maxlaunder > 0) {
			/*
			 * if a page has been used even if it is in the laundry,
			 * activate it.
			 */

			if (pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
				vm_page_activate(m);
				m->flags &= ~PG_LAUNDRY;
				m = next;
				continue;
			}

			/*
			 *	If a page is dirty, then it is either
			 *	being washed (but not yet cleaned)
			 *	or it is still in the laundry.  If it is
			 *	still in the laundry, then we start the
			 *	cleaning operation.
			 */

			if (vm_pageout_clean(m,0)) {
				--maxlaunder;
				/*
				 * if the next page has been re-activated, start scanning again
				 */
				if ((next->flags & PG_INACTIVE) == 0)
					goto rescan;
			}
		}  else if (pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
			vm_page_activate(m);
		}
		m = next;
	}

	/*
	 * now check malloc area or swap processes out if we are in low
	 * memory conditions
	 */
	free = vm_page_free_count;
	if (free <= vm_page_free_min) {
		/*
		 *	Be sure the pmap system is updated so
		 *	we can scan the inactive queue.
		 */
		pmap_update();

		/*
		 * swap out inactive processes
		 */
		swapout_threads();

#if 0
		/*
		 * see if malloc has anything for us
		 */
		if (free <= vm_page_free_reserved)
			malloc_gc(); 
#endif
	}

skipfree:
	/*
	 * If we did not free any pages, but we need to do so, we grow the
	 * inactive target.  But as we successfully free pages, then we
	 * shrink the inactive target.
	 */
	if (pages_freed == 0 && vm_page_free_count < vm_page_free_min) {
		vm_page_inactive_target += (vm_page_free_min - vm_page_free_count);
		if (vm_page_inactive_target > vm_page_free_target*5)
			vm_page_inactive_target = vm_page_free_target*5;
	} else if (pages_freed > 0) {
		vm_page_inactive_target -= vm_page_free_min/2;
		if (vm_page_inactive_target < vm_page_free_target*2)
			vm_page_inactive_target = vm_page_free_target*2;
	}

	/*
	 *	Compute the page shortage.  If we are still very low on memory
	 *	be sure that we will move a minimal amount of pages from active
	 *	to inactive.
	 */

restart_inactivate_all:

	page_shortage = vm_page_inactive_target - vm_page_inactive_count;
	page_shortage -= vm_page_free_count;  

	if (page_shortage <= 0) {
		if (pages_freed == 0 &&
			((vm_page_free_count + vm_page_inactive_count) <
				(vm_page_free_min + vm_page_inactive_target))) {
			page_shortage = 1;
		} else {
			page_shortage = 0;
		}
	}

	maxscan = vm_page_active_count;

	/*
	 * deactivate pages that are active, but have not been used
	 * for a while.
	 */
restart_inactivate:
	m = (vm_page_t) queue_first(&vm_page_queue_active);
	while (maxscan-- > 0) {

		if (page_shortage <= 0 &&
			maxscan < (vm_page_active_count - minscan) )
			break;
	
		if (queue_end(&vm_page_queue_active, (queue_entry_t) m)) {
			break;
		}

		next = (vm_page_t) queue_next(&m->pageq);

		/*
 		 * dont mess with pages that are busy
		 */
		if (m->flags & PG_BUSY) {
			m = next;
			continue;
		}

	/*
	 *	Move some more pages from active to inactive.
	 */

		/*
		 * see if there are any pages that are able to be deactivated
		 */
		/*
		 * the referenced bit is the one that say that the page
		 * has been used.
		 */
		if (!pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
			/*
			 * if the page has not been referenced, call the
			 * vm_page_pageout_deactivate routine.  It might
			 * not deactivate the page every time.  There is
			 * a policy associated with it.
			 */
			if (page_shortage > 0) {
				if (vm_page_pageout_deactivate(m)) {
					/*
					 * if the page was really deactivated, then
					 * decrement the page_shortage
					 */
					if ((m->flags & PG_ACTIVE) == 0) {
						--page_shortage;
					}
				}
			}
		} else {
			/*
			 * if the page was recently referenced, set our
			 * deactivate count and clear reference for a future
			 * check for deactivation.
			 */
			vm_pageout_deact_bump(m);
			if (page_shortage > 0 || m->deact >= (DEACT_MAX/2))
				pmap_clear_reference(VM_PAGE_TO_PHYS(m));
			queue_remove(&m->object->memq, m, vm_page_t, listq);
			queue_enter(&m->object->memq, m, vm_page_t, listq);
			queue_remove(&vm_page_queue_active, m, vm_page_t, pageq);
			queue_enter(&vm_page_queue_active, m, vm_page_t, pageq);
		}
		m = next;
	}

	vm_page_pagesfreed += pages_freed;
}

/*
 * this code maintains a dynamic reference count per page
 */
void
vm_pageout_deact_bump(vm_page_t m) {
	if( m->deact >= DEACT_START) {
		m->deact += 1;
		if( m->deact > DEACT_MAX)
			m->deact = DEACT_MAX;
	} else {
		m->deact += DEACT_START;
	}
}

/*
 * optionally do a deactivate if the deactivate has been done
 * enough to justify it.
 */
int
vm_page_pageout_deactivate(m)
	vm_page_t m;
{

	switch (m->deact) {
case DEACT_FREE:
		vm_page_deactivate(m);
		return 1;
case DEACT_CLEAN:
		break;
case DEACT_DELAY:
		vm_page_makefault(m);
case DEACT_START:
		break;
	}
	--m->deact;
	return 0;
}

/*
 *	vm_pageout is the high level pageout daemon.
 */

void
vm_pageout()
{
	extern npendingio, swiopend;
	extern int vm_page_count;
	(void) spl0();

	/*
	 *	Initialize some paging parameters.
	 */

vmretry:
	vm_page_free_min = npendingio/3;
#ifdef VSMALL
	vm_page_free_min = 8;
#endif
	vm_page_free_reserved = 8;
	if (vm_page_free_min < 8)
		vm_page_free_min = 8;
	if (vm_page_free_min > 32)
		vm_page_free_min = 32;
	vm_pageout_free_min = 3;
	vm_page_free_target = 2*vm_page_free_min + vm_page_free_reserved;
	vm_page_inactive_target = 3*vm_page_free_min + vm_page_free_reserved;
	vm_page_free_min += vm_page_free_reserved;
	minscan = MINSCAN;
	if (minscan > vm_page_count/3)
		minscan = vm_page_count/3;

	/*
	 *	The pageout daemon is never done, so loop
	 *	forever.
	 */

		
	while (TRUE) {
		
		splhigh();
		if (vm_page_free_count > vm_page_free_min) {
			wakeup((caddr_t) &vm_page_free_count);
			tsleep((caddr_t) &vm_pages_needed, PVM, "psleep", 0);
		} else {
			if (nswiodone) {
				spl0();
				goto dosync;
			}
			tsleep((caddr_t) &vm_pages_needed, PVM, "pslp1", 5);
		}
		spl0();

		vm_pager_sync();
		vm_pageout_scan();
	dosync:
		vm_pager_sync();
		cnt.v_scan++;
		wakeup((caddr_t) kmem_map);
	}
}
