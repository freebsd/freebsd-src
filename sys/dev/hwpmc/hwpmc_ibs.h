/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026, Ali Jose Mashtizadeh
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

#ifndef	_DEV_HWPMC_IBS_H_
#define	_DEV_HWPMC_IBS_H_ 1

#define	IBS_NPMCS			2
#define	IBS_PMC_FETCH			0
#define	IBS_PMC_OP			1

/*
 * All of the CPUID definitions come from AMD PPR Vol 1 for AMD Family 1Ah
 * Model 02h C1 (57238) 2024-09-29 Revision 0.24.
 */
#define	CPUID_IBSID			0x8000001B
#define	CPUID_IBSID_IBSFFV		0x00000001 /* IBS Feature Flags Valid */
#define	CPUID_IBSID_FETCHSAM		0x00000002 /* IBS Fetch Sampling */
#define	CPUID_IBSID_OPSAM		0x00000004 /* IBS Execution Sampling */
#define	CPUID_IBSID_RDWROPCNT		0x00000008 /* RdWr Operationg Counter */
#define	CPUID_IBSID_OPCNT		0x00000010 /* Operation Counter */
#define	CPUID_IBSID_BRNTRGT		0x00000020 /* Branch Target Address */
#define	CPUID_IBSID_OPCNTEXT		0x00000040 /* Extend Counter */
#define	CPUID_IBSID_RIPINVALIDCHK	0x00000080 /* Invalid RIP Indication */
#define	CPUID_IBSID_OPFUSE		0x00000010 /* Fused Branch Operation */
#define	CPUID_IBSID_IBSFETCHCTLEXTD	0x00000020 /* IBS Fetch Control Ext */
#define	CPUID_IBSID_IBSOPDATA4		0x00000040 /* IBS OP DATA4 */
#define	CPUID_IBSID_ZEN4IBSEXTENSIONS	0x00000080 /* IBS Zen 4 Extensions */
#define	CPUID_IBSID_IBSLOADLATENCYFILT	0x00000100 /* Load Latency Filtering */
#define	CPUID_IBSID_IBSUPDTDDTLBSTATS	0x00080000 /* Simplified DTLB Stats */

/*
 * All of these definitions here come from AMD64 Architecture Programmer's
 * Manual Volume 2: System Programming (24593) 2025-07-02 Version 3.43. with
 * the following exceptions:
 *
 * OpData4 and fields come from the BKDG for AMD Family 15h Model 70-7Fh
 * (55072) 2018-06-20 Revision 3.09.
 */

/* IBS MSRs */
#define IBS_CTL				0xC001103A /* IBS Control */
#define IBS_CTL_LVTOFFSETVALID		(1ULL << 8)
#define IBS_CTL_LVTOFFSETMASK		0x0000000F

/* IBS Fetch Control */
#define IBS_FETCH_CTL			0xC0011030 /* IBS Fetch Control */
#define IBS_FETCH_CTL_L3MISS		(1ULL << 61) /* L3 Cache Miss */
#define IBS_FETCH_CTL_OPCACHEMISS	(1ULL << 60) /* Op Cache Miss */
#define IBS_FETCH_CTL_L3MISSONLY	(1ULL << 59) /* L3 Miss Filtering */
#define IBS_FETCH_CTL_RANDOMIZE		(1ULL << 57) /* Randomized Tagging */
#define IBS_FETCH_CTL_L1TLBMISS		(1ULL << 55) /* L1 TLB Miss */
// Page size 54:53
#define IBS_FETCH_CTL_PHYSADDRVALID	(1ULL << 52) /* PHYSADDR Valid */
#define IBS_FETCH_CTL_ICMISS		(1ULL << 51) /* Inst. Cache Miss */
#define IBS_FETCH_CTL_COMPLETE		(1ULL << 50) /* Complete */
#define IBS_FETCH_CTL_VALID		(1ULL << 49) /* Valid */
#define IBS_FETCH_CTL_ENABLE		(1ULL << 48) /* Enable */
#define IBS_FETCH_CTL_MAXCNTMASK	0x0000FFFFULL

#define IBS_FETCH_CTL_TO_LAT(_c)	((_c >> 32) & 0x0000FFFF)

#define IBS_FETCH_LINADDR		0xC0011031 /* Fetch Linear Address */
#define IBS_FETCH_PHYSADDR		0xC0011032 /* Fetch Physical Address */
#define IBS_FETCH_EXTCTL		0xC001103C /* Fetch Control Extended */

#define PMC_MPIDX_FETCH_CTL		0
#define PMC_MPIDX_FETCH_EXTCTL		1
#define PMC_MPIDX_FETCH_LINADDR		2
#define PMC_MPIDX_FETCH_PHYSADDR	3

