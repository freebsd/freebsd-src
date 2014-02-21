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

#ifndef _VM__DOMAIN_H_
#define	_VM__DOMAIN_H_

#include <sys/_bitset.h>
#include <sys/bitset.h>

#ifdef _KERNEL
#define	VM_DOMAIN_SETSIZE	MAXMEMDOM
#endif

#define	VM_DOMAIN_MAXSIZE	64

#ifndef	VM_DOMAIN_SETSIZE
#define	VM_DOMAIN_SETSIZE	VM_DOMAIN_MAXSIZE
#endif

#define	_NVM_DOMAINBITS		_BITSET_BITS
#define	_NVM_DOMAINWORDS	__bitset_words(VM_DOMAIN_SETSIZE)

BITSET_DEFINE(_vm_domainset, VM_DOMAIN_SETSIZE);
typedef struct _vm_domainset vm_domainset_t;

#define	VM_DOMAIN_FSET		BITSET_FSET(_NVM_DOMAINWORDS)
#define	VM_DOMAIN_T_INITIALIZER	BITSET_T_INITIALIZER
#define	VM_DOMAIN_SETBUFSIZ	((2 + sizeof(long) * 2) * _NVM_DOMAINWORDS)

#ifdef _KERNEL

/*
 * Valid memory domain (NUMA) policy values.
 */
enum vm_domain_policy {
	ROUNDROBIN,		/* Select between any in the set. */
	FIRSTTOUCH		/* Select the current domain. */
};

/*
 * The select structure encapsulate domain allocation strategy with
 * allocator information.
 */
struct vm_domain_select {
	vm_domainset_t		ds_mask;	/* bitmask of valid domains. */
	enum vm_domain_policy	ds_policy;	/* Allocation policy. */
	int			ds_cursor;	/* Allocation cursor. */
	int			ds_count;	/* Domains in policy. */
};

#endif /* _KERNEL */

#endif /* !_VM__DOMAIN_H_ */
