/*-
 * Copyright 2014 Svatopluk Kraus <onwahe@gmail.com>
 * Copyright 2014 Michal Meloun <meloun@miracle.cz>
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
 * The ARM version of this file was more or less based on the i386 version,
 * which has the following provenance...
 *
 * Derived from hp300 version by Mike Hibler, this version by William
 * Jolitz uses a recursive map [a pde points to the page directory] to
 * map the page tables using the pagetables themselves. This is done to
 * reduce the impact on kernel virtual memory for lots of sparse address
 * space, and to reduce the cost of memory to each process.
 *
 *      from: hp300: @(#)pmap.h 7.2 (Berkeley) 12/16/90
 *      from: @(#)pmap.h        7.4 (Berkeley) 5/12/91
 * 	from: FreeBSD: src/sys/i386/include/pmap.h,v 1.70 2000/11/30
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_PMAP_H_
#define _MACHINE_PMAP_H_

#include <sys/queue.h>
#include <sys/_cpuset.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>

typedef	uint32_t	pt1_entry_t;		/* L1 table entry */
typedef	uint32_t	pt2_entry_t;		/* L2 table entry */
typedef uint32_t	ttb_entry_t;		/* TTB entry */

#ifdef _KERNEL

#if 0
#define PMAP_PTE_NOCACHE // Use uncached page tables
#endif

/*
 *  (1) During pmap bootstrap, physical pages for L2 page tables are
 *      allocated in advance which are used for KVA continuous mapping
 *      starting from KERNBASE. This makes things more simple.
 *  (2) During vm subsystem initialization, only vm subsystem itself can
 *      allocate physical memory safely. As pmap_map() is called during
 *      this initialization, we must be prepared for that and have some
 *      preallocated physical pages for L2 page tables.
 *
 *  Note that some more pages for L2 page tables are preallocated too
 *  for mappings laying above VM_MAX_KERNEL_ADDRESS.
 */
#ifndef NKPT2PG
/*
 *  The optimal way is to define this in board configuration as
 *  definition here must be safe enough. It means really big.
 *
 *  1 GB KVA <=> 256 kernel L2 page table pages
 *
 *  From real platforms:
 *	1 GB physical memory <=> 10 pages is enough
 *	2 GB physical memory <=> 21 pages is enough
 */
#define NKPT2PG		32
#endif

extern vm_paddr_t phys_avail[];
extern vm_paddr_t dump_avail[];
extern char *_tmppt;      /* poor name! */
extern vm_offset_t virtual_avail;
extern vm_offset_t virtual_end;

/*
 * Pmap stuff
 */

/*
 * This structure is used to hold a virtual<->physical address
 * association and is used mostly by bootstrap code
 */
struct pv_addr {
	SLIST_ENTRY(pv_addr) pv_list;
	vm_offset_t	pv_va;
	vm_paddr_t	pv_pa;
};
#endif
struct	pv_entry;
struct	pv_chunk;

struct	md_page {
	TAILQ_HEAD(,pv_entry)	pv_list;
	uint16_t		pt2_wirecount[4];
	int			pat_mode;
};

struct	pmap {
	struct mtx		pm_mtx;
	pt1_entry_t		*pm_pt1;	/* KVA of pt1 */
	pt2_entry_t		*pm_pt2tab;	/* KVA of pt2 pages table */
	TAILQ_HEAD(,pv_chunk)	pm_pvchunk;	/* list of mappings in pmap */
	cpuset_t		pm_active;	/* active on cpus */
	struct pmap_statistics	pm_stats;	/* pmap statictics */
	LIST_ENTRY(pmap) 	pm_list;	/* List of all pmaps */
};

typedef struct pmap *pmap_t;

#ifdef _KERNEL
extern struct pmap	        kernel_pmap_store;
#define kernel_pmap	        (&kernel_pmap_store)

