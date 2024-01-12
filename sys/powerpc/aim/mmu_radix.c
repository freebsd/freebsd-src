/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Matthew Macy
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/bitstring.h>
#include <sys/queue.h>
#include <sys/cpuset.h>
#include <sys/endian.h>
#include <sys/kerneldump.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/syslog.h>
#include <sys/msgbuf.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/vmem.h>
#include <sys/vmmeter.h>
#include <sys/smp.h>

#include <sys/kdb.h>

#include <dev/ofw/openfirm.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_phys.h>
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>
#include <vm/vm_dumpset.h>
#include <vm/uma.h>

#include <machine/_inttypes.h>
#include <machine/cpu.h>
#include <machine/platform.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/psl.h>
#include <machine/bat.h>
#include <machine/hid.h>
#include <machine/pte.h>
#include <machine/sr.h>
#include <machine/trap.h>
#include <machine/mmuvar.h>

/* For pseries bit. */
#include <powerpc/pseries/phyp-hvcall.h>

#ifdef INVARIANTS
#include <vm/uma_dbg.h>
#endif

#define PPC_BITLSHIFT(bit)	(sizeof(long)*NBBY - 1 - (bit))
#define PPC_BIT(bit)		(1UL << PPC_BITLSHIFT(bit))
#define PPC_BITLSHIFT_VAL(val, bit) ((val) << PPC_BITLSHIFT(bit))

#include "opt_ddb.h"

#ifdef DDB
static void pmap_pte_walk(pml1_entry_t *l1, vm_offset_t va);
#endif

#define PG_W	RPTE_WIRED
#define PG_V	RPTE_VALID
#define PG_MANAGED	RPTE_MANAGED
#define PG_PROMOTED	RPTE_PROMOTED
#define PG_M	RPTE_C
#define PG_A	RPTE_R
#define PG_X	RPTE_EAA_X
#define PG_RW	RPTE_EAA_W
#define PG_PTE_CACHE RPTE_ATTR_MASK

#define RPTE_SHIFT 9
#define NLS_MASK ((1UL<<5)-1)
#define RPTE_ENTRIES (1UL<<RPTE_SHIFT)
#define RPTE_MASK (RPTE_ENTRIES-1)

#define NLB_SHIFT 0
#define NLB_MASK (((1UL<<52)-1) << 8)

extern int nkpt;
extern caddr_t crashdumpmap;

#define RIC_FLUSH_TLB 0
#define RIC_FLUSH_PWC 1
#define RIC_FLUSH_ALL 2

#define POWER9_TLB_SETS_RADIX	128	/* # sets in POWER9 TLB Radix mode */

#define PPC_INST_TLBIE			0x7c000264
#define PPC_INST_TLBIEL			0x7c000224
#define PPC_INST_SLBIA			0x7c0003e4

#define ___PPC_RA(a)	(((a) & 0x1f) << 16)
#define ___PPC_RB(b)	(((b) & 0x1f) << 11)
#define ___PPC_RS(s)	(((s) & 0x1f) << 21)
#define ___PPC_RT(t)	___PPC_RS(t)
#define ___PPC_R(r)	(((r) & 0x1) << 16)
#define ___PPC_PRS(prs)	(((prs) & 0x1) << 17)
#define ___PPC_RIC(ric)	(((ric) & 0x3) << 18)

#define PPC_SLBIA(IH)	__XSTRING(.long PPC_INST_SLBIA | \
				       ((IH & 0x7) << 21))
#define	PPC_TLBIE_5(rb,rs,ric,prs,r)				\
	__XSTRING(.long PPC_INST_TLBIE |			\
			  ___PPC_RB(rb) | ___PPC_RS(rs) |	\
			  ___PPC_RIC(ric) | ___PPC_PRS(prs) |	\
			  ___PPC_R(r))

#define	PPC_TLBIEL(rb,rs,ric,prs,r) \
	 __XSTRING(.long PPC_INST_TLBIEL | \
			   ___PPC_RB(rb) | ___PPC_RS(rs) |	\
			   ___PPC_RIC(ric) | ___PPC_PRS(prs) |	\
			   ___PPC_R(r))

#define PPC_INVALIDATE_ERAT		PPC_SLBIA(7)

static __inline void
ttusync(void)
{
	__asm __volatile("eieio; tlbsync; ptesync" ::: "memory");
}

#define TLBIEL_INVAL_SEL_MASK	0xc00	/* invalidation selector */
#define  TLBIEL_INVAL_PAGE	0x000	/* invalidate a single page */
#define  TLBIEL_INVAL_SET_PID	0x400	/* invalidate a set for the current PID */
#define  TLBIEL_INVAL_SET_LPID	0x800	/* invalidate a set for current LPID */
#define  TLBIEL_INVAL_SET	0xc00	/* invalidate a set for all LPIDs */

#define TLBIE_ACTUAL_PAGE_MASK		0xe0
#define  TLBIE_ACTUAL_PAGE_4K		0x00
#define  TLBIE_ACTUAL_PAGE_64K		0xa0
#define  TLBIE_ACTUAL_PAGE_2M		0x20
#define  TLBIE_ACTUAL_PAGE_1G		0x40

#define TLBIE_PRS_PARTITION_SCOPE	0x0
#define TLBIE_PRS_PROCESS_SCOPE	0x1

#define TLBIE_RIC_INVALIDATE_TLB	0x0	/* Invalidate just TLB */
#define TLBIE_RIC_INVALIDATE_PWC	0x1	/* Invalidate just PWC */
#define TLBIE_RIC_INVALIDATE_ALL	0x2	/* Invalidate TLB, PWC,
						 * cached {proc, part}tab entries
						 */
#define TLBIE_RIC_INVALIDATE_SEQ	0x3	/* HPT - only:
						 * Invalidate a range of translations
						 */

static __always_inline void
radix_tlbie(uint8_t ric, uint8_t prs, uint16_t is, uint32_t pid, uint32_t lpid,
			vm_offset_t va, uint16_t ap)
{
	uint64_t rb, rs;

	MPASS((va & PAGE_MASK) == 0);

	rs = ((uint64_t)pid << 32) | lpid;
	rb = va | is | ap;
	__asm __volatile(PPC_TLBIE_5(%0, %1, %2, %3, 1) : :
		"r" (rb), "r" (rs), "i" (ric), "i" (prs) : "memory");
}

static __inline void
radix_tlbie_fixup(uint32_t pid, vm_offset_t va, int ap)
{

	__asm __volatile("ptesync" ::: "memory");
	radix_tlbie(TLBIE_RIC_INVALIDATE_TLB, TLBIE_PRS_PROCESS_SCOPE,
	    TLBIEL_INVAL_PAGE, 0, 0, va, ap);
	__asm __volatile("ptesync" ::: "memory");
	radix_tlbie(TLBIE_RIC_INVALIDATE_TLB, TLBIE_PRS_PROCESS_SCOPE,
	    TLBIEL_INVAL_PAGE, pid, 0, va, ap);
}

static __inline void
radix_tlbie_invlpg_user_4k(uint32_t pid, vm_offset_t va)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_TLB, TLBIE_PRS_PROCESS_SCOPE,
		TLBIEL_INVAL_PAGE, pid, 0, va, TLBIE_ACTUAL_PAGE_4K);
	radix_tlbie_fixup(pid, va, TLBIE_ACTUAL_PAGE_4K);
}

static __inline void
radix_tlbie_invlpg_user_2m(uint32_t pid, vm_offset_t va)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_TLB, TLBIE_PRS_PROCESS_SCOPE,
		TLBIEL_INVAL_PAGE, pid, 0, va, TLBIE_ACTUAL_PAGE_2M);
	radix_tlbie_fixup(pid, va, TLBIE_ACTUAL_PAGE_2M);
}

static __inline void
radix_tlbie_invlpwc_user(uint32_t pid)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_PWC, TLBIE_PRS_PROCESS_SCOPE,
		TLBIEL_INVAL_SET_PID, pid, 0, 0, 0);
}

static __inline void
radix_tlbie_flush_user(uint32_t pid)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_ALL, TLBIE_PRS_PROCESS_SCOPE,
		TLBIEL_INVAL_SET_PID, pid, 0, 0, 0);
}

static __inline void
radix_tlbie_invlpg_kernel_4k(vm_offset_t va)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_TLB, TLBIE_PRS_PROCESS_SCOPE,
	    TLBIEL_INVAL_PAGE, 0, 0, va, TLBIE_ACTUAL_PAGE_4K);
	radix_tlbie_fixup(0, va, TLBIE_ACTUAL_PAGE_4K);
}

static __inline void
radix_tlbie_invlpg_kernel_2m(vm_offset_t va)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_TLB, TLBIE_PRS_PROCESS_SCOPE,
	    TLBIEL_INVAL_PAGE, 0, 0, va, TLBIE_ACTUAL_PAGE_2M);
	radix_tlbie_fixup(0, va, TLBIE_ACTUAL_PAGE_2M);
}

/* 1GB pages aren't currently supported. */
static __inline __unused void
radix_tlbie_invlpg_kernel_1g(vm_offset_t va)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_TLB, TLBIE_PRS_PROCESS_SCOPE,
	    TLBIEL_INVAL_PAGE, 0, 0, va, TLBIE_ACTUAL_PAGE_1G);
	radix_tlbie_fixup(0, va, TLBIE_ACTUAL_PAGE_1G);
}

static __inline void
radix_tlbie_invlpwc_kernel(void)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_PWC, TLBIE_PRS_PROCESS_SCOPE,
	    TLBIEL_INVAL_SET_LPID, 0, 0, 0, 0);
}

static __inline void
radix_tlbie_flush_kernel(void)
{

	radix_tlbie(TLBIE_RIC_INVALIDATE_ALL, TLBIE_PRS_PROCESS_SCOPE,
	    TLBIEL_INVAL_SET_LPID, 0, 0, 0, 0);
}

static __inline vm_pindex_t
pmap_l3e_pindex(vm_offset_t va)
{
	return ((va & PG_FRAME) >> L3_PAGE_SIZE_SHIFT);
}

static __inline vm_pindex_t
pmap_pml3e_index(vm_offset_t va)
{

	return ((va >> L3_PAGE_SIZE_SHIFT) & RPTE_MASK);
}

static __inline vm_pindex_t
pmap_pml2e_index(vm_offset_t va)
{
	return ((va >> L2_PAGE_SIZE_SHIFT) & RPTE_MASK);
}

static __inline vm_pindex_t
pmap_pml1e_index(vm_offset_t va)
{
	return ((va & PG_FRAME) >> L1_PAGE_SIZE_SHIFT);
}

/* Return various clipped indexes for a given VA */
static __inline vm_pindex_t
pmap_pte_index(vm_offset_t va)
{

	return ((va >> PAGE_SHIFT) & RPTE_MASK);
}

/* Return a pointer to the PT slot that corresponds to a VA */
static __inline pt_entry_t *
pmap_l3e_to_pte(pt_entry_t *l3e, vm_offset_t va)
{
	pt_entry_t *pte;
	vm_paddr_t ptepa;

	ptepa = (be64toh(*l3e) & NLB_MASK);
	pte = (pt_entry_t *)PHYS_TO_DMAP(ptepa);
	return (&pte[pmap_pte_index(va)]);
}

/* Return a pointer to the PD slot that corresponds to a VA */
static __inline pt_entry_t *
pmap_l2e_to_l3e(pt_entry_t *l2e, vm_offset_t va)
{
	pt_entry_t *l3e;
	vm_paddr_t l3pa;

	l3pa = (be64toh(*l2e) & NLB_MASK);
	l3e = (pml3_entry_t *)PHYS_TO_DMAP(l3pa);
	return (&l3e[pmap_pml3e_index(va)]);
}

/* Return a pointer to the PD slot that corresponds to a VA */
static __inline pt_entry_t *
pmap_l1e_to_l2e(pt_entry_t *l1e, vm_offset_t va)
{
	pt_entry_t *l2e;
	vm_paddr_t l2pa;

	l2pa = (be64toh(*l1e) & NLB_MASK);

	l2e = (pml2_entry_t *)PHYS_TO_DMAP(l2pa);
	return (&l2e[pmap_pml2e_index(va)]);
}

static __inline pml1_entry_t *
pmap_pml1e(pmap_t pmap, vm_offset_t va)
{

	return (&pmap->pm_pml1[pmap_pml1e_index(va)]);
}

static pt_entry_t *
pmap_pml2e(pmap_t pmap, vm_offset_t va)
{
	pt_entry_t *l1e;

	l1e = pmap_pml1e(pmap, va);
	if (l1e == NULL || (be64toh(*l1e) & RPTE_VALID) == 0)
		return (NULL);
	return (pmap_l1e_to_l2e(l1e, va));
}

static __inline pt_entry_t *
pmap_pml3e(pmap_t pmap, vm_offset_t va)
{
	pt_entry_t *l2e;

	l2e = pmap_pml2e(pmap, va);
	if (l2e == NULL || (be64toh(*l2e) & RPTE_VALID) == 0)
		return (NULL);
	return (pmap_l2e_to_l3e(l2e, va));
}

static __inline pt_entry_t *
pmap_pte(pmap_t pmap, vm_offset_t va)
{
	pt_entry_t *l3e;

	l3e = pmap_pml3e(pmap, va);
	if (l3e == NULL || (be64toh(*l3e) & RPTE_VALID) == 0)
		return (NULL);
	return (pmap_l3e_to_pte(l3e, va));
}

int nkpt = 64;
SYSCTL_INT(_machdep, OID_AUTO, nkpt, CTLFLAG_RD, &nkpt, 0,
    "Number of kernel page table pages allocated on bootup");

vm_paddr_t dmaplimit;

SYSCTL_DECL(_vm_pmap);

#ifdef INVARIANTS
#define VERBOSE_PMAP 0
#define VERBOSE_PROTECT 0
static int pmap_logging;
SYSCTL_INT(_vm_pmap, OID_AUTO, pmap_logging, CTLFLAG_RWTUN,
    &pmap_logging, 0, "verbose debug logging");
#endif

static u_int64_t	KPTphys;	/* phys addr of kernel level 1 */

//static vm_paddr_t	KERNend;	/* phys addr of end of bootstrap data */

static vm_offset_t qframe = 0;
static struct mtx qframe_mtx;

void mmu_radix_activate(struct thread *);
void mmu_radix_advise(pmap_t, vm_offset_t, vm_offset_t, int);
void mmu_radix_align_superpage(vm_object_t, vm_ooffset_t, vm_offset_t *,
    vm_size_t);
void mmu_radix_clear_modify(vm_page_t);
void mmu_radix_copy(pmap_t, pmap_t, vm_offset_t, vm_size_t, vm_offset_t);
int mmu_radix_decode_kernel_ptr(vm_offset_t, int *, vm_offset_t *);
int mmu_radix_enter(pmap_t, vm_offset_t, vm_page_t, vm_prot_t, u_int, int8_t);
void mmu_radix_enter_object(pmap_t, vm_offset_t, vm_offset_t, vm_page_t,
	vm_prot_t);
void mmu_radix_enter_quick(pmap_t, vm_offset_t, vm_page_t, vm_prot_t);
vm_paddr_t mmu_radix_extract(pmap_t pmap, vm_offset_t va);
vm_page_t mmu_radix_extract_and_hold(pmap_t, vm_offset_t, vm_prot_t);
void mmu_radix_kenter(vm_offset_t, vm_paddr_t);
vm_paddr_t mmu_radix_kextract(vm_offset_t);
void mmu_radix_kremove(vm_offset_t);
boolean_t mmu_radix_is_modified(vm_page_t);
boolean_t mmu_radix_is_prefaultable(pmap_t, vm_offset_t);
boolean_t mmu_radix_is_referenced(vm_page_t);
void mmu_radix_object_init_pt(pmap_t, vm_offset_t, vm_object_t,
	vm_pindex_t, vm_size_t);
boolean_t mmu_radix_page_exists_quick(pmap_t, vm_page_t);
void mmu_radix_page_init(vm_page_t);
boolean_t mmu_radix_page_is_mapped(vm_page_t m);
void mmu_radix_page_set_memattr(vm_page_t, vm_memattr_t);
int mmu_radix_page_wired_mappings(vm_page_t);
int mmu_radix_pinit(pmap_t);
void mmu_radix_protect(pmap_t, vm_offset_t, vm_offset_t, vm_prot_t);
bool mmu_radix_ps_enabled(pmap_t);
void mmu_radix_qenter(vm_offset_t, vm_page_t *, int);
void mmu_radix_qremove(vm_offset_t, int);
vm_offset_t mmu_radix_quick_enter_page(vm_page_t);
void mmu_radix_quick_remove_page(vm_offset_t);
int mmu_radix_ts_referenced(vm_page_t);
void mmu_radix_release(pmap_t);
void mmu_radix_remove(pmap_t, vm_offset_t, vm_offset_t);
void mmu_radix_remove_all(vm_page_t);
void mmu_radix_remove_pages(pmap_t);
void mmu_radix_remove_write(vm_page_t);
void mmu_radix_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz);
void mmu_radix_unwire(pmap_t, vm_offset_t, vm_offset_t);
void mmu_radix_zero_page(vm_page_t);
void mmu_radix_zero_page_area(vm_page_t, int, int);
int mmu_radix_change_attr(vm_offset_t, vm_size_t, vm_memattr_t);
void mmu_radix_page_array_startup(long pages);

#include "mmu_oea64.h"

/*
 * Kernel MMU interface
 */

static void	mmu_radix_bootstrap(vm_offset_t, vm_offset_t);

static void mmu_radix_copy_page(vm_page_t, vm_page_t);
static void mmu_radix_copy_pages(vm_page_t *ma, vm_offset_t a_offset,
    vm_page_t *mb, vm_offset_t b_offset, int xfersize);
static void mmu_radix_growkernel(vm_offset_t);
static void mmu_radix_init(void);
static int mmu_radix_mincore(pmap_t, vm_offset_t, vm_paddr_t *);
static vm_offset_t mmu_radix_map(vm_offset_t *, vm_paddr_t, vm_paddr_t, int);
static void mmu_radix_pinit0(pmap_t);

static void *mmu_radix_mapdev(vm_paddr_t, vm_size_t);
static void *mmu_radix_mapdev_attr(vm_paddr_t, vm_size_t, vm_memattr_t);
static void mmu_radix_unmapdev(void *, vm_size_t);
static void mmu_radix_kenter_attr(vm_offset_t, vm_paddr_t, vm_memattr_t ma);
static int mmu_radix_dev_direct_mapped(vm_paddr_t, vm_size_t);
static void mmu_radix_dumpsys_map(vm_paddr_t pa, size_t sz, void **va);
static void mmu_radix_scan_init(void);
static void	mmu_radix_cpu_bootstrap(int ap);
static void	mmu_radix_tlbie_all(void);

static struct pmap_funcs mmu_radix_methods = {
	.bootstrap = mmu_radix_bootstrap,
	.copy_page = mmu_radix_copy_page,
	.copy_pages = mmu_radix_copy_pages,
	.cpu_bootstrap = mmu_radix_cpu_bootstrap,
	.growkernel = mmu_radix_growkernel,
	.init = mmu_radix_init,
	.map =      		mmu_radix_map,
	.mincore =      	mmu_radix_mincore,
	.pinit = mmu_radix_pinit,
	.pinit0 = mmu_radix_pinit0,

	.mapdev = mmu_radix_mapdev,
	.mapdev_attr = mmu_radix_mapdev_attr,
	.unmapdev = mmu_radix_unmapdev,
	.kenter_attr = mmu_radix_kenter_attr,
	.dev_direct_mapped = mmu_radix_dev_direct_mapped,
	.dumpsys_pa_init = mmu_radix_scan_init,
	.dumpsys_map_chunk = mmu_radix_dumpsys_map,
	.page_is_mapped = mmu_radix_page_is_mapped,
	.ps_enabled = mmu_radix_ps_enabled,
	.align_superpage = mmu_radix_align_superpage,
	.object_init_pt = mmu_radix_object_init_pt,
	.protect = mmu_radix_protect,
	/* pmap dispatcher interface */
	.clear_modify = mmu_radix_clear_modify,
	.copy = mmu_radix_copy,
	.enter = mmu_radix_enter,
	.enter_object = mmu_radix_enter_object,
	.enter_quick = mmu_radix_enter_quick,
	.extract = mmu_radix_extract,
	.extract_and_hold = mmu_radix_extract_and_hold,
	.is_modified = mmu_radix_is_modified,
	.is_prefaultable = mmu_radix_is_prefaultable,
	.is_referenced = mmu_radix_is_referenced,
	.ts_referenced = mmu_radix_ts_referenced,
	.page_exists_quick = mmu_radix_page_exists_quick,
	.page_init = mmu_radix_page_init,
	.page_wired_mappings =  mmu_radix_page_wired_mappings,
	.qenter = mmu_radix_qenter,
	.qremove = mmu_radix_qremove,
	.release = mmu_radix_release,
	.remove = mmu_radix_remove,
	.remove_all = mmu_radix_remove_all,
	.remove_write = mmu_radix_remove_write,
	.sync_icache = mmu_radix_sync_icache,
	.unwire = mmu_radix_unwire,
	.zero_page = mmu_radix_zero_page,
	.zero_page_area = mmu_radix_zero_page_area,
	.activate = mmu_radix_activate,
	.quick_enter_page =  mmu_radix_quick_enter_page,
	.quick_remove_page =  mmu_radix_quick_remove_page,
	.page_set_memattr = mmu_radix_page_set_memattr,
	.page_array_startup =  mmu_radix_page_array_startup,

	/* Internal interfaces */
	.kenter = mmu_radix_kenter,
	.kextract = mmu_radix_kextract,
	.kremove = mmu_radix_kremove,
	.change_attr = mmu_radix_change_attr,
	.decode_kernel_ptr =  mmu_radix_decode_kernel_ptr,

	.tlbie_all = mmu_radix_tlbie_all,
};

MMU_DEF(mmu_radix, MMU_TYPE_RADIX, mmu_radix_methods);

static boolean_t pmap_demote_l3e_locked(pmap_t pmap, pml3_entry_t *l3e, vm_offset_t va,
	struct rwlock **lockp);
static boolean_t pmap_demote_l3e(pmap_t pmap, pml3_entry_t *pde, vm_offset_t va);
static int pmap_unuse_pt(pmap_t, vm_offset_t, pml3_entry_t, struct spglist *);
static int pmap_remove_l3e(pmap_t pmap, pml3_entry_t *pdq, vm_offset_t sva,
    struct spglist *free, struct rwlock **lockp);
static int pmap_remove_pte(pmap_t pmap, pt_entry_t *ptq, vm_offset_t sva,
    pml3_entry_t ptepde, struct spglist *free, struct rwlock **lockp);
static vm_page_t pmap_remove_pt_page(pmap_t pmap, vm_offset_t va);
static bool pmap_remove_page(pmap_t pmap, vm_offset_t va, pml3_entry_t *pde,
    struct spglist *free);
static bool	pmap_remove_ptes(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
	pml3_entry_t *l3e, struct spglist *free, struct rwlock **lockp);

static bool	pmap_pv_insert_l3e(pmap_t pmap, vm_offset_t va, pml3_entry_t l3e,
		    u_int flags, struct rwlock **lockp);
#if VM_NRESERVLEVEL > 0
static void	pmap_pv_promote_l3e(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
	struct rwlock **lockp);
#endif
static void	pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va);
static int pmap_insert_pt_page(pmap_t pmap, vm_page_t mpte);
static vm_page_t mmu_radix_enter_quick_locked(pmap_t pmap, vm_offset_t va, vm_page_t m,
	vm_prot_t prot, vm_page_t mpte, struct rwlock **lockp, bool *invalidate);

static bool	pmap_enter_2mpage(pmap_t pmap, vm_offset_t va, vm_page_t m,
	vm_prot_t prot, struct rwlock **lockp);
static int	pmap_enter_l3e(pmap_t pmap, vm_offset_t va, pml3_entry_t newpde,
	u_int flags, vm_page_t m, struct rwlock **lockp);

static vm_page_t reclaim_pv_chunk(pmap_t locked_pmap, struct rwlock **lockp);
static void free_pv_chunk(struct pv_chunk *pc);
static vm_page_t _pmap_allocpte(pmap_t pmap, vm_pindex_t ptepindex, struct rwlock **lockp);
static vm_page_t pmap_allocl3e(pmap_t pmap, vm_offset_t va,
	struct rwlock **lockp);
static vm_page_t pmap_allocpte(pmap_t pmap, vm_offset_t va,
	struct rwlock **lockp);
static void _pmap_unwire_ptp(pmap_t pmap, vm_offset_t va, vm_page_t m,
    struct spglist *free);
static boolean_t pmap_unwire_ptp(pmap_t pmap, vm_offset_t va, vm_page_t m, struct spglist *free);

static void pmap_invalidate_page(pmap_t pmap, vm_offset_t start);
static void pmap_invalidate_all(pmap_t pmap);
static int pmap_change_attr_locked(vm_offset_t va, vm_size_t size, int mode, bool flush);
static void pmap_fill_ptp(pt_entry_t *firstpte, pt_entry_t newpte);

/*
 * Internal flags for pmap_enter()'s helper functions.
 */
#define	PMAP_ENTER_NORECLAIM	0x1000000	/* Don't reclaim PV entries. */
#define	PMAP_ENTER_NOREPLACE	0x2000000	/* Don't replace mappings. */

#define UNIMPLEMENTED() panic("%s not implemented", __func__)
#define UNTESTED() panic("%s not yet tested", __func__)

/* Number of supported PID bits */
static unsigned int isa3_pid_bits;

/* PID to start allocating from */
static unsigned int isa3_base_pid;

#define PROCTAB_SIZE_SHIFT	(isa3_pid_bits + 4)
#define PROCTAB_ENTRIES	(1ul << isa3_pid_bits)

/*
 * Map of physical memory regions.
 */
static struct	mem_region *regions, *pregions;
static struct	numa_mem_region *numa_pregions;
static u_int	phys_avail_count;
static int	regions_sz, pregions_sz, numa_pregions_sz;
static struct pate *isa3_parttab;
static struct prte *isa3_proctab;
static vmem_t *asid_arena;

extern void bs_remap_earlyboot(void);

#define	RADIX_PGD_SIZE_SHIFT	16
#define RADIX_PGD_SIZE	(1UL << RADIX_PGD_SIZE_SHIFT)

#define	RADIX_PGD_INDEX_SHIFT	(RADIX_PGD_SIZE_SHIFT-3)
#define NL2EPG (PAGE_SIZE/sizeof(pml2_entry_t))
#define NL3EPG (PAGE_SIZE/sizeof(pml3_entry_t))

#define	NUPML1E		(RADIX_PGD_SIZE/sizeof(uint64_t))	/* number of userland PML1 pages */
#define	NUPDPE		(NUPML1E * NL2EPG)/* number of userland PDP pages */
#define	NUPDE		(NUPDPE * NL3EPG)	/* number of userland PD entries */

/* POWER9 only permits a 64k partition table size. */
#define	PARTTAB_SIZE_SHIFT	16
#define PARTTAB_SIZE	(1UL << PARTTAB_SIZE_SHIFT)

#define PARTTAB_HR		(1UL << 63) /* host uses radix */
#define PARTTAB_GR		(1UL << 63) /* guest uses radix must match host */

/* TLB flush actions. Used as argument to tlbiel_flush() */
enum {
	TLB_INVAL_SCOPE_LPID = 2,	/* invalidate TLBs for current LPID */
	TLB_INVAL_SCOPE_GLOBAL = 3,	/* invalidate all TLBs */
};

#define	NPV_LIST_LOCKS	MAXCPU
static int pmap_initialized;
static vm_paddr_t proctab0pa;
static vm_paddr_t parttab_phys;
CTASSERT(sizeof(struct pv_chunk) == PAGE_SIZE);

/*
 * Data for the pv entry allocation mechanism.
 * Updates to pv_invl_gen are protected by the pv_list_locks[]
 * elements, but reads are not.
 */
static TAILQ_HEAD(pch, pv_chunk) pv_chunks = TAILQ_HEAD_INITIALIZER(pv_chunks);
static struct mtx __exclusive_cache_line pv_chunks_mutex;
static struct rwlock __exclusive_cache_line pv_list_locks[NPV_LIST_LOCKS];
static struct md_page *pv_table;
static struct md_page pv_dummy;

#ifdef PV_STATS
#define PV_STAT(x)	do { x ; } while (0)
#else
#define PV_STAT(x)	do { } while (0)
#endif

#define	pa_radix_index(pa)	((pa) >> L3_PAGE_SIZE_SHIFT)
#define	pa_to_pvh(pa)	(&pv_table[pa_radix_index(pa)])

#define	PHYS_TO_PV_LIST_LOCK(pa)	\
			(&pv_list_locks[pa_radix_index(pa) % NPV_LIST_LOCKS])

#define	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa)	do {	\
	struct rwlock **_lockp = (lockp);		\
	struct rwlock *_new_lock;			\
							\
	_new_lock = PHYS_TO_PV_LIST_LOCK(pa);		\
	if (_new_lock != *_lockp) {			\
		if (*_lockp != NULL)			\
			rw_wunlock(*_lockp);		\
		*_lockp = _new_lock;			\
		rw_wlock(*_lockp);			\
	}						\
} while (0)

#define	CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m)	\
	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, VM_PAGE_TO_PHYS(m))

#define	RELEASE_PV_LIST_LOCK(lockp)		do {	\
	struct rwlock **_lockp = (lockp);		\
							\
	if (*_lockp != NULL) {				\
		rw_wunlock(*_lockp);			\
		*_lockp = NULL;				\
	}						\
} while (0)

