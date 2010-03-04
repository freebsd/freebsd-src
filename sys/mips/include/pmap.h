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
 *	from: hp300: @(#)pmap.h 7.2 (Berkeley) 12/16/90
 *	from: @(#)pmap.h	7.4 (Berkeley) 5/12/91
 *	from: src/sys/i386/include/pmap.h,v 1.65.2.2 2000/11/30 01:54:42 peter
 *	JNPR: pmap.h,v 1.7.2.1 2007/09/10 07:44:12 girish
 *      $FreeBSD$
 */

#ifndef _MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#include <machine/vmparam.h>
#include <machine/pte.h>

#define	VADDR(pdi, pti)	((vm_offset_t)(((pdi)<<PDRSHIFT)|((pti)<<PAGE_SHIFT)))

#define	NKPT		120	/* actual number of kernel page tables */

#ifndef NKPDE
#define	NKPDE		255	/* addressable number of page tables/pde's */
#endif

#define	KPTDI		(VM_MIN_KERNEL_ADDRESS >> SEGSHIFT)
#define	NUSERPGTBLS	(VM_MAXUSER_ADDRESS >> SEGSHIFT)

#ifndef LOCORE

#include <sys/queue.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>

/*
 * Pmap stuff
 */
struct pv_entry;

struct md_page {
	int pv_list_count;
	int pv_flags;
	TAILQ_HEAD(, pv_entry) pv_list;
};

#define	PV_TABLE_MOD		0x01	/* modified */
#define	PV_TABLE_REF		0x02	/* referenced */

#define	ASID_BITS		8
#define	ASIDGEN_BITS		(32 - ASID_BITS)
#define	ASIDGEN_MASK		((1 << ASIDGEN_BITS) - 1)

struct pmap {
	pd_entry_t *pm_segtab;	/* KVA of segment table */
	TAILQ_HEAD(, pv_entry) pm_pvlist;	/* list of mappings in
						 * pmap */
	int pm_active;		/* active on cpus */
	struct {
		u_int32_t asid:ASID_BITS;	/* TLB address space tag */
		u_int32_t gen:ASIDGEN_BITS;	/* its generation number */
	}      pm_asid[MAXSMPCPU];
	struct pmap_statistics pm_stats;	/* pmap statistics */
	struct vm_page *pm_ptphint;	/* pmap ptp hint */
	struct mtx pm_mtx;
};

typedef struct pmap *pmap_t;

#ifdef	_KERNEL

pt_entry_t *pmap_pte(pmap_t, vm_offset_t);
pd_entry_t pmap_segmap(pmap_t pmap, vm_offset_t va);
vm_offset_t pmap_kextract(vm_offset_t va);

#define	vtophys(va)	pmap_kextract(((vm_offset_t) (va)))

extern struct pmap	kernel_pmap_store;
#define kernel_pmap	(&kernel_pmap_store)

#define	PMAP_LOCK(pmap)		mtx_lock(&(pmap)->pm_mtx)
#define	PMAP_LOCK_ASSERT(pmap, type)	mtx_assert(&(pmap)->pm_mtx, (type))
#define	PMAP_LOCK_DESTROY(pmap) mtx_destroy(&(pmap)->pm_mtx)
#define	PMAP_LOCK_INIT(pmap)	mtx_init(&(pmap)->pm_mtx, "pmap", \
				    NULL, MTX_DEF)
#define	PMAP_LOCKED(pmap)	mtx_owned(&(pmap)->pm_mtx)
#define	PMAP_MTX(pmap)		(&(pmap)->pm_mtx)
#define	PMAP_TRYLOCK(pmap)	mtx_trylock(&(pmap)->pm_mtx)
#define	PMAP_UNLOCK(pmap)	mtx_unlock(&(pmap)->pm_mtx)

#define PMAP_LGMEM_LOCK_INIT(sysmap) mtx_init(&(sysmap)->lock, "pmap-lgmem", \
				    "per-cpu-map", (MTX_DEF| MTX_DUPOK))
