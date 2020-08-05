/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define	RB_AUGMENT(entry) iommu_gas_augment_entry(entry)

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
		res->domain = domain;
		atomic_add_int(&domain->entries_cnt, 1);
	}
	return (res);
}

void
iommu_gas_free_entry(struct iommu_domain *domain, struct iommu_map_entry *entry)
{

	KASSERT(domain == entry->domain,
	    ("mismatched free domain %p entry %p entry->domain %p", domain,
	    entry, entry->domain));
	atomic_subtract_int(&domain->entries_cnt, 1);
	uma_zfree(iommu_map_entry_zone, entry);
}

static int
iommu_gas_cmp_entries(struct iommu_map_entry *a, struct iommu_map_entry *b)
{

	/* Last entry have zero size, so <= */
	KASSERT(a->start <= a->end, ("inverted entry %p (%jx, %jx)",
	    a, (uintmax_t)a->start, (uintmax_t)a->end));
	KASSERT(b->start <= b->end, ("inverted entry %p (%jx, %jx)",
	    b, (uintmax_t)b->start, (uintmax_t)b->end));
	KASSERT(a->end <= b->start || b->end <= a->start ||
	    a->end == a->start || b->end == b->start,
	    ("overlapping entries %p (%jx, %jx) %p (%jx, %jx)",
	    a, (uintmax_t)a->start, (uintmax_t)a->end,
	    b, (uintmax_t)b->start, (uintmax_t)b->end));

	if (a->end < b->end)
		return (-1);
	else if (b->end < a->end)
		return (1);
	return (0);
}

