/*-
 * Copyright (c) 2002-2006 Rice University
 * Copyright (c) 2007 Alan L. Cox <alc@cs.rice.edu>
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Alan L. Cox,
 * Olivier Crameri, Peter Druschel, Sitaram Iyer, and Juan Navarro.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	Physical memory system implementation
 *
 * Any external functions defined by this module are only to be used by the
 * virtual memory system.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#if MAXMEMDOM > 1
#include <sys/proc.h>
#endif
#include <sys/queue.h>
#include <sys/rwlock.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/tree.h>
#include <sys/vmmeter.h>

#include <ddb/ddb.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>

_Static_assert(sizeof(long) * NBBY >= VM_PHYSSEG_MAX,
    "Too many physsegs.");

struct mem_affinity *mem_affinity;

int vm_ndomains = 1;

struct vm_phys_seg vm_phys_segs[VM_PHYSSEG_MAX];
int vm_phys_nsegs;

struct vm_phys_fictitious_seg;
static int vm_phys_fictitious_cmp(struct vm_phys_fictitious_seg *,
    struct vm_phys_fictitious_seg *);

RB_HEAD(fict_tree, vm_phys_fictitious_seg) vm_phys_fictitious_tree =
    RB_INITIALIZER(_vm_phys_fictitious_tree);

struct vm_phys_fictitious_seg {
	RB_ENTRY(vm_phys_fictitious_seg) node;
	/* Memory region data */
	vm_paddr_t	start;
	vm_paddr_t	end;
	vm_page_t	first_page;
};

RB_GENERATE_STATIC(fict_tree, vm_phys_fictitious_seg, node,
    vm_phys_fictitious_cmp);

static struct rwlock vm_phys_fictitious_reg_lock;
MALLOC_DEFINE(M_FICT_PAGES, "vm_fictitious", "Fictitious VM pages");

static struct vm_freelist
    vm_phys_free_queues[MAXMEMDOM][VM_NFREELIST][VM_NFREEPOOL][VM_NFREEORDER];

static int vm_nfreelists = VM_FREELIST_DEFAULT + 1;

static int cnt_prezero;
SYSCTL_INT(_vm_stats_misc, OID_AUTO, cnt_prezero, CTLFLAG_RD,
    &cnt_prezero, 0, "The number of physical pages prezeroed at idle time");

static int sysctl_vm_phys_free(SYSCTL_HANDLER_ARGS);
SYSCTL_OID(_vm, OID_AUTO, phys_free, CTLTYPE_STRING | CTLFLAG_RD,
    NULL, 0, sysctl_vm_phys_free, "A", "Phys Free Info");

static int sysctl_vm_phys_segs(SYSCTL_HANDLER_ARGS);
SYSCTL_OID(_vm, OID_AUTO, phys_segs, CTLTYPE_STRING | CTLFLAG_RD,
    NULL, 0, sysctl_vm_phys_segs, "A", "Phys Seg Info");

SYSCTL_INT(_vm, OID_AUTO, ndomains, CTLFLAG_RD,
    &vm_ndomains, 0, "Number of physical memory domains available.");

static vm_page_t vm_phys_alloc_domain_pages(int domain, int flind, int pool,
    int order);
static void _vm_phys_create_seg(vm_paddr_t start, vm_paddr_t end, int flind,
    int domain);
static void vm_phys_create_seg(vm_paddr_t start, vm_paddr_t end, int flind);
static int vm_phys_paddr_to_segind(vm_paddr_t pa);
static void vm_phys_split_pages(vm_page_t m, int oind, struct vm_freelist *fl,
    int order);

/*
 * Red-black tree helpers for vm fictitious range management.
 */
static inline int
vm_phys_fictitious_in_range(struct vm_phys_fictitious_seg *p,
    struct vm_phys_fictitious_seg *range)
{

	KASSERT(range->start != 0 && range->end != 0,
	    ("Invalid range passed on search for vm_fictitious page"));
	if (p->start >= range->end)
		return (1);
	if (p->start < range->start)
		return (-1);

	return (0);
}

static int
vm_phys_fictitious_cmp(struct vm_phys_fictitious_seg *p1,
    struct vm_phys_fictitious_seg *p2)
{

	/* Check if this is a search for a page */
	if (p1->end == 0)
		return (vm_phys_fictitious_in_range(p1, p2));

	KASSERT(p2->end != 0,
    ("Invalid range passed as second parameter to vm fictitious comparison"));

