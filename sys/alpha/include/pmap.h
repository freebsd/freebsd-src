/*-
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
 *	from: i386 pmap.h,v 1.54 1997/11/20 19:30:35 bde Exp
 * $FreeBSD$
 */

#ifndef _MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#include <machine/chipset.h>
#include <sys/systm.h>

/*
 * Define meanings for a few software bits in the pte
 */
#define	PG_V		ALPHA_PTE_VALID
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
#define PG_SHIFT	32

#define PG_W		0x00010000 /* software wired */
#define PG_MANAGED	0x00020000 /* software managed */

/*
 * Pte related macros
 */
#define VADDR(l1, l2, l3)	(((l1) << ALPHA_L1SHIFT)	\
				 + ((l2) << ALPHA_L2SHIFT)	\
				 + ((l3) << ALPHA_L3SHIFT)

#ifndef NKPT
#define	NKPT			9	/* initial number of kernel page tables */
#endif
#define NKLEV2MAPS		255	/* max number of lev2 page tables */
#define NKLEV3MAPS		(NKLEV2MAPS << ALPHA_PTSHIFT) /* max number of lev3 page tables */

/*
 * The *PTDI values control the layout of virtual memory
 *
 * XXX This works for now, but I am not real happy with it, I'll fix it
 * right after I fix locore.s and the magic 28K hole
 *
 * SMP_PRIVPAGES: The per-cpu address space is 0xff80000 -> 0xffbfffff
 */
#define PTLEV1I		(NPTEPG-1)	/* Lev0 entry that points to Lev0 */
#define K0SEGLEV1I	(NPTEPG/2)
#define K1SEGLEV1I	(K0SEGLEV1I+(NPTEPG/4))

#define NUSERLEV2MAPS	(NPTEPG/2)
#define NUSERLEV3MAPS	(NUSERLEV2MAPS << ALPHA_PTSHIFT)

#ifndef LOCORE

#include <sys/queue.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>

typedef alpha_pt_entry_t pt_entry_t;

#define PTESIZE		sizeof(pt_entry_t) /* for assembly files */

/*
 * Address of current address space page table maps
 */
#ifdef _KERNEL
extern pt_entry_t PTmap[];	/* lev3 page tables */
extern pt_entry_t PTlev2[];	/* lev2 page tables */
extern pt_entry_t PTlev1[];	/* lev1 page table */
extern pt_entry_t PTlev1pte;	/* pte that maps lev1 page table */
#endif

#ifdef _KERNEL
/*
 * virtual address to page table entry and
 * to physical address.
 * Note: this work recursively, thus vtopte of a pte will give
 * the corresponding lev1 that in turn maps it.
 */
#define	vtopte(va)	(PTmap + (alpha_btop(va) \
				  & ((1 << 3*ALPHA_PTSHIFT)-1)))

/*
 *	Routine:	pmap_kextract
 *	Function:
 *		Extract the physical page address associated
 *		kernel virtual address.
 */
static __inline vm_offset_t
pmap_kextract(vm_offset_t va)
{
	vm_offset_t pa;
	if (va >= ALPHA_K0SEG_BASE && va <= ALPHA_K0SEG_END)
		pa = ALPHA_K0SEG_TO_PHYS(va);
	else
		pa = alpha_ptob(ALPHA_PTE_TO_PFN(*vtopte(va)))
			| (va & PAGE_MASK);
	return pa;
}

#define	vtophys(va)	pmap_kextract(((vm_offset_t) (va)))

static __inline vm_offset_t
alpha_XXX_dmamap(vm_offset_t va)
{
	vm_offset_t pa = pmap_kextract(va);
	if (pa >= chipset.dmsize)
		panic ("driver uses alpha_XXX_dmamap() for an address that"
		    "is not within direct map");
	if (chipset.pci_sgmap != NULL)
		panic ("driver uses alpha_XXX_dmamap() on largemem system");
	return (pa + chipset.dmoffset);
}

