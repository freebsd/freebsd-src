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
 * $Id: vm_pageout.c,v 1.24 1994/06/17 13:29:15 davidg Exp $
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
int	vm_pagescanner;			/* Event on which pagescanner sleeps */
int	vm_pageout_free_min = 0;	/* Stop pageout to wait for pagers at this free level */

int	vm_pageout_pages_needed = 0;	/* flag saying that the pageout daemon needs pages */
int	vm_page_pagesfreed;
extern int vm_page_count;

extern int npendingio;
extern int hz;
int	vm_pageout_proc_limit;
extern int nswiodone;
extern int swap_pager_full;
extern int swap_pager_ready();

#define MAXREF 32767

#define MAXSCAN 512	/* maximum number of pages to scan in active queue */
			/* set the "clock" hands to be (MAXSCAN * 4096) Bytes */
#define ACT_DECLINE	1
#define ACT_ADVANCE	3
#define ACT_MAX		300

#define LOWATER ((2048*1024)/NBPG)

#define VM_PAGEOUT_PAGE_COUNT 8
int vm_pageout_page_count = VM_PAGEOUT_PAGE_COUNT;
static vm_offset_t vm_space_needed;
int vm_pageout_req_do_stats;
int vm_pageout_do_stats;


/*
 * vm_pageout_clean:
 * 	cleans a vm_page
 */
