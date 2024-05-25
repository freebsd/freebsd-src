/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013-2015 The FreeBSD Foundation
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

#endif
