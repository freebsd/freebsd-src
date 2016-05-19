/*-
 * Copyright (c) 2015 Adrian Chadd <adrian@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_vm.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#ifdef VM_NUMA_ALLOC
#include <sys/proc.h>
#endif
#include <sys/queue.h>
#include <sys/rwlock.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/tree.h>
#include <sys/vmmeter.h>
#include <sys/seq.h>

#include <ddb/ddb.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>

#include <vm/vm_domain.h>

#ifdef VM_NUMA_ALLOC
static __inline int
vm_domain_rr_selectdomain(int skip_domain)
{
	struct thread *td;

	td = curthread;

	td->td_dom_rr_idx++;
	td->td_dom_rr_idx %= vm_ndomains;

	/*
	 * If skip_domain is provided then skip over that
	 * domain.  This is intended for round robin variants
	 * which first try a fixed domain.
	 */
	if ((skip_domain > -1) && (td->td_dom_rr_idx == skip_domain)) {
		td->td_dom_rr_idx++;
		td->td_dom_rr_idx %= vm_ndomains;
	}
	return (td->td_dom_rr_idx);
}
#endif

/*
 * This implements a very simple set of VM domain memory allocation
 * policies and iterators.
 */

/*
 * A VM domain policy represents a desired VM domain policy.
 * Iterators implement searching through VM domains in a specific
 * order.
 */

/*
 * When setting a policy, the caller must establish their own
 * exclusive write protection for the contents of the domain
 * policy.
 */
int
vm_domain_policy_init(struct vm_domain_policy *vp)
{

	bzero(vp, sizeof(*vp));
	vp->p.policy = VM_POLICY_NONE;
	vp->p.domain = -1;
	return (0);
}

int
vm_domain_policy_set(struct vm_domain_policy *vp,
    vm_domain_policy_type_t vt, int domain)
{

	seq_write_begin(&vp->seq);
	vp->p.policy = vt;
	vp->p.domain = domain;
	seq_write_end(&vp->seq);
	return (0);
}

/*
 * Take a local copy of a policy.
 *
 * The destination policy isn't write-barriered; this is used
 * for doing local copies into something that isn't shared.
 */
void
vm_domain_policy_localcopy(struct vm_domain_policy *dst,
    const struct vm_domain_policy *src)
{
	seq_t seq;

	for (;;) {
		seq = seq_read(&src->seq);
		*dst = *src;
		if (seq_consistent(&src->seq, seq))
			return;
		cpu_spinwait();
	}
}

/*
 * Take a write-barrier copy of a policy.
 *
 * The destination policy is write -barriered; this is used
 * for doing copies into policies that may be read by other
 * threads.
 */
void
vm_domain_policy_copy(struct vm_domain_policy *dst,
    const struct vm_domain_policy *src)
{
	seq_t seq;
	struct vm_domain_policy d;

	for (;;) {
		seq = seq_read(&src->seq);
		d = *src;
		if (seq_consistent(&src->seq, seq)) {
			seq_write_begin(&dst->seq);
			dst->p.domain = d.p.domain;
			dst->p.policy = d.p.policy;
			seq_write_end(&dst->seq);
			return;
		}
		cpu_spinwait();
	}
}

int
vm_domain_policy_validate(const struct vm_domain_policy *vp)
{

	switch (vp->p.policy) {
	case VM_POLICY_NONE:
	case VM_POLICY_ROUND_ROBIN:
	case VM_POLICY_FIRST_TOUCH:
	case VM_POLICY_FIRST_TOUCH_ROUND_ROBIN:
		if (vp->p.domain == -1)
			return (0);
		return (-1);
	case VM_POLICY_FIXED_DOMAIN:
	case VM_POLICY_FIXED_DOMAIN_ROUND_ROBIN:
#ifdef VM_NUMA_ALLOC
		if (vp->p.domain >= 0 && vp->p.domain < vm_ndomains)
			return (0);
#else
		if (vp->p.domain == 0)
			return (0);
#endif
		return (-1);
	default:
		return (-1);
	}
	return (-1);
}

int
vm_domain_policy_cleanup(struct vm_domain_policy *vp)
{

	/* For now, empty */
	return (0);
}

int
vm_domain_iterator_init(struct vm_domain_iterator *vi)
{

	/* Nothing to do for now */
	return (0);
}

/*
 * Manually setup an iterator with the given details.
 */