int
vm_pageout_clean(m, sync) 
	register vm_page_t m;
	int sync;
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
	int			pageout_status[VM_PAGEOUT_PAGE_COUNT];
	vm_page_t		ms[VM_PAGEOUT_PAGE_COUNT];
	int			pageout_count;
	int			anyok=0;
	int			i;
	vm_offset_t offset = m->offset;

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

	if (!object->pager &&
		object->shadow &&
		object->shadow->paging_in_progress)
		return 0;

	if( !sync) {
		if (object->shadow) {
			vm_object_collapse(object);
			if (!vm_page_lookup(object, offset))
				return 0;
		}

		if ((m->flags & PG_BUSY) || (m->hold_count != 0)) {
			return 0;
		} 
	}

	pageout_count = 1;
	ms[0] = m;

	if( pager = object->pager) {
		for(i=1;i<vm_pageout_page_count;i++) {
			if( ms[i] = vm_page_lookup( object, offset+i*NBPG)) {
				if( ((ms[i]->flags & (PG_CLEAN|PG_INACTIVE|PG_BUSY)) == PG_INACTIVE)
					&& (ms[i]->wire_count == 0)
					&& (ms[i]->hold_count == 0))
					pageout_count++;
				else
					break;
			} else
				break;
		}
		for(i=0;i<pageout_count;i++) {
			ms[i]->flags |= PG_BUSY;
			pmap_page_protect(VM_PAGE_TO_PHYS(ms[i]), VM_PROT_READ);
		}
		object->paging_in_progress += pageout_count;
		vm_stat.pageouts += pageout_count;
	} else {

		m->flags |= PG_BUSY;

		pmap_page_protect(VM_PAGE_TO_PHYS(m), VM_PROT_READ);

		vm_stat.pageouts++;

		object->paging_in_progress++;

		pager = vm_pager_allocate(PG_DFLT, (caddr_t)0,
			object->size, VM_PROT_ALL, 0);
		if (pager != NULL) {
			vm_object_setpager(object, pager, 0, FALSE);
		}
	}

	/*
	 *	If there is no pager for the page,
	 *	use the default pager.  If there's
	 *	no place to put the page at the
	 *	moment, leave it in the laundry and
	 *	hope that there will be paging space
	 *	later.
	 */

	if ((pager && pager->pg_type == PG_SWAP) || 
		vm_page_free_count >= vm_pageout_free_min) {
		if( pageout_count == 1) {
			pageout_status[0] = pager ?
				vm_pager_put(pager, m,
				    ((sync || (object == kernel_object)) ? TRUE: FALSE)) :
				VM_PAGER_FAIL;
		} else {
			if( !pager) {
				for(i=0;i<pageout_count;i++)
					pageout_status[i] = VM_PAGER_FAIL;
			} else {
				vm_pager_putmulti(pager, ms, pageout_count,
				    ((sync || (object == kernel_object)) ? TRUE : FALSE),
				    pageout_status);
			}
		}
			
	} else {
		for(i=0;i<pageout_count;i++)
			pageout_status[i] = VM_PAGER_FAIL;
	}

	for(i=0;i<pageout_count;i++) {
		switch (pageout_status[i]) {
		case VM_PAGER_OK:
			ms[i]->flags &= ~PG_LAUNDRY;
			++anyok;
			break;
		case VM_PAGER_PEND:
			ms[i]->flags &= ~PG_LAUNDRY;
			++anyok;
			break;
		case VM_PAGER_BAD:
			/*
			 * Page outside of range of object.
			 * Right now we essentially lose the
			 * changes by pretending it worked.
			 */
			ms[i]->flags &= ~PG_LAUNDRY;
			ms[i]->flags |= PG_CLEAN;
			pmap_clear_modify(VM_PAGE_TO_PHYS(ms[i]));
			break;
		case VM_PAGER_FAIL:
			/*
			 * If page couldn't be paged out, then
			 * reactivate the page so it doesn't
			 * clog the inactive list.  (We will
			 * try paging out it again later).
			 */
			if (ms[i]->flags & PG_INACTIVE)
				vm_page_activate(ms[i]);
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
		if (pageout_status[i] != VM_PAGER_PEND) {
			PAGE_WAKEUP(ms[i]);
			if (--object->paging_in_progress == 0)
				wakeup((caddr_t) object);
			if (pmap_is_referenced(VM_PAGE_TO_PHYS(ms[i]))) {
				pmap_clear_reference(VM_PAGE_TO_PHYS(ms[i]));
				if( ms[i]->flags & PG_INACTIVE)
					vm_page_activate(ms[i]);
			}
		}
	}
	return anyok;
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
		int scount = count;
		if( object->shadow->ref_count > 1)
			scount /= object->shadow->ref_count;
		if( scount)
			dcount += vm_pageout_object_deactivate_pages(map, object->shadow, scount);
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
			p->hold_count == 0 &&
			pmap_page_exists(vm_map_pmap(map), VM_PAGE_TO_PHYS(p))) {
			if (!pmap_is_referenced(VM_PAGE_TO_PHYS(p))) {
				p->act_count -= min(p->act_count, ACT_DECLINE);
				/*
				 * if the page act_count is zero -- then we deactivate
				 */
				if (!p->act_count) {
					vm_page_deactivate(p);
					pmap_page_protect(VM_PAGE_TO_PHYS(p),
						VM_PROT_NONE);
				/*
				 * else if on the next go-around we will deactivate the page
				 * we need to place the page on the end of the queue to age
				 * the other pages in memory.
				 */
				} else {
					queue_remove(&vm_page_queue_active, p, vm_page_t, pageq);
					queue_enter(&vm_page_queue_active, p, vm_page_t, pageq);
					queue_remove(&object->memq, p, vm_page_t, listq);
					queue_enter(&object->memq, p, vm_page_t, listq);
				}
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
				/*
				 * Move the page to the bottom of the queue.
				 */
				pmap_clear_reference(VM_PAGE_TO_PHYS(p));
				if (p->act_count < ACT_MAX)
					p->act_count += ACT_ADVANCE;

				queue_remove(&object->memq, p, vm_page_t, listq);
				queue_enter(&object->memq, p, vm_page_t, listq);
				queue_remove(&vm_page_queue_active, p, vm_page_t, pageq);
				queue_enter(&vm_page_queue_active, p, vm_page_t, pageq);
			}
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

/*
 *	vm_pageout_scan does the dirty work for the pageout daemon.
 */
int
vm_pageout_scan()
{
	vm_page_t	m;
	int		page_shortage, maxscan, maxlaunder;
	int		pages_freed, free, nproc;
	int		desired_free;
	vm_page_t	next;
	struct proc	*p;
	vm_object_t	object;
	int		s;
	int		force_wakeup = 0;

morefree:
	/*
	 * scan the processes for exceeding their rlimits or if process
	 * is swapped out -- deactivate pages 
	 */

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
		}

	}

	if (((vm_page_free_count + vm_page_inactive_count) >=
		(vm_page_inactive_target + vm_page_free_target)) &&
		(vm_page_free_count >= vm_page_free_target))
		return force_wakeup;

	pages_freed = 0;
	desired_free = vm_page_free_target;

	/*
	 *	Start scanning the inactive queue for pages we can free.
	 *	We keep scanning until we have enough free pages or
	 *	we have scanned through the entire queue.  If we
	 *	encounter dirty pages, we start cleaning them.
	 */

	maxlaunder = (vm_page_free_target - vm_page_free_count);
	maxscan = vm_page_inactive_count;
