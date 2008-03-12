/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 *	from: @(#)specialreg.h	7.1 (Berkeley) 5/9/91
 * $FreeBSD$
 */

#ifndef _MACHINE_SPECIALREG_H_
#define	_MACHINE_SPECIALREG_H_

/*
 * Bits in 386 special registers:
 */
#define	CR0_PE	0x00000001	/* Protected mode Enable */
#define	CR0_MP	0x00000002	/* "Math" (fpu) Present */
#define	CR0_EM	0x00000004	/* EMulate FPU instructions. (trap ESC only) */
#define	CR0_TS	0x00000008	/* Task Switched (if MP, trap ESC and WAIT) */
#define	CR0_PG	0x80000000	/* PaGing enable */

/*
 * Bits in 486 special registers:
 */
#define	CR0_NE	0x00000020	/* Numeric Error enable (EX16 vs IRQ13) */
#define	CR0_WP	0x00010000	/* Write Protect (honor page protect in
							   all modes) */
#define	CR0_AM	0x00040000	/* Alignment Mask (set to enable AC flag) */
#define	CR0_NW  0x20000000	/* Not Write-through */
#define	CR0_CD  0x40000000	/* Cache Disable */

/*
 * Bits in PPro special registers
 */
#define	CR4_VME	0x00000001	/* Virtual 8086 mode extensions */
#define	CR4_PVI	0x00000002	/* Protected-mode virtual interrupts */
#define	CR4_TSD	0x00000004	/* Time stamp disable */
#define	CR4_DE	0x00000008	/* Debugging extensions */
#define	CR4_PSE	0x00000010	/* Page size extensions */
#define	CR4_PAE	0x00000020	/* Physical address extension */
#define	CR4_MCE	0x00000040	/* Machine check enable */
#define	CR4_PGE	0x00000080	/* Page global enable */
#define	CR4_PCE	0x00000100	/* Performance monitoring counter enable */
#define	CR4_FXSR 0x00000200	/* Fast FPU save/restore used by OS */
#define	CR4_XMM	0x00000400	/* enable SIMD/MMX2 to use except 16 */

/*
 * Bits in AMD64 special registers.  EFER is 64 bits wide.
 */
#define	EFER_NXE 0x000000800	/* PTE No-Execute bit enable (R/W) */

/*
 * CPUID instruction features register
 */
#define	CPUID_FPU	0x00000001
#define	CPUID_VME	0x00000002
#define	CPUID_DE	0x00000004
#define	CPUID_PSE	0x00000008
#define	CPUID_TSC	0x00000010
#define	CPUID_MSR	0x00000020
#define	CPUID_PAE	0x00000040
#define	CPUID_MCE	0x00000080
#define	CPUID_CX8	0x00000100
#define	CPUID_APIC	0x00000200
#define	CPUID_B10	0x00000400
#define	CPUID_SEP	0x00000800
#define	CPUID_MTRR	0x00001000
#define	CPUID_PGE	0x00002000
#define	CPUID_MCA	0x00004000
#define	CPUID_CMOV	0x00008000
#define	CPUID_PAT	0x00010000
#define	CPUID_PSE36	0x00020000
#define	CPUID_PSN	0x00040000
#define	CPUID_CLFSH	0x00080000
#define	CPUID_B20	0x00100000
#define	CPUID_DS	0x00200000
#define	CPUID_ACPI	0x00400000
#define	CPUID_MMX	0x00800000
#define	CPUID_FXSR	0x01000000
#define	CPUID_SSE	0x02000000
#define	CPUID_XMM	0x02000000
#define	CPUID_SSE2	0x04000000
#define	CPUID_SS	0x08000000
#define	CPUID_HTT	0x10000000
#define	CPUID_TM	0x20000000
#define	CPUID_IA64	0x40000000
#define	CPUID_PBE	0x80000000

#define	CPUID2_SSE3	0x00000001
#define	CPUID2_MON	0x00000008
#define	CPUID2_DS_CPL	0x00000010
#define	CPUID2_VMX	0x00000020
#define	CPUID2_SMX	0x00000040
#define	CPUID2_EST	0x00000080
#define	CPUID2_TM2	0x00000100
#define	CPUID2_SSSE3	0x00000200
#define	CPUID2_CNXTID	0x00000400
#define	CPUID2_CX16	0x00002000
#define	CPUID2_XTPR	0x00004000
#define	CPUID2_PDCM	0x00008000
#define	CPUID2_DCA	0x00040000