	/* Searching to add a new range */
	if (p1->end <= p2->start)
		return (-1);
	if (p1->start >= p2->end)
		return (1);

	panic("Trying to add overlapping vm fictitious ranges:\n"
	    "[%#jx:%#jx] and [%#jx:%#jx]", (uintmax_t)p1->start,
	    (uintmax_t)p1->end, (uintmax_t)p2->start, (uintmax_t)p2->end);
}

static __inline int
vm_rr_selectdomain(void)
{
#if MAXMEMDOM > 1
	struct thread *td;

	td = curthread;

	td->td_dom_rr_idx++;
	td->td_dom_rr_idx %= vm_ndomains;
	return (td->td_dom_rr_idx);
#else
	return (0);
#endif
}

boolean_t
vm_phys_domain_intersects(long mask, vm_paddr_t low, vm_paddr_t high)
{
	struct vm_phys_seg *s;
	int idx;

	while ((idx = ffsl(mask)) != 0) {
		idx--;	/* ffsl counts from 1 */
		mask &= ~(1UL << idx);
		s = &vm_phys_segs[idx];
		if (low < s->end && high > s->start)
			return (TRUE);
	}
	return (FALSE);
}

/*
 * Outputs the state of the physical memory allocator, specifically,
 * the amount of physical memory in each free list.
 */
static int
sysctl_vm_phys_free(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	struct vm_freelist *fl;
	int dom, error, flind, oind, pind;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	sbuf_new_for_sysctl(&sbuf, NULL, 128 * vm_ndomains, req);
	for (dom = 0; dom < vm_ndomains; dom++) {
		sbuf_printf(&sbuf,"\nDOMAIN %d:\n", dom);
		for (flind = 0; flind < vm_nfreelists; flind++) {
			sbuf_printf(&sbuf, "\nFREE LIST %d:\n"
			    "\n  ORDER (SIZE)  |  NUMBER"
			    "\n              ", flind);
			for (pind = 0; pind < VM_NFREEPOOL; pind++)
				sbuf_printf(&sbuf, "  |  POOL %d", pind);
			sbuf_printf(&sbuf, "\n--            ");
			for (pind = 0; pind < VM_NFREEPOOL; pind++)
				sbuf_printf(&sbuf, "-- --      ");
			sbuf_printf(&sbuf, "--\n");
			for (oind = VM_NFREEORDER - 1; oind >= 0; oind--) {
				sbuf_printf(&sbuf, "  %2d (%6dK)", oind,
				    1 << (PAGE_SHIFT - 10 + oind));
				for (pind = 0; pind < VM_NFREEPOOL; pind++) {
				fl = vm_phys_free_queues[dom][flind][pind];
					sbuf_printf(&sbuf, "  |  %6d",
					    fl[oind].lcnt);
				}
				sbuf_printf(&sbuf, "\n");
			}
		}
	}
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	return (error);
}

/*
 * Outputs the set of physical memory segments.
 */
static int
sysctl_vm_phys_segs(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	struct vm_phys_seg *seg;
	int error, segind;

	error = sysctl_wire_old_buffer(req, 0);
	if (error != 0)
		return (error);
	sbuf_new_for_sysctl(&sbuf, NULL, 128, req);
	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		sbuf_printf(&sbuf, "\nSEGMENT %d:\n\n", segind);
		seg = &vm_phys_segs[segind];
		sbuf_printf(&sbuf, "start:     %#jx\n",
		    (uintmax_t)seg->start);
		sbuf_printf(&sbuf, "end:       %#jx\n",
		    (uintmax_t)seg->end);
		sbuf_printf(&sbuf, "domain:    %d\n", seg->domain);
		sbuf_printf(&sbuf, "free list: %p\n", seg->free_queues);
	}
	error = sbuf_finish(&sbuf);
	sbuf_delete(&sbuf);
	return (error);
}

static void
vm_freelist_add(struct vm_freelist *fl, vm_page_t m, int order, int tail)
{

	m->order = order;
	if (tail)
		TAILQ_INSERT_TAIL(&fl[order].pl, m, plinks.q);
	else
		TAILQ_INSERT_HEAD(&fl[order].pl, m, plinks.q);
	fl[order].lcnt++;
}

static void
vm_freelist_rem(struct vm_freelist *fl, vm_page_t m, int order)
{

	TAILQ_REMOVE(&fl[order].pl, m, plinks.q);
	fl[order].lcnt--;
	m->order = VM_NFREEORDER;
}

