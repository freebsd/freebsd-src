/*	$NetBSD: armreg.h,v 1.28 2003/10/31 16:30:15 scw Exp $	*/

/*-
 * Copyright (c) 1998, 2001 Ben Harris
 * Copyright (c) 1994-1996 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef MACHINE_ARMREG_H
#define MACHINE_ARMREG_H
#define INSN_SIZE	4
#define INSN_COND_MASK	0xf0000000	/* Condition mask */
#define PSR_MODE        0x0000001f      /* mode mask */
#define PSR_USR26_MODE  0x00000000
#define PSR_FIQ26_MODE  0x00000001
#define PSR_IRQ26_MODE  0x00000002
#define PSR_SVC26_MODE  0x00000003
#define PSR_USR32_MODE  0x00000010
#define PSR_FIQ32_MODE  0x00000011
#define PSR_IRQ32_MODE  0x00000012
#define PSR_SVC32_MODE  0x00000013
#define PSR_ABT32_MODE  0x00000017
#define PSR_UND32_MODE  0x0000001b
#define PSR_SYS32_MODE  0x0000001f
#define PSR_32_MODE     0x00000010
#define PSR_FLAGS	0xf0000000    /* flags */

#define PSR_C_bit (1 << 29)       /* carry */

/* The high-order byte is always the implementor */
#define CPU_ID_IMPLEMENTOR_MASK	0xff000000
#define CPU_ID_ARM_LTD		0x41000000 /* 'A' */
#define CPU_ID_DEC		0x44000000 /* 'D' */
#define CPU_ID_INTEL		0x69000000 /* 'i' */
#define	CPU_ID_TI		0x54000000 /* 'T' */

/* How to decide what format the CPUID is in. */
#define CPU_ID_ISOLD(x)		(((x) & 0x0000f000) == 0x00000000)
#define CPU_ID_IS7(x)		(((x) & 0x0000f000) == 0x00007000)
#define CPU_ID_ISNEW(x)		(!CPU_ID_ISOLD(x) && !CPU_ID_IS7(x))

/* On ARM3 and ARM6, this byte holds the foundry ID. */
#define CPU_ID_FOUNDRY_MASK	0x00ff0000
#define CPU_ID_FOUNDRY_VLSI	0x00560000

/* On ARM7 it holds the architecture and variant (sub-model) */
#define CPU_ID_7ARCH_MASK	0x00800000
#define CPU_ID_7ARCH_V3		0x00000000
#define CPU_ID_7ARCH_V4T	0x00800000
#define CPU_ID_7VARIANT_MASK	0x007f0000

/* On more recent ARMs, it does the same, but in a different format */
#define CPU_ID_ARCH_MASK	0x000f0000
#define CPU_ID_ARCH_V3		0x00000000
#define CPU_ID_ARCH_V4		0x00010000
#define CPU_ID_ARCH_V4T		0x00020000
#define CPU_ID_ARCH_V5		0x00030000
#define CPU_ID_ARCH_V5T		0x00040000
#define CPU_ID_ARCH_V5TE	0x00050000
#define CPU_ID_VARIANT_MASK	0x00f00000

/* Next three nybbles are part number */
#define CPU_ID_PARTNO_MASK	0x0000fff0

/* Intel XScale has sub fields in part number */
#define CPU_ID_XSCALE_COREGEN_MASK	0x0000e000 /* core generation */
#define CPU_ID_XSCALE_COREREV_MASK	0x00001c00 /* core revision */
#define CPU_ID_XSCALE_PRODUCT_MASK	0x000003f0 /* product number */

/* And finally, the revision number. */
#define CPU_ID_REVISION_MASK	0x0000000f

/* Individual CPUs are probably best IDed by everything but the revision. */
#define CPU_ID_CPU_MASK		0xfffffff0

/* Fake CPU IDs for ARMs without CP15 */
#define CPU_ID_ARM2		0x41560200
#define CPU_ID_ARM250		0x41560250

/* Pre-ARM7 CPUs -- [15:12] == 0 */
#define CPU_ID_ARM3		0x41560300
#define CPU_ID_ARM600		0x41560600
#define CPU_ID_ARM610		0x41560610
#define CPU_ID_ARM620		0x41560620

/* ARM7 CPUs -- [15:12] == 7 */
#define CPU_ID_ARM700		0x41007000 /* XXX This is a guess. */
#define CPU_ID_ARM710		0x41007100
#define CPU_ID_ARM7500		0x41027100 /* XXX This is a guess. */
#define CPU_ID_ARM710A		0x41047100 /* inc ARM7100 */
#define CPU_ID_ARM7500FE	0x41077100
#define CPU_ID_ARM710T		0x41807100
#define CPU_ID_ARM720T		0x41807200
#define CPU_ID_ARM740T8K	0x41807400 /* XXX no MMU, 8KB cache */
#define CPU_ID_ARM740T4K	0x41817400 /* XXX no MMU, 4KB cache */