#define	VM_PAGE_TO_PV_LIST_LOCK(m)	\
	PHYS_TO_PV_LIST_LOCK(VM_PAGE_TO_PHYS(m))

/*
 * We support 52 bits, hence:
 * bits 52 - 31 = 21, 0b10101
 * RTS encoding details
 * bits 0 - 3 of rts -> bits 6 - 8 unsigned long
 * bits 4 - 5 of rts -> bits 62 - 63 of unsigned long
 */
#define RTS_SIZE ((0x2UL << 61) | (0x5UL << 5))

static int powernv_enabled = 1;

static __always_inline void
tlbiel_radix_set_isa300(uint32_t set, uint32_t is,
	uint32_t pid, uint32_t ric, uint32_t prs)
{
	uint64_t rb;
	uint64_t rs;

	rb = PPC_BITLSHIFT_VAL(set, 51) | PPC_BITLSHIFT_VAL(is, 53);
	rs = PPC_BITLSHIFT_VAL((uint64_t)pid, 31);

	__asm __volatile(PPC_TLBIEL(%0, %1, %2, %3, 1)
		     : : "r"(rb), "r"(rs), "i"(ric), "i"(prs)
		     : "memory");
}

static void
tlbiel_flush_isa3(uint32_t num_sets, uint32_t is)
{
	uint32_t set;

	__asm __volatile("ptesync": : :"memory");

	/*
	 * Flush the first set of the TLB, and the entire Page Walk Cache
	 * and partition table entries. Then flush the remaining sets of the
	 * TLB.
	 */
	if (is == TLB_INVAL_SCOPE_GLOBAL) {
		tlbiel_radix_set_isa300(0, is, 0, RIC_FLUSH_ALL, 0);
		for (set = 1; set < num_sets; set++)
			tlbiel_radix_set_isa300(set, is, 0, RIC_FLUSH_TLB, 0);
	}

	/* Do the same for process scoped entries. */
	tlbiel_radix_set_isa300(0, is, 0, RIC_FLUSH_ALL, 1);
	for (set = 1; set < num_sets; set++)
		tlbiel_radix_set_isa300(set, is, 0, RIC_FLUSH_TLB, 1);

	__asm __volatile("ptesync": : :"memory");
}

static void
mmu_radix_tlbiel_flush(int scope)
{
	MPASS(scope == TLB_INVAL_SCOPE_LPID ||
		  scope == TLB_INVAL_SCOPE_GLOBAL);

	tlbiel_flush_isa3(POWER9_TLB_SETS_RADIX, scope);
	__asm __volatile(PPC_INVALIDATE_ERAT "; isync" : : :"memory");
}

static void
mmu_radix_tlbie_all(void)
{
	if (powernv_enabled)
		mmu_radix_tlbiel_flush(TLB_INVAL_SCOPE_GLOBAL);
	else
		mmu_radix_tlbiel_flush(TLB_INVAL_SCOPE_LPID);
}

static void
mmu_radix_init_amor(void)
{
	/*
	* In HV mode, we init AMOR (Authority Mask Override Register) so that
	* the hypervisor and guest can setup IAMR (Instruction Authority Mask
	* Register), enable key 0 and set it to 1.
	*
	* AMOR = 0b1100 .... 0000 (Mask for key 0 is 11)
	*/
	mtspr(SPR_AMOR, (3ul << 62));
}

static void
mmu_radix_init_iamr(void)
{
	/*
	 * Radix always uses key0 of the IAMR to determine if an access is
	 * allowed. We set bit 0 (IBM bit 1) of key0, to prevent instruction
	 * fetch.
	 */
	mtspr(SPR_IAMR, (1ul << 62));
}

static void
mmu_radix_pid_set(pmap_t pmap)
{

	mtspr(SPR_PID, pmap->pm_pid);
	isync();
}

/* Quick sort callout for comparing physical addresses. */
static int
pa_cmp(const void *a, const void *b)
{
	const vm_paddr_t *pa = a, *pb = b;

	if (*pa < *pb)
		return (-1);
	else if (*pa > *pb)
		return (1);
	else
		return (0);
}

#define	pte_load_store(ptep, pte)	atomic_swap_long(ptep, pte)
#define	pte_load_clear(ptep)		atomic_swap_long(ptep, 0)
#define	pte_store(ptep, pte) do {	   \
	MPASS((pte) & (RPTE_EAA_R | RPTE_EAA_W | RPTE_EAA_X));	\
	*(u_long *)(ptep) = htobe64((u_long)((pte) | PG_V | RPTE_LEAF)); \
} while (0)
/*
 * NB: should only be used for adding directories - not for direct mappings
 */
#define	pde_store(ptep, pa) do {				\
	*(u_long *)(ptep) = htobe64((u_long)(pa|RPTE_VALID|RPTE_SHIFT)); \
} while (0)

#define	pte_clear(ptep) do {					\
		*(u_long *)(ptep) = (u_long)(0);		\
} while (0)

#define	PMAP_PDE_SUPERPAGE	(1 << 8)	/* supports 2MB superpages */

/*
 * Promotion to a 2MB (PDE) page mapping requires that the corresponding 4KB
 * (PTE) page mappings have identical settings for the following fields:
 */
#define	PG_PTE_PROMOTE	(PG_X | PG_MANAGED | PG_W | PG_PTE_CACHE | \
	    PG_M | PG_A | RPTE_EAA_MASK | PG_V)

static __inline void
pmap_resident_count_inc(pmap_t pmap, int count)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	pmap->pm_stats.resident_count += count;
}

static __inline void
pmap_resident_count_dec(pmap_t pmap, int count)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT(pmap->pm_stats.resident_count >= count,
	    ("pmap %p resident count underflow %ld %d", pmap,
	    pmap->pm_stats.resident_count, count));
	pmap->pm_stats.resident_count -= count;
}

static void
pagezero(vm_offset_t va)
{
	va = trunc_page(va);

	bzero((void *)va, PAGE_SIZE);
}

static uint64_t
allocpages(int n)
{
	u_int64_t ret;

	ret = moea64_bootstrap_alloc(n * PAGE_SIZE, PAGE_SIZE);
	for (int i = 0; i < n; i++)
		pagezero(PHYS_TO_DMAP(ret + i * PAGE_SIZE));
	return (ret);
}

static pt_entry_t *
kvtopte(vm_offset_t va)
{
	pt_entry_t *l3e;

	l3e = pmap_pml3e(kernel_pmap, va);
	if (l3e == NULL || (be64toh(*l3e) & RPTE_VALID) == 0)
		return (NULL);
	return (pmap_l3e_to_pte(l3e, va));
}

void
mmu_radix_kenter(vm_offset_t va, vm_paddr_t pa)
{
	pt_entry_t *pte;

	pte = kvtopte(va);
	MPASS(pte != NULL);
	*pte = htobe64(pa | RPTE_VALID | RPTE_LEAF | RPTE_EAA_R | \
	    RPTE_EAA_W | RPTE_EAA_P | PG_M | PG_A);
}

bool
mmu_radix_ps_enabled(pmap_t pmap)
{
	return (superpages_enabled && (pmap->pm_flags & PMAP_PDE_SUPERPAGE) != 0);
}

static pt_entry_t *
pmap_nofault_pte(pmap_t pmap, vm_offset_t va, int *is_l3e)
{
	pml3_entry_t *l3e;
	pt_entry_t *pte;

	va &= PG_PS_FRAME;
	l3e = pmap_pml3e(pmap, va);
	if (l3e == NULL || (be64toh(*l3e) & PG_V) == 0)
		return (NULL);

	if (be64toh(*l3e) & RPTE_LEAF) {
		*is_l3e = 1;
		return (l3e);
	}
	*is_l3e = 0;
	va &= PG_FRAME;
	pte = pmap_l3e_to_pte(l3e, va);
	if (pte == NULL || (be64toh(*pte) & PG_V) == 0)
		return (NULL);
	return (pte);
}

int
pmap_nofault(pmap_t pmap, vm_offset_t va, vm_prot_t flags)
{
	pt_entry_t *pte;
	pt_entry_t startpte, origpte, newpte;
	vm_page_t m;
	int is_l3e;

	startpte = 0;
 retry:
	if ((pte = pmap_nofault_pte(pmap, va, &is_l3e)) == NULL)
		return (KERN_INVALID_ADDRESS);
	origpte = newpte = be64toh(*pte);
	if (startpte == 0) {
		startpte = origpte;
		if (((flags & VM_PROT_WRITE) && (startpte & PG_M)) ||
		    ((flags & VM_PROT_READ) && (startpte & PG_A))) {
			pmap_invalidate_all(pmap);
#ifdef INVARIANTS
			if (VERBOSE_PMAP || pmap_logging)
				printf("%s(%p, %#lx, %#x) (%#lx) -- invalidate all\n",
				    __func__, pmap, va, flags, origpte);
#endif
			return (KERN_FAILURE);
		}
	}
#ifdef INVARIANTS
	if (VERBOSE_PMAP || pmap_logging)
		printf("%s(%p, %#lx, %#x) (%#lx)\n", __func__, pmap, va,
		    flags, origpte);
#endif
	PMAP_LOCK(pmap);
	if ((pte = pmap_nofault_pte(pmap, va, &is_l3e)) == NULL ||
	    be64toh(*pte) != origpte) {
		PMAP_UNLOCK(pmap);
		return (KERN_FAILURE);
	}
	m = PHYS_TO_VM_PAGE(newpte & PG_FRAME);
	MPASS(m != NULL);
	switch (flags) {
	case VM_PROT_READ:
		if ((newpte & (RPTE_EAA_R|RPTE_EAA_X)) == 0)
			goto protfail;
		newpte |= PG_A;
		vm_page_aflag_set(m, PGA_REFERENCED);
		break;
	case VM_PROT_WRITE:
		if ((newpte & RPTE_EAA_W) == 0)
			goto protfail;
		if (is_l3e)
			goto protfail;
		newpte |= PG_M;
		vm_page_dirty(m);
		break;
	case VM_PROT_EXECUTE:
		if ((newpte & RPTE_EAA_X) == 0)
			goto protfail;
		newpte |= PG_A;
		vm_page_aflag_set(m, PGA_REFERENCED);
		break;
	}

	if (!atomic_cmpset_long(pte, htobe64(origpte), htobe64(newpte)))
		goto retry;
	ptesync();
	PMAP_UNLOCK(pmap);
	if (startpte == newpte)
		return (KERN_FAILURE);
	return (0);
 protfail:
	PMAP_UNLOCK(pmap);
	return (KERN_PROTECTION_FAILURE);
}

/*
 * Returns TRUE if the given page is mapped individually or as part of
 * a 2mpage.  Otherwise, returns FALSE.
 */
boolean_t
mmu_radix_page_is_mapped(vm_page_t m)
{
	struct rwlock *lock;
	boolean_t rv;

	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (FALSE);
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_rlock(lock);
	rv = !TAILQ_EMPTY(&m->md.pv_list) ||
	    ((m->flags & PG_FICTITIOUS) == 0 &&
	    !TAILQ_EMPTY(&pa_to_pvh(VM_PAGE_TO_PHYS(m))->pv_list));
	rw_runlock(lock);
	return (rv);
}

/*
 * Determine the appropriate bits to set in a PTE or PDE for a specified
 * caching mode.
 */
static int
pmap_cache_bits(vm_memattr_t ma)
{
	if (ma != VM_MEMATTR_DEFAULT) {
		switch (ma) {
		case VM_MEMATTR_UNCACHEABLE:
			return (RPTE_ATTR_GUARDEDIO);
		case VM_MEMATTR_CACHEABLE:
			return (RPTE_ATTR_MEM);
		case VM_MEMATTR_WRITE_BACK:
		case VM_MEMATTR_PREFETCHABLE:
		case VM_MEMATTR_WRITE_COMBINING:
			return (RPTE_ATTR_UNGUARDEDIO);
		}
	}
	return (0);
}

static void
pmap_invalidate_page(pmap_t pmap, vm_offset_t start)
{
	ptesync();
	if (pmap == kernel_pmap)
		radix_tlbie_invlpg_kernel_4k(start);
	else
		radix_tlbie_invlpg_user_4k(pmap->pm_pid, start);
	ttusync();
}

static void
pmap_invalidate_page_2m(pmap_t pmap, vm_offset_t start)
{
	ptesync();
	if (pmap == kernel_pmap)
		radix_tlbie_invlpg_kernel_2m(start);
	else
		radix_tlbie_invlpg_user_2m(pmap->pm_pid, start);
	ttusync();
}

static void
pmap_invalidate_pwc(pmap_t pmap)
{
	ptesync();
	if (pmap == kernel_pmap)
		radix_tlbie_invlpwc_kernel();
	else
		radix_tlbie_invlpwc_user(pmap->pm_pid);
	ttusync();
}

static void
pmap_invalidate_range(pmap_t pmap, vm_offset_t start, vm_offset_t end)
{
	if (((start - end) >> PAGE_SHIFT) > 8) {
		pmap_invalidate_all(pmap);
		return;
	}
	ptesync();
	if (pmap == kernel_pmap) {
		while (start < end) {
			radix_tlbie_invlpg_kernel_4k(start);
			start += PAGE_SIZE;
		}
	} else {
		while (start < end) {
			radix_tlbie_invlpg_user_4k(pmap->pm_pid, start);
			start += PAGE_SIZE;
		}
	}
	ttusync();
}

static void
pmap_invalidate_all(pmap_t pmap)
{
	ptesync();
	if (pmap == kernel_pmap)
		radix_tlbie_flush_kernel();
	else
		radix_tlbie_flush_user(pmap->pm_pid);
	ttusync();
}

static void
pmap_invalidate_l3e_page(pmap_t pmap, vm_offset_t va, pml3_entry_t l3e)
{

	/*
	 * When the PDE has PG_PROMOTED set, the 2MB page mapping was created
	 * by a promotion that did not invalidate the 512 4KB page mappings
	 * that might exist in the TLB.  Consequently, at this point, the TLB
	 * may hold both 4KB and 2MB page mappings for the address range [va,
	 * va + L3_PAGE_SIZE).  Therefore, the entire range must be invalidated here.
	 * In contrast, when PG_PROMOTED is clear, the TLB will not hold any
	 * 4KB page mappings for the address range [va, va + L3_PAGE_SIZE), and so a
	 * single INVLPG suffices to invalidate the 2MB page mapping from the
	 * TLB.
	 */
	ptesync();
	if ((l3e & PG_PROMOTED) != 0)
		pmap_invalidate_range(pmap, va, va + L3_PAGE_SIZE - 1);
	else
		pmap_invalidate_page_2m(pmap, va);

	pmap_invalidate_pwc(pmap);
}

static __inline struct pv_chunk *
pv_to_chunk(pv_entry_t pv)
{

	return ((struct pv_chunk *)((uintptr_t)pv & ~(uintptr_t)PAGE_MASK));
}

#define PV_PMAP(pv) (pv_to_chunk(pv)->pc_pmap)

#define	PC_FREE0	0xfffffffffffffffful
#define	PC_FREE1	((1ul << (_NPCPV % 64)) - 1)

static const uint64_t pc_freemask[_NPCM] = { PC_FREE0, PC_FREE1 };

/*
 * Ensure that the number of spare PV entries in the specified pmap meets or
 * exceeds the given count, "needed".
 *
 * The given PV list lock may be released.
 */
static void
reserve_pv_entries(pmap_t pmap, int needed, struct rwlock **lockp)
{
	struct pch new_tail;
	struct pv_chunk *pc;
	vm_page_t m;
	int avail, free;
	bool reclaimed;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT(lockp != NULL, ("reserve_pv_entries: lockp is NULL"));

	/*
	 * Newly allocated PV chunks must be stored in a private list until
	 * the required number of PV chunks have been allocated.  Otherwise,
	 * reclaim_pv_chunk() could recycle one of these chunks.  In
	 * contrast, these chunks must be added to the pmap upon allocation.
	 */
	TAILQ_INIT(&new_tail);
retry:
	avail = 0;
	TAILQ_FOREACH(pc, &pmap->pm_pvchunk, pc_list) {
		//		if ((cpu_feature2 & CPUID2_POPCNT) == 0)
		bit_count((bitstr_t *)pc->pc_map, 0,
				  sizeof(pc->pc_map) * NBBY, &free);
#if 0
		free = popcnt_pc_map_pq(pc->pc_map);
#endif
		if (free == 0)
			break;
		avail += free;
		if (avail >= needed)
			break;
	}
	for (reclaimed = false; avail < needed; avail += _NPCPV) {
		m = vm_page_alloc_noobj(VM_ALLOC_WIRED);
		if (m == NULL) {
			m = reclaim_pv_chunk(pmap, lockp);
			if (m == NULL)
				goto retry;
			reclaimed = true;
		}
		PV_STAT(atomic_add_int(&pc_chunk_count, 1));
		PV_STAT(atomic_add_int(&pc_chunk_allocs, 1));
		dump_add_page(m->phys_addr);
		pc = (void *)PHYS_TO_DMAP(m->phys_addr);
		pc->pc_pmap = pmap;
		pc->pc_map[0] = PC_FREE0;
		pc->pc_map[1] = PC_FREE1;
		TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
		TAILQ_INSERT_TAIL(&new_tail, pc, pc_lru);
		PV_STAT(atomic_add_int(&pv_entry_spare, _NPCPV));

		/*
		 * The reclaim might have freed a chunk from the current pmap.
		 * If that chunk contained available entries, we need to
		 * re-count the number of available entries.
		 */
		if (reclaimed)
			goto retry;
	}
	if (!TAILQ_EMPTY(&new_tail)) {
		mtx_lock(&pv_chunks_mutex);
		TAILQ_CONCAT(&pv_chunks, &new_tail, pc_lru);
		mtx_unlock(&pv_chunks_mutex);
	}
}

/*
 * First find and then remove the pv entry for the specified pmap and virtual
 * address from the specified pv list.  Returns the pv entry if found and NULL
 * otherwise.  This operation can be performed on pv lists for either 4KB or
 * 2MB page mappings.
 */
static __inline pv_entry_t
pmap_pvh_remove(struct md_page *pvh, pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	TAILQ_FOREACH(pv, &pvh->pv_list, pv_link) {
#ifdef INVARIANTS
		if (PV_PMAP(pv) == NULL) {
			printf("corrupted pv_chunk/pv %p\n", pv);
			printf("pv_chunk: %64D\n", pv_to_chunk(pv), ":");
		}
		MPASS(PV_PMAP(pv) != NULL);
		MPASS(pv->pv_va != 0);
#endif
		if (pmap == PV_PMAP(pv) && va == pv->pv_va) {
			TAILQ_REMOVE(&pvh->pv_list, pv, pv_link);
			pvh->pv_gen++;
			break;
		}
	}
	return (pv);
}

/*
 * After demotion from a 2MB page mapping to 512 4KB page mappings,
 * destroy the pv entry for the 2MB page mapping and reinstantiate the pv
 * entries for each of the 4KB page mappings.
 */
static void
pmap_pv_demote_l3e(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
    struct rwlock **lockp)
{
	struct md_page *pvh;
	struct pv_chunk *pc;
	pv_entry_t pv;
	vm_offset_t va_last;
	vm_page_t m;
	int bit, field;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((pa & L3_PAGE_MASK) == 0,
	    ("pmap_pv_demote_pde: pa is not 2mpage aligned"));
	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa);

	/*
	 * Transfer the 2mpage's pv entry for this mapping to the first
	 * page's pv list.  Once this transfer begins, the pv list lock
	 * must not be released until the last pv entry is reinstantiated.
	 */
	pvh = pa_to_pvh(pa);
	va = trunc_2mpage(va);
	pv = pmap_pvh_remove(pvh, pmap, va);
	KASSERT(pv != NULL, ("pmap_pv_demote_pde: pv not found"));
	m = PHYS_TO_VM_PAGE(pa);
	TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_link);

	m->md.pv_gen++;
	/* Instantiate the remaining NPTEPG - 1 pv entries. */
	PV_STAT(atomic_add_long(&pv_entry_allocs, NPTEPG - 1));
	va_last = va + L3_PAGE_SIZE - PAGE_SIZE;
	for (;;) {
		pc = TAILQ_FIRST(&pmap->pm_pvchunk);
		KASSERT(pc->pc_map[0] != 0 || pc->pc_map[1] != 0
		    , ("pmap_pv_demote_pde: missing spare"));
		for (field = 0; field < _NPCM; field++) {
			while (pc->pc_map[field]) {
				bit = cnttzd(pc->pc_map[field]);
				pc->pc_map[field] &= ~(1ul << bit);
				pv = &pc->pc_pventry[field * 64 + bit];
				va += PAGE_SIZE;
				pv->pv_va = va;
				m++;
				KASSERT((m->oflags & VPO_UNMANAGED) == 0,
			    ("pmap_pv_demote_pde: page %p is not managed", m));
				TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_link);

				m->md.pv_gen++;
				if (va == va_last)
					goto out;
			}
		}
		TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
		TAILQ_INSERT_TAIL(&pmap->pm_pvchunk, pc, pc_list);
	}
out:
	if (pc->pc_map[0] == 0 && pc->pc_map[1] == 0) {
		TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
		TAILQ_INSERT_TAIL(&pmap->pm_pvchunk, pc, pc_list);
	}
	PV_STAT(atomic_add_long(&pv_entry_count, NPTEPG - 1));
	PV_STAT(atomic_subtract_int(&pv_entry_spare, NPTEPG - 1));
}

static void
reclaim_pv_chunk_leave_pmap(pmap_t pmap, pmap_t locked_pmap)
{

	if (pmap == NULL)
		return;
	pmap_invalidate_all(pmap);
	if (pmap != locked_pmap)
		PMAP_UNLOCK(pmap);
}

/*
 * We are in a serious low memory condition.  Resort to
 * drastic measures to free some pages so we can allocate
 * another pv entry chunk.
 *
 * Returns NULL if PV entries were reclaimed from the specified pmap.
 *
 * We do not, however, unmap 2mpages because subsequent accesses will
 * allocate per-page pv entries until repromotion occurs, thereby
 * exacerbating the shortage of free pv entries.
 */
static int active_reclaims = 0;
static vm_page_t
reclaim_pv_chunk(pmap_t locked_pmap, struct rwlock **lockp)
{
	struct pv_chunk *pc, *pc_marker, *pc_marker_end;
	struct pv_chunk_header pc_marker_b, pc_marker_end_b;
	struct md_page *pvh;
	pml3_entry_t *l3e;
	pmap_t next_pmap, pmap;
	pt_entry_t *pte, tpte;
	pv_entry_t pv;
	vm_offset_t va;
	vm_page_t m, m_pc;
	struct spglist free;
	uint64_t inuse;
	int bit, field, freed;

	PMAP_LOCK_ASSERT(locked_pmap, MA_OWNED);
	KASSERT(lockp != NULL, ("reclaim_pv_chunk: lockp is NULL"));
	pmap = NULL;
	m_pc = NULL;
	SLIST_INIT(&free);
	bzero(&pc_marker_b, sizeof(pc_marker_b));
	bzero(&pc_marker_end_b, sizeof(pc_marker_end_b));
	pc_marker = (struct pv_chunk *)&pc_marker_b;
	pc_marker_end = (struct pv_chunk *)&pc_marker_end_b;

	mtx_lock(&pv_chunks_mutex);
	active_reclaims++;
	TAILQ_INSERT_HEAD(&pv_chunks, pc_marker, pc_lru);
	TAILQ_INSERT_TAIL(&pv_chunks, pc_marker_end, pc_lru);
	while ((pc = TAILQ_NEXT(pc_marker, pc_lru)) != pc_marker_end &&
	    SLIST_EMPTY(&free)) {
		next_pmap = pc->pc_pmap;
		if (next_pmap == NULL) {
			/*
			 * The next chunk is a marker.  However, it is
			 * not our marker, so active_reclaims must be
			 * > 1.  Consequently, the next_chunk code
			 * will not rotate the pv_chunks list.
			 */
			goto next_chunk;
		}
		mtx_unlock(&pv_chunks_mutex);

		/*
		 * A pv_chunk can only be removed from the pc_lru list
		 * when both pc_chunks_mutex is owned and the
		 * corresponding pmap is locked.
		 */
		if (pmap != next_pmap) {
			reclaim_pv_chunk_leave_pmap(pmap, locked_pmap);
			pmap = next_pmap;
			/* Avoid deadlock and lock recursion. */
			if (pmap > locked_pmap) {
				RELEASE_PV_LIST_LOCK(lockp);
				PMAP_LOCK(pmap);
				mtx_lock(&pv_chunks_mutex);
				continue;
			} else if (pmap != locked_pmap) {
				if (PMAP_TRYLOCK(pmap)) {
					mtx_lock(&pv_chunks_mutex);
					continue;
				} else {
					pmap = NULL; /* pmap is not locked */
					mtx_lock(&pv_chunks_mutex);
					pc = TAILQ_NEXT(pc_marker, pc_lru);
					if (pc == NULL ||
					    pc->pc_pmap != next_pmap)
						continue;
					goto next_chunk;
				}
			}
		}

		/*
		 * Destroy every non-wired, 4 KB page mapping in the chunk.
		 */
		freed = 0;
		for (field = 0; field < _NPCM; field++) {
			for (inuse = ~pc->pc_map[field] & pc_freemask[field];
			    inuse != 0; inuse &= ~(1UL << bit)) {
				bit = cnttzd(inuse);
				pv = &pc->pc_pventry[field * 64 + bit];
				va = pv->pv_va;
				l3e = pmap_pml3e(pmap, va);
				if ((be64toh(*l3e) & RPTE_LEAF) != 0)
					continue;
				pte = pmap_l3e_to_pte(l3e, va);
				if ((be64toh(*pte) & PG_W) != 0)
					continue;
				tpte = be64toh(pte_load_clear(pte));
				m = PHYS_TO_VM_PAGE(tpte & PG_FRAME);
				if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
					vm_page_dirty(m);
				if ((tpte & PG_A) != 0)
					vm_page_aflag_set(m, PGA_REFERENCED);
				CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m);
				TAILQ_REMOVE(&m->md.pv_list, pv, pv_link);

				m->md.pv_gen++;
				if (TAILQ_EMPTY(&m->md.pv_list) &&
				    (m->flags & PG_FICTITIOUS) == 0) {
					pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
					if (TAILQ_EMPTY(&pvh->pv_list)) {
						vm_page_aflag_clear(m,
						    PGA_WRITEABLE);
					}
				}
				pc->pc_map[field] |= 1UL << bit;
				pmap_unuse_pt(pmap, va, be64toh(*l3e), &free);
				freed++;
			}
		}
		if (freed == 0) {
			mtx_lock(&pv_chunks_mutex);
			goto next_chunk;
		}
		/* Every freed mapping is for a 4 KB page. */
		pmap_resident_count_dec(pmap, freed);
		PV_STAT(atomic_add_long(&pv_entry_frees, freed));
		PV_STAT(atomic_add_int(&pv_entry_spare, freed));
		PV_STAT(atomic_subtract_long(&pv_entry_count, freed));
		TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
		if (pc->pc_map[0] == PC_FREE0 && pc->pc_map[1] == PC_FREE1) {
			PV_STAT(atomic_subtract_int(&pv_entry_spare, _NPCPV));
			PV_STAT(atomic_subtract_int(&pc_chunk_count, 1));
			PV_STAT(atomic_add_int(&pc_chunk_frees, 1));
			/* Entire chunk is free; return it. */
			m_pc = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pc));
			dump_drop_page(m_pc->phys_addr);
			mtx_lock(&pv_chunks_mutex);
			TAILQ_REMOVE(&pv_chunks, pc, pc_lru);
			break;
		}
		TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
		mtx_lock(&pv_chunks_mutex);
		/* One freed pv entry in locked_pmap is sufficient. */
		if (pmap == locked_pmap)
			break;
next_chunk:
		TAILQ_REMOVE(&pv_chunks, pc_marker, pc_lru);
		TAILQ_INSERT_AFTER(&pv_chunks, pc, pc_marker, pc_lru);
		if (active_reclaims == 1 && pmap != NULL) {
			/*
			 * Rotate the pv chunks list so that we do not
			 * scan the same pv chunks that could not be
			 * freed (because they contained a wired
			 * and/or superpage mapping) on every
			 * invocation of reclaim_pv_chunk().
			 */
			while ((pc = TAILQ_FIRST(&pv_chunks)) != pc_marker) {
				MPASS(pc->pc_pmap != NULL);
				TAILQ_REMOVE(&pv_chunks, pc, pc_lru);
				TAILQ_INSERT_TAIL(&pv_chunks, pc, pc_lru);
			}
		}
	}
	TAILQ_REMOVE(&pv_chunks, pc_marker, pc_lru);
	TAILQ_REMOVE(&pv_chunks, pc_marker_end, pc_lru);
	active_reclaims--;
	mtx_unlock(&pv_chunks_mutex);
	reclaim_pv_chunk_leave_pmap(pmap, locked_pmap);
	if (m_pc == NULL && !SLIST_EMPTY(&free)) {
		m_pc = SLIST_FIRST(&free);
		SLIST_REMOVE_HEAD(&free, plinks.s.ss);
		/* Recycle a freed page table page. */
		m_pc->ref_count = 1;
	}
	vm_page_free_pages_toq(&free, true);
	return (m_pc);
}

/*
 * free the pv_entry back to the free list
 */
