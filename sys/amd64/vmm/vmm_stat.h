/*-
 * Copyright (c) 2011 NetApp, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _VMM_STAT_H_
#define	_VMM_STAT_H_

struct vm;

#define	MAX_VMM_STAT_TYPES	64		/* arbitrary */

enum vmm_stat_scope {
	VMM_STAT_SCOPE_ANY,
	VMM_STAT_SCOPE_INTEL,		/* Intel VMX specific statistic */
	VMM_STAT_SCOPE_AMD,		/* AMD SVM specific statistic */
};

struct vmm_stat_type {
	int	index;			/* position in the stats buffer */
	const char *desc;		/* description of statistic */
	enum vmm_stat_scope scope;
};

void	vmm_stat_init(void *arg);

#define	VMM_STAT_DEFINE(type, desc, scope)				\
	struct vmm_stat_type type[1] = {				\
		{ -1, desc, scope }					\
	};								\
	SYSINIT(type##_stat, SI_SUB_KLD, SI_ORDER_ANY, vmm_stat_init, type)

#define	VMM_STAT_DECLARE(type)						\
	extern struct vmm_stat_type type[1]

#define	VMM_STAT(type, desc)		\
	VMM_STAT_DEFINE(type, desc, VMM_STAT_SCOPE_ANY)
#define	VMM_STAT_INTEL(type, desc)	\
	VMM_STAT_DEFINE(type, desc, VMM_STAT_SCOPE_INTEL)
#define	VMM_STAT_AMD(type, desc)	\
	VMM_STAT_DEFINE(type, desc, VMM_STAT_SCOPE_AMD)

void	*vmm_stat_alloc(void);
void 	vmm_stat_free(void *vp);

/*
 * 'buf' should be at least fit 'MAX_VMM_STAT_TYPES' entries
 */
int	vmm_stat_copy(struct vm *vm, int vcpu, int *num_stats, uint64_t *buf);
const char *vmm_stat_desc(int index);

static void __inline
vmm_stat_incr(struct vm *vm, int vcpu, struct vmm_stat_type *vst, uint64_t x)
{
#ifdef	VMM_KEEP_STATS
	uint64_t *stats = vcpu_stats(vm, vcpu);
	if (vst->index >= 0)
		stats[vst->index] += x;
#endif
}

VMM_STAT_DECLARE(VCPU_MIGRATIONS);
VMM_STAT_DECLARE(VMEXIT_COUNT);
VMM_STAT_DECLARE(VMEXIT_EXTINT);
VMM_STAT_DECLARE(VMEXIT_HLT);
VMM_STAT_DECLARE(VMEXIT_CR_ACCESS);
VMM_STAT_DECLARE(VMEXIT_RDMSR);
VMM_STAT_DECLARE(VMEXIT_WRMSR);
VMM_STAT_DECLARE(VMEXIT_MTRAP);
VMM_STAT_DECLARE(VMEXIT_PAUSE);
VMM_STAT_DECLARE(VMEXIT_INTR_WINDOW);
VMM_STAT_DECLARE(VMEXIT_NMI_WINDOW);
VMM_STAT_DECLARE(VMEXIT_INOUT);
VMM_STAT_DECLARE(VMEXIT_CPUID);
VMM_STAT_DECLARE(VMEXIT_EPT_FAULT);
VMM_STAT_DECLARE(VMEXIT_UNKNOWN);
VMM_STAT_DECLARE(VMEXIT_ASTPENDING);
VMM_STAT_DECLARE(VMEXIT_USERSPACE);
#endif
