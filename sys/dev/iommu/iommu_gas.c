/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define	RB_AUGMENT_CHECK(entry) iommu_gas_augment_entry(entry)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/memdesc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <sys/uio.h>
#include <sys/vmem.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/uma.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/iommu/iommu.h>
#include <dev/iommu/iommu_gas.h>
#include <dev/iommu/iommu_msi.h>
#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/iommu.h>
#include <dev/iommu/busdma_iommu.h>

/*
 * Guest Address Space management.
 */

static uma_zone_t iommu_map_entry_zone;

#ifdef INVARIANTS
static int iommu_check_free;
#endif

static void
intel_gas_init(void)
{

	iommu_map_entry_zone = uma_zcreate("IOMMU_MAP_ENTRY",
	    sizeof(struct iommu_map_entry), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_NODUMP);
}
SYSINIT(intel_gas, SI_SUB_DRIVERS, SI_ORDER_FIRST, intel_gas_init, NULL);

struct iommu_map_entry *
iommu_gas_alloc_entry(struct iommu_domain *domain, u_int flags)
{
	struct iommu_map_entry *res;

	KASSERT((flags & ~(IOMMU_PGF_WAITOK)) == 0,
	    ("unsupported flags %x", flags));

	res = uma_zalloc(iommu_map_entry_zone, ((flags & IOMMU_PGF_WAITOK) !=
	    0 ? M_WAITOK : M_NOWAIT) | M_ZERO);
	if (res != NULL) {
		SLIST_INIT(&res->pgtbl_free);
		if (domain != NULL) {
			res->domain = domain;
			atomic_add_int(&domain->entries_cnt, 1);
		}
	}
	return (res);
}

void
iommu_gas_free_entry(struct iommu_map_entry *entry)
{
	struct iommu_domain *domain;
	int n __unused;

	n = vm_page_free_pages_toq(&entry->pgtbl_free, false);
#if defined(__i386__) || defined(__amd64__)
	atomic_subtract_int(&iommu_tbl_pagecnt, n);
#endif
	domain = entry->domain;
	if (domain != NULL)
		atomic_subtract_int(&domain->entries_cnt, 1);
	uma_zfree(iommu_map_entry_zone, entry);
}

static int
iommu_gas_cmp_entries(struct iommu_map_entry *a, struct iommu_map_entry *b)
{

	/* First and last entries have zero size, so <= */
	KASSERT(a->start <= a->end, ("inverted entry %p (%jx, %jx)",
	    a, (uintmax_t)a->start, (uintmax_t)a->end));
	KASSERT(b->start <= b->end, ("inverted entry %p (%jx, %jx)",
	    b, (uintmax_t)b->start, (uintmax_t)b->end));
	KASSERT(((a->flags | b->flags) & IOMMU_MAP_ENTRY_FAKE) != 0 ||
	    a->end <= b->start || b->end <= a->start ||
	    a->end == a->start || b->end == b->start,
	    ("overlapping entries %p (%jx, %jx) f %#x %p (%jx, %jx) f %#x"
	    " domain %p %p",
	    a, (uintmax_t)a->start, (uintmax_t)a->end, a->flags,
	    b, (uintmax_t)b->start, (uintmax_t)b->end, b->flags,
	    a->domain, b->domain));

	if (a->end < b->end)
		return (-1);
	else if (b->end < a->end)
		return (1);
	return (0);
}

/*
 * Update augmentation data based on data from children.
 * Return true if and only if the update changes the augmentation data.
 */
static bool
iommu_gas_augment_entry(struct iommu_map_entry *entry)
{
	struct iommu_map_entry *child;
	iommu_gaddr_t bound, delta, free_down;

	free_down = 0;
	bound = entry->start;
	if ((child = RB_LEFT(entry, rb_entry)) != NULL) {
		free_down = MAX(child->free_down, bound - child->last);
		bound = child->first;
	}
	delta = bound - entry->first;
	entry->first = bound;
	bound = entry->end;
	if ((child = RB_RIGHT(entry, rb_entry)) != NULL) {
		free_down = MAX(free_down, child->free_down);
		free_down = MAX(free_down, child->first - bound);
		bound = child->last;
	}
	delta += entry->last - bound;
	if (delta == 0)
		delta = entry->free_down - free_down;
	entry->last = bound;
	entry->free_down = free_down;

	/*
	 * Return true either if the value of last-first changed,
	 * or if free_down changed.
	 */
	return (delta != 0);
}

RB_GENERATE(iommu_gas_entries_tree, iommu_map_entry, rb_entry,
    iommu_gas_cmp_entries);

