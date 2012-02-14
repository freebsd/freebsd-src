/*-
 * Copyright (c) 2001 Doug Rabson
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
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_PTE_H_
#define	_MACHINE_PTE_H_

#define	PTE_PRESENT	0x0000000000000001
#define	PTE__RV1_	0x0000000000000002
#define	PTE_MA_MASK	0x000000000000001C
#define	PTE_MA_WB	0x0000000000000000
#define	PTE_MA_UC	0x0000000000000010
#define	PTE_MA_UCE	0x0000000000000014
#define	PTE_MA_WC	0x0000000000000018
#define	PTE_MA_NATPAGE	0x000000000000001C
#define	PTE_ACCESSED	0x0000000000000020
#define	PTE_DIRTY	0x0000000000000040
#define	PTE_PL_MASK	0x0000000000000180
#define	PTE_PL_KERN	0x0000000000000000
#define	PTE_PL_USER	0x0000000000000180
#define	PTE_AR_MASK	0x0000000000000E00
#define	PTE_AR_R	0x0000000000000000
#define	PTE_AR_RX	0x0000000000000200
#define	PTE_AR_RW	0x0000000000000400
#define	PTE_AR_RWX	0x0000000000000600
#define	PTE_AR_R_RW	0x0000000000000800
#define	PTE_AR_RX_RWX	0x0000000000000A00
#define	PTE_AR_RWX_RW	0x0000000000000C00
#define	PTE_AR_X_RX	0x0000000000000E00
#define	PTE_PPN_MASK	0x0003FFFFFFFFF000
#define	PTE__RV2_	0x000C000000000000
#define	PTE_ED		0x0010000000000000
#define	PTE_IG_MASK	0xFFE0000000000000
#define	PTE_WIRED	0x0020000000000000
#define	PTE_MANAGED	0x0040000000000000
#define	PTE_PROT_MASK	0x0700000000000000

#define	ITIR__RV1_	0x0000000000000003
#define	ITIR_PS_MASK	0x00000000000000FC
#define	ITIR_KEY_MASK	0x00000000FFFFFF00
#define	ITIR__RV2_	0xFFFFFFFF00000000

#ifndef LOCORE

typedef uint64_t pt_entry_t;

static __inline pt_entry_t
pte_atomic_clear(pt_entry_t *ptep, uint64_t val)
{
	return (atomic_clear_64(ptep, val));
}

static __inline pt_entry_t
pte_atomic_set(pt_entry_t *ptep, uint64_t val)
{
	return (atomic_set_64(ptep, val));
}

/*
 * A long-format VHPT entry.
 */
struct ia64_lpte {
	pt_entry_t	pte;
	uint64_t	itir;
	uint64_t	tag;		/* includes ti */
	uint64_t	chain;		/* pa of collision chain */
};

/*
 * Layout of rr[x].
 */
struct ia64_rr {
	uint64_t	rr_ve	:1;	/* bit 0 */
	uint64_t	__rv1__	:1;	/* bit 1 */
	uint64_t	rr_ps	:6;	/* bits 2..7 */
	uint64_t	rr_rid	:24;	/* bits 8..31 */
	uint64_t	__rv2__	:32;	/* bits 32..63 */
};

#endif /* !LOCORE */

#endif /* !_MACHINE_PTE_H_ */