/*
 * Important bits in the AMD extended cpuid flags
 */
#define	AMDID_SYSCALL	0x00000800
#define	AMDID_MP	0x00080000
#define	AMDID_NX	0x00100000
#define	AMDID_EXT_MMX	0x00400000
#define	AMDID_FFXSR	0x01000000
#define	AMDID_RDTSCP	0x08000000
#define	AMDID_LM	0x20000000
#define	AMDID_EXT_3DNOW	0x40000000
#define	AMDID_3DNOW	0x80000000

#define	AMDID2_LAHF	0x00000001
#define	AMDID2_CMP	0x00000002
#define	AMDID2_SVM	0x00000004
#define	AMDID2_EXT_APIC	0x00000008
#define	AMDID2_CR8	0x00000010
#define	AMDID2_PREFETCH	0x00000100

/*
 * CPUID instruction 1 ebx info
 */
#define	CPUID_BRAND_INDEX	0x000000ff
#define	CPUID_CLFUSH_SIZE	0x0000ff00
#define	CPUID_HTT_CORES		0x00ff0000
#define	CPUID_LOCAL_APIC_ID	0xff000000

/*
 * AMD extended function 8000_0008h ecx info
 */
#define	AMDID_CMP_CORES		0x000000ff

/*
 * Model-specific registers for the i386 family
 */
#define	MSR_P5_MC_ADDR		0x000
#define	MSR_P5_MC_TYPE		0x001
#define	MSR_TSC			0x010
#define	MSR_P5_CESR		0x011
#define	MSR_P5_CTR0		0x012
#define	MSR_P5_CTR1		0x013
#define	MSR_IA32_PLATFORM_ID	0x017
#define	MSR_APICBASE		0x01b
#define	MSR_EBL_CR_POWERON	0x02a
#define	MSR_TEST_CTL		0x033
#define	MSR_BIOS_UPDT_TRIG	0x079
#define	MSR_BBL_CR_D0		0x088
#define	MSR_BBL_CR_D1		0x089
#define	MSR_BBL_CR_D2		0x08a
#define	MSR_BIOS_SIGN		0x08b
#define	MSR_PERFCTR0		0x0c1
#define	MSR_PERFCTR1		0x0c2
#define	MSR_IA32_EXT_CONFIG	0x0ee	/* Undocumented. Core Solo/Duo only */
#define	MSR_MTRRcap		0x0fe
#define	MSR_BBL_CR_ADDR		0x116
#define	MSR_BBL_CR_DECC		0x118
#define	MSR_BBL_CR_CTL		0x119
#define	MSR_BBL_CR_TRIG		0x11a
#define	MSR_BBL_CR_BUSY		0x11b
#define	MSR_BBL_CR_CTL3		0x11e
#define	MSR_SYSENTER_CS_MSR	0x174
#define	MSR_SYSENTER_ESP_MSR	0x175
#define	MSR_SYSENTER_EIP_MSR	0x176
#define	MSR_MCG_CAP		0x179
#define	MSR_MCG_STATUS		0x17a
#define	MSR_MCG_CTL		0x17b
#define	MSR_EVNTSEL0		0x186
#define	MSR_EVNTSEL1		0x187
#define	MSR_THERM_CONTROL	0x19a
#define	MSR_THERM_INTERRUPT	0x19b
#define	MSR_THERM_STATUS	0x19c
#define	MSR_IA32_MISC_ENABLE	0x1a0
#define	MSR_DEBUGCTLMSR		0x1d9
#define	MSR_LASTBRANCHFROMIP	0x1db
#define	MSR_LASTBRANCHTOIP	0x1dc
#define	MSR_LASTINTFROMIP	0x1dd
#define	MSR_LASTINTTOIP		0x1de
#define	MSR_ROB_CR_BKUPTMPDR6	0x1e0
#define	MSR_MTRRVarBase		0x200
#define	MSR_MTRR64kBase		0x250
#define	MSR_MTRR16kBase		0x258
#define	MSR_MTRR4kBase		0x268
#define	MSR_PAT			0x277
#define	MSR_MTRRdefType		0x2ff
#define	MSR_MC0_CTL		0x400
#define	MSR_MC0_STATUS		0x401
#define	MSR_MC0_ADDR		0x402
#define	MSR_MC0_MISC		0x403
#define	MSR_MC1_CTL		0x404
#define	MSR_MC1_STATUS		0x405
#define	MSR_MC1_ADDR		0x406
#define	MSR_MC1_MISC		0x407
#define	MSR_MC2_CTL		0x408
#define	MSR_MC2_STATUS		0x409
#define	MSR_MC2_ADDR		0x40a
#define	MSR_MC2_MISC		0x40b
#define	MSR_MC3_CTL		0x40c
#define	MSR_MC3_STATUS		0x40d
#define	MSR_MC3_ADDR		0x40e
#define	MSR_MC3_MISC		0x40f
#define	MSR_MC4_CTL		0x410
#define	MSR_MC4_STATUS		0x411
#define	MSR_MC4_ADDR		0x412
#define	MSR_MC4_MISC		0x413