#ifdef INVARIANTS
static void
iommu_gas_check_free(struct iommu_domain *domain)
{
	struct iommu_map_entry *entry, *l, *r;
	iommu_gaddr_t v;

	RB_FOREACH(entry, iommu_gas_entries_tree, &domain->rb_root) {
		KASSERT(domain == entry->domain,
		    ("mismatched free domain %p entry %p entry->domain %p",
		    domain, entry, entry->domain));
		l = RB_LEFT(entry, rb_entry);
		r = RB_RIGHT(entry, rb_entry);
		v = 0;
		if (l != NULL) {
			v = MAX(v, l->free_down);
			v = MAX(v, entry->start - l->last);
		}
		if (r != NULL) {
			v = MAX(v, r->free_down);
			v = MAX(v, r->first - entry->end);
		}
		MPASS(entry->free_down == v);
	}
}
#endif

static void
iommu_gas_rb_remove(struct iommu_domain *domain, struct iommu_map_entry *entry)
{
	struct iommu_map_entry *nbr;

	/* Removing entry may open a new free gap before domain->start_gap. */
	if (entry->end <= domain->start_gap->end) {
		if (RB_RIGHT(entry, rb_entry) != NULL)
			nbr = iommu_gas_entries_tree_RB_NEXT(entry);
		else if (RB_LEFT(entry, rb_entry) != NULL)
			nbr = RB_LEFT(entry, rb_entry);
		else
			nbr = RB_PARENT(entry, rb_entry);
		domain->start_gap = nbr;
	}
	RB_REMOVE(iommu_gas_entries_tree, &domain->rb_root, entry);
}

struct iommu_domain *
iommu_get_ctx_domain(struct iommu_ctx *ctx)
{

	return (ctx->domain);
}

void
iommu_gas_init_domain(struct iommu_domain *domain)
{
	struct iommu_map_entry *begin, *end;

	begin = iommu_gas_alloc_entry(domain, IOMMU_PGF_WAITOK);
	end = iommu_gas_alloc_entry(domain, IOMMU_PGF_WAITOK);

	IOMMU_DOMAIN_LOCK(domain);
	KASSERT(domain->entries_cnt == 2, ("dirty domain %p", domain));
	KASSERT(RB_EMPTY(&domain->rb_root),
	    ("non-empty entries %p", domain));

	end->start = domain->end;
	end->end = domain->end;
	end->flags = IOMMU_MAP_ENTRY_PLACE | IOMMU_MAP_ENTRY_UNMAPPED;
	RB_INSERT(iommu_gas_entries_tree, &domain->rb_root, end);

	begin->start = 0;
	begin->end = 0;
	begin->flags = IOMMU_MAP_ENTRY_PLACE | IOMMU_MAP_ENTRY_UNMAPPED;
	RB_INSERT_PREV(iommu_gas_entries_tree, &domain->rb_root, end, begin);
	iommu_gas_augment_entry(end);
	iommu_gas_augment_entry(begin);

	domain->start_gap = begin;
	domain->first_place = begin;
	domain->last_place = end;
	domain->flags |= IOMMU_DOMAIN_GAS_INITED;
	IOMMU_DOMAIN_UNLOCK(domain);
}

void
iommu_gas_fini_domain(struct iommu_domain *domain)
{
	struct iommu_map_entry *entry;

	IOMMU_DOMAIN_ASSERT_LOCKED(domain);
	KASSERT(domain->entries_cnt == 2,
	    ("domain still in use %p", domain));

	entry = RB_MIN(iommu_gas_entries_tree, &domain->rb_root);
	KASSERT(entry->start == 0, ("start entry start %p", domain));
	KASSERT(entry->end == IOMMU_PAGE_SIZE, ("start entry end %p", domain));
	KASSERT(entry->flags ==
	    (IOMMU_MAP_ENTRY_PLACE | IOMMU_MAP_ENTRY_UNMAPPED),
	    ("start entry flags %p", domain));
	iommu_gas_rb_remove(domain, entry);
	iommu_gas_free_entry(entry);

	entry = RB_MAX(iommu_gas_entries_tree, &domain->rb_root);
	KASSERT(entry->start == domain->end, ("end entry start %p", domain));
	KASSERT(entry->end == domain->end, ("end entry end %p", domain));
	KASSERT(entry->flags ==
	    (IOMMU_MAP_ENTRY_PLACE | IOMMU_MAP_ENTRY_UNMAPPED),
	    ("end entry flags %p", domain));
	iommu_gas_rb_remove(domain, entry);
	iommu_gas_free_entry(entry);
}

struct iommu_gas_match_args {
	iommu_gaddr_t size;
	int offset;
	const struct bus_dma_tag_common *common;
	u_int gas_flags;
	struct iommu_map_entry *entry;
};

/*
 * The interval [beg, end) is a free interval between two iommu_map_entries.
 * Addresses can be allocated only in the range [lbound, ubound]. Try to
 * allocate space in the free interval, subject to the conditions expressed by
 * a, and return 'true' if and only if the allocation attempt succeeds.
 */
