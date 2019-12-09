/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Leandro Lupori
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#ifndef	__KVM_POWERPC64_H__
#define	__KVM_POWERPC64_H__

/* Debug stuff */
#define	KVM_PPC64_DBG	0
#if	KVM_PPC64_DBG
#include <stdio.h>
#define	dprintf(fmt, ...)	printf(fmt, ## __VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif


#define	PPC64_KERNBASE		0x100100ULL

/* Page params */
#define	PPC64_PAGE_SHIFT	12
#define	PPC64_PAGE_SIZE		(1ULL << PPC64_PAGE_SHIFT)
#define	PPC64_PAGE_MASK		(PPC64_PAGE_SIZE - 1)

#define	ppc64_round_page(x)	roundup2((kvaddr_t)(x), PPC64_PAGE_SIZE)

#define	PPC64_MMU_G5		"mmu_g5"
#define	PPC64_MMU_PHYP		"mmu_phyp"

/* MMU interface */
#define	PPC64_MMU_OPS(kd)	(kd)->vmst->mmu.ops
#define	PPC64_MMU_OP(kd, op, ...) PPC64_MMU_OPS(kd)->op((kd), ## __VA_ARGS__)
#define	PPC64_MMU_DATA(kd)	(kd)->vmst->mmu.data

struct ppc64_mmu_ops {
	int (*init)(kvm_t *);
	void (*cleanup)(kvm_t *);
	int (*kvatop)(kvm_t *, kvaddr_t, off_t *);
	int (*walk_pages)(kvm_t *, kvm_walk_pages_cb_t *, void *);
};

struct ppc64_mmu {
	struct ppc64_mmu_ops *ops;
	void *data;
};

struct vmstate {
	struct minidumphdr hdr;
	uint64_t kimg_start;
	uint64_t kimg_end;
	struct ppc64_mmu mmu;
};

extern struct ppc64_mmu_ops *ppc64_mmu_ops_hpt;

#endif /* !__KVM_POWERPC64_H__ */