/*
 * Constants related to MSR's.
 */
#define	APICBASE_RESERVED	0x000006ff
#define	APICBASE_BSP		0x00000100
#define	APICBASE_ENABLED	0x00000800
#define	APICBASE_ADDRESS	0xfffff000

/*
 * PAT modes.
 */
#define	PAT_UNCACHEABLE		0x00
#define	PAT_WRITE_COMBINING	0x01
#define	PAT_WRITE_THROUGH	0x04
#define	PAT_WRITE_PROTECTED	0x05
#define	PAT_WRITE_BACK		0x06
#define	PAT_UNCACHED		0x07
#define	PAT_VALUE(i, m)		((long long)(m) << (8 * (i)))
#define	PAT_MASK(i)		PAT_VALUE(i, 0xff)

/*
 * Constants related to MTRRs
 */
#define	MTRR_UNCACHEABLE	0x00
#define	MTRR_WRITE_COMBINING	0x01
#define	MTRR_WRITE_THROUGH	0x04
#define	MTRR_WRITE_PROTECTED	0x05
#define	MTRR_WRITE_BACK		0x06
#define	MTRR_N64K		8	/* numbers of fixed-size entries */
#define	MTRR_N16K		16
#define	MTRR_N4K		64
#define	MTRR_CAP_WC		0x0000000000000400ULL
#define	MTRR_CAP_FIXED		0x0000000000000100ULL
#define	MTRR_CAP_VCNT		0x00000000000000ffULL
#define	MTRR_DEF_ENABLE		0x0000000000000800ULL
#define	MTRR_DEF_FIXED_ENABLE	0x0000000000000400ULL
#define	MTRR_DEF_TYPE		0x00000000000000ffULL
#define	MTRR_PHYSBASE_PHYSBASE	0x000ffffffffff000ULL
#define	MTRR_PHYSBASE_TYPE	0x00000000000000ffULL
#define	MTRR_PHYSMASK_PHYSMASK	0x000ffffffffff000ULL
#define	MTRR_PHYSMASK_VALID	0x0000000000000800ULL

/*
 * Cyrix configuration registers, accessible as IO ports.
 */
#define	CCR0			0xc0	/* Configuration control register 0 */
#define	CCR0_NC0		0x01	/* First 64K of each 1M memory region is
								   non-cacheable */
#define	CCR0_NC1		0x02	/* 640K-1M region is non-cacheable */
#define	CCR0_A20M		0x04	/* Enables A20M# input pin */
#define	CCR0_KEN		0x08	/* Enables KEN# input pin */
#define	CCR0_FLUSH		0x10	/* Enables FLUSH# input pin */
#define	CCR0_BARB		0x20	/* Flushes internal cache when entering hold
								   state */
#define	CCR0_CO			0x40	/* Cache org: 1=direct mapped, 0=2x set
								   assoc */
#define	CCR0_SUSPEND	0x80	/* Enables SUSP# and SUSPA# pins */

#define	CCR1			0xc1	/* Configuration control register 1 */
#define	CCR1_RPL		0x01	/* Enables RPLSET and RPLVAL# pins */
#define	CCR1_SMI		0x02	/* Enables SMM pins */
#define	CCR1_SMAC		0x04	/* System management memory access */
#define	CCR1_MMAC		0x08	/* Main memory access */
#define	CCR1_NO_LOCK	0x10	/* Negate LOCK# */
#define	CCR1_SM3		0x80	/* SMM address space address region 3 */

#define	CCR2			0xc2
#define	CCR2_WB			0x02	/* Enables WB cache interface pins */
#define	CCR2_SADS		0x02	/* Slow ADS */
#define	CCR2_LOCK_NW	0x04	/* LOCK NW Bit */
#define	CCR2_SUSP_HLT	0x08	/* Suspend on HALT */
#define	CCR2_WT1		0x10	/* WT region 1 */
#define	CCR2_WPR1		0x10	/* Write-protect region 1 */
#define	CCR2_BARB		0x20	/* Flushes write-back cache when entering
								   hold state. */