#define PMAP_LGMEM_LOCK(sysmap) mtx_lock(&(sysmap)->lock)
#define PMAP_LGMEM_UNLOCK(sysmap) mtx_unlock(&(sysmap)->lock)
#define PMAP_LGMEM_DESTROY(sysmap) mtx_destroy(&(sysmap)->lock)

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_table.
 */
typedef struct pv_entry {
	pmap_t pv_pmap;		/* pmap where mapping lies */
	vm_offset_t pv_va;	/* virtual address for mapping */
	TAILQ_ENTRY(pv_entry) pv_list;
	TAILQ_ENTRY(pv_entry) pv_plist;
	vm_page_t pv_ptem;	/* VM page for pte */
	boolean_t pv_wired;	/* whether this entry is wired */
}       *pv_entry_t;


#if defined(DIAGNOSTIC)
#define	PMAP_DIAGNOSTIC
#endif

/*
 * physmem_desc[] is a superset of phys_avail[] and describes all the
 * memory present in the system.
 *
 * phys_avail[] is similar but does not include the memory stolen by
 * pmap_steal_memory().
 *
 * Each memory region is described by a pair of elements in the array
 * so we can describe up to (PHYS_AVAIL_ENTRIES / 2) distinct memory
 * regions.
 */
#define	PHYS_AVAIL_ENTRIES	10
extern vm_offset_t phys_avail[PHYS_AVAIL_ENTRIES + 2];
extern vm_offset_t physmem_desc[PHYS_AVAIL_ENTRIES + 2];

extern char *ptvmmap;		/* poor name! */
extern vm_offset_t virtual_avail;
extern vm_offset_t virtual_end;
extern pd_entry_t *segbase;

extern vm_paddr_t mips_wired_tlb_physmem_start;
extern vm_paddr_t mips_wired_tlb_physmem_end;
extern u_int need_wired_tlb_page_pool;

#define	pmap_page_get_memattr(m)	VM_MEMATTR_DEFAULT
#define	pmap_page_is_mapped(m)	(!TAILQ_EMPTY(&(m)->md.pv_list))
#define	pmap_page_set_memattr(m, ma)	(void)0

void pmap_bootstrap(void);
void *pmap_mapdev(vm_offset_t, vm_size_t);
void pmap_unmapdev(vm_offset_t, vm_size_t);
vm_offset_t pmap_steal_memory(vm_size_t size);
void pmap_set_modified(vm_offset_t pa);
int page_is_managed(vm_offset_t pa);
void pmap_page_is_free(vm_page_t m);
void pmap_kenter(vm_offset_t va, vm_paddr_t pa);
void pmap_kremove(vm_offset_t va);
void *pmap_kenter_temporary(vm_paddr_t pa, int i);
void pmap_kenter_temporary_free(vm_paddr_t pa);
int pmap_compute_pages_to_dump(void);
void pmap_update_page(pmap_t pmap, vm_offset_t va, pt_entry_t pte);
void pmap_flush_pvcache(vm_page_t m);

/*
 * floating virtual pages (FPAGES)
 *
 * These are the reserved virtual memory areas which can be
 * mapped to any physical memory.
 */
#define	FPAGES			2
#define	FPAGES_SHARED		2
#define	FSPACE			((FPAGES * MAXCPU + FPAGES_SHARED)  * PAGE_SIZE)
#define	PMAP_FPAGE1		0x00	/* Used by pmap_zero_page &
					 * pmap_copy_page */
#define	PMAP_FPAGE2		0x01	/* Used by pmap_copy_page */

#define	PMAP_FPAGE3		0x00	/* Used by pmap_zero_page_idle */
#define	PMAP_FPAGE_KENTER_TEMP	0x01	/* Used by coredump */

struct fpage {
	vm_offset_t kva;
	u_int state;
};

struct sysmaps {
	struct mtx lock;
	struct fpage fp[FPAGES];
};

vm_offset_t 
pmap_map_fpage(vm_paddr_t pa, struct fpage *fp,
    boolean_t check_unmaped);
void pmap_unmap_fpage(vm_paddr_t pa, struct fpage *fp);

#endif				/* _KERNEL */

#endif				/* !LOCORE */

#endif				/* !_MACHINE_PMAP_H_ */
