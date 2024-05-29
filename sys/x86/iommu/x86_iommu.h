/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013-2015, 2024 The FreeBSD Foundation
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

#ifndef __X86_IOMMU_X86_IOMMU_H
#define	__X86_IOMMU_X86_IOMMU_H

/* Both Intel and AMD are not too crazy to have different sizes. */
typedef struct iommu_pte {
	uint64_t pte;
} iommu_pte_t;

#define	IOMMU_PAGE_SIZE		PAGE_SIZE
#define	IOMMU_PAGE_MASK		(IOMMU_PAGE_SIZE - 1)
#define	IOMMU_PAGE_SHIFT	PAGE_SHIFT
#define	IOMMU_NPTEPG		(IOMMU_PAGE_SIZE / sizeof(iommu_pte_t))
#define	IOMMU_NPTEPGSHIFT 	9
#define	IOMMU_PTEMASK		(IOMMU_NPTEPG - 1)

struct sf_buf;
struct vm_object;

struct vm_page *iommu_pgalloc(struct vm_object *obj, vm_pindex_t idx,
    int flags);
void iommu_pgfree(struct vm_object *obj, vm_pindex_t idx, int flags);
void *iommu_map_pgtbl(struct vm_object *obj, vm_pindex_t idx, int flags,
    struct sf_buf **sf);
void iommu_unmap_pgtbl(struct sf_buf *sf);

extern iommu_haddr_t iommu_high;
extern int iommu_tbl_pagecnt;

SYSCTL_DECL(_hw_iommu);
SYSCTL_DECL(_hw_iommu_dmar);

struct x86_iommu {
	void (*domain_unload_entry)(struct iommu_map_entry *entry, bool free,
	    bool cansleep);
	void (*domain_unload)(struct iommu_domain *iodom,
		struct iommu_map_entries_tailq *entries, bool cansleep);
	struct iommu_ctx *(*get_ctx)(struct iommu_unit *iommu,
	    device_t dev, uint16_t rid, bool id_mapped, bool rmrr_init);
	void (*free_ctx_locked)(struct iommu_unit *iommu,
	    struct iommu_ctx *context);
	void (*free_ctx)(struct iommu_ctx *context);
	struct iommu_unit *(*find)(device_t dev, bool verbose);
	int (*alloc_msi_intr)(device_t src, u_int *cookies, u_int count);
	int (*map_msi_intr)(device_t src, u_int cpu, u_int vector,
	    u_int cookie, uint64_t *addr, uint32_t *data);
	int (*unmap_msi_intr)(device_t src, u_int cookie);
	int (*map_ioapic_intr)(u_int ioapic_id, u_int cpu, u_int vector,
	    bool edge, bool activehi, int irq, u_int *cookie, uint32_t *hi,
	    uint32_t *lo);
	int (*unmap_ioapic_intr)(u_int ioapic_id, u_int *cookie);
};
void set_x86_iommu(struct x86_iommu *);
struct x86_iommu *get_x86_iommu(void);

#endif
