/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)isa.h	5.7 (Berkeley) 5/9/91
 * $FreeBSD$
 */

#ifdef PC98
#error isa.h is included from PC-9801 source
#endif

#ifndef _I386_ISA_ISA_H_
#define	_I386_ISA_ISA_H_

/* BEWARE:  Included in both assembler and C code */

/*
 * ISA Bus conventions
 */

/*
 * Input / Output Port Assignments
 */
#ifndef IO_ISABEGIN
#define	IO_ISABEGIN	0x000		/* 0x000 - Beginning of I/O Registers */

		/* CPU Board */
#define	IO_DMA1		0x000		/* 8237A DMA Controller #1 */
#define	IO_ICU1		0x020		/* 8259A Interrupt Controller #1 */
#define	IO_PMP1		0x026		/* 82347 Power Management Peripheral */
#define	IO_TIMER1	0x040		/* 8253 Timer #1 */
#define	IO_TIMER2	0x048		/* 8253 Timer #2 */
#define	IO_KBD		0x060		/* 8042 Keyboard */
#define	IO_PPI		0x061		/* Programmable Peripheral Interface */
#define	IO_RTC		0x070		/* RTC */
#define	IO_NMI		IO_RTC		/* NMI Control */
#define	IO_DMAPG	0x080		/* DMA Page Registers */
#define	IO_ICU2		0x0A0		/* 8259A Interrupt Controller #2 */
#define	IO_DMA2		0x0C0		/* 8237A DMA Controller #2 */
#define	IO_NPX		0x0F0		/* Numeric Coprocessor */

		/* Cards */
					/* 0x100 - 0x16F Open */

#define	IO_WD2		0x170		/* Secondary Fixed Disk Controller */

#define	IO_PMP2		0x178		/* 82347 Power Management Peripheral */

					/* 0x17A - 0x1EF Open */

#define	IO_WD1		0x1F0		/* Primary Fixed Disk Controller */
#define	IO_GAME		0x201		/* Game Controller */

					/* 0x202 - 0x22A Open */

#define	IO_ASC2		0x22B		/* AmiScan addr.grp. 2 */

					/* 0x230 - 0x26A Open */

#define	IO_ASC3		0x26B		/* AmiScan addr.grp. 3 */
#define	IO_GSC1		0x270 /* -- 0x27B! GeniScan GS-4500 addr.grp. 1 */
#define	IO_LPT2		0x278		/* Parallel Port #2 */

					/* 0x280 - 0x2AA Open */

#define	IO_ASC4		0x2AB		/* AmiScan addr.grp. 4 */

					/* 0x2B0 - 0x2DF Open */

#define	IO_GSC2		0x2E0		/* GeniScan GS-4500 addr.grp. 2 */
#define	IO_COM4		0x2E8		/* COM4 i/o address */
#define	IO_ASC5		0x2EB		/* AmiScan addr.grp. 5 */

					/* 0x2F0 - 0x2F7 Open */

#define	IO_COM2		0x2F8		/* COM2 i/o address */

					/* 0x300 - 0x32A Open */

#define	IO_ASC6		0x32B		/* AmiScan addr.grp. 6 */
#define	IO_AHA0		0x330		/* adaptec 1542 default addr. */
#define	IO_BT0		0x330		/* bustek 742a default addr. */
#define	IO_UHA0		0x330		/* ultrastore 14f default addr. */
#define	IO_AHA1		0x334		/* adaptec 1542 default addr. */
#define	IO_BT1		0x334		/* bustek 742a default addr. */

					/* 0x340 - 0x36A Open */

#define	IO_ASC7		0x36B		/* AmiScan addr.grp. 7 */
#define	IO_GSC3		0x370		/* GeniScan GS-4500 addr.grp. 3 */
#define	IO_FD2		0x370		/* secondary base i/o address */
#define	IO_LPT1		0x378		/* Parallel Port #1 */

					/* 0x380 - 0x3AA Open */

#define	IO_ASC8		0x3AB		/* AmiScan addr.grp. 8 */
#define	IO_MDA		0x3B0		/* Monochome Adapter */
#define	IO_LPT3		0x3BC		/* Monochome Adapter Printer Port */
#define	IO_VGA		0x3C0		/* E/VGA Ports */
#define	IO_CGA		0x3D0		/* CGA Ports */
#define	IO_GSC4		0x3E0		/* GeniScan GS-4500 addr.grp. 4 */
#define	IO_COM3		0x3E8		/* COM3 i/o address */
#define	IO_ASC1		0x3EB		/* AmiScan addr.grp. 1 */
#define	IO_FD1		0x3F0		/* primary base i/o address */
#define	IO_COM1		0x3F8		/* COM1 i/o address */

#define	IO_ISAEND	0x3FF		/* End (actually Max) of I/O Regs */
#endif /* !IO_ISABEGIN */

/*
 * Input / Output Port Sizes - these are from several sources, and tend
 * to be the larger of what was found.
 */
#ifndef	IO_ISASIZES
#define	IO_ISASIZES

#define	IO_ASCSIZE	5		/* AmiScan GI1904-based hand scanner */
#define	IO_CGASIZE	12		/* CGA controllers */
#define	IO_COMSIZE	8		/* 8250, 16x50 com controllers */
#define	IO_DMASIZE	16		/* 8237 DMA controllers */
#define	IO_DPGSIZE	32		/* 74LS612 DMA page registers */
#define	IO_EISASIZE	256		/* EISA controllers */
#define	IO_FDCSIZE	8		/* Nec765 floppy controllers */
#define	IO_GAMSIZE	16		/* AT compatible game controllers */
#define	IO_GSCSIZE	8		/* GeniScan GS-4500G hand scanner */
#define	IO_ICUSIZE	16		/* 8259A interrupt controllers */
#define	IO_KBDSIZE	16		/* 8042 Keyboard controllers */
#define	IO_LPTSIZE	8		/* LPT controllers, some use only 4 */
#define	IO_MDASIZE	12		/* Monochrome display controllers */
#define	IO_NPXSIZE	16		/* 80387/80487 NPX registers */
#define	IO_PMPSIZE	2		/* 82347 power management peripheral */
#define	IO_PSMSIZE	5		/* 8042 Keyboard controllers */
#define	IO_RTCSIZE	16		/* CMOS real time clock, NMI control */
#define	IO_TMRSIZE	16		/* 8253 programmable timers */
#define	IO_VGASIZE	16		/* VGA controllers */
#define	IO_WDCSIZE	8		/* WD compatible disk controllers */

#endif /* !IO_ISASIZES */

/*
 * Input / Output Memory Physical Addresses
 */
#ifndef	IOM_BEGIN
#define	IOM_BEGIN	0x0A0000	/* Start of I/O Memory "hole" */
#define	IOM_END		0x100000	/* End of I/O Memory "hole" */
#define	IOM_SIZE	(IOM_END - IOM_BEGIN)
#endif /* !IOM_BEGIN */

/*
 * RAM Physical Address Space (ignoring the above mentioned "hole")
 */
#ifndef	RAM_BEGIN
#define	RAM_BEGIN	0x0000000	/* Start of RAM Memory */
#define	RAM_END		0x1000000	/* End of RAM Memory */
#define	RAM_SIZE	(RAM_END - RAM_BEGIN)
#endif /* !RAM_BEGIN */

#endif /* !_I386_ISA_ISA_H_ */
