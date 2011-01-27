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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *      from: hp300: @(#)pmap.h 7.2 (Berkeley) 12/16/90
 *      from: @(#)pmap.h        7.4 (Berkeley) 5/12/91
 * 	from: FreeBSD: src/sys/i386/include/pmap.h,v 1.70 2000/11/30
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_PMAP_H_
#define _MACHINE_PMAP_H_

#include <machine/pte.h>
#include <machine/cpuconf.h>
/*
 * Pte related macros
 */
#define PTE_NOCACHE	0
#define PTE_CACHE	1
#define PTE_PAGETABLE	2
 
#ifndef LOCORE

#include <sys/queue.h>
#include <sys/_lock.h>
#include <sys/_mutex.h>

#define PDESIZE		sizeof(pd_entry_t)	/* for assembly files */
#define PTESIZE		sizeof(pt_entry_t)	/* for assembly files */

#ifdef _KERNEL

#define vtophys(va)	pmap_extract(pmap_kernel(), (vm_offset_t)(va))
#define pmap_kextract(va)	pmap_extract(pmap_kernel(), (vm_offset_t)(va))

#endif

#define	pmap_page_get_memattr(m)	VM_MEMATTR_DEFAULT
#define	pmap_page_is_mapped(m)	(!TAILQ_EMPTY(&(m)->md.pv_list))
#define	pmap_page_set_memattr(m, ma)	(void)0

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

struct	pv_entry;

struct	md_page {
	int pvh_attrs;
	vm_offset_t pv_kva;		/* first kernel VA mapping */
	TAILQ_HEAD(,pv_entry)	pv_list;
};

#define	VM_MDPAGE_INIT(pg)						\
do {									\
	TAILQ_INIT(&pg->pv_list);					\
	mtx_init(&(pg)->md_page.pvh_mtx, "MDPAGE Mutex", NULL, MTX_DEV);\
	(pg)->mdpage.pvh_attrs = 0;					\
} while (/*CONSTCOND*/0)

struct l1_ttable;
struct l2_dtable;


/*
 * The number of L2 descriptor tables which can be tracked by an l2_dtable.
 * A bucket size of 16 provides for 16MB of contiguous virtual address
 * space per l2_dtable. Most processes will, therefore, require only two or
 * three of these to map their whole working set.
 */
#define	L2_BUCKET_LOG2	4
#define	L2_BUCKET_SIZE	(1 << L2_BUCKET_LOG2)
/*
 * Given the above "L2-descriptors-per-l2_dtable" constant, the number
 * of l2_dtable structures required to track all possible page descriptors
 * mappable by an L1 translation table is given by the following constants:
 */
#define	L2_LOG2		((32 - L1_S_SHIFT) - L2_BUCKET_LOG2)
#define	L2_SIZE		(1 << L2_LOG2)

struct	pmap {
	struct mtx		pm_mtx;
	u_int8_t		pm_domain;
	struct l1_ttable	*pm_l1;
	struct l2_dtable	*pm_l2[L2_SIZE];
	pd_entry_t		*pm_pdir;	/* KVA of page directory */
	uint32_t		pm_gen_count;	/* generation count (pmap lock dropped) */
	u_int			pm_retries;
	cpumask_t			pm_active;	/* active on cpus */
	struct pmap_statistics	pm_stats;	/* pmap statictics */
	TAILQ_HEAD(,pv_entry)	pm_pvlist;	/* list of mappings in pmap */
};

typedef struct pmap *pmap_t;

#ifdef _KERNEL
extern struct pmap	kernel_pmap_store;
#define kernel_pmap	(&kernel_pmap_store)
#define pmap_kernel() kernel_pmap

#define	PMAP_ASSERT_LOCKED(pmap) \
				mtx_assert(&(pmap)->pm_mtx, MA_OWNED)
#define	PMAP_LOCK(pmap)		mtx_lock(&(pmap)->pm_mtx)
#define	PMAP_LOCK_DESTROY(pmap)	mtx_destroy(&(pmap)->pm_mtx)
#define	PMAP_LOCK_INIT(pmap)	mtx_init(&(pmap)->pm_mtx, "pmap", \
				    NULL, MTX_DEF | MTX_DUPOK)
#define	PMAP_OWNED(pmap)	mtx_owned(&(pmap)->pm_mtx)
#define	PMAP_MTX(pmap)		(&(pmap)->pm_mtx)
#define	PMAP_TRYLOCK(pmap)	mtx_trylock(&(pmap)->pm_mtx)
#define	PMAP_UNLOCK(pmap)	mtx_unlock(&(pmap)->pm_mtx)
#endif


/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_list.
 */
typedef struct pv_entry {
	pmap_t          pv_pmap;        /* pmap where mapping lies */
	vm_offset_t     pv_va;          /* virtual address for mapping */
	TAILQ_ENTRY(pv_entry)   pv_list;
	TAILQ_ENTRY(pv_entry)	pv_plist;
	int		pv_flags;	/* flags (wired, etc...) */
} *pv_entry_t;

#ifdef _KERNEL

boolean_t pmap_get_pde_pte(pmap_t, vm_offset_t, pd_entry_t **, pt_entry_t **);

/*
 * virtual address to page table entry and
 * to physical address. Likewise for alternate address space.
 * Note: these work recursively, thus vtopte of a pte will give
 * the corresponding pde that in turn maps it.
 */

/*
 * The current top of kernel VM.
 */
extern vm_offset_t pmap_curmaxkvaddr;

struct pcb;

void	pmap_set_pcb_pagedir(pmap_t, struct pcb *);
/* Virtual address to page table entry */
static __inline pt_entry_t *
vtopte(vm_offset_t va)
{
	pd_entry_t *pdep;
	pt_entry_t *ptep;

	if (pmap_get_pde_pte(pmap_kernel(), va, &pdep, &ptep) == FALSE)
		return (NULL);
	return (ptep);
}

extern vm_offset_t phys_avail[];
extern vm_offset_t virtual_avail;
extern vm_offset_t virtual_end;

void	pmap_bootstrap(vm_offset_t, vm_offset_t, struct pv_addr *);
void	pmap_kenter(vm_offset_t va, vm_paddr_t pa);
void	pmap_kenter_nocache(vm_offset_t va, vm_paddr_t pa);
void	*pmap_kenter_temp(vm_paddr_t pa, int i);
void 	pmap_kenter_user(vm_offset_t va, vm_paddr_t pa);
void	pmap_kremove(vm_offset_t);
void	*pmap_mapdev(vm_offset_t, vm_size_t);
void	pmap_unmapdev(vm_offset_t, vm_size_t);
vm_page_t	pmap_use_pt(pmap_t, vm_offset_t);
void	pmap_debug(int);
void	pmap_map_section(vm_offset_t, vm_offset_t, vm_offset_t, int, int);
void	pmap_link_l2pt(vm_offset_t, vm_offset_t, struct pv_addr *);
vm_size_t	pmap_map_chunk(vm_offset_t, vm_offset_t, vm_offset_t, vm_size_t, int, int);
void
pmap_map_entry(vm_offset_t l1pt, vm_offset_t va, vm_offset_t pa, int prot,
    int cache);
int pmap_fault_fixup(pmap_t, vm_offset_t, vm_prot_t, int);

/*
 * Definitions for MMU domains
 */
#define	PMAP_DOMAINS		15	/* 15 'user' domains (1-15) */
#define	PMAP_DOMAIN_KERNEL	0	/* The kernel uses domain #0 */

/*
 * The new pmap ensures that page-tables are always mapping Write-Thru.
 * Thus, on some platforms we can run fast and loose and avoid syncing PTEs
 * on every change.
 *
 * Unfortunately, not all CPUs have a write-through cache mode.  So we
 * define PMAP_NEEDS_PTE_SYNC for C code to conditionally do PTE syncs,
 * and if there is the chance for PTE syncs to be needed, we define
 * PMAP_INCLUDE_PTE_SYNC so e.g. assembly code can include (and run)
 * the code.
 */
extern int pmap_needs_pte_sync;

/*
 * These macros define the various bit masks in the PTE.
 *
 * We use these macros since we use different bits on different processor
 * models.
 */
#define	L1_S_PROT_U		(L1_S_AP(AP_U))
#define	L1_S_PROT_W		(L1_S_AP(AP_W))
#define	L1_S_PROT_MASK		(L1_S_PROT_U|L1_S_PROT_W)

#define	L1_S_CACHE_MASK_generic	(L1_S_B|L1_S_C)
#define	L1_S_CACHE_MASK_xscale	(L1_S_B|L1_S_C|L1_S_XSCALE_TEX(TEX_XSCALE_X)|\
    				L1_S_XSCALE_TEX(TEX_XSCALE_T))