rescan1:
	m = (vm_page_t) queue_first(&vm_page_queue_inactive);
	while (maxscan-- > 0) {
		vm_page_t	next;

		if (queue_end(&vm_page_queue_inactive, (queue_entry_t) m)
			|| (vm_page_free_count >= desired_free)) {
			break;
		}

		next = (vm_page_t) queue_next(&m->pageq);

		if( (m->flags & PG_INACTIVE) == 0) {
			printf("vm_pageout_scan: page not inactive?");
			continue;
		}

		/*
		 * activate held pages
		 */
		if (m->hold_count != 0) {
			vm_page_activate(m);
			m = next;
			continue;
		}

		/*
		 * dont mess with busy pages
		 */
		if (m->flags & PG_BUSY) {
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
			if ((vm_page_free_count > vm_pageout_free_min)			/* XXX */
				&& pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
				vm_page_activate(m);
			} else if (!m->act_count) {
				pmap_page_protect(VM_PAGE_TO_PHYS(m),
						  VM_PROT_NONE);
				vm_page_free(m);
				++pages_freed;
			} else {
				m->act_count -= min(m->act_count, ACT_DECLINE);
			}
		} else if ((m->flags & PG_LAUNDRY) && maxlaunder > 0) {
			int written;
			if (pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
				pmap_clear_reference(VM_PAGE_TO_PHYS(m));
				vm_page_activate(m);
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

			if (written = vm_pageout_clean(m,0)) {
				maxlaunder -= written;
			}
			/*
			 * if the next page has been re-activated, start scanning again
			 */
			if (!next || (next->flags & PG_INACTIVE) == 0)
				goto rescan1;
		} else if (pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
			pmap_clear_reference(VM_PAGE_TO_PHYS(m));
			vm_page_activate(m);
		} 
		m = next;
	}

	/*
	 * now check malloc area or swap processes out if we are in low
	 * memory conditions
	 */
	if (vm_page_free_count < vm_page_free_min) {
		/*
		 * swap out inactive processes
		 */
		swapout_threads();
	}

	/*
	 *	Compute the page shortage.  If we are still very low on memory
	 *	be sure that we will move a minimal amount of pages from active
	 *	to inactive.
	 */

	page_shortage = vm_page_inactive_target -
	    (vm_page_free_count + vm_page_inactive_count);

	if (page_shortage <= 0) {
		if (pages_freed == 0) {
			if( vm_page_free_count < vm_page_free_min) {
				page_shortage = vm_page_free_min - vm_page_free_count;
			} else if(((vm_page_free_count + vm_page_inactive_count) <
				(vm_page_free_min + vm_page_inactive_target))) {
				page_shortage = 1;
			} else {
				page_shortage = 0;
			}
		}
			
	}

	m = (vm_page_t) queue_first(&vm_page_queue_active);
	maxscan = vm_page_active_count;
	while (maxscan-- && (page_shortage > 0)) {

		if (queue_end(&vm_page_queue_active, (queue_entry_t) m)) {
			break;
		}

		next = (vm_page_t) queue_next(&m->pageq);

		/*
 		 * Don't deactivate pages that are busy.
		 */
		if ((m->flags & PG_BUSY) || (m->hold_count != 0)) {
			m = next;
			continue;
		}

		if (pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
			pmap_clear_reference(VM_PAGE_TO_PHYS(m));
			if (m->act_count < ACT_MAX)
				m->act_count += ACT_ADVANCE;
			queue_remove(&vm_page_queue_active, m, vm_page_t, pageq);
			queue_enter(&vm_page_queue_active, m, vm_page_t, pageq);
			queue_remove(&m->object->memq, m, vm_page_t, listq);
			queue_enter(&m->object->memq, m, vm_page_t, listq);
		} else {
			m->act_count -= min(m->act_count, ACT_DECLINE);

			/*
			 * if the page act_count is zero -- then we deactivate
			 */
			if (!m->act_count) {
				vm_page_deactivate(m);
				--page_shortage;
			/*
			 * else if on the next go-around we will deactivate the page
			 * we need to place the page on the end of the queue to age
			 * the other pages in memory.
			 */
			} else {
				queue_remove(&vm_page_queue_active, m, vm_page_t, pageq);
				queue_enter(&vm_page_queue_active, m, vm_page_t, pageq);
				queue_remove(&m->object->memq, m, vm_page_t, listq);
				queue_enter(&m->object->memq, m, vm_page_t, listq);
			}
		}

		m = next;
	}

	/*
	 * if we have not freed any pages and we are desparate for memory
	 * then we keep trying until we get some (any) memory.
	 */

	if( !force_wakeup && (swap_pager_full || !force_wakeup ||
		(pages_freed == 0 && (vm_page_free_count < vm_page_free_min)))){
		vm_pager_sync();
		force_wakeup = 1;
		goto morefree;
	}
	vm_page_pagesfreed += pages_freed;
	return force_wakeup;
}