/*
 * Create a physical memory segment.
 */
static void
_vm_phys_create_seg(vm_paddr_t start, vm_paddr_t end, int flind, int domain)
{
	struct vm_phys_seg *seg;
#ifdef VM_PHYSSEG_SPARSE
	long pages;
	int segind;

	pages = 0;
	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		seg = &vm_phys_segs[segind];
		pages += atop(seg->end - seg->start);
	}
#endif
	KASSERT(vm_phys_nsegs < VM_PHYSSEG_MAX,
	    ("vm_phys_create_seg: increase VM_PHYSSEG_MAX"));
	KASSERT(domain < vm_ndomains,
	    ("vm_phys_create_seg: invalid domain provided"));
	seg = &vm_phys_segs[vm_phys_nsegs++];
	seg->start = start;
	seg->end = end;
	seg->domain = domain;
#ifdef VM_PHYSSEG_SPARSE
	seg->first_page = &vm_page_array[pages];
#else
	seg->first_page = PHYS_TO_VM_PAGE(start);
#endif
	seg->free_queues = &vm_phys_free_queues[domain][flind];
}

static void
vm_phys_create_seg(vm_paddr_t start, vm_paddr_t end, int flind)
{
	int i;

	if (mem_affinity == NULL) {
		_vm_phys_create_seg(start, end, flind, 0);
		return;
	}

	for (i = 0;; i++) {
		if (mem_affinity[i].end == 0)
			panic("Reached end of affinity info");
		if (mem_affinity[i].end <= start)
			continue;
		if (mem_affinity[i].start > start)
			panic("No affinity info for start %jx",
			    (uintmax_t)start);
		if (mem_affinity[i].end >= end) {
			_vm_phys_create_seg(start, end, flind,
			    mem_affinity[i].domain);
			break;
		}
		_vm_phys_create_seg(start, mem_affinity[i].end, flind,
		    mem_affinity[i].domain);
		start = mem_affinity[i].end;
	}
}

/*
 * Initialize the physical memory allocator.
 */
void
vm_phys_init(void)
{
	struct vm_freelist *fl;
	int dom, flind, i, oind, pind;

	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
#ifdef	VM_FREELIST_ISADMA
		if (phys_avail[i] < 16777216) {
			if (phys_avail[i + 1] > 16777216) {
				vm_phys_create_seg(phys_avail[i], 16777216,
				    VM_FREELIST_ISADMA);
				vm_phys_create_seg(16777216, phys_avail[i + 1],
				    VM_FREELIST_DEFAULT);
			} else {
				vm_phys_create_seg(phys_avail[i],
				    phys_avail[i + 1], VM_FREELIST_ISADMA);
			}
			if (VM_FREELIST_ISADMA >= vm_nfreelists)
				vm_nfreelists = VM_FREELIST_ISADMA + 1;
		} else
#endif
#ifdef	VM_FREELIST_HIGHMEM
		if (phys_avail[i + 1] > VM_HIGHMEM_ADDRESS) {
			if (phys_avail[i] < VM_HIGHMEM_ADDRESS) {
				vm_phys_create_seg(phys_avail[i],
				    VM_HIGHMEM_ADDRESS, VM_FREELIST_DEFAULT);
				vm_phys_create_seg(VM_HIGHMEM_ADDRESS,
				    phys_avail[i + 1], VM_FREELIST_HIGHMEM);
			} else {
				vm_phys_create_seg(phys_avail[i],
				    phys_avail[i + 1], VM_FREELIST_HIGHMEM);
			}
			if (VM_FREELIST_HIGHMEM >= vm_nfreelists)
				vm_nfreelists = VM_FREELIST_HIGHMEM + 1;
		} else
#endif
		vm_phys_create_seg(phys_avail[i], phys_avail[i + 1],
		    VM_FREELIST_DEFAULT);
	}
	for (dom = 0; dom < vm_ndomains; dom++) {
		for (flind = 0; flind < vm_nfreelists; flind++) {
			for (pind = 0; pind < VM_NFREEPOOL; pind++) {
				fl = vm_phys_free_queues[dom][flind][pind];
				for (oind = 0; oind < VM_NFREEORDER; oind++)
					TAILQ_INIT(&fl[oind].pl);
			}
		}
	}
	rw_init(&vm_phys_fictitious_reg_lock, "vmfctr");
}

