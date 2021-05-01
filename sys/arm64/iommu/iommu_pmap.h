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
 *
 * $FreeBSD$
 */

#ifndef	_ARM64_IOMMU_IOMMU_PMAP_H_
#define	_ARM64_IOMMU_IOMMU_PMAP_H_

/* System MMU (SMMU). */
int pmap_smmu_enter(pmap_t pmap, vm_offset_t va, vm_paddr_t pa, vm_prot_t prot,
    u_int flags);
int pmap_smmu_remove(pmap_t pmap, vm_offset_t va);

/* Mali GPU */
int pmap_gpu_enter(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
    vm_prot_t prot, u_int flags);
int pmap_gpu_remove(pmap_t pmap, vm_offset_t va);

/* Common */
void iommu_pmap_remove_pages(pmap_t pmap);
void iommu_pmap_release(pmap_t pmap);
int iommu_pmap_pinit(pmap_t);

#endif /* !_ARM64_IOMMU_IOMMU_PMAP_H_ */