#define	PMAP_LOCK(pmap)		mtx_lock(&(pmap)->pm_mtx)
#define	PMAP_LOCK_ASSERT(pmap, type) \
				mtx_assert(&(pmap)->pm_mtx, (type))
#define	PMAP_LOCK_DESTROY(pmap)	mtx_destroy(&(pmap)->pm_mtx)
#define	PMAP_LOCK_INIT(pmap)	mtx_init(&(pmap)->pm_mtx, "pmap", \
				    NULL, MTX_DEF | MTX_DUPOK)
#define	PMAP_LOCKED(pmap)	mtx_owned(&(pmap)->pm_mtx)
#define	PMAP_MTX(pmap)		(&(pmap)->pm_mtx)
#define	PMAP_TRYLOCK(pmap)	mtx_trylock(&(pmap)->pm_mtx)
#define	PMAP_UNLOCK(pmap)	mtx_unlock(&(pmap)->pm_mtx)
#endif

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_list.
 */
typedef struct pv_entry {
	vm_offset_t	pv_va;		/* virtual address for mapping */
	TAILQ_ENTRY(pv_entry)	pv_next;
} *pv_entry_t;

/*
 * pv_entries are allocated in chunks per-process.  This avoids the
 * need to track per-pmap assignments.
 */
#define	_NPCM	11
#define	_NPCPV	336
struct pv_chunk {
	pmap_t			pc_pmap;
	TAILQ_ENTRY(pv_chunk)	pc_list;
	uint32_t		pc_map[_NPCM];	/* bitmap; 1 = free */
	TAILQ_ENTRY(pv_chunk)	pc_lru;
	struct pv_entry		pc_pventry[_NPCPV];
};

#ifdef _KERNEL
struct pcb;
extern ttb_entry_t pmap_kern_ttb; 	/* TTB for kernel pmap */

#define	pmap_page_get_memattr(m)	((vm_memattr_t)(m)->md.pat_mode)
#define	pmap_page_is_write_mapped(m)	(((m)->aflags & PGA_WRITEABLE) != 0)

/*
 * Only the following functions or macros may be used before pmap_bootstrap()
 * is called: pmap_kenter(), pmap_kextract(), pmap_kremove(), vtophys(), and
 * vtopte2().
 */
void pmap_bootstrap(vm_offset_t );
void pmap_kenter(vm_offset_t , vm_paddr_t );
void *pmap_kenter_temporary(vm_paddr_t , int );
void pmap_kremove(vm_offset_t);
void *pmap_mapdev(vm_paddr_t, vm_size_t);
void *pmap_mapdev_attr(vm_paddr_t, vm_size_t, int);
boolean_t pmap_page_is_mapped(vm_page_t );
void pmap_page_set_memattr(vm_page_t , vm_memattr_t );
void pmap_unmapdev(vm_offset_t, vm_size_t);
void pmap_kenter_device(vm_offset_t, vm_size_t, vm_paddr_t);
void pmap_kremove_device(vm_offset_t, vm_size_t);
void pmap_set_pcb_pagedir(pmap_t , struct pcb *);

void pmap_tlb_flush(pmap_t , vm_offset_t );
void pmap_tlb_flush_range(pmap_t , vm_offset_t , vm_size_t );
void pmap_tlb_flush_ng(pmap_t );

void pmap_dcache_wb_range(vm_paddr_t , vm_size_t , vm_memattr_t );

vm_paddr_t pmap_kextract(vm_offset_t );
int pmap_fault(pmap_t , vm_offset_t , uint32_t , int , bool);
#define	vtophys(va)	pmap_kextract((vm_offset_t)(va))

void pmap_set_tex(void);
void reinit_mmu(ttb_entry_t ttb, u_int aux_clr, u_int aux_set);

/*
 * Pre-bootstrap epoch functions set.
 */
