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

#ifdef LOCORE

#define PTE_P		(1<<0)
#define PTE_MA_WB	(0<<2)
#define PTE_MA_UC	(4<<2)
#define PTE_MA_UCE	(5<<2)
#define PTE_MA_WC	(6<<2)
#define PTE_MA_NATPAGE	(7<<2)
#define PTE_A		(1<<5)
#define PTE_D		(1<<6)
#define PTE_PL_KERN	(0<<7)
#define PTE_PL_USER	(3<<7)
#define PTE_AR_R	(0<<9)
#define PTE_AR_RX	(1<<9)
#define PTE_AR_RW	(2<<9)
#define PTE_AR_RWX	(3<<9)
#define PTE_AR_R_RW	(4<<9)
#define PTE_AR_RX_RWX	(5<<9)
#define PTE_AR_RWX_RW	(6<<9)
#define PTE_AR_X_RX	(7<<9)

#else

#define PTE_MA_WB	0
#define PTE_MA_UC	4
#define PTE_MA_UCE	5
#define PTE_MA_WC	6
#define PTE_MA_NATPAGE	7

#define PTE_PL_KERN	0
#define PTE_PL_USER	3

#define PTE_AR_R	0
#define PTE_AR_RX	1
#define PTE_AR_RW	2
#define PTE_AR_RWX	3
#define PTE_AR_R_RW	4
#define PTE_AR_RX_RWX	5
#define PTE_AR_RWX_RW	6
#define PTE_AR_X_RX	7

/*
 * A short-format VHPT entry. Also matches the TLB insertion format.
 */
struct ia64_pte {
	uint64_t	pte_p	:1;	/* bit 0 */
	uint64_t	__rv1__	:1;	/* bit 1 */
	uint64_t	pte_ma	:3;	/* bits 2..4 */
	uint64_t	pte_a	:1;	/* bit 5 */
	uint64_t	pte_d	:1;	/* bit 6 */
	uint64_t	pte_pl	:2;	/* bits 7..8 */
	uint64_t	pte_ar	:3;	/* bits 9..11 */
	uint64_t	pte_ppn	:38;	/* bits 12..49 */
	uint64_t	__rv2__	:2;	/* bits 50..51 */
	uint64_t	pte_ed	:1;	/* bit 52 */
	/* The following bits are ignored by the hardware. */
	uint64_t	pte_w	:1;	/* bit 53 */
	uint64_t	pte_m	:1;	/* bit 54 */
	uint64_t	pte_prot:3;	/* bits 55..57 */
	uint64_t	__ig__	:6;	/* bits 58..63 */
};

/*
 * A long-format VHPT entry.
 */
struct ia64_lpte {
	uint64_t	pte_p	:1;	/* bit 0 */
	uint64_t	__rv1__	:1;	/* bit 1 */
	uint64_t	pte_ma	:3;	/* bits 2..4 */
	uint64_t	pte_a	:1;	/* bit 5 */
	uint64_t	pte_d	:1;	/* bit 6 */
	uint64_t	pte_pl	:2;	/* bits 7..8 */
	uint64_t	pte_ar	:3;	/* bits 9..11 */
	uint64_t	pte_ppn	:38;	/* bits 12..49 */
	uint64_t	__rv2__	:2;	/* bits 50..51 */
	uint64_t	pte_ed	:1;	/* bit 52 */
	/* The following 11 bits are ignored by the hardware. */
	uint64_t	pte_w	:1;	/* bit 53 */
	uint64_t	pte_m	:1;	/* bit 54 */
	uint64_t	pte_prot:3;	/* bits 55..57 */
	uint64_t	__ig__	:6;	/* bits 58..63 */

	uint64_t	__rv3__	:2;	/* bits 0..1 */
	uint64_t	pte_ps	:6;	/* bits 2..7 */
	uint64_t	pte_key	:24;	/* bits 8..31 */
	uint64_t	__rv4__	:32;	/* bits 32..63 */

	uint64_t	pte_tag;	/* includes ti */

	uint64_t	pte_chain;	/* pa of collision chain */
};

/*
 * Layout of cr.itir.
 */
struct ia64_itir {
	uint64_t	__rv1__	:2;	/* bits 0..1 */
	uint64_t	itir_ps	:6;	/* bits 2..7 */
	uint64_t	itir_key:24;	/* bits 8..31 */
	uint64_t	__rv2__	:32;	/* bits 32..63 */
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