#define	L2_L_PROT_U		(L2_AP(AP_U))
#define	L2_L_PROT_W		(L2_AP(AP_W))
#define	L2_L_PROT_MASK		(L2_L_PROT_U|L2_L_PROT_W)

#define	L2_L_CACHE_MASK_generic	(L2_B|L2_C)
#define	L2_L_CACHE_MASK_xscale	(L2_B|L2_C|L2_XSCALE_L_TEX(TEX_XSCALE_X) | \
    				L2_XSCALE_L_TEX(TEX_XSCALE_T))

#define	L2_S_PROT_U_generic	(L2_AP(AP_U))
#define	L2_S_PROT_W_generic	(L2_AP(AP_W))
#define	L2_S_PROT_MASK_generic	(L2_S_PROT_U|L2_S_PROT_W)

#define	L2_S_PROT_U_xscale	(L2_AP0(AP_U))
#define	L2_S_PROT_W_xscale	(L2_AP0(AP_W))
#define	L2_S_PROT_MASK_xscale	(L2_S_PROT_U|L2_S_PROT_W)

#define	L2_S_CACHE_MASK_generic	(L2_B|L2_C)
#define	L2_S_CACHE_MASK_xscale	(L2_B|L2_C|L2_XSCALE_T_TEX(TEX_XSCALE_X)| \
    				 L2_XSCALE_T_TEX(TEX_XSCALE_X))

