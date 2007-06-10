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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>

#include <ddb/ddb.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>

struct vm_freelist {
	struct pglist pl;
	int lcnt;
};

struct vm_phys_seg {
	vm_paddr_t	start;
	vm_paddr_t	end;
	vm_page_t	first_page;
	struct vm_freelist (*free_queues)[VM_NFREEPOOL][VM_NFREEORDER];
};

static struct vm_phys_seg vm_phys_segs[VM_PHYSSEG_MAX];

static int vm_phys_nsegs;

static struct vm_freelist
    vm_phys_free_queues[VM_NFREELIST][VM_NFREEPOOL][VM_NFREEORDER];

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

static void vm_phys_create_seg(vm_paddr_t start, vm_paddr_t end, int flind);
static int vm_phys_paddr_to_segind(vm_paddr_t pa);
static void vm_phys_set_pool(int pool, vm_page_t m, int order);
static void vm_phys_split_pages(vm_page_t m, int oind, struct vm_freelist *fl,
    int order);

/*
 * Outputs the state of the physical memory allocator, specifically,
 * the amount of physical memory in each free list.
 */
static int
sysctl_vm_phys_free(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sbuf;
	struct vm_freelist *fl;
	char *cbuf;
	const int cbufsize = vm_nfreelists*(VM_NFREEORDER + 1)*81;
	int error, flind, oind, pind;

	cbuf = malloc(cbufsize, M_TEMP, M_WAITOK | M_ZERO);
	sbuf_new(&sbuf, cbuf, cbufsize, SBUF_FIXEDLEN);
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
			sbuf_printf(&sbuf, "  %2.2d (%6.6dK)", oind,
			    1 << (PAGE_SHIFT - 10 + oind));
			for (pind = 0; pind < VM_NFREEPOOL; pind++) {
				fl = vm_phys_free_queues[flind][pind];
				sbuf_printf(&sbuf, "  |  %6.6d", fl[oind].lcnt);
			}
			sbuf_printf(&sbuf, "\n");
		}
	}
	sbuf_finish(&sbuf);
	error = SYSCTL_OUT(req, sbuf_data(&sbuf), sbuf_len(&sbuf));
	sbuf_delete(&sbuf);
	free(cbuf, M_TEMP);
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
	char *cbuf;
	const int cbufsize = VM_PHYSSEG_MAX*(VM_NFREEORDER + 1)*81;
	int error, segind;

	cbuf = malloc(cbufsize, M_TEMP, M_WAITOK | M_ZERO);
	sbuf_new(&sbuf, cbuf, cbufsize, SBUF_FIXEDLEN);
	for (segind = 0; segind < vm_phys_nsegs; segind++) {
		sbuf_printf(&sbuf, "\nSEGMENT %d:\n\n", segind);
		seg = &vm_phys_segs[segind];
		sbuf_printf(&sbuf, "start:     %#jx\n",
		    (uintmax_t)seg->start);
		sbuf_printf(&sbuf, "end:       %#jx\n",
		    (uintmax_t)seg->end);
		sbuf_printf(&sbuf, "free list: %p\n", seg->free_queues);
	}
	sbuf_finish(&sbuf);
	error = SYSCTL_OUT(req, sbuf_data(&sbuf), sbuf_len(&sbuf));
	sbuf_delete(&sbuf);
	free(cbuf, M_TEMP);
	return (error);
}

/*
 * Create a physical memory segment.
 */
static void
vm_phys_create_seg(vm_paddr_t start, vm_paddr_t end, int flind)
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
	seg = &vm_phys_segs[vm_phys_nsegs++];
	seg->start = start;
	seg->end = end;
#ifdef VM_PHYSSEG_SPARSE
	seg->first_page = &vm_page_array[pages];
#else
	seg->first_page = PHYS_TO_VM_PAGE(start);
#endif
	seg->free_queues = &vm_phys_free_queues[flind];
}

/*
 * Initialize the physical memory allocator.
 */