/*
 * Split a contiguous, power of two-sized set of physical pages.
 */
static __inline void
vm_phys_split_pages(vm_page_t m, int oind, struct vm_freelist *fl, int order)
{
	vm_page_t m_buddy;

	while (oind > order) {
		oind--;
		m_buddy = &m[1 << oind];
		KASSERT(m_buddy->order == VM_NFREEORDER,
		    ("vm_phys_split_pages: page %p has unexpected order %d",
		    m_buddy, m_buddy->order));
		vm_freelist_add(fl, m_buddy, oind, 0);
        }
}

/*
 * Initialize a physical page and add it to the free lists.
 */
void
vm_phys_add_page(vm_paddr_t pa)
{
	vm_page_t m;
	struct vm_domain *vmd;

	vm_cnt.v_page_count++;
	m = vm_phys_paddr_to_vm_page(pa);
	m->phys_addr = pa;
	m->queue = PQ_NONE;
	m->segind = vm_phys_paddr_to_segind(pa);
	vmd = vm_phys_domain(m);
	vmd->vmd_page_count++;
	vmd->vmd_segs |= 1UL << m->segind;
	KASSERT(m->order == VM_NFREEORDER,
	    ("vm_phys_add_page: page %p has unexpected order %d",
	    m, m->order));
	m->pool = VM_FREEPOOL_DEFAULT;
	pmap_page_init(m);
	mtx_lock(&vm_page_queue_free_mtx);
	vm_phys_freecnt_adj(m, 1);
	vm_phys_free_pages(m, 0);
	mtx_unlock(&vm_page_queue_free_mtx);
}

/*
 * Allocate a contiguous, power of two-sized set of physical pages
 * from the free lists.
 *
 * The free page queues must be locked.
 */
vm_page_t
vm_phys_alloc_pages(int pool, int order)
{
	vm_page_t m;
	int dom, domain, flind;

	KASSERT(pool < VM_NFREEPOOL,
	    ("vm_phys_alloc_pages: pool %d is out of range", pool));
	KASSERT(order < VM_NFREEORDER,
	    ("vm_phys_alloc_pages: order %d is out of range", order));

	for (dom = 0; dom < vm_ndomains; dom++) {
		domain = vm_rr_selectdomain();
		for (flind = 0; flind < vm_nfreelists; flind++) {
			m = vm_phys_alloc_domain_pages(domain, flind, pool,
			    order);
			if (m != NULL)
				return (m);
		}
	}
	return (NULL);
}

/*
 * Find and dequeue a free page on the given free list, with the 
 * specified pool and order
 */
vm_page_t
vm_phys_alloc_freelist_pages(int flind, int pool, int order)
{
	vm_page_t m;
	int dom, domain;

	KASSERT(flind < VM_NFREELIST,
	    ("vm_phys_alloc_freelist_pages: freelist %d is out of range", flind));
	KASSERT(pool < VM_NFREEPOOL,
	    ("vm_phys_alloc_freelist_pages: pool %d is out of range", pool));
	KASSERT(order < VM_NFREEORDER,
	    ("vm_phys_alloc_freelist_pages: order %d is out of range", order));

	for (dom = 0; dom < vm_ndomains; dom++) {
		domain = vm_rr_selectdomain();
		m = vm_phys_alloc_domain_pages(domain, flind, pool, order);
		if (m != NULL)
			return (m);
	}
	return (NULL);
}

static vm_page_t
vm_phys_alloc_domain_pages(int domain, int flind, int pool, int order)
{	
	struct vm_freelist *fl;
	struct vm_freelist *alt;
	int oind, pind;
	vm_page_t m;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	fl = &vm_phys_free_queues[domain][flind][pool][0];
	for (oind = order; oind < VM_NFREEORDER; oind++) {
		m = TAILQ_FIRST(&fl[oind].pl);
		if (m != NULL) {
			vm_freelist_rem(fl, m, oind);
			vm_phys_split_pages(m, oind, fl, order);
			return (m);
		}
	}

	/*
	 * The given pool was empty.  Find the largest
	 * contiguous, power-of-two-sized set of pages in any
	 * pool.  Transfer these pages to the given pool, and
	 * use them to satisfy the allocation.
	 */
	for (oind = VM_NFREEORDER - 1; oind >= order; oind--) {
		for (pind = 0; pind < VM_NFREEPOOL; pind++) {
			alt = &vm_phys_free_queues[domain][flind][pind][0];
			m = TAILQ_FIRST(&alt[oind].pl);
			if (m != NULL) {
				vm_freelist_rem(alt, m, oind);
				vm_phys_set_pool(pool, m, oind);
				vm_phys_split_pages(m, oind, fl, order);
				return (m);
			}
		}
	}
	return (NULL);
}