int
vm_domain_iterator_set(struct vm_domain_iterator *vi,
    vm_domain_policy_type_t vt, int domain)
{

#ifdef VM_NUMA_ALLOC
	switch (vt) {
	case VM_POLICY_FIXED_DOMAIN:
		vi->policy = VM_POLICY_FIXED_DOMAIN;
		vi->domain = domain;
		vi->n = 1;
		break;
	case VM_POLICY_FIXED_DOMAIN_ROUND_ROBIN:
		vi->policy = VM_POLICY_FIXED_DOMAIN_ROUND_ROBIN;
		vi->domain = domain;
		vi->n = vm_ndomains;
		break;
	case VM_POLICY_FIRST_TOUCH:
		vi->policy = VM_POLICY_FIRST_TOUCH;
		vi->domain = PCPU_GET(domain);
		vi->n = 1;
		break;
	case VM_POLICY_FIRST_TOUCH_ROUND_ROBIN:
		vi->policy = VM_POLICY_FIRST_TOUCH_ROUND_ROBIN;
		vi->domain = PCPU_GET(domain);
		vi->n = vm_ndomains;
		break;
	case VM_POLICY_ROUND_ROBIN:
	default:
		vi->policy = VM_POLICY_ROUND_ROBIN;
		vi->domain = -1;
		vi->n = vm_ndomains;
		break;
	}
#else
	vi->domain = 0;
	vi->n = 1;
#endif
	return (0);
}

/*
 * Setup an iterator based on the given policy.
 */
static inline void
_vm_domain_iterator_set_policy(struct vm_domain_iterator *vi,
    const struct vm_domain_policy *vt)
{

#ifdef VM_NUMA_ALLOC
	/*
	 * Initialise the iterator.
	 *
	 * For first-touch, the initial domain is set
	 * via the current thread CPU domain.
	 *
	 * For fixed-domain, it's assumed that the
	 * caller has initialised the specific domain
	 * it is after.
	 */
	switch (vt->p.policy) {
	case VM_POLICY_FIXED_DOMAIN:
		vi->policy = vt->p.policy;
		vi->domain = vt->p.domain;
		vi->n = 1;
		break;
	case VM_POLICY_FIXED_DOMAIN_ROUND_ROBIN:
		vi->policy = vt->p.policy;
		vi->domain = vt->p.domain;
		vi->n = vm_ndomains;
		break;
	case VM_POLICY_FIRST_TOUCH:
		vi->policy = vt->p.policy;
		vi->domain = PCPU_GET(domain);
		vi->n = 1;
		break;
	case VM_POLICY_FIRST_TOUCH_ROUND_ROBIN:
		vi->policy = vt->p.policy;
		vi->domain = PCPU_GET(domain);
		vi->n = vm_ndomains;
		break;
	case VM_POLICY_ROUND_ROBIN:
	default:
		/*
		 * Default to round-robin policy.
		 */
		vi->policy = VM_POLICY_ROUND_ROBIN;
		vi->domain = -1;
		vi->n = vm_ndomains;
		break;
	}
#else
	vi->domain = 0;
	vi->n = 1;
#endif
}

void
vm_domain_iterator_set_policy(struct vm_domain_iterator *vi,
    const struct vm_domain_policy *vt)
{
	seq_t seq;
	struct vm_domain_policy vt_lcl;

	for (;;) {
		seq = seq_read(&vt->seq);
		vt_lcl = *vt;
		if (seq_consistent(&vt->seq, seq)) {
			_vm_domain_iterator_set_policy(vi, &vt_lcl);
			return;
		}
		cpu_spinwait();
	}
}

/*
 * Return the next VM domain to use.
 *
 * Returns 0 w/ domain set to the next domain to use, or
 * -1 to indicate no more domains are available.
 */
int
vm_domain_iterator_run(struct vm_domain_iterator *vi, int *domain)
{

	/* General catch-all */
	if (vi->n <= 0)
		return (-1);

#ifdef VM_NUMA_ALLOC
	switch (vi->policy) {
	case VM_POLICY_FIXED_DOMAIN:
	case VM_POLICY_FIRST_TOUCH:
		*domain = vi->domain;
		vi->n--;
		break;
	case VM_POLICY_FIXED_DOMAIN_ROUND_ROBIN:
	case VM_POLICY_FIRST_TOUCH_ROUND_ROBIN:
		/*
		 * XXX TODO: skip over the rr'ed domain
		 * if it equals the one we started with.
		 */
		if (vi->n == vm_ndomains)
			*domain = vi->domain;
		else
			*domain = vm_domain_rr_selectdomain(vi->domain);
		vi->n--;
		break;
	case VM_POLICY_ROUND_ROBIN:
	default:
		*domain = vm_domain_rr_selectdomain(-1);
		vi->n--;
		break;
	}
#else
	*domain = 0;
	vi->n--;
#endif

	return (0);
}

/*
 * Returns 1 if the iteration is done, or 0 if it has not.

 * This can only be called after at least one loop through
 * the iterator.  Ie, it's designed to be used as a tail
 * check of a loop, not the head check of a loop.
 */
int
vm_domain_iterator_isdone(struct vm_domain_iterator *vi)
{

	return (vi->n <= 0);
}

int
vm_domain_iterator_cleanup(struct vm_domain_iterator *vi)
{

	return (0);
}