void
vm_phys_init(void)
{
	struct vm_freelist *fl;
	int flind, i, oind, pind;

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
	for (flind = 0; flind < vm_nfreelists; flind++) {
		for (pind = 0; pind < VM_NFREEPOOL; pind++) {
			fl = vm_phys_free_queues[flind][pind];
			for (oind = 0; oind < VM_NFREEORDER; oind++)
				TAILQ_INIT(&fl[oind].pl);
		}
	}
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
		m_buddy->order = oind;
		TAILQ_INSERT_HEAD(&fl[oind].pl, m_buddy, pageq);
		fl[oind].lcnt++;
        }
}

/*
 * Initialize a physical page and add it to the free lists.
 */
void
vm_phys_add_page(vm_paddr_t pa)
{
	vm_page_t m;

	cnt.v_page_count++;
	m = vm_phys_paddr_to_vm_page(pa);
	m->phys_addr = pa;
	m->segind = vm_phys_paddr_to_segind(pa);
	m->flags = PG_FREE;
	KASSERT(m->order == VM_NFREEORDER,
	    ("vm_phys_add_page: page %p has unexpected order %d",
	    m, m->order));
	m->pool = VM_FREEPOOL_DEFAULT;
	pmap_page_init(m);
	vm_phys_free_pages(m, 0);
}

/*
 * Allocate a contiguous, power of two-sized set of physical pages
 * from the free lists.
 */
vm_page_t
vm_phys_alloc_pages(int pool, int order)
{
	vm_page_t m;

	mtx_lock(&vm_page_queue_free_mtx);
	m = vm_phys_alloc_pages_locked(pool, order);
	mtx_unlock(&vm_page_queue_free_mtx);
	return (m);
}

/*
 * Allocate a contiguous, power of two-sized set of physical pages
 * from the free lists.
 */
vm_page_t
vm_phys_alloc_pages_locked(int pool, int order)
{
	struct vm_freelist *fl;
	struct vm_freelist *alt;
	int flind, oind, pind;
	vm_page_t m;

	KASSERT(pool < VM_NFREEPOOL,
	    ("vm_phys_alloc_pages_locked: pool %d is out of range", pool));
	KASSERT(order < VM_NFREEORDER,
	    ("vm_phys_alloc_pages_locked: order %d is out of range", order));
	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	for (flind = 0; flind < vm_nfreelists; flind++) {
		fl = vm_phys_free_queues[flind][pool];
		for (oind = order; oind < VM_NFREEORDER; oind++) {
			m = TAILQ_FIRST(&fl[oind].pl);
			if (m != NULL) {
				TAILQ_REMOVE(&fl[oind].pl, m, pageq);
				fl[oind].lcnt--;
				m->order = VM_NFREEORDER;
				vm_phys_split_pages(m, oind, fl, order);
				cnt.v_free_count -= 1 << order;
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
				alt = vm_phys_free_queues[flind][pind];
				m = TAILQ_FIRST(&alt[oind].pl);
				if (m != NULL) {
					TAILQ_REMOVE(&alt[oind].pl, m, pageq);
					alt[oind].lcnt--;
					m->order = VM_NFREEORDER;
					vm_phys_set_pool(pool, m, oind);
					vm_phys_split_pages(m, oind, fl, order);
					cnt.v_free_count -= 1 << order;
					return (m);
				}
			}
		}
	}
	return (NULL);
}

/*
 * Allocate physical memory from phys_avail[].
 */
