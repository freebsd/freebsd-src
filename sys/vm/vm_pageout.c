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
 * $Id: vm_pageout.c,v 1.57 1995/10/07 19:02:55 davidg Exp $
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

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_kern.h>
#include <vm/vm_pager.h>
#include <vm/swap_pager.h>

/*
 * System initialization
 */

/* the kernel process "vm_pageout"*/
static void vm_pageout __P((void));
struct proc *pageproc;

static struct kproc_desc page_kp = {
	"pagedaemon",
	vm_pageout,
	&pageproc
};
SYSINIT_KT(pagedaemon, SI_SUB_KTHREAD_PAGE, SI_ORDER_FIRST, kproc_start, &page_kp)

/* the kernel process "vm_daemon"*/
static void vm_daemon __P((void));
struct	proc *vmproc;

static struct kproc_desc vm_kp = {
	"vmdaemon",
	vm_daemon,
	&vmproc
};
SYSINIT_KT(vmdaemon, SI_SUB_KTHREAD_VM, SI_ORDER_FIRST, kproc_start, &vm_kp)


int vm_pages_needed;		/* Event on which pageout daemon sleeps */

int vm_pageout_pages_needed;	/* flag saying that the pageout daemon needs pages */

extern int npendingio;
int vm_pageout_req_swapout;	/* XXX */
int vm_daemon_needed;
extern int nswiodone;
extern int vm_swap_size;
extern int vfs_update_wakeup;

#define MAXSCAN 1024		/* maximum number of pages to scan in queues */

#define MAXLAUNDER (cnt.v_page_count > 1800 ? 32 : 16)

#define VM_PAGEOUT_PAGE_COUNT 8
int vm_pageout_page_count = VM_PAGEOUT_PAGE_COUNT;

int vm_page_max_wired;		/* XXX max # of wired pages system-wide */

typedef int freeer_fcn_t __P((vm_map_t, vm_object_t, int, int));
static void vm_pageout_map_deactivate_pages __P((vm_map_t, vm_map_entry_t,
						 int *, freeer_fcn_t *));
