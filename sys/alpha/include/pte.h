/* $Id: pte.h,v 1.1.1.1 1998/03/09 05:43:16 jb Exp $ */
/* From: NetBSD: pte.h,v 1.10 1997/09/02 19:07:22 thorpej Exp */

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * Alpha page table entry.
 * Things which are in the VMS PALcode but not in the OSF PALcode
 * are marked with "(VMS)".
 *
 * This information derived from pp. (II) 3-3 - (II) 3-6 and
 * (III) 3-3 - (III) 3-5 of the "Alpha Architecture Reference Manual" by
 * Richard L. Sites.
 */

/*
 * Alpha Page Table Entry
 */

#include <machine/alpha_cpu.h>

typedef	alpha_pt_entry_t	pt_entry_t;

#define	PT_ENTRY_NULL	((pt_entry_t *) 0)
#define	PTESHIFT	3			/* pte size == 1 << PTESHIFT */

#define	PG_V		ALPHA_PTE_VALID
#define	PG_NV		0
#define	PG_FOR		ALPHA_PTE_FAULT_ON_READ
#define	PG_FOW		ALPHA_PTE_FAULT_ON_WRITE
#define	PG_FOE		ALPHA_PTE_FAULT_ON_EXECUTE
#define	PG_ASM		ALPHA_PTE_ASM
#define	PG_GH		ALPHA_PTE_GRANULARITY
#define	PG_KRE		ALPHA_PTE_KR
#define	PG_URE		ALPHA_PTE_UR
#define	PG_KWE		ALPHA_PTE_KW
#define	PG_UWE		ALPHA_PTE_UW
#define	PG_PROT		ALPHA_PTE_PROT
#define	PG_RSVD		0x000000000000cc80	/* Reserved fpr hardware */
#define	PG_WIRED	0x0000000000010000	/* Wired. [SOFTWARE] */
#define	PG_FRAME	ALPHA_PTE_RAME
#define	PG_SHIFT	32
#define	PG_PFNUM(x)	ALPHA_PTE_TO_PFN(x)

#if 0 /* XXX NOT HERE */
#define	K0SEG_BEGIN	0xfffffc0000000000	/* unmapped, cached */
#define	K0SEG_END	0xfffffe0000000000
#define PHYS_UNCACHED	0x0000000040000000
#endif

#ifndef _LOCORE
#if 0 /* XXX NOT HERE */
#define	k0segtophys(x)	((vm_offset_t)(x) & 0x00000003ffffffff)
#define	phystok0seg(x)	((vm_offset_t)(x) | K0SEG_BEGIN)

#define phystouncached(x) ((vm_offset_t)(x) | PHYS_UNCACHED)
#define uncachedtophys(x) ((vm_offset_t)(x) & ~PHYS_UNCACHED)
#endif

#define	PTEMASK		(NPTEPG - 1)
#define	vatopte(va)	(((va) >> PGSHIFT) & PTEMASK)
#define	vatoste(va)	(((va) >> SEGSHIFT) & PTEMASK)
#define kvtol1pte(va) \
	(((vm_offset_t)(va) >> (PGSHIFT + 2*(PGSHIFT-PTESHIFT))) & PTEMASK)

#define	vatopa(va) \
	((PG_PFNUM(*kvtopte(va)) << PGSHIFT) | ((vm_offset_t)(va) & PGOFSET))

#define	ALPHA_STSIZE		((u_long)PAGE_SIZE)		/* 8k */
#define	ALPHA_MAX_PTSIZE	((u_long)(NPTEPG * NBPG))	/* 8M */

#ifdef _KERNEL
/*
 * Kernel virtual address to Sysmap entry and visa versa.
 */
#define	kvtopte(va) \
	(Sysmap + (((vm_offset_t)(va) - VM_MIN_KERNEL_ADDRESS) >> PGSHIFT))
#define	ptetokv(pte) \
	((((pt_entry_t *)(pte) - Sysmap) << PGSHIFT) + VM_MIN_KERNEL_ADDRESS)

extern	pt_entry_t *Lev1map;		/* Alpha Level One page table */
extern	pt_entry_t *Sysmap;		/* kernel pte table */
extern	vm_size_t Sysmapsize;		/* number of pte's in Sysmap */
#endif
#endif
