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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	$Id: specialreg.h,v 1.14 1997/07/21 17:53:51 fsmp Exp $
 */

#ifndef _MACHINE_SPECIALREG_H_
#define	_MACHINE_SPECIALREG_H_

/*
 * Bits in 386 special registers:
 */
#define	CR0_PE	0x00000001	/* Protected mode Enable */
#define	CR0_MP	0x00000002	/* "Math" Present (NPX or NPX emulator) */
#define	CR0_EM	0x00000004	/* EMulate non-NPX coproc. (trap ESC only) */
#define	CR0_TS	0x00000008	/* Task Switched (if MP, trap ESC and WAIT) */
#ifdef notused
#define	CR0_ET	0x00000010	/* Extension Type (387 (if set) vs 287) */
#endif
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

/*
 * CPUID instruction features register
 */
#define	CPUID_FPU	0x0001
#define	CPUID_VME	0x0002
#define	CPUID_DE	0x0004
#define	CPUID_PSE	0x0008
#define	CPUID_TSC	0x0010
#define	CPUID_MSR	0x0020
#define	CPUID_PAE	0x0040
#define	CPUID_MCE	0x0080
#define	CPUID_CX8	0x0100
#define	CPUID_APIC	0x0200
#define	CPUID_B10	0x0400
#define	CPUID_B11	0x0800
#define	CPUID_MTRR	0x1000
#define	CPUID_PGE	0x2000
#define	CPUID_MCA	0x4000
#define	CPUID_CMOV	0x8000

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
#define CCR2_BARB		0x20	/* Flushes write-back cache when entering
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

#define RCR_RCD	0x01	/* Disables caching for ARRx (x = 0-6). */
#define RCR_RCE	0x01	/* Enables caching for ARR7. */
#define RCR_WWO	0x02	/* Weak write ordering. */
#define	RCR_WL	0x04	/* Weak locking. */
#define RCR_WG	0x08	/* Write gathering. */
#define	RCR_WT	0x10	/* Write-through. */
#define	RCR_NLB	0x20	/* LBA# pin is not asserted. */


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