#define	L1_S_PROTO_generic	(L1_TYPE_S | L1_S_IMP)
#define	L1_S_PROTO_xscale	(L1_TYPE_S)

#define	L1_C_PROTO_generic	(L1_TYPE_C | L1_C_IMP2)
#define	L1_C_PROTO_xscale	(L1_TYPE_C)

#define	L2_L_PROTO		(L2_TYPE_L)

#define	L2_S_PROTO_generic	(L2_TYPE_S)
#define	L2_S_PROTO_xscale	(L2_TYPE_XSCALE_XS)

/*
 * User-visible names for the ones that vary with MMU class.
 */

#if ARM_NMMUS > 1
/* More than one MMU class configured; use variables. */
#define	L2_S_PROT_U		pte_l2_s_prot_u
#define	L2_S_PROT_W		pte_l2_s_prot_w
#define	L2_S_PROT_MASK		pte_l2_s_prot_mask

#define	L1_S_CACHE_MASK		pte_l1_s_cache_mask
#define	L2_L_CACHE_MASK		pte_l2_l_cache_mask
#define	L2_S_CACHE_MASK		pte_l2_s_cache_mask

#define	L1_S_PROTO		pte_l1_s_proto
#define	L1_C_PROTO		pte_l1_c_proto
#define	L2_S_PROTO		pte_l2_s_proto

#elif (ARM_MMU_GENERIC + ARM_MMU_SA1) != 0
#define	L2_S_PROT_U		L2_S_PROT_U_generic
#define	L2_S_PROT_W		L2_S_PROT_W_generic
#define	L2_S_PROT_MASK		L2_S_PROT_MASK_generic

#define	L1_S_CACHE_MASK		L1_S_CACHE_MASK_generic
#define	L2_L_CACHE_MASK		L2_L_CACHE_MASK_generic
#define	L2_S_CACHE_MASK		L2_S_CACHE_MASK_generic

#define	L1_S_PROTO		L1_S_PROTO_generic
#define	L1_C_PROTO		L1_C_PROTO_generic
#define	L2_S_PROTO		L2_S_PROTO_generic

#elif ARM_MMU_XSCALE == 1
#define	L2_S_PROT_U		L2_S_PROT_U_xscale
#define	L2_S_PROT_W		L2_S_PROT_W_xscale
#define	L2_S_PROT_MASK		L2_S_PROT_MASK_xscale

#define	L1_S_CACHE_MASK		L1_S_CACHE_MASK_xscale
#define	L2_L_CACHE_MASK		L2_L_CACHE_MASK_xscale
#define	L2_S_CACHE_MASK		L2_S_CACHE_MASK_xscale

#define	L1_S_PROTO		L1_S_PROTO_xscale
#define	L1_C_PROTO		L1_C_PROTO_xscale
#define	L2_S_PROTO		L2_S_PROTO_xscale

#endif /* ARM_NMMUS > 1 */

#if (ARM_MMU_SA1 == 1) && (ARM_NMMUS == 1)
#define	PMAP_NEEDS_PTE_SYNC	1
#define	PMAP_INCLUDE_PTE_SYNC
#elif defined(CPU_XSCALE_81342)
#define PMAP_NEEDS_PTE_SYNC	1
#define PMAP_INCLUDE_PTE_SYNC
#elif (ARM_MMU_SA1 == 0)
#define	PMAP_NEEDS_PTE_SYNC	0
#endif