/* Post-ARM7 CPUs */
#define CPU_ID_ARM810		0x41018100
#define CPU_ID_ARM920T		0x41129200
#define CPU_ID_ARM922T		0x41029220
#define CPU_ID_ARM940T		0x41029400 /* XXX no MMU */
#define CPU_ID_ARM946ES		0x41049460 /* XXX no MMU */
#define	CPU_ID_ARM966ES		0x41049660 /* XXX no MMU */
#define	CPU_ID_ARM966ESR1	0x41059660 /* XXX no MMU */
#define CPU_ID_ARM1020E		0x4115a200 /* (AKA arm10 rev 1) */
#define CPU_ID_ARM1022ES	0x4105a220
#define CPU_ID_SA110		0x4401a100
#define CPU_ID_SA1100		0x4401a110
#define	CPU_ID_TI925T		0x54029250
#define CPU_ID_SA1110		0x6901b110
#define CPU_ID_IXP1200		0x6901c120
#define CPU_ID_80200		0x69052000
#define CPU_ID_PXA250    	0x69052100 /* sans core revision */
#define CPU_ID_PXA210    	0x69052120
#define CPU_ID_PXA250A		0x69052100 /* 1st version Core */
#define CPU_ID_PXA210A		0x69052120 /* 1st version Core */
#define CPU_ID_PXA250B		0x69052900 /* 3rd version Core */
#define CPU_ID_PXA210B		0x69052920 /* 3rd version Core */
#define CPU_ID_PXA250C		0x69052d00 /* 4th version Core */
#define CPU_ID_PXA210C		0x69052d20 /* 4th version Core */
#define	CPU_ID_80321_400	0x69052420
#define	CPU_ID_80321_600	0x69052430
#define	CPU_ID_80321_400_B0	0x69052c20
#define	CPU_ID_80321_600_B0	0x69052c30
#define	CPU_ID_IXP425_533	0x690541c0
#define	CPU_ID_IXP425_400	0x690541d0
#define	CPU_ID_IXP425_266	0x690541f0

/* ARM3-specific coprocessor 15 registers */
#define ARM3_CP15_FLUSH		1
#define ARM3_CP15_CONTROL	2
#define ARM3_CP15_CACHEABLE	3
#define ARM3_CP15_UPDATEABLE	4
#define ARM3_CP15_DISRUPTIVE	5	

/* ARM3 Control register bits */
#define ARM3_CTL_CACHE_ON	0x00000001
#define ARM3_CTL_SHARED		0x00000002
#define ARM3_CTL_MONITOR	0x00000004

/*
 * Post-ARM3 CP15 registers:
 *
 *	1	Control register
 *
 *	2	Translation Table Base
 *
 *	3	Domain Access Control
 *
 *	4	Reserved
 *
 *	5	Fault Status
 *
 *	6	Fault Address
 *
 *	7	Cache/write-buffer Control
 *
 *	8	TLB Control
 *
 *	9	Cache Lockdown
 *
 *	10	TLB Lockdown
 *
 *	11	Reserved
 *
 *	12	Reserved
 *
 *	13	Process ID (for FCSE)
 *
 *	14	Reserved
 *
 *	15	Implementation Dependent
 */

/* Some of the definitions below need cleaning up for V3/V4 architectures */

/* CPU control register (CP15 register 1) */
#define CPU_CONTROL_MMU_ENABLE	0x00000001 /* M: MMU/Protection unit enable */
#define CPU_CONTROL_AFLT_ENABLE	0x00000002 /* A: Alignment fault enable */
#define CPU_CONTROL_DC_ENABLE	0x00000004 /* C: IDC/DC enable */
#define CPU_CONTROL_WBUF_ENABLE 0x00000008 /* W: Write buffer enable */
#define CPU_CONTROL_32BP_ENABLE 0x00000010 /* P: 32-bit exception handlers */
#define CPU_CONTROL_32BD_ENABLE 0x00000020 /* D: 32-bit addressing */
#define CPU_CONTROL_LABT_ENABLE 0x00000040 /* L: Late abort enable */
#define CPU_CONTROL_BEND_ENABLE 0x00000080 /* B: Big-endian mode */
#define CPU_CONTROL_SYST_ENABLE 0x00000100 /* S: System protection bit */
#define CPU_CONTROL_ROM_ENABLE	0x00000200 /* R: ROM protection bit */
#define CPU_CONTROL_CPCLK	0x00000400 /* F: Implementation defined */
#define CPU_CONTROL_BPRD_ENABLE 0x00000800 /* Z: Branch prediction enable */
#define CPU_CONTROL_IC_ENABLE   0x00001000 /* I: IC enable */
#define CPU_CONTROL_VECRELOC	0x00002000 /* V: Vector relocation */
#define CPU_CONTROL_ROUNDROBIN	0x00004000 /* RR: Predictable replacement */
#define CPU_CONTROL_V4COMPAT	0x00008000 /* L4: ARMv4 compat LDR R15 etc */

