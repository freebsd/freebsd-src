/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: pte.h,v 1.2 1998/08/31 14:43:40 tsubai Exp $
 * $FreeBSD$
 */

#ifndef	_MACHINE_PTE_H_
#define	_MACHINE_PTE_H_

/*
 * Page Table Entries
 */
#ifndef	LOCORE
#include <sys/queue.h>

struct pte {
	u_int pte_hi;
	u_int pte_lo;
};
#endif	/* LOCORE */
/* High word: */
#define	PTE_VALID	0x80000000
#define	PTE_VSID_SHFT	7
#define	PTE_HID		0x00000040
#define	PTE_API		0x0000003f
/* Low word: */
#define	PTE_RPGN	0xfffff000
#define	PTE_REF		0x00000100
#define	PTE_CHG		0x00000080
#define	PTE_WIMG	0x00000078
#define	PTE_W		0x00000040
#define	PTE_I		0x00000020
#define	PTE_M		0x00000010
#define	PTE_G		0x00000008
#define	PTE_PP		0x00000003
#define	PTE_RO		0x00000003
#define	PTE_RW		0x00000002

#ifndef	LOCORE
typedef	struct pte pte_t;
#endif	/* LOCORE */

/*
 * Extract bits from address
 */
#define	ADDR_SR_SHFT	28
#define	ADDR_PIDX	0x0ffff000
#define	ADDR_PIDX_SHFT	12
#define	ADDR_API_SHFT	22
#define	ADDR_POFF	0x00000fff

#ifndef	LOCORE
#ifdef	_KERNEL
extern pte_t *ptable;
extern int ptab_cnt;
#endif	/* _KERNEL */
#endif	/* LOCORE */

/*
 * Bits in DSISR:
 */
#define	DSISR_DIRECT	0x80000000
#define	DSISR_NOTFOUND	0x40000000
#define	DSISR_PROTECT	0x08000000
#define	DSISR_INVRX	0x04000000
#define	DSISR_STORE	0x02000000
#define	DSISR_DABR	0x00400000
#define	DSISR_SEGMENT	0x00200000
#define	DSISR_EAR	0x00100000

/*
 * Bits in SRR1 on ISI:
 */
#define	ISSRR1_NOTFOUND	0x40000000
#define	ISSRR1_DIRECT	0x10000000
#define	ISSRR1_PROTECT	0x08000000
#define	ISSRR1_SEGMENT	0x00200000

#ifdef	_KERNEL
#ifndef	LOCORE
extern u_int dsisr __P((void));
#endif	/* _KERNEL */
#endif	/* LOCORE */
#endif	/* _MACHINE_PTE_H_ */