static void
free_pv_entry(pmap_t pmap, pv_entry_t pv)
{
	struct pv_chunk *pc;
	int idx, field, bit;

#ifdef VERBOSE_PV
	if (pmap != kernel_pmap)
		printf("%s(%p, %p)\n", __func__, pmap, pv);
#endif
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	PV_STAT(atomic_add_long(&pv_entry_frees, 1));
	PV_STAT(atomic_add_int(&pv_entry_spare, 1));
	PV_STAT(atomic_subtract_long(&pv_entry_count, 1));
	pc = pv_to_chunk(pv);
	idx = pv - &pc->pc_pventry[0];
	field = idx / 64;
	bit = idx % 64;
	pc->pc_map[field] |= 1ul << bit;
	if (pc->pc_map[0] != PC_FREE0 || pc->pc_map[1] != PC_FREE1) {
		/* 98% of the time, pc is already at the head of the list. */
		if (__predict_false(pc != TAILQ_FIRST(&pmap->pm_pvchunk))) {
			TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
			TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
		}
		return;
	}
	TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
	free_pv_chunk(pc);
}

static void
free_pv_chunk(struct pv_chunk *pc)
{
	vm_page_t m;

	mtx_lock(&pv_chunks_mutex);
 	TAILQ_REMOVE(&pv_chunks, pc, pc_lru);
	mtx_unlock(&pv_chunks_mutex);
	PV_STAT(atomic_subtract_int(&pv_entry_spare, _NPCPV));
	PV_STAT(atomic_subtract_int(&pc_chunk_count, 1));
	PV_STAT(atomic_add_int(&pc_chunk_frees, 1));
	/* entire chunk is free, return it */
	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pc));
	dump_drop_page(m->phys_addr);
	vm_page_unwire_noq(m);
	vm_page_free(m);
}

/*
 * Returns a new PV entry, allocating a new PV chunk from the system when
 * needed.  If this PV chunk allocation fails and a PV list lock pointer was
 * given, a PV chunk is reclaimed from an arbitrary pmap.  Otherwise, NULL is
 * returned.
 *
 * The given PV list lock may be released.
 */
static pv_entry_t
get_pv_entry(pmap_t pmap, struct rwlock **lockp)
{
	int bit, field;
	pv_entry_t pv;
	struct pv_chunk *pc;
	vm_page_t m;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	PV_STAT(atomic_add_long(&pv_entry_allocs, 1));
retry:
	pc = TAILQ_FIRST(&pmap->pm_pvchunk);
	if (pc != NULL) {
		for (field = 0; field < _NPCM; field++) {
			if (pc->pc_map[field]) {
				bit = cnttzd(pc->pc_map[field]);
				break;
			}
		}
		if (field < _NPCM) {
			pv = &pc->pc_pventry[field * 64 + bit];
			pc->pc_map[field] &= ~(1ul << bit);
			/* If this was the last item, move it to tail */
			if (pc->pc_map[0] == 0 && pc->pc_map[1] == 0) {
				TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
				TAILQ_INSERT_TAIL(&pmap->pm_pvchunk, pc,
				    pc_list);
			}
			PV_STAT(atomic_add_long(&pv_entry_count, 1));
			PV_STAT(atomic_subtract_int(&pv_entry_spare, 1));
			MPASS(PV_PMAP(pv) != NULL);
			return (pv);
		}
	}
	/* No free items, allocate another chunk */
	m = vm_page_alloc_noobj(VM_ALLOC_WIRED);
	if (m == NULL) {
		if (lockp == NULL) {
			PV_STAT(pc_chunk_tryfail++);
			return (NULL);
		}
		m = reclaim_pv_chunk(pmap, lockp);
		if (m == NULL)
			goto retry;
	}
	PV_STAT(atomic_add_int(&pc_chunk_count, 1));
	PV_STAT(atomic_add_int(&pc_chunk_allocs, 1));
	dump_add_page(m->phys_addr);
	pc = (void *)PHYS_TO_DMAP(m->phys_addr);
	pc->pc_pmap = pmap;
	pc->pc_map[0] = PC_FREE0 & ~1ul;	/* preallocated bit 0 */
	pc->pc_map[1] = PC_FREE1;
	mtx_lock(&pv_chunks_mutex);
	TAILQ_INSERT_TAIL(&pv_chunks, pc, pc_lru);
	mtx_unlock(&pv_chunks_mutex);
	pv = &pc->pc_pventry[0];
	TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
	PV_STAT(atomic_add_long(&pv_entry_count, 1));
	PV_STAT(atomic_add_int(&pv_entry_spare, _NPCPV - 1));
	MPASS(PV_PMAP(pv) != NULL);
	return (pv);
}

#if VM_NRESERVLEVEL > 0
/*
 * After promotion from 512 4KB page mappings to a single 2MB page mapping,
 * replace the many pv entries for the 4KB page mappings by a single pv entry
 * for the 2MB page mapping.
 */
static void
pmap_pv_promote_l3e(pmap_t pmap, vm_offset_t va, vm_paddr_t pa,
    struct rwlock **lockp)
{
	struct md_page *pvh;
	pv_entry_t pv;
	vm_offset_t va_last;
	vm_page_t m;

	KASSERT((pa & L3_PAGE_MASK) == 0,
	    ("pmap_pv_promote_pde: pa is not 2mpage aligned"));
	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa);

	/*
	 * Transfer the first page's pv entry for this mapping to the 2mpage's
	 * pv list.  Aside from avoiding the cost of a call to get_pv_entry(),
	 * a transfer avoids the possibility that get_pv_entry() calls
	 * reclaim_pv_chunk() and that reclaim_pv_chunk() removes one of the
	 * mappings that is being promoted.
	 */
	m = PHYS_TO_VM_PAGE(pa);
	va = trunc_2mpage(va);
	pv = pmap_pvh_remove(&m->md, pmap, va);
	KASSERT(pv != NULL, ("pmap_pv_promote_pde: pv not found"));
	pvh = pa_to_pvh(pa);
	TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_link);
	pvh->pv_gen++;
	/* Free the remaining NPTEPG - 1 pv entries. */
	va_last = va + L3_PAGE_SIZE - PAGE_SIZE;
	do {
		m++;
		va += PAGE_SIZE;
		pmap_pvh_free(&m->md, pmap, va);
	} while (va < va_last);
}
#endif /* VM_NRESERVLEVEL > 0 */

/*
 * First find and then destroy the pv entry for the specified pmap and virtual
 * address.  This operation can be performed on pv lists for either 4KB or 2MB
 * page mappings.
 */
static void
pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	pv = pmap_pvh_remove(pvh, pmap, va);
	KASSERT(pv != NULL, ("pmap_pvh_free: pv not found"));
	free_pv_entry(pmap, pv);
}

/*
 * Conditionally create the PV entry for a 4KB page mapping if the required
 * memory can be allocated without resorting to reclamation.
 */
static boolean_t
pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va, vm_page_t m,
    struct rwlock **lockp)
{
	pv_entry_t pv;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	/* Pass NULL instead of the lock pointer to disable reclamation. */
	if ((pv = get_pv_entry(pmap, NULL)) != NULL) {
		pv->pv_va = va;
		CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m);
		TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_link);
		m->md.pv_gen++;
		return (TRUE);
	} else
		return (FALSE);
}

vm_paddr_t phys_avail_debug[2 * VM_PHYSSEG_MAX];
#ifdef INVARIANTS
static void
validate_addr(vm_paddr_t addr, vm_size_t size)
{
	vm_paddr_t end = addr + size;
	bool found = false;

	for (int i = 0; i < 2 * phys_avail_count; i += 2) {
		if (addr >= phys_avail_debug[i] &&
			end <= phys_avail_debug[i + 1]) {
			found = true;
			break;
		}
	}
	KASSERT(found, ("%#lx-%#lx outside of initial phys_avail array",
					addr, end));
}
#else
static void validate_addr(vm_paddr_t addr, vm_size_t size) {}
#endif
#define DMAP_PAGE_BITS (RPTE_VALID | RPTE_LEAF | RPTE_EAA_MASK | PG_M | PG_A)

static vm_paddr_t
alloc_pt_page(void)
{
	vm_paddr_t page;

	page = allocpages(1);
	pagezero(PHYS_TO_DMAP(page));
	return (page);
}

static void
mmu_radix_dmap_range(vm_paddr_t start, vm_paddr_t end)
{
	pt_entry_t *pte, pteval;
	vm_paddr_t page;

	if (bootverbose)
		printf("%s %lx -> %lx\n", __func__, start, end);
	while (start < end) {
		pteval = start | DMAP_PAGE_BITS;
		pte = pmap_pml1e(kernel_pmap, PHYS_TO_DMAP(start));
		if ((be64toh(*pte) & RPTE_VALID) == 0) {
			page = alloc_pt_page();
			pde_store(pte, page);
		}
		pte = pmap_l1e_to_l2e(pte, PHYS_TO_DMAP(start));
		if ((start & L2_PAGE_MASK) == 0 &&
			end - start >= L2_PAGE_SIZE) {
			start += L2_PAGE_SIZE;
			goto done;
		} else if ((be64toh(*pte) & RPTE_VALID) == 0) {
			page = alloc_pt_page();
			pde_store(pte, page);
		}

		pte = pmap_l2e_to_l3e(pte, PHYS_TO_DMAP(start));
		if ((start & L3_PAGE_MASK) == 0 &&
			end - start >= L3_PAGE_SIZE) {
			start += L3_PAGE_SIZE;
			goto done;
		} else if ((be64toh(*pte) & RPTE_VALID) == 0) {
			page = alloc_pt_page();
			pde_store(pte, page);
		}
		pte = pmap_l3e_to_pte(pte, PHYS_TO_DMAP(start));
		start += PAGE_SIZE;
	done:
		pte_store(pte, pteval);
	}
}

static void
mmu_radix_dmap_populate(vm_size_t hwphyssz)
{
	vm_paddr_t start, end;

	for (int i = 0; i < pregions_sz; i++) {
		start = pregions[i].mr_start;
		end = start + pregions[i].mr_size;
		if (hwphyssz && start >= hwphyssz)
			break;
		if (hwphyssz && hwphyssz < end)
			end = hwphyssz;
		mmu_radix_dmap_range(start, end);
	}
}

static void
mmu_radix_setup_pagetables(vm_size_t hwphyssz)
{
	vm_paddr_t ptpages, pages;
	pt_entry_t *pte;
	vm_paddr_t l1phys;

	bzero(kernel_pmap, sizeof(struct pmap));
	PMAP_LOCK_INIT(kernel_pmap);
	vm_radix_init(&kernel_pmap->pm_radix);

	ptpages = allocpages(3);
	l1phys = moea64_bootstrap_alloc(RADIX_PGD_SIZE, RADIX_PGD_SIZE);
	validate_addr(l1phys, RADIX_PGD_SIZE);
	if (bootverbose)
		printf("l1phys=%lx\n", l1phys);
	MPASS((l1phys & (RADIX_PGD_SIZE-1)) == 0);
	for (int i = 0; i < RADIX_PGD_SIZE/PAGE_SIZE; i++)
		pagezero(PHYS_TO_DMAP(l1phys + i * PAGE_SIZE));
	kernel_pmap->pm_pml1 = (pml1_entry_t *)PHYS_TO_DMAP(l1phys);

	mmu_radix_dmap_populate(hwphyssz);

	/*
	 * Create page tables for first 128MB of KVA
	 */
	pages = ptpages;
	pte = pmap_pml1e(kernel_pmap, VM_MIN_KERNEL_ADDRESS);
	*pte = htobe64(pages | RPTE_VALID | RPTE_SHIFT);
	pages += PAGE_SIZE;
	pte = pmap_l1e_to_l2e(pte, VM_MIN_KERNEL_ADDRESS);
	*pte = htobe64(pages | RPTE_VALID | RPTE_SHIFT);
	pages += PAGE_SIZE;
	pte = pmap_l2e_to_l3e(pte, VM_MIN_KERNEL_ADDRESS);
	/*
	 * the kernel page table pages need to be preserved in
	 * phys_avail and not overlap with previous  allocations
	 */
	pages = allocpages(nkpt);
	if (bootverbose) {
		printf("phys_avail after dmap populate and nkpt allocation\n");
		for (int j = 0; j < 2 * phys_avail_count; j+=2)
			printf("phys_avail[%d]=%08lx - phys_avail[%d]=%08lx\n",
				   j, phys_avail[j], j + 1, phys_avail[j + 1]);
	}
	KPTphys = pages;
	for (int i = 0; i < nkpt; i++, pte++, pages += PAGE_SIZE)
		*pte = htobe64(pages | RPTE_VALID | RPTE_SHIFT);
	kernel_vm_end = VM_MIN_KERNEL_ADDRESS + nkpt * L3_PAGE_SIZE;
	if (bootverbose)
		printf("kernel_pmap pml1 %p\n", kernel_pmap->pm_pml1);
	/*
	 * Add a physical memory segment (vm_phys_seg) corresponding to the
	 * preallocated kernel page table pages so that vm_page structures
	 * representing these pages will be created.  The vm_page structures
	 * are required for promotion of the corresponding kernel virtual
	 * addresses to superpage mappings.
	 */
	vm_phys_add_seg(KPTphys, KPTphys + ptoa(nkpt));
}

static void
mmu_radix_early_bootstrap(vm_offset_t start, vm_offset_t end)
{
	vm_paddr_t	kpstart, kpend;
	vm_size_t	physsz, hwphyssz;
	//uint64_t	l2virt;
	int		rm_pavail, proctab_size;
	int		i, j;

	kpstart = start & ~DMAP_BASE_ADDRESS;
	kpend = end & ~DMAP_BASE_ADDRESS;

	/* Get physical memory regions from firmware */
	mem_regions(&pregions, &pregions_sz, &regions, &regions_sz);
	CTR0(KTR_PMAP, "mmu_radix_early_bootstrap: physical memory");

	if (2 * VM_PHYSSEG_MAX < regions_sz)
		panic("mmu_radix_early_bootstrap: phys_avail too small");

	if (bootverbose)
		for (int i = 0; i < regions_sz; i++)
			printf("regions[%d].mr_start=%lx regions[%d].mr_size=%lx\n",
			    i, regions[i].mr_start, i, regions[i].mr_size);
	/*
	 * XXX workaround a simulator bug
	 */
	for (int i = 0; i < regions_sz; i++)
		if (regions[i].mr_start & PAGE_MASK) {
			regions[i].mr_start += PAGE_MASK;
			regions[i].mr_start &= ~PAGE_MASK;
			regions[i].mr_size &= ~PAGE_MASK;
		}
	if (bootverbose)
		for (int i = 0; i < pregions_sz; i++)
			printf("pregions[%d].mr_start=%lx pregions[%d].mr_size=%lx\n",
			    i, pregions[i].mr_start, i, pregions[i].mr_size);

	phys_avail_count = 0;
	physsz = 0;
	hwphyssz = 0;
	TUNABLE_ULONG_FETCH("hw.physmem", (u_long *) &hwphyssz);
	for (i = 0, j = 0; i < regions_sz; i++) {
		if (bootverbose)
			printf("regions[%d].mr_start=%016lx regions[%d].mr_size=%016lx\n",
			    i, regions[i].mr_start, i, regions[i].mr_size);

		if (regions[i].mr_size < PAGE_SIZE)
			continue;

		if (hwphyssz != 0 &&
		    (physsz + regions[i].mr_size) >= hwphyssz) {
			if (physsz < hwphyssz) {
				phys_avail[j] = regions[i].mr_start;
				phys_avail[j + 1] = regions[i].mr_start +
				    (hwphyssz - physsz);
				physsz = hwphyssz;
				phys_avail_count++;
				dump_avail[j] = phys_avail[j];
				dump_avail[j + 1] = phys_avail[j + 1];
			}
			break;
		}
		phys_avail[j] = regions[i].mr_start;
		phys_avail[j + 1] = regions[i].mr_start + regions[i].mr_size;
		dump_avail[j] = phys_avail[j];
		dump_avail[j + 1] = phys_avail[j + 1];

		phys_avail_count++;
		physsz += regions[i].mr_size;
		j += 2;
	}

	/* Check for overlap with the kernel and exception vectors */
	rm_pavail = 0;
	for (j = 0; j < 2 * phys_avail_count; j+=2) {
		if (phys_avail[j] < EXC_LAST)
			phys_avail[j] += EXC_LAST;

		if (phys_avail[j] >= kpstart &&
		    phys_avail[j + 1] <= kpend) {
			phys_avail[j] = phys_avail[j + 1] = ~0;
			rm_pavail++;
			continue;
		}

		if (kpstart >= phys_avail[j] &&
		    kpstart < phys_avail[j + 1]) {
			if (kpend < phys_avail[j + 1]) {
				phys_avail[2 * phys_avail_count] =
				    (kpend & ~PAGE_MASK) + PAGE_SIZE;
				phys_avail[2 * phys_avail_count + 1] =
				    phys_avail[j + 1];
				phys_avail_count++;
			}

			phys_avail[j + 1] = kpstart & ~PAGE_MASK;
		}

		if (kpend >= phys_avail[j] &&
		    kpend < phys_avail[j + 1]) {
			if (kpstart > phys_avail[j]) {
				phys_avail[2 * phys_avail_count] = phys_avail[j];
				phys_avail[2 * phys_avail_count + 1] =
				    kpstart & ~PAGE_MASK;
				phys_avail_count++;
			}

			phys_avail[j] = (kpend & ~PAGE_MASK) +
			    PAGE_SIZE;
		}
	}
	qsort(phys_avail, 2 * phys_avail_count, sizeof(phys_avail[0]), pa_cmp);
	for (i = 0; i < 2 * phys_avail_count; i++)
		phys_avail_debug[i] = phys_avail[i];

	/* Remove physical available regions marked for removal (~0) */
	if (rm_pavail) {
		phys_avail_count -= rm_pavail;
		for (i = 2 * phys_avail_count;
		     i < 2*(phys_avail_count + rm_pavail); i+=2)
			phys_avail[i] = phys_avail[i + 1] = 0;
	}
	if (bootverbose) {
		printf("phys_avail ranges after filtering:\n");
		for (j = 0; j < 2 * phys_avail_count; j+=2)
			printf("phys_avail[%d]=%08lx - phys_avail[%d]=%08lx\n",
				   j, phys_avail[j], j + 1, phys_avail[j + 1]);
	}
	physmem = btoc(physsz);

	/* XXX assume we're running non-virtualized and
	 * we don't support BHYVE
	 */
	if (isa3_pid_bits == 0)
		isa3_pid_bits = 20;
	if (powernv_enabled) {
		parttab_phys =
		    moea64_bootstrap_alloc(PARTTAB_SIZE, PARTTAB_SIZE);
		validate_addr(parttab_phys, PARTTAB_SIZE);
		for (int i = 0; i < PARTTAB_SIZE/PAGE_SIZE; i++)
			pagezero(PHYS_TO_DMAP(parttab_phys + i * PAGE_SIZE));

	}
	proctab_size = 1UL << PROCTAB_SIZE_SHIFT;
	proctab0pa = moea64_bootstrap_alloc(proctab_size, proctab_size);
	validate_addr(proctab0pa, proctab_size);
	for (int i = 0; i < proctab_size/PAGE_SIZE; i++)
		pagezero(PHYS_TO_DMAP(proctab0pa + i * PAGE_SIZE));

	mmu_radix_setup_pagetables(hwphyssz);
}

static void
mmu_radix_late_bootstrap(vm_offset_t start, vm_offset_t end)
{
	int		i;
	vm_paddr_t	pa;
	void		*dpcpu;
	vm_offset_t va;

	/*
	 * Set up the Open Firmware pmap and add its mappings if not in real
	 * mode.
	 */
	if (bootverbose)
		printf("%s enter\n", __func__);

	/*
	 * Calculate the last available physical address, and reserve the
	 * vm_page_array (upper bound).
	 */
	Maxmem = 0;
	for (i = 0; phys_avail[i + 1] != 0; i += 2)
		Maxmem = MAX(Maxmem, powerpc_btop(phys_avail[i + 1]));

	/*
	 * Remap any early IO mappings (console framebuffer, etc.)
	 */
	bs_remap_earlyboot();

	/*
	 * Allocate a kernel stack with a guard page for thread0 and map it
	 * into the kernel page map.
	 */
	pa = allocpages(kstack_pages);
	va = virtual_avail + KSTACK_GUARD_PAGES * PAGE_SIZE;
	virtual_avail = va + kstack_pages * PAGE_SIZE;
	CTR2(KTR_PMAP, "moea64_bootstrap: kstack0 at %#x (%#x)", pa, va);
	thread0.td_kstack = va;
	for (i = 0; i < kstack_pages; i++) {
		mmu_radix_kenter(va, pa);
		pa += PAGE_SIZE;
		va += PAGE_SIZE;
	}
	thread0.td_kstack_pages = kstack_pages;

	/*
	 * Allocate virtual address space for the message buffer.
	 */
	pa = msgbuf_phys = allocpages((msgbufsize + PAGE_MASK)  >> PAGE_SHIFT);
	msgbufp = (struct msgbuf *)PHYS_TO_DMAP(pa);

	/*
	 * Allocate virtual address space for the dynamic percpu area.
	 */
	pa = allocpages(DPCPU_SIZE >> PAGE_SHIFT);
	dpcpu = (void *)PHYS_TO_DMAP(pa);
	dpcpu_init(dpcpu, curcpu);

	crashdumpmap = (caddr_t)virtual_avail;
	virtual_avail += MAXDUMPPGS * PAGE_SIZE;

	/*
	 * Reserve some special page table entries/VA space for temporary
	 * mapping of pages.
	 */
}

static void
mmu_parttab_init(void)
{
	uint64_t ptcr;

	isa3_parttab = (struct pate *)PHYS_TO_DMAP(parttab_phys);

	if (bootverbose)
		printf("%s parttab: %p\n", __func__, isa3_parttab);
	ptcr = parttab_phys | (PARTTAB_SIZE_SHIFT-12);
	if (bootverbose)
		printf("setting ptcr %lx\n", ptcr);
	mtspr(SPR_PTCR, ptcr);
}

static void
mmu_parttab_update(uint64_t lpid, uint64_t pagetab, uint64_t proctab)
{
	uint64_t prev;

	if (bootverbose)
		printf("%s isa3_parttab %p lpid %lx pagetab %lx proctab %lx\n", __func__, isa3_parttab,
			   lpid, pagetab, proctab);
	prev = be64toh(isa3_parttab[lpid].pagetab);
	isa3_parttab[lpid].pagetab = htobe64(pagetab);
	isa3_parttab[lpid].proctab = htobe64(proctab);

	if (prev & PARTTAB_HR) {
		__asm __volatile(PPC_TLBIE_5(%0,%1,2,0,1) : :
			     "r" (TLBIEL_INVAL_SET_LPID), "r" (lpid));
		__asm __volatile(PPC_TLBIE_5(%0,%1,2,1,1) : :
			     "r" (TLBIEL_INVAL_SET_LPID), "r" (lpid));
	} else {
		__asm __volatile(PPC_TLBIE_5(%0,%1,2,0,0) : :
			     "r" (TLBIEL_INVAL_SET_LPID), "r" (lpid));
	}
	ttusync();
}

static void
mmu_radix_parttab_init(void)
{
	uint64_t pagetab;

	mmu_parttab_init();
	pagetab = RTS_SIZE | DMAP_TO_PHYS((vm_offset_t)kernel_pmap->pm_pml1) | \
		         RADIX_PGD_INDEX_SHIFT | PARTTAB_HR;
	mmu_parttab_update(0, pagetab, 0);
}

static void
mmu_radix_proctab_register(vm_paddr_t proctabpa, uint64_t table_size)
{
	uint64_t pagetab, proctab;

	pagetab = be64toh(isa3_parttab[0].pagetab);
	proctab = proctabpa | table_size | PARTTAB_GR;
	mmu_parttab_update(0, pagetab, proctab);
}

static void
mmu_radix_proctab_init(void)
{

	isa3_base_pid = 1;

	isa3_proctab = (void*)PHYS_TO_DMAP(proctab0pa);
	isa3_proctab->proctab0 =
	    htobe64(RTS_SIZE | DMAP_TO_PHYS((vm_offset_t)kernel_pmap->pm_pml1) |
		RADIX_PGD_INDEX_SHIFT);

	if (powernv_enabled) {
		mmu_radix_proctab_register(proctab0pa, PROCTAB_SIZE_SHIFT - 12);
		__asm __volatile("ptesync" : : : "memory");
		__asm __volatile(PPC_TLBIE_5(%0,%1,2,1,1) : :
			     "r" (TLBIEL_INVAL_SET_LPID), "r" (0));
		__asm __volatile("eieio; tlbsync; ptesync" : : : "memory");
#ifdef PSERIES
	} else {
		int64_t rc;

		rc = phyp_hcall(H_REGISTER_PROC_TBL,
		    PROC_TABLE_NEW | PROC_TABLE_RADIX | PROC_TABLE_GTSE,
		    proctab0pa, 0, PROCTAB_SIZE_SHIFT - 12);
		if (rc != H_SUCCESS)
			panic("mmu_radix_proctab_init: "
				"failed to register process table: rc=%jd",
				(intmax_t)rc);
#endif
	}

	if (bootverbose)
		printf("process table %p and kernel radix PDE: %p\n",
			   isa3_proctab, kernel_pmap->pm_pml1);
	mtmsr(mfmsr() | PSL_DR );
	mtmsr(mfmsr() &  ~PSL_DR);
	kernel_pmap->pm_pid = isa3_base_pid;
	isa3_base_pid++;
}