/*
 * Find the vm_page corresponding to the given physical address.
 */
vm_page_t
vm_phys_paddr_to_vm_page(vm_paddr_t pa)
{
	struct vm_phys_seg *seg;
	int segind;

	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		seg = &vm_phys_segs[segind];
		if (pa >= seg->start && pa < seg->end)
			return (&seg->first_page[atop(pa - seg->start)]);
	}
	return (NULL);
}

vm_page_t
vm_phys_fictitious_to_vm_page(vm_paddr_t pa)
{
	struct vm_phys_fictitious_seg tmp, *seg;
	vm_page_t m;

	m = NULL;
	tmp.start = pa;
	tmp.end = 0;

	rw_rlock(&vm_phys_fictitious_reg_lock);
	seg = RB_FIND(fict_tree, &vm_phys_fictitious_tree, &tmp);
	rw_runlock(&vm_phys_fictitious_reg_lock);
	if (seg == NULL)
		return (NULL);

	m = &seg->first_page[atop(pa - seg->start)];
	KASSERT((m->flags & PG_FICTITIOUS) != 0, ("%p not fictitious", m));

	return (m);
}

int
vm_phys_fictitious_reg_range(vm_paddr_t start, vm_paddr_t end,
    vm_memattr_t memattr)
{
	struct vm_phys_fictitious_seg *seg;
	vm_page_t fp;
	long i, page_count;
#ifdef VM_PHYSSEG_DENSE
	long pi;
#endif

	page_count = (end - start) / PAGE_SIZE;

#ifdef VM_PHYSSEG_DENSE
	pi = atop(start);
	if (pi >= first_page && pi < vm_page_array_size + first_page) {
		if (atop(end) >= vm_page_array_size + first_page)
			return (EINVAL);
		fp = &vm_page_array[pi - first_page];
	} else
#endif
	{
		fp = malloc(page_count * sizeof(struct vm_page), M_FICT_PAGES,
		    M_WAITOK | M_ZERO);
	}
	for (i = 0; i < page_count; i++) {
		vm_page_initfake(&fp[i], start + PAGE_SIZE * i, memattr);
		fp[i].oflags &= ~VPO_UNMANAGED;
		fp[i].busy_lock = VPB_UNBUSIED;
	}

	seg = malloc(sizeof(*seg), M_FICT_PAGES, M_WAITOK | M_ZERO);
	seg->start = start;
	seg->end = end;
	seg->first_page = fp;

	rw_wlock(&vm_phys_fictitious_reg_lock);
	RB_INSERT(fict_tree, &vm_phys_fictitious_tree, seg);
	rw_wunlock(&vm_phys_fictitious_reg_lock);

	return (0);
}

void
vm_phys_fictitious_unreg_range(vm_paddr_t start, vm_paddr_t end)
{
	struct vm_phys_fictitious_seg *seg, tmp;
#ifdef VM_PHYSSEG_DENSE
	long pi;
#endif

#ifdef VM_PHYSSEG_DENSE
	pi = atop(start);
#endif
	tmp.start = start;
	tmp.end = 0;

	rw_wlock(&vm_phys_fictitious_reg_lock);
	seg = RB_FIND(fict_tree, &vm_phys_fictitious_tree, &tmp);
	if (seg->start != start || seg->end != end) {
		rw_wunlock(&vm_phys_fictitious_reg_lock);
		panic(
		    "Unregistering not registered fictitious range [%#jx:%#jx]",
		    (uintmax_t)start, (uintmax_t)end);
	}
	RB_REMOVE(fict_tree, &vm_phys_fictitious_tree, seg);
	rw_wunlock(&vm_phys_fictitious_reg_lock);
#ifdef VM_PHYSSEG_DENSE
	if (pi < first_page || atop(end) >= vm_page_array_size)
#endif
		free(seg->first_page, M_FICT_PAGES);
	free(seg, M_FICT_PAGES);
}

/*
 * Find the segment containing the given physical address.
 */
