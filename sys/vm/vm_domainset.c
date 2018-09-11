/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2017,	Jeffrey Roberson <jeff@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitset.h>
#include <sys/domainset.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_domainset.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>

#ifdef NUMA
/*
 * Iterators are written such that the first nowait pass has as short a
 * codepath as possible to eliminate bloat from the allocator.  It is
 * assumed that most allocations are successful.
 */

static int vm_domainset_default_stride = 64;

/*
 * Determine which policy is to be used for this allocation.
 */
static void
vm_domainset_iter_init(struct vm_domainset_iter *di, struct vm_object *obj,
    vm_pindex_t pindex)
{
	struct domainset *domain;

	/*
	 * object policy takes precedence over thread policy.  The policies
	 * are immutable and unsynchronized.  Updates can race but pointer
	 * loads are assumed to be atomic.
	 */
	if (obj != NULL && (domain = obj->domain.dr_policy) != NULL) {
		di->di_domain = domain;
		di->di_iter = &obj->domain.dr_iterator;
	} else {
		di->di_domain = curthread->td_domain.dr_policy;
		di->di_iter = &curthread->td_domain.dr_iterator;
	}
	di->di_policy = di->di_domain->ds_policy;
	if (di->di_policy == DOMAINSET_POLICY_INTERLEAVE) {
#if VM_NRESERVLEVEL > 0
		if (vm_object_reserv(obj)) {
			/*
			 * Color the pindex so we end up on the correct
			 * reservation boundary.
			 */
			pindex += obj->pg_color;
			pindex >>= VM_LEVEL_0_ORDER;
		} else
#endif
			pindex /= vm_domainset_default_stride;
		/*
		 * Offset pindex so the first page of each object does
		 * not end up in domain 0.
		 */
		if (obj != NULL)
			pindex += (((uintptr_t)obj) / sizeof(*obj));
		di->di_offset = pindex;
	}
	/* Skip domains below min on the first pass. */
	di->di_minskip = true;
}

static void
vm_domainset_iter_rr(struct vm_domainset_iter *di, int *domain)
{

	*domain = di->di_domain->ds_order[
	    ++(*di->di_iter) % di->di_domain->ds_cnt];
}

static void
vm_domainset_iter_prefer(struct vm_domainset_iter *di, int *domain)
{
	int d;

	do {
		d = di->di_domain->ds_order[
		    ++(*di->di_iter) % di->di_domain->ds_cnt];
	} while (d == di->di_domain->ds_prefer);
	*domain = d;
}

static void
vm_domainset_iter_interleave(struct vm_domainset_iter *di, int *domain)
{
	int d;

	d = di->di_offset % di->di_domain->ds_cnt;
	*di->di_iter = d;
	*domain = di->di_domain->ds_order[d];
}

static void
vm_domainset_iter_next(struct vm_domainset_iter *di, int *domain)
{

	KASSERT(di->di_n > 0,
	    ("vm_domainset_iter_first: Invalid n %d", di->di_n));
	switch (di->di_policy) {
	case DOMAINSET_POLICY_FIRSTTOUCH:
		/*
		 * To prevent impossible allocations we convert an invalid
		 * first-touch to round-robin.
		 */
		/* FALLTHROUGH */
	case DOMAINSET_POLICY_INTERLEAVE:
		/* FALLTHROUGH */
	case DOMAINSET_POLICY_ROUNDROBIN:
		vm_domainset_iter_rr(di, domain);
		break;
	case DOMAINSET_POLICY_PREFER:
		vm_domainset_iter_prefer(di, domain);
		break;
	default:
		panic("vm_domainset_iter_first: Unknown policy %d",
		    di->di_policy);
	}
	KASSERT(*domain < vm_ndomains,
	    ("vm_domainset_iter_next: Invalid domain %d", *domain));
}

static void
vm_domainset_iter_first(struct vm_domainset_iter *di, int *domain)
{

	switch (di->di_policy) {
	case DOMAINSET_POLICY_FIRSTTOUCH:
		*domain = PCPU_GET(domain);
		if (DOMAINSET_ISSET(*domain, &di->di_domain->ds_mask)) {
			/*
			 * Add an extra iteration because we will visit the
			 * current domain a second time in the rr iterator.
			 */
			di->di_n = di->di_domain->ds_cnt + 1;
			break;
		}
		/*
		 * To prevent impossible allocations we convert an invalid
		 * first-touch to round-robin.
		 */
		/* FALLTHROUGH */
	case DOMAINSET_POLICY_ROUNDROBIN:
		di->di_n = di->di_domain->ds_cnt;
		vm_domainset_iter_rr(di, domain);
		break;
	case DOMAINSET_POLICY_PREFER:
		*domain = di->di_domain->ds_prefer;
		di->di_n = di->di_domain->ds_cnt;
		break;
	case DOMAINSET_POLICY_INTERLEAVE:
		vm_domainset_iter_interleave(di, domain);
		di->di_n = di->di_domain->ds_cnt;
		break;
	default:
		panic("vm_domainset_iter_first: Unknown policy %d",
		    di->di_policy);
	}
	KASSERT(di->di_n > 0,
	    ("vm_domainset_iter_first: Invalid n %d", di->di_n));
	KASSERT(*domain < vm_ndomains,
	    ("vm_domainset_iter_first: Invalid domain %d", *domain));
}