void pmap_bootstrap_prepare(vm_paddr_t );
vm_paddr_t pmap_preboot_get_pages(u_int );
void pmap_preboot_map_pages(vm_paddr_t , vm_offset_t , u_int );
vm_offset_t pmap_preboot_reserve_pages(u_int );
vm_offset_t pmap_preboot_get_vpages(u_int );
void pmap_preboot_map_attr(vm_paddr_t , vm_offset_t , vm_size_t ,
	int , int );
static __inline void
pmap_map_chunk(vm_offset_t l1pt, vm_offset_t va, vm_offset_t pa,
    vm_size_t size, int prot, int cache)
{
	pmap_preboot_map_attr(pa, va, size, prot, cache);
}

/*
 * This structure is used by machine-dependent code to describe
 * static mappings of devices, created at bootstrap time.
 */
struct pmap_devmap {
	vm_offset_t	pd_va;		/* virtual address */
	vm_paddr_t	pd_pa;		/* physical address */
	vm_size_t	pd_size;	/* size of region */
	vm_prot_t	pd_prot;	/* protection code */
	int		pd_cache;	/* cache attributes */
};

void pmap_devmap_bootstrap(const struct pmap_devmap *);

#endif	/* _KERNEL */

// ----------------- TO BE DELETED ---------------------------------------------
#include <machine/pte-v6.h>

#ifdef _KERNEL

/*
 * sys/arm/arm/elf_trampoline.c
 * sys/arm/arm/genassym.c
 * sys/arm/arm/machdep.c
 * sys/arm/arm/mp_machdep.c
 * sys/arm/arm/locore.S
 * sys/arm/arm/pmap.c
 * sys/arm/arm/swtch.S
 * sys/arm/at91/at91_machdep.c
 * sys/arm/cavium/cns11xx/econa_machdep.c
 * sys/arm/s3c2xx0/s3c24x0_machdep.c
 * sys/arm/xscale/ixp425/avila_machdep.c
 * sys/arm/xscale/i8134x/crb_machdep.c
 * sys/arm/xscale/i80321/ep80219_machdep.c
 * sys/arm/xscale/i80321/iq31244_machdep.c
 * sys/arm/xscale/pxa/pxa_machdep.c
 */
#define	PMAP_DOMAIN_KERNEL	0	/* The kernel uses domain #0 */

/*
 * sys/arm/arm/cpufunc.c
 */
void pmap_pte_init_mmu_v6(void);
void vector_page_setprot(int);


/*
 * sys/arm/arm/db_interface.c
 * sys/arm/arm/machdep.c
 * sys/arm/arm/minidump_machdep.c
 * sys/arm/arm/pmap.c
 */
#define pmap_kernel() kernel_pmap

/*
 * sys/arm/arm/bus_space_generic.c (just comment)
 * sys/arm/arm/devmap.c
 * sys/arm/arm/pmap.c (just comment)
 * sys/arm/at91/at91_machdep.c
 * sys/arm/cavium/cns11xx/econa_machdep.c
 * sys/arm/freescale/imx/imx6_machdep.c (just comment)
 * sys/arm/mv/orion/db88f5xxx.c
 * sys/arm/mv/mv_localbus.c
 * sys/arm/mv/mv_machdep.c
 * sys/arm/mv/mv_pci.c
 * sys/arm/s3c2xx0/s3c24x0_machdep.c
 * sys/arm/versatile/versatile_machdep.c
 * sys/arm/xscale/ixp425/avila_machdep.c
 * sys/arm/xscale/i8134x/crb_machdep.c
 * sys/arm/xscale/i80321/ep80219_machdep.c
 * sys/arm/xscale/i80321/iq31244_machdep.c
 * sys/arm/xscale/pxa/pxa_machdep.c
 */
#define PTE_DEVICE	PTE2_ATTR_DEVICE



#endif	/* _KERNEL */
// -----------------------------------------------------------------------------

#endif	/* !_MACHINE_PMAP_H_ */
