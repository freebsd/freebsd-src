/*-
 * Copyright (c) 2004-2010 Juli Mallett <jmallett@FreeBSD.org>
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

#ifndef	_MACHINE_PTE_H_
#define	_MACHINE_PTE_H_

/*
 * TLB and PTE management.  Most things operate within the context of
 * EntryLo0,1, and begin with TLBLO_.  Things which work with EntryHi
 * start with TLBHI_.  PTE bits begin with PG_.
 *
 * Note that we use the same size VM and TLB pages.
 */
#define	TLB_PAGE_SHIFT	(PAGE_SHIFT)
#define	TLB_PAGE_SIZE	(1 << TLB_PAGE_SHIFT)
#define	TLB_PAGE_MASK	(TLB_PAGE_SIZE - 1)

/*
 * TLB PageMask register.  Has mask bits set above the default, 4K, page mask.
 */
#define	TLBMASK_SHIFT	(13)
#define	TLBMASK_MASK	((PAGE_MASK >> TLBMASK_SHIFT) << TLBMASK_SHIFT)

/*
 * PFN for EntryLo register.  Upper bits are 0, which is to say that
 * bit 29 is the last hardware bit;  Bits 30 and upwards (EntryLo is
 * 64 bit though it can be referred to in 32-bits providing 2 software
 * bits safely.  We use it as 64 bits to get many software bits, and
 * god knows what else.) are unacknowledged by hardware.  They may be
 * written as anything, but otherwise they have as much meaning as
 * other 0 fields.
 */
#define	TLBLO_SWBITS_SHIFT	(30)
#define	TLBLO_SWBITS_MASK	(0x3U << TLBLO_SWBITS_SHIFT)
#define	TLBLO_PFN_SHIFT		(6)
#define	TLBLO_PFN_MASK		(0x3FFFFFC0)
#define	TLBLO_PA_TO_PFN(pa)	((((pa) >> TLB_PAGE_SHIFT) << TLBLO_PFN_SHIFT) & TLBLO_PFN_MASK)
#define	TLBLO_PFN_TO_PA(pfn)	((vm_paddr_t)((pfn) >> TLBLO_PFN_SHIFT) << TLB_PAGE_SHIFT)
#define	TLBLO_PTE_TO_PFN(pte)	((pte) & TLBLO_PFN_MASK)
#define	TLBLO_PTE_TO_PA(pte)	(TLBLO_PFN_TO_PA(TLBLO_PTE_TO_PFN((pte))))
  
/*
 * VPN for EntryHi register.  Upper two bits select user, supervisor,
 * or kernel.  Bits 61 to 40 copy bit 63.  VPN2 is bits 39 and down to
 * as low as 13, down to PAGE_SHIFT, to index 2 TLB pages*.  From bit 12
 * to bit 8 there is a 5-bit 0 field.  Low byte is ASID.
 *
 * Note that in FreeBSD, we map 2 TLB pages is equal to 1 VM page.
 */
#define	TLBHI_ASID_MASK		(0xff)
#define	TLBHI_ENTRY(va, asid)	(((va) & ~PAGE_MASK) | ((asid) & TLBHI_ASID_MASK))

#ifndef _LOCORE
typedef unsigned int pt_entry_t;
typedef pt_entry_t *pd_entry_t;
#endif

#define	PDESIZE		sizeof(pd_entry_t)	/* for assembly files */
#define	PTESIZE		sizeof(pt_entry_t)	/* for assembly files */

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


/* User virtual to pte offset in page table */
#define	vad_to_pte_offset(adr)	(((adr) >> PAGE_SHIFT) & (NPTEPG -1))

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

#endif /* !_MACHINE_PTE_H_ */
