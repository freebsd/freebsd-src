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
 *	$Id: specialreg.h,v 1.7 1995/05/30 08:00:54 rgrimes Exp $
 */

#ifndef _MACHINE_SPECIALREG_H_
#define	_MACHINE_SPECIALREG_H_

/*
 * Bits in 386 special registers:
 */
#define	CR0_PE	0x00000001	/* Protected mode Enable */
#define	CR0_MP	0x00000002	/* "Math" Present (NPX or NPX emulator) */
#ifdef notused
#define	CR0_EM	0x00000004	/* EMulate non-NPX coproc. (trap ESC only) */
#endif
#define	CR0_TS	0x00000008	/* Task Switched (if MP, trap ESC and WAIT) */
#ifdef notused
#define	CR0_ET	0x00000010	/* Extension Type (387 (if set) vs 287) */
#endif
#define	CR0_PG	0x80000000	/* PaGing enable */

/*
 * Bits in 486 special registers:
 */
#define CR0_NE	0x00000020	/* Numeric Error enable (EX16 vs IRQ13) */
#define CR0_WP	0x00010000	/* Write Protect (honor page protect in all modes) */
#define CR0_AM	0x00040000	/* Alignment Mask (set to enable AC flag) */

/*
 * Cyrix 486 DLC special registers, accessible as IO ports.
 */
#define CCR0	0xc0		/* configuration control register 0 */
#define CCR0_NC0	0x01	/* first 64K of each 1M memory region is
				   non-cacheable */
#define CCR0_NC1	0x02	/* 640K-1M region is non-cacheable */
#define CCR0_A20M	0x04	/* enables A20M# input pin */
#define CCR0_KEN	0x08	/* enables KEN# input pin */
#define CCR0_FLUSH	0x10	/* enables FLUSH# input pin */
#define CCR0_BARB	0x20	/* flushes internal cache when entering hold
				   state */
#define CCR0_CO		0x40	/* cache org: 1=direct mapped, 0=2x set assoc */
#define CCR0_SUSPEND	0x80	/* enables SUSP# and SUSPA# pins */

#define CCR1	0xc1		/* configuration control register 1 */
#define CCR1_RPL	0x01	/* enables RPLSET and RPLVAL# pins */
/* the remaining 7 bits of this register are reserved */

/*
 * the following four 3-byte registers control the non-cacheable regions.
 * These registers must be written as three separate bytes.
 *
 * NCRx+0: A31-A24 of starting address
 * NCRx+1: A23-A16 of starting address
 * NCRx+2: A15-A12 of starting address | NCR_SIZE_xx.
 *
 * The non-cacheable region's starting address must be aligned to the
 * size indicated by the NCR_SIZE_xx field.
 */
#define NCR1	0xc4
#define NCR2	0xc7
#define NCR3	0xca
#define NCR4	0xcd

#define NCR_SIZE_0K	0
#define NCR_SIZE_4K	1
#define NCR_SIZE_8K	2
#define NCR_SIZE_16K	3
#define NCR_SIZE_32K	4
#define NCR_SIZE_64K	5
#define NCR_SIZE_128K	6
#define NCR_SIZE_256K	7
#define NCR_SIZE_512K	8
#define NCR_SIZE_1M	9
#define NCR_SIZE_2M	10
#define NCR_SIZE_4M	11
#define NCR_SIZE_8M	12
#define NCR_SIZE_16M	13
#define NCR_SIZE_32M	14
#define NCR_SIZE_4G	15

#endif /* !_MACHINE_SPECIALREG_H_ */