static int
vm_phys_paddr_to_segind(vm_paddr_t pa)
{
	struct vm_phys_seg *seg;
	int segind;

	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		seg = &vm_phys_segs[segind];
		if (pa >= seg->start && pa < seg->end)
			return (segind);
	}
	panic("vm_phys_paddr_to_segind: paddr %#jx is not in any segment" ,
	    (uintmax_t)pa);
}

/*
 * Free a contiguous, power of two-sized set of physical pages.
 *
 * The free page queues must be locked.
 */
void
vm_phys_free_pages(vm_page_t m, int order)
{
	struct vm_freelist *fl;
	struct vm_phys_seg *seg;
	vm_paddr_t pa;
	vm_page_t m_buddy;

	KASSERT(m->order == VM_NFREEORDER,
	    ("vm_phys_free_pages: page %p has unexpected order %d",
	    m, m->order));
	KASSERT(m->pool < VM_NFREEPOOL,
	    ("vm_phys_free_pages: page %p has unexpected pool %d",
	    m, m->pool));
	KASSERT(order < VM_NFREEORDER,
	    ("vm_phys_free_pages: order %d is out of range", order));
	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	seg = &vm_phys_segs[m->segind];
	if (order < VM_NFREEORDER - 1) {
		pa = VM_PAGE_TO_PHYS(m);
		do {
			pa ^= ((vm_paddr_t)1 << (PAGE_SHIFT + order));
			if (pa < seg->start || pa >= seg->end)
				break;
			m_buddy = &seg->first_page[atop(pa - seg->start)];
			if (m_buddy->order != order)
				break;
			fl = (*seg->free_queues)[m_buddy->pool];
			vm_freelist_rem(fl, m_buddy, order);
			if (m_buddy->pool != m->pool)
				vm_phys_set_pool(m->pool, m_buddy, order);
			order++;
			pa &= ~(((vm_paddr_t)1 << (PAGE_SHIFT + order)) - 1);
			m = &seg->first_page[atop(pa - seg->start)];
		} while (order < VM_NFREEORDER - 1);
	}
	fl = (*seg->free_queues)[m->pool];
	vm_freelist_add(fl, m, order, 1);
}

/*
 * Free a contiguous, arbitrarily sized set of physical pages.
 *
 * The free page queues must be locked.
 */
void
vm_phys_free_contig(vm_page_t m, u_long npages)
{
	u_int n;
	int order;

	/*
	 * Avoid unnecessary coalescing by freeing the pages in the largest
	 * possible power-of-two-sized subsets.
	 */
	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	for (;; npages -= n) {
		/*
		 * Unsigned "min" is used here so that "order" is assigned
		 * "VM_NFREEORDER - 1" when "m"'s physical address is zero
		 * or the low-order bits of its physical address are zero
		 * because the size of a physical address exceeds the size of
		 * a long.
		 */
		order = min(ffsl(VM_PAGE_TO_PHYS(m) >> PAGE_SHIFT) - 1,
		    VM_NFREEORDER - 1);
		n = 1 << order;
		if (npages < n)
			break;
		vm_phys_free_pages(m, order);
		m += n;
	}
	/* The residual "npages" is less than "1 << (VM_NFREEORDER - 1)". */
	for (; npages > 0; npages -= n) {
		order = flsl(npages) - 1;
		n = 1 << order;
		vm_phys_free_pages(m, order);
		m += n;
	}
}

/*
 * Set the pool for a contiguous, power of two-sized set of physical pages. 
 */
void
vm_phys_set_pool(int pool, vm_page_t m, int order)
{
	vm_page_t m_tmp;

	for (m_tmp = m; m_tmp < &m[1 << order]; m_tmp++)
		m_tmp->pool = pool;
}

/*
 * Search for the given physical page "m" in the free lists.  If the search
 * succeeds, remove "m" from the free lists and return TRUE.  Otherwise, return
 * FALSE, indicating that "m" is not in the free lists.
 *
 * The free page queues must be locked.
 */