#endif /* _KERNEL */

/*
 * Pmap stuff
 */
struct	pv_entry;

struct md_page {
	int pv_list_count;
	TAILQ_HEAD(,pv_entry)	pv_list;
};

#define	ASN_BITS	8
#define	ASNGEN_BITS	(32 - ASN_BITS)
#define	ASNGEN_MASK	((1 << ASNGEN_BITS) - 1)

struct pmap {
	struct mtx		pm_mtx;
	pt_entry_t		*pm_lev1;	/* KVA of lev0map */
	TAILQ_HEAD(,pv_entry)	pm_pvlist;	/* list of mappings in pmap */
	u_int32_t		pm_active;	/* active cpus */
	struct {
		u_int32_t	asn:ASN_BITS;	/* address space number */
		u_int32_t	gen:ASNGEN_BITS; /* generation number */
	}			pm_asn[MAXSMPCPU];
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	struct	vm_page		*pm_ptphint;	/* pmap ptp hint */
	LIST_ENTRY(pmap)	pm_list;	/* list of all pmaps. */
};

typedef struct pmap	*pmap_t;

#ifdef _KERNEL
extern struct pmap	kernel_pmap_store;
#define kernel_pmap	(&kernel_pmap_store)

#define	PMAP_LOCK(pmap)		mtx_lock(&(pmap)->pm_mtx)
#define	PMAP_LOCK_ASSERT(pmap, type) \
				mtx_assert(&(pmap)->pm_mtx, (type))
#define	PMAP_LOCK_DESTROY(pmap)	mtx_destroy(&(pmap)->pm_mtx)
#define	PMAP_LOCK_INIT(pmap)	mtx_init(&(pmap)->pm_mtx, "pmap", \
				    NULL, MTX_DEF)
#define	PMAP_LOCKED(pmap)	mtx_owned(&(pmap)->pm_mtx)
#define	PMAP_MTX(pmap)		(&(pmap)->pm_mtx)
#define	PMAP_TRYLOCK(pmap)	mtx_trylock(&(pmap)->pm_mtx)
#define	PMAP_UNLOCK(pmap)	mtx_unlock(&(pmap)->pm_mtx)
#endif

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_table.
 */
typedef struct pv_entry {
	pmap_t		pv_pmap;	/* pmap where mapping lies */
	vm_offset_t	pv_va;		/* virtual address for mapping */
	TAILQ_ENTRY(pv_entry)	pv_list;
	TAILQ_ENTRY(pv_entry)	pv_plist;
	vm_page_t	pv_ptem;	/* VM page for pte */
} *pv_entry_t;

#ifdef	_KERNEL

extern vm_offset_t phys_avail[];
extern vm_offset_t virtual_avail;
extern vm_offset_t virtual_end;

struct vmspace;

#define	pmap_page_is_mapped(m)	(!TAILQ_EMPTY(&(m)->md.pv_list))

vm_offset_t pmap_steal_memory(vm_size_t);
void	pmap_bootstrap(vm_offset_t, u_int);
void	pmap_kenter(vm_offset_t va, vm_offset_t pa);
void	*pmap_kenter_temporary(vm_offset_t pa, int i);
void	pmap_kremove(vm_offset_t);
void	pmap_setdevram(unsigned long long basea, vm_offset_t sizea);
int	pmap_uses_prom_console(void);
void	*pmap_mapdev(vm_offset_t, vm_size_t);
void	pmap_unmapdev(vm_offset_t, vm_size_t);
unsigned *pmap_pte(pmap_t, vm_offset_t) __pure2;
void	pmap_set_opt	(unsigned *);
void	pmap_set_opt_bsp	(void);
void	pmap_deactivate(struct thread *td);
void	pmap_emulate_reference(struct vmspace *vm, vm_offset_t v, int user, int write);

#endif /* _KERNEL */

#endif /* !LOCORE */

#endif /* !_MACHINE_PMAP_H_ */
