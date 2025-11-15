/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bitset.h>
#include <sys/domainset.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/rwlock.h>
#include <sys/pctrie.h>
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

static bool vm_domainset_iter_next(struct vm_domainset_iter *di, int *domain);


/*
 * Determine which policy is to be used for this allocation.
 */
static void
vm_domainset_iter_init(struct vm_domainset_iter *di, struct domainset *ds,
    int *iter, struct vm_object *obj, vm_pindex_t pindex)
{

	di->di_domain = ds;
	di->di_iter = iter;
	di->di_policy = ds->ds_policy;
	DOMAINSET_COPY(&ds->ds_mask, &di->di_valid_mask);
	if (di->di_policy == DOMAINSET_POLICY_INTERLEAVE) {
#if VM_NRESERVLEVEL > 0
		if (vm_object_reserv(obj)) {
			/*
			 * Color the pindex so we end up on the correct
			 * reservation boundary.
			 */
			pindex += obj->pg_color;
#if VM_NRESERVLEVEL > 1
			pindex >>= VM_LEVEL_1_ORDER;
#endif
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
}

static void
vm_domainset_iter_rr(struct vm_domainset_iter *di, int *domain)
{

	/* Grab the next domain in 'ds_order'. */
	*domain = di->di_domain->ds_order[
	    (*di->di_iter)++ % di->di_domain->ds_cnt];
}

static void
vm_domainset_iter_interleave(struct vm_domainset_iter *di, int *domain)
{
	int d;

	d = di->di_offset % di->di_domain->ds_cnt;
	*domain = di->di_domain->ds_order[d];
}

/*
 * Internal function determining the current phase's first candidate domain.
 *
 * Returns whether these is an eligible domain, which is returned through
 * '*domain'.  '*domain' can be modified even if there is no eligible domain.
 *
 * See herald comment of vm_domainset_iter_first() below about phases.
 */
static bool
vm_domainset_iter_phase_first(struct vm_domainset_iter *di, int *domain)
{
	switch (di->di_policy) {
	case DOMAINSET_POLICY_FIRSTTOUCH:
		*domain = PCPU_GET(domain);
		break;
	case DOMAINSET_POLICY_ROUNDROBIN:
		vm_domainset_iter_rr(di, domain);
		break;
	case DOMAINSET_POLICY_PREFER:
		*domain = di->di_domain->ds_prefer;
		break;
	case DOMAINSET_POLICY_INTERLEAVE:
		vm_domainset_iter_interleave(di, domain);
		break;
	default:
		panic("%s: Unknown policy %d", __func__, di->di_policy);
	}
	KASSERT(*domain < vm_ndomains,
	    ("%s: Invalid domain %d", __func__, *domain));

	/*
	 * Has the policy's start domain already been visited?
	 */
	if (!DOMAINSET_ISSET(*domain, &di->di_remain_mask))
		return (vm_domainset_iter_next(di, domain));

	DOMAINSET_CLR(*domain, &di->di_remain_mask);

	/* Does it have enough free pages (phase 1)? */
	if (di->di_minskip && vm_page_count_min_domain(*domain)) {
		/* Mark the domain as eligible for phase 2. */
		DOMAINSET_SET(*domain, &di->di_min_mask);
		return (vm_domainset_iter_next(di, domain));
	}

	return (true);
}

/*
 * Resets an iterator to point to the first candidate domain.
 *
 * Returns whether there is an eligible domain to start with.  '*domain' may be
 * modified even if there is none.
 *
 * There must have been one call to vm_domainset_iter_init() before.
 *
 * This function must be called at least once before calling
 * vm_domainset_iter_next().  Note that functions wrapping
 * vm_domainset_iter_init() usually do that themselves.
 *
 * This function may be called again to reset the iterator to the policy's first
 * candidate domain.  After each reset, the iterator will visit the same domains
 * as in the previous iteration minus those on which vm_domainset_iter_ignore()
 * has been called.  Note that the first candidate domain may change at each
 * reset (at time of this writing, only on the DOMAINSET_POLICY_ROUNDROBIN
 * policy).
 *
 * Domains which have a number of free pages over 'v_free_min' are always
 * visited first (this is called the "phase 1" in comments, "phase 2" being the
 * examination of the remaining domains; no domains are ever visited twice).
 */
static bool
vm_domainset_iter_first(struct vm_domainset_iter *di, int *domain)
{
	/* Initialize the mask of domains to visit. */
	DOMAINSET_COPY(&di->di_valid_mask, &di->di_remain_mask);
	/*
	 * No candidate domains for phase 2 at start.  This will be filled by
	 * phase 1.
	 */
	DOMAINSET_ZERO(&di->di_min_mask);
	/* Skip domains below 'v_free_min' on phase 1. */
	di->di_minskip = true;

	return (vm_domainset_iter_phase_first(di, domain));
}

/*
 * Advances the iterator to the next candidate domain.
 *
 * Returns whether there was another domain to visit.  '*domain' may be modified
 * even if there is none.
 *
 * vm_domainset_iter_first() must have been called at least once before using
 * this function (see its herald comment for more details on iterators).
 */
static bool
vm_domainset_iter_next(struct vm_domainset_iter *di, int *domain)
{
	/* Loop while there remains domains to visit in the current phase. */
	while (!DOMAINSET_EMPTY(&di->di_remain_mask)) {
		/* Grab the next domain in 'ds_order'. */
		vm_domainset_iter_rr(di, domain);
		KASSERT(*domain < vm_ndomains,
		    ("%s: Invalid domain %d", __func__, *domain));

		if (DOMAINSET_ISSET(*domain, &di->di_remain_mask)) {
			DOMAINSET_CLR(*domain, &di->di_remain_mask);
			if (!di->di_minskip || !vm_page_count_min_domain(*domain))
				return (true);
			DOMAINSET_SET(*domain, &di->di_min_mask);
		}
	}

	/*
	 * If phase 1 (skip low memory domains) is over, start phase 2 (consider
	 * low memory domains).
	 */
	if (di->di_minskip) {
		di->di_minskip = false;
		/* Browse domains that were under 'v_free_min'. */
		DOMAINSET_COPY(&di->di_min_mask, &di->di_remain_mask);
		return (vm_domainset_iter_phase_first(di, domain));
	}

	return (false);
}

int
vm_domainset_iter_page_init(struct vm_domainset_iter *di, struct vm_object *obj,
    vm_pindex_t pindex, int *domain, int *req)
{
	struct domainset_ref *dr;

	di->di_flags = *req;
	*req = (di->di_flags & ~(VM_ALLOC_WAITOK | VM_ALLOC_WAITFAIL)) |
	    VM_ALLOC_NOWAIT;

	/*
	 * Object policy takes precedence over thread policy.  The policies
	 * are immutable and unsynchronized.  Updates can race but pointer
	 * loads are assumed to be atomic.
	 */
	if (obj != NULL && obj->domain.dr_policy != NULL) {
		/*
		 * This write lock protects non-atomic increments of the
		 * iterator index in vm_domainset_iter_rr().
		 */
		VM_OBJECT_ASSERT_WLOCKED(obj);
		dr = &obj->domain;
	} else
		dr = &curthread->td_domain;

	vm_domainset_iter_init(di, dr->dr_policy, &dr->dr_iter, obj, pindex);
	/*
	 * XXXOC: Shouldn't we just panic on 'false' if VM_ALLOC_WAITOK was
	 * passed?
	 */
	return (vm_domainset_iter_first(di, domain) ? 0 : ENOMEM);
}

int
vm_domainset_iter_page(struct vm_domainset_iter *di, struct vm_object *obj,
    int *domain, struct pctrie_iter *pages)
{
	if (vm_domainset_iter_next(di, domain))
		return (0);

	/* If we visited all domains and this was a NOWAIT we return error. */
	if ((di->di_flags & (VM_ALLOC_WAITOK | VM_ALLOC_WAITFAIL)) == 0)
		return (ENOMEM);

	/* Wait for one of the domains to accumulate some free pages. */
	if (obj != NULL) {
		VM_OBJECT_WUNLOCK(obj);
		if (pages != NULL)
			pctrie_iter_reset(pages);
	}
	vm_wait_doms(&di->di_valid_mask, 0);
	if (obj != NULL)
		VM_OBJECT_WLOCK(obj);
	if ((di->di_flags & VM_ALLOC_WAITFAIL) != 0)
		return (ENOMEM);

	/* Restart the search. */
	/* XXXOC: Shouldn't we just panic on 'false'? */
	return (vm_domainset_iter_first(di, domain) ? 0 : ENOMEM);
}

static int
_vm_domainset_iter_policy_init(struct vm_domainset_iter *di, int *domain,
    int *flags)
{
	di->di_flags = *flags;
	*flags = (di->di_flags & ~M_WAITOK) | M_NOWAIT;
	/* XXXOC: Shouldn't we just panic on 'false' if M_WAITOK was passed? */
	return (vm_domainset_iter_first(di, domain) ? 0 : ENOMEM);
}

int
vm_domainset_iter_policy_init(struct vm_domainset_iter *di,
    struct domainset *ds, int *domain, int *flags)
{

	vm_domainset_iter_init(di, ds, &curthread->td_domain.dr_iter, NULL, 0);
	return (_vm_domainset_iter_policy_init(di, domain, flags));
}

int
vm_domainset_iter_policy_ref_init(struct vm_domainset_iter *di,
    struct domainset_ref *dr, int *domain, int *flags)
{

	vm_domainset_iter_init(di, dr->dr_policy, &dr->dr_iter, NULL, 0);
	return (_vm_domainset_iter_policy_init(di, domain, flags));
}

int
vm_domainset_iter_policy(struct vm_domainset_iter *di, int *domain)
{
	if (vm_domainset_iter_next(di, domain))
		return (0);

	/* If we visited all domains and this was a NOWAIT we return error. */
	if ((di->di_flags & M_WAITOK) == 0)
		return (ENOMEM);

	/* Wait for one of the domains to accumulate some free pages. */
	vm_wait_doms(&di->di_valid_mask, 0);

	/* Restart the search. */
	/* XXXOC: Shouldn't we just panic on 'false'? */
	return (vm_domainset_iter_first(di, domain) ? 0 : ENOMEM);
}

void
vm_domainset_iter_ignore(struct vm_domainset_iter *di, int domain)
{
	KASSERT(DOMAINSET_ISSET(domain, &di->di_valid_mask),
	    ("%s: domain %d not present in di_valid_mask for di %p",
	    __func__, domain, di));
	DOMAINSET_CLR(domain, &di->di_valid_mask);
}

#else /* !NUMA */

int
vm_domainset_iter_page(struct vm_domainset_iter *di, struct vm_object *obj,
    int *domain, struct pctrie_iter *pages)
{

	return (EJUSTRETURN);
}

int
vm_domainset_iter_page_init(struct vm_domainset_iter *di, struct vm_object *obj,
    vm_pindex_t pindex, int *domain, int *flags)
{
	*domain = 0;
	return (0);
}

int
vm_domainset_iter_policy(struct vm_domainset_iter *di, int *domain)
{

	return (EJUSTRETURN);
}

int
vm_domainset_iter_policy_init(struct vm_domainset_iter *di,
    struct domainset *ds, int *domain, int *flags)
{
	*domain = 0;
	return (0);
}

int
vm_domainset_iter_policy_ref_init(struct vm_domainset_iter *di,
    struct domainset_ref *dr, int *domain, int *flags)
{
	*domain = 0;
	return (0);
}

void
vm_domainset_iter_ignore(struct vm_domainset_iter *di __unused,
    int domain __unused)
{
}

#endif /* NUMA */