void
mmu_radix_advise(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
    int advice)
{
	struct rwlock *lock;
	pml1_entry_t *l1e;
	pml2_entry_t *l2e;
	pml3_entry_t oldl3e, *l3e;
	pt_entry_t *pte;
	vm_offset_t va, va_next;
	vm_page_t m;
	bool anychanged;

	if (advice != MADV_DONTNEED && advice != MADV_FREE)
		return;
	anychanged = false;
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		l1e = pmap_pml1e(pmap, sva);
		if ((be64toh(*l1e) & PG_V) == 0) {
			va_next = (sva + L1_PAGE_SIZE) & ~L1_PAGE_MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
		l2e = pmap_l1e_to_l2e(l1e, sva);
		if ((be64toh(*l2e) & PG_V) == 0) {
			va_next = (sva + L2_PAGE_SIZE) & ~L2_PAGE_MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
		va_next = (sva + L3_PAGE_SIZE) & ~L3_PAGE_MASK;
		if (va_next < sva)
			va_next = eva;
		l3e = pmap_l2e_to_l3e(l2e, sva);
		oldl3e = be64toh(*l3e);
		if ((oldl3e & PG_V) == 0)
			continue;
		else if ((oldl3e & RPTE_LEAF) != 0) {
			if ((oldl3e & PG_MANAGED) == 0)
				continue;
			lock = NULL;
			if (!pmap_demote_l3e_locked(pmap, l3e, sva, &lock)) {
				if (lock != NULL)
					rw_wunlock(lock);

				/*
				 * The large page mapping was destroyed.
				 */
				continue;
			}

			/*
			 * Unless the page mappings are wired, remove the
			 * mapping to a single page so that a subsequent
			 * access may repromote.  Choosing the last page
			 * within the address range [sva, min(va_next, eva))
			 * generally results in more repromotions.  Since the
			 * underlying page table page is fully populated, this
			 * removal never frees a page table page.
			 */
			if ((oldl3e & PG_W) == 0) {
				va = eva;
				if (va > va_next)
					va = va_next;
				va -= PAGE_SIZE;
				KASSERT(va >= sva,
				    ("mmu_radix_advise: no address gap"));
				pte = pmap_l3e_to_pte(l3e, va);
				KASSERT((be64toh(*pte) & PG_V) != 0,
				    ("pmap_advise: invalid PTE"));
				pmap_remove_pte(pmap, pte, va, be64toh(*l3e), NULL,
				    &lock);
				anychanged = true;
			}
			if (lock != NULL)
				rw_wunlock(lock);
		}
		if (va_next > eva)
			va_next = eva;
		va = va_next;
		for (pte = pmap_l3e_to_pte(l3e, sva); sva != va_next;
			 pte++, sva += PAGE_SIZE) {
			MPASS(pte == pmap_pte(pmap, sva));

			if ((be64toh(*pte) & (PG_MANAGED | PG_V)) != (PG_MANAGED | PG_V))
				goto maybe_invlrng;
			else if ((be64toh(*pte) & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
				if (advice == MADV_DONTNEED) {
					/*
					 * Future calls to pmap_is_modified()
					 * can be avoided by making the page
					 * dirty now.
					 */
					m = PHYS_TO_VM_PAGE(be64toh(*pte) & PG_FRAME);
					vm_page_dirty(m);
				}
				atomic_clear_long(pte, htobe64(PG_M | PG_A));
			} else if ((be64toh(*pte) & PG_A) != 0)
				atomic_clear_long(pte, htobe64(PG_A));
			else
				goto maybe_invlrng;
			anychanged = true;
			continue;
maybe_invlrng:
			if (va != va_next) {
				anychanged = true;
				va = va_next;
			}
		}
		if (va != va_next)
			anychanged = true;
	}
	if (anychanged)
		pmap_invalidate_all(pmap);
	PMAP_UNLOCK(pmap);
}

/*
 * Routines used in machine-dependent code
 */
static void
mmu_radix_bootstrap(vm_offset_t start, vm_offset_t end)
{
	uint64_t lpcr;

	if (bootverbose)
		printf("%s\n", __func__);
	hw_direct_map = 1;
	powernv_enabled = (mfmsr() & PSL_HV) ? 1 : 0;
	mmu_radix_early_bootstrap(start, end);
	if (bootverbose)
		printf("early bootstrap complete\n");
	if (powernv_enabled) {
		lpcr = mfspr(SPR_LPCR);
		mtspr(SPR_LPCR, lpcr | LPCR_UPRT | LPCR_HR);
		mmu_radix_parttab_init();
		mmu_radix_init_amor();
		if (bootverbose)
			printf("powernv init complete\n");
	}
	mmu_radix_init_iamr();
	mmu_radix_proctab_init();
	mmu_radix_pid_set(kernel_pmap);
	if (powernv_enabled)
		mmu_radix_tlbiel_flush(TLB_INVAL_SCOPE_GLOBAL);
	else
		mmu_radix_tlbiel_flush(TLB_INVAL_SCOPE_LPID);

	mmu_radix_late_bootstrap(start, end);
	numa_mem_regions(&numa_pregions, &numa_pregions_sz);
	if (bootverbose)
		printf("%s done\n", __func__);
	pmap_bootstrapped = 1;
	dmaplimit = roundup2(powerpc_ptob(Maxmem), L2_PAGE_SIZE);
	PCPU_SET(flags, PCPU_GET(flags) | PC_FLAG_NOSRS);
}

static void
mmu_radix_cpu_bootstrap(int ap)
{
	uint64_t lpcr;
	uint64_t ptcr;

	if (powernv_enabled) {
		lpcr = mfspr(SPR_LPCR);
		mtspr(SPR_LPCR, lpcr | LPCR_UPRT | LPCR_HR);

		ptcr = parttab_phys | (PARTTAB_SIZE_SHIFT-12);
		mtspr(SPR_PTCR, ptcr);
		mmu_radix_init_amor();
	}
	mmu_radix_init_iamr();
	mmu_radix_pid_set(kernel_pmap);
	if (powernv_enabled)
		mmu_radix_tlbiel_flush(TLB_INVAL_SCOPE_GLOBAL);
	else
		mmu_radix_tlbiel_flush(TLB_INVAL_SCOPE_LPID);
}

static SYSCTL_NODE(_vm_pmap, OID_AUTO, l3e, CTLFLAG_RD, 0,
    "2MB page mapping counters");

static COUNTER_U64_DEFINE_EARLY(pmap_l3e_demotions);
SYSCTL_COUNTER_U64(_vm_pmap_l3e, OID_AUTO, demotions, CTLFLAG_RD,
    &pmap_l3e_demotions, "2MB page demotions");

static COUNTER_U64_DEFINE_EARLY(pmap_l3e_mappings);
SYSCTL_COUNTER_U64(_vm_pmap_l3e, OID_AUTO, mappings, CTLFLAG_RD,
    &pmap_l3e_mappings, "2MB page mappings");

static COUNTER_U64_DEFINE_EARLY(pmap_l3e_p_failures);
SYSCTL_COUNTER_U64(_vm_pmap_l3e, OID_AUTO, p_failures, CTLFLAG_RD,
    &pmap_l3e_p_failures, "2MB page promotion failures");

static COUNTER_U64_DEFINE_EARLY(pmap_l3e_promotions);
SYSCTL_COUNTER_U64(_vm_pmap_l3e, OID_AUTO, promotions, CTLFLAG_RD,
    &pmap_l3e_promotions, "2MB page promotions");

static SYSCTL_NODE(_vm_pmap, OID_AUTO, l2e, CTLFLAG_RD, 0,
    "1GB page mapping counters");

static COUNTER_U64_DEFINE_EARLY(pmap_l2e_demotions);
SYSCTL_COUNTER_U64(_vm_pmap_l2e, OID_AUTO, demotions, CTLFLAG_RD,
    &pmap_l2e_demotions, "1GB page demotions");

void
mmu_radix_clear_modify(vm_page_t m)
{
	struct md_page *pvh;
	pmap_t pmap;
	pv_entry_t next_pv, pv;
	pml3_entry_t oldl3e, *l3e;
	pt_entry_t oldpte, *pte;
	struct rwlock *lock;
	vm_offset_t va;
	int md_gen, pvh_gen;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_clear_modify: page %p is not managed", m));
	vm_page_assert_busied(m);
	CTR2(KTR_PMAP, "%s(%p)", __func__, m);

	/*
	 * If the page is not PGA_WRITEABLE, then no PTEs can have PG_M set.
	 * If the object containing the page is locked and the page is not
	 * exclusive busied, then PGA_WRITEABLE cannot be concurrently set.
	 */
	if ((m->a.flags & PGA_WRITEABLE) == 0)
		return;
	pvh = (m->flags & PG_FICTITIOUS) != 0 ? &pv_dummy :
	    pa_to_pvh(VM_PAGE_TO_PHYS(m));
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_wlock(lock);
restart:
	TAILQ_FOREACH_SAFE(pv, &pvh->pv_list, pv_link, next_pv) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen) {
				PMAP_UNLOCK(pmap);
				goto restart;
			}
		}
		va = pv->pv_va;
		l3e = pmap_pml3e(pmap, va);
		oldl3e = be64toh(*l3e);
		if ((oldl3e & PG_RW) != 0 &&
		    pmap_demote_l3e_locked(pmap, l3e, va, &lock) &&
		    (oldl3e & PG_W) == 0) {
			/*
			 * Write protect the mapping to a
			 * single page so that a subsequent
			 * write access may repromote.
			 */
			va += VM_PAGE_TO_PHYS(m) - (oldl3e &
			    PG_PS_FRAME);
			pte = pmap_l3e_to_pte(l3e, va);
			oldpte = be64toh(*pte);
			while (!atomic_cmpset_long(pte,
			    htobe64(oldpte),
				htobe64((oldpte | RPTE_EAA_R) & ~(PG_M | PG_RW))))
				   oldpte = be64toh(*pte);
			vm_page_dirty(m);
			pmap_invalidate_page(pmap, va);
		}
		PMAP_UNLOCK(pmap);
	}
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_link) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			md_gen = m->md.pv_gen;
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen || md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto restart;
			}
		}
		l3e = pmap_pml3e(pmap, pv->pv_va);
		KASSERT((be64toh(*l3e) & RPTE_LEAF) == 0, ("pmap_clear_modify: found"
		    " a 2mpage in page %p's pv list", m));
		pte = pmap_l3e_to_pte(l3e, pv->pv_va);
		if ((be64toh(*pte) & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
			atomic_clear_long(pte, htobe64(PG_M));
			pmap_invalidate_page(pmap, pv->pv_va);
		}
		PMAP_UNLOCK(pmap);
	}
	rw_wunlock(lock);
}

void
mmu_radix_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr,
    vm_size_t len, vm_offset_t src_addr)
{
	struct rwlock *lock;
	struct spglist free;
	vm_offset_t addr;
	vm_offset_t end_addr = src_addr + len;
	vm_offset_t va_next;
	vm_page_t dst_pdpg, dstmpte, srcmpte;
	bool invalidate_all;

	CTR6(KTR_PMAP,
	    "%s(dst_pmap=%p, src_pmap=%p, dst_addr=%lx, len=%lu, src_addr=%lx)\n",
	    __func__, dst_pmap, src_pmap, dst_addr, len, src_addr);

	if (dst_addr != src_addr)
		return;
	lock = NULL;
	invalidate_all = false;
	if (dst_pmap < src_pmap) {
		PMAP_LOCK(dst_pmap);
		PMAP_LOCK(src_pmap);
	} else {
		PMAP_LOCK(src_pmap);
		PMAP_LOCK(dst_pmap);
	}

	for (addr = src_addr; addr < end_addr; addr = va_next) {
		pml1_entry_t *l1e;
		pml2_entry_t *l2e;
		pml3_entry_t srcptepaddr, *l3e;
		pt_entry_t *src_pte, *dst_pte;

		l1e = pmap_pml1e(src_pmap, addr);
		if ((be64toh(*l1e) & PG_V) == 0) {
			va_next = (addr + L1_PAGE_SIZE) & ~L1_PAGE_MASK;
			if (va_next < addr)
				va_next = end_addr;
			continue;
		}

		l2e = pmap_l1e_to_l2e(l1e, addr);
		if ((be64toh(*l2e) & PG_V) == 0) {
			va_next = (addr + L2_PAGE_SIZE) & ~L2_PAGE_MASK;
			if (va_next < addr)
				va_next = end_addr;
			continue;
		}

		va_next = (addr + L3_PAGE_SIZE) & ~L3_PAGE_MASK;
		if (va_next < addr)
			va_next = end_addr;

		l3e = pmap_l2e_to_l3e(l2e, addr);
		srcptepaddr = be64toh(*l3e);
		if (srcptepaddr == 0)
			continue;

		if (srcptepaddr & RPTE_LEAF) {
			if ((addr & L3_PAGE_MASK) != 0 ||
			    addr + L3_PAGE_SIZE > end_addr)
				continue;
			dst_pdpg = pmap_allocl3e(dst_pmap, addr, NULL);
			if (dst_pdpg == NULL)
				break;
			l3e = (pml3_entry_t *)
			    PHYS_TO_DMAP(VM_PAGE_TO_PHYS(dst_pdpg));
			l3e = &l3e[pmap_pml3e_index(addr)];
			if (be64toh(*l3e) == 0 && ((srcptepaddr & PG_MANAGED) == 0 ||
			    pmap_pv_insert_l3e(dst_pmap, addr, srcptepaddr,
			    PMAP_ENTER_NORECLAIM, &lock))) {
				*l3e = htobe64(srcptepaddr & ~PG_W);
				pmap_resident_count_inc(dst_pmap,
				    L3_PAGE_SIZE / PAGE_SIZE);
				counter_u64_add(pmap_l3e_mappings, 1);
			} else
				dst_pdpg->ref_count--;
			continue;
		}

		srcptepaddr &= PG_FRAME;
		srcmpte = PHYS_TO_VM_PAGE(srcptepaddr);
		KASSERT(srcmpte->ref_count > 0,
		    ("pmap_copy: source page table page is unused"));

		if (va_next > end_addr)
			va_next = end_addr;

		src_pte = (pt_entry_t *)PHYS_TO_DMAP(srcptepaddr);
		src_pte = &src_pte[pmap_pte_index(addr)];
		dstmpte = NULL;
		while (addr < va_next) {
			pt_entry_t ptetemp;
			ptetemp = be64toh(*src_pte);
			/*
			 * we only virtual copy managed pages
			 */
			if ((ptetemp & PG_MANAGED) != 0) {
				if (dstmpte != NULL &&
				    dstmpte->pindex == pmap_l3e_pindex(addr))
					dstmpte->ref_count++;
				else if ((dstmpte = pmap_allocpte(dst_pmap,
				    addr, NULL)) == NULL)
					goto out;
				dst_pte = (pt_entry_t *)
				    PHYS_TO_DMAP(VM_PAGE_TO_PHYS(dstmpte));
				dst_pte = &dst_pte[pmap_pte_index(addr)];
				if (be64toh(*dst_pte) == 0 &&
				    pmap_try_insert_pv_entry(dst_pmap, addr,
				    PHYS_TO_VM_PAGE(ptetemp & PG_FRAME),
				    &lock)) {
					/*
					 * Clear the wired, modified, and
					 * accessed (referenced) bits
					 * during the copy.
					 */
					*dst_pte = htobe64(ptetemp & ~(PG_W | PG_M |
					    PG_A));
					pmap_resident_count_inc(dst_pmap, 1);
				} else {
					SLIST_INIT(&free);
					if (pmap_unwire_ptp(dst_pmap, addr,
					    dstmpte, &free)) {
						/*
						 * Although "addr" is not
						 * mapped, paging-structure
						 * caches could nonetheless
						 * have entries that refer to
						 * the freed page table pages.
						 * Invalidate those entries.
						 */
						invalidate_all = true;
						vm_page_free_pages_toq(&free,
						    true);
					}
					goto out;
				}
				if (dstmpte->ref_count >= srcmpte->ref_count)
					break;
			}
			addr += PAGE_SIZE;
			if (__predict_false((addr & L3_PAGE_MASK) == 0))
				src_pte = pmap_pte(src_pmap, addr);
			else
				src_pte++;
		}
	}
out:
	if (invalidate_all)
		pmap_invalidate_all(dst_pmap);
	if (lock != NULL)
		rw_wunlock(lock);
	PMAP_UNLOCK(src_pmap);
	PMAP_UNLOCK(dst_pmap);
}

static void
mmu_radix_copy_page(vm_page_t msrc, vm_page_t mdst)
{
	vm_offset_t src = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(msrc));
	vm_offset_t dst = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(mdst));

	CTR3(KTR_PMAP, "%s(%p, %p)", __func__, src, dst);
	/*
	 * XXX slow
	 */
	bcopy((void *)src, (void *)dst, PAGE_SIZE);
}

static void
mmu_radix_copy_pages(vm_page_t ma[], vm_offset_t a_offset, vm_page_t mb[],
    vm_offset_t b_offset, int xfersize)
{
        void *a_cp, *b_cp;
        vm_offset_t a_pg_offset, b_pg_offset;
        int cnt;

	CTR6(KTR_PMAP, "%s(%p, %#x, %p, %#x, %#x)", __func__, ma,
	    a_offset, mb, b_offset, xfersize);
        
        while (xfersize > 0) {
                a_pg_offset = a_offset & PAGE_MASK;
                cnt = min(xfersize, PAGE_SIZE - a_pg_offset);
                a_cp = (char *)(uintptr_t)PHYS_TO_DMAP(
                    VM_PAGE_TO_PHYS(ma[a_offset >> PAGE_SHIFT])) +
                    a_pg_offset;
                b_pg_offset = b_offset & PAGE_MASK;
                cnt = min(cnt, PAGE_SIZE - b_pg_offset);
                b_cp = (char *)(uintptr_t)PHYS_TO_DMAP(
                    VM_PAGE_TO_PHYS(mb[b_offset >> PAGE_SHIFT])) +
                    b_pg_offset;
                bcopy(a_cp, b_cp, cnt);
                a_offset += cnt;
                b_offset += cnt;
                xfersize -= cnt;
        }
}

#if VM_NRESERVLEVEL > 0
/*
 * Tries to promote the 512, contiguous 4KB page mappings that are within a
 * single page table page (PTP) to a single 2MB page mapping.  For promotion
 * to occur, two conditions must be met: (1) the 4KB page mappings must map
 * aligned, contiguous physical memory and (2) the 4KB page mappings must have
 * identical characteristics.
 */
static int
pmap_promote_l3e(pmap_t pmap, pml3_entry_t *pde, vm_offset_t va,
    struct rwlock **lockp)
{
	pml3_entry_t newpde;
	pt_entry_t *firstpte, oldpte, pa, *pte;
	vm_page_t mpte;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * Examine the first PTE in the specified PTP.  Abort if this PTE is
	 * either invalid, unused, or does not map the first 4KB physical page
	 * within a 2MB page.
	 */
	firstpte = (pt_entry_t *)PHYS_TO_DMAP(be64toh(*pde) & PG_FRAME);
setpde:
	newpde = be64toh(*firstpte);
	if ((newpde & ((PG_FRAME & L3_PAGE_MASK) | PG_A | PG_V)) != (PG_A | PG_V)) {
		CTR2(KTR_PMAP, "pmap_promote_l3e: failure for va %#lx"
		    " in pmap %p", va, pmap);
		goto fail;
	}
	if ((newpde & (PG_M | PG_RW)) == PG_RW) {
		/*
		 * When PG_M is already clear, PG_RW can be cleared without
		 * a TLB invalidation.
		 */
		if (!atomic_cmpset_long(firstpte, htobe64(newpde), htobe64((newpde | RPTE_EAA_R) & ~RPTE_EAA_W)))
			goto setpde;
		newpde &= ~RPTE_EAA_W;
	}

	/*
	 * Examine each of the other PTEs in the specified PTP.  Abort if this
	 * PTE maps an unexpected 4KB physical page or does not have identical
	 * characteristics to the first PTE.
	 */
	pa = (newpde & (PG_PS_FRAME | PG_A | PG_V)) + L3_PAGE_SIZE - PAGE_SIZE;
	for (pte = firstpte + NPTEPG - 1; pte > firstpte; pte--) {
setpte:
		oldpte = be64toh(*pte);
		if ((oldpte & (PG_FRAME | PG_A | PG_V)) != pa) {
			CTR2(KTR_PMAP, "pmap_promote_l3e: failure for va %#lx"
			    " in pmap %p", va, pmap);
			goto fail;
		}
		if ((oldpte & (PG_M | PG_RW)) == PG_RW) {
			/*
			 * When PG_M is already clear, PG_RW can be cleared
			 * without a TLB invalidation.
			 */
			if (!atomic_cmpset_long(pte, htobe64(oldpte), htobe64((oldpte | RPTE_EAA_R) & ~RPTE_EAA_W)))
				goto setpte;
			oldpte &= ~RPTE_EAA_W;
			CTR2(KTR_PMAP, "pmap_promote_l3e: protect for va %#lx"
			    " in pmap %p", (oldpte & PG_FRAME & L3_PAGE_MASK) |
			    (va & ~L3_PAGE_MASK), pmap);
		}
		if ((oldpte & PG_PTE_PROMOTE) != (newpde & PG_PTE_PROMOTE)) {
			CTR2(KTR_PMAP, "pmap_promote_l3e: failure for va %#lx"
			    " in pmap %p", va, pmap);
			goto fail;
		}
		pa -= PAGE_SIZE;
	}

	/*
	 * Save the page table page in its current state until the PDE
	 * mapping the superpage is demoted by pmap_demote_pde() or
	 * destroyed by pmap_remove_pde().
	 */
	mpte = PHYS_TO_VM_PAGE(be64toh(*pde) & PG_FRAME);
	KASSERT(mpte >= vm_page_array &&
	    mpte < &vm_page_array[vm_page_array_size],
	    ("pmap_promote_l3e: page table page is out of range"));
	KASSERT(mpte->pindex == pmap_l3e_pindex(va),
	    ("pmap_promote_l3e: page table page's pindex is wrong"));
	if (pmap_insert_pt_page(pmap, mpte)) {
		CTR2(KTR_PMAP,
		    "pmap_promote_l3e: failure for va %#lx in pmap %p", va,
		    pmap);
		goto fail;
	}

	/*
	 * Promote the pv entries.
	 */
	if ((newpde & PG_MANAGED) != 0)
		pmap_pv_promote_l3e(pmap, va, newpde & PG_PS_FRAME, lockp);

	pte_store(pde, PG_PROMOTED | newpde);
	ptesync();
	counter_u64_add(pmap_l3e_promotions, 1);
	CTR2(KTR_PMAP, "pmap_promote_l3e: success for va %#lx"
	    " in pmap %p", va, pmap);
	return (0);
 fail:
	counter_u64_add(pmap_l3e_p_failures, 1);
	return (KERN_FAILURE);
}
#endif /* VM_NRESERVLEVEL > 0 */

int
mmu_radix_enter(pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot, u_int flags, int8_t psind)
{
	struct rwlock *lock;
	pml3_entry_t *l3e;
	pt_entry_t *pte;
	pt_entry_t newpte, origpte;
	pv_entry_t pv;
	vm_paddr_t opa, pa;
	vm_page_t mpte, om;
	int rv, retrycount;
	boolean_t nosleep, invalidate_all, invalidate_page;

	va = trunc_page(va);
	retrycount = 0;
	invalidate_page = invalidate_all = false;
	CTR6(KTR_PMAP, "pmap_enter(%p, %#lx, %p, %#x, %#x, %d)", pmap, va,
	    m, prot, flags, psind);
	KASSERT(va <= VM_MAX_KERNEL_ADDRESS, ("pmap_enter: toobig"));
	KASSERT((m->oflags & VPO_UNMANAGED) != 0 || !VA_IS_CLEANMAP(va),
	    ("pmap_enter: managed mapping within the clean submap"));
	if ((m->oflags & VPO_UNMANAGED) == 0)
		VM_PAGE_OBJECT_BUSY_ASSERT(m);

	KASSERT((flags & PMAP_ENTER_RESERVED) == 0,
	    ("pmap_enter: flags %u has reserved bits set", flags));
	pa = VM_PAGE_TO_PHYS(m);
	newpte = (pt_entry_t)(pa | PG_A | PG_V | RPTE_LEAF);
	if ((flags & VM_PROT_WRITE) != 0)
		newpte |= PG_M;
	if ((flags & VM_PROT_READ) != 0)
		newpte |= PG_A;
	if (prot & VM_PROT_READ)
		newpte |= RPTE_EAA_R;
	if ((prot & VM_PROT_WRITE) != 0)
		newpte |= RPTE_EAA_W;
	KASSERT((newpte & (PG_M | PG_RW)) != PG_M,
	    ("pmap_enter: flags includes VM_PROT_WRITE but prot doesn't"));

	if (prot & VM_PROT_EXECUTE)
		newpte |= PG_X;
	if ((flags & PMAP_ENTER_WIRED) != 0)
		newpte |= PG_W;
	if (va >= DMAP_MIN_ADDRESS)
		newpte |= RPTE_EAA_P;
	newpte |= pmap_cache_bits(m->md.mdpg_cache_attrs);
	/*
	 * Set modified bit gratuitously for writeable mappings if
	 * the page is unmanaged. We do not want to take a fault
	 * to do the dirty bit accounting for these mappings.
	 */
	if ((m->oflags & VPO_UNMANAGED) != 0) {
		if ((newpte & PG_RW) != 0)
			newpte |= PG_M;
	} else
		newpte |= PG_MANAGED;

	lock = NULL;
	PMAP_LOCK(pmap);
	if (psind == 1) {
		/* Assert the required virtual and physical alignment. */
		KASSERT((va & L3_PAGE_MASK) == 0, ("pmap_enter: va unaligned"));
		KASSERT(m->psind > 0, ("pmap_enter: m->psind < psind"));
		rv = pmap_enter_l3e(pmap, va, newpte | RPTE_LEAF, flags, m, &lock);
		goto out;
	}
	mpte = NULL;

	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
retry:
	l3e = pmap_pml3e(pmap, va);
	if (l3e != NULL && (be64toh(*l3e) & PG_V) != 0 && ((be64toh(*l3e) & RPTE_LEAF) == 0 ||
	    pmap_demote_l3e_locked(pmap, l3e, va, &lock))) {
		pte = pmap_l3e_to_pte(l3e, va);
		if (va < VM_MAXUSER_ADDRESS && mpte == NULL) {
			mpte = PHYS_TO_VM_PAGE(be64toh(*l3e) & PG_FRAME);
			mpte->ref_count++;
		}
	} else if (va < VM_MAXUSER_ADDRESS) {
		/*
		 * Here if the pte page isn't mapped, or if it has been
		 * deallocated.
		 */
		nosleep = (flags & PMAP_ENTER_NOSLEEP) != 0;
		mpte = _pmap_allocpte(pmap, pmap_l3e_pindex(va),
		    nosleep ? NULL : &lock);
		if (mpte == NULL && nosleep) {
			rv = KERN_RESOURCE_SHORTAGE;
			goto out;
		}
		if (__predict_false(retrycount++ == 6))
			panic("too many retries");
		invalidate_all = true;
		goto retry;
	} else
		panic("pmap_enter: invalid page directory va=%#lx", va);

	origpte = be64toh(*pte);
	pv = NULL;

	/*
	 * Is the specified virtual address already mapped?
	 */
	if ((origpte & PG_V) != 0) {
#ifdef INVARIANTS
		if (VERBOSE_PMAP || pmap_logging) {
			printf("cow fault pmap_enter(%p, %#lx, %p, %#x, %x, %d) --"
			    " asid=%lu curpid=%d name=%s origpte0x%lx\n",
			    pmap, va, m, prot, flags, psind, pmap->pm_pid,
			    curproc->p_pid, curproc->p_comm, origpte);
#ifdef DDB
			pmap_pte_walk(pmap->pm_pml1, va);
#endif
		}
#endif
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is wired,
		 * the PT page will be also.
		 */
		if ((newpte & PG_W) != 0 && (origpte & PG_W) == 0)
			pmap->pm_stats.wired_count++;
		else if ((newpte & PG_W) == 0 && (origpte & PG_W) != 0)
			pmap->pm_stats.wired_count--;

		/*
		 * Remove the extra PT page reference.
		 */
		if (mpte != NULL) {
			mpte->ref_count--;
			KASSERT(mpte->ref_count > 0,
			    ("pmap_enter: missing reference to page table page,"
			     " va: 0x%lx", va));
		}

		/*
		 * Has the physical page changed?
		 */
		opa = origpte & PG_FRAME;
		if (opa == pa) {
			/*
			 * No, might be a protection or wiring change.
			 */
			if ((origpte & PG_MANAGED) != 0 &&
			    (newpte & PG_RW) != 0)
				vm_page_aflag_set(m, PGA_WRITEABLE);
			if (((origpte ^ newpte) & ~(PG_M | PG_A)) == 0) {
				if ((newpte & (PG_A|PG_M)) != (origpte & (PG_A|PG_M))) {
					if (!atomic_cmpset_long(pte, htobe64(origpte), htobe64(newpte)))
						goto retry;
					if ((newpte & PG_M) != (origpte & PG_M))
						vm_page_dirty(m);
					if ((newpte & PG_A) != (origpte & PG_A))
						vm_page_aflag_set(m, PGA_REFERENCED);
					ptesync();
				} else
					invalidate_all = true;
				if (((origpte ^ newpte) & ~(PG_M | PG_A)) == 0)
					goto unchanged;
			}
			goto validate;
		}

		/*
		 * The physical page has changed.  Temporarily invalidate
		 * the mapping.  This ensures that all threads sharing the
		 * pmap keep a consistent view of the mapping, which is
		 * necessary for the correct handling of COW faults.  It
		 * also permits reuse of the old mapping's PV entry,
		 * avoiding an allocation.
		 *
		 * For consistency, handle unmanaged mappings the same way.
		 */
		origpte = be64toh(pte_load_clear(pte));
		KASSERT((origpte & PG_FRAME) == opa,
		    ("pmap_enter: unexpected pa update for %#lx", va));
		if ((origpte & PG_MANAGED) != 0) {
			om = PHYS_TO_VM_PAGE(opa);

			/*
			 * The pmap lock is sufficient to synchronize with
			 * concurrent calls to pmap_page_test_mappings() and
			 * pmap_ts_referenced().
			 */
			if ((origpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
				vm_page_dirty(om);
			if ((origpte & PG_A) != 0)
				vm_page_aflag_set(om, PGA_REFERENCED);
			CHANGE_PV_LIST_LOCK_TO_PHYS(&lock, opa);
			pv = pmap_pvh_remove(&om->md, pmap, va);
			if ((newpte & PG_MANAGED) == 0)
				free_pv_entry(pmap, pv);
#ifdef INVARIANTS
			else if (origpte & PG_MANAGED) {
				if (pv == NULL) {
#ifdef DDB
					pmap_page_print_mappings(om);
#endif
					MPASS(pv != NULL);
				}
			}
#endif
			if ((om->a.flags & PGA_WRITEABLE) != 0 &&
			    TAILQ_EMPTY(&om->md.pv_list) &&
			    ((om->flags & PG_FICTITIOUS) != 0 ||
			    TAILQ_EMPTY(&pa_to_pvh(opa)->pv_list)))
				vm_page_aflag_clear(om, PGA_WRITEABLE);
		}
		if ((origpte & PG_A) != 0)
			invalidate_page = true;
		origpte = 0;
	} else {
		if (pmap != kernel_pmap) {
#ifdef INVARIANTS
			if (VERBOSE_PMAP || pmap_logging)
				printf("pmap_enter(%p, %#lx, %p, %#x, %x, %d) -- asid=%lu curpid=%d name=%s\n",
				    pmap, va, m, prot, flags, psind,
				    pmap->pm_pid, curproc->p_pid,
				    curproc->p_comm);
#endif
		}

		/*
		 * Increment the counters.
		 */
		if ((newpte & PG_W) != 0)
			pmap->pm_stats.wired_count++;
		pmap_resident_count_inc(pmap, 1);
	}

	/*
	 * Enter on the PV list if part of our managed memory.
	 */
	if ((newpte & PG_MANAGED) != 0) {
		if (pv == NULL) {
			pv = get_pv_entry(pmap, &lock);
			pv->pv_va = va;
		}
#ifdef VERBOSE_PV
		else
			printf("reassigning pv: %p to pmap: %p\n",
				   pv, pmap);
#endif
		CHANGE_PV_LIST_LOCK_TO_PHYS(&lock, pa);
		TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_link);
		m->md.pv_gen++;
		if ((newpte & PG_RW) != 0)
			vm_page_aflag_set(m, PGA_WRITEABLE);
	}

	/*
	 * Update the PTE.
	 */
	if ((origpte & PG_V) != 0) {
validate:
		origpte = be64toh(pte_load_store(pte, htobe64(newpte)));
		KASSERT((origpte & PG_FRAME) == pa,
		    ("pmap_enter: unexpected pa update for %#lx", va));
		if ((newpte & PG_M) == 0 && (origpte & (PG_M | PG_RW)) ==
		    (PG_M | PG_RW)) {
			if ((origpte & PG_MANAGED) != 0)
				vm_page_dirty(m);
			invalidate_page = true;

			/*
			 * Although the PTE may still have PG_RW set, TLB
			 * invalidation may nonetheless be required because
			 * the PTE no longer has PG_M set.
			 */
		} else if ((origpte & PG_X) != 0 || (newpte & PG_X) == 0) {
			/*
			 * Removing capabilities requires invalidation on POWER
			 */
			invalidate_page = true;
			goto unchanged;
		}
		if ((origpte & PG_A) != 0)
			invalidate_page = true;
	} else {
		pte_store(pte, newpte);
		ptesync();
	}
unchanged:

#if VM_NRESERVLEVEL > 0
	/*
	 * If both the page table page and the reservation are fully
	 * populated, then attempt promotion.
	 */
	if ((mpte == NULL || mpte->ref_count == NPTEPG) &&
	    mmu_radix_ps_enabled(pmap) &&
	    (m->flags & PG_FICTITIOUS) == 0 &&
	    vm_reserv_level_iffullpop(m) == 0 &&
		pmap_promote_l3e(pmap, l3e, va, &lock) == 0)
		invalidate_all = true;
#endif
	if (invalidate_all)
		pmap_invalidate_all(pmap);
	else if (invalidate_page)
		pmap_invalidate_page(pmap, va);

	rv = KERN_SUCCESS;
out:
	if (lock != NULL)
		rw_wunlock(lock);
	PMAP_UNLOCK(pmap);

	return (rv);
}

/*
 * Tries to create a read- and/or execute-only 2MB page mapping.  Returns true
 * if successful.  Returns false if (1) a page table page cannot be allocated
 * without sleeping, (2) a mapping already exists at the specified virtual
 * address, or (3) a PV entry cannot be allocated without reclaiming another
 * PV entry.
 */
static bool
pmap_enter_2mpage(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
    struct rwlock **lockp)
{
	pml3_entry_t newpde;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	newpde = VM_PAGE_TO_PHYS(m) | pmap_cache_bits(m->md.mdpg_cache_attrs) |
	    RPTE_LEAF | PG_V;
	if ((m->oflags & VPO_UNMANAGED) == 0)
		newpde |= PG_MANAGED;
	if (prot & VM_PROT_EXECUTE)
		newpde |= PG_X;
	if (prot & VM_PROT_READ)
		newpde |= RPTE_EAA_R;
	if (va >= DMAP_MIN_ADDRESS)
		newpde |= RPTE_EAA_P;
	return (pmap_enter_l3e(pmap, va, newpde, PMAP_ENTER_NOSLEEP |
	    PMAP_ENTER_NOREPLACE | PMAP_ENTER_NORECLAIM, NULL, lockp) ==
	    KERN_SUCCESS);
}

/*
 * Tries to create the specified 2MB page mapping.  Returns KERN_SUCCESS if
 * the mapping was created, and either KERN_FAILURE or KERN_RESOURCE_SHORTAGE
 * otherwise.  Returns KERN_FAILURE if PMAP_ENTER_NOREPLACE was specified and
 * a mapping already exists at the specified virtual address.  Returns
 * KERN_RESOURCE_SHORTAGE if PMAP_ENTER_NOSLEEP was specified and a page table
 * page allocation failed.  Returns KERN_RESOURCE_SHORTAGE if
 * PMAP_ENTER_NORECLAIM was specified and a PV entry allocation failed.
 *
 * The parameter "m" is only used when creating a managed, writeable mapping.
 */
static int
pmap_enter_l3e(pmap_t pmap, vm_offset_t va, pml3_entry_t newpde, u_int flags,
    vm_page_t m, struct rwlock **lockp)
{
	struct spglist free;
	pml3_entry_t oldl3e, *l3e;
	vm_page_t mt, pdpg;
	vm_page_t uwptpg;

	KASSERT((newpde & (PG_M | PG_RW)) != PG_RW,
	    ("pmap_enter_pde: newpde is missing PG_M"));
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	if ((pdpg = pmap_allocl3e(pmap, va, (flags & PMAP_ENTER_NOSLEEP) != 0 ?
	    NULL : lockp)) == NULL) {
		CTR2(KTR_PMAP, "pmap_enter_pde: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return (KERN_RESOURCE_SHORTAGE);
	}
	l3e = (pml3_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pdpg));
	l3e = &l3e[pmap_pml3e_index(va)];
	oldl3e = be64toh(*l3e);
	if ((oldl3e & PG_V) != 0) {
		KASSERT(pdpg->ref_count > 1,
		    ("pmap_enter_pde: pdpg's wire count is too low"));
		if ((flags & PMAP_ENTER_NOREPLACE) != 0) {
			pdpg->ref_count--;
			CTR2(KTR_PMAP, "pmap_enter_pde: failure for va %#lx"
			    " in pmap %p", va, pmap);
			return (KERN_FAILURE);
		}
		/* Break the existing mapping(s). */
		SLIST_INIT(&free);
		if ((oldl3e & RPTE_LEAF) != 0) {
			/*
			 * The reference to the PD page that was acquired by
			 * pmap_allocl3e() ensures that it won't be freed.
			 * However, if the PDE resulted from a promotion, then
			 * a reserved PT page could be freed.
			 */
			(void)pmap_remove_l3e(pmap, l3e, va, &free, lockp);
			pmap_invalidate_l3e_page(pmap, va, oldl3e);
		} else {
			if (pmap_remove_ptes(pmap, va, va + L3_PAGE_SIZE, l3e,
			    &free, lockp))
		               pmap_invalidate_all(pmap);
		}
		vm_page_free_pages_toq(&free, true);
		if (va >= VM_MAXUSER_ADDRESS) {
			mt = PHYS_TO_VM_PAGE(be64toh(*l3e) & PG_FRAME);
			if (pmap_insert_pt_page(pmap, mt)) {
				/*
				 * XXX Currently, this can't happen because
				 * we do not perform pmap_enter(psind == 1)
				 * on the kernel pmap.
				 */
				panic("pmap_enter_pde: trie insert failed");
			}
		} else
			KASSERT(be64toh(*l3e) == 0, ("pmap_enter_pde: non-zero pde %p",
			    l3e));
	}

	/*
	 * Allocate leaf ptpage for wired userspace pages.
	 */
	uwptpg = NULL;
	if ((newpde & PG_W) != 0 && pmap != kernel_pmap) {
		uwptpg = vm_page_alloc_noobj(VM_ALLOC_WIRED);
		if (uwptpg == NULL)
			return (KERN_RESOURCE_SHORTAGE);
		uwptpg->pindex = pmap_l3e_pindex(va);
		if (pmap_insert_pt_page(pmap, uwptpg)) {
			vm_page_unwire_noq(uwptpg);
			vm_page_free(uwptpg);
			return (KERN_RESOURCE_SHORTAGE);
		}
		pmap_resident_count_inc(pmap, 1);
		uwptpg->ref_count = NPTEPG;
		pmap_fill_ptp((pt_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(uwptpg)),
		    newpde);
	}
	if ((newpde & PG_MANAGED) != 0) {
		/*
		 * Abort this mapping if its PV entry could not be created.
		 */
		if (!pmap_pv_insert_l3e(pmap, va, newpde, flags, lockp)) {
			SLIST_INIT(&free);
			if (pmap_unwire_ptp(pmap, va, pdpg, &free)) {
				/*
				 * Although "va" is not mapped, paging-
				 * structure caches could nonetheless have
				 * entries that refer to the freed page table
				 * pages.  Invalidate those entries.
				 */
				pmap_invalidate_page(pmap, va);
				vm_page_free_pages_toq(&free, true);
			}
			if (uwptpg != NULL) {
				mt = pmap_remove_pt_page(pmap, va);
				KASSERT(mt == uwptpg,
				    ("removed pt page %p, expected %p", mt,
				    uwptpg));
				pmap_resident_count_dec(pmap, 1);
				uwptpg->ref_count = 1;
				vm_page_unwire_noq(uwptpg);
				vm_page_free(uwptpg);
			}
			CTR2(KTR_PMAP, "pmap_enter_pde: failure for va %#lx"
			    " in pmap %p", va, pmap);
			return (KERN_RESOURCE_SHORTAGE);
		}
		if ((newpde & PG_RW) != 0) {
			for (mt = m; mt < &m[L3_PAGE_SIZE / PAGE_SIZE]; mt++)
				vm_page_aflag_set(mt, PGA_WRITEABLE);
		}
	}

	/*
	 * Increment counters.
	 */
	if ((newpde & PG_W) != 0)
		pmap->pm_stats.wired_count += L3_PAGE_SIZE / PAGE_SIZE;
	pmap_resident_count_inc(pmap, L3_PAGE_SIZE / PAGE_SIZE);

	/*
	 * Map the superpage.  (This is not a promoted mapping; there will not
	 * be any lingering 4KB page mappings in the TLB.)
	 */
	pte_store(l3e, newpde);
	ptesync();

	counter_u64_add(pmap_l3e_mappings, 1);
	CTR2(KTR_PMAP, "pmap_enter_pde: success for va %#lx"
	    " in pmap %p", va, pmap);
	return (KERN_SUCCESS);
}

void
mmu_radix_enter_object(pmap_t pmap, vm_offset_t start,
    vm_offset_t end, vm_page_t m_start, vm_prot_t prot)
{

	struct rwlock *lock;
	vm_offset_t va;
	vm_page_t m, mpte;
	vm_pindex_t diff, psize;
	bool invalidate;
	VM_OBJECT_ASSERT_LOCKED(m_start->object);

	CTR6(KTR_PMAP, "%s(%p, %#x, %#x, %p, %#x)", __func__, pmap, start,
	    end, m_start, prot);

	invalidate = false;
	psize = atop(end - start);
	mpte = NULL;
	m = m_start;
	lock = NULL;
	PMAP_LOCK(pmap);
	while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
		va = start + ptoa(diff);
		if ((va & L3_PAGE_MASK) == 0 && va + L3_PAGE_SIZE <= end &&
		    m->psind == 1 && mmu_radix_ps_enabled(pmap) &&
		    pmap_enter_2mpage(pmap, va, m, prot, &lock))
			m = &m[L3_PAGE_SIZE / PAGE_SIZE - 1];
		else
			mpte = mmu_radix_enter_quick_locked(pmap, va, m, prot,
			    mpte, &lock, &invalidate);
		m = TAILQ_NEXT(m, listq);
	}
	ptesync();
	if (lock != NULL)
		rw_wunlock(lock);
	if (invalidate)
		pmap_invalidate_all(pmap);
	PMAP_UNLOCK(pmap);
}

