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
 * $Id: vm_pageout.c,v 1.26 1994/11/17 06:24:25 davidg Exp $
 */

/*
 *	The proverbial page-out daemon.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/swap_pager.h>

extern vm_map_t kmem_map;
int	vm_pages_needed;		/* Event on which pageout daemon sleeps */
int	vm_pagescanner;			/* Event on which pagescanner sleeps */
int	vm_pageout_free_min = 0;	/* Stop pageout to wait for pagers at this free level */

int	vm_pageout_pages_needed = 0;	/* flag saying that the pageout daemon needs pages */
int	vm_page_pagesfreed;
int	vm_desired_cache_size;

extern int npendingio;
extern int hz;
int	vm_pageout_proc_limit;
int	vm_pageout_req_swapout;
int vm_daemon_needed;
extern int nswiodone;
extern int swap_pager_full;
extern int vm_swap_size;
extern int swap_pager_ready();

#define MAXREF 32767

#define MAXSCAN 512	/* maximum number of pages to scan in active queue */
			/* set the "clock" hands to be (MAXSCAN * 4096) Bytes */
#define ACT_DECLINE	1
#define ACT_ADVANCE	3
#define ACT_MAX		100

#define LOWATER ((2048*1024)/NBPG)

#define VM_PAGEOUT_PAGE_COUNT 8
int vm_pageout_page_count = VM_PAGEOUT_PAGE_COUNT;
int vm_pageout_req_do_stats;

