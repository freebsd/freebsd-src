/*
 *	(c)Copyright 1998, Matthew Dillon.  Terms for use and redistribution
 *	are covered by the BSD Copyright as found in /usr/src/COPYRIGHT.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/vm_extern.h>

struct vpgqueues vm_page_queues[PQ_COUNT];

void
vm_pageq_init(void) 
{
	int i;

	for (i = 0; i < PQ_L2_SIZE; i++) {
		vm_page_queues[PQ_FREE+i].cnt = &cnt.v_free_count;
	}
	for (i = 0; i < PQ_L2_SIZE; i++) {
		vm_page_queues[PQ_CACHE+i].cnt = &cnt.v_cache_count;
	}
	vm_page_queues[PQ_INACTIVE].cnt = &cnt.v_inactive_count;
	vm_page_queues[PQ_ACTIVE].cnt = &cnt.v_active_count;
	vm_page_queues[PQ_HOLD].cnt = &cnt.v_active_count;

	for (i = 0; i < PQ_COUNT; i++) {
		TAILQ_INIT(&vm_page_queues[i].pl);
	}
}

static __inline struct vpgqueues *
vm_pageq_aquire(int queue)
{
	struct vpgqueues *vpq = NULL;

	if (queue != PQ_NONE) {
		vpq = &vm_page_queues[queue];
	}
	return (vpq);
}

void
vm_pageq_requeue(vm_page_t m)
{
	int queue = m->queue;
	struct vpgqueues *vpq;

	vpq = vm_pageq_aquire(queue);
	TAILQ_REMOVE(&vpq->pl, m, pageq);
	TAILQ_INSERT_TAIL(&vpq->pl, m, pageq);
}

/*
 *	vm_pageq_enqueue:
 *
 */
void
vm_pageq_enqueue(int queue, vm_page_t m)
{
	struct vpgqueues *vpq;

	vpq = &vm_page_queues[queue];
	m->queue = queue;
	TAILQ_INSERT_TAIL(&vpq->pl, m, pageq);
	++*vpq->cnt;
	++vpq->lcnt;
}

/*
 *	vm_add_new_page:
 *
 *	Add a new page to the freelist for use by the system.
 *	Must be called at splhigh().
 */
vm_page_t
vm_pageq_add_new_page(vm_offset_t pa)
{
	vm_page_t m;

	GIANT_REQUIRED;

	++cnt.v_page_count;
	m = PHYS_TO_VM_PAGE(pa);
	m->phys_addr = pa;
	m->flags = 0;
	m->pc = (pa >> PAGE_SHIFT) & PQ_L2_MASK;
	vm_pageq_enqueue(m->pc + PQ_FREE, m);
	return (m);
}

/*
 * vm_pageq_remove_nowakeup:
 *
 * 	vm_page_unqueue() without any wakeup
 *
 *	This routine must be called at splhigh().
 *	This routine may not block.
 */
void
vm_pageq_remove_nowakeup(vm_page_t m)
{
	int queue = m->queue;
	struct vpgqueues *pq;
	if (queue != PQ_NONE) {
		pq = &vm_page_queues[queue];
		m->queue = PQ_NONE;
		TAILQ_REMOVE(&pq->pl, m, pageq);
		(*pq->cnt)--;
		pq->lcnt--;
	}
}

/*
 * vm_pageq_remove:
 *
 *	Remove a page from its queue.
 *
 *	This routine must be called at splhigh().
 *	This routine may not block.
 */
void
vm_pageq_remove(vm_page_t m)
{
	int queue = m->queue;
	struct vpgqueues *pq;

	GIANT_REQUIRED;
	if (queue != PQ_NONE) {
		m->queue = PQ_NONE;
		pq = &vm_page_queues[queue];
		TAILQ_REMOVE(&pq->pl, m, pageq);
		(*pq->cnt)--;
		pq->lcnt--;
		if ((queue - m->pc) == PQ_CACHE) {
			if (vm_paging_needed())
				pagedaemon_wakeup();
		}
	}
}

#if PQ_L2_SIZE > 1

/*
 *	vm_pageq_find:
 *
 *	Find a page on the specified queue with color optimization.
 *
 *	The page coloring optimization attempts to locate a page
 *	that does not overload other nearby pages in the object in
 *	the cpu's L1 or L2 caches.  We need this optimization because 
 *	cpu caches tend to be physical caches, while object spaces tend 
 *	to be virtual.
 *
 *	This routine must be called at splvm().
 *	This routine may not block.
 *
 *	This routine may only be called from the vm_page_list_find() macro
 *	in vm_page.h
 */
static __inline vm_page_t
_vm_pageq_find(int basequeue, int index)
{
	int i;
	vm_page_t m = NULL;
	struct vpgqueues *pq;

	pq = &vm_page_queues[basequeue];

	/*
	 * Note that for the first loop, index+i and index-i wind up at the
	 * same place.  Even though this is not totally optimal, we've already
	 * blown it by missing the cache case so we do not care.
	 */
	for (i = PQ_L2_SIZE / 2; i > 0; --i) {
		if ((m = TAILQ_FIRST(&pq[(index + i) & PQ_L2_MASK].pl)) != NULL)
			break;

		if ((m = TAILQ_FIRST(&pq[(index - i) & PQ_L2_MASK].pl)) != NULL)
			break;
	}
	return (m);
}
#endif		/* PQ_L2_SIZE > 1 */

vm_page_t
vm_pageq_find(int basequeue, int index, boolean_t prefer_zero)
{
        vm_page_t m;

#if PQ_L2_SIZE > 1
        if (prefer_zero) {
                m = TAILQ_LAST(&vm_page_queues[basequeue+index].pl, pglist);
        } else {
                m = TAILQ_FIRST(&vm_page_queues[basequeue+index].pl);
        }
        if (m == NULL) {
                m = _vm_pageq_find(basequeue, index);
	}
#else
        if (prefer_zero) {
                m = TAILQ_LAST(&vm_page_queues[basequeue].pl, pglist);
        } else {
                m = TAILQ_FIRST(&vm_page_queues[basequeue].pl);
        }
#endif
        return (m);
}