void
vm_pagescan()
{
	int maxscan, pages_scanned, pages_referenced, nextscan, scantick = hz/20;
	int m_ref, next_ref;
	vm_page_t m, next;

	(void) splnone();

	nextscan = scantick;

scanloop:

	pages_scanned = 0;
	pages_referenced = 0;
	maxscan = min(vm_page_active_count, MAXSCAN);

	/*
	 * Gather statistics on page usage.
	 */
	m = (vm_page_t) queue_first(&vm_page_queue_active);
	while (maxscan-- > 0) {

		if (queue_end(&vm_page_queue_active, (queue_entry_t) m)) {
			break;
		}

		++pages_scanned;

		next = (vm_page_t) queue_next(&m->pageq);

		/*
 		 * Dont mess with pages that are busy.
		 */
		if ((m->flags & PG_BUSY) || (m->hold_count != 0)) {
			m = next;
			continue;
		}

		/*
		 * Advance pages that have been referenced, decline pages that
		 * have not.
		 */
		if (pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
			pmap_clear_reference(VM_PAGE_TO_PHYS(m));
			pages_referenced++;
			if (m->act_count < ACT_MAX)
				m->act_count += ACT_ADVANCE;
			queue_remove(&vm_page_queue_active, m, vm_page_t, pageq);
			queue_enter(&vm_page_queue_active, m, vm_page_t, pageq);
			queue_remove(&m->object->memq, m, vm_page_t, listq);
			queue_enter(&m->object->memq, m, vm_page_t, listq);
		} else {
			m->act_count -= min(m->act_count, ACT_DECLINE);
			/*
			 * if the page act_count is zero, and we are low on mem -- then we deactivate
			 */
			if (!m->act_count &&
			    (vm_page_free_count+vm_page_inactive_count < vm_page_free_target+vm_page_inactive_target )) {
				vm_page_deactivate(m);
			/*
			 * else if on the next go-around we will deactivate the page
			 * we need to place the page on the end of the queue to age
			 * the other pages in memory.
			 */
			} else {
				queue_remove(&vm_page_queue_active, m, vm_page_t, pageq);
				queue_enter(&vm_page_queue_active, m, vm_page_t, pageq);
				queue_remove(&m->object->memq, m, vm_page_t, listq);
				queue_enter(&m->object->memq, m, vm_page_t, listq);
			}
		}
		m = next;
	}

	if (pages_referenced) {
		nextscan = (pages_scanned / pages_referenced) * scantick;
		nextscan = max(nextscan, scantick);
		nextscan = min(nextscan, hz);
	} else
		nextscan = hz;
	tsleep((caddr_t) &vm_pagescanner, PVM, "scanw", nextscan);

	goto scanloop;
}

/*
 *	vm_pageout is the high level pageout daemon.
 */
void
vm_pageout()
{
	extern npendingio, swiopend;
	extern int vm_page_count;
	static nowakeup;
	(void) spl0();

	/*
	 *	Initialize some paging parameters.
	 */

vmretry:
	vm_page_free_min = 12;
	vm_page_free_reserved = 8;
	if (vm_page_free_min < 8)
		vm_page_free_min = 8;
	if (vm_page_free_min > 32)
		vm_page_free_min = 32;
	vm_pageout_free_min = 4;
	vm_page_free_target = 2*vm_page_free_min + vm_page_free_reserved;
	vm_page_inactive_target = vm_page_free_count / 12;
	vm_page_free_min += vm_page_free_reserved;

	(void) swap_pager_alloc(0, 0, 0, 0);

	/*
	 *	The pageout daemon is never done, so loop
	 *	forever.
	 */
	while (TRUE) {
		int force_wakeup;
		
		tsleep((caddr_t) &vm_pages_needed, PVM, "psleep", 0);
	
		vm_pager_sync();
		/*
		 * The force wakeup hack added to eliminate delays and potiential
		 * deadlock.  It was possible for the page daemon to indefintely
		 * postpone waking up a process that it might be waiting for memory
		 * on.  The putmulti stuff seems to have aggravated the situation.
		 */
		force_wakeup = vm_pageout_scan();
		vm_pager_sync();
		if( force_wakeup)
			wakeup( (caddr_t) &vm_page_free_count);
		vm_pageout_do_stats = 0;
		cnt.v_scan++;
		wakeup((caddr_t) kmem_map);
	}
}

