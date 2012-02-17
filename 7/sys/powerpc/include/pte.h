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

/* 32-bit PTE */
struct pte {
	u_int32_t pte_hi;
	u_int32_t pte_lo;
};

struct pteg {
	struct	pte pt[8];
};

/* 64-bit (long) PTE */
struct lpte {
	u_int64_t pte_hi;
	u_int64_t pte_lo;
};

struct lpteg {
	struct lpte pt[8];
};

#endif	/* LOCORE */

/* 32-bit PTE definitions */

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
#define	PTE_SO		0x00000000	/* Super. Only       (U: XX, S: RW) */
#define PTE_SW		0x00000001	/* Super. Write-Only (U: RO, S: RW) */
#define	PTE_BW		0x00000002	/* Supervisor        (U: RW, S: RW) */
#define	PTE_BR		0x00000003	/* Both Read Only    (U: RO, S: RO) */
#define	PTE_RW		PTE_BW
#define	PTE_RO		PTE_BR

#define	PTE_EXEC	0x00000200	/* pseudo bit in attrs; page is exec */

/* 64-bit PTE definitions */

/* High quadword: */
#define LPTE_VSID_SHIFT		12
#define LPTE_API		0x0000000000000F80ULL
#define LPTE_BIG		0x0000000000000004ULL	/* 4kb/16Mb page */
#define LPTE_HID		0x0000000000000002ULL
#define LPTE_VALID		0x0000000000000001ULL

/* Low quadword: */
#define EXTEND_PTE(x)	UINT64_C(x)	/* make constants 64-bit */
#define	LPTE_RPGN	0xfffffffffffff000ULL
#define	LPTE_REF	EXTEND_PTE( PTE_REF )
#define	LPTE_CHG	EXTEND_PTE( PTE_CHG )
#define	LPTE_WIMG	EXTEND_PTE( PTE_WIMG )
#define	LPTE_W		EXTEND_PTE( PTE_W )
#define	LPTE_I		EXTEND_PTE( PTE_I )
#define	LPTE_M		EXTEND_PTE( PTE_M )
#define	LPTE_G		EXTEND_PTE( PTE_G )
#define	LPTE_NOEXEC	0x0000000000000004ULL
#define	LPTE_PP		EXTEND_PTE( PTE_PP )

#define	LPTE_SO		EXTEND_PTE( PTE_SO )	/* Super. Only */
#define	LPTE_SW		EXTEND_PTE( PTE_SW )	/* Super. Write-Only */
#define	LPTE_BW		EXTEND_PTE( PTE_BW )	/* Supervisor */
#define	LPTE_BR		EXTEND_PTE( PTE_BR )	/* Both Read Only */
#define	LPTE_RW		LPTE_BW
#define	LPTE_RO		LPTE_BR

#ifndef	LOCORE
typedef	struct pte pte_t;
typedef	struct lpte lpte_t;
#endif	/* LOCORE */

/*
 * Extract bits from address
 */
#define	ADDR_SR_SHFT	28
#define	ADDR_PIDX	0x0ffff000
#define	ADDR_PIDX_SHFT	12
#define	ADDR_API_SHFT	22
#define	ADDR_POFF	0x00000fff

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
extern u_int dsisr(void);
#endif	/* _KERNEL */
#endif	/* LOCORE */
#endif	/* _MACHINE_PTE_H_ */