static vm_page_t
mmu_radix_enter_quick_locked(pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot, vm_page_t mpte, struct rwlock **lockp, bool *invalidate)
{
	struct spglist free;
	pt_entry_t *pte;
	vm_paddr_t pa;

	KASSERT(!VA_IS_CLEANMAP(va) ||
	    (m->oflags & VPO_UNMANAGED) != 0,
	    ("mmu_radix_enter_quick_locked: managed mapping within the clean submap"));
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		vm_pindex_t ptepindex;
		pml3_entry_t *ptepa;

		/*
		 * Calculate pagetable page index
		 */
		ptepindex = pmap_l3e_pindex(va);
		if (mpte && (mpte->pindex == ptepindex)) {
			mpte->ref_count++;
		} else {
			/*
			 * Get the page directory entry
			 */
			ptepa = pmap_pml3e(pmap, va);

			/*
			 * If the page table page is mapped, we just increment
			 * the hold count, and activate it.  Otherwise, we
			 * attempt to allocate a page table page.  If this
			 * attempt fails, we don't retry.  Instead, we give up.
			 */
			if (ptepa && (be64toh(*ptepa) & PG_V) != 0) {
				if (be64toh(*ptepa) & RPTE_LEAF)
					return (NULL);
				mpte = PHYS_TO_VM_PAGE(be64toh(*ptepa) & PG_FRAME);
				mpte->ref_count++;
			} else {
				/*
				 * Pass NULL instead of the PV list lock
				 * pointer, because we don't intend to sleep.
				 */
				mpte = _pmap_allocpte(pmap, ptepindex, NULL);
				if (mpte == NULL)
					return (mpte);
			}
		}
		pte = (pt_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(mpte));
		pte = &pte[pmap_pte_index(va)];
	} else {
		mpte = NULL;
		pte = pmap_pte(pmap, va);
	}
	if (be64toh(*pte)) {
		if (mpte != NULL) {
			mpte->ref_count--;
			mpte = NULL;
		}
		return (mpte);
	}

	/*
	 * Enter on the PV list if part of our managed memory.
	 */
	if ((m->oflags & VPO_UNMANAGED) == 0 &&
	    !pmap_try_insert_pv_entry(pmap, va, m, lockp)) {
		if (mpte != NULL) {
			SLIST_INIT(&free);
			if (pmap_unwire_ptp(pmap, va, mpte, &free)) {
				/*
				 * Although "va" is not mapped, paging-
				 * structure caches could nonetheless have
				 * entries that refer to the freed page table
				 * pages.  Invalidate those entries.
				 */
				*invalidate = true;
				vm_page_free_pages_toq(&free, true);
			}
			mpte = NULL;
		}
		return (mpte);
	}

	/*
	 * Increment counters
	 */
	pmap_resident_count_inc(pmap, 1);

	pa = VM_PAGE_TO_PHYS(m) | pmap_cache_bits(m->md.mdpg_cache_attrs);
	if (prot & VM_PROT_EXECUTE)
		pa |= PG_X;
	else
		pa |= RPTE_EAA_R;
	if ((m->oflags & VPO_UNMANAGED) == 0)
		pa |= PG_MANAGED;

	pte_store(pte, pa);
	return (mpte);
}

void
mmu_radix_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot)
{
	struct rwlock *lock;
	bool invalidate;

	lock = NULL;
	invalidate = false;
	PMAP_LOCK(pmap);
	mmu_radix_enter_quick_locked(pmap, va, m, prot, NULL, &lock,
	    &invalidate);
	ptesync();
	if (lock != NULL)
		rw_wunlock(lock);
	if (invalidate)
		pmap_invalidate_all(pmap);
	PMAP_UNLOCK(pmap);
}

vm_paddr_t
mmu_radix_extract(pmap_t pmap, vm_offset_t va)
{
	pml3_entry_t *l3e;
	pt_entry_t *pte;
	vm_paddr_t pa;

	l3e = pmap_pml3e(pmap, va);
	if (__predict_false(l3e == NULL))
		return (0);
	if (be64toh(*l3e) & RPTE_LEAF) {
		pa = (be64toh(*l3e) & PG_PS_FRAME) | (va & L3_PAGE_MASK);
		pa |= (va & L3_PAGE_MASK);
	} else {
		/*
		 * Beware of a concurrent promotion that changes the
		 * PDE at this point!  For example, vtopte() must not
		 * be used to access the PTE because it would use the
		 * new PDE.  It is, however, safe to use the old PDE
		 * because the page table page is preserved by the
		 * promotion.
		 */
		pte = pmap_l3e_to_pte(l3e, va);
		if (__predict_false(pte == NULL))
			return (0);
		pa = be64toh(*pte);
		pa = (pa & PG_FRAME) | (va & PAGE_MASK);
		pa |= (va & PAGE_MASK);
	}
	return (pa);
}

vm_page_t
mmu_radix_extract_and_hold(pmap_t pmap, vm_offset_t va, vm_prot_t prot)
{
	pml3_entry_t l3e, *l3ep;
	pt_entry_t pte;
	vm_page_t m;

	m = NULL;
	CTR4(KTR_PMAP, "%s(%p, %#x, %#x)", __func__, pmap, va, prot);
	PMAP_LOCK(pmap);
	l3ep = pmap_pml3e(pmap, va);
	if (l3ep != NULL && (l3e = be64toh(*l3ep))) {
		if (l3e & RPTE_LEAF) {
			if ((l3e & PG_RW) || (prot & VM_PROT_WRITE) == 0)
				m = PHYS_TO_VM_PAGE((l3e & PG_PS_FRAME) |
				    (va & L3_PAGE_MASK));
		} else {
			/* Native endian PTE, do not pass to pmap functions */
			pte = be64toh(*pmap_l3e_to_pte(l3ep, va));
			if ((pte & PG_V) &&
			    ((pte & PG_RW) || (prot & VM_PROT_WRITE) == 0))
				m = PHYS_TO_VM_PAGE(pte & PG_FRAME);
		}
		if (m != NULL && !vm_page_wire_mapped(m))
			m = NULL;
	}
	PMAP_UNLOCK(pmap);
	return (m);
}

static void
mmu_radix_growkernel(vm_offset_t addr)
{
	vm_paddr_t paddr;
	vm_page_t nkpg;
	pml3_entry_t *l3e;
	pml2_entry_t *l2e;

	CTR2(KTR_PMAP, "%s(%#x)", __func__, addr);
	if (VM_MIN_KERNEL_ADDRESS < addr &&
		addr < (VM_MIN_KERNEL_ADDRESS + nkpt * L3_PAGE_SIZE))
		return;

	addr = roundup2(addr, L3_PAGE_SIZE);
	if (addr - 1 >= vm_map_max(kernel_map))
		addr = vm_map_max(kernel_map);
	while (kernel_vm_end < addr) {
		l2e = pmap_pml2e(kernel_pmap, kernel_vm_end);
		if ((be64toh(*l2e) & PG_V) == 0) {
			/* We need a new PDP entry */
			nkpg = vm_page_alloc_noobj(VM_ALLOC_INTERRUPT |
			    VM_ALLOC_WIRED | VM_ALLOC_ZERO);
			if (nkpg == NULL)
				panic("pmap_growkernel: no memory to grow kernel");
			nkpg->pindex = kernel_vm_end >> L2_PAGE_SIZE_SHIFT;
			paddr = VM_PAGE_TO_PHYS(nkpg);
			pde_store(l2e, paddr);
			continue; /* try again */
		}
		l3e = pmap_l2e_to_l3e(l2e, kernel_vm_end);
		if ((be64toh(*l3e) & PG_V) != 0) {
			kernel_vm_end = (kernel_vm_end + L3_PAGE_SIZE) & ~L3_PAGE_MASK;
			if (kernel_vm_end - 1 >= vm_map_max(kernel_map)) {
				kernel_vm_end = vm_map_max(kernel_map);
				break;
			}
			continue;
		}

		nkpg = vm_page_alloc_noobj(VM_ALLOC_INTERRUPT | VM_ALLOC_WIRED |
		    VM_ALLOC_ZERO);
		if (nkpg == NULL)
			panic("pmap_growkernel: no memory to grow kernel");
		nkpg->pindex = pmap_l3e_pindex(kernel_vm_end);
		paddr = VM_PAGE_TO_PHYS(nkpg);
		pde_store(l3e, paddr);

		kernel_vm_end = (kernel_vm_end + L3_PAGE_SIZE) & ~L3_PAGE_MASK;
		if (kernel_vm_end - 1 >= vm_map_max(kernel_map)) {
			kernel_vm_end = vm_map_max(kernel_map);
			break;
		}
	}
	ptesync();
}

static MALLOC_DEFINE(M_RADIX_PGD, "radix_pgd", "radix page table root directory");
static uma_zone_t zone_radix_pgd;

static int
radix_pgd_import(void *arg __unused, void **store, int count, int domain __unused,
    int flags)
{
	int req;

	req = VM_ALLOC_WIRED | malloc2vm_flags(flags);
	for (int i = 0; i < count; i++) {
		vm_page_t m = vm_page_alloc_noobj_contig(req,
		    RADIX_PGD_SIZE / PAGE_SIZE,
		    0, (vm_paddr_t)-1, RADIX_PGD_SIZE, L1_PAGE_SIZE,
		    VM_MEMATTR_DEFAULT);
		store[i] = (void *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));
	}
	return (count);
}

static void
radix_pgd_release(void *arg __unused, void **store, int count)
{
	vm_page_t m;
	struct spglist free;
	int page_count;

	SLIST_INIT(&free);
	page_count = RADIX_PGD_SIZE/PAGE_SIZE;

	for (int i = 0; i < count; i++) {
		/*
		 * XXX selectively remove dmap and KVA entries so we don't
		 * need to bzero
		 */
		m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)store[i]));
		for (int j = page_count-1; j >= 0; j--) {
			vm_page_unwire_noq(&m[j]);
			SLIST_INSERT_HEAD(&free, &m[j], plinks.s.ss);
		}
		vm_page_free_pages_toq(&free, false);
	}
}

static void
mmu_radix_init(void)
{
	vm_page_t mpte;
	vm_size_t s;
	int error, i, pv_npg;

	/* XXX is this really needed for POWER? */
	/* L1TF, reserve page @0 unconditionally */
	vm_page_blacklist_add(0, bootverbose);

	zone_radix_pgd = uma_zcache_create("radix_pgd_cache",
		RADIX_PGD_SIZE, NULL, NULL,
#ifdef INVARIANTS
	    trash_init, trash_fini,
#else
	    NULL, NULL,
#endif
		radix_pgd_import, radix_pgd_release,
		NULL, UMA_ZONE_NOBUCKET);

	/*
	 * Initialize the vm page array entries for the kernel pmap's
	 * page table pages.
	 */
	PMAP_LOCK(kernel_pmap);
	for (i = 0; i < nkpt; i++) {
		mpte = PHYS_TO_VM_PAGE(KPTphys + (i << PAGE_SHIFT));
		KASSERT(mpte >= vm_page_array &&
		    mpte < &vm_page_array[vm_page_array_size],
		    ("pmap_init: page table page is out of range size: %lu",
		     vm_page_array_size));
		mpte->pindex = pmap_l3e_pindex(VM_MIN_KERNEL_ADDRESS) + i;
		mpte->phys_addr = KPTphys + (i << PAGE_SHIFT);
		MPASS(PHYS_TO_VM_PAGE(mpte->phys_addr) == mpte);
		//pmap_insert_pt_page(kernel_pmap, mpte);
		mpte->ref_count = 1;
	}
	PMAP_UNLOCK(kernel_pmap);
	vm_wire_add(nkpt);

	CTR1(KTR_PMAP, "%s()", __func__);
	TAILQ_INIT(&pv_dummy.pv_list);

	/*
	 * Are large page mappings enabled?
	 */
	TUNABLE_INT_FETCH("vm.pmap.superpages_enabled", &superpages_enabled);
	if (superpages_enabled) {
		KASSERT(MAXPAGESIZES > 1 && pagesizes[1] == 0,
		    ("pmap_init: can't assign to pagesizes[1]"));
		pagesizes[1] = L3_PAGE_SIZE;
	}

	/*
	 * Initialize the pv chunk list mutex.
	 */
	mtx_init(&pv_chunks_mutex, "pmap pv chunk list", NULL, MTX_DEF);

	/*
	 * Initialize the pool of pv list locks.
	 */
	for (i = 0; i < NPV_LIST_LOCKS; i++)
		rw_init(&pv_list_locks[i], "pmap pv list");

	/*
	 * Calculate the size of the pv head table for superpages.
	 */
	pv_npg = howmany(vm_phys_segs[vm_phys_nsegs - 1].end, L3_PAGE_SIZE);

	/*
	 * Allocate memory for the pv head table for superpages.
	 */
	s = (vm_size_t)(pv_npg * sizeof(struct md_page));
	s = round_page(s);
	pv_table = kmem_malloc(s, M_WAITOK | M_ZERO);
	for (i = 0; i < pv_npg; i++)
		TAILQ_INIT(&pv_table[i].pv_list);
	TAILQ_INIT(&pv_dummy.pv_list);

	pmap_initialized = 1;
	mtx_init(&qframe_mtx, "qfrmlk", NULL, MTX_SPIN);
	error = vmem_alloc(kernel_arena, PAGE_SIZE, M_BESTFIT | M_WAITOK,
	    (vmem_addr_t *)&qframe);

	if (error != 0)
		panic("qframe allocation failed");
	asid_arena = vmem_create("ASID", isa3_base_pid + 1, (1<<isa3_pid_bits),
	    1, 1, M_WAITOK);
}

static boolean_t
pmap_page_test_mappings(vm_page_t m, boolean_t accessed, boolean_t modified)
{
	struct rwlock *lock;
	pv_entry_t pv;
	struct md_page *pvh;
	pt_entry_t *pte, mask;
	pmap_t pmap;
	int md_gen, pvh_gen;
	boolean_t rv;

	rv = FALSE;
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_rlock(lock);
restart:
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_link) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			md_gen = m->md.pv_gen;
			rw_runlock(lock);
			PMAP_LOCK(pmap);
			rw_rlock(lock);
			if (md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto restart;
			}
		}
		pte = pmap_pte(pmap, pv->pv_va);
		mask = 0;
		if (modified)
			mask |= PG_RW | PG_M;
		if (accessed)
			mask |= PG_V | PG_A;
		rv = (be64toh(*pte) & mask) == mask;
		PMAP_UNLOCK(pmap);
		if (rv)
			goto out;
	}
	if ((m->flags & PG_FICTITIOUS) == 0) {
		pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
		TAILQ_FOREACH(pv, &pvh->pv_list, pv_link) {
			pmap = PV_PMAP(pv);
			if (!PMAP_TRYLOCK(pmap)) {
				md_gen = m->md.pv_gen;
				pvh_gen = pvh->pv_gen;
				rw_runlock(lock);
				PMAP_LOCK(pmap);
				rw_rlock(lock);
				if (md_gen != m->md.pv_gen ||
				    pvh_gen != pvh->pv_gen) {
					PMAP_UNLOCK(pmap);
					goto restart;
				}
			}
			pte = pmap_pml3e(pmap, pv->pv_va);
			mask = 0;
			if (modified)
				mask |= PG_RW | PG_M;
			if (accessed)
				mask |= PG_V | PG_A;
			rv = (be64toh(*pte) & mask) == mask;
			PMAP_UNLOCK(pmap);
			if (rv)
				goto out;
		}
	}
out:
	rw_runlock(lock);
	return (rv);
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page was modified
 *	in any physical maps.
 */
boolean_t
mmu_radix_is_modified(vm_page_t m)
{

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_is_modified: page %p is not managed", m));

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	/*
	 * If the page is not busied then this check is racy.
	 */
	if (!pmap_page_is_write_mapped(m))
		return (FALSE);
	return (pmap_page_test_mappings(m, FALSE, TRUE));
}

boolean_t
mmu_radix_is_prefaultable(pmap_t pmap, vm_offset_t addr)
{
	pml3_entry_t *l3e;
	pt_entry_t *pte;
	boolean_t rv;

	CTR3(KTR_PMAP, "%s(%p, %#x)", __func__, pmap, addr);
	rv = FALSE;
	PMAP_LOCK(pmap);
	l3e = pmap_pml3e(pmap, addr);
	if (l3e != NULL && (be64toh(*l3e) & (RPTE_LEAF | PG_V)) == PG_V) {
		pte = pmap_l3e_to_pte(l3e, addr);
		rv = (be64toh(*pte) & PG_V) == 0;
	}
	PMAP_UNLOCK(pmap);
	return (rv);
}

boolean_t
mmu_radix_is_referenced(vm_page_t m)
{
	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_is_referenced: page %p is not managed", m));
	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	return (pmap_page_test_mappings(m, TRUE, FALSE));
}

/*
 *	pmap_ts_referenced:
 *
 *	Return a count of reference bits for a page, clearing those bits.
 *	It is not necessary for every reference bit to be cleared, but it
 *	is necessary that 0 only be returned when there are truly no
 *	reference bits set.
 *
 *	As an optimization, update the page's dirty field if a modified bit is
 *	found while counting reference bits.  This opportunistic update can be
 *	performed at low cost and can eliminate the need for some future calls
 *	to pmap_is_modified().  However, since this function stops after
 *	finding PMAP_TS_REFERENCED_MAX reference bits, it may not detect some
 *	dirty pages.  Those dirty pages will only be detected by a future call
 *	to pmap_is_modified().
 *
 *	A DI block is not needed within this function, because
 *	invalidations are performed before the PV list lock is
 *	released.
 */
int
mmu_radix_ts_referenced(vm_page_t m)
{
	struct md_page *pvh;
	pv_entry_t pv, pvf;
	pmap_t pmap;
	struct rwlock *lock;
	pml3_entry_t oldl3e, *l3e;
	pt_entry_t *pte;
	vm_paddr_t pa;
	int cleared, md_gen, not_cleared, pvh_gen;
	struct spglist free;

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_ts_referenced: page %p is not managed", m));
	SLIST_INIT(&free);
	cleared = 0;
	pa = VM_PAGE_TO_PHYS(m);
	lock = PHYS_TO_PV_LIST_LOCK(pa);
	pvh = (m->flags & PG_FICTITIOUS) != 0 ? &pv_dummy : pa_to_pvh(pa);
	rw_wlock(lock);
retry:
	not_cleared = 0;
	if ((pvf = TAILQ_FIRST(&pvh->pv_list)) == NULL)
		goto small_mappings;
	pv = pvf;
	do {
		if (pvf == NULL)
			pvf = pv;
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen) {
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		l3e = pmap_pml3e(pmap, pv->pv_va);
		oldl3e = be64toh(*l3e);
		if ((oldl3e & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
			/*
			 * Although "oldpde" is mapping a 2MB page, because
			 * this function is called at a 4KB page granularity,
			 * we only update the 4KB page under test.
			 */
			vm_page_dirty(m);
		}
		if ((oldl3e & PG_A) != 0) {
			/*
			 * Since this reference bit is shared by 512 4KB
			 * pages, it should not be cleared every time it is
			 * tested.  Apply a simple "hash" function on the
			 * physical page number, the virtual superpage number,
			 * and the pmap address to select one 4KB page out of
			 * the 512 on which testing the reference bit will
			 * result in clearing that reference bit.  This
			 * function is designed to avoid the selection of the
			 * same 4KB page for every 2MB page mapping.
			 *
			 * On demotion, a mapping that hasn't been referenced
			 * is simply destroyed.  To avoid the possibility of a
			 * subsequent page fault on a demoted wired mapping,
			 * always leave its reference bit set.  Moreover,
			 * since the superpage is wired, the current state of
			 * its reference bit won't affect page replacement.
			 */
			if ((((pa >> PAGE_SHIFT) ^ (pv->pv_va >> L3_PAGE_SIZE_SHIFT) ^
			    (uintptr_t)pmap) & (NPTEPG - 1)) == 0 &&
			    (oldl3e & PG_W) == 0) {
				atomic_clear_long(l3e, htobe64(PG_A));
				pmap_invalidate_page(pmap, pv->pv_va);
				cleared++;
				KASSERT(lock == VM_PAGE_TO_PV_LIST_LOCK(m),
				    ("inconsistent pv lock %p %p for page %p",
				    lock, VM_PAGE_TO_PV_LIST_LOCK(m), m));
			} else
				not_cleared++;
		}
		PMAP_UNLOCK(pmap);
		/* Rotate the PV list if it has more than one entry. */
		if (pv != NULL && TAILQ_NEXT(pv, pv_link) != NULL) {
			TAILQ_REMOVE(&pvh->pv_list, pv, pv_link);
			TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_link);
			pvh->pv_gen++;
		}
		if (cleared + not_cleared >= PMAP_TS_REFERENCED_MAX)
			goto out;
	} while ((pv = TAILQ_FIRST(&pvh->pv_list)) != pvf);
small_mappings:
	if ((pvf = TAILQ_FIRST(&m->md.pv_list)) == NULL)
		goto out;
	pv = pvf;
	do {
		if (pvf == NULL)
			pvf = pv;
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			md_gen = m->md.pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen || md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		l3e = pmap_pml3e(pmap, pv->pv_va);
		KASSERT((be64toh(*l3e) & RPTE_LEAF) == 0,
		    ("pmap_ts_referenced: found a 2mpage in page %p's pv list",
		    m));
		pte = pmap_l3e_to_pte(l3e, pv->pv_va);
		if ((be64toh(*pte) & (PG_M | PG_RW)) == (PG_M | PG_RW))
			vm_page_dirty(m);
		if ((be64toh(*pte) & PG_A) != 0) {
			atomic_clear_long(pte, htobe64(PG_A));
			pmap_invalidate_page(pmap, pv->pv_va);
			cleared++;
		}
		PMAP_UNLOCK(pmap);
		/* Rotate the PV list if it has more than one entry. */
		if (pv != NULL && TAILQ_NEXT(pv, pv_link) != NULL) {
			TAILQ_REMOVE(&m->md.pv_list, pv, pv_link);
			TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_link);
			m->md.pv_gen++;
		}
	} while ((pv = TAILQ_FIRST(&m->md.pv_list)) != pvf && cleared +
	    not_cleared < PMAP_TS_REFERENCED_MAX);
