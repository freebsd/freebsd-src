/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 The FreeBSD Foundation
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

#ifndef _DEV_IOMMU_IOMMU_GAS_H_
#define _DEV_IOMMU_IOMMU_GAS_H_

/* Map flags */
#define	IOMMU_MF_CANWAIT	0x0001
#define	IOMMU_MF_CANSPLIT	0x0002
#define	IOMMU_MF_RMRR		0x0004

#define	IOMMU_PGF_WAITOK	0x0001
#define	IOMMU_PGF_ZERO		0x0002
#define	IOMMU_PGF_ALLOC		0x0004
#define	IOMMU_PGF_NOALLOC	0x0008
#define	IOMMU_PGF_OBJL		0x0010

#define	IOMMU_MAP_ENTRY_PLACE	0x0001	/* Fake entry */
#define	IOMMU_MAP_ENTRY_RMRR	0x0002	/* Permanent, not linked by
					   dmamap_link */
#define	IOMMU_MAP_ENTRY_MAP	0x0004	/* Busdma created, linked by
					   dmamap_link */
#define	IOMMU_MAP_ENTRY_UNMAPPED	0x0010	/* No backing pages */
#define	IOMMU_MAP_ENTRY_REMOVING	0x0020	/* In process of removal by
						   iommu_gas_remove() */
#define	IOMMU_MAP_ENTRY_READ	0x1000	/* Read permitted */
#define	IOMMU_MAP_ENTRY_WRITE	0x2000	/* Write permitted */
#define	IOMMU_MAP_ENTRY_SNOOP	0x4000	/* Snoop */
#define	IOMMU_MAP_ENTRY_TM	0x8000	/* Transient */

#endif /* !_DEV_IOMMU_IOMMU_GAS_H_ */
