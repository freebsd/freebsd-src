/*
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
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
 * Derived from hp300 version by Mike Hibler, this version by William
 * Jolitz uses a recursive map [a pde points to the page directory] to
 * map the page tables using the pagetables themselves. This is done to
 * reduce the impact on kernel virtual memory for lots of sparse address
 * space, and to reduce the cost of memory to each process.
 *
 *	from: hp300: @(#)pmap.h	7.2 (Berkeley) 12/16/90
 *	from: @(#)pmap.h	7.4 (Berkeley) 5/12/91
 * 	$Id: pmap.h,v 1.34 1996/02/25 03:02:53 dyson Exp $
 */

#ifndef _MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#include <machine/pte.h>

typedef unsigned int *pd_entry_t;
typedef unsigned int *pt_entry_t;
struct vm_map;

/*
 * NKPDE controls the virtual space of the kernel, what ever is left, minus
 * the alternate page table area is given to the user (NUPDE)
 */
/*
 * NKPDE controls the virtual space of the kernel, what ever is left is
 * given to the user (NUPDE)
 */
#ifndef NKPT
#if 0
#define	NKPT			26	/* actual number of kernel page tables */
#else
#define	NKPT			9	/* actual number of kernel page tables */
#endif
#endif
#ifndef NKPDE
#define NKPDE			63	/* addressable number of page tables/pde's */
#endif

#define	NUPDE		(NPTEPG-NKPDE)	/* number of user pde's */

/*
 * The *PTDI values control the layout of virtual memory
 *
 * XXX This works for now, but I am not real happy with it, I'll fix it
 * right after I fix locore.s and the magic 28K hole
 */
#define	APTDPTDI	(NPTEPG-1)	/* alt ptd entry that points to APTD */
#define	KPTDI		(APTDPTDI-NKPDE)/* start of kernel virtual pde's */
#define	PTDPTDI		(KPTDI-1)	/* ptd entry that points to ptd! */
#define	KSTKPTDI	(PTDPTDI-1)	/* ptd entry for u./kernel&user stack */
#define KSTKPTEOFF	(NBPG/sizeof(pd_entry_t)-UPAGES) /* pte entry for kernel stack */

#define PDESIZE		sizeof(pd_entry_t) /* for assembly files */
#define PTESIZE		sizeof(pt_entry_t) /* for assembly files */

/*
 * Address of current and alternate address space page table maps
 * and directories.
 */
#ifdef KERNEL
extern pt_entry_t PTmap[], APTmap[], Upte;
extern pd_entry_t PTD[], APTD[], PTDpde, APTDpde, Upde;

extern int	IdlePTD;	/* physical address of "Idle" state directory */
#endif

/*
 * virtual address to page table entry and
 * to physical address. Likewise for alternate address space.
 * Note: these work recursively, thus vtopte of a pte will give
 * the corresponding pde that in turn maps it.
 */
#define	vtopte(va)	(PTmap + i386_btop(va))
#define	kvtopte(va)	vtopte(va)
#define	ptetov(pt)	(i386_ptob(pt - PTmap))
#define	vtophys(va)	(((int) (*vtopte(va))&PG_FRAME) | ((int)(va) & PGOFSET))
#define	ispt(va)	((va) >= UPT_MIN_ADDRESS && (va) <= KPT_MAX_ADDRESS)

#define	avtopte(va)	(APTmap + i386_btop(va))
#define	ptetoav(pt)	(i386_ptob(pt - APTmap))
#define	avtophys(va)	(((int) (*avtopte(va))&PG_FRAME) | ((int)(va) & PGOFSET))

#ifdef KERNEL
/*
 *	Routine:	pmap_kextract
 *	Function:
 *		Extract the physical page address associated
 *		kernel virtual address.
 */
static __inline vm_offset_t
pmap_kextract(vm_offset_t va)
{
	vm_offset_t pa = *(int *)vtopte(va);
	pa = (pa & PG_FRAME) | (va & ~PG_FRAME);
	return pa;
}
#endif

/*
 * macros to generate page directory/table indices
 */

#define	pdei(va)	(((va)&PD_MASK)>>PD_SHIFT)
#define	ptei(va)	(((va)&PT_MASK)>>PG_SHIFT)

/*
 * Pmap stuff
 */

struct pmap {
	pd_entry_t		*pm_pdir;	/* KVA of page directory */
	short			pm_dref;	/* page directory ref count */
	short			pm_count;	/* pmap reference count */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	struct	vm_map		*pm_map;	/* map that owns this pmap */
};

typedef struct pmap	*pmap_t;

#ifdef KERNEL
extern pmap_t		kernel_pmap;
#endif

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_table.
 */
typedef struct pv_entry {
	struct pv_entry	*pv_next;	/* next pv_entry */
	pmap_t		pv_pmap;	/* pmap where mapping lies */
	vm_offset_t	pv_va;		/* virtual address for mapping */
	vm_page_t	pv_ptem;	/* VM page for pte */
} *pv_entry_t;

#define	PV_ENTRY_NULL	((pv_entry_t) 0)

#define	PV_CI		0x01	/* all entries must be cache inhibited */
#define	PV_PTPAGE	0x02	/* entry maps a page table page */

#ifdef	KERNEL

extern caddr_t	CADDR1;
extern pt_entry_t *CMAP1;
extern vm_offset_t avail_end;
extern vm_offset_t avail_start;
extern vm_offset_t phys_avail[];
extern pv_entry_t pv_table;	/* array of entries, one per page */
extern vm_offset_t virtual_avail;
extern vm_offset_t virtual_end;

#define	pa_index(pa)		atop(pa - vm_first_phys)
#define	pa_to_pvh(pa)		(&pv_table[pa_index(pa)])

#define	pmap_resident_count(pmap)	((pmap)->pm_stats.resident_count)

struct pcb;

void	pmap_bootstrap __P(( vm_offset_t, vm_offset_t));
pmap_t	pmap_kernel __P((void));
void	*pmap_mapdev __P((vm_offset_t, vm_size_t));
pt_entry_t * __pure pmap_pte __P((pmap_t, vm_offset_t)) __pure2;
void	pmap_unuse_pt __P((pmap_t, vm_offset_t, vm_page_t));
vm_page_t pmap_use_pt __P((pmap_t, vm_offset_t));

#endif /* KERNEL */

#endif /* !_MACHINE_PMAP_H_ */