static bool
iommu_gas_match_one(struct iommu_gas_match_args *a, iommu_gaddr_t beg,
    iommu_gaddr_t end, iommu_gaddr_t lbound, iommu_gaddr_t ubound)
{
	struct iommu_map_entry *entry;
	iommu_gaddr_t first, size, start;
	int offset;

	/*
	 * The prev->end is always aligned on the page size, which
	 * causes page alignment for the entry->start too.
	 *
	 * Create IOMMU_PAGE_SIZE gaps before, after new entry
	 * to ensure that out-of-bounds accesses fault.
	 */
	beg = MAX(beg + IOMMU_PAGE_SIZE, lbound);
	start = roundup2(beg, a->common->alignment);
	if (start < beg)
		return (false);
	if (end < IOMMU_PAGE_SIZE + 1)
		return (false);
	end = MIN(end - IOMMU_PAGE_SIZE - 1, ubound);
	offset = a->offset;
	size = a->size;
	if (start + offset + size - 1 > end)
		return (false);

	/* Check for and try to skip past boundary crossing. */
	if (!vm_addr_bound_ok(start + offset, size, a->common->boundary)) {
		/*
		 * The start + offset to start + offset + size region crosses
		 * the boundary.  Check if there is enough space after the next
		 * boundary after the beg.
		 */
		first = start;
		beg = roundup2(start + offset + 1, a->common->boundary);
		start = roundup2(beg, a->common->alignment);

		if (start + offset + size - 1 > end ||
		    !vm_addr_bound_ok(start + offset, size,
		    a->common->boundary)) {
			/*
			 * Not enough space to align at the requested boundary,
			 * or boundary is smaller than the size, but allowed to
			 * split.  We already checked that start + size does not
			 * overlap ubound.
			 *
			 * XXXKIB. It is possible that beg is exactly at the
			 * start of the next entry, then we do not have gap.
			 * Ignore for now.
			 */
			if ((a->gas_flags & IOMMU_MF_CANSPLIT) == 0)
				return (false);
			size = beg - first - offset;
			start = first;
		}
	}
	entry = a->entry;
	entry->start = start;
	entry->end = start + roundup2(size + offset, IOMMU_PAGE_SIZE);
	entry->flags = IOMMU_MAP_ENTRY_MAP;
	return (true);
}

/* Find the next entry that might abut a big-enough range. */
static struct iommu_map_entry *
iommu_gas_next(struct iommu_map_entry *curr, iommu_gaddr_t min_free)
{
	struct iommu_map_entry *next;

	if ((next = RB_RIGHT(curr, rb_entry)) != NULL &&
	    next->free_down >= min_free) {
		/* Find next entry in right subtree. */
		do
			curr = next;
		while ((next = RB_LEFT(curr, rb_entry)) != NULL &&
		    next->free_down >= min_free);
	} else {
		/* Find next entry in a left-parent ancestor. */
		while ((next = RB_PARENT(curr, rb_entry)) != NULL &&
		    curr == RB_RIGHT(next, rb_entry))
			curr = next;
		curr = next;
	}
	return (curr);
}

/*
 * Address-ordered first-fit search of 'domain' for free space satisfying the
 * conditions of 'a'.  The space allocated is at least one page big, and is
 * bounded by guard pages to the left and right.  The allocated space for
 * 'domain' is described by an rb-tree of map entries at domain->rb_root, and
 * domain->start_gap points to a map entry less than or adjacent to the first
 * free-space of size at least 3 pages.
 */
static int
iommu_gas_find_space(struct iommu_domain *domain,
    struct iommu_gas_match_args *a)
{
	struct iommu_map_entry *curr, *first;
	iommu_gaddr_t addr, min_free;

	IOMMU_DOMAIN_ASSERT_LOCKED(domain);
	KASSERT(a->entry->flags == 0,
	    ("dirty entry %p %p", domain, a->entry));

	/*
	 * start_gap may point to an entry adjacent to gaps too small for any
	 * new allocation.  In that case, advance start_gap to the first free
	 * space big enough for a minimum allocation plus two guard pages.
	 */
	min_free = 3 * IOMMU_PAGE_SIZE;
	first = domain->start_gap;
	while (first != NULL && first->free_down < min_free)
		first = RB_PARENT(first, rb_entry);
	for (curr = first; curr != NULL;
	    curr = iommu_gas_next(curr, min_free)) {
		if ((first = RB_LEFT(curr, rb_entry)) != NULL &&
		    first->last + min_free <= curr->start)
			break;
		if ((first = RB_RIGHT(curr, rb_entry)) != NULL &&
		    curr->end + min_free <= first->first)
			break;
	}
	domain->start_gap = curr;

	/*
	 * If the subtree doesn't have free space for the requested allocation
	 * plus two guard pages, skip it.
	 */
	min_free = 2 * IOMMU_PAGE_SIZE +
	    roundup2(a->size + a->offset, IOMMU_PAGE_SIZE);

	/* Climb to find a node in the subtree of big-enough ranges. */
	first = curr;
	while (first != NULL && first->free_down < min_free)
		first = RB_PARENT(first, rb_entry);