/*
 * These macros return various bits based on kernel/user and protection.
 * Note that the compiler will usually fold these at compile time.
 */
#define	L1_S_PROT(ku, pr)	((((ku) == PTE_USER) ? L1_S_PROT_U : 0) | \
				 (((pr) & VM_PROT_WRITE) ? L1_S_PROT_W : 0))

#define	L2_L_PROT(ku, pr)	((((ku) == PTE_USER) ? L2_L_PROT_U : 0) | \
				 (((pr) & VM_PROT_WRITE) ? L2_L_PROT_W : 0))

#define	L2_S_PROT(ku, pr)	((((ku) == PTE_USER) ? L2_S_PROT_U : 0) | \
				 (((pr) & VM_PROT_WRITE) ? L2_S_PROT_W : 0))

/*
 * Macros to test if a mapping is mappable with an L1 Section mapping
 * or an L2 Large Page mapping.
 */
#define	L1_S_MAPPABLE_P(va, pa, size)					\
	((((va) | (pa)) & L1_S_OFFSET) == 0 && (size) >= L1_S_SIZE)

#define	L2_L_MAPPABLE_P(va, pa, size)					\
	((((va) | (pa)) & L2_L_OFFSET) == 0 && (size) >= L2_L_SIZE)

/*
 * Provide a fallback in case we were not able to determine it at
 * compile-time.
 */
#ifndef PMAP_NEEDS_PTE_SYNC
#define	PMAP_NEEDS_PTE_SYNC	pmap_needs_pte_sync
#define	PMAP_INCLUDE_PTE_SYNC
#endif

#define	PTE_SYNC(pte)							\
do {									\
	if (PMAP_NEEDS_PTE_SYNC) {					\
		cpu_dcache_wb_range((vm_offset_t)(pte), sizeof(pt_entry_t));\
		cpu_l2cache_wb_range((vm_offset_t)(pte), sizeof(pt_entry_t));\
	}\
} while (/*CONSTCOND*/0)

#define	PTE_SYNC_RANGE(pte, cnt)					\
do {									\
	if (PMAP_NEEDS_PTE_SYNC) {					\
		cpu_dcache_wb_range((vm_offset_t)(pte),			\
		    (cnt) << 2); /* * sizeof(pt_entry_t) */		\
		cpu_l2cache_wb_range((vm_offset_t)(pte), 		\
		    (cnt) << 2); /* * sizeof(pt_entry_t) */		\
	}								\
} while (/*CONSTCOND*/0)

extern pt_entry_t		pte_l1_s_cache_mode;
extern pt_entry_t		pte_l1_s_cache_mask;

extern pt_entry_t		pte_l2_l_cache_mode;
extern pt_entry_t		pte_l2_l_cache_mask;

extern pt_entry_t		pte_l2_s_cache_mode;
extern pt_entry_t		pte_l2_s_cache_mask;

extern pt_entry_t		pte_l1_s_cache_mode_pt;
extern pt_entry_t		pte_l2_l_cache_mode_pt;
extern pt_entry_t		pte_l2_s_cache_mode_pt;

extern pt_entry_t		pte_l2_s_prot_u;
extern pt_entry_t		pte_l2_s_prot_w;
extern pt_entry_t		pte_l2_s_prot_mask;
 
extern pt_entry_t		pte_l1_s_proto;
extern pt_entry_t		pte_l1_c_proto;
extern pt_entry_t		pte_l2_s_proto;

extern void (*pmap_copy_page_func)(vm_paddr_t, vm_paddr_t);
extern void (*pmap_zero_page_func)(vm_paddr_t, int, int);

#if (ARM_MMU_GENERIC + ARM_MMU_SA1) != 0 || defined(CPU_XSCALE_81342)
void	pmap_copy_page_generic(vm_paddr_t, vm_paddr_t);
void	pmap_zero_page_generic(vm_paddr_t, int, int);

void	pmap_pte_init_generic(void);
#if defined(CPU_ARM8)
void	pmap_pte_init_arm8(void);
#endif
#if defined(CPU_ARM9)
void	pmap_pte_init_arm9(void);
#endif /* CPU_ARM9 */
#if defined(CPU_ARM10)
void	pmap_pte_init_arm10(void);
#endif /* CPU_ARM10 */
#endif /* (ARM_MMU_GENERIC + ARM_MMU_SA1) != 0 */

#if /* ARM_MMU_SA1 == */1
void	pmap_pte_init_sa1(void);
#endif /* ARM_MMU_SA1 == 1 */