boolean_t
vm_phys_unfree_page(vm_page_t m)
{
	struct vm_freelist *fl;
	struct vm_phys_seg *seg;
	vm_paddr_t pa, pa_half;
	vm_page_t m_set, m_tmp;
	int order;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);

	/*
	 * First, find the contiguous, power of two-sized set of free
	 * physical pages containing the given physical page "m" and
	 * assign it to "m_set".
	 */
	seg = &vm_phys_segs[m->segind];
	for (m_set = m, order = 0; m_set->order == VM_NFREEORDER &&
	    order < VM_NFREEORDER - 1; ) {
		order++;
		pa = m->phys_addr & (~(vm_paddr_t)0 << (PAGE_SHIFT + order));
		if (pa >= seg->start)
			m_set = &seg->first_page[atop(pa - seg->start)];
		else
			return (FALSE);
	}
	if (m_set->order < order)
		return (FALSE);
	if (m_set->order == VM_NFREEORDER)
		return (FALSE);
	KASSERT(m_set->order < VM_NFREEORDER,
	    ("vm_phys_unfree_page: page %p has unexpected order %d",
	    m_set, m_set->order));

	/*
	 * Next, remove "m_set" from the free lists.  Finally, extract
	 * "m" from "m_set" using an iterative algorithm: While "m_set"
	 * is larger than a page, shrink "m_set" by returning the half
	 * of "m_set" that does not contain "m" to the free lists.
	 */
	fl = (*seg->free_queues)[m_set->pool];
	order = m_set->order;
	vm_freelist_rem(fl, m_set, order);
	while (order > 0) {
		order--;
		pa_half = m_set->phys_addr ^ (1 << (PAGE_SHIFT + order));
		if (m->phys_addr < pa_half)
			m_tmp = &seg->first_page[atop(pa_half - seg->start)];
		else {
			m_tmp = m_set;
			m_set = &seg->first_page[atop(pa_half - seg->start)];
		}
		vm_freelist_add(fl, m_tmp, order, 0);
	}
	KASSERT(m_set == m, ("vm_phys_unfree_page: fatal inconsistency"));
	return (TRUE);
}

/*
 * Try to zero one physical page.  Used by an idle priority thread.
 */
boolean_t
vm_phys_zero_pages_idle(void)
{
	static struct vm_freelist *fl;
	static int flind, oind, pind;
	vm_page_t m, m_tmp;
	int domain;

	domain = vm_rr_selectdomain();
	fl = vm_phys_free_queues[domain][0][0];
	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	for (;;) {
		TAILQ_FOREACH_REVERSE(m, &fl[oind].pl, pglist, plinks.q) {
			for (m_tmp = m; m_tmp < &m[1 << oind]; m_tmp++) {
				if ((m_tmp->flags & (PG_CACHED | PG_ZERO)) == 0) {
					vm_phys_unfree_page(m_tmp);
					vm_phys_freecnt_adj(m, -1);
					mtx_unlock(&vm_page_queue_free_mtx);
					pmap_zero_page_idle(m_tmp);
					m_tmp->flags |= PG_ZERO;
					mtx_lock(&vm_page_queue_free_mtx);
					vm_phys_freecnt_adj(m, 1);
					vm_phys_free_pages(m_tmp, 0);
					vm_page_zero_count++;
					cnt_prezero++;
					return (TRUE);
				}
			}
		}
		oind++;
		if (oind == VM_NFREEORDER) {
			oind = 0;
			pind++;
			if (pind == VM_NFREEPOOL) {
				pind = 0;
				flind++;
				if (flind == vm_nfreelists)
					flind = 0;
			}
			fl = vm_phys_free_queues[domain][flind][pind];
		}
	}
}

/*
 * Allocate a contiguous set of physical pages of the given size
 * "npages" from the free lists.  All of the physical pages must be at
 * or above the given physical address "low" and below the given
 * physical address "high".  The given value "alignment" determines the
 * alignment of the first physical page in the set.  If the given value
 * "boundary" is non-zero, then the set of physical pages cannot cross
 * any physical address boundary that is a multiple of that value.  Both
 * "alignment" and "boundary" must be a power of two.
 */
