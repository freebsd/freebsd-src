/*	$OpenBSD: pte.h,v 1.4 1998/01/28 13:46:25 pefo Exp $	*/

/*-
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 *	from: Utah Hdr: pte.h 1.11 89/09/03
 *	from: @(#)pte.h 8.1 (Berkeley) 6/10/93
 *	JNPR: pte.h,v 1.1.4.1 2007/09/10 06:20:19 girish
 * $FreeBSD$
 */

#ifndef _MACHINE_PTE_H_
#define	_MACHINE_PTE_H_

#include <machine/endian.h>

/*
 * MIPS hardware page table entry
 */

#ifndef _LOCORE
struct pte {
#if BYTE_ORDER == BIG_ENDIAN
unsigned int	pg_prot:2,		/* SW: access control */
		pg_pfnum:24,		/* HW: core page frame number or 0 */
		pg_attr:3,		/* HW: cache attribute */
		pg_m:1,			/* HW: modified (dirty) bit */
		pg_v:1,			/* HW: valid bit */
		pg_g:1;			/* HW: ignore pid bit */
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
unsigned int	pg_g:1,			/* HW: ignore pid bit */
		pg_v:1,			/* HW: valid bit */
		pg_m:1,			/* HW: modified (dirty) bit */
		pg_attr:3,		/* HW: cache attribute */
		pg_pfnum:24,		/* HW: core page frame number or 0 */
		pg_prot:2;		/* SW: access control */
#endif
};

/*
 * Structure defining an tlb entry data set.
 */

struct tlb {
	int	tlb_mask;
	int	tlb_hi;
	int	tlb_lo0;
	int	tlb_lo1;
};

typedef unsigned long pt_entry_t;
typedef pt_entry_t *pd_entry_t;

#define	PDESIZE		sizeof(pd_entry_t)	/* for assembly files */
#define	PTESIZE		sizeof(pt_entry_t)	/* for assembly files */

#endif /* _LOCORE */

#define	PT_ENTRY_NULL	((pt_entry_t *) 0)

#define	PTE_WIRED	0x80000000	/* SW */
#define	PTE_W		PTE_WIRED
#define	PTE_RO		0x40000000	/* SW */

#define	PTE_G		0x00000001	/* HW */
#define	PTE_V		0x00000002
/*#define	PTE_NV		0x00000000       Not Used */
#define	PTE_M		0x00000004
#define	PTE_RW		PTE_M
#define PTE_ODDPG       0x00001000 
/*#define	PG_ATTR		0x0000003f  Not Used */
#define	PTE_UNCACHED	0x00000010
#ifdef CPU_SB1
#define	PTE_CACHE	0x00000028	/* cacheable coherent */
#else
#define	PTE_CACHE	0x00000018
#endif
/*#define	PG_CACHEMODE	0x00000038 Not Used*/
#define	PTE_ROPAGE	(PTE_V | PTE_RO | PTE_CACHE) /* Write protected */
#define	PTE_RWPAGE	(PTE_V | PTE_M | PTE_CACHE)  /* Not wr-prot not clean */
#define	PTE_CWPAGE	(PTE_V | PTE_CACHE)	   /* Not wr-prot but clean */
#define	PTE_IOPAGE	(PTE_G | PTE_V | PTE_M | PTE_UNCACHED)
#define	PTE_FRAME	0x3fffffc0
#define PTE_HVPN        0xffffe000      /* Hardware page no mask */
#define PTE_ASID        0x000000ff      /* Address space ID */

#define	PTE_SHIFT	6
#define	pfn_is_ext(x)	((x) & 0x3c000000)
#define	vad_to_pfn(x)	(((unsigned)(x) >> PTE_SHIFT) & PTE_FRAME)
#define	vad_to_pfn64(x)	((quad_t)(x) >> PTE_SHIFT) & PTE_FRAME)
#define	pfn_to_vad(x)	(((x) & PTE_FRAME) << PTE_SHIFT)

/* User virtual to pte offset in page table */
#define	vad_to_pte_offset(adr)	(((adr) >> PGSHIFT) & (NPTEPG -1))

#define	mips_pg_v(entry)	((entry) & PTE_V)
#define	mips_pg_wired(entry)	((entry) & PTE_WIRED)
#define	mips_pg_m_bit()		(PTE_M)
#define	mips_pg_rw_bit()	(PTE_M)
#define	mips_pg_ro_bit()	(PTE_RO)
#define	mips_pg_ropage_bit()	(PTE_ROPAGE)
#define	mips_pg_rwpage_bit()	(PTE_RWPAGE)
#define	mips_pg_cwpage_bit()	(PTE_CWPAGE)
#define	mips_pg_global_bit()	(PTE_G)
#define	mips_pg_wired_bit()	(PTE_WIRED)
#define	mips_tlbpfn_to_paddr(x)	pfn_to_vad((x))
#define	mips_paddr_to_tlbpfn(x)	vad_to_pfn((x))

/* These are not used */
#define	PTE_SIZE_4K	0x00000000
#define	PTE_SIZE_16K	0x00006000
#define	PTE_SIZE_64K	0x0001e000
#define	PTE_SIZE_256K	0x0007e000
#define	PTE_SIZE_1M	0x001fe000
#define	PTE_SIZE_4M	0x007fe000
#define	PTE_SIZE_16M	0x01ffe000

#endif	/* !_MACHINE_PTE_H_ */