static freeer_fcn_t vm_pageout_object_deactivate_pages;
static void vm_req_vmdaemon __P((void));

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
int
vm_pageout_clean(m, sync)
	vm_page_t m;
	int sync;
{
	register vm_object_t object;
	int pageout_status[VM_PAGEOUT_PAGE_COUNT];
	vm_page_t mc[2*VM_PAGEOUT_PAGE_COUNT];
	int pageout_count;
	int anyok = 0;
	int i, forward_okay, backward_okay, page_base;
	vm_offset_t offset = m->offset;

	object = m->object;

	/*
	 * If not OBJT_SWAP, additional memory may be needed to do the pageout.
	 * Try to avoid the deadlock.
	 */
	if ((sync != VM_PAGEOUT_FORCE) &&
	    (object->type != OBJT_SWAP) &&
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
	mc[VM_PAGEOUT_PAGE_COUNT] = m;
	pageout_count = 1;
	page_base = VM_PAGEOUT_PAGE_COUNT;
	forward_okay = TRUE;
	if (offset != 0)
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
			if ((offset + i * PAGE_SIZE) > object->size) {
				forward_okay = FALSE;
				goto do_backward;
			}
			p = vm_page_lookup(object, offset + i * PAGE_SIZE);
			if (p) {
				if ((p->flags & (PG_BUSY|PG_CACHE)) || p->busy) {
					forward_okay = FALSE;
					goto do_backward;
				}
				vm_page_test_dirty(p);
				if ((p->dirty & p->valid) != 0 &&
				    ((p->flags & PG_INACTIVE) ||
				     (sync == VM_PAGEOUT_FORCE)) &&
				    (p->wire_count == 0) &&
				    (p->hold_count == 0)) {
					mc[VM_PAGEOUT_PAGE_COUNT + i] = p;
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
			if ((offset - i * PAGE_SIZE) == 0) {
				backward_okay = FALSE;
			}
			p = vm_page_lookup(object, offset - i * PAGE_SIZE);
			if (p) {
				if ((p->flags & (PG_BUSY|PG_CACHE)) || p->busy) {
					backward_okay = FALSE;
					continue;
				}
				vm_page_test_dirty(p);
				if ((p->dirty & p->valid) != 0 &&
				    ((p->flags & PG_INACTIVE) ||
				     (sync == VM_PAGEOUT_FORCE)) &&
				    (p->wire_count == 0) &&
				    (p->hold_count == 0)) {
					mc[VM_PAGEOUT_PAGE_COUNT - i] = p;
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
	object->paging_in_progress += pageout_count;

	vm_pager_put_pages(object, &mc[page_base], pageout_count,
	    ((sync || (object == kernel_object)) ? TRUE : FALSE),
	    pageout_status);

	for (i = 0; i < pageout_count; i++) {
		vm_page_t mt = mc[page_base + i];

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
			if (mt->flags & PG_INACTIVE)
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
			if ((mt->flags & (PG_REFERENCED|PG_WANTED)) ||
			    pmap_is_referenced(VM_PAGE_TO_PHYS(mt))) {
				pmap_clear_reference(VM_PAGE_TO_PHYS(mt));
				mt->flags &= ~PG_REFERENCED;
				if (mt->flags & PG_INACTIVE)
					vm_page_activate(mt);
			}
			PAGE_WAKEUP(mt);
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
 *	backing_objects.
 *
 *	The object and map must be locked.
 */
static int
vm_pageout_object_deactivate_pages(map, object, count, map_remove_only)
	vm_map_t map;
	vm_object_t object;
	int count;
	int map_remove_only;
{
	register vm_page_t p, next;
	int rcount;
	int dcount;

	dcount = 0;
	if (count == 0)
		count = 1;

	if (object->type == OBJT_DEVICE)
		return 0;

	if (object->backing_object) {
		if (object->backing_object->ref_count == 1)
			dcount += vm_pageout_object_deactivate_pages(map,
			    object->backing_object, count / 2 + 1, map_remove_only);
		else
			vm_pageout_object_deactivate_pages(map,
			    object->backing_object, count, 1);
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
		if (p->wire_count != 0 ||
		    p->hold_count != 0 ||
		    p->busy != 0 ||
		    !pmap_page_exists(vm_map_pmap(map), VM_PAGE_TO_PHYS(p))) {
			p = next;
			continue;
		}
		/*
		 * if a page is active, not wired and is in the processes
		 * pmap, then deactivate the page.
		 */
		if ((p->flags & (PG_ACTIVE | PG_BUSY)) == PG_ACTIVE) {
			if (!pmap_is_referenced(VM_PAGE_TO_PHYS(p)) &&
			    (p->flags & (PG_REFERENCED|PG_WANTED)) == 0) {
				p->act_count -= min(p->act_count, ACT_DECLINE);
				/*
				 * if the page act_count is zero -- then we
				 * deactivate
				 */
				if (!p->act_count) {
					if (!map_remove_only)
						vm_page_deactivate(p);
					vm_page_protect(p, VM_PROT_NONE);
					/*
					 * else if on the next go-around we
					 * will deactivate the page we need to
					 * place the page on the end of the
					 * queue to age the other pages in
					 * memory.
					 */
				} else {
					TAILQ_REMOVE(&vm_page_queue_active, p, pageq);
					TAILQ_INSERT_TAIL(&vm_page_queue_active, p, pageq);
				}
				/*
				 * see if we are done yet
				 */
				if (p->flags & PG_INACTIVE) {
					--count;
					++dcount;
					if (count <= 0 &&
					    cnt.v_inactive_count > cnt.v_inactive_target) {
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
			}
		} else if ((p->flags & (PG_INACTIVE | PG_BUSY)) == PG_INACTIVE) {
			vm_page_protect(p, VM_PROT_NONE);
		}
		p = next;
	}
	return dcount;
}


/*
 * deactivate some number of pages in a map, try to do it fairly, but
 * that is really hard to do.
 */

static void
vm_pageout_map_deactivate_pages(map, entry, count, freeer)
	vm_map_t map;
	vm_map_entry_t entry;
	int *count;
	freeer_fcn_t *freeer;
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
		*count -= (*freeer) (map, obj, *count, TRUE);
	}
	lock_read_done(&map->lock);
	vm_map_deallocate(map);
	return;
}

static void
vm_req_vmdaemon()
{
	static int lastrun = 0;

	if ((ticks > (lastrun + hz / 10)) || (ticks < lastrun)) {
		wakeup(&vm_daemon_needed);
		lastrun = ticks;
	}
}

/*
 *	vm_pageout_scan does the dirty work for the pageout daemon.
 */
int
vm_pageout_scan()
{
	vm_page_t m;
	int page_shortage, maxscan, maxlaunder, pcount;
	int pages_freed;
	vm_page_t next;
	struct proc *p, *bigproc;
	vm_offset_t size, bigsize;
	vm_object_t object;
	int force_wakeup = 0;
	int vnodes_skipped = 0;

	pages_freed = 0;

	/*
	 * Start scanning the inactive queue for pages we can free. We keep
	 * scanning until we have enough free pages or we have scanned through
	 * the entire queue.  If we encounter dirty pages, we start cleaning
	 * them.
	 */

	maxlaunder = (cnt.v_inactive_target > MAXLAUNDER) ?
	    MAXLAUNDER : cnt.v_inactive_target;

rescan1:
	maxscan = cnt.v_inactive_count;
	m = vm_page_queue_inactive.tqh_first;
	while ((m != NULL) && (maxscan-- > 0) &&
	    ((cnt.v_cache_count + cnt.v_free_count) < (cnt.v_cache_min + cnt.v_free_target))) {
		vm_page_t next;

		cnt.v_pdpages++;
		next = m->pageq.tqe_next;

#if defined(VM_DIAGNOSE)
		if ((m->flags & PG_INACTIVE) == 0) {
			printf("vm_pageout_scan: page not inactive?\n");
			break;
		}
#endif

		/*
		 * dont mess with busy pages
		 */
		if (m->hold_count || m->busy || (m->flags & PG_BUSY)) {
			TAILQ_REMOVE(&vm_page_queue_inactive, m, pageq);
			TAILQ_INSERT_TAIL(&vm_page_queue_inactive, m, pageq);
			m = next;
			continue;
		}
		if (((m->flags & PG_REFERENCED) == 0) &&
		    pmap_is_referenced(VM_PAGE_TO_PHYS(m))) {
			m->flags |= PG_REFERENCED;
		}
		if (m->object->ref_count == 0) {
			m->flags &= ~PG_REFERENCED;
			pmap_clear_reference(VM_PAGE_TO_PHYS(m));
		}
		if ((m->flags & (PG_REFERENCED|PG_WANTED)) != 0) {
			m->flags &= ~PG_REFERENCED;
			pmap_clear_reference(VM_PAGE_TO_PHYS(m));
			vm_page_activate(m);
			if (m->act_count < ACT_MAX)
				m->act_count += ACT_ADVANCE;
			m = next;
			continue;
		}

		vm_page_test_dirty(m);
		if (m->dirty == 0) {
			if (m->bmapped == 0) {
				if (m->valid == 0) {
					pmap_page_protect(VM_PAGE_TO_PHYS(m), VM_PROT_NONE);
					vm_page_free(m);
					cnt.v_dfree++;
				} else {
					vm_page_cache(m);
				}
				++pages_freed;
			} else {
				m = next;
				continue;
			}
		} else if (maxlaunder > 0) {
			int written;
			struct vnode *vp = NULL;

			object = m->object;
			if (object->flags & OBJ_DEAD) {
				m = next;
				continue;
			}

			if (object->type == OBJT_VNODE) {
				vp = object->handle;
				if (VOP_ISLOCKED(vp) || vget(vp, 1)) {
					if (object->flags & OBJ_WRITEABLE)
						++vnodes_skipped;
					m = next;
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

			if (!next) {
				break;
			}
			maxlaunder -= written;
			/*
			 * if the next page has been re-activated, start
			 * scanning again
			 */
			if ((next->flags & PG_INACTIVE) == 0) {
				goto rescan1;
			}
		}
		m = next;
	}

	/*
	 * Compute the page shortage.  If we are still very low on memory be
	 * sure that we will move a minimal amount of pages from active to
	 * inactive.
	 */

	page_shortage = cnt.v_inactive_target -
	    (cnt.v_free_count + cnt.v_inactive_count + cnt.v_cache_count);
	if (page_shortage <= 0) {
		if (pages_freed == 0) {
			page_shortage = cnt.v_free_min - cnt.v_free_count;
		} else {
			page_shortage = 1;
		}
	}
	maxscan = MAXSCAN;
	pcount = cnt.v_active_count;
	m = vm_page_queue_active.tqh_first;
	while ((m != NULL) && (maxscan > 0) && (pcount-- > 0) && (page_shortage > 0)) {

		cnt.v_pdpages++;
		next = m->pageq.tqe_next;

		/*
		 * Don't deactivate pages that are busy.
		 */
		if ((m->busy != 0) ||
		    (m->flags & PG_BUSY) ||
		    (m->hold_count != 0)) {
			TAILQ_REMOVE(&vm_page_queue_active, m, pageq);
			TAILQ_INSERT_TAIL(&vm_page_queue_active, m, pageq);
			m = next;
			continue;
		}
		if (m->object->ref_count && ((m->flags & (PG_REFERENCED|PG_WANTED)) ||
			pmap_is_referenced(VM_PAGE_TO_PHYS(m)))) {
			int s;

			pmap_clear_reference(VM_PAGE_TO_PHYS(m));
			m->flags &= ~PG_REFERENCED;
			if (m->act_count < ACT_MAX) {
				m->act_count += ACT_ADVANCE;
			}
			TAILQ_REMOVE(&vm_page_queue_active, m, pageq);
			TAILQ_INSERT_TAIL(&vm_page_queue_active, m, pageq);
		} else {
			m->flags &= ~PG_REFERENCED;
			pmap_clear_reference(VM_PAGE_TO_PHYS(m));
			m->act_count -= min(m->act_count, ACT_DECLINE);

			/*
			 * if the page act_count is zero -- then we deactivate
			 */
			if (!m->act_count && (page_shortage > 0)) {
				if (m->object->ref_count == 0) {
					--page_shortage;
					vm_page_test_dirty(m);
					if ((m->bmapped == 0) && (m->dirty == 0) ) {
						m->act_count = 0;
						vm_page_cache(m);
					} else {
						vm_page_deactivate(m);
					}
				} else {
					vm_page_deactivate(m);
					--page_shortage;
				}
			} else if (m->act_count) {
				TAILQ_REMOVE(&vm_page_queue_active, m, pageq);
				TAILQ_INSERT_TAIL(&vm_page_queue_active, m, pageq);
			}
		}
		maxscan--;
		m = next;
	}

	/*
	 * We try to maintain some *really* free pages, this allows interrupt
	 * code to be guaranteed space.
	 */
	while (cnt.v_free_count < cnt.v_free_reserved) {
		m = vm_page_queue_cache.tqh_first;
		if (!m)
			break;
		vm_page_free(m);
		cnt.v_dfree++;
	}

	/*
	 * If we didn't get enough free pages, and we have skipped a vnode
	 * in a writeable object, wakeup the sync daemon.  And kick swapout
	 * if we did not get enough free pages.
	 */
	if ((cnt.v_cache_count + cnt.v_free_count) < cnt.v_free_target) {
		if (vnodes_skipped &&
		    (cnt.v_cache_count + cnt.v_free_count) < cnt.v_free_min) {
			if (!vfs_update_wakeup) {
				vfs_update_wakeup = 1;
				wakeup(&vfs_update_wakeup);
			}
		}
		/*
		 * now swap processes out if we are in low memory conditions
		 */
		if (!swap_pager_full && vm_swap_size &&
			vm_pageout_req_swapout == 0) {
			vm_pageout_req_swapout = 1;
			vm_req_vmdaemon();
		}
	}

	if ((cnt.v_inactive_count + cnt.v_free_count + cnt.v_cache_count) <
	    (cnt.v_inactive_target + cnt.v_free_min)) {
		vm_req_vmdaemon();
	}

	/*
	 * make sure that we have swap space -- if we are low on memory and
	 * swap -- then kill the biggest process.
	 */
	if ((vm_swap_size == 0 || swap_pager_full) &&
	    ((cnt.v_free_count + cnt.v_cache_count) < cnt.v_free_min)) {
		bigproc = NULL;
		bigsize = 0;
		for (p = (struct proc *) allproc; p != NULL; p = p->p_next) {
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
			printf("Process %lu killed by vm_pageout -- out of swap\n", (u_long) bigproc->p_pid);
			psignal(bigproc, SIGKILL);
			bigproc->p_estcpu = 0;
			bigproc->p_nice = PRIO_MIN;
			resetpriority(bigproc);
			wakeup(&cnt.v_free_count);
		}
	}
	return force_wakeup;
}

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

	if (cnt.v_page_count > 1024)
		cnt.v_free_min = 4 + (cnt.v_page_count - 1024) / 200;
	else
		cnt.v_free_min = 4;
	/*
	 * free_reserved needs to include enough for the largest swap pager
	 * structures plus enough for any pv_entry structs when paging.
	 */
	cnt.v_pageout_free_min = 6 + cnt.v_page_count / 1024 +
				cnt.v_interrupt_free_min;
	cnt.v_free_reserved = cnt.v_pageout_free_min + 6;
	cnt.v_free_target = 3 * cnt.v_free_min + cnt.v_free_reserved;
	cnt.v_free_min += cnt.v_free_reserved;

	if (cnt.v_page_count > 1024) {
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
		int s = splhigh();

		if (!vm_pages_needed ||
			((cnt.v_free_count >= cnt.v_free_reserved) &&
			 (cnt.v_free_count + cnt.v_cache_count >= cnt.v_free_min))) {
			vm_pages_needed = 0;
			tsleep(&vm_pages_needed, PVM, "psleep", 0);
		}
		vm_pages_needed = 0;
		splx(s);
		cnt.v_pdwakeups++;
		vm_pager_sync();
		vm_pageout_scan();
		vm_pager_sync();
		wakeup(&cnt.v_free_count);
		wakeup(kmem_map);
	}
}

static void
vm_daemon()
{
	vm_object_t object;
	struct proc *p;

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

		for (p = (struct proc *) allproc; p != NULL; p = p->p_next) {
			int overage;
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
				overage = (size - limit) >> PAGE_SHIFT;
				vm_pageout_map_deactivate_pages(&p->p_vmspace->vm_map,
				    (vm_map_entry_t) 0, &overage, vm_pageout_object_deactivate_pages);
			}
		}

		/*
		 * we remove cached objects that have no RSS...
		 */
restart:
		object = vm_object_cached_list.tqh_first;
		while (object) {
			/*
			 * if there are no resident pages -- get rid of the object
			 */
			if (object->resident_page_count == 0) {
				vm_object_reference(object);
				pager_cache(object, FALSE);
				goto restart;
			}
			object = object->cached_list.tqe_next;
		}
	}
}