#define	CCR2_BWRT		0x40	/* Enables burst write cycles */
#define	CCR2_USE_SUSP	0x80	/* Enables suspend pins */

#define	CCR3			0xc3
#define	CCR3_SMILOCK	0x01	/* SMM register lock */
#define	CCR3_NMI		0x02	/* Enables NMI during SMM */
#define	CCR3_LINBRST	0x04	/* Linear address burst cycles */
#define	CCR3_SMMMODE	0x08	/* SMM Mode */
#define	CCR3_MAPEN0		0x10	/* Enables Map0 */
#define	CCR3_MAPEN1		0x20	/* Enables Map1 */
#define	CCR3_MAPEN2		0x40	/* Enables Map2 */
#define	CCR3_MAPEN3		0x80	/* Enables Map3 */

#define	CCR4			0xe8
#define	CCR4_IOMASK		0x07
#define	CCR4_MEM		0x08	/* Enables momory bypassing */
#define	CCR4_DTE		0x10	/* Enables directory table entry cache */
#define	CCR4_FASTFPE	0x20	/* Fast FPU exception */
#define	CCR4_CPUID		0x80	/* Enables CPUID instruction */

#define	CCR5			0xe9
#define	CCR5_WT_ALLOC	0x01	/* Write-through allocate */
#define	CCR5_SLOP		0x02	/* LOOP instruction slowed down */
#define	CCR5_LBR1		0x10	/* Local bus region 1 */
#define	CCR5_ARREN		0x20	/* Enables ARR region */

#define	CCR6			0xea

#define	CCR7			0xeb

/* Performance Control Register (5x86 only). */
#define	PCR0			0x20
#define	PCR0_RSTK		0x01	/* Enables return stack */
#define	PCR0_BTB		0x02	/* Enables branch target buffer */
#define	PCR0_LOOP		0x04	/* Enables loop */
#define	PCR0_AIS		0x08	/* Enables all instrcutions stalled to
								   serialize pipe. */
#define	PCR0_MLR		0x10	/* Enables reordering of misaligned loads */
#define	PCR0_BTBRT		0x40	/* Enables BTB test register. */
#define	PCR0_LSSER		0x80	/* Disable reorder */

/* Device Identification Registers */
#define	DIR0			0xfe
#define	DIR1			0xff

/*
 * The following four 3-byte registers control the non-cacheable regions.
 * These registers must be written as three separate bytes.
 *
 * NCRx+0: A31-A24 of starting address
 * NCRx+1: A23-A16 of starting address
 * NCRx+2: A15-A12 of starting address | NCR_SIZE_xx.
 *
 * The non-cacheable region's starting address must be aligned to the
 * size indicated by the NCR_SIZE_xx field.
 */
#define	NCR1	0xc4
#define	NCR2	0xc7
#define	NCR3	0xca
#define	NCR4	0xcd

#define	NCR_SIZE_0K	0
#define	NCR_SIZE_4K	1
#define	NCR_SIZE_8K	2
#define	NCR_SIZE_16K	3
#define	NCR_SIZE_32K	4
#define	NCR_SIZE_64K	5
#define	NCR_SIZE_128K	6
#define	NCR_SIZE_256K	7
#define	NCR_SIZE_512K	8
#define	NCR_SIZE_1M	9
#define	NCR_SIZE_2M	10
#define	NCR_SIZE_4M	11
#define	NCR_SIZE_8M	12
#define	NCR_SIZE_16M	13
#define	NCR_SIZE_32M	14
#define	NCR_SIZE_4G	15

/*
 * The address region registers are used to specify the location and
 * size for the eight address regions.
 *
 * ARRx + 0: A31-A24 of start address
 * ARRx + 1: A23-A16 of start address
 * ARRx + 2: A15-A12 of start address | ARR_SIZE_xx
 */
#define	ARR0	0xc4
#define	ARR1	0xc7
#define	ARR2	0xca
#define	ARR3	0xcd
#define	ARR4	0xd0
#define	ARR5	0xd3
#define	ARR6	0xd6
#define	ARR7	0xd9