void
vm_domainset_iter_page_init(struct vm_domainset_iter *di, struct vm_object *obj,
    vm_pindex_t pindex, int *domain, int *req)
{

	vm_domainset_iter_init(di, obj, pindex);
	di->di_flags = *req;
	*req = (di->di_flags & ~(VM_ALLOC_WAITOK | VM_ALLOC_WAITFAIL)) |
	    VM_ALLOC_NOWAIT;
	vm_domainset_iter_first(di, domain);
	if (DOMAINSET_ISSET(*domain, &vm_min_domains))
		vm_domainset_iter_page(di, domain, req);
}

int
vm_domainset_iter_page(struct vm_domainset_iter *di, int *domain, int *req)
{

	/*
	 * If we exhausted all options with NOWAIT and did a WAITFAIL it
	 * is time to return an error to the caller.
	 */
	if ((*req & VM_ALLOC_WAITFAIL) != 0)
		return (ENOMEM);

	/* If there are more domains to visit we run the iterator. */
	while (--di->di_n != 0) {
		vm_domainset_iter_next(di, domain);
		if (!di->di_minskip ||
		    !DOMAINSET_ISSET(*domain, &vm_min_domains))
			return (0);
	}
	if (di->di_minskip) {
		di->di_minskip = false;
		vm_domainset_iter_first(di, domain);
		return (0);
	}

	/* If we visited all domains and this was a NOWAIT we return error. */
	if ((di->di_flags & (VM_ALLOC_WAITOK | VM_ALLOC_WAITFAIL)) == 0)
		return (ENOMEM);

	/*
	 * We have visited all domains with non-blocking allocations, try
	 * from the beginning with a blocking allocation.
	 */
	vm_domainset_iter_first(di, domain);
	*req = di->di_flags;

	return (0);
}


void
vm_domainset_iter_malloc_init(struct vm_domainset_iter *di,
    struct vm_object *obj, int *domain, int *flags)
{

	vm_domainset_iter_init(di, obj, 0);
	if (di->di_policy == DOMAINSET_POLICY_INTERLEAVE)
		di->di_policy = DOMAINSET_POLICY_ROUNDROBIN;
	di->di_flags = *flags;
	*flags = (di->di_flags & ~M_WAITOK) | M_NOWAIT;
	vm_domainset_iter_first(di, domain);
	if (DOMAINSET_ISSET(*domain, &vm_min_domains))
		vm_domainset_iter_malloc(di, domain, flags);
}

int
vm_domainset_iter_malloc(struct vm_domainset_iter *di, int *domain, int *flags)
{

	/* If there are more domains to visit we run the iterator. */
	while (--di->di_n != 0) {
		vm_domainset_iter_next(di, domain);
		if (!di->di_minskip ||
		    !DOMAINSET_ISSET(*domain, &vm_min_domains))
			return (0);
	}

	/* If we skipped domains below min restart the search. */
	if (di->di_minskip) {
		di->di_minskip = false;
		vm_domainset_iter_first(di, domain);
		return (0);
	}

	/* If we visited all domains and this was a NOWAIT we return error. */
	if ((di->di_flags & M_WAITOK) == 0)
		return (ENOMEM);

	/*
	 * We have visited all domains with non-blocking allocations, try
	 * from the beginning with a blocking allocation.
	 */
	vm_domainset_iter_first(di, domain);
	*flags = di->di_flags;

	return (0);
}

#else /* !NUMA */
int
vm_domainset_iter_page(struct vm_domainset_iter *di, int *domain, int *flags)
{

	return (EJUSTRETURN);
}

void
vm_domainset_iter_page_init(struct vm_domainset_iter *di,
            struct vm_object *obj, vm_pindex_t pindex, int *domain, int *flags)
{

	*domain = 0;
}

int
vm_domainset_iter_malloc(struct vm_domainset_iter *di, int *domain, int *flags)
{

	return (EJUSTRETURN);
}

void
vm_domainset_iter_malloc_init(struct vm_domainset_iter *di,
            struct vm_object *obj, int *domain, int *flags)
{

	*domain = 0;
}

#endif