	/*
	 * Walk the big-enough ranges tree until one satisfies alignment
	 * requirements, or violates lowaddr address requirement.
	 */
	addr = a->common->lowaddr;
	for (curr = first; curr != NULL;
	    curr = iommu_gas_next(curr, min_free)) {
		if ((first = RB_LEFT(curr, rb_entry)) != NULL &&
		    iommu_gas_match_one(a, first->last, curr->start,
		    0, addr)) {
			RB_INSERT_PREV(iommu_gas_entries_tree,
			    &domain->rb_root, curr, a->entry);
			return (0);
		}
		if (curr->end >= addr) {
			/* All remaining ranges > addr */
			break;
		}
		if ((first = RB_RIGHT(curr, rb_entry)) != NULL &&
		    iommu_gas_match_one(a, curr->end, first->first,
		    0, addr)) {
			RB_INSERT_NEXT(iommu_gas_entries_tree,
			    &domain->rb_root, curr, a->entry);
			return (0);
		}
	}

	/*
	 * To resume the search at the start of the upper region, first climb to
	 * the nearest ancestor that spans highaddr.  Then find the last entry
	 * before highaddr that could abut a big-enough range.
	 */
	addr = a->common->highaddr;
	while (curr != NULL && curr->last < addr)
		curr = RB_PARENT(curr, rb_entry);
	first = NULL;
	while (curr != NULL && curr->free_down >= min_free) {
		if (addr < curr->end)
			curr = RB_LEFT(curr, rb_entry);
		else {
			first = curr;
			curr = RB_RIGHT(curr, rb_entry);
		}
	}

	/*
	 * Walk the remaining big-enough ranges until one satisfies alignment
	 * requirements.
	 */
	for (curr = first; curr != NULL;
	    curr = iommu_gas_next(curr, min_free)) {
		if ((first = RB_LEFT(curr, rb_entry)) != NULL &&
		    iommu_gas_match_one(a, first->last, curr->start,
		    addr + 1, domain->end - 1)) {
			RB_INSERT_PREV(iommu_gas_entries_tree,
			    &domain->rb_root, curr, a->entry);
			return (0);
		}
		if ((first = RB_RIGHT(curr, rb_entry)) != NULL &&
		    iommu_gas_match_one(a, curr->end, first->first,
		    addr + 1, domain->end - 1)) {
			RB_INSERT_NEXT(iommu_gas_entries_tree,
			    &domain->rb_root, curr, a->entry);
			return (0);
		}
	}

	return (ENOMEM);
}

static int
iommu_gas_alloc_region(struct iommu_domain *domain, struct iommu_map_entry *entry,
    u_int flags)
{
	struct iommu_map_entry *next, *prev;

	IOMMU_DOMAIN_ASSERT_LOCKED(domain);

	if ((entry->start & IOMMU_PAGE_MASK) != 0 ||
	    (entry->end & IOMMU_PAGE_MASK) != 0)
		return (EINVAL);
	if (entry->start >= entry->end)
		return (EINVAL);
	if (entry->end >= domain->end)
		return (EINVAL);

	entry->flags |= IOMMU_MAP_ENTRY_FAKE;
	next = RB_NFIND(iommu_gas_entries_tree, &domain->rb_root, entry);
	KASSERT(next != NULL, ("next must be non-null %p %jx", domain,
	    (uintmax_t)entry->start));
	prev = RB_PREV(iommu_gas_entries_tree, &domain->rb_root, next);
	/* prev could be NULL */
	entry->flags &= ~IOMMU_MAP_ENTRY_FAKE;

	/*
	 * Adapt to broken BIOSes which specify overlapping RMRR
	 * entries.
	 *
	 * XXXKIB: this does not handle a case when prev or next
	 * entries are completely covered by the current one, which
	 * extends both ways.
	 */
	if (prev != NULL && prev->end > entry->start &&
	    (prev->flags & IOMMU_MAP_ENTRY_PLACE) == 0) {
		if ((flags & IOMMU_MF_RMRR) == 0 ||
		    (prev->flags & IOMMU_MAP_ENTRY_RMRR) == 0)
			return (EBUSY);
		entry->start = prev->end;
	}
	if (next->start < entry->end &&
	    (next->flags & IOMMU_MAP_ENTRY_PLACE) == 0) {
		if ((flags & IOMMU_MF_RMRR) == 0 ||
		    (next->flags & IOMMU_MAP_ENTRY_RMRR) == 0)
			return (EBUSY);
		entry->end = next->start;
	}
	if (entry->end == entry->start)
		return (0);

	if (prev != NULL && prev->end > entry->start) {
		/* This assumes that prev is the placeholder entry. */
		iommu_gas_rb_remove(domain, prev);
		prev = NULL;
	}
	RB_INSERT_PREV(iommu_gas_entries_tree,
	    &domain->rb_root, next, entry);
	if (next->start < entry->end) {
		iommu_gas_rb_remove(domain, next);
		next = NULL;
	}

	if ((flags & IOMMU_MF_RMRR) != 0)
		entry->flags = IOMMU_MAP_ENTRY_RMRR;

#ifdef INVARIANTS
	struct iommu_map_entry *ip, *in;
	ip = RB_PREV(iommu_gas_entries_tree, &domain->rb_root, entry);
	in = RB_NEXT(iommu_gas_entries_tree, &domain->rb_root, entry);
	KASSERT(prev == NULL || ip == prev,
	    ("RMRR %p (%jx %jx) prev %p (%jx %jx) ins prev %p (%jx %jx)",
	    entry, entry->start, entry->end, prev,
	    prev == NULL ? 0 : prev->start, prev == NULL ? 0 : prev->end,
	    ip, ip == NULL ? 0 : ip->start, ip == NULL ? 0 : ip->end));
	KASSERT(next == NULL || in == next,
	    ("RMRR %p (%jx %jx) next %p (%jx %jx) ins next %p (%jx %jx)",
	    entry, entry->start, entry->end, next,
	    next == NULL ? 0 : next->start, next == NULL ? 0 : next->end,
	    in, in == NULL ? 0 : in->start, in == NULL ? 0 : in->end));
#endif

	return (0);
}

