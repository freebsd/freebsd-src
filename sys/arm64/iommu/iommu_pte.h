/*-
 * Copyright (c) 2014 Andrew Turner
 * Copyright (c) 2014-2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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

#ifndef _ARM64_IOMMU_IOMMU_PTE_H_
#define	_ARM64_IOMMU_IOMMU_PTE_H_

/* Level 0 table, 512GiB per entry */
#define	IOMMU_L0_SHIFT		39
#define	IOMMU_L0_INVAL		0x0 /* An invalid address */
		/* 0x1 Level 0 doesn't support block translation */
		/* 0x2 also marks an invalid address */
#define	IOMMU_L0_TABLE		0x3 /* A next-level table */

/* Level 1 table, 1GiB per entry */
#define	IOMMU_L1_SHIFT		30
#define	IOMMU_L1_INVAL		IOMMU_L0_INVAL
#define	IOMMU_L1_BLOCK		0x1
#define	IOMMU_L1_TABLE		IOMMU_L0_TABLE

/* Level 2 table, 2MiB per entry */
#define	IOMMU_L2_SHIFT		21
#define	IOMMU_L2_INVAL		IOMMU_L1_INVAL
#define	IOMMU_L2_BLOCK		IOMMU_L1_BLOCK
#define	IOMMU_L2_TABLE		IOMMU_L1_TABLE

/* Level 3 table, 4KiB per entry */
#define	IOMMU_L3_SHIFT		12
#define	IOMMU_L3_SIZE 		(1 << IOMMU_L3_SHIFT)
#define	IOMMU_L3_SHIFT		12
#define	IOMMU_L3_INVAL		0x0
	/* 0x1 is reserved */
	/* 0x2 also marks an invalid address */
#define	IOMMU_L3_PAGE		0x3
#define	IOMMU_L3_BLOCK		IOMMU_L2_BLOCK	/* Mali GPU only. */

#define	IOMMU_L0_ENTRIES_SHIFT	9
#define	IOMMU_L0_ENTRIES	(1 << IOMMU_L0_ENTRIES_SHIFT)
#define	IOMMU_L0_ADDR_MASK	(IOMMU_L0_ENTRIES - 1)

#define	IOMMU_Ln_ENTRIES_SHIFT	9
#define	IOMMU_Ln_ENTRIES	(1 << IOMMU_Ln_ENTRIES_SHIFT)
#define	IOMMU_Ln_ADDR_MASK	(IOMMU_Ln_ENTRIES - 1)
#define	IOMMU_Ln_TABLE_MASK	((1 << 12) - 1)

#endif /* !_ARM64_IOMMU_IOMMU_PTE_H_ */
