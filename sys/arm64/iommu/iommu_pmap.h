/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Ruslan Bukin <br@bsdpad.com>
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#ifndef	_ARM64_IOMMU_IOMMU_PMAP_H_
#define	_ARM64_IOMMU_IOMMU_PMAP_H_

struct smmu_pmap {
	struct mtx	sp_mtx;
	vm_paddr_t	sp_l0_paddr;
	pd_entry_t	*sp_l0;
#ifdef INVARIANTS
	long		sp_resident_count;
#endif
};

/* System MMU (SMMU). */
int smmu_pmap_enter(struct smmu_pmap *pmap, vm_offset_t va, vm_paddr_t pa,
    vm_prot_t prot, u_int flags);
int smmu_pmap_remove(struct smmu_pmap *pmap, vm_offset_t va);

/* Mali GPU */
int pmap_gpu_enter(struct smmu_pmap *pmap, vm_offset_t va, vm_paddr_t pa,
    vm_prot_t prot, u_int flags);
int pmap_gpu_remove(struct smmu_pmap *pmap, vm_offset_t va);

/* Common */
void smmu_pmap_remove_pages(struct smmu_pmap *pmap);
void smmu_pmap_release(struct smmu_pmap *pmap);
int smmu_pmap_pinit(struct smmu_pmap *pmap);

#endif /* !_ARM64_IOMMU_IOMMU_PMAP_H_ */