vm_paddr_t
vm_phys_bootstrap_alloc(vm_size_t size, unsigned long alignment)
{
	vm_paddr_t pa;
	int i;

	size = round_page(size);
	for (i = 0; phys_avail[i + 1] != 0; i += 2) {
		if (phys_avail[i + 1] - phys_avail[i] < size)
			continue;
		pa = phys_avail[i];
		phys_avail[i] += size;
		return (pa);
	}
	panic("vm_phys_bootstrap_alloc");
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
	panic("vm_phys_paddr_to_vm_page: paddr %#jx is not in any segment",
	    (uintmax_t)pa);
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
 */
void
vm_phys_free_pages(vm_page_t m, int order)
{

	mtx_lock(&vm_page_queue_free_mtx);
	vm_phys_free_pages_locked(m, order);
	mtx_unlock(&vm_page_queue_free_mtx);
}

/*
 * Free a contiguous, power of two-sized set of physical pages.
 */
void
vm_phys_free_pages_locked(vm_page_t m, int order)
{
	struct vm_freelist *fl;
	struct vm_phys_seg *seg;
	vm_paddr_t pa, pa_buddy;
	vm_page_t m_buddy;

	KASSERT(m->order == VM_NFREEORDER,
	    ("vm_phys_free_pages_locked: page %p has unexpected order %d",
	    m, m->order));
	KASSERT(m->pool < VM_NFREEPOOL,
	    ("vm_phys_free_pages_locked: page %p has unexpected pool %d",
	    m, m->pool));
	KASSERT(order < VM_NFREEORDER,
	    ("vm_phys_free_pages_locked: order %d is out of range", order));
	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	pa = VM_PAGE_TO_PHYS(m);
	seg = &vm_phys_segs[m->segind];
	cnt.v_free_count += 1 << order;
	while (order < VM_NFREEORDER - 1) {
		pa_buddy = pa ^ (1 << (PAGE_SHIFT + order));
		if (pa_buddy < seg->start ||
		    pa_buddy >= seg->end)
			break;
		m_buddy = &seg->first_page[atop(pa_buddy - seg->start)];
		if (m_buddy->order != order)
			break;
		fl = (*seg->free_queues)[m_buddy->pool];
		TAILQ_REMOVE(&fl[m_buddy->order].pl, m_buddy, pageq);
		fl[m_buddy->order].lcnt--;
		m_buddy->order = VM_NFREEORDER;
		if (m_buddy->pool != m->pool)
			vm_phys_set_pool(m->pool, m_buddy, order);
		order++;
		pa &= ~((1 << (PAGE_SHIFT + order)) - 1);
		m = &seg->first_page[atop(pa - seg->start)];
	}
	m->order = order;
	fl = (*seg->free_queues)[m->pool];
	TAILQ_INSERT_TAIL(&fl[order].pl, m, pageq);
	fl[order].lcnt++;
}

/*
 * Set the pool for a contiguous, power of two-sized set of physical pages. 
 */
static void
vm_phys_set_pool(int pool, vm_page_t m, int order)
{
	vm_page_t m_tmp;

	for (m_tmp = m; m_tmp < &m[1 << order]; m_tmp++)
		m_tmp->pool = pool;
}

/*
 * Try to zero one or more physical pages.  Used by an idle priority thread.
 */
boolean_t
vm_phys_zero_pages_idle(void)
{
	struct vm_freelist *fl;
	vm_page_t m, m_tmp;
	int flind, pind, q, zeroed;

	mtx_assert(&vm_page_queue_free_mtx, MA_OWNED);
	for (flind = 0; flind < vm_nfreelists; flind++) {
		pind = VM_FREEPOOL_DEFAULT;
		fl = vm_phys_free_queues[flind][pind];
		for (q = 0; q < VM_NFREEORDER; q++) {
			m = TAILQ_FIRST(&fl[q].pl);
			if (m != NULL && (m->flags & PG_ZERO) == 0) {
				TAILQ_REMOVE(&fl[q].pl, m, pageq);
				fl[q].lcnt--;
				m->order = VM_NFREEORDER;
				cnt.v_free_count -= 1 << q;
				mtx_unlock(&vm_page_queue_free_mtx);
				zeroed = 0;
				for (m_tmp = m; m_tmp < &m[1 << q]; m_tmp++) {
					if ((m_tmp->flags & PG_ZERO) == 0) {
						pmap_zero_page_idle(m_tmp);
						m_tmp->flags |= PG_ZERO;
						zeroed++;
					}
				}
				cnt_prezero += zeroed;
				mtx_lock(&vm_page_queue_free_mtx);
				vm_phys_free_pages_locked(m, q);
				vm_page_zero_count += zeroed;
				return (TRUE);
			}
		}
	}
	return (FALSE);
}

/*
 * Allocate a contiguous set of physical pages of the given size from
 * the free lists.  All of the physical pages must be at or above the
 * given physical address "low" and below the given physical address
 * "high".  If the given value "alignment" is non-zero, then the
 * lowest page in the set must be aligned to that value.  If the given
 * value "boundary" is non-zero, then the set of physical pages cannot
 * cross any boundary that is a multiple of that value.  Both
 * "alignment" and "boundary" must be a power of two.
 */
vm_page_t
vm_phys_alloc_contig(unsigned long npages, vm_paddr_t low, vm_paddr_t high,
    unsigned long alignment, unsigned long boundary)
{
	struct vm_freelist *fl;
	struct vm_phys_seg *seg;
	vm_paddr_t pa, pa_last, size;
	vm_page_t m, m_ret;
	int flind, i, oind, order, pind;

	size = npages << PAGE_SHIFT;
	KASSERT(size != 0,
	    ("vm_phys_alloc_contig: size must not be 0"));
	KASSERT((alignment & (alignment - 1)) == 0,
	    ("vm_phys_alloc_contig: alignment must be a power of 2"));
	KASSERT((boundary & (boundary - 1)) == 0,
	    ("vm_phys_alloc_contig: boundary must be a power of 2"));
	/* Compute the queue that is the best fit for npages. */
	for (order = 0; (1 << order) < npages; order++);
	mtx_lock(&vm_page_queue_free_mtx);
	for (flind = 0; flind < vm_nfreelists; flind++) {
		for (oind = min(order, VM_NFREEORDER - 1); oind < VM_NFREEORDER; oind++) {
			for (pind = 0; pind < VM_NFREEPOOL; pind++) {
				fl = vm_phys_free_queues[flind][pind];
				TAILQ_FOREACH(m_ret, &fl[oind].pl, pageq) {
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
	mtx_unlock(&vm_page_queue_free_mtx);
	return (NULL);
done:
	for (m = m_ret; m < &m_ret[npages]; m = &m[1 << oind]) {
		fl = (*seg->free_queues)[m->pool];
		TAILQ_REMOVE(&fl[m->order].pl, m, pageq);
		fl[m->order].lcnt--;
		m->order = VM_NFREEORDER;
	}
	if (m_ret->pool != VM_FREEPOOL_DEFAULT)
		vm_phys_set_pool(VM_FREEPOOL_DEFAULT, m_ret, oind);
	fl = (*seg->free_queues)[m_ret->pool];
	vm_phys_split_pages(m_ret, oind, fl, order);
	cnt.v_free_count -= roundup2(npages, 1 << imin(oind, order));
	for (i = 0; i < npages; i++) {
		m = &m_ret[i];
		KASSERT(m->queue == PQ_NONE,
		    ("vm_phys_alloc_contig: page %p has unexpected queue %d",
		    m, m->queue));
		m->valid = VM_PAGE_BITS_ALL;
		if (m->flags & PG_ZERO)
			vm_page_zero_count--;
		/* Don't clear the PG_ZERO flag; we'll need it later. */
		m->flags = PG_UNMANAGED | (m->flags & PG_ZERO);
		m->oflags = 0;
		KASSERT(m->dirty == 0,
		    ("vm_phys_alloc_contig: page %p was dirty", m));
		m->wire_count = 0;
		m->busy = 0;
	}
	for (; i < roundup2(npages, 1 << imin(oind, order)); i++) {
		m = &m_ret[i];
		KASSERT(m->order == VM_NFREEORDER,
		    ("vm_phys_alloc_contig: page %p has unexpected order %d",
		    m, m->order));
		vm_phys_free_pages_locked(m, 0);
	}
	mtx_unlock(&vm_page_queue_free_mtx);
	return (m_ret);
}

#ifdef DDB
/*
 * Show the number of physical pages in each of the free lists.
 */
DB_SHOW_COMMAND(freepages, db_show_freepages)
{
	struct vm_freelist *fl;
	int flind, oind, pind;

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
				fl = vm_phys_free_queues[flind][pind];
				db_printf("  |  %6.6d", fl[oind].lcnt);
			}
			db_printf("\n");
		}
		db_printf("\n");
	}
}
#endif