#define CPU_CONTROL_IDC_ENABLE	CPU_CONTROL_DC_ENABLE

/* XScale Auxillary Control Register (CP15 register 1, opcode2 1) */
#define	XSCALE_AUXCTL_K		0x00000001 /* dis. write buffer coalescing */
#define	XSCALE_AUXCTL_P		0x00000002 /* ECC protect page table access */
#define	XSCALE_AUXCTL_MD_WB_RA	0x00000000 /* mini-D$ wb, read-allocate */
#define	XSCALE_AUXCTL_MD_WB_RWA	0x00000010 /* mini-D$ wb, read/write-allocate */
#define	XSCALE_AUXCTL_MD_WT	0x00000020 /* mini-D$ wt, read-allocate */
#define	XSCALE_AUXCTL_MD_MASK	0x00000030

/* Cache type register definitions */
#define	CPU_CT_ISIZE(x)		((x) & 0xfff)		/* I$ info */
#define	CPU_CT_DSIZE(x)		(((x) >> 12) & 0xfff)	/* D$ info */
#define	CPU_CT_S		(1U << 24)		/* split cache */
#define	CPU_CT_CTYPE(x)		(((x) >> 25) & 0xf)	/* cache type */

#define	CPU_CT_CTYPE_WT		0	/* write-through */
#define	CPU_CT_CTYPE_WB1	1	/* write-back, clean w/ read */
#define	CPU_CT_CTYPE_WB2	2	/* w/b, clean w/ cp15,7 */
#define	CPU_CT_CTYPE_WB6	6	/* w/b, cp15,7, lockdown fmt A */
#define	CPU_CT_CTYPE_WB7	7	/* w/b, cp15,7, lockdown fmt B */

#define	CPU_CT_xSIZE_LEN(x)	((x) & 0x3)		/* line size */
#define	CPU_CT_xSIZE_M		(1U << 2)		/* multiplier */
#define	CPU_CT_xSIZE_ASSOC(x)	(((x) >> 3) & 0x7)	/* associativity */
#define	CPU_CT_xSIZE_SIZE(x)	(((x) >> 6) & 0x7)	/* size */

/* Fault status register definitions */

#define FAULT_TYPE_MASK 0x0f
#define FAULT_USER      0x10

#define FAULT_WRTBUF_0  0x00 /* Vector Exception */
#define FAULT_WRTBUF_1  0x02 /* Terminal Exception */
#define FAULT_BUSERR_0  0x04 /* External Abort on Linefetch -- Section */
#define FAULT_BUSERR_1  0x06 /* External Abort on Linefetch -- Page */
#define FAULT_BUSERR_2  0x08 /* External Abort on Non-linefetch -- Section */
#define FAULT_BUSERR_3  0x0a /* External Abort on Non-linefetch -- Page */
#define FAULT_BUSTRNL1  0x0c /* External abort on Translation -- Level 1 */
#define FAULT_BUSTRNL2  0x0e /* External abort on Translation -- Level 2 */
#define FAULT_ALIGN_0   0x01 /* Alignment */
#define FAULT_ALIGN_1   0x03 /* Alignment */
#define FAULT_TRANS_S   0x05 /* Translation -- Section */
#define FAULT_TRANS_P   0x07 /* Translation -- Page */
#define FAULT_DOMAIN_S  0x09 /* Domain -- Section */
#define FAULT_DOMAIN_P  0x0b /* Domain -- Page */
#define FAULT_PERM_S    0x0d /* Permission -- Section */
#define FAULT_PERM_P    0x0f /* Permission -- Page */

#define	FAULT_IMPRECISE	0x400	/* Imprecise exception (XSCALE) */

/*
 * Address of the vector page, low and high versions.
 */
#define	ARM_VECTORS_LOW		0x00000000U
#define	ARM_VECTORS_HIGH	0xffff0000U

/*
 * ARM Instructions
 *
 *       3 3 2 2 2                              
 *       1 0 9 8 7                                                     0
 *      +-------+-------------------------------------------------------+
 *      | cond  |              instruction dependant                    |
 *      |c c c c|                                                       |
 *      +-------+-------------------------------------------------------+
 */

#define INSN_SIZE		4		/* Always 4 bytes */
#define INSN_COND_MASK		0xf0000000	/* Condition mask */
#define INSN_COND_AL		0xe0000000	/* Always condition */

#endif /* !MACHINE_ARMREG_H */