void
iommu_gas_free_space(struct iommu_map_entry *entry)
{
	struct iommu_domain *domain;

	domain = entry->domain;
	KASSERT((entry->flags & (IOMMU_MAP_ENTRY_PLACE | IOMMU_MAP_ENTRY_RMRR |
	    IOMMU_MAP_ENTRY_MAP)) == IOMMU_MAP_ENTRY_MAP,
	    ("permanent entry %p %p", domain, entry));

	IOMMU_DOMAIN_LOCK(domain);
	iommu_gas_rb_remove(domain, entry);
	entry->flags &= ~IOMMU_MAP_ENTRY_MAP;
#ifdef INVARIANTS
	if (iommu_check_free)
		iommu_gas_check_free(domain);
#endif
	IOMMU_DOMAIN_UNLOCK(domain);
}

void
iommu_gas_free_region(struct iommu_map_entry *entry)
{
	struct iommu_domain *domain;

	domain = entry->domain;
	KASSERT((entry->flags & (IOMMU_MAP_ENTRY_PLACE | IOMMU_MAP_ENTRY_RMRR |
	    IOMMU_MAP_ENTRY_MAP)) == IOMMU_MAP_ENTRY_RMRR,
	    ("non-RMRR entry %p %p", domain, entry));

	IOMMU_DOMAIN_LOCK(domain);
	if (entry != domain->first_place &&
	    entry != domain->last_place)
		iommu_gas_rb_remove(domain, entry);
	entry->flags &= ~IOMMU_MAP_ENTRY_RMRR;
	IOMMU_DOMAIN_UNLOCK(domain);
}

static struct iommu_map_entry *
iommu_gas_remove_clip_left(struct iommu_domain *domain, iommu_gaddr_t start,
    iommu_gaddr_t end, struct iommu_map_entry **r)
{
	struct iommu_map_entry *entry, *res, fentry;

	IOMMU_DOMAIN_ASSERT_LOCKED(domain);
	MPASS(start <= end);
	MPASS(end <= domain->end);

	/*
	 * Find an entry which contains the supplied guest's address
	 * start, or the first entry after the start.  Since we
	 * asserted that start is below domain end, entry should
	 * exist.  Then clip it if needed.
	 */
	bzero(&fentry, sizeof(fentry));
	fentry.start = start + 1;
	fentry.end = start + 1;
	fentry.flags = IOMMU_MAP_ENTRY_FAKE;
	entry = RB_NFIND(iommu_gas_entries_tree, &domain->rb_root, &fentry);

	if (entry->start >= start ||
	    (entry->flags & IOMMU_MAP_ENTRY_RMRR) != 0)
		return (entry);

	res = *r;
	*r = NULL;
	*res = *entry;
	res->start = entry->end = start;
	RB_UPDATE_AUGMENT(entry, rb_entry);
	RB_INSERT_NEXT(iommu_gas_entries_tree,
	    &domain->rb_root, entry, res);
	return (res);
}

static bool
iommu_gas_remove_clip_right(struct iommu_domain *domain,
    iommu_gaddr_t end, struct iommu_map_entry *entry,
    struct iommu_map_entry *r)
{
	if (entry->start >= end || (entry->flags & IOMMU_MAP_ENTRY_RMRR) != 0)
		return (false);

	*r = *entry;
	r->end = entry->start = end;
	RB_UPDATE_AUGMENT(entry, rb_entry);
	RB_INSERT_PREV(iommu_gas_entries_tree,
	    &domain->rb_root, entry, r);
	return (true);
}

static void
iommu_gas_remove_unmap(struct iommu_domain *domain,
    struct iommu_map_entry *entry, struct iommu_map_entries_tailq *gcp)
{
	IOMMU_DOMAIN_ASSERT_LOCKED(domain);

	if ((entry->flags & (IOMMU_MAP_ENTRY_UNMAPPED |
	    IOMMU_MAP_ENTRY_RMRR |
	    IOMMU_MAP_ENTRY_REMOVING)) != 0)
		return;
	MPASS((entry->flags & IOMMU_MAP_ENTRY_PLACE) == 0);
	entry->flags |= IOMMU_MAP_ENTRY_REMOVING;
	TAILQ_INSERT_TAIL(gcp, entry, dmamap_link);
}