#if ARM_MMU_XSCALE == 1
void	pmap_copy_page_xscale(vm_paddr_t, vm_paddr_t);
void	pmap_zero_page_xscale(vm_paddr_t, int, int);

void	pmap_pte_init_xscale(void);

void	xscale_setup_minidata(vm_offset_t, vm_offset_t, vm_offset_t);

void	pmap_use_minicache(vm_offset_t, vm_size_t);
#endif /* ARM_MMU_XSCALE == 1 */
#if defined(CPU_XSCALE_81342)
#define ARM_HAVE_SUPERSECTIONS
#endif

#define PTE_KERNEL	0
#define PTE_USER	1
#define	l1pte_valid(pde)	((pde) != 0)
#define	l1pte_section_p(pde)	(((pde) & L1_TYPE_MASK) == L1_TYPE_S)
#define	l1pte_page_p(pde)	(((pde) & L1_TYPE_MASK) == L1_TYPE_C)
#define	l1pte_fpage_p(pde)	(((pde) & L1_TYPE_MASK) == L1_TYPE_F)

#define l2pte_index(v)		(((v) & L2_ADDR_BITS) >> L2_S_SHIFT)
#define	l2pte_valid(pte)	((pte) != 0)
#define	l2pte_pa(pte)		((pte) & L2_S_FRAME)
#define l2pte_minidata(pte)	(((pte) & \
				 (L2_B | L2_C | L2_XSCALE_T_TEX(TEX_XSCALE_X)))\
				 == (L2_C | L2_XSCALE_T_TEX(TEX_XSCALE_X)))

/* L1 and L2 page table macros */
#define pmap_pde_v(pde)		l1pte_valid(*(pde))
#define pmap_pde_section(pde)	l1pte_section_p(*(pde))
#define pmap_pde_page(pde)	l1pte_page_p(*(pde))
#define pmap_pde_fpage(pde)	l1pte_fpage_p(*(pde))

#define	pmap_pte_v(pte)		l2pte_valid(*(pte))
#define	pmap_pte_pa(pte)	l2pte_pa(*(pte))

/*
 * Flags that indicate attributes of pages or mappings of pages.
 *
 * The PVF_MOD and PVF_REF flags are stored in the mdpage for each
 * page.  PVF_WIRED, PVF_WRITE, and PVF_NC are kept in individual
 * pv_entry's for each page.  They live in the same "namespace" so
 * that we can clear multiple attributes at a time.
 *
 * Note the "non-cacheable" flag generally means the page has
 * multiple mappings in a given address space.
 */
#define	PVF_MOD		0x01		/* page is modified */
#define	PVF_REF		0x02		/* page is referenced */
#define	PVF_WIRED	0x04		/* mapping is wired */
#define	PVF_WRITE	0x08		/* mapping is writable */
#define	PVF_EXEC	0x10		/* mapping is executable */
#define	PVF_NC		0x20		/* mapping is non-cacheable */
#define	PVF_MWC		0x40		/* mapping is used multiple times in userland */
#define	PVF_UNMAN	0x80		/* mapping is unmanaged */

void vector_page_setprot(int);

void pmap_update(pmap_t);

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

const struct pmap_devmap *pmap_devmap_find_pa(vm_paddr_t, vm_size_t);
const struct pmap_devmap *pmap_devmap_find_va(vm_offset_t, vm_size_t);

void	pmap_devmap_bootstrap(vm_offset_t, const struct pmap_devmap *);
void	pmap_devmap_register(const struct pmap_devmap *);

#define SECTION_CACHE	0x1
#define SECTION_PT	0x2
void	pmap_kenter_section(vm_offset_t, vm_paddr_t, int flags);
#ifdef ARM_HAVE_SUPERSECTIONS
void	pmap_kenter_supersection(vm_offset_t, uint64_t, int flags);
#endif

extern char *_tmppt;

void	pmap_postinit(void);

#ifdef ARM_USE_SMALL_ALLOC
void	arm_add_smallalloc_pages(void *, void *, int, int);
vm_offset_t arm_ptovirt(vm_paddr_t);
void arm_init_smallalloc(void);
struct arm_small_page {
	void *addr;
	TAILQ_ENTRY(arm_small_page) pg_list;
};

#endif

#define ARM_NOCACHE_KVA_SIZE 0x1000000
extern vm_offset_t arm_nocache_startaddr;
void *arm_remap_nocache(void *, vm_size_t);
void arm_unmap_nocache(void *, vm_size_t);

extern vm_paddr_t dump_avail[];
#endif	/* _KERNEL */

#endif	/* !LOCORE */

#endif	/* !_MACHINE_PMAP_H_ */
