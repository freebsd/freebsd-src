/*-
 * Copyright (c) 2014,	Jeffrey Roberson <jeff@freebsd.org>
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
 * $FreeBSD$
 */

#ifndef _VM_DOMAIN_H_
#define	_VM_DOMAIN_H_

#include <sys/bitset.h>

#include <vm/_vm_domain.h>

#define	VM_DOMAIN_CLR(n, p)		BIT_CLR(VM_DOMAIN_SETSIZE, n, p)
#define	VM_DOMAIN_COPY(f, t)		BIT_COPY(VM_DOMAIN_SETSIZE, f, t)
#define	VM_DOMAIN_ISSET(n, p)		BIT_ISSET(VM_DOMAIN_SETSIZE, n, p)
#define	VM_DOMAIN_SET(n, p)		BIT_SET(VM_DOMAIN_SETSIZE, n, p)
#define	VM_DOMAIN_ZERO(p) 		BIT_ZERO(VM_DOMAIN_SETSIZE, p)
#define	VM_DOMAIN_FILL(p) 		BIT_FILL(VM_DOMAIN_SETSIZE, p)
#define	VM_DOMAIN_SETOF(n, p)		BIT_SETOF(VM_DOMAIN_SETSIZE, n, p)
#define	VM_DOMAIN_EMPTY(p)		BIT_EMPTY(VM_DOMAIN_SETSIZE, p)
#define	VM_DOMAIN_ISFULLSET(p)		BIT_ISFULLSET(VM_DOMAIN_SETSIZE, p)
#define	VM_DOMAIN_SUBSET(p, c)		BIT_SUBSET(VM_DOMAIN_SETSIZE, p, c)
#define	VM_DOMAIN_OVERLAP(p, c)		BIT_OVERLAP(VM_DOMAIN_SETSIZE, p, c)
#define	VM_DOMAIN_CMP(p, c)		BIT_CMP(VM_DOMAIN_SETSIZE, p, c)
#define	VM_DOMAIN_OR(d, s)		BIT_OR(VM_DOMAIN_SETSIZE, d, s)
#define	VM_DOMAIN_AND(d, s)		BIT_AND(VM_DOMAIN_SETSIZE, d, s)
#define	VM_DOMAIN_NAND(d, s)		BIT_NAND(VM_DOMAIN_SETSIZE, d, s)
#define	VM_DOMAIN_CLR_ATOMIC(n, p)	BIT_CLR_ATOMIC(VM_DOMAIN_SETSIZE, n, p)
#define	VM_DOMAIN_SET_ATOMIC(n, p)	BIT_SET_ATOMIC(VM_DOMAIN_SETSIZE, n, p)
#define	VM_DOMAIN_AND_ATOMIC(n, p)	BIT_AND_ATOMIC(VM_DOMAIN_SETSIZE, n, p)
#define	VM_DOMAIN_OR_ATOMIC(d, s)	BIT_OR_ATOMIC(VM_DOMAIN_SETSIZE, d, s)
#define	VM_DOMAIN_COPY_STORE_REL(f, t)	BIT_COPY_STORE_REL(VM_DOMAIN_SETSIZE, f, t)
#define	VM_DOMAIN_FFS(p)		BIT_FFS(VM_DOMAIN_SETSIZE, p)

#ifdef _KERNEL

/*
 * Domain sets.
 */
extern vm_domainset_t vm_alldomains;		/* All domains. */
extern vm_domainset_t vm_domset[MAXMEMDOM];	/* Specific domain bitmask. */
extern int vm_ndomains;

/*
 * Domain allocation selectors.
 */
extern struct vm_domain_select vm_sel_def;		/* default */
extern struct vm_domain_select vm_sel_rr;		/* round-robin */
extern struct vm_domain_select vm_sel_ft;		/* first-touch */
extern struct vm_domain_select vm_sel_dom[MAXMEMDOM];	/* specific domain */

static inline int
vm_domain_select_next(struct vm_domain_select *sel, int domain)
{

	switch (sel->ds_policy) {
	case FIRSTTOUCH:
		/* FALLTHROUGH */
	case ROUNDROBIN:
		do {
			domain = (domain + 1) % vm_ndomains;
		} while (!VM_DOMAIN_ISSET(domain, &sel->ds_mask));
	}
	return (domain);
}

static inline int
vm_domain_select_first(struct vm_domain_select *sel)
{
	int domain;

	switch (sel->ds_policy) {
	case FIRSTTOUCH:
		domain = PCPU_GET(domain);
		if (VM_DOMAIN_ISSET(domain, &sel->ds_mask))
			break;
		/* FALLTHROUGH */
	case ROUNDROBIN:
		domain = atomic_fetchadd_int(&sel->ds_cursor, 1) % vm_ndomains;
		if (!VM_DOMAIN_ISSET(domain, &sel->ds_mask))
			domain = vm_domain_select_next(sel, domain);
	}
	return (domain);
}

#endif /* _KERNEL */

#endif /* !_VM_DOMAIN_H_ */