out:
	rw_wunlock(lock);
	vm_page_free_pages_toq(&free, true);
	return (cleared + not_cleared);
}

static vm_offset_t
mmu_radix_map(vm_offset_t *virt __unused, vm_paddr_t start,
    vm_paddr_t end, int prot __unused)
{

	CTR5(KTR_PMAP, "%s(%p, %#x, %#x, %#x)", __func__, virt, start, end,
		 prot);
	return (PHYS_TO_DMAP(start));
}

void
mmu_radix_object_init_pt(pmap_t pmap, vm_offset_t addr,
    vm_object_t object, vm_pindex_t pindex, vm_size_t size)
{
	pml3_entry_t *l3e;
	vm_paddr_t pa, ptepa;
	vm_page_t p, pdpg;
	vm_memattr_t ma;

	CTR6(KTR_PMAP, "%s(%p, %#x, %p, %u, %#x)", __func__, pmap, addr,
	    object, pindex, size);
	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT(object->type == OBJT_DEVICE || object->type == OBJT_SG,
			("pmap_object_init_pt: non-device object"));
	/* NB: size can be logically ored with addr here */
	if ((addr & L3_PAGE_MASK) == 0 && (size & L3_PAGE_MASK) == 0) {
		if (!mmu_radix_ps_enabled(pmap))
			return;
		if (!vm_object_populate(object, pindex, pindex + atop(size)))
			return;
		p = vm_page_lookup(object, pindex);
		KASSERT(p->valid == VM_PAGE_BITS_ALL,
		    ("pmap_object_init_pt: invalid page %p", p));
		ma = p->md.mdpg_cache_attrs;

		/*
		 * Abort the mapping if the first page is not physically
		 * aligned to a 2MB page boundary.
		 */
		ptepa = VM_PAGE_TO_PHYS(p);
		if (ptepa & L3_PAGE_MASK)
			return;

		/*
		 * Skip the first page.  Abort the mapping if the rest of
		 * the pages are not physically contiguous or have differing
		 * memory attributes.
		 */
		p = TAILQ_NEXT(p, listq);
		for (pa = ptepa + PAGE_SIZE; pa < ptepa + size;
		    pa += PAGE_SIZE) {
			KASSERT(p->valid == VM_PAGE_BITS_ALL,
			    ("pmap_object_init_pt: invalid page %p", p));
			if (pa != VM_PAGE_TO_PHYS(p) ||
			    ma != p->md.mdpg_cache_attrs)
				return;
			p = TAILQ_NEXT(p, listq);
		}

		PMAP_LOCK(pmap);
		for (pa = ptepa | pmap_cache_bits(ma);
		    pa < ptepa + size; pa += L3_PAGE_SIZE) {
			pdpg = pmap_allocl3e(pmap, addr, NULL);
			if (pdpg == NULL) {
				/*
				 * The creation of mappings below is only an
				 * optimization.  If a page directory page
				 * cannot be allocated without blocking,
				 * continue on to the next mapping rather than
				 * blocking.
				 */
				addr += L3_PAGE_SIZE;
				continue;
			}
			l3e = (pml3_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pdpg));
			l3e = &l3e[pmap_pml3e_index(addr)];
			if ((be64toh(*l3e) & PG_V) == 0) {
				pa |= PG_M | PG_A | PG_RW;
				pte_store(l3e, pa);
				pmap_resident_count_inc(pmap, L3_PAGE_SIZE / PAGE_SIZE);
				counter_u64_add(pmap_l3e_mappings, 1);
			} else {
				/* Continue on if the PDE is already valid. */
				pdpg->ref_count--;
				KASSERT(pdpg->ref_count > 0,
				    ("pmap_object_init_pt: missing reference "
				    "to page directory page, va: 0x%lx", addr));
			}
			addr += L3_PAGE_SIZE;
		}
		ptesync();
		PMAP_UNLOCK(pmap);
	}
}

boolean_t
mmu_radix_page_exists_quick(pmap_t pmap, vm_page_t m)
{
	struct md_page *pvh;
	struct rwlock *lock;
	pv_entry_t pv;
	int loops = 0;
	boolean_t rv;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_page_exists_quick: page %p is not managed", m));
	CTR3(KTR_PMAP, "%s(%p, %p)", __func__, pmap, m);
	rv = FALSE;
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_rlock(lock);
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_link) {
		if (PV_PMAP(pv) == pmap) {
			rv = TRUE;
			break;
		}
		loops++;
		if (loops >= 16)
			break;
	}
	if (!rv && loops < 16 && (m->flags & PG_FICTITIOUS) == 0) {
		pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
		TAILQ_FOREACH(pv, &pvh->pv_list, pv_link) {
			if (PV_PMAP(pv) == pmap) {
				rv = TRUE;
				break;
			}
			loops++;
			if (loops >= 16)
				break;
		}
	}
	rw_runlock(lock);
	return (rv);
}

void
mmu_radix_page_init(vm_page_t m)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	TAILQ_INIT(&m->md.pv_list);
	m->md.mdpg_cache_attrs = VM_MEMATTR_DEFAULT;
}

int
mmu_radix_page_wired_mappings(vm_page_t m)
{
	struct rwlock *lock;
	struct md_page *pvh;
	pmap_t pmap;
	pt_entry_t *pte;
	pv_entry_t pv;
	int count, md_gen, pvh_gen;

	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (0);
	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	rw_rlock(lock);
restart:
	count = 0;
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_link) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			md_gen = m->md.pv_gen;
			rw_runlock(lock);
			PMAP_LOCK(pmap);
			rw_rlock(lock);
			if (md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				goto restart;
			}
		}
		pte = pmap_pte(pmap, pv->pv_va);
		if ((be64toh(*pte) & PG_W) != 0)
			count++;
		PMAP_UNLOCK(pmap);
	}
	if ((m->flags & PG_FICTITIOUS) == 0) {
		pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
		TAILQ_FOREACH(pv, &pvh->pv_list, pv_link) {
			pmap = PV_PMAP(pv);
			if (!PMAP_TRYLOCK(pmap)) {
				md_gen = m->md.pv_gen;
				pvh_gen = pvh->pv_gen;
				rw_runlock(lock);
				PMAP_LOCK(pmap);
				rw_rlock(lock);
				if (md_gen != m->md.pv_gen ||
				    pvh_gen != pvh->pv_gen) {
					PMAP_UNLOCK(pmap);
					goto restart;
				}
			}
			pte = pmap_pml3e(pmap, pv->pv_va);
			if ((be64toh(*pte) & PG_W) != 0)
				count++;
			PMAP_UNLOCK(pmap);
		}
	}
	rw_runlock(lock);
	return (count);
}

static void
mmu_radix_update_proctab(int pid, pml1_entry_t l1pa)
{
	isa3_proctab[pid].proctab0 = htobe64(RTS_SIZE |  l1pa | RADIX_PGD_INDEX_SHIFT);
}

int
mmu_radix_pinit(pmap_t pmap)
{
	vmem_addr_t pid;
	vm_paddr_t l1pa;

	CTR2(KTR_PMAP, "%s(%p)", __func__, pmap);

	/*
	 * allocate the page directory page
	 */
	pmap->pm_pml1 = uma_zalloc(zone_radix_pgd, M_WAITOK);

	for (int j = 0; j <  RADIX_PGD_SIZE_SHIFT; j++)
		pagezero((vm_offset_t)pmap->pm_pml1 + j * PAGE_SIZE);
	vm_radix_init(&pmap->pm_radix);
	TAILQ_INIT(&pmap->pm_pvchunk);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
	pmap->pm_flags = PMAP_PDE_SUPERPAGE;
	vmem_alloc(asid_arena, 1, M_FIRSTFIT|M_WAITOK, &pid);

	pmap->pm_pid = pid;
	l1pa = DMAP_TO_PHYS((vm_offset_t)pmap->pm_pml1);
	mmu_radix_update_proctab(pid, l1pa);
	__asm __volatile("ptesync;isync" : : : "memory");

	return (1);
}

/*
 * This routine is called if the desired page table page does not exist.
 *
 * If page table page allocation fails, this routine may sleep before
 * returning NULL.  It sleeps only if a lock pointer was given.
 *
 * Note: If a page allocation fails at page table level two or three,
 * one or two pages may be held during the wait, only to be released
 * afterwards.  This conservative approach is easily argued to avoid
 * race conditions.
 */
static vm_page_t
_pmap_allocpte(pmap_t pmap, vm_pindex_t ptepindex, struct rwlock **lockp)
{
	vm_page_t m, pdppg, pdpg;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * Allocate a page table page.
	 */
	if ((m = vm_page_alloc_noobj(VM_ALLOC_WIRED | VM_ALLOC_ZERO)) == NULL) {
		if (lockp != NULL) {
			RELEASE_PV_LIST_LOCK(lockp);
			PMAP_UNLOCK(pmap);
			vm_wait(NULL);
			PMAP_LOCK(pmap);
		}
		/*
		 * Indicate the need to retry.  While waiting, the page table
		 * page may have been allocated.
		 */
		return (NULL);
	}
	m->pindex = ptepindex;

	/*
	 * Map the pagetable page into the process address space, if
	 * it isn't already there.
	 */

	if (ptepindex >= (NUPDE + NUPDPE)) {
		pml1_entry_t *l1e;
		vm_pindex_t pml1index;

		/* Wire up a new PDPE page */
		pml1index = ptepindex - (NUPDE + NUPDPE);
		l1e = &pmap->pm_pml1[pml1index];
		KASSERT((be64toh(*l1e) & PG_V) == 0,
		    ("%s: L1 entry %#lx is valid", __func__, *l1e));
		pde_store(l1e, VM_PAGE_TO_PHYS(m));
	} else if (ptepindex >= NUPDE) {
		vm_pindex_t pml1index;
		vm_pindex_t pdpindex;
		pml1_entry_t *l1e;
		pml2_entry_t *l2e;

		/* Wire up a new l2e page */
		pdpindex = ptepindex - NUPDE;
		pml1index = pdpindex >> RPTE_SHIFT;

		l1e = &pmap->pm_pml1[pml1index];
		if ((be64toh(*l1e) & PG_V) == 0) {
			/* Have to allocate a new pdp, recurse */
			if (_pmap_allocpte(pmap, NUPDE + NUPDPE + pml1index,
				lockp) == NULL) {
				vm_page_unwire_noq(m);
				vm_page_free_zero(m);
				return (NULL);
			}
		} else {
			/* Add reference to l2e page */
			pdppg = PHYS_TO_VM_PAGE(be64toh(*l1e) & PG_FRAME);
			pdppg->ref_count++;
		}
		l2e = (pml2_entry_t *)PHYS_TO_DMAP(be64toh(*l1e) & PG_FRAME);

		/* Now find the pdp page */
		l2e = &l2e[pdpindex & RPTE_MASK];
		KASSERT((be64toh(*l2e) & PG_V) == 0,
		    ("%s: L2 entry %#lx is valid", __func__, *l2e));
		pde_store(l2e, VM_PAGE_TO_PHYS(m));
	} else {
		vm_pindex_t pml1index;
		vm_pindex_t pdpindex;
		pml1_entry_t *l1e;
		pml2_entry_t *l2e;
		pml3_entry_t *l3e;

		/* Wire up a new PTE page */
		pdpindex = ptepindex >> RPTE_SHIFT;
		pml1index = pdpindex >> RPTE_SHIFT;

		/* First, find the pdp and check that its valid. */
		l1e = &pmap->pm_pml1[pml1index];
		if ((be64toh(*l1e) & PG_V) == 0) {
			/* Have to allocate a new pd, recurse */
			if (_pmap_allocpte(pmap, NUPDE + pdpindex,
			    lockp) == NULL) {
				vm_page_unwire_noq(m);
				vm_page_free_zero(m);
				return (NULL);
			}
			l2e = (pml2_entry_t *)PHYS_TO_DMAP(be64toh(*l1e) & PG_FRAME);
			l2e = &l2e[pdpindex & RPTE_MASK];
		} else {
			l2e = (pml2_entry_t *)PHYS_TO_DMAP(be64toh(*l1e) & PG_FRAME);
			l2e = &l2e[pdpindex & RPTE_MASK];
			if ((be64toh(*l2e) & PG_V) == 0) {
				/* Have to allocate a new pd, recurse */
				if (_pmap_allocpte(pmap, NUPDE + pdpindex,
				    lockp) == NULL) {
					vm_page_unwire_noq(m);
					vm_page_free_zero(m);
					return (NULL);
				}
			} else {
				/* Add reference to the pd page */
				pdpg = PHYS_TO_VM_PAGE(be64toh(*l2e) & PG_FRAME);
				pdpg->ref_count++;
			}
		}
		l3e = (pml3_entry_t *)PHYS_TO_DMAP(be64toh(*l2e) & PG_FRAME);

		/* Now we know where the page directory page is */
		l3e = &l3e[ptepindex & RPTE_MASK];
		KASSERT((be64toh(*l3e) & PG_V) == 0,
		    ("%s: L3 entry %#lx is valid", __func__, *l3e));
		pde_store(l3e, VM_PAGE_TO_PHYS(m));
	}

	pmap_resident_count_inc(pmap, 1);
	return (m);
}
static vm_page_t
pmap_allocl3e(pmap_t pmap, vm_offset_t va, struct rwlock **lockp)
{
	vm_pindex_t pdpindex, ptepindex;
	pml2_entry_t *pdpe;
	vm_page_t pdpg;

retry:
	pdpe = pmap_pml2e(pmap, va);
	if (pdpe != NULL && (be64toh(*pdpe) & PG_V) != 0) {
		/* Add a reference to the pd page. */
		pdpg = PHYS_TO_VM_PAGE(be64toh(*pdpe) & PG_FRAME);
		pdpg->ref_count++;
	} else {
		/* Allocate a pd page. */
		ptepindex = pmap_l3e_pindex(va);
		pdpindex = ptepindex >> RPTE_SHIFT;
		pdpg = _pmap_allocpte(pmap, NUPDE + pdpindex, lockp);
		if (pdpg == NULL && lockp != NULL)
			goto retry;
	}
	return (pdpg);
}

static vm_page_t
pmap_allocpte(pmap_t pmap, vm_offset_t va, struct rwlock **lockp)
{
	vm_pindex_t ptepindex;
	pml3_entry_t *pd;
	vm_page_t m;

	/*
	 * Calculate pagetable page index
	 */
	ptepindex = pmap_l3e_pindex(va);
retry:
	/*
	 * Get the page directory entry
	 */
	pd = pmap_pml3e(pmap, va);

	/*
	 * This supports switching from a 2MB page to a
	 * normal 4K page.
	 */
	if (pd != NULL && (be64toh(*pd) & (RPTE_LEAF | PG_V)) == (RPTE_LEAF | PG_V)) {
		if (!pmap_demote_l3e_locked(pmap, pd, va, lockp)) {
			/*
			 * Invalidation of the 2MB page mapping may have caused
			 * the deallocation of the underlying PD page.
			 */
			pd = NULL;
		}
	}

	/*
	 * If the page table page is mapped, we just increment the
	 * hold count, and activate it.
	 */
	if (pd != NULL && (be64toh(*pd) & PG_V) != 0) {
		m = PHYS_TO_VM_PAGE(be64toh(*pd) & PG_FRAME);
		m->ref_count++;
	} else {
		/*
		 * Here if the pte page isn't mapped, or if it has been
		 * deallocated.
		 */
		m = _pmap_allocpte(pmap, ptepindex, lockp);
		if (m == NULL && lockp != NULL)
			goto retry;
	}
	return (m);
}

static void
mmu_radix_pinit0(pmap_t pmap)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, pmap);
	PMAP_LOCK_INIT(pmap);
	pmap->pm_pml1 = kernel_pmap->pm_pml1;
	pmap->pm_pid = kernel_pmap->pm_pid;

	vm_radix_init(&pmap->pm_radix);
	TAILQ_INIT(&pmap->pm_pvchunk);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
	kernel_pmap->pm_flags =
		pmap->pm_flags = PMAP_PDE_SUPERPAGE;
}
/*
 * pmap_protect_l3e: do the things to protect a 2mpage in a process
 */
static boolean_t
pmap_protect_l3e(pmap_t pmap, pt_entry_t *l3e, vm_offset_t sva, vm_prot_t prot)
{
	pt_entry_t newpde, oldpde;
	vm_offset_t eva, va;
	vm_page_t m;
	boolean_t anychanged;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((sva & L3_PAGE_MASK) == 0,
	    ("pmap_protect_l3e: sva is not 2mpage aligned"));
	anychanged = FALSE;
retry:
	oldpde = newpde = be64toh(*l3e);
	if ((oldpde & (PG_MANAGED | PG_M | PG_RW)) ==
	    (PG_MANAGED | PG_M | PG_RW)) {
		eva = sva + L3_PAGE_SIZE;
		for (va = sva, m = PHYS_TO_VM_PAGE(oldpde & PG_PS_FRAME);
		    va < eva; va += PAGE_SIZE, m++)
			vm_page_dirty(m);
	}
	if ((prot & VM_PROT_WRITE) == 0) {
		newpde &= ~(PG_RW | PG_M);
		newpde |= RPTE_EAA_R;
	}
	if (prot & VM_PROT_EXECUTE)
		newpde |= PG_X;
	if (newpde != oldpde) {
		/*
		 * As an optimization to future operations on this PDE, clear
		 * PG_PROMOTED.  The impending invalidation will remove any
		 * lingering 4KB page mappings from the TLB.
		 */
		if (!atomic_cmpset_long(l3e, htobe64(oldpde), htobe64(newpde & ~PG_PROMOTED)))
			goto retry;
		anychanged = TRUE;
	}
	return (anychanged);
}

void
mmu_radix_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
    vm_prot_t prot)
{
	vm_offset_t va_next;
	pml1_entry_t *l1e;
	pml2_entry_t *l2e;
	pml3_entry_t ptpaddr, *l3e;
	pt_entry_t *pte;
	boolean_t anychanged;

	CTR5(KTR_PMAP, "%s(%p, %#x, %#x, %#x)", __func__, pmap, sva, eva,
	    prot);

	KASSERT((prot & ~VM_PROT_ALL) == 0, ("invalid prot %x", prot));
	if (prot == VM_PROT_NONE) {
		mmu_radix_remove(pmap, sva, eva);
		return;
	}

	if ((prot & (VM_PROT_WRITE|VM_PROT_EXECUTE)) ==
	    (VM_PROT_WRITE|VM_PROT_EXECUTE))
		return;

#ifdef INVARIANTS
	if (VERBOSE_PROTECT || pmap_logging)
		printf("pmap_protect(%p, %#lx, %#lx, %x) - asid: %lu\n",
			   pmap, sva, eva, prot, pmap->pm_pid);
#endif
	anychanged = FALSE;

	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		l1e = pmap_pml1e(pmap, sva);
		if ((be64toh(*l1e) & PG_V) == 0) {
			va_next = (sva + L1_PAGE_SIZE) & ~L1_PAGE_MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		l2e = pmap_l1e_to_l2e(l1e, sva);
		if ((be64toh(*l2e) & PG_V) == 0) {
			va_next = (sva + L2_PAGE_SIZE) & ~L2_PAGE_MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		va_next = (sva + L3_PAGE_SIZE) & ~L3_PAGE_MASK;
		if (va_next < sva)
			va_next = eva;

		l3e = pmap_l2e_to_l3e(l2e, sva);
		ptpaddr = be64toh(*l3e);

		/*
		 * Weed out invalid mappings.
		 */
		if (ptpaddr == 0)
			continue;

		/*
		 * Check for large page.
		 */
		if ((ptpaddr & RPTE_LEAF) != 0) {
			/*
			 * Are we protecting the entire large page?  If not,
			 * demote the mapping and fall through.
			 */
			if (sva + L3_PAGE_SIZE == va_next && eva >= va_next) {
				if (pmap_protect_l3e(pmap, l3e, sva, prot))
					anychanged = TRUE;
				continue;
			} else if (!pmap_demote_l3e(pmap, l3e, sva)) {
				/*
				 * The large page mapping was destroyed.
				 */
				continue;
			}
		}

		if (va_next > eva)
			va_next = eva;

		for (pte = pmap_l3e_to_pte(l3e, sva); sva != va_next; pte++,
		    sva += PAGE_SIZE) {
			pt_entry_t obits, pbits;
			vm_page_t m;

retry:
			MPASS(pte == pmap_pte(pmap, sva));
			obits = pbits = be64toh(*pte);
			if ((pbits & PG_V) == 0)
				continue;

			if ((prot & VM_PROT_WRITE) == 0) {
				if ((pbits & (PG_MANAGED | PG_M | PG_RW)) ==
				    (PG_MANAGED | PG_M | PG_RW)) {
					m = PHYS_TO_VM_PAGE(pbits & PG_FRAME);
					vm_page_dirty(m);
				}
				pbits &= ~(PG_RW | PG_M);
				pbits |= RPTE_EAA_R;
			}
			if (prot & VM_PROT_EXECUTE)
				pbits |= PG_X;

			if (pbits != obits) {
				if (!atomic_cmpset_long(pte, htobe64(obits), htobe64(pbits)))
					goto retry;
				if (obits & (PG_A|PG_M)) {
					anychanged = TRUE;
#ifdef INVARIANTS
					if (VERBOSE_PROTECT || pmap_logging)
						printf("%#lx %#lx -> %#lx\n",
						    sva, obits, pbits);
#endif
				}
			}
		}
	}
	if (anychanged)
		pmap_invalidate_all(pmap);
	PMAP_UNLOCK(pmap);
}

void
mmu_radix_qenter(vm_offset_t sva, vm_page_t *ma, int count)
{

	CTR4(KTR_PMAP, "%s(%#x, %p, %d)", __func__, sva, ma, count);
	pt_entry_t oldpte, pa, *pte;
	vm_page_t m;
	uint64_t cache_bits, attr_bits;
	vm_offset_t va;

	oldpte = 0;
	attr_bits = RPTE_EAA_R | RPTE_EAA_W | RPTE_EAA_P | PG_M | PG_A;
	va = sva;
	pte = kvtopte(va);
	while (va < sva + PAGE_SIZE * count) {
		if (__predict_false((va & L3_PAGE_MASK) == 0))
			pte = kvtopte(va);
		MPASS(pte == pmap_pte(kernel_pmap, va));

		/*
		 * XXX there has to be a more efficient way than traversing
		 * the page table every time - but go for correctness for
		 * today
		 */

		m = *ma++;
		cache_bits = pmap_cache_bits(m->md.mdpg_cache_attrs);
		pa = VM_PAGE_TO_PHYS(m) | cache_bits | attr_bits;
		if (be64toh(*pte) != pa) {
			oldpte |= be64toh(*pte);
			pte_store(pte, pa);
		}
		va += PAGE_SIZE;
		pte++;
	}
	if (__predict_false((oldpte & RPTE_VALID) != 0))
		pmap_invalidate_range(kernel_pmap, sva, sva + count *
		    PAGE_SIZE);
	else
		ptesync();
}

void
mmu_radix_qremove(vm_offset_t sva, int count)
{
	vm_offset_t va;
	pt_entry_t *pte;

	CTR3(KTR_PMAP, "%s(%#x, %d)", __func__, sva, count);
	KASSERT(sva >= VM_MIN_KERNEL_ADDRESS, ("usermode or dmap va %lx", sva));

	va = sva;
	pte = kvtopte(va);
	while (va < sva + PAGE_SIZE * count) {
		if (__predict_false((va & L3_PAGE_MASK) == 0))
			pte = kvtopte(va);
		pte_clear(pte);
		pte++;
		va += PAGE_SIZE;
	}
	pmap_invalidate_range(kernel_pmap, sva, va);
}

/***************************************************
 * Page table page management routines.....
 ***************************************************/
/*
 * Schedule the specified unused page table page to be freed.  Specifically,
 * add the page to the specified list of pages that will be released to the
 * physical memory manager after the TLB has been updated.
 */
static __inline void
pmap_add_delayed_free_list(vm_page_t m, struct spglist *free,
    boolean_t set_PG_ZERO)
{

	if (set_PG_ZERO)
		m->flags |= PG_ZERO;
	else
		m->flags &= ~PG_ZERO;
	SLIST_INSERT_HEAD(free, m, plinks.s.ss);
}

/*
 * Inserts the specified page table page into the specified pmap's collection
 * of idle page table pages.  Each of a pmap's page table pages is responsible
 * for mapping a distinct range of virtual addresses.  The pmap's collection is
 * ordered by this virtual address range.
 */
static __inline int
pmap_insert_pt_page(pmap_t pmap, vm_page_t mpte)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	return (vm_radix_insert(&pmap->pm_radix, mpte));
}

/*
 * Removes the page table page mapping the specified virtual address from the
 * specified pmap's collection of idle page table pages, and returns it.
 * Otherwise, returns NULL if there is no page table page corresponding to the
 * specified virtual address.
 */
static __inline vm_page_t
pmap_remove_pt_page(pmap_t pmap, vm_offset_t va)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	return (vm_radix_remove(&pmap->pm_radix, pmap_l3e_pindex(va)));
}

/*
 * Decrements a page table page's wire count, which is used to record the
 * number of valid page table entries within the page.  If the wire count
 * drops to zero, then the page table page is unmapped.  Returns TRUE if the
 * page table page was unmapped and FALSE otherwise.
 */
static inline boolean_t
pmap_unwire_ptp(pmap_t pmap, vm_offset_t va, vm_page_t m, struct spglist *free)
{

	--m->ref_count;
	if (m->ref_count == 0) {
		_pmap_unwire_ptp(pmap, va, m, free);
		return (TRUE);
	} else
		return (FALSE);
}

static void
_pmap_unwire_ptp(pmap_t pmap, vm_offset_t va, vm_page_t m, struct spglist *free)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	/*
	 * unmap the page table page
	 */
	if (m->pindex >= NUPDE + NUPDPE) {
		/* PDP page */
		pml1_entry_t *pml1;
		pml1 = pmap_pml1e(pmap, va);
		*pml1 = 0;
	} else if (m->pindex >= NUPDE) {
		/* PD page */
		pml2_entry_t *l2e;
		l2e = pmap_pml2e(pmap, va);
		*l2e = 0;
	} else {
		/* PTE page */
		pml3_entry_t *l3e;
		l3e = pmap_pml3e(pmap, va);
		*l3e = 0;
	}
	pmap_resident_count_dec(pmap, 1);
	if (m->pindex < NUPDE) {
		/* We just released a PT, unhold the matching PD */
		vm_page_t pdpg;

		pdpg = PHYS_TO_VM_PAGE(be64toh(*pmap_pml2e(pmap, va)) & PG_FRAME);
		pmap_unwire_ptp(pmap, va, pdpg, free);
	}
	else if (m->pindex >= NUPDE && m->pindex < (NUPDE + NUPDPE)) {
		/* We just released a PD, unhold the matching PDP */
		vm_page_t pdppg;

		pdppg = PHYS_TO_VM_PAGE(be64toh(*pmap_pml1e(pmap, va)) & PG_FRAME);
		pmap_unwire_ptp(pmap, va, pdppg, free);
	}

	/*
	 * Put page on a list so that it is released after
	 * *ALL* TLB shootdown is done
	 */
	pmap_add_delayed_free_list(m, free, TRUE);
}

/*
 * After removing a page table entry, this routine is used to
 * conditionally free the page, and manage the hold/wire counts.
 */
static int
pmap_unuse_pt(pmap_t pmap, vm_offset_t va, pml3_entry_t ptepde,
    struct spglist *free)
{
	vm_page_t mpte;

	if (va >= VM_MAXUSER_ADDRESS)
		return (0);
	KASSERT(ptepde != 0, ("pmap_unuse_pt: ptepde != 0"));
	mpte = PHYS_TO_VM_PAGE(ptepde & PG_FRAME);
	return (pmap_unwire_ptp(pmap, va, mpte, free));
}