static void
iommu_gas_remove_locked(struct iommu_domain *domain,
    iommu_gaddr_t start, iommu_gaddr_t size,
    struct iommu_map_entries_tailq *gc,
    struct iommu_map_entry **r1, struct iommu_map_entry **r2)
{
	struct iommu_map_entry *entry, *nentry;
	iommu_gaddr_t end;

	IOMMU_DOMAIN_ASSERT_LOCKED(domain);

	end = start + size;

	nentry = iommu_gas_remove_clip_left(domain, start, end, r1);
	RB_FOREACH_FROM(entry, iommu_gas_entries_tree, nentry) {
		if (entry->start >= end)
			break;
		KASSERT(start <= entry->start,
		    ("iommu_gas_remove entry (%#jx, %#jx) start %#jx",
		    entry->start, entry->end, start));
		iommu_gas_remove_unmap(domain, entry, gc);
	}
	if (iommu_gas_remove_clip_right(domain, end, entry, *r2)) {
		iommu_gas_remove_unmap(domain, *r2, gc);
		*r2 = NULL;
	}

#ifdef INVARIANTS
	RB_FOREACH(entry, iommu_gas_entries_tree, &domain->rb_root) {
		if ((entry->flags & (IOMMU_MAP_ENTRY_RMRR |
		    IOMMU_MAP_ENTRY_PLACE)) != 0)
			continue;
		KASSERT(entry->end <= start || entry->start >= end,
		    ("iommu_gas_remove leftover entry (%#jx, %#jx) range "
		    "(%#jx, %#jx)",
		    entry->start, entry->end, start, end));
	}
#endif
}

static void
iommu_gas_remove_init(struct iommu_domain *domain,
    struct iommu_map_entries_tailq *gc, struct iommu_map_entry **r1,
    struct iommu_map_entry **r2)
{
	TAILQ_INIT(gc);
	*r1 = iommu_gas_alloc_entry(domain, IOMMU_PGF_WAITOK);
	*r2 = iommu_gas_alloc_entry(domain, IOMMU_PGF_WAITOK);
}

static void
iommu_gas_remove_cleanup(struct iommu_domain *domain,
    struct iommu_map_entries_tailq *gc, struct iommu_map_entry **r1,
    struct iommu_map_entry **r2)
{
	if (*r1 != NULL) {
		iommu_gas_free_entry(*r1);
		*r1 = NULL;
	}
	if (*r2 != NULL) {
		iommu_gas_free_entry(*r2);
		*r2 = NULL;
	}
	iommu_domain_unload(domain, gc, true);
}

/*
 * Remove specified range from the GAS of the domain.  Note that the
 * removal is not guaranteed to occur upon the function return, it
 * might be finalized some time after, when hardware reports that
 * (queued) IOTLB invalidation was performed.
 */
void
iommu_gas_remove(struct iommu_domain *domain, iommu_gaddr_t start,
    iommu_gaddr_t size)
{
	struct iommu_map_entry *r1, *r2;
	struct iommu_map_entries_tailq gc;

	iommu_gas_remove_init(domain, &gc, &r1, &r2);
	IOMMU_DOMAIN_LOCK(domain);
	iommu_gas_remove_locked(domain, start, size, &gc, &r1, &r2);
	IOMMU_DOMAIN_UNLOCK(domain);
	iommu_gas_remove_cleanup(domain, &gc, &r1, &r2);
}

int
iommu_gas_map(struct iommu_domain *domain,
    const struct bus_dma_tag_common *common, iommu_gaddr_t size, int offset,
    u_int eflags, u_int flags, vm_page_t *ma, struct iommu_map_entry **res)
{
	struct iommu_gas_match_args a;
	struct iommu_map_entry *entry;
	int error;

	KASSERT((flags & ~(IOMMU_MF_CANWAIT | IOMMU_MF_CANSPLIT)) == 0,
	    ("invalid flags 0x%x", flags));

	a.size = size;
	a.offset = offset;
	a.common = common;
	a.gas_flags = flags;
	entry = iommu_gas_alloc_entry(domain,
	    (flags & IOMMU_MF_CANWAIT) != 0 ? IOMMU_PGF_WAITOK : 0);
	if (entry == NULL)
		return (ENOMEM);
	a.entry = entry;
	IOMMU_DOMAIN_LOCK(domain);
	error = iommu_gas_find_space(domain, &a);
	if (error == ENOMEM) {
		IOMMU_DOMAIN_UNLOCK(domain);
		iommu_gas_free_entry(entry);
		return (error);
	}
#ifdef INVARIANTS
	if (iommu_check_free)
		iommu_gas_check_free(domain);
#endif
	KASSERT(error == 0,
	    ("unexpected error %d from iommu_gas_find_entry", error));
	KASSERT(entry->end < domain->end, ("allocated GPA %jx, max GPA %jx",
	    (uintmax_t)entry->end, (uintmax_t)domain->end));
	entry->flags |= eflags;
	IOMMU_DOMAIN_UNLOCK(domain);

	error = domain->ops->map(domain, entry, ma, eflags,
	    ((flags & IOMMU_MF_CANWAIT) != 0 ? IOMMU_PGF_WAITOK : 0));
	if (error == ENOMEM) {
		iommu_domain_unload_entry(entry, true,
		    (flags & IOMMU_MF_CANWAIT) != 0);
		return (error);
	}
	KASSERT(error == 0,
	    ("unexpected error %d from domain_map_buf", error));

	*res = entry;
	return (0);
}