/* IBS Execution Control */
#define IBS_OP_CTL			0xC0011033 /* IBS Execution Control */
#define IBS_OP_CTL_COUNTERCONTROL	(1ULL << 19) /* Counter Control */
#define IBS_OP_CTL_VALID		(1ULL << 18) /* Valid */
#define IBS_OP_CTL_ENABLE		(1ULL << 17) /* Enable */
#define IBS_OP_CTL_L3MISSONLY		(1ULL << 16) /* L3 Miss Filtering */
#define IBS_OP_CTL_MAXCNTMASK		0x0000FFFFULL

#define IBS_OP_RIP			0xC0011034 /* IBS Op RIP */
#define IBS_OP_DATA			0xC0011035 /* IBS Op Data */
#define IBS_OP_DATA_RIPINVALID		(1ULL << 38) /* RIP Invalid */
#define IBS_OP_DATA_BRANCHRETIRED	(1ULL << 37) /* Branch Retired */
#define IBS_OP_DATA_BRANCHMISPREDICTED	(1ULL << 36) /* Branch Mispredicted */
#define IBS_OP_DATA_BRANCHTAKEN		(1ULL << 35) /* Branch Taken */
#define IBS_OP_DATA_RETURN		(1ULL << 34) /* Return */

#define IBS_OP_DATA2			0xC0011036 /* IBS Op Data 2 */
#define IBS_OP_DATA3			0xC0011037 /* IBS Op Data 3 */
#define IBS_OP_DATA3_DCPHYADDRVALID	(1ULL << 18) /* DC Physical Address */
#define IBS_OP_DATA3_DCLINADDRVALID	(1ULL << 17) /* DC Linear Address */
#define IBS_OP_DATA3_LOCKEDOP		(1ULL << 15) /* DC Locked Op */
#define IBS_OP_DATA3_UCMEMACCESS	(1ULL << 14) /* DC UC Memory Access */
#define IBS_OP_DATA3_WCMEMACCESS	(1ULL << 13) /* DC WC Memory Access */
#define IBS_OP_DATA3_DCMISALIGN		(1ULL << 8)  /* DC Misaligned Access */
#define IBS_OP_DATA3_DCMISS		(1ULL << 7)  /* DC Miss */
#define IBS_OP_DATA3_DCL1TLBHIT1G	(1ULL << 5)  /* DC L1 TLB Hit 1-GB */
#define IBS_OP_DATA3_DCL1TLBHIT2M	(1ULL << 4)  /* DC L1 TLB Hit 2-MB */
#define IBS_OP_DATA3_DCL1TLBMISS	(1ULL << 2)  /* DC L1 TLB Miss */
#define IBS_OP_DATA3_STORE		(1ULL << 1)  /* Store */
#define IBS_OP_DATA3_LOAD		(1ULL << 0)  /* Load */
#define IBS_OP_DATA3_TO_DCLAT(_c)	((_c >> 32) & 0x0000FFFF)

#define IBS_OP_DC_LINADDR		0xC0011038 /* IBS DC Linear Address */
#define IBS_OP_DC_PHYSADDR		0xC0011039 /* IBS DC Physical Address */
#define IBS_TGT_RIP			0xC001103B /* IBS Branch Target */
#define IBS_OP_DATA4			0xC001103D /* IBS Op Data 4 */
#define IBS_OP_DATA4_LDRESYNC		(1ULL << 0)  /* Load Resync */

#define PMC_MPIDX_OP_CTL		0
#define PMC_MPIDX_OP_RIP		1
#define PMC_MPIDX_OP_DATA		2
#define PMC_MPIDX_OP_DATA2		3
#define PMC_MPIDX_OP_DATA3		4
#define PMC_MPIDX_OP_DC_LINADDR		5
#define PMC_MPIDX_OP_DC_PHYSADDR	6
#define PMC_MPIDX_OP_TGT_RIP		7
#define PMC_MPIDX_OP_DATA4		8

/*
 * IBS data is encoded as using the multipart flag in the existing callchain
 * structure.  The PMC ID number tells you if the sample contains a fetch or an
 * op sample.  The available payload will be encoded in the MSR order with a
 * variable length.
 */

struct pmc_md_ibs_op_pmcallocate {
	uint32_t	ibs_flag;
	uint32_t	ibs_type;
	uint64_t	ibs_ctl;
	uint64_t	ibs_ctl2;
};

#ifdef _KERNEL

/* MD extension for 'struct pmc' */
struct pmc_md_ibs_pmc {
	uint32_t	ibs_flag;
	uint32_t	ibs_type;
	uint64_t	ibs_ctl;
	uint64_t	ibs_ctl2;
};

#define IBS_PMC_CAPS			(PMC_CAP_INTERRUPT | PMC_CAP_SYSTEM | \
	PMC_CAP_EDGE | PMC_CAP_QUALIFIER | PMC_CAP_PRECISE)

int	pmc_ibs_initialize(struct pmc_mdep *md, int ncpu);
void	pmc_ibs_finalize(struct pmc_mdep *md);
int	pmc_ibs_intr(struct trapframe *tf);

#endif /* _KERNEL */
#endif /* _DEV_HWPMC_IBS_H_ */
