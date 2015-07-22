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
#ifndef	__SYS_VM_DOMAIN_H__
#define	__SYS_VM_DOMAIN_H__

#include <sys/seq.h>

typedef enum {
	VM_POLICY_NONE,
	VM_POLICY_ROUND_ROBIN,
	VM_POLICY_FIXED_DOMAIN,
	VM_POLICY_FIXED_DOMAIN_ROUND_ROBIN,
	VM_POLICY_FIRST_TOUCH,
	VM_POLICY_FIRST_TOUCH_ROUND_ROBIN,
	VM_POLICY_MAX
} vm_domain_policy_type_t;

struct vm_domain_policy_entry {
	vm_domain_policy_type_t policy;
	int domain;
};

struct vm_domain_policy {
	seq_t seq;
	struct vm_domain_policy_entry p;
};

#define VM_DOMAIN_POLICY_STATIC_INITIALISER(vt, vd) \
	{ .seq = 0, \
	  .p.policy = vt, \
	  .p.domain = vd }

#endif	/* __SYS_VM_DOMAIN_H__ */