int
iommu_gas_map_region(struct iommu_domain *domain, struct iommu_map_entry *entry,
    u_int eflags, u_int flags, vm_page_t *ma)
{
	iommu_gaddr_t start;
	int error;

	KASSERT(entry->domain == domain,
	    ("mismatched domain %p entry %p entry->domain %p", domain,
	    entry, entry->domain));
	KASSERT(entry->flags == 0, ("used RMRR entry %p %p %x", domain,
	    entry, entry->flags));
	KASSERT((flags & ~(IOMMU_MF_CANWAIT | IOMMU_MF_RMRR)) == 0,
	    ("invalid flags 0x%x", flags));

	start = entry->start;
	IOMMU_DOMAIN_LOCK(domain);
	error = iommu_gas_alloc_region(domain, entry, flags);
	if (error != 0) {
		IOMMU_DOMAIN_UNLOCK(domain);
		return (error);
	}
	entry->flags |= eflags;
	IOMMU_DOMAIN_UNLOCK(domain);
	if (entry->end == entry->start)
		return (0);

	/*
	 * iommu_gas_alloc_region() might clipped the entry start and
	 * end positions.  Adjust the beginning of the ma array to map
	 * the pages at the requested relative positions.
	 */
	error = domain->ops->map(domain, entry,
	    ma + OFF_TO_IDX(start - entry->start), eflags,
	    ((flags & IOMMU_MF_CANWAIT) != 0 ? IOMMU_PGF_WAITOK : 0));
	if (error == ENOMEM) {
		iommu_domain_unload_entry(entry, false,
		    (flags & IOMMU_MF_CANWAIT) != 0);
		return (error);
	}
	KASSERT(error == 0,
	    ("unexpected error %d from domain_map_buf", error));

	return (0);
}

static int
iommu_gas_reserve_region_locked(struct iommu_domain *domain,
    iommu_gaddr_t start, iommu_gaddr_t end, struct iommu_map_entry *entry)
{
	int error;

	IOMMU_DOMAIN_ASSERT_LOCKED(domain);

	entry->start = start;
	entry->end = end;
	error = iommu_gas_alloc_region(domain, entry, IOMMU_MF_CANWAIT);
	if (error == 0)
		entry->flags |= IOMMU_MAP_ENTRY_UNMAPPED;
	return (error);
}

int
iommu_gas_reserve_region(struct iommu_domain *domain, iommu_gaddr_t start,
    iommu_gaddr_t end, struct iommu_map_entry **entry0)
{
	struct iommu_map_entry *entry;
	int error;

	entry = iommu_gas_alloc_entry(domain, IOMMU_PGF_WAITOK);
	IOMMU_DOMAIN_LOCK(domain);
	error = iommu_gas_reserve_region_locked(domain, start, end, entry);
	IOMMU_DOMAIN_UNLOCK(domain);
	if (error != 0)
		iommu_gas_free_entry(entry);
	else if (entry0 != NULL)
		*entry0 = entry;
	return (error);
}

/*
 * As in iommu_gas_reserve_region, reserve [start, end), but allow for existing
 * entries.
 */
int
iommu_gas_reserve_region_extend(struct iommu_domain *domain,
    iommu_gaddr_t start, iommu_gaddr_t end)
{
	struct iommu_map_entry *entry, *next, *prev, key = {};
	iommu_gaddr_t entry_start, entry_end;
	int error;

	error = 0;
	entry = NULL;
	end = ummin(end, domain->end);
	while (start < end) {
		/* Preallocate an entry. */
		if (entry == NULL)
			entry = iommu_gas_alloc_entry(domain,
			    IOMMU_PGF_WAITOK);
		/* Calculate the free region from here to the next entry. */
		key.start = key.end = start;
		IOMMU_DOMAIN_LOCK(domain);
		next = RB_NFIND(iommu_gas_entries_tree, &domain->rb_root, &key);
		KASSERT(next != NULL, ("domain %p with end %#jx has no entry "
		    "after %#jx", domain, (uintmax_t)domain->end,
		    (uintmax_t)start));
		entry_end = ummin(end, next->start);
		prev = RB_PREV(iommu_gas_entries_tree, &domain->rb_root, next);
		if (prev != NULL)
			entry_start = ummax(start, prev->end);
		else
			entry_start = start;
		start = next->end;
		/* Reserve the region if non-empty. */
		if (entry_start != entry_end) {
			error = iommu_gas_reserve_region_locked(domain,
			    entry_start, entry_end, entry);
			if (error != 0) {
				IOMMU_DOMAIN_UNLOCK(domain);
				break;
			}
			entry = NULL;
		}
		IOMMU_DOMAIN_UNLOCK(domain);
	}
	/* Release a preallocated entry if it was not used. */
	if (entry != NULL)
		iommu_gas_free_entry(entry);
	return (error);
}

