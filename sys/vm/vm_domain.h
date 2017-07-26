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
 *
 * $FreeBSD$
 */
#ifndef	__VM_DOMAIN_H__
#define	__VM_DOMAIN_H__

#include <sys/_vm_domain.h>

extern int vm_ndomains;
extern struct vm_domain_policy *vm_default_policy;

static inline int
vm_domain_select_first(struct vm_domain_iterator *vi)
{
	int domain;

	switch (vi->policy) {
	case VM_POLICY_NONE:
		domain = 0;
		break;
	case VM_POLICY_ROUND_ROBIN:
		domain = atomic_fetchadd_int(&vi->cursor, 1) % vm_ndomains;
		break;
	case VM_POLICY_FIXED_DOMAIN:
	case VM_POLICY_FIXED_DOMAIN_ROUND_ROBIN:
		domain = vi->domain;
		break;
	case VM_POLICY_FIRST_TOUCH:
	case VM_POLICY_FIRST_TOUCH_ROUND_ROBIN:
		domain = PCPU_GET(domain);
		break;
	}
	return (domain);
}

static inline int
vm_domain_select_next(struct vm_domain_iterator *vi, int domain)
{

	switch (vi->policy) {
	case VM_POLICY_FIXED_DOMAIN:
	case VM_POLICY_FIRST_TOUCH:
		return (-1);
	default:
		return ((domain + 1) % vm_ndomains);
	}
}

/*
 * TODO: check to see if these should just become inline functions
 * at some point.
 */
extern	int vm_domain_policy_init(struct vm_domain_policy *vp);
extern	int vm_domain_policy_set(struct vm_domain_policy *vp,
	    vm_domain_policy_type_t vt, int domain);
extern	int vm_domain_policy_cleanup(struct vm_domain_policy *vp);
extern	void vm_domain_policy_localcopy(struct vm_domain_policy *dst,
	    const struct vm_domain_policy *src);
extern	void vm_domain_policy_copy(struct vm_domain_policy *dst,
	    const struct vm_domain_policy *src);
extern	int vm_domain_policy_validate(const struct vm_domain_policy *vp);

extern	int vm_domain_iterator_init(struct vm_domain_iterator *vi);
extern	int vm_domain_iterator_set(struct vm_domain_iterator *vi,
	    vm_domain_policy_type_t vt, int domain);
extern	void vm_domain_iterator_set_policy(struct vm_domain_iterator *vi,
	    const struct vm_domain_policy *vt);
extern	int vm_domain_iterator_run(struct vm_domain_iterator *vi,
	    int *domain);
extern	int vm_domain_iterator_isdone(struct vm_domain_iterator *vi);
extern	int vm_domain_iterator_cleanup(struct vm_domain_iterator *vi);

#endif	/* __VM_DOMAIN_H__ */