void
mmu_radix_release(pmap_t pmap)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, pmap);
	KASSERT(pmap->pm_stats.resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->pm_stats.resident_count));
	KASSERT(vm_radix_is_empty(&pmap->pm_radix),
	    ("pmap_release: pmap has reserved page table page(s)"));

	pmap_invalidate_all(pmap);
	isa3_proctab[pmap->pm_pid].proctab0 = 0;
	uma_zfree(zone_radix_pgd, pmap->pm_pml1);
	vmem_free(asid_arena, pmap->pm_pid, 1);
}

/*
 * Create the PV entry for a 2MB page mapping.  Always returns true unless the
 * flag PMAP_ENTER_NORECLAIM is specified.  If that flag is specified, returns
 * false if the PV entry cannot be allocated without resorting to reclamation.
 */
static bool
pmap_pv_insert_l3e(pmap_t pmap, vm_offset_t va, pml3_entry_t pde, u_int flags,
    struct rwlock **lockp)
{
	struct md_page *pvh;
	pv_entry_t pv;
	vm_paddr_t pa;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	/* Pass NULL instead of the lock pointer to disable reclamation. */
	if ((pv = get_pv_entry(pmap, (flags & PMAP_ENTER_NORECLAIM) != 0 ?
	    NULL : lockp)) == NULL)
		return (false);
	pv->pv_va = va;
	pa = pde & PG_PS_FRAME;
	CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, pa);
	pvh = pa_to_pvh(pa);
	TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_link);
	pvh->pv_gen++;
	return (true);
}

/*
 * Fills a page table page with mappings to consecutive physical pages.
 */
static void
pmap_fill_ptp(pt_entry_t *firstpte, pt_entry_t newpte)
{
	pt_entry_t *pte;

	for (pte = firstpte; pte < firstpte + NPTEPG; pte++) {
		*pte = htobe64(newpte);
		newpte += PAGE_SIZE;
	}
}

static boolean_t
pmap_demote_l3e(pmap_t pmap, pml3_entry_t *pde, vm_offset_t va)
{
	struct rwlock *lock;
	boolean_t rv;

	lock = NULL;
	rv = pmap_demote_l3e_locked(pmap, pde, va, &lock);
	if (lock != NULL)
		rw_wunlock(lock);
	return (rv);
}

static boolean_t
pmap_demote_l3e_locked(pmap_t pmap, pml3_entry_t *l3e, vm_offset_t va,
    struct rwlock **lockp)
{
	pml3_entry_t oldpde;
	pt_entry_t *firstpte;
	vm_paddr_t mptepa;
	vm_page_t mpte;
	struct spglist free;
	vm_offset_t sva;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	oldpde = be64toh(*l3e);
	KASSERT((oldpde & (RPTE_LEAF | PG_V)) == (RPTE_LEAF | PG_V),
	    ("pmap_demote_l3e: oldpde is missing RPTE_LEAF and/or PG_V %lx",
	    oldpde));
	if ((oldpde & PG_A) == 0 || (mpte = pmap_remove_pt_page(pmap, va)) ==
	    NULL) {
		KASSERT((oldpde & PG_W) == 0,
		    ("pmap_demote_l3e: page table page for a wired mapping"
		    " is missing"));

		/*
		 * Invalidate the 2MB page mapping and return "failure" if the
		 * mapping was never accessed or the allocation of the new
		 * page table page fails.  If the 2MB page mapping belongs to
		 * the direct map region of the kernel's address space, then
		 * the page allocation request specifies the highest possible
		 * priority (VM_ALLOC_INTERRUPT).  Otherwise, the priority is
		 * normal.  Page table pages are preallocated for every other
		 * part of the kernel address space, so the direct map region
		 * is the only part of the kernel address space that must be
		 * handled here.
		 */
		if ((oldpde & PG_A) == 0 || (mpte = vm_page_alloc_noobj(
		    (va >= DMAP_MIN_ADDRESS && va < DMAP_MAX_ADDRESS ?
		    VM_ALLOC_INTERRUPT : 0) | VM_ALLOC_WIRED)) == NULL) {
			SLIST_INIT(&free);
			sva = trunc_2mpage(va);
			pmap_remove_l3e(pmap, l3e, sva, &free, lockp);
			pmap_invalidate_l3e_page(pmap, sva, oldpde);
			vm_page_free_pages_toq(&free, true);
			CTR2(KTR_PMAP, "pmap_demote_l3e: failure for va %#lx"
			    " in pmap %p", va, pmap);
			return (FALSE);
		}
		mpte->pindex = pmap_l3e_pindex(va);
		if (va < VM_MAXUSER_ADDRESS)
			pmap_resident_count_inc(pmap, 1);
	}
	mptepa = VM_PAGE_TO_PHYS(mpte);
	firstpte = (pt_entry_t *)PHYS_TO_DMAP(mptepa);
	KASSERT((oldpde & PG_A) != 0,
	    ("pmap_demote_l3e: oldpde is missing PG_A"));
	KASSERT((oldpde & (PG_M | PG_RW)) != PG_RW,
	    ("pmap_demote_l3e: oldpde is missing PG_M"));

	/*
	 * If the page table page is new, initialize it.
	 */
	if (mpte->ref_count == 1) {
		mpte->ref_count = NPTEPG;
		pmap_fill_ptp(firstpte, oldpde);
	}

	KASSERT((be64toh(*firstpte) & PG_FRAME) == (oldpde & PG_FRAME),
	    ("pmap_demote_l3e: firstpte and newpte map different physical"
	    " addresses"));

	/*
	 * If the mapping has changed attributes, update the page table
	 * entries.
	 */
	if ((be64toh(*firstpte) & PG_PTE_PROMOTE) != (oldpde & PG_PTE_PROMOTE))
		pmap_fill_ptp(firstpte, oldpde);

	/*
	 * The spare PV entries must be reserved prior to demoting the
	 * mapping, that is, prior to changing the PDE.  Otherwise, the state
	 * of the PDE and the PV lists will be inconsistent, which can result
	 * in reclaim_pv_chunk() attempting to remove a PV entry from the
	 * wrong PV list and pmap_pv_demote_l3e() failing to find the expected
	 * PV entry for the 2MB page mapping that is being demoted.
	 */
	if ((oldpde & PG_MANAGED) != 0)
		reserve_pv_entries(pmap, NPTEPG - 1, lockp);

	/*
	 * Demote the mapping.  This pmap is locked.  The old PDE has
	 * PG_A set.  If the old PDE has PG_RW set, it also has PG_M
	 * set.  Thus, there is no danger of a race with another
	 * processor changing the setting of PG_A and/or PG_M between
	 * the read above and the store below.
	 */
	pde_store(l3e, mptepa);
	pmap_invalidate_l3e_page(pmap, trunc_2mpage(va), oldpde);
	/*
	 * Demote the PV entry.
	 */
	if ((oldpde & PG_MANAGED) != 0)
		pmap_pv_demote_l3e(pmap, va, oldpde & PG_PS_FRAME, lockp);

	counter_u64_add(pmap_l3e_demotions, 1);
	CTR2(KTR_PMAP, "pmap_demote_l3e: success for va %#lx"
	    " in pmap %p", va, pmap);
	return (TRUE);
}

/*
 * pmap_remove_kernel_pde: Remove a kernel superpage mapping.
 */
static void
pmap_remove_kernel_l3e(pmap_t pmap, pml3_entry_t *l3e, vm_offset_t va)
{
	vm_paddr_t mptepa;
	vm_page_t mpte;

	KASSERT(pmap == kernel_pmap, ("pmap %p is not kernel_pmap", pmap));
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	mpte = pmap_remove_pt_page(pmap, va);
	if (mpte == NULL)
		panic("pmap_remove_kernel_pde: Missing pt page.");

	mptepa = VM_PAGE_TO_PHYS(mpte);

	/*
	 * Initialize the page table page.
	 */
	pagezero(PHYS_TO_DMAP(mptepa));

	/*
	 * Demote the mapping.
	 */
	pde_store(l3e, mptepa);
	ptesync();
}

/*
 * pmap_remove_l3e: do the things to unmap a superpage in a process
 */
static int
pmap_remove_l3e(pmap_t pmap, pml3_entry_t *pdq, vm_offset_t sva,
    struct spglist *free, struct rwlock **lockp)
{
	struct md_page *pvh;
	pml3_entry_t oldpde;
	vm_offset_t eva, va;
	vm_page_t m, mpte;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((sva & L3_PAGE_MASK) == 0,
	    ("pmap_remove_l3e: sva is not 2mpage aligned"));
	oldpde = be64toh(pte_load_clear(pdq));
	if (oldpde & PG_W)
		pmap->pm_stats.wired_count -= (L3_PAGE_SIZE / PAGE_SIZE);
	pmap_resident_count_dec(pmap, L3_PAGE_SIZE / PAGE_SIZE);
	if (oldpde & PG_MANAGED) {
		CHANGE_PV_LIST_LOCK_TO_PHYS(lockp, oldpde & PG_PS_FRAME);
		pvh = pa_to_pvh(oldpde & PG_PS_FRAME);
		pmap_pvh_free(pvh, pmap, sva);
		eva = sva + L3_PAGE_SIZE;
		for (va = sva, m = PHYS_TO_VM_PAGE(oldpde & PG_PS_FRAME);
		    va < eva; va += PAGE_SIZE, m++) {
			if ((oldpde & (PG_M | PG_RW)) == (PG_M | PG_RW))
				vm_page_dirty(m);
			if (oldpde & PG_A)
				vm_page_aflag_set(m, PGA_REFERENCED);
			if (TAILQ_EMPTY(&m->md.pv_list) &&
			    TAILQ_EMPTY(&pvh->pv_list))
				vm_page_aflag_clear(m, PGA_WRITEABLE);
		}
	}
	if (pmap == kernel_pmap) {
		pmap_remove_kernel_l3e(pmap, pdq, sva);
	} else {
		mpte = pmap_remove_pt_page(pmap, sva);
		if (mpte != NULL) {
			pmap_resident_count_dec(pmap, 1);
			KASSERT(mpte->ref_count == NPTEPG,
			    ("pmap_remove_l3e: pte page wire count error"));
			mpte->ref_count = 0;
			pmap_add_delayed_free_list(mpte, free, FALSE);
		}
	}
	return (pmap_unuse_pt(pmap, sva, be64toh(*pmap_pml2e(pmap, sva)), free));
}

/*
 * pmap_remove_pte: do the things to unmap a page in a process
 */
static int
pmap_remove_pte(pmap_t pmap, pt_entry_t *ptq, vm_offset_t va,
    pml3_entry_t ptepde, struct spglist *free, struct rwlock **lockp)
{
	struct md_page *pvh;
	pt_entry_t oldpte;
	vm_page_t m;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	oldpte = be64toh(pte_load_clear(ptq));
	if (oldpte & RPTE_WIRED)
		pmap->pm_stats.wired_count -= 1;
	pmap_resident_count_dec(pmap, 1);
	if (oldpte & RPTE_MANAGED) {
		m = PHYS_TO_VM_PAGE(oldpte & PG_FRAME);
		if ((oldpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
			vm_page_dirty(m);
		if (oldpte & PG_A)
			vm_page_aflag_set(m, PGA_REFERENCED);
		CHANGE_PV_LIST_LOCK_TO_VM_PAGE(lockp, m);
		pmap_pvh_free(&m->md, pmap, va);
		if (TAILQ_EMPTY(&m->md.pv_list) &&
		    (m->flags & PG_FICTITIOUS) == 0) {
			pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
			if (TAILQ_EMPTY(&pvh->pv_list))
				vm_page_aflag_clear(m, PGA_WRITEABLE);
		}
	}
	return (pmap_unuse_pt(pmap, va, ptepde, free));
}

/*
 * Remove a single page from a process address space
 */
static bool
pmap_remove_page(pmap_t pmap, vm_offset_t va, pml3_entry_t *l3e,
    struct spglist *free)
{
	struct rwlock *lock;
	pt_entry_t *pte;
	bool invalidate_all;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if ((be64toh(*l3e) & RPTE_VALID) == 0) {
		return (false);
	}
	pte = pmap_l3e_to_pte(l3e, va);
	if ((be64toh(*pte) & RPTE_VALID) == 0) {
		return (false);
	}
	lock = NULL;

	invalidate_all = pmap_remove_pte(pmap, pte, va, be64toh(*l3e), free, &lock);
	if (lock != NULL)
		rw_wunlock(lock);
	if (!invalidate_all)
		pmap_invalidate_page(pmap, va);
	return (invalidate_all);
}

/*
 * Removes the specified range of addresses from the page table page.
 */
static bool
pmap_remove_ptes(pmap_t pmap, vm_offset_t sva, vm_offset_t eva,
    pml3_entry_t *l3e, struct spglist *free, struct rwlock **lockp)
{
	pt_entry_t *pte;
	vm_offset_t va;
	bool anyvalid;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	anyvalid = false;
	va = eva;
	for (pte = pmap_l3e_to_pte(l3e, sva); sva != eva; pte++,
	    sva += PAGE_SIZE) {
		MPASS(pte == pmap_pte(pmap, sva));
		if (*pte == 0) {
			if (va != eva) {
				anyvalid = true;
				va = eva;
			}
			continue;
		}
		if (va == eva)
			va = sva;
		if (pmap_remove_pte(pmap, pte, sva, be64toh(*l3e), free, lockp)) {
			anyvalid = true;
			sva += PAGE_SIZE;
			break;
		}
	}
	if (anyvalid)
		pmap_invalidate_all(pmap);
	else if (va != eva)
		pmap_invalidate_range(pmap, va, sva);
	return (anyvalid);
}

void
mmu_radix_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	struct rwlock *lock;
	vm_offset_t va_next;
	pml1_entry_t *l1e;
	pml2_entry_t *l2e;
	pml3_entry_t ptpaddr, *l3e;
	struct spglist free;
	bool anyvalid;

	CTR4(KTR_PMAP, "%s(%p, %#x, %#x)", __func__, pmap, sva, eva);

	/*
	 * Perform an unsynchronized read.  This is, however, safe.
	 */
	if (pmap->pm_stats.resident_count == 0)
		return;

	anyvalid = false;
	SLIST_INIT(&free);

	/* XXX something fishy here */
	sva = (sva + PAGE_MASK) & ~PAGE_MASK;
	eva = (eva + PAGE_MASK) & ~PAGE_MASK;

	PMAP_LOCK(pmap);

	/*
	 * special handling of removing one page.  a very
	 * common operation and easy to short circuit some
	 * code.
	 */
	if (sva + PAGE_SIZE == eva) {
		l3e = pmap_pml3e(pmap, sva);
		if (l3e && (be64toh(*l3e) & RPTE_LEAF) == 0) {
			anyvalid = pmap_remove_page(pmap, sva, l3e, &free);
			goto out;
		}
	}

	lock = NULL;
	for (; sva < eva; sva = va_next) {
		if (pmap->pm_stats.resident_count == 0)
			break;
		l1e = pmap_pml1e(pmap, sva);
		if (l1e == NULL || (be64toh(*l1e) & PG_V) == 0) {
			va_next = (sva + L1_PAGE_SIZE) & ~L1_PAGE_MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		l2e = pmap_l1e_to_l2e(l1e, sva);
		if (l2e == NULL || (be64toh(*l2e) & PG_V) == 0) {
			va_next = (sva + L2_PAGE_SIZE) & ~L2_PAGE_MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}

		/*
		 * Calculate index for next page table.
		 */
		va_next = (sva + L3_PAGE_SIZE) & ~L3_PAGE_MASK;
		if (va_next < sva)
			va_next = eva;

		l3e = pmap_l2e_to_l3e(l2e, sva);
		ptpaddr = be64toh(*l3e);

		/*
		 * Weed out invalid mappings.
		 */
		if (ptpaddr == 0)
			continue;

		/*
		 * Check for large page.
		 */
		if ((ptpaddr & RPTE_LEAF) != 0) {
			/*
			 * Are we removing the entire large page?  If not,
			 * demote the mapping and fall through.
			 */
			if (sva + L3_PAGE_SIZE == va_next && eva >= va_next) {
				pmap_remove_l3e(pmap, l3e, sva, &free, &lock);
				anyvalid = true;
				continue;
			} else if (!pmap_demote_l3e_locked(pmap, l3e, sva,
			    &lock)) {
				/* The large page mapping was destroyed. */
				continue;
			} else
				ptpaddr = be64toh(*l3e);
		}

		/*
		 * Limit our scan to either the end of the va represented
		 * by the current page table page, or to the end of the
		 * range being removed.
		 */
		if (va_next > eva)
			va_next = eva;

		if (pmap_remove_ptes(pmap, sva, va_next, l3e, &free, &lock))
			anyvalid = true;
	}
	if (lock != NULL)
		rw_wunlock(lock);
out:
	if (anyvalid)
		pmap_invalidate_all(pmap);
	PMAP_UNLOCK(pmap);
	vm_page_free_pages_toq(&free, true);
}

void
mmu_radix_remove_all(vm_page_t m)
{
	struct md_page *pvh;
	pv_entry_t pv;
	pmap_t pmap;
	struct rwlock *lock;
	pt_entry_t *pte, tpte;
	pml3_entry_t *l3e;
	vm_offset_t va;
	struct spglist free;
	int pvh_gen, md_gen;

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_all: page %p is not managed", m));
	SLIST_INIT(&free);
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	pvh = (m->flags & PG_FICTITIOUS) != 0 ? &pv_dummy :
	    pa_to_pvh(VM_PAGE_TO_PHYS(m));
retry:
	rw_wlock(lock);
	while ((pv = TAILQ_FIRST(&pvh->pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen) {
				rw_wunlock(lock);
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		va = pv->pv_va;
		l3e = pmap_pml3e(pmap, va);
		(void)pmap_demote_l3e_locked(pmap, l3e, va, &lock);
		PMAP_UNLOCK(pmap);
	}
	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			md_gen = m->md.pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen || md_gen != m->md.pv_gen) {
				rw_wunlock(lock);
				PMAP_UNLOCK(pmap);
				goto retry;
			}
		}
		pmap_resident_count_dec(pmap, 1);
		l3e = pmap_pml3e(pmap, pv->pv_va);
		KASSERT((be64toh(*l3e) & RPTE_LEAF) == 0, ("pmap_remove_all: found"
		    " a 2mpage in page %p's pv list", m));
		pte = pmap_l3e_to_pte(l3e, pv->pv_va);
		tpte = be64toh(pte_load_clear(pte));
		if (tpte & PG_W)
			pmap->pm_stats.wired_count--;
		if (tpte & PG_A)
			vm_page_aflag_set(m, PGA_REFERENCED);

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
			vm_page_dirty(m);
		pmap_unuse_pt(pmap, pv->pv_va, be64toh(*l3e), &free);
		pmap_invalidate_page(pmap, pv->pv_va);
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_link);
		m->md.pv_gen++;
		free_pv_entry(pmap, pv);
		PMAP_UNLOCK(pmap);
	}
	vm_page_aflag_clear(m, PGA_WRITEABLE);
	rw_wunlock(lock);
	vm_page_free_pages_toq(&free, true);
}

/*
 * Destroy all managed, non-wired mappings in the given user-space
 * pmap.  This pmap cannot be active on any processor besides the
 * caller.
 *
 * This function cannot be applied to the kernel pmap.  Moreover, it
 * is not intended for general use.  It is only to be used during
 * process termination.  Consequently, it can be implemented in ways
 * that make it faster than pmap_remove().  First, it can more quickly
 * destroy mappings by iterating over the pmap's collection of PV
 * entries, rather than searching the page table.  Second, it doesn't
 * have to test and clear the page table entries atomically, because
 * no processor is currently accessing the user address space.  In
 * particular, a page table entry's dirty bit won't change state once
 * this function starts.
 *
 * Although this function destroys all of the pmap's managed,
 * non-wired mappings, it can delay and batch the invalidation of TLB
 * entries without calling pmap_delayed_invl_started() and
 * pmap_delayed_invl_finished().  Because the pmap is not active on
 * any other processor, none of these TLB entries will ever be used
 * before their eventual invalidation.  Consequently, there is no need
 * for either pmap_remove_all() or pmap_remove_write() to wait for
 * that eventual TLB invalidation.
 */

void
mmu_radix_remove_pages(pmap_t pmap)
{

	CTR2(KTR_PMAP, "%s(%p)", __func__, pmap);
	pml3_entry_t ptel3e;
	pt_entry_t *pte, tpte;
	struct spglist free;
	vm_page_t m, mpte, mt;
	pv_entry_t pv;
	struct md_page *pvh;
	struct pv_chunk *pc, *npc;
	struct rwlock *lock;
	int64_t bit;
	uint64_t inuse, bitmask;
	int allfree, field, idx;
#ifdef PV_STATS
	int freed;
#endif
	boolean_t superpage;
	vm_paddr_t pa;

	/*
	 * Assert that the given pmap is only active on the current
	 * CPU.  Unfortunately, we cannot block another CPU from
	 * activating the pmap while this function is executing.
	 */
	KASSERT(pmap->pm_pid == mfspr(SPR_PID),
	    ("non-current asid %lu - expected %lu", pmap->pm_pid,
	    mfspr(SPR_PID)));

	lock = NULL;

	SLIST_INIT(&free);
	PMAP_LOCK(pmap);
	TAILQ_FOREACH_SAFE(pc, &pmap->pm_pvchunk, pc_list, npc) {
		allfree = 1;
#ifdef PV_STATS
		freed = 0;
#endif
		for (field = 0; field < _NPCM; field++) {
			inuse = ~pc->pc_map[field] & pc_freemask[field];
			while (inuse != 0) {
				bit = cnttzd(inuse);
				bitmask = 1UL << bit;
				idx = field * 64 + bit;
				pv = &pc->pc_pventry[idx];
				inuse &= ~bitmask;

				pte = pmap_pml2e(pmap, pv->pv_va);
				ptel3e = be64toh(*pte);
				pte = pmap_l2e_to_l3e(pte, pv->pv_va);
				tpte = be64toh(*pte);
				if ((tpte & (RPTE_LEAF | PG_V)) == PG_V) {
					superpage = FALSE;
					ptel3e = tpte;
					pte = (pt_entry_t *)PHYS_TO_DMAP(tpte &
					    PG_FRAME);
					pte = &pte[pmap_pte_index(pv->pv_va)];
					tpte = be64toh(*pte);
				} else {
					/*
					 * Keep track whether 'tpte' is a
					 * superpage explicitly instead of
					 * relying on RPTE_LEAF being set.
					 *
					 * This is because RPTE_LEAF is numerically
					 * identical to PG_PTE_PAT and thus a
					 * regular page could be mistaken for
					 * a superpage.
					 */
					superpage = TRUE;
				}

				if ((tpte & PG_V) == 0) {
					panic("bad pte va %lx pte %lx",
					    pv->pv_va, tpte);
				}

/*
 * We cannot remove wired pages from a process' mapping at this time
 */
				if (tpte & PG_W) {
					allfree = 0;
					continue;
				}

				if (superpage)
					pa = tpte & PG_PS_FRAME;
				else
					pa = tpte & PG_FRAME;

				m = PHYS_TO_VM_PAGE(pa);
				KASSERT(m->phys_addr == pa,
				    ("vm_page_t %p phys_addr mismatch %016jx %016jx",
				    m, (uintmax_t)m->phys_addr,
				    (uintmax_t)tpte));

				KASSERT((m->flags & PG_FICTITIOUS) != 0 ||
				    m < &vm_page_array[vm_page_array_size],
				    ("pmap_remove_pages: bad tpte %#jx",
				    (uintmax_t)tpte));

				pte_clear(pte);

				/*
				 * Update the vm_page_t clean/reference bits.
				 */
				if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
					if (superpage) {
						for (mt = m; mt < &m[L3_PAGE_SIZE / PAGE_SIZE]; mt++)
							vm_page_dirty(mt);
					} else
						vm_page_dirty(m);
				}

				CHANGE_PV_LIST_LOCK_TO_VM_PAGE(&lock, m);

				/* Mark free */
				pc->pc_map[field] |= bitmask;
				if (superpage) {
					pmap_resident_count_dec(pmap, L3_PAGE_SIZE / PAGE_SIZE);
					pvh = pa_to_pvh(tpte & PG_PS_FRAME);
					TAILQ_REMOVE(&pvh->pv_list, pv, pv_link);
					pvh->pv_gen++;
					if (TAILQ_EMPTY(&pvh->pv_list)) {
						for (mt = m; mt < &m[L3_PAGE_SIZE / PAGE_SIZE]; mt++)
							if ((mt->a.flags & PGA_WRITEABLE) != 0 &&
							    TAILQ_EMPTY(&mt->md.pv_list))
								vm_page_aflag_clear(mt, PGA_WRITEABLE);
					}
					mpte = pmap_remove_pt_page(pmap, pv->pv_va);
					if (mpte != NULL) {
						pmap_resident_count_dec(pmap, 1);
						KASSERT(mpte->ref_count == NPTEPG,
						    ("pmap_remove_pages: pte page wire count error"));
						mpte->ref_count = 0;
						pmap_add_delayed_free_list(mpte, &free, FALSE);
					}
				} else {
					pmap_resident_count_dec(pmap, 1);
#ifdef VERBOSE_PV
					printf("freeing pv (%p, %p)\n",
						   pmap, pv);
#endif
					TAILQ_REMOVE(&m->md.pv_list, pv, pv_link);
					m->md.pv_gen++;
					if ((m->a.flags & PGA_WRITEABLE) != 0 &&
					    TAILQ_EMPTY(&m->md.pv_list) &&
					    (m->flags & PG_FICTITIOUS) == 0) {
						pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
						if (TAILQ_EMPTY(&pvh->pv_list))
							vm_page_aflag_clear(m, PGA_WRITEABLE);
					}
				}
				pmap_unuse_pt(pmap, pv->pv_va, ptel3e, &free);
#ifdef PV_STATS
				freed++;
#endif
			}
		}
		PV_STAT(atomic_add_long(&pv_entry_frees, freed));
		PV_STAT(atomic_add_int(&pv_entry_spare, freed));
		PV_STAT(atomic_subtract_long(&pv_entry_count, freed));
		if (allfree) {
			TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
			free_pv_chunk(pc);
		}
	}
	if (lock != NULL)
		rw_wunlock(lock);
	pmap_invalidate_all(pmap);
	PMAP_UNLOCK(pmap);
	vm_page_free_pages_toq(&free, true);
}

void
mmu_radix_remove_write(vm_page_t m)
{
	struct md_page *pvh;
	pmap_t pmap;
	struct rwlock *lock;
	pv_entry_t next_pv, pv;
	pml3_entry_t *l3e;
	pt_entry_t oldpte, *pte;
	int pvh_gen, md_gen;

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_write: page %p is not managed", m));
	vm_page_assert_busied(m);

	if (!pmap_page_is_write_mapped(m))
		return;
	lock = VM_PAGE_TO_PV_LIST_LOCK(m);
	pvh = (m->flags & PG_FICTITIOUS) != 0 ? &pv_dummy :
	    pa_to_pvh(VM_PAGE_TO_PHYS(m));
retry_pv_loop:
	rw_wlock(lock);
	TAILQ_FOREACH_SAFE(pv, &pvh->pv_list, pv_link, next_pv) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen) {
				PMAP_UNLOCK(pmap);
				rw_wunlock(lock);
				goto retry_pv_loop;
			}
		}
		l3e = pmap_pml3e(pmap, pv->pv_va);
		if ((be64toh(*l3e) & PG_RW) != 0)
			(void)pmap_demote_l3e_locked(pmap, l3e, pv->pv_va, &lock);
		KASSERT(lock == VM_PAGE_TO_PV_LIST_LOCK(m),
		    ("inconsistent pv lock %p %p for page %p",
		    lock, VM_PAGE_TO_PV_LIST_LOCK(m), m));
		PMAP_UNLOCK(pmap);
	}
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_link) {
		pmap = PV_PMAP(pv);
		if (!PMAP_TRYLOCK(pmap)) {
			pvh_gen = pvh->pv_gen;
			md_gen = m->md.pv_gen;
			rw_wunlock(lock);
			PMAP_LOCK(pmap);
			rw_wlock(lock);
			if (pvh_gen != pvh->pv_gen ||
			    md_gen != m->md.pv_gen) {
				PMAP_UNLOCK(pmap);
				rw_wunlock(lock);
				goto retry_pv_loop;
			}
		}
		l3e = pmap_pml3e(pmap, pv->pv_va);
		KASSERT((be64toh(*l3e) & RPTE_LEAF) == 0,
		    ("pmap_remove_write: found a 2mpage in page %p's pv list",
		    m));
		pte = pmap_l3e_to_pte(l3e, pv->pv_va);
retry:
		oldpte = be64toh(*pte);
		if (oldpte & PG_RW) {
			if (!atomic_cmpset_long(pte, htobe64(oldpte),
			    htobe64((oldpte | RPTE_EAA_R) & ~(PG_RW | PG_M))))
				goto retry;
			if ((oldpte & PG_M) != 0)
				vm_page_dirty(m);
			pmap_invalidate_page(pmap, pv->pv_va);
		}
		PMAP_UNLOCK(pmap);
	}
	rw_wunlock(lock);
	vm_page_aflag_clear(m, PGA_WRITEABLE);
}

/*
 *	Clear the wired attribute from the mappings for the specified range of
 *	addresses in the given pmap.  Every valid mapping within that range
 *	must have the wired attribute set.  In contrast, invalid mappings
 *	cannot have the wired attribute set, so they are ignored.
 *
 *	The wired attribute of the page table entry is not a hardware
 *	feature, so there is no need to invalidate any TLB entries.
 *	Since pmap_demote_l3e() for the wired entry must never fail,
 *	pmap_delayed_invl_started()/finished() calls around the
 *	function are not needed.
 */