static void
iommu_gas_augment_entry(struct iommu_map_entry *entry)
{
	struct iommu_map_entry *child;
	iommu_gaddr_t free_down;

	free_down = 0;
	if ((child = RB_LEFT(entry, rb_entry)) != NULL) {
		free_down = MAX(free_down, child->free_down);
		free_down = MAX(free_down, entry->start - child->last);
		entry->first = child->first;
	} else
		entry->first = entry->start;
	
	if ((child = RB_RIGHT(entry, rb_entry)) != NULL) {
		free_down = MAX(free_down, child->free_down);
		free_down = MAX(free_down, child->first - entry->end);
		entry->last = child->last;
	} else
		entry->last = entry->end;
	entry->free_down = free_down;
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

static bool
iommu_gas_rb_insert(struct iommu_domain *domain, struct iommu_map_entry *entry)
{
	struct iommu_map_entry *found;

	found = RB_INSERT(iommu_gas_entries_tree,
	    &domain->rb_root, entry);
	return (found == NULL);
}

static void
iommu_gas_rb_remove(struct iommu_domain *domain, struct iommu_map_entry *entry)
{

	RB_REMOVE(iommu_gas_entries_tree, &domain->rb_root, entry);
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

	begin->start = 0;
	begin->end = IOMMU_PAGE_SIZE;
	begin->flags = IOMMU_MAP_ENTRY_PLACE | IOMMU_MAP_ENTRY_UNMAPPED;
	iommu_gas_rb_insert(domain, begin);

	end->start = domain->end;
	end->end = domain->end;
	end->flags = IOMMU_MAP_ENTRY_PLACE | IOMMU_MAP_ENTRY_UNMAPPED;
	iommu_gas_rb_insert(domain, end);

	domain->first_place = begin;
	domain->last_place = end;
	domain->flags |= IOMMU_DOMAIN_GAS_INITED;
	IOMMU_DOMAIN_UNLOCK(domain);
}

void
iommu_gas_fini_domain(struct iommu_domain *domain)
{
	struct iommu_map_entry *entry, *entry1;

	IOMMU_DOMAIN_ASSERT_LOCKED(domain);
	KASSERT(domain->entries_cnt == 2,
	    ("domain still in use %p", domain));

	entry = RB_MIN(iommu_gas_entries_tree, &domain->rb_root);
	KASSERT(entry->start == 0, ("start entry start %p", domain));
	KASSERT(entry->end == IOMMU_PAGE_SIZE, ("start entry end %p", domain));
	KASSERT(entry->flags == IOMMU_MAP_ENTRY_PLACE,
	    ("start entry flags %p", domain));
	RB_REMOVE(iommu_gas_entries_tree, &domain->rb_root, entry);
	iommu_gas_free_entry(domain, entry);

	entry = RB_MAX(iommu_gas_entries_tree, &domain->rb_root);
	KASSERT(entry->start == domain->end, ("end entry start %p", domain));
	KASSERT(entry->end == domain->end, ("end entry end %p", domain));
	KASSERT(entry->flags == IOMMU_MAP_ENTRY_PLACE,
	    ("end entry flags %p", domain));
	RB_REMOVE(iommu_gas_entries_tree, &domain->rb_root, entry);
	iommu_gas_free_entry(domain, entry);

	RB_FOREACH_SAFE(entry, iommu_gas_entries_tree, &domain->rb_root,
	    entry1) {
		KASSERT((entry->flags & IOMMU_MAP_ENTRY_RMRR) != 0,
		    ("non-RMRR entry left %p", domain));
		RB_REMOVE(iommu_gas_entries_tree, &domain->rb_root,
		    entry);
		iommu_gas_free_entry(domain, entry);
	}
}

struct iommu_gas_match_args {
	struct iommu_domain *domain;
	iommu_gaddr_t size;
	int offset;
	const struct bus_dma_tag_common *common;
	u_int gas_flags;
	struct iommu_map_entry *entry;
};

/*
 * The interval [beg, end) is a free interval between two iommu_map_entries.
 * maxaddr is an upper bound on addresses that can be allocated. Try to
 * allocate space in the free interval, subject to the conditions expressed
 * by a, and return 'true' if and only if the allocation attempt succeeds.
 */
static bool
iommu_gas_match_one(struct iommu_gas_match_args *a, iommu_gaddr_t beg,
    iommu_gaddr_t end, iommu_gaddr_t maxaddr)
{
	iommu_gaddr_t bs, start;

	a->entry->start = roundup2(beg + IOMMU_PAGE_SIZE,
	    a->common->alignment);
	if (a->entry->start + a->size > maxaddr)
		return (false);

	/* IOMMU_PAGE_SIZE to create gap after new entry. */
	if (a->entry->start < beg + IOMMU_PAGE_SIZE ||
	    a->entry->start + a->size + a->offset + IOMMU_PAGE_SIZE > end)
		return (false);

	/* No boundary crossing. */
	if (iommu_test_boundary(a->entry->start + a->offset, a->size,
	    a->common->boundary))
		return (true);

	/*
	 * The start + offset to start + offset + size region crosses
	 * the boundary.  Check if there is enough space after the
	 * next boundary after the beg.
	 */
	bs = rounddown2(a->entry->start + a->offset + a->common->boundary,
	    a->common->boundary);
	start = roundup2(bs, a->common->alignment);
	/* IOMMU_PAGE_SIZE to create gap after new entry. */
	if (start + a->offset + a->size + IOMMU_PAGE_SIZE <= end &&
	    start + a->offset + a->size <= maxaddr &&
	    iommu_test_boundary(start + a->offset, a->size,
	    a->common->boundary)) {
		a->entry->start = start;
		return (true);
	}

	/*
	 * Not enough space to align at the requested boundary, or
	 * boundary is smaller than the size, but allowed to split.
	 * We already checked that start + size does not overlap maxaddr.
	 *
	 * XXXKIB. It is possible that bs is exactly at the start of
	 * the next entry, then we do not have gap.  Ignore for now.
	 */
	if ((a->gas_flags & IOMMU_MF_CANSPLIT) != 0) {
		a->size = bs - a->entry->start;
		return (true);
	}

	return (false);
}

static void
iommu_gas_match_insert(struct iommu_gas_match_args *a)
{
	bool found;

	/*
	 * The prev->end is always aligned on the page size, which
	 * causes page alignment for the entry->start too.  The size
	 * is checked to be multiple of the page size.
	 *
	 * The page sized gap is created between consequent
	 * allocations to ensure that out-of-bounds accesses fault.
	 */
	a->entry->end = a->entry->start + a->size;

	found = iommu_gas_rb_insert(a->domain, a->entry);
	KASSERT(found, ("found dup %p start %jx size %jx",
	    a->domain, (uintmax_t)a->entry->start, (uintmax_t)a->size));
	a->entry->flags = IOMMU_MAP_ENTRY_MAP;
}

static int
iommu_gas_lowermatch(struct iommu_gas_match_args *a, struct iommu_map_entry *entry)
{
	struct iommu_map_entry *child;

	child = RB_RIGHT(entry, rb_entry);
	if (child != NULL && entry->end < a->common->lowaddr &&
	    iommu_gas_match_one(a, entry->end, child->first,
	    a->common->lowaddr)) {
		iommu_gas_match_insert(a);
		return (0);
	}
	if (entry->free_down < a->size + a->offset + IOMMU_PAGE_SIZE)
		return (ENOMEM);
	if (entry->first >= a->common->lowaddr)
		return (ENOMEM);
	child = RB_LEFT(entry, rb_entry);
	if (child != NULL && 0 == iommu_gas_lowermatch(a, child))
		return (0);
	if (child != NULL && child->last < a->common->lowaddr &&
	    iommu_gas_match_one(a, child->last, entry->start,
	    a->common->lowaddr)) {
		iommu_gas_match_insert(a);
		return (0);
	}
	child = RB_RIGHT(entry, rb_entry);
	if (child != NULL && 0 == iommu_gas_lowermatch(a, child))
		return (0);
	return (ENOMEM);
}

static int
iommu_gas_uppermatch(struct iommu_gas_match_args *a, struct iommu_map_entry *entry)
{
	struct iommu_map_entry *child;

	if (entry->free_down < a->size + a->offset + IOMMU_PAGE_SIZE)
		return (ENOMEM);
	if (entry->last < a->common->highaddr)
		return (ENOMEM);
	child = RB_LEFT(entry, rb_entry);
	if (child != NULL && 0 == iommu_gas_uppermatch(a, child))
		return (0);
	if (child != NULL && child->last >= a->common->highaddr &&
	    iommu_gas_match_one(a, child->last, entry->start,
	    a->domain->end)) {
		iommu_gas_match_insert(a);
		return (0);
	}
	child = RB_RIGHT(entry, rb_entry);
	if (child != NULL && entry->end >= a->common->highaddr &&
	    iommu_gas_match_one(a, entry->end, child->first,
	    a->domain->end)) {
		iommu_gas_match_insert(a);
		return (0);
	}
	if (child != NULL && 0 == iommu_gas_uppermatch(a, child))
		return (0);
	return (ENOMEM);
}

static int
iommu_gas_find_space(struct iommu_domain *domain,
    const struct bus_dma_tag_common *common, iommu_gaddr_t size,
    int offset, u_int flags, struct iommu_map_entry *entry)
{
	struct iommu_gas_match_args a;
	int error;

	IOMMU_DOMAIN_ASSERT_LOCKED(domain);
	KASSERT(entry->flags == 0, ("dirty entry %p %p", domain, entry));
	KASSERT((size & IOMMU_PAGE_MASK) == 0, ("size %jx", (uintmax_t)size));

	a.domain = domain;
	a.size = size;
	a.offset = offset;
	a.common = common;
	a.gas_flags = flags;
	a.entry = entry;

	/* Handle lower region. */
	if (common->lowaddr > 0) {
		error = iommu_gas_lowermatch(&a,
		    RB_ROOT(&domain->rb_root));
		if (error == 0)
			return (0);
		KASSERT(error == ENOMEM,
		    ("error %d from iommu_gas_lowermatch", error));
	}
	/* Handle upper region. */
	if (common->highaddr >= domain->end)
		return (ENOMEM);
	error = iommu_gas_uppermatch(&a, RB_ROOT(&domain->rb_root));
	KASSERT(error == ENOMEM,
	    ("error %d from iommu_gas_uppermatch", error));
	return (error);
}

static int
iommu_gas_alloc_region(struct iommu_domain *domain, struct iommu_map_entry *entry,
    u_int flags)
{
	struct iommu_map_entry *next, *prev;
	bool found;

	IOMMU_DOMAIN_ASSERT_LOCKED(domain);

	if ((entry->start & IOMMU_PAGE_MASK) != 0 ||
	    (entry->end & IOMMU_PAGE_MASK) != 0)
		return (EINVAL);
	if (entry->start >= entry->end)
		return (EINVAL);
	if (entry->end >= domain->end)
		return (EINVAL);

	next = RB_NFIND(iommu_gas_entries_tree, &domain->rb_root, entry);
	KASSERT(next != NULL, ("next must be non-null %p %jx", domain,
	    (uintmax_t)entry->start));
	prev = RB_PREV(iommu_gas_entries_tree, &domain->rb_root, next);
	/* prev could be NULL */

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
	if (next->start < entry->end) {
		iommu_gas_rb_remove(domain, next);
		next = NULL;
	}

	found = iommu_gas_rb_insert(domain, entry);
	KASSERT(found, ("found RMRR dup %p start %jx end %jx",
	    domain, (uintmax_t)entry->start, (uintmax_t)entry->end));
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
iommu_gas_free_space(struct iommu_domain *domain, struct iommu_map_entry *entry)
{

	IOMMU_DOMAIN_ASSERT_LOCKED(domain);
	KASSERT((entry->flags & (IOMMU_MAP_ENTRY_PLACE | IOMMU_MAP_ENTRY_RMRR |
	    IOMMU_MAP_ENTRY_MAP)) == IOMMU_MAP_ENTRY_MAP,
	    ("permanent entry %p %p", domain, entry));

	iommu_gas_rb_remove(domain, entry);
	entry->flags &= ~IOMMU_MAP_ENTRY_MAP;
#ifdef INVARIANTS
	if (iommu_check_free)
		iommu_gas_check_free(domain);
#endif
}

void
iommu_gas_free_region(struct iommu_domain *domain, struct iommu_map_entry *entry)
{
	struct iommu_map_entry *next, *prev;

	IOMMU_DOMAIN_ASSERT_LOCKED(domain);
	KASSERT((entry->flags & (IOMMU_MAP_ENTRY_PLACE | IOMMU_MAP_ENTRY_RMRR |
	    IOMMU_MAP_ENTRY_MAP)) == IOMMU_MAP_ENTRY_RMRR,
	    ("non-RMRR entry %p %p", domain, entry));

	prev = RB_PREV(iommu_gas_entries_tree, &domain->rb_root, entry);
	next = RB_NEXT(iommu_gas_entries_tree, &domain->rb_root, entry);
	iommu_gas_rb_remove(domain, entry);
	entry->flags &= ~IOMMU_MAP_ENTRY_RMRR;

	if (prev == NULL)
		iommu_gas_rb_insert(domain, domain->first_place);
	if (next == NULL)
		iommu_gas_rb_insert(domain, domain->last_place);
}

int
iommu_gas_map(struct iommu_domain *domain,
    const struct bus_dma_tag_common *common, iommu_gaddr_t size, int offset,
    u_int eflags, u_int flags, vm_page_t *ma, struct iommu_map_entry **res)
{
	struct iommu_map_entry *entry;
	int error;

	KASSERT((flags & ~(IOMMU_MF_CANWAIT | IOMMU_MF_CANSPLIT)) == 0,
	    ("invalid flags 0x%x", flags));

	entry = iommu_gas_alloc_entry(domain,
	    (flags & IOMMU_MF_CANWAIT) != 0 ?  IOMMU_PGF_WAITOK : 0);
	if (entry == NULL)
		return (ENOMEM);
	IOMMU_DOMAIN_LOCK(domain);
	error = iommu_gas_find_space(domain, common, size, offset, flags,
	    entry);
	if (error == ENOMEM) {
		IOMMU_DOMAIN_UNLOCK(domain);
		iommu_gas_free_entry(domain, entry);
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

	error = domain->ops->map(domain, entry->start,
	    entry->end - entry->start, ma, eflags,
	    ((flags & IOMMU_MF_CANWAIT) != 0 ?  IOMMU_PGF_WAITOK : 0));
	if (error == ENOMEM) {
		iommu_domain_unload_entry(entry, true);
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

	error = domain->ops->map(domain, entry->start,
	    entry->end - entry->start, ma + OFF_TO_IDX(start - entry->start),
	    eflags, ((flags & IOMMU_MF_CANWAIT) != 0 ? IOMMU_PGF_WAITOK : 0));
	if (error == ENOMEM) {
		iommu_domain_unload_entry(entry, false);
		return (error);
	}
	KASSERT(error == 0,
	    ("unexpected error %d from domain_map_buf", error));

	return (0);
}

int
iommu_gas_reserve_region(struct iommu_domain *domain, iommu_gaddr_t start,
    iommu_gaddr_t end)
{
	struct iommu_map_entry *entry;
	int error;

	entry = iommu_gas_alloc_entry(domain, IOMMU_PGF_WAITOK);
	entry->start = start;
	entry->end = end;
	IOMMU_DOMAIN_LOCK(domain);
	error = iommu_gas_alloc_region(domain, entry, IOMMU_MF_CANWAIT);
	if (error == 0)
		entry->flags |= IOMMU_MAP_ENTRY_UNMAPPED;
	IOMMU_DOMAIN_UNLOCK(domain);
	if (error != 0)
		iommu_gas_free_entry(domain, entry);
	return (error);
}

struct iommu_map_entry *
iommu_map_alloc_entry(struct iommu_domain *domain, u_int flags)
{
	struct iommu_map_entry *res;

	res = iommu_gas_alloc_entry(domain, flags);

	return (res);
}

void
iommu_map_free_entry(struct iommu_domain *domain, struct iommu_map_entry *entry)
{

	iommu_gas_free_entry(domain, entry);
}

int
iommu_map(struct iommu_domain *domain,
    const struct bus_dma_tag_common *common, iommu_gaddr_t size, int offset,
    u_int eflags, u_int flags, vm_page_t *ma, struct iommu_map_entry **res)
{
	int error;

	error = iommu_gas_map(domain, common, size, offset, eflags, flags,
	    ma, res);

	return (error);
}

int
iommu_map_region(struct iommu_domain *domain, struct iommu_map_entry *entry,
    u_int eflags, u_int flags, vm_page_t *ma)
{
	int error;

	error = iommu_gas_map_region(domain, entry, eflags, flags, ma);

	return (error);
}

SYSCTL_NODE(_hw, OID_AUTO, iommu, CTLFLAG_RW | CTLFLAG_MPSAFE, NULL, "");

#ifdef INVARIANTS
SYSCTL_INT(_hw_iommu, OID_AUTO, check_free, CTLFLAG_RWTUN,
    &iommu_check_free, 0,
    "Check the GPA RBtree for free_down and free_after validity");
#endif