#define	ARR_SIZE_0K		0
#define	ARR_SIZE_4K		1
#define	ARR_SIZE_8K		2
#define	ARR_SIZE_16K	3
#define	ARR_SIZE_32K	4
#define	ARR_SIZE_64K	5
#define	ARR_SIZE_128K	6
#define	ARR_SIZE_256K	7
#define	ARR_SIZE_512K	8
#define	ARR_SIZE_1M		9
#define	ARR_SIZE_2M		10
#define	ARR_SIZE_4M		11
#define	ARR_SIZE_8M		12
#define	ARR_SIZE_16M	13
#define	ARR_SIZE_32M	14
#define	ARR_SIZE_4G		15

/*
 * The region control registers specify the attributes associated with
 * the ARRx addres regions.
 */
#define	RCR0	0xdc
#define	RCR1	0xdd
#define	RCR2	0xde
#define	RCR3	0xdf
#define	RCR4	0xe0
#define	RCR5	0xe1
#define	RCR6	0xe2
#define	RCR7	0xe3

#define	RCR_RCD	0x01	/* Disables caching for ARRx (x = 0-6). */
#define	RCR_RCE	0x01	/* Enables caching for ARR7. */
#define	RCR_WWO	0x02	/* Weak write ordering. */
#define	RCR_WL	0x04	/* Weak locking. */
#define	RCR_WG	0x08	/* Write gathering. */
#define	RCR_WT	0x10	/* Write-through. */
#define	RCR_NLB	0x20	/* LBA# pin is not asserted. */

/* AMD Write Allocate Top-Of-Memory and Control Register */
#define	AMD_WT_ALLOC_TME	0x40000	/* top-of-memory enable */
#define	AMD_WT_ALLOC_PRE	0x20000	/* programmable range enable */
#define	AMD_WT_ALLOC_FRE	0x10000	/* fixed (A0000-FFFFF) range enable */

/* AMD64 MSR's */
#define	MSR_EFER	0xc0000080	/* extended features */

/* VIA ACE crypto featureset: for via_feature_rng */
#define	VIA_HAS_RNG		1	/* cpu has RNG */

/* VIA ACE crypto featureset: for via_feature_xcrypt */
#define	VIA_HAS_AES		1	/* cpu has AES */
#define	VIA_HAS_SHA		2	/* cpu has SHA1 & SHA256 */
#define	VIA_HAS_MM		4	/* cpu has RSA instructions */
#define	VIA_HAS_AESCTR		8	/* cpu has AES-CTR instructions */

/* Centaur Extended Feature flags */
#define	VIA_CPUID_HAS_RNG	0x000004
#define	VIA_CPUID_DO_RNG	0x000008
#define	VIA_CPUID_HAS_ACE	0x000040
#define	VIA_CPUID_DO_ACE	0x000080
#define	VIA_CPUID_HAS_ACE2	0x000100
#define	VIA_CPUID_DO_ACE2	0x000200
#define	VIA_CPUID_HAS_PHE	0x000400
#define	VIA_CPUID_DO_PHE	0x000800
#define	VIA_CPUID_HAS_PMM	0x001000
#define	VIA_CPUID_DO_PMM	0x002000

/* VIA ACE xcrypt-* instruction context control options */
#define	VIA_CRYPT_CWLO_ROUND_M		0x0000000f
#define	VIA_CRYPT_CWLO_ALG_M		0x00000070
#define	VIA_CRYPT_CWLO_ALG_AES		0x00000000
#define	VIA_CRYPT_CWLO_KEYGEN_M		0x00000080
#define	VIA_CRYPT_CWLO_KEYGEN_HW	0x00000000
#define	VIA_CRYPT_CWLO_KEYGEN_SW	0x00000080
#define	VIA_CRYPT_CWLO_NORMAL		0x00000000
#define	VIA_CRYPT_CWLO_INTERMEDIATE	0x00000100
#define	VIA_CRYPT_CWLO_ENCRYPT		0x00000000
#define	VIA_CRYPT_CWLO_DECRYPT		0x00000200
#define	VIA_CRYPT_CWLO_KEY128		0x0000000a	/* 128bit, 10 rds */
#define	VIA_CRYPT_CWLO_KEY192		0x0000040c	/* 192bit, 12 rds */
#define	VIA_CRYPT_CWLO_KEY256		0x0000080e	/* 256bit, 15 rds */

#ifndef LOCORE
static __inline u_char
read_cyrix_reg(u_char reg)
{
	outb(0x22, reg);
	return inb(0x23);
}

static __inline void
write_cyrix_reg(u_char reg, u_char data)
{
	outb(0x22, reg);
	outb(0x23, data);
}
#endif

#endif /* !_MACHINE_SPECIALREG_H_ */