void
mmu_radix_unwire(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	vm_offset_t va_next;
	pml1_entry_t *l1e;
	pml2_entry_t *l2e;
	pml3_entry_t *l3e;
	pt_entry_t *pte;

	CTR4(KTR_PMAP, "%s(%p, %#x, %#x)", __func__, pmap, sva, eva);
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = va_next) {
		l1e = pmap_pml1e(pmap, sva);
		if ((be64toh(*l1e) & PG_V) == 0) {
			va_next = (sva + L1_PAGE_SIZE) & ~L1_PAGE_MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
		l2e = pmap_l1e_to_l2e(l1e, sva);
		if ((be64toh(*l2e) & PG_V) == 0) {
			va_next = (sva + L2_PAGE_SIZE) & ~L2_PAGE_MASK;
			if (va_next < sva)
				va_next = eva;
			continue;
		}
		va_next = (sva + L3_PAGE_SIZE) & ~L3_PAGE_MASK;
		if (va_next < sva)
			va_next = eva;
		l3e = pmap_l2e_to_l3e(l2e, sva);
		if ((be64toh(*l3e) & PG_V) == 0)
			continue;
		if ((be64toh(*l3e) & RPTE_LEAF) != 0) {
			if ((be64toh(*l3e) & PG_W) == 0)
				panic("pmap_unwire: pde %#jx is missing PG_W",
				    (uintmax_t)(be64toh(*l3e)));

			/*
			 * Are we unwiring the entire large page?  If not,
			 * demote the mapping and fall through.
			 */
			if (sva + L3_PAGE_SIZE == va_next && eva >= va_next) {
				atomic_clear_long(l3e, htobe64(PG_W));
				pmap->pm_stats.wired_count -= L3_PAGE_SIZE /
				    PAGE_SIZE;
				continue;
			} else if (!pmap_demote_l3e(pmap, l3e, sva))
				panic("pmap_unwire: demotion failed");
		}
		if (va_next > eva)
			va_next = eva;
		for (pte = pmap_l3e_to_pte(l3e, sva); sva != va_next; pte++,
		    sva += PAGE_SIZE) {
			MPASS(pte == pmap_pte(pmap, sva));
			if ((be64toh(*pte) & PG_V) == 0)
				continue;
			if ((be64toh(*pte) & PG_W) == 0)
				panic("pmap_unwire: pte %#jx is missing PG_W",
				    (uintmax_t)(be64toh(*pte)));

			/*
			 * PG_W must be cleared atomically.  Although the pmap
			 * lock synchronizes access to PG_W, another processor
			 * could be setting PG_M and/or PG_A concurrently.
			 */
			atomic_clear_long(pte, htobe64(PG_W));
			pmap->pm_stats.wired_count--;
		}
	}
	PMAP_UNLOCK(pmap);
}

void
mmu_radix_zero_page(vm_page_t m)
{
	vm_offset_t addr;

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	addr = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));
	pagezero(addr);
}

void
mmu_radix_zero_page_area(vm_page_t m, int off, int size)
{
	caddr_t addr;

	CTR4(KTR_PMAP, "%s(%p, %d, %d)", __func__, m, off, size);
	MPASS(off + size <= PAGE_SIZE);
	addr = (caddr_t)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));
	memset(addr + off, 0, size);
}

static int
mmu_radix_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{
	pml3_entry_t *l3ep;
	pt_entry_t pte;
	vm_paddr_t pa;
	int val;

	CTR3(KTR_PMAP, "%s(%p, %#x)", __func__, pmap, addr);
	PMAP_LOCK(pmap);

	l3ep = pmap_pml3e(pmap, addr);
	if (l3ep != NULL && (be64toh(*l3ep) & PG_V)) {
		if (be64toh(*l3ep) & RPTE_LEAF) {
			pte = be64toh(*l3ep);
			/* Compute the physical address of the 4KB page. */
			pa = ((be64toh(*l3ep) & PG_PS_FRAME) | (addr & L3_PAGE_MASK)) &
			    PG_FRAME;
			val = MINCORE_PSIND(1);
		} else {
			/* Native endian PTE, do not pass to functions */
			pte = be64toh(*pmap_l3e_to_pte(l3ep, addr));
			pa = pte & PG_FRAME;
			val = 0;
		}
	} else {
		pte = 0;
		pa = 0;
		val = 0;
	}
	if ((pte & PG_V) != 0) {
		val |= MINCORE_INCORE;
		if ((pte & (PG_M | PG_RW)) == (PG_M | PG_RW))
			val |= MINCORE_MODIFIED | MINCORE_MODIFIED_OTHER;
		if ((pte & PG_A) != 0)
			val |= MINCORE_REFERENCED | MINCORE_REFERENCED_OTHER;
	}
	if ((val & (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER)) !=
	    (MINCORE_MODIFIED_OTHER | MINCORE_REFERENCED_OTHER) &&
	    (pte & (PG_MANAGED | PG_V)) == (PG_MANAGED | PG_V)) {
		*locked_pa = pa;
	}
	PMAP_UNLOCK(pmap);
	return (val);
}

void
mmu_radix_activate(struct thread *td)
{
	pmap_t pmap;
	uint32_t curpid;

	CTR2(KTR_PMAP, "%s(%p)", __func__, td);
	critical_enter();
	pmap = vmspace_pmap(td->td_proc->p_vmspace);
	curpid = mfspr(SPR_PID);
	if (pmap->pm_pid > isa3_base_pid &&
		curpid != pmap->pm_pid) {
		mmu_radix_pid_set(pmap);
	}
	critical_exit();
}

/*
 *	Increase the starting virtual address of the given mapping if a
 *	different alignment might result in more superpage mappings.
 */
void
mmu_radix_align_superpage(vm_object_t object, vm_ooffset_t offset,
    vm_offset_t *addr, vm_size_t size)
{

	CTR5(KTR_PMAP, "%s(%p, %#x, %p, %#x)", __func__, object, offset, addr,
	    size);
	vm_offset_t superpage_offset;

	if (size < L3_PAGE_SIZE)
		return;
	if (object != NULL && (object->flags & OBJ_COLORED) != 0)
		offset += ptoa(object->pg_color);
	superpage_offset = offset & L3_PAGE_MASK;
	if (size - ((L3_PAGE_SIZE - superpage_offset) & L3_PAGE_MASK) < L3_PAGE_SIZE ||
	    (*addr & L3_PAGE_MASK) == superpage_offset)
		return;
	if ((*addr & L3_PAGE_MASK) < superpage_offset)
		*addr = (*addr & ~L3_PAGE_MASK) + superpage_offset;
	else
		*addr = ((*addr + L3_PAGE_MASK) & ~L3_PAGE_MASK) + superpage_offset;
}

static void *
mmu_radix_mapdev_attr(vm_paddr_t pa, vm_size_t size, vm_memattr_t attr)
{
	vm_offset_t va, tmpva, ppa, offset;

	ppa = trunc_page(pa);
	offset = pa & PAGE_MASK;
	size = roundup2(offset + size, PAGE_SIZE);
	if (pa < powerpc_ptob(Maxmem))
		panic("bad pa: %#lx less than Maxmem %#lx\n",
			  pa, powerpc_ptob(Maxmem));
	va = kva_alloc(size);
	if (bootverbose)
		printf("%s(%#lx, %lu, %d)\n", __func__, pa, size, attr);
	KASSERT(size > 0, ("%s(%#lx, %lu, %d)", __func__, pa, size, attr));

	if (!va)
		panic("%s: Couldn't alloc kernel virtual memory", __func__);

	for (tmpva = va; size > 0;) {
		mmu_radix_kenter_attr(tmpva, ppa, attr);
		size -= PAGE_SIZE;
		tmpva += PAGE_SIZE;
		ppa += PAGE_SIZE;
	}
	ptesync();

	return ((void *)(va + offset));
}

static void *
mmu_radix_mapdev(vm_paddr_t pa, vm_size_t size)
{

	CTR3(KTR_PMAP, "%s(%#x, %#x)", __func__, pa, size);

	return (mmu_radix_mapdev_attr(pa, size, VM_MEMATTR_DEFAULT));
}

void
mmu_radix_page_set_memattr(vm_page_t m, vm_memattr_t ma)
{

	CTR3(KTR_PMAP, "%s(%p, %#x)", __func__, m, ma);
	m->md.mdpg_cache_attrs = ma;

	/*
	 * If "m" is a normal page, update its direct mapping.  This update
	 * can be relied upon to perform any cache operations that are
	 * required for data coherence.
	 */
	if ((m->flags & PG_FICTITIOUS) == 0 &&
	    mmu_radix_change_attr(PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m)),
	    PAGE_SIZE, m->md.mdpg_cache_attrs))
		panic("memory attribute change on the direct map failed");
}

static void
mmu_radix_unmapdev(void *p, vm_size_t size)
{
	vm_offset_t offset, va;

	CTR3(KTR_PMAP, "%s(%p, %#x)", __func__, p, size);

	/* If we gave a direct map region in pmap_mapdev, do nothing */
	va = (vm_offset_t)p;
	if (va >= DMAP_MIN_ADDRESS && va < DMAP_MAX_ADDRESS)
		return;

	offset = va & PAGE_MASK;
	size = round_page(offset + size);
	va = trunc_page(va);

	if (pmap_initialized) {
		mmu_radix_qremove(va, atop(size));
		kva_free(va, size);
	}
}

void
mmu_radix_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz)
{
	vm_paddr_t pa = 0;
	int sync_sz;

	if (__predict_false(pm == NULL))
		pm = &curthread->td_proc->p_vmspace->vm_pmap;

	while (sz > 0) {
		pa = pmap_extract(pm, va);
		sync_sz = PAGE_SIZE - (va & PAGE_MASK);
		sync_sz = min(sync_sz, sz);
		if (pa != 0) {
			pa += (va & PAGE_MASK);
			__syncicache((void *)PHYS_TO_DMAP(pa), sync_sz);
		}
		va += sync_sz;
		sz -= sync_sz;
	}
}

static __inline void
pmap_pte_attr(pt_entry_t *pte, uint64_t cache_bits, uint64_t mask)
{
	uint64_t opte, npte;

	/*
	 * The cache mode bits are all in the low 32-bits of the
	 * PTE, so we can just spin on updating the low 32-bits.
	 */
	do {
		opte = be64toh(*pte);
		npte = opte & ~mask;
		npte |= cache_bits;
	} while (npte != opte && !atomic_cmpset_long(pte, htobe64(opte), htobe64(npte)));
}

/*
 * Tries to demote a 1GB page mapping.
 */
static boolean_t
pmap_demote_l2e(pmap_t pmap, pml2_entry_t *l2e, vm_offset_t va)
{
	pml2_entry_t oldpdpe;
	pml3_entry_t *firstpde, newpde, *pde;
	vm_paddr_t pdpgpa;
	vm_page_t pdpg;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	oldpdpe = be64toh(*l2e);
	KASSERT((oldpdpe & (RPTE_LEAF | PG_V)) == (RPTE_LEAF | PG_V),
	    ("pmap_demote_pdpe: oldpdpe is missing PG_PS and/or PG_V"));
	pdpg = vm_page_alloc_noobj(VM_ALLOC_INTERRUPT | VM_ALLOC_WIRED);
	if (pdpg == NULL) {
		CTR2(KTR_PMAP, "pmap_demote_pdpe: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return (FALSE);
	}
	pdpg->pindex = va >> L2_PAGE_SIZE_SHIFT;
	pdpgpa = VM_PAGE_TO_PHYS(pdpg);
	firstpde = (pml3_entry_t *)PHYS_TO_DMAP(pdpgpa);
	KASSERT((oldpdpe & PG_A) != 0,
	    ("pmap_demote_pdpe: oldpdpe is missing PG_A"));
	KASSERT((oldpdpe & (PG_M | PG_RW)) != PG_RW,
	    ("pmap_demote_pdpe: oldpdpe is missing PG_M"));
	newpde = oldpdpe;

	/*
	 * Initialize the page directory page.
	 */
	for (pde = firstpde; pde < firstpde + NPDEPG; pde++) {
		*pde = htobe64(newpde);
		newpde += L3_PAGE_SIZE;
	}

	/*
	 * Demote the mapping.
	 */
	pde_store(l2e, pdpgpa);

	/*
	 * Flush PWC --- XXX revisit
	 */
	pmap_invalidate_all(pmap);

	counter_u64_add(pmap_l2e_demotions, 1);
	CTR2(KTR_PMAP, "pmap_demote_pdpe: success for va %#lx"
	    " in pmap %p", va, pmap);
	return (TRUE);
}

vm_paddr_t
mmu_radix_kextract(vm_offset_t va)
{
	pml3_entry_t l3e;
	vm_paddr_t pa;

	CTR2(KTR_PMAP, "%s(%#x)", __func__, va);
	if (va >= DMAP_MIN_ADDRESS && va < DMAP_MAX_ADDRESS) {
		pa = DMAP_TO_PHYS(va);
	} else {
		/* Big-endian PTE on stack */
		l3e = *pmap_pml3e(kernel_pmap, va);
		if (be64toh(l3e) & RPTE_LEAF) {
			pa = (be64toh(l3e) & PG_PS_FRAME) | (va & L3_PAGE_MASK);
			pa |= (va & L3_PAGE_MASK);
		} else {
			/*
			 * Beware of a concurrent promotion that changes the
			 * PDE at this point!  For example, vtopte() must not
			 * be used to access the PTE because it would use the
			 * new PDE.  It is, however, safe to use the old PDE
			 * because the page table page is preserved by the
			 * promotion.
			 */
			pa = be64toh(*pmap_l3e_to_pte(&l3e, va));
			pa = (pa & PG_FRAME) | (va & PAGE_MASK);
			pa |= (va & PAGE_MASK);
		}
	}
	return (pa);
}

static pt_entry_t
mmu_radix_calc_wimg(vm_paddr_t pa, vm_memattr_t ma)
{

	if (ma != VM_MEMATTR_DEFAULT) {
		return pmap_cache_bits(ma);
	}

	/*
	 * Assume the page is cache inhibited and access is guarded unless
	 * it's in our available memory array.
	 */
	for (int i = 0; i < pregions_sz; i++) {
		if ((pa >= pregions[i].mr_start) &&
		    (pa < (pregions[i].mr_start + pregions[i].mr_size)))
			return (RPTE_ATTR_MEM);
	}
	return (RPTE_ATTR_GUARDEDIO);
}

static void
mmu_radix_kenter_attr(vm_offset_t va, vm_paddr_t pa, vm_memattr_t ma)
{
	pt_entry_t *pte, pteval;
	uint64_t cache_bits;

	pte = kvtopte(va);
	MPASS(pte != NULL);
	pteval = pa | RPTE_EAA_R | RPTE_EAA_W | RPTE_EAA_P | PG_M | PG_A;
	cache_bits = mmu_radix_calc_wimg(pa, ma);
	pte_store(pte, pteval | cache_bits);
}

void
mmu_radix_kremove(vm_offset_t va)
{
	pt_entry_t *pte;

	CTR2(KTR_PMAP, "%s(%#x)", __func__, va);

	pte = kvtopte(va);
	pte_clear(pte);
}

int
mmu_radix_decode_kernel_ptr(vm_offset_t addr,
    int *is_user, vm_offset_t *decoded)
{

	CTR2(KTR_PMAP, "%s(%#jx)", __func__, (uintmax_t)addr);
	*decoded = addr;
	*is_user = (addr < VM_MAXUSER_ADDRESS);
	return (0);
}

static int
mmu_radix_dev_direct_mapped(vm_paddr_t pa, vm_size_t size)
{

	CTR3(KTR_PMAP, "%s(%#x, %#x)", __func__, pa, size);
	return (mem_valid(pa, size));
}

static void
mmu_radix_scan_init(void)
{

	CTR1(KTR_PMAP, "%s()", __func__);
	UNIMPLEMENTED();
}

static void
mmu_radix_dumpsys_map(vm_paddr_t pa, size_t sz,
	void **va)
{
	CTR4(KTR_PMAP, "%s(%#jx, %#zx, %p)", __func__, (uintmax_t)pa, sz, va);
	UNIMPLEMENTED();
}

vm_offset_t
mmu_radix_quick_enter_page(vm_page_t m)
{
	vm_paddr_t paddr;

	CTR2(KTR_PMAP, "%s(%p)", __func__, m);
	paddr = VM_PAGE_TO_PHYS(m);
	return (PHYS_TO_DMAP(paddr));
}

void
mmu_radix_quick_remove_page(vm_offset_t addr __unused)
{
	/* no work to do here */
	CTR2(KTR_PMAP, "%s(%#x)", __func__, addr);
}

static void
pmap_invalidate_cache_range(vm_offset_t sva, vm_offset_t eva)
{
	cpu_flush_dcache((void *)sva, eva - sva);
}

int
mmu_radix_change_attr(vm_offset_t va, vm_size_t size,
    vm_memattr_t mode)
{
	int error;

	CTR4(KTR_PMAP, "%s(%#x, %#zx, %d)", __func__, va, size, mode);
	PMAP_LOCK(kernel_pmap);
	error = pmap_change_attr_locked(va, size, mode, true);
	PMAP_UNLOCK(kernel_pmap);
	return (error);
}

static int
pmap_change_attr_locked(vm_offset_t va, vm_size_t size, int mode, bool flush)
{
	vm_offset_t base, offset, tmpva;
	vm_paddr_t pa_start, pa_end, pa_end1;
	pml2_entry_t *l2e;
	pml3_entry_t *l3e;
	pt_entry_t *pte;
	int cache_bits, error;
	boolean_t changed;

	PMAP_LOCK_ASSERT(kernel_pmap, MA_OWNED);
	base = trunc_page(va);
	offset = va & PAGE_MASK;
	size = round_page(offset + size);

	/*
	 * Only supported on kernel virtual addresses, including the direct
	 * map but excluding the recursive map.
	 */
	if (base < DMAP_MIN_ADDRESS)
		return (EINVAL);

	cache_bits = pmap_cache_bits(mode);
	changed = FALSE;

	/*
	 * Pages that aren't mapped aren't supported.  Also break down 2MB pages
	 * into 4KB pages if required.
	 */
	for (tmpva = base; tmpva < base + size; ) {
		l2e = pmap_pml2e(kernel_pmap, tmpva);
		if (l2e == NULL || *l2e == 0)
			return (EINVAL);
		if (be64toh(*l2e) & RPTE_LEAF) {
			/*
			 * If the current 1GB page already has the required
			 * memory type, then we need not demote this page. Just
			 * increment tmpva to the next 1GB page frame.
			 */
			if ((be64toh(*l2e) & RPTE_ATTR_MASK) == cache_bits) {
				tmpva = trunc_1gpage(tmpva) + L2_PAGE_SIZE;
				continue;
			}

			/*
			 * If the current offset aligns with a 1GB page frame
			 * and there is at least 1GB left within the range, then
			 * we need not break down this page into 2MB pages.
			 */
			if ((tmpva & L2_PAGE_MASK) == 0 &&
			    tmpva + L2_PAGE_MASK < base + size) {
				tmpva += L2_PAGE_MASK;
				continue;
			}
			if (!pmap_demote_l2e(kernel_pmap, l2e, tmpva))
				return (ENOMEM);
		}
		l3e = pmap_l2e_to_l3e(l2e, tmpva);
		KASSERT(l3e != NULL, ("no l3e entry for %#lx in %p\n",
		    tmpva, l2e));
		if (*l3e == 0)
			return (EINVAL);
		if (be64toh(*l3e) & RPTE_LEAF) {
			/*
			 * If the current 2MB page already has the required
			 * memory type, then we need not demote this page. Just
			 * increment tmpva to the next 2MB page frame.
			 */
			if ((be64toh(*l3e) & RPTE_ATTR_MASK) == cache_bits) {
				tmpva = trunc_2mpage(tmpva) + L3_PAGE_SIZE;
				continue;
			}

			/*
			 * If the current offset aligns with a 2MB page frame
			 * and there is at least 2MB left within the range, then
			 * we need not break down this page into 4KB pages.
			 */
			if ((tmpva & L3_PAGE_MASK) == 0 &&
			    tmpva + L3_PAGE_MASK < base + size) {
				tmpva += L3_PAGE_SIZE;
				continue;
			}
			if (!pmap_demote_l3e(kernel_pmap, l3e, tmpva))
				return (ENOMEM);
		}
		pte = pmap_l3e_to_pte(l3e, tmpva);
		if (*pte == 0)
			return (EINVAL);
		tmpva += PAGE_SIZE;
	}
	error = 0;

	/*
	 * Ok, all the pages exist, so run through them updating their
	 * cache mode if required.
	 */
	pa_start = pa_end = 0;
	for (tmpva = base; tmpva < base + size; ) {
		l2e = pmap_pml2e(kernel_pmap, tmpva);
		if (be64toh(*l2e) & RPTE_LEAF) {
			if ((be64toh(*l2e) & RPTE_ATTR_MASK) != cache_bits) {
				pmap_pte_attr(l2e, cache_bits,
				    RPTE_ATTR_MASK);
				changed = TRUE;
			}
			if (tmpva >= VM_MIN_KERNEL_ADDRESS &&
			    (*l2e & PG_PS_FRAME) < dmaplimit) {
				if (pa_start == pa_end) {
					/* Start physical address run. */
					pa_start = be64toh(*l2e) & PG_PS_FRAME;
					pa_end = pa_start + L2_PAGE_SIZE;
				} else if (pa_end == (be64toh(*l2e) & PG_PS_FRAME))
					pa_end += L2_PAGE_SIZE;
				else {
					/* Run ended, update direct map. */
					error = pmap_change_attr_locked(
					    PHYS_TO_DMAP(pa_start),
					    pa_end - pa_start, mode, flush);
					if (error != 0)
						break;
					/* Start physical address run. */
					pa_start = be64toh(*l2e) & PG_PS_FRAME;
					pa_end = pa_start + L2_PAGE_SIZE;
				}
			}
			tmpva = trunc_1gpage(tmpva) + L2_PAGE_SIZE;
			continue;
		}
		l3e = pmap_l2e_to_l3e(l2e, tmpva);
		if (be64toh(*l3e) & RPTE_LEAF) {
			if ((be64toh(*l3e) & RPTE_ATTR_MASK) != cache_bits) {
				pmap_pte_attr(l3e, cache_bits,
				    RPTE_ATTR_MASK);
				changed = TRUE;
			}
			if (tmpva >= VM_MIN_KERNEL_ADDRESS &&
			    (be64toh(*l3e) & PG_PS_FRAME) < dmaplimit) {
				if (pa_start == pa_end) {
					/* Start physical address run. */
					pa_start = be64toh(*l3e) & PG_PS_FRAME;
					pa_end = pa_start + L3_PAGE_SIZE;
				} else if (pa_end == (be64toh(*l3e) & PG_PS_FRAME))
					pa_end += L3_PAGE_SIZE;
				else {
					/* Run ended, update direct map. */
					error = pmap_change_attr_locked(
					    PHYS_TO_DMAP(pa_start),
					    pa_end - pa_start, mode, flush);
					if (error != 0)
						break;
					/* Start physical address run. */
					pa_start = be64toh(*l3e) & PG_PS_FRAME;
					pa_end = pa_start + L3_PAGE_SIZE;
				}
			}
			tmpva = trunc_2mpage(tmpva) + L3_PAGE_SIZE;
		} else {
			pte = pmap_l3e_to_pte(l3e, tmpva);
			if ((be64toh(*pte) & RPTE_ATTR_MASK) != cache_bits) {
				pmap_pte_attr(pte, cache_bits,
				    RPTE_ATTR_MASK);
				changed = TRUE;
			}
			if (tmpva >= VM_MIN_KERNEL_ADDRESS &&
			    (be64toh(*pte) & PG_FRAME) < dmaplimit) {
				if (pa_start == pa_end) {
					/* Start physical address run. */
					pa_start = be64toh(*pte) & PG_FRAME;
					pa_end = pa_start + PAGE_SIZE;
				} else if (pa_end == (be64toh(*pte) & PG_FRAME))
					pa_end += PAGE_SIZE;
				else {
					/* Run ended, update direct map. */
					error = pmap_change_attr_locked(
					    PHYS_TO_DMAP(pa_start),
					    pa_end - pa_start, mode, flush);
					if (error != 0)
						break;
					/* Start physical address run. */
					pa_start = be64toh(*pte) & PG_FRAME;
					pa_end = pa_start + PAGE_SIZE;
				}
			}
			tmpva += PAGE_SIZE;
		}
	}
	if (error == 0 && pa_start != pa_end && pa_start < dmaplimit) {
		pa_end1 = MIN(pa_end, dmaplimit);
		if (pa_start != pa_end1)
			error = pmap_change_attr_locked(PHYS_TO_DMAP(pa_start),
			    pa_end1 - pa_start, mode, flush);
	}

	/*
	 * Flush CPU caches if required to make sure any data isn't cached that
	 * shouldn't be, etc.
	 */
	if (changed) {
		pmap_invalidate_all(kernel_pmap);

		if (flush)
			pmap_invalidate_cache_range(base, tmpva);
	}
	return (error);
}

/*
 * Allocate physical memory for the vm_page array and map it into KVA,
 * attempting to back the vm_pages with domain-local memory.
 */
void
mmu_radix_page_array_startup(long pages)
{
#ifdef notyet
	pml2_entry_t *l2e;
	pml3_entry_t *pde;
	pml3_entry_t newl3;
	vm_offset_t va;
	long pfn;
	int domain, i;
#endif
	vm_paddr_t pa;
	vm_offset_t start, end;

	vm_page_array_size = pages;

	start = VM_MIN_KERNEL_ADDRESS;
	end = start + pages * sizeof(struct vm_page);

	pa = vm_phys_early_alloc(0, end - start);

	start = mmu_radix_map(&start, pa, end - start, VM_MEMATTR_DEFAULT);
#ifdef notyet
	/* TODO: NUMA vm_page_array.  Blocked out until then (copied from amd64). */
	for (va = start; va < end; va += L3_PAGE_SIZE) {
		pfn = first_page + (va - start) / sizeof(struct vm_page);
		domain = vm_phys_domain(ptoa(pfn));
		l2e = pmap_pml2e(kernel_pmap, va);
		if ((be64toh(*l2e) & PG_V) == 0) {
			pa = vm_phys_early_alloc(domain, PAGE_SIZE);
			dump_add_page(pa);
			pagezero(PHYS_TO_DMAP(pa));
			pde_store(l2e, (pml2_entry_t)pa);
		}
		pde = pmap_l2e_to_l3e(l2e, va);
		if ((be64toh(*pde) & PG_V) != 0)
			panic("Unexpected pde %p", pde);
		pa = vm_phys_early_alloc(domain, L3_PAGE_SIZE);
		for (i = 0; i < NPDEPG; i++)
			dump_add_page(pa + i * PAGE_SIZE);
		newl3 = (pml3_entry_t)(pa | RPTE_EAA_P | RPTE_EAA_R | RPTE_EAA_W);
		pte_store(pde, newl3);
	}
#endif
	vm_page_array = (vm_page_t)start;
}

#ifdef DDB
#include <sys/kdb.h>
#include <ddb/ddb.h>

static void
pmap_pte_walk(pml1_entry_t *l1, vm_offset_t va)
{
	pml1_entry_t *l1e;
	pml2_entry_t *l2e;
	pml3_entry_t *l3e;
	pt_entry_t *pte;

	l1e = &l1[pmap_pml1e_index(va)];
	db_printf("VA %#016lx l1e %#016lx", va, be64toh(*l1e));
	if ((be64toh(*l1e) & PG_V) == 0) {
		db_printf("\n");
		return;
	}
	l2e = pmap_l1e_to_l2e(l1e, va);
	db_printf(" l2e %#016lx", be64toh(*l2e));
	if ((be64toh(*l2e) & PG_V) == 0 || (be64toh(*l2e) & RPTE_LEAF) != 0) {
		db_printf("\n");
		return;
	}
	l3e = pmap_l2e_to_l3e(l2e, va);
	db_printf(" l3e %#016lx", be64toh(*l3e));
	if ((be64toh(*l3e) & PG_V) == 0 || (be64toh(*l3e) & RPTE_LEAF) != 0) {
		db_printf("\n");
		return;
	}
	pte = pmap_l3e_to_pte(l3e, va);
	db_printf(" pte %#016lx\n", be64toh(*pte));
}

void
pmap_page_print_mappings(vm_page_t m)
{
	pmap_t pmap;
	pv_entry_t pv;

	db_printf("page %p(%lx)\n", m, m->phys_addr);
	/* need to elide locks if running in ddb */
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_link) {
		db_printf("pv: %p ", pv);
		db_printf("va: %#016lx ", pv->pv_va);
		pmap = PV_PMAP(pv);
		db_printf("pmap %p  ", pmap);
		if (pmap != NULL) {
			db_printf("asid: %lu\n", pmap->pm_pid);
			pmap_pte_walk(pmap->pm_pml1, pv->pv_va);
		}
	}
}

DB_SHOW_COMMAND(pte, pmap_print_pte)
{
	vm_offset_t va;
	pmap_t pmap;

	if (!have_addr) {
		db_printf("show pte addr\n");
		return;
	}
	va = (vm_offset_t)addr;

	if (va >= DMAP_MIN_ADDRESS)
		pmap = kernel_pmap;
	else if (kdb_thread != NULL)
		pmap = vmspace_pmap(kdb_thread->td_proc->p_vmspace);
	else
		pmap = vmspace_pmap(curthread->td_proc->p_vmspace);

	pmap_pte_walk(pmap->pm_pml1, va);
}

#endif