int	vm_page_max_wired = 0;	/* XXX max # of wired pages system-wide */


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
		cnt.v_free_count < vm_pageout_free_min)
		return 0;

	if( !sync) {
		if (object->shadow) {
			vm_object_collapse(object);
		}

		if ((m->busy != 0) ||
			(m->flags & PG_BUSY) || (m->hold_count != 0)) {
			return 0;
		} 
	}

	pageout_count = 1;
	ms[0] = m;

	pager = object->pager;
	if (pager) {
		for (i = 1; i < vm_pageout_page_count; i++) {
			ms[i] = vm_page_lookup(object, offset+i*NBPG);
			if (ms[i]) {
				if (((ms[i]->flags & PG_CLEAN) != 0) &&
				    pmap_is_modified(VM_PAGE_TO_PHYS(ms[i]))) {
					ms[i]->flags &= ~PG_CLEAN;
				}
				if (( ((ms[i]->flags & (PG_CLEAN|PG_INACTIVE|PG_BUSY)) == PG_INACTIVE)
					|| ( (ms[i]->flags & (PG_CLEAN|PG_BUSY)) == 0 && sync == VM_PAGEOUT_FORCE))
					&& (ms[i]->wire_count == 0)
					&& (ms[i]->busy == 0)
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
	} else {

		m->flags |= PG_BUSY;

		pmap_page_protect(VM_PAGE_TO_PHYS(m), VM_PROT_READ);

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
		cnt.v_free_count >= vm_pageout_free_min) {
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
				vm_pager_put_pages(pager, ms, pageout_count,
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
		case VM_PAGER_ERROR:
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
		case VM_PAGER_AGAIN:
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
			if ((ms[i]->flags & PG_REFERENCED) ||
				pmap_is_referenced(VM_PAGE_TO_PHYS(ms[i]))) {
				pmap_clear_reference(VM_PAGE_TO_PHYS(ms[i]));
				ms[i]->flags &= ~PG_REFERENCED;
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
	int dcount;

	dcount = 0;
	if (count == 0)
		count = 1;

	if (object->shadow) {
		if( object->shadow->ref_count == 1)
			dcount += vm_pageout_object_deactivate_pages(map, object->shadow, count/2);
	}

	if (object->paging_in_progress)
		return dcount;

	/*
	 * scan the objects entire memory queue
	 */
	rcount = object->resident_page_count;
	p = object->memq.tqh_first;
	while (p && (rcount-- > 0)) {
		next = p->listq.tqe_next;
		cnt.v_pdpages++;
		vm_page_lock_queues();
		/*
		 * if a page is active, not wired and is in the processes pmap,
		 * then deactivate the page.
		 */
		if ((p->flags & (PG_ACTIVE|PG_BUSY)) == PG_ACTIVE &&
			p->wire_count == 0 &&
			p->hold_count == 0 &&
			p->busy == 0 &&
			pmap_page_exists(vm_map_pmap(map), VM_PAGE_TO_PHYS(p))) {
			if (!pmap_is_referenced(VM_PAGE_TO_PHYS(p)) &&
				(p->flags & PG_REFERENCED) == 0) {
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
					TAILQ_REMOVE(&vm_page_queue_active, p, pageq);
					TAILQ_INSERT_TAIL(&vm_page_queue_active, p, pageq);
					TAILQ_REMOVE(&object->memq, p, listq);
					TAILQ_INSERT_TAIL(&object->memq, p, listq);
				}
				/*
				 * see if we are done yet
				 */
				if (p->flags & PG_INACTIVE) {
					--count;
					++dcount;
					if (count <= 0 &&
						cnt.v_inactive_count > cnt.v_inactive_target) {
							vm_page_unlock_queues();
							return dcount;
					}
				}
				
			} else {
				/*
				 * Move the page to the bottom of the queue.
				 */
				pmap_clear_reference(VM_PAGE_TO_PHYS(p));
				p->flags &= ~PG_REFERENCED;
				if (p->act_count < ACT_MAX)
					p->act_count += ACT_ADVANCE;

				TAILQ_REMOVE(&vm_page_queue_active, p, pageq);
				TAILQ_INSERT_TAIL(&vm_page_queue_active, p, pageq);
				TAILQ_REMOVE(&object->memq, p, listq);
				TAILQ_INSERT_TAIL(&object->memq, p, listq);
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
	} else if ((obj = entry->object.vm_object) != 0) {
		*count -= (*freeer)(map, obj, *count);
	}
	lock_read_done(&map->lock);
	vm_map_deallocate(map);
	return;
}

void
vm_req_vmdaemon() {
	extern	int ticks;
	static lastrun = 0;
	if( (ticks > (lastrun + hz/10)) || (ticks < lastrun)) {
		wakeup((caddr_t) &vm_daemon_needed);
		lastrun = ticks;
	}
}

/*
 *	vm_pageout_scan does the dirty work for the pageout daemon.
 */
int
vm_pageout_scan()
{
	vm_page_t	m;
	int		page_shortage, maxscan, maxlaunder;
	int		pages_freed;
	int		desired_free;
	vm_page_t	next;
	struct proc	*p, *bigproc;
	vm_offset_t size, bigsize;
	vm_object_t	object;
	int		force_wakeup = 0;
	int		cache_size, orig_cache_size;

	/* calculate the total cached size */

	if( cnt.v_inactive_count < cnt.v_inactive_target) {
		vm_req_vmdaemon();
	}

morefree:
	/*
	 * now swap processes out if we are in low memory conditions
	 */
	if ((cnt.v_free_count <= cnt.v_free_min) && !swap_pager_full && vm_swap_size&& vm_pageout_req_swapout == 0) {
		vm_pageout_req_swapout = 1;
		vm_req_vmdaemon();
	}

	pages_freed = 0;
	desired_free = cnt.v_free_target;

	/*
	 *	Start scanning the inactive queue for pages we can free.
	 *	We keep scanning until we have enough free pages or
	 *	we have scanned through the entire queue.  If we
	 *	encounter dirty pages, we start cleaning them.
	 */

	maxlaunder = 128;
	maxscan = cnt.v_inactive_count;
rescan1:
	m = vm_page_queue_inactive.tqh_first;
	while (m && (maxscan-- > 0) &&
		(cnt.v_free_count < desired_free) ) {
		vm_page_t	next;

		cnt.v_pdpages++;
		next = m->pageq.tqe_next;

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
		if (m->busy || (m->flags & PG_BUSY)) {
			m = next;
			continue;
		}

		if (((m->flags & PG_CLEAN) != 0) && pmap_is_modified(VM_PAGE_TO_PHYS(m))) {
			m->flags &= ~PG_CLEAN;
			m->flags |= PG_LAUNDRY;
		}

		if (((m->flags & PG_REFERENCED) == 0) && pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
			m->flags |= PG_REFERENCED;
			pmap_clear_reference(VM_PAGE_TO_PHYS(m));
		}

		if (m->flags & PG_CLEAN) {
			/*
			 * If we're not low on memory and the page has been reference,
			 * then reactivate the page.
			 */
			if ((cnt.v_free_count > vm_pageout_free_min) &&
			    ((m->flags & PG_REFERENCED) != 0)) {
				m->flags &= ~PG_REFERENCED;
				vm_page_activate(m);
			} else if (m->act_count == 0) {
				pmap_page_protect(VM_PAGE_TO_PHYS(m),
						  VM_PROT_NONE);
				vm_page_free(m);
				++cnt.v_dfree;
				++pages_freed;
			} else {
				m->act_count -= min(m->act_count, ACT_DECLINE);
				TAILQ_REMOVE(&vm_page_queue_inactive, m, pageq);
				TAILQ_INSERT_TAIL(&vm_page_queue_inactive, m, pageq);
			}
		} else if ((m->flags & PG_LAUNDRY) && maxlaunder > 0) {
			int written;
			if ((m->flags & PG_REFERENCED) != 0) {
				m->flags &= ~PG_REFERENCED;
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
			written = vm_pageout_clean(m,0);
			if (written) 
				maxlaunder -= written;

			if (!next)
				break;
			/*
			 * if the next page has been re-activated, start scanning again
			 */
			if ((written != 0) || ((next->flags & PG_INACTIVE) == 0))
				goto rescan1;
		} else if ((m->flags & PG_REFERENCED) != 0) {
			m->flags &= ~PG_REFERENCED;
			vm_page_activate(m);
		} 
		m = next;
	}

	/*
	 *	Compute the page shortage.  If we are still very low on memory
	 *	be sure that we will move a minimal amount of pages from active
	 *	to inactive.
	 */

	page_shortage = cnt.v_inactive_target - 
	    (cnt.v_free_count + cnt.v_inactive_count); 

	if (page_shortage <= 0) {
		if (pages_freed == 0) {
			if( cnt.v_free_count < cnt.v_free_min) {
				page_shortage = cnt.v_free_min - cnt.v_free_count + 1;
			} else if(((cnt.v_free_count + cnt.v_inactive_count) <
				(cnt.v_free_min + cnt.v_inactive_target))) {
				page_shortage = 1;
			} else {
				page_shortage = 0;
			}
		}
			
	}

	maxscan = cnt.v_active_count;
	m = vm_page_queue_active.tqh_first;
	while (m && maxscan-- && (page_shortage > 0)) {

		cnt.v_pdpages++;
		next = m->pageq.tqe_next;

		/*
 		 * Don't deactivate pages that are busy.
		 */
		if ((m->busy != 0) ||
			(m->flags & PG_BUSY) || (m->hold_count != 0)) {
			m = next;
			continue;
		}

		if ((m->flags & PG_REFERENCED) ||
			pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
			pmap_clear_reference(VM_PAGE_TO_PHYS(m));
			m->flags &= ~PG_REFERENCED;
			if (m->act_count < ACT_MAX)
				m->act_count += ACT_ADVANCE;
			TAILQ_REMOVE(&vm_page_queue_active, m, pageq);
			TAILQ_INSERT_TAIL(&vm_page_queue_active, m, pageq);
			TAILQ_REMOVE(&m->object->memq, m, listq);
			TAILQ_INSERT_TAIL(&m->object->memq, m, listq);
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
				TAILQ_REMOVE(&vm_page_queue_active, m, pageq);
				TAILQ_INSERT_TAIL(&vm_page_queue_active, m, pageq);
				TAILQ_REMOVE(&m->object->memq, m, listq);
				TAILQ_INSERT_TAIL(&m->object->memq, m, listq);
			}
		}
		m = next;
	}

	/*
	 * if we have not freed any pages and we are desparate for memory
	 * then we keep trying until we get some (any) memory.
	 */

	if (!force_wakeup && (swap_pager_full || !force_wakeup ||
	    (pages_freed == 0 && (cnt.v_free_count < cnt.v_free_min)))){
		vm_pager_sync();
		force_wakeup = 1;
		goto morefree;
	}

	/*
	 * make sure that we have swap space -- if we are low on
	 * memory and swap -- then kill the biggest process.
	 */
	if ((vm_swap_size == 0 || swap_pager_full) &&
	    (cnt.v_free_count < cnt.v_free_min)) {
		bigproc = NULL;
		bigsize = 0;
		for (p = (struct proc *)allproc; p != NULL; p = p->p_next) {
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
			printf("Process %lu killed by vm_pageout -- out of swap\n", (u_long)bigproc->p_pid);
			psignal(bigproc, SIGKILL);
			bigproc->p_estcpu = 0;
			bigproc->p_nice = PRIO_MIN;
			resetpriority(bigproc);
			wakeup( (caddr_t) &cnt.v_free_count);
		}
	}
	vm_page_pagesfreed += pages_freed;
	return force_wakeup;
}

/*
 *	vm_pageout is the high level pageout daemon.
 */
void
vm_pageout()
{
	(void) spl0();

	/*
	 *	Initialize some paging parameters.
	 */

	cnt.v_free_min = 12;
	/*
	 * free_reserved needs to include enough for the largest
	 * swap pager structures plus enough for any pv_entry
	 * structs when paging.
	 */
	vm_pageout_free_min = 4 + cnt.v_page_count / 1024;
	cnt.v_free_reserved = vm_pageout_free_min + 2;
	if (cnt.v_free_min < 8)
		cnt.v_free_min = 8;
	if (cnt.v_free_min > 32)
		cnt.v_free_min = 32;
	cnt.v_free_target = 2*cnt.v_free_min + cnt.v_free_reserved;
	cnt.v_inactive_target = cnt.v_free_count / 12;
	cnt.v_free_min += cnt.v_free_reserved;
	vm_desired_cache_size = cnt.v_page_count / 3;

        /* XXX does not really belong here */
	if (vm_page_max_wired == 0)
		vm_page_max_wired = cnt.v_free_count / 3;


	(void) swap_pager_alloc(0, 0, 0, 0);

	/*
	 *	The pageout daemon is never done, so loop
	 *	forever.
	 */
	while (TRUE) {
		int force_wakeup;
		
		tsleep((caddr_t) &vm_pages_needed, PVM, "psleep", 0);
		cnt.v_pdwakeups++;
	
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
			wakeup( (caddr_t) &cnt.v_free_count);
		wakeup((caddr_t) kmem_map);
	}
}

void
vm_daemon() {
	int cache_size;
	vm_object_t object;
	struct proc *p;
	while(TRUE) {
		tsleep((caddr_t) &vm_daemon_needed, PUSER, "psleep", 0);
		if( vm_pageout_req_swapout) {
		/*
		 * swap out inactive processes
		 */
			swapout_threads();
			vm_pageout_req_swapout = 0;
		}
	/*
	 * scan the processes for exceeding their rlimits or if process
	 * is swapped out -- deactivate pages 
	 */

		for (p = (struct proc *)allproc; p != NULL; p = p->p_next) {
			int overage;
			quad_t limit;
			vm_offset_t size;

			/*
			 * if this is a system process or if we have already
			 * looked at this process, skip it.
			 */
			if (p->p_flag & (P_SYSTEM|P_WEXIT)) {
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
		 * let processes that are swapped out really be swapped out
		 * set the limit to nothing (will force a swap-out.)
		 */
			if ((p->p_flag & P_INMEM) == 0)
				limit = 0;

			size = p->p_vmspace->vm_pmap.pm_stats.resident_count * NBPG;
			if (limit >= 0 && size >= limit) {
				overage = (size - limit) / NBPG;
				vm_pageout_map_deactivate_pages(&p->p_vmspace->vm_map,
					(vm_map_entry_t) 0, &overage, vm_pageout_object_deactivate_pages);
			}
		}

	/*
	 * We manage the cached memory by attempting to keep it
	 * at about the desired level.
	 * We deactivate the pages for the oldest cached objects
	 * first.  This keeps pages that are "cached" from hogging
	 * physical memory.
	 */
restart:
		cache_size = 0;
		object = vm_object_cached_list.tqh_first;
	/* calculate the total cached size */
		while( object) {
			cache_size += object->resident_page_count;
			object = object->cached_list.tqe_next;
		}

		vm_object_cache_lock();
		object = vm_object_cached_list.tqh_first;
		while ( object) {
			vm_object_cache_unlock();
		/*
		 * if there are no resident pages -- get rid of the object
		 */
			if( object->resident_page_count == 0) {
				if (object != vm_object_lookup(object->pager))
					panic("vm_object_cache_trim: I'm sooo confused.");
				pager_cache(object, FALSE);
				goto restart;
			} else if( cache_size >= (vm_swap_size?vm_desired_cache_size:0)) {
		/*
		 * if there are resident pages -- deactivate them
		 */
				vm_object_deactivate_pages(object);
				cache_size -= object->resident_page_count;
			}
			object = object->cached_list.tqe_next;
			vm_object_cache_lock();
		}
		vm_object_cache_unlock();
	}
}
