/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Ruslan Bukin <br@bsdpad.com>
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
 */

#ifndef	_RISCV_IOMMU_IOMMU_PMAP_H_
#define	_RISCV_IOMMU_IOMMU_PMAP_H_

struct riscv_iommu_pmap {
	pd_entry_t	*pm_top;
	enum pmap_mode	pm_mode;
	uint64_t	pm_satp;
	struct mtx	pm_mtx;
#ifdef INVARIANTS
	long		sp_resident_count;
#endif
};

int iommu_pmap_enter(struct riscv_iommu_pmap *pmap, vm_offset_t va,
    vm_paddr_t pa, vm_prot_t prot, u_int flags);
int iommu_pmap_remove(struct riscv_iommu_pmap *pmap, vm_offset_t va);
void iommu_pmap_remove_pages(struct riscv_iommu_pmap *pmap);
int iommu_pmap_pinit(struct riscv_iommu_pmap *pmap, enum pmap_mode pm_mode);
void iommu_pmap_release(struct riscv_iommu_pmap *pmap);
void iommu_pmap_remove_pages(struct riscv_iommu_pmap *pmap);

#endif /* !_RISCV_IOMMU_IOMMU_PMAP_H_ */