vm_page_t
vm_phys_alloc_contig(u_long npages, vm_paddr_t low, vm_paddr_t high,
    u_long alignment, vm_paddr_t boundary)
{
	struct vm_freelist *fl;
	struct vm_phys_seg *seg;
	vm_paddr_t pa, pa_last, size;
	vm_page_t m, m_ret;
	u_long npages_end;
	int dom, domain, flind, oind, order, pind;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	size = npages << PAGE_SHIFT;
	KASSERT(size != 0,
	    ("vm_phys_alloc_contig: size must not be 0"));
	KASSERT((alignment & (alignment - 1)) == 0,
	    ("vm_phys_alloc_contig: alignment must be a power of 2"));
	KASSERT((boundary & (boundary - 1)) == 0,
	    ("vm_phys_alloc_contig: boundary must be a power of 2"));
	/* Compute the queue that is the best fit for npages. */
	for (order = 0; (1 << order) < npages; order++);
	dom = 0;
restartdom:
	domain = vm_rr_selectdomain();
	for (flind = 0; flind < vm_nfreelists; flind++) {
		for (oind = min(order, VM_NFREEORDER - 1); oind < VM_NFREEORDER; oind++) {
			for (pind = 0; pind < VM_NFREEPOOL; pind++) {
				fl = &vm_phys_free_queues[domain][flind][pind][0];
				TAILQ_FOREACH(m_ret, &fl[oind].pl, plinks.q) {
					/*
					 * A free list may contain physical pages
					 * from one or more segments.
					 */
					seg = &vm_phys_segs[m_ret->segind];
					if (seg->start > high ||
					    low >= seg->end)
						continue;

					/*
					 * Is the size of this allocation request
					 * larger than the largest block size?
					 */
					if (order >= VM_NFREEORDER) {
						/*
						 * Determine if a sufficient number
						 * of subsequent blocks to satisfy
						 * the allocation request are free.
						 */
						pa = VM_PAGE_TO_PHYS(m_ret);
						pa_last = pa + size;
						for (;;) {
							pa += 1 << (PAGE_SHIFT + VM_NFREEORDER - 1);
							if (pa >= pa_last)
								break;
							if (pa < seg->start ||
							    pa >= seg->end)
								break;
							m = &seg->first_page[atop(pa - seg->start)];
							if (m->order != VM_NFREEORDER - 1)
								break;
						}
						/* If not, continue to the next block. */
						if (pa < pa_last)
							continue;
					}

					/*
					 * Determine if the blocks are within the given range,
					 * satisfy the given alignment, and do not cross the
					 * given boundary.
					 */
					pa = VM_PAGE_TO_PHYS(m_ret);
					if (pa >= low &&
					    pa + size <= high &&
					    (pa & (alignment - 1)) == 0 &&
					    ((pa ^ (pa + size - 1)) & ~(boundary - 1)) == 0)
						goto done;
				}
			}
		}
	}
	if (++dom < vm_ndomains)
		goto restartdom;
	return (NULL);
done:
	for (m = m_ret; m < &m_ret[npages]; m = &m[1 << oind]) {
		fl = (*seg->free_queues)[m->pool];
		vm_freelist_rem(fl, m, m->order);
	}
	if (m_ret->pool != VM_FREEPOOL_DEFAULT)
		vm_phys_set_pool(VM_FREEPOOL_DEFAULT, m_ret, oind);
	fl = (*seg->free_queues)[m_ret->pool];
	vm_phys_split_pages(m_ret, oind, fl, order);
	/* Return excess pages to the free lists. */
	npages_end = roundup2(npages, 1 << imin(oind, order));
	if (npages < npages_end)
		vm_phys_free_contig(&m_ret[npages], npages_end - npages);
	return (m_ret);
}

#ifdef DDB
/*
 * Show the number of physical pages in each of the free lists.
 */
DB_SHOW_COMMAND(freepages, db_show_freepages)
{
	struct vm_freelist *fl;
	int flind, oind, pind, dom;

	for (dom = 0; dom < vm_ndomains; dom++) {
		db_printf("DOMAIN: %d\n", dom);
		for (flind = 0; flind < vm_nfreelists; flind++) {
			db_printf("FREE LIST %d:\n"
			    "\n  ORDER (SIZE)  |  NUMBER"
			    "\n              ", flind);
			for (pind = 0; pind < VM_NFREEPOOL; pind++)
				db_printf("  |  POOL %d", pind);
			db_printf("\n--            ");
			for (pind = 0; pind < VM_NFREEPOOL; pind++)
				db_printf("-- --      ");
			db_printf("--\n");
			for (oind = VM_NFREEORDER - 1; oind >= 0; oind--) {
				db_printf("  %2.2d (%6.6dK)", oind,
				    1 << (PAGE_SHIFT - 10 + oind));
				for (pind = 0; pind < VM_NFREEPOOL; pind++) {
				fl = vm_phys_free_queues[dom][flind][pind];
					db_printf("  |  %6.6d", fl[oind].lcnt);
				}
				db_printf("\n");
			}
			db_printf("\n");
		}
		db_printf("\n");
	}
}
#endif