void
iommu_unmap_msi(struct iommu_ctx *ctx)
{
	struct iommu_map_entry *entry;
	struct iommu_domain *domain;

	domain = ctx->domain;
	entry = domain->msi_entry;
	if (entry == NULL)
		return;

	domain->ops->unmap(domain, entry, IOMMU_PGF_WAITOK);

	iommu_gas_free_space(entry);

	iommu_gas_free_entry(entry);

	domain->msi_entry = NULL;
	domain->msi_base = 0;
	domain->msi_phys = 0;
}

int
iommu_map_msi(struct iommu_ctx *ctx, iommu_gaddr_t size, int offset,
    u_int eflags, u_int flags, vm_page_t *ma)
{
	struct iommu_domain *domain;
	struct iommu_map_entry *entry;
	int error;

	error = 0;
	domain = ctx->domain;

	/* Check if there is already an MSI page allocated */
	IOMMU_DOMAIN_LOCK(domain);
	entry = domain->msi_entry;
	IOMMU_DOMAIN_UNLOCK(domain);

	if (entry == NULL) {
		error = iommu_gas_map(domain, &ctx->tag->common, size, offset,
		    eflags, flags, ma, &entry);
		IOMMU_DOMAIN_LOCK(domain);
		if (error == 0) {
			if (domain->msi_entry == NULL) {
				MPASS(domain->msi_base == 0);
				MPASS(domain->msi_phys == 0);

				domain->msi_entry = entry;
				domain->msi_base = entry->start;
				domain->msi_phys = VM_PAGE_TO_PHYS(ma[0]);
			} else {
				/*
				 * We lost the race and already have an
				 * MSI page allocated. Free the unneeded entry.
				 */
				iommu_gas_free_entry(entry);
			}
		} else if (domain->msi_entry != NULL) {
			/*
			 * The allocation failed, but another succeeded.
			 * Return success as there is a valid MSI page.
			 */
			error = 0;
		}
		IOMMU_DOMAIN_UNLOCK(domain);
	}

	return (error);
}

void
iommu_translate_msi(struct iommu_domain *domain, uint64_t *addr)
{

	*addr = (*addr - domain->msi_phys) + domain->msi_base;

	KASSERT(*addr >= domain->msi_entry->start,
	    ("%s: Address is below the MSI entry start address (%jx < %jx)",
	    __func__, (uintmax_t)*addr, (uintmax_t)domain->msi_entry->start));

	KASSERT(*addr + sizeof(*addr) <= domain->msi_entry->end,
	    ("%s: Address is above the MSI entry end address (%jx < %jx)",
	    __func__, (uintmax_t)*addr, (uintmax_t)domain->msi_entry->end));
}

SYSCTL_NODE(_hw, OID_AUTO, iommu, CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, "");

#ifdef INVARIANTS
SYSCTL_INT(_hw_iommu, OID_AUTO, check_free, CTLFLAG_RWTUN,
    &iommu_check_free, 0,
    "Check the GPA RBtree for free_down and free_after validity");
#endif

#include "opt_ddb.h"
#ifdef DDB

#include <ddb/ddb.h>

static void
iommu_debug_dump_gas(struct iommu_domain *domain)
{
	struct iommu_map_entry *entry;

	db_printf("iommu_domain %p tree %p iommu %p fl %#x\n", domain,
	    &domain->rb_root, domain->iommu, domain->flags);
	db_printf("iommu_domain %p tree %p\n", domain, &domain->rb_root);
	RB_FOREACH(entry, iommu_gas_entries_tree, &domain->rb_root) {
		db_printf(
	    "  e %p [%#jx %#jx] fl %#x first %#jx last %#jx free_down %#jx",
		    entry, (uintmax_t)entry->start, (uintmax_t)entry->end,
		    entry->flags,
		    (uintmax_t)entry->first, (uintmax_t)entry->last,
		    (uintmax_t)entry->free_down);
		if (entry == domain->start_gap)
			db_printf(" start_gap");
		if (entry == domain->first_place)
			db_printf(" first_place");
		if (entry == domain->last_place)
			db_printf(" last_place");
		db_printf("\n");
	}
}

DB_SHOW_COMMAND(iommu_domain, iommu_domain_show)
{
	struct iommu_domain *domain;

	if (!have_addr) {
		db_printf("show iommu_domain addr\n");
		return;
	}

	domain = (void *)addr;
	iommu_debug_dump_gas(domain);
}

#endif
