/*
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 * Copyright (c) 1998,2000 Doug Rabson
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
 *	from:	@(#)pmap.c	7.7 (Berkeley)	5/12/91
 *	from:	i386 Id: pmap.c,v 1.193 1998/04/19 15:22:48 bde Exp
 *		with some ideas from NetBSD's alpha pmap
 * $FreeBSD$
 */

/*
 *	Manages physical address maps.
 *
 *	In addition to hardware address maps, this
 *	module is called upon to provide software-use-only
 *	maps which may or may not be stored in the same
 *	form as hardware maps.  These pseudo-maps are
 *	used to store intermediate results from copy
 *	operations to and from address spaces.
 *
 *	Since the information managed by this module is
 *	also stored by the logical address mapping module,
 *	this module may throw away valid virtual-to-physical
 *	mappings at almost any time.  However, invalidations
 *	of virtual-to-physical mappings must be done as
 *	requested.
 *
 *	In order to cope with hardware architectures which
 *	make virtual-to-physical map invalidates expensive,
 *	this module may delay invalidate or reduced protection
 *	operations until such time as they are actually
 *	necessary.  This module is given full information as
 *	to which processors are currently using which maps,
 *	and to when physical maps must be made correct.
 */

/*
 * Following the Linux model, region IDs are allocated in groups of
 * eight so that a single region ID can be used for as many RRs as we
 * want by encoding the RR number into the low bits of the ID.
 *
 * We reserve region ID 0 for the kernel and allocate the remaining
 * IDs for user pmaps.
 *
 * Region 0..4
 *	User virtually mapped
 *
 * Region 5
 *	Kernel virtually mapped
 *
 * Region 6
 *	Kernel physically mapped uncacheable
 *
 * Region 7
 *	Kernel physically mapped cacheable
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sx.h>
#include <sys/systm.h>
#include <sys/vmmeter.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_pager.h>
#include <vm/uma.h>

#include <sys/user.h>

#include <machine/pal.h>
#include <machine/md_var.h>

MALLOC_DEFINE(M_PMAP, "PMAP", "PMAP Structures");

#ifndef PMAP_SHPGPERPROC
#define PMAP_SHPGPERPROC 200
#endif

#if defined(DIAGNOSTIC)
#define PMAP_DIAGNOSTIC
#endif

#define MINPV 2048	/* Preallocate at least this many */
#define MAXPV 20480	/* But no more than this */

#if 0
#define PMAP_DIAGNOSTIC
#define PMAP_DEBUG
#endif

#if !defined(PMAP_DIAGNOSTIC)
#define PMAP_INLINE __inline
#else
#define PMAP_INLINE
#endif

/*
 * Get PDEs and PTEs for user/kernel address space
 */
#define pmap_pte_w(pte)		((pte)->pte_ig & PTE_IG_WIRED)
#define pmap_pte_managed(pte)	((pte)->pte_ig & PTE_IG_MANAGED)
#define pmap_pte_v(pte)		((pte)->pte_p)
#define pmap_pte_pa(pte)	(((pte)->pte_ppn) << 12)
#define pmap_pte_prot(pte)	(((pte)->pte_ar << 2) | (pte)->pte_pl)

#define pmap_pte_set_w(pte, v) ((v)?((pte)->pte_ig |= PTE_IG_WIRED) \
				:((pte)->pte_ig &= ~PTE_IG_WIRED))
#define pmap_pte_set_prot(pte, v) do {		\
    (pte)->pte_ar = v >> 2;			\
    (pte)->pte_pl = v & 3;			\
} while (0)

/*
 * Given a map and a machine independent protection code,
 * convert to an ia64 protection code.
 */
#define pte_prot(m, p)		(protection_codes[m == kernel_pmap ? 0 : 1][p])
#define pte_prot_pl(m, p)	(pte_prot(m, p) & 3)
#define pte_prot_ar(m, p)	(pte_prot(m, p) >> 2)
int	protection_codes[2][8];

/*
 * Return non-zero if this pmap is currently active
 */
#define pmap_isactive(pmap)	(pmap->pm_active)

/*
 * Statically allocated kernel pmap
 */
struct pmap kernel_pmap_store;

vm_offset_t avail_start;	/* PA of first available physical page */
vm_offset_t avail_end;		/* PA of last available physical page */
vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */
static boolean_t pmap_initialized = FALSE;	/* Has pmap_init completed? */

vm_offset_t vhpt_base, vhpt_size;

/*
 * We use an object to own the kernel's 'page tables'. For simplicity,
 * we use one page directory to index a set of pages containing
 * ia64_lptes. This gives us up to 2Gb of kernel virtual space.
 */
static vm_object_t kptobj;
static int nkpt;
static struct ia64_lpte **kptdir;
#define KPTE_DIR_INDEX(va) \
	((va >> (2*PAGE_SHIFT-5)) & ((1<<(PAGE_SHIFT-3))-1))
#define KPTE_PTE_INDEX(va) \
	((va >> PAGE_SHIFT) & ((1<<(PAGE_SHIFT-5))-1))
#define NKPTEPG		(PAGE_SIZE / sizeof(struct ia64_lpte))

vm_offset_t kernel_vm_end;

/*
 * Values for ptc.e. XXX values for SKI.
 */
static u_int64_t pmap_ptc_e_base = 0x100000000;
static u_int64_t pmap_ptc_e_count1 = 3;
static u_int64_t pmap_ptc_e_count2 = 2;
static u_int64_t pmap_ptc_e_stride1 = 0x2000;
static u_int64_t pmap_ptc_e_stride2 = 0x100000000;

/*
 * Data for the RID allocator
 */
static u_int64_t *pmap_ridbusy;
static int pmap_ridmax, pmap_ridcount;
struct mtx pmap_ridmutex;

/*
 * Data for the pv entry allocation mechanism
 */
static uma_zone_t pvzone;
#if 0
static struct vm_object pvzone_obj;
#endif
static int pv_entry_count = 0, pv_entry_max = 0, pv_entry_high_water = 0;
static int pmap_pagedaemon_waken = 0;
static struct pv_entry *pvbootentries;
static int pvbootnext, pvbootmax;

/*
 * Data for allocating PTEs for user processes.
 */
static uma_zone_t ptezone;
#if 0
static struct vm_object ptezone_obj;
#endif

/*
 * VHPT instrumentation.
 */
static int pmap_vhpt_inserts;
static int pmap_vhpt_collisions;
static int pmap_vhpt_resident;
SYSCTL_DECL(_vm_stats);
SYSCTL_NODE(_vm_stats, OID_AUTO, vhpt, CTLFLAG_RD, 0, "");
SYSCTL_INT(_vm_stats_vhpt, OID_AUTO, inserts, CTLFLAG_RD,
	   &pmap_vhpt_inserts, 0, "");
SYSCTL_INT(_vm_stats_vhpt, OID_AUTO, collisions, CTLFLAG_RD,
	   &pmap_vhpt_collisions, 0, "");
SYSCTL_INT(_vm_stats_vhpt, OID_AUTO, resident, CTLFLAG_RD,
	   &pmap_vhpt_resident, 0, "");

static PMAP_INLINE void	free_pv_entry(pv_entry_t pv);
static pv_entry_t get_pv_entry(void);
static void	ia64_protection_init(void);

static void	pmap_invalidate_all(pmap_t pmap);
static void	pmap_remove_all(vm_page_t m);
static void	pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m);
static void	*pmap_allocf(uma_zone_t zone, int bytes, u_int8_t *flags, int wait);       

vm_offset_t
pmap_steal_memory(vm_size_t size)
{
	vm_size_t bank_size;
	vm_offset_t pa, va;

	size = round_page(size);

	bank_size = phys_avail[1] - phys_avail[0];
	while (size > bank_size) {
		int i;
		for (i = 0; phys_avail[i+2]; i+= 2) {
			phys_avail[i] = phys_avail[i+2];
			phys_avail[i+1] = phys_avail[i+3];
		}
		phys_avail[i] = 0;
		phys_avail[i+1] = 0;
		if (!phys_avail[0])
			panic("pmap_steal_memory: out of memory");
		bank_size = phys_avail[1] - phys_avail[0];
	}

	pa = phys_avail[0];
	phys_avail[0] += size;

	va = IA64_PHYS_TO_RR7(pa);
	bzero((caddr_t) va, size);
	return va;
}

/*
 *	Bootstrap the system enough to run with virtual memory.
 */
void
pmap_bootstrap()
{
	int i, j, count, ridbits;
	struct ia64_pal_result res;

	/*
	 * Query the PAL Code to find the loop parameters for the
	 * ptc.e instruction.
	 */
	res = ia64_call_pal_static(PAL_PTCE_INFO, 0, 0, 0);
	if (res.pal_status != 0)
		panic("Can't configure ptc.e parameters");
	pmap_ptc_e_base = res.pal_result[0];
	pmap_ptc_e_count1 = res.pal_result[1] >> 32;
	pmap_ptc_e_count2 = res.pal_result[1] & ((1L<<32) - 1);
	pmap_ptc_e_stride1 = res.pal_result[2] >> 32;
	pmap_ptc_e_stride2 = res.pal_result[2] & ((1L<<32) - 1);
	if (bootverbose)
		printf("ptc.e base=0x%lx, count1=%ld, count2=%ld, "
		       "stride1=0x%lx, stride2=0x%lx\n",
		       pmap_ptc_e_base,
		       pmap_ptc_e_count1,
		       pmap_ptc_e_count2,
		       pmap_ptc_e_stride1,
		       pmap_ptc_e_stride2);

	/*
	 * Setup RIDs. RIDs 0..7 are reserved for the kernel.
	 */
	res = ia64_call_pal_static(PAL_VM_SUMMARY, 0, 0, 0);
	if (res.pal_status != 0) {
		if (bootverbose)
			printf("Can't read VM Summary - assuming 18 Region ID bits\n");
		ridbits = 18; /* guaranteed minimum */
	} else {
		ridbits = (res.pal_result[1] >> 8) & 0xff;
		if (bootverbose)
			printf("Processor supports %d Region ID bits\n",
			       ridbits);
	}
	pmap_ridmax = (1 << ridbits);
	pmap_ridcount = 8;
	pmap_ridbusy = (u_int64_t *)
		pmap_steal_memory(pmap_ridmax / 8);
	bzero(pmap_ridbusy, pmap_ridmax / 8);
	pmap_ridbusy[0] |= 0xff;
	mtx_init(&pmap_ridmutex, "RID allocator lock", NULL, MTX_DEF);

	/*
	 * Allocate some memory for initial kernel 'page tables'.
	 */
	kptdir = (struct ia64_lpte **) pmap_steal_memory(PAGE_SIZE);
	for (i = 0; i < NKPT; i++) {
		kptdir[i] = (struct ia64_lpte *) pmap_steal_memory(PAGE_SIZE);
	}
	nkpt = NKPT;

	avail_start = phys_avail[0];
	for (i = 0; phys_avail[i+2]; i+= 2) ;
	avail_end = phys_avail[i+1];
	count = i+2;

	/*
	 * Figure out a useful size for the VHPT, based on the size of
	 * physical memory and try to locate a region which is large
	 * enough to contain the VHPT (which must be a power of two in
	 * size and aligned to a natural boundary).
	 */
	vhpt_size = 15;
	while ((1<<vhpt_size) < ia64_btop(avail_end - avail_start) * 32)
		vhpt_size++;

	vhpt_base = 0;
	while (!vhpt_base) {
		vm_offset_t mask;
		if (bootverbose)
			printf("Trying VHPT size 0x%lx\n", (1L<<vhpt_size));
		mask = (1L << vhpt_size) - 1;
		for (i = 0; i < count; i += 2) {
			vm_offset_t base, limit;
			base = (phys_avail[i] + mask) & ~mask;
			limit = base + (1L << vhpt_size);
			if (limit <= phys_avail[i+1])
				/*
				 * VHPT can fit in this region
				 */
				break;
		}
		if (!phys_avail[i]) {
			/*
			 * Can't fit, try next smaller size.
			 */
			vhpt_size--;
		} else {
			vhpt_base = (phys_avail[i] + mask) & ~mask;
		}
	}
	if (vhpt_size < 15)
		panic("Can't find space for VHPT");

	if (bootverbose)
		printf("Putting VHPT at %p\n", (void *) vhpt_base);
	if (vhpt_base != phys_avail[i]) {
		/*
		 * Split this region.
		 */
		if (bootverbose)
			printf("Splitting [%p-%p]\n",
			       (void *) phys_avail[i],
			       (void *) phys_avail[i+1]);
		for (j = count; j > i; j -= 2) {
			phys_avail[j] = phys_avail[j-2];
			phys_avail[j+1] = phys_avail[j-2+1];
		}
		phys_avail[count+2] = 0;
		phys_avail[count+3] = 0;
		phys_avail[i+1] = vhpt_base;
		phys_avail[i+2] = vhpt_base + (1L << vhpt_size);
	} else {
		phys_avail[i] = vhpt_base + (1L << vhpt_size);
	}

	vhpt_base = IA64_PHYS_TO_RR7(vhpt_base);
	bzero((void *) vhpt_base, (1L << vhpt_size));
	__asm __volatile("mov cr.pta=%0;; srlz.i;;"
			 :: "r" (vhpt_base + (1<<8) + (vhpt_size<<2) + 1));

	virtual_avail = IA64_RR_BASE(5);
	virtual_end = IA64_RR_BASE(6)-1;

	/*
	 * Initialize protection array.
	 */
	ia64_protection_init();

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	for (i = 0; i < 5; i++)
		kernel_pmap->pm_rid[i] = 0;
	kernel_pmap->pm_active = 1;
	TAILQ_INIT(&kernel_pmap->pm_pvlist);
	PCPU_SET(current_pmap, kernel_pmap);

	/*
	 * Region 5 is mapped via the vhpt.
	 */
	ia64_set_rr(IA64_RR_BASE(5),
		    (5 << 8) | (PAGE_SHIFT << 2) | 1);

	/*
	 * Region 6 is direct mapped UC and region 7 is direct mapped
	 * WC. The details of this is controlled by the Alt {I,D}TLB
	 * handlers. Here we just make sure that they have the largest 
	 * possible page size to minimise TLB usage.
	 */
	ia64_set_rr(IA64_RR_BASE(6), (6 << 8) | (28 << 2));
	ia64_set_rr(IA64_RR_BASE(7), (7 << 8) | (28 << 2));
			 
	/*
	 * Set up proc0's PCB.
	 */
#if 0
	thread0.td_pcb->pcb_hw.apcb_asn = 0;
#endif

	/*
	 * Reserve some memory for allocating pvs while bootstrapping
	 * the pv allocator. We need to have enough to cover mapping
	 * the kmem_alloc region used to allocate the initial_pvs in
	 * pmap_init. In general, the size of this region is
	 * approximately (# physical pages) * (size of pv entry).
	 */
	pvbootmax = ((physmem * sizeof(struct pv_entry)) >> PAGE_SHIFT) + 128;
	pvbootentries = (struct pv_entry *)
		pmap_steal_memory(pvbootmax * sizeof(struct pv_entry));
	pvbootnext = 0;

	/*
	 * Clear out any random TLB entries left over from booting.
	 */
	pmap_invalidate_all(kernel_pmap);
}

static void *
pmap_allocf(uma_zone_t zone, int bytes, u_int8_t *flags, int wait)
{
	*flags = UMA_SLAB_PRIV;
	return (void *)IA64_PHYS_TO_RR7(ia64_tpa(kmem_alloc(kernel_map, bytes)));
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 *	pmap_init has been enhanced to support in a fairly consistant
 *	way, discontiguous physical memory.
 */
void
pmap_init(vm_offset_t phys_start, vm_offset_t phys_end)
{
	int i;
	int initial_pvs;

	/*
	 * Allocate memory for random pmap data structures.  Includes the
	 * pv_head_table.
	 */

	for(i = 0; i < vm_page_array_size; i++) {
		vm_page_t m;

		m = &vm_page_array[i];
		TAILQ_INIT(&m->md.pv_list);
		m->md.pv_list_count = 0;
 	}

	/*
	 * Init the pv free list and the PTE free list.
	 */
	initial_pvs = vm_page_array_size;
	if (initial_pvs < MINPV)
		initial_pvs = MINPV;
	if (initial_pvs > MAXPV)
		initial_pvs = MAXPV;
	pvzone = uma_zcreate("PV ENTRY", sizeof (struct pv_entry),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM);
	uma_zone_set_allocf(pvzone, pmap_allocf);
	uma_prealloc(pvzone, initial_pvs);

	ptezone = uma_zcreate("PT ENTRY", sizeof (struct ia64_lpte), 
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, UMA_ZONE_VM);
	uma_zone_set_allocf(ptezone, pmap_allocf);
	uma_prealloc(ptezone, initial_pvs);

	/*
	 * Create the object for the kernel's page tables.
	 */
	kptobj = vm_object_allocate(OBJT_DEFAULT, MAXKPT);

	/*
	 * Now it is safe to enable pv_table recording.
	 */
	pmap_initialized = TRUE;
}

/*
 * Initialize the address space (zone) for the pv_entries.  Set a
 * high water mark so that the system can recover from excessive
 * numbers of pv entries.
 */
void
pmap_init2()
{
	int shpgperproc = PMAP_SHPGPERPROC;

	TUNABLE_INT_FETCH("vm.pmap.shpgperproc", &shpgperproc);
	pv_entry_max = shpgperproc * maxproc + vm_page_array_size;
	pv_entry_high_water = 9 * (pv_entry_max / 10);
#if 0	/* incompatable with pmap_allocf above */
	uma_zone_set_obj(pvzone, &pvzone_obj, pv_entry_max);
	uma_zone_set_obj(ptezone, &ptezone_obj, pv_entry_max);
#endif
}


/***************************************************
 * Manipulate TLBs for a pmap
 ***************************************************/

static void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{
	KASSERT((pmap == kernel_pmap || pmap == PCPU_GET(current_pmap)),
		("invalidating TLB for non-current pmap"));
	ia64_ptc_g(va, PAGE_SHIFT << 2);
}

static void
pmap_invalidate_all_1(void *arg)
{
	u_int64_t addr;
	int i, j;
	register_t psr;

	psr = intr_disable();
	addr = pmap_ptc_e_base;
	for (i = 0; i < pmap_ptc_e_count1; i++) {
		for (j = 0; j < pmap_ptc_e_count2; j++) {
			ia64_ptc_e(addr);
			addr += pmap_ptc_e_stride2;
		}
		addr += pmap_ptc_e_stride1;
	}
	intr_restore(psr);
}

static void
pmap_invalidate_all(pmap_t pmap)
{
	KASSERT((pmap == kernel_pmap || pmap == PCPU_GET(current_pmap)),
		("invalidating TLB for non-current pmap"));


#ifdef SMP
	smp_rendezvous(0, pmap_invalidate_all_1, 0, 0);
#else
	pmap_invalidate_all_1(0);
#endif
}

static u_int32_t
pmap_allocate_rid(void)
{
	int rid;

	if (pmap_ridcount == pmap_ridmax)
		panic("pmap_allocate_rid: All Region IDs used");

	do {
		rid = arc4random() & (pmap_ridmax - 1);
	} while (pmap_ridbusy[rid / 64] & (1L << (rid & 63)));
	pmap_ridbusy[rid / 64] |= (1L << (rid & 63));
	pmap_ridcount++;

	return rid;
}

static void
pmap_free_rid(u_int32_t rid)
{
	mtx_lock(&pmap_ridmutex);
	pmap_ridbusy[rid / 64] &= ~(1L << (rid & 63));
	pmap_ridcount--;
	mtx_unlock(&pmap_ridmutex);
}

static void
pmap_ensure_rid(pmap_t pmap, vm_offset_t va)
{
	int rr;

	rr = va >> 61;

	/*
	 * We get called for virtual addresses that may just as well be
	 * kernel addresses (ie region 5, 6 or 7). Since the pm_rid field
	 * only holds region IDs for user regions, we have to make sure
	 * the region is within bounds.
	 */
	if (rr >= 5)
		return;

	if (pmap->pm_rid[rr])
		return;

	mtx_lock(&pmap_ridmutex);
	pmap->pm_rid[rr] = pmap_allocate_rid();
	if (pmap == PCPU_GET(current_pmap))
		ia64_set_rr(IA64_RR_BASE(rr),
			    (pmap->pm_rid[rr] << 8)|(PAGE_SHIFT << 2)|1);
	mtx_unlock(&pmap_ridmutex);
}

/***************************************************
 * Low level helper routines.....
 ***************************************************/

/*
 * Install a pte into the VHPT
 */
static PMAP_INLINE void
pmap_install_pte(struct ia64_lpte *vhpte, struct ia64_lpte *pte)
{
	u_int64_t *vhp, *p;

	/* invalidate the pte */
	atomic_set_64(&vhpte->pte_tag, 1L << 63);
	ia64_mf();			/* make sure everyone sees */

	vhp = (u_int64_t *) vhpte;
	p = (u_int64_t *) pte;

	vhp[0] = p[0];
	vhp[1] = p[1];
	vhp[2] = p[2];			/* sets ti to one */

	ia64_mf();
}

/*
 * Compare essential parts of pte.
 */
static PMAP_INLINE int
pmap_equal_pte(struct ia64_lpte *pte1, struct ia64_lpte *pte2)
{
	return *(u_int64_t *) pte1 == *(u_int64_t *) pte2;
}

/*
 * this routine defines the region(s) of memory that should
 * not be tested for the modified bit.
 */
static PMAP_INLINE int
pmap_track_modified(vm_offset_t va)
{
	if ((va < kmi.clean_sva) || (va >= kmi.clean_eva)) 
		return 1;
	else
		return 0;
}

#ifndef KSTACK_MAX_PAGES
#define KSTACK_MAX_PAGES 32
#endif

/*
 * Create the KSTACK for a new thread.
 * This routine directly affects the fork perf for a process/thread.
 */
void
pmap_new_thread(struct thread *td, int pages)
{
	vm_offset_t *ks;

	/* Bounds check */
	if (pages <= 1)
		pages = KSTACK_PAGES;
	else if (pages > KSTACK_MAX_PAGES)
		pages = KSTACK_MAX_PAGES;

	/*
	 * Use contigmalloc for user area so that we can use a region
	 * 7 address for it which makes it impossible to accidentally
	 * lose when recording a trapframe.
	 */
	ks = contigmalloc(pages * PAGE_SIZE, M_PMAP, M_WAITOK, 0ul,
	    256*1024*1024 - 1, PAGE_SIZE, 256*1024*1024);
	if (ks == NULL)
		panic("pmap_new_thread: could not contigmalloc %d pages\n",
		    pages);

	td->td_md.md_kstackvirt = ks;
	td->td_kstack = IA64_PHYS_TO_RR7(ia64_tpa((u_int64_t)ks));
	td->td_kstack_pages = pages;
}

/*
 * Dispose the KSTACK for a thread that has exited.
 * This routine directly impacts the exit perf of a process/thread.
 */
void
pmap_dispose_thread(struct thread *td)
{
	int pages;

	pages = td->td_kstack_pages;
	contigfree(td->td_md.md_kstackvirt, pages * PAGE_SIZE, M_PMAP);
	td->td_md.md_kstackvirt = NULL;
	td->td_kstack = 0;
}

/*
 * Set up a variable sized alternate kstack.  This appears to be MI.
 */
void
pmap_new_altkstack(struct thread *td, int pages)
{

	/*
	 * Shuffle the original stack. Save the virtual kstack address
	 * instead of the physical address because 1) we can derive the
	 * physical address from the virtual address and 2) we need the
	 * virtual address in pmap_dispose_thread.
	 */
	td->td_altkstack_obj = td->td_kstack_obj;
	td->td_altkstack = (vm_offset_t)td->td_md.md_kstackvirt;
	td->td_altkstack_pages = td->td_kstack_pages;

	pmap_new_thread(td, pages);
}

void
pmap_dispose_altkstack(struct thread *td)
{

	pmap_dispose_thread(td);

	/*
	 * Restore the original kstack. Note that td_altkstack holds the
	 * virtual kstack address of the previous kstack.
	 */
	td->td_md.md_kstackvirt = (void*)td->td_altkstack;
	td->td_kstack = IA64_PHYS_TO_RR7(ia64_tpa(td->td_altkstack));
	td->td_kstack_obj = td->td_altkstack_obj;
	td->td_kstack_pages = td->td_altkstack_pages;
	td->td_altkstack = 0;
	td->td_altkstack_obj = NULL;
	td->td_altkstack_pages = 0;
}

/*
 * Allow the KSTACK for a thread to be prejudicially paged out.
 */
void
pmap_swapout_thread(struct thread *td)
{
}

/*
 * Bring the KSTACK for a specified thread back in.
 */
void
pmap_swapin_thread(struct thread *td)
{
}

/***************************************************
 * Page table page management routines.....
 ***************************************************/

void
pmap_pinit0(struct pmap *pmap)
{
	/* kernel_pmap is the same as any other pmap. */
	pmap_pinit(pmap);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
void
pmap_pinit(struct pmap *pmap)
{
	int i;

	pmap->pm_flags = 0;
	for (i = 0; i < 5; i++)
		pmap->pm_rid[i] = 0;
	pmap->pm_ptphint = NULL;
	pmap->pm_active = 0;
	TAILQ_INIT(&pmap->pm_pvlist);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
}

/*
 * Wire in kernel global address entries.  To avoid a race condition
 * between pmap initialization and pmap_growkernel, this procedure
 * should be called after the vmspace is attached to the process
 * but before this pmap is activated.
 */
void
pmap_pinit2(struct pmap *pmap)
{
}

/***************************************************
* Pmap allocation/deallocation routines.
 ***************************************************/

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap_t pmap)
{
	int i;

	for (i = 0; i < 5; i++)
		if (pmap->pm_rid[i])
			pmap_free_rid(pmap->pm_rid[i]);
}

/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	struct ia64_lpte *ptepage;
	vm_page_t nkpg;

	if (kernel_vm_end == 0) {
		kernel_vm_end = nkpt * PAGE_SIZE * NKPTEPG
			+ IA64_RR_BASE(5);
	}
	addr = (addr + PAGE_SIZE * NKPTEPG) & ~(PAGE_SIZE * NKPTEPG - 1);
	while (kernel_vm_end < addr) {
		if (kptdir[KPTE_DIR_INDEX(kernel_vm_end)]) {
			kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NKPTEPG)
				& ~(PAGE_SIZE * NKPTEPG - 1);
			continue;
		}

		/*
		 * We could handle more by increasing the size of kptdir.
		 */
		if (nkpt == MAXKPT)
			panic("pmap_growkernel: out of kernel address space");

		/*
		 * This index is bogus, but out of the way
		 */
		nkpg = vm_page_alloc(kptobj, nkpt,
		    VM_ALLOC_SYSTEM | VM_ALLOC_WIRED);
		if (!nkpg)
			panic("pmap_growkernel: no memory to grow kernel");

		nkpt++;
		ptepage = (struct ia64_lpte *)
			IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(nkpg));
		bzero(ptepage, PAGE_SIZE);
		kptdir[KPTE_DIR_INDEX(kernel_vm_end)] = ptepage;

		kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NKPTEPG) & ~(PAGE_SIZE * NKPTEPG - 1);
	}
}

/***************************************************
 * page management routines.
 ***************************************************/

/*
 * free the pv_entry back to the free list
 */
static PMAP_INLINE void
free_pv_entry(pv_entry_t pv)
{
	pv_entry_count--;
	uma_zfree(pvzone, pv);
}

/*
 * get a new pv_entry, allocating a block from the system
 * when needed.
 * the memory allocation is performed bypassing the malloc code
 * because of the possibility of allocations at interrupt time.
 */
static pv_entry_t
get_pv_entry(void)
{
	pv_entry_count++;
	if (pv_entry_high_water &&
		(pv_entry_count > pv_entry_high_water) &&
		(pmap_pagedaemon_waken == 0)) {
		pmap_pagedaemon_waken = 1;
		wakeup (&vm_pages_needed);
	}
	return uma_zalloc(pvzone, M_NOWAIT);
}

/*
 * Add an ia64_lpte to the VHPT.
 */
static void
pmap_enter_vhpt(struct ia64_lpte *pte, vm_offset_t va)
{
	struct ia64_lpte *vhpte;

	pmap_vhpt_inserts++;
	pmap_vhpt_resident++;

	vhpte = (struct ia64_lpte *) ia64_thash(va);

	if (vhpte->pte_chain)
		pmap_vhpt_collisions++;

	pte->pte_chain = vhpte->pte_chain;
	vhpte->pte_chain = ia64_tpa((vm_offset_t) pte);

	if (!vhpte->pte_p && pte->pte_p)
		pmap_install_pte(vhpte, pte);
	else
		ia64_mf();
}

/*
 * Update VHPT after a pte has changed.
 */
static void
pmap_update_vhpt(struct ia64_lpte *pte, vm_offset_t va)
{
	struct ia64_lpte *vhpte;

	vhpte = (struct ia64_lpte *) ia64_thash(va);

	if ((!vhpte->pte_p || vhpte->pte_tag == pte->pte_tag)
	    && pte->pte_p)
		pmap_install_pte(vhpte, pte);
}

/*
 * Remove the ia64_lpte matching va from the VHPT. Return zero if it
 * worked or an appropriate error code otherwise.
 */
static int
pmap_remove_vhpt(vm_offset_t va)
{
	struct ia64_lpte *pte;
	struct ia64_lpte *lpte;
	struct ia64_lpte *vhpte;
	u_int64_t tag;
	int error = ENOENT;

	vhpte = (struct ia64_lpte *) ia64_thash(va);

	/*
	 * If the VHPTE is invalid, there can't be a collision chain.
	 */
	if (!vhpte->pte_p) {
		KASSERT(!vhpte->pte_chain, ("bad vhpte"));
		printf("can't remove vhpt entry for 0x%lx\n", va);
		goto done;
	}

	lpte = vhpte;
	pte = (struct ia64_lpte *) IA64_PHYS_TO_RR7(vhpte->pte_chain);
	tag = ia64_ttag(va);

	while (pte->pte_tag != tag) {
		lpte = pte;
		if (pte->pte_chain)
			pte = (struct ia64_lpte *) IA64_PHYS_TO_RR7(pte->pte_chain);
		else {
			printf("can't remove vhpt entry for 0x%lx\n", va);
			goto done;
		}
	}

	/*
	 * Snip this pv_entry out of the collision chain.
	 */
	lpte->pte_chain = pte->pte_chain;

	/*
	 * If the VHPTE matches as well, change it to map the first
	 * element from the chain if there is one.
	 */
	if (vhpte->pte_tag == tag) {
		if (vhpte->pte_chain) {
			pte = (struct ia64_lpte *)
				IA64_PHYS_TO_RR7(vhpte->pte_chain);
			pmap_install_pte(vhpte, pte);
		} else {
			vhpte->pte_p = 0;
			ia64_mf();
		}
	}

	pmap_vhpt_resident--;
	error = 0;
 done:
	return error;
}

/*
 * Find the ia64_lpte for the given va, if any.
 */
static struct ia64_lpte *
pmap_find_vhpt(vm_offset_t va)
{
	struct ia64_lpte *pte;
	u_int64_t tag;

	pte = (struct ia64_lpte *) ia64_thash(va);
	if (!pte->pte_chain) {
		pte = 0;
		goto done;
	}

	tag = ia64_ttag(va);
	pte = (struct ia64_lpte *) IA64_PHYS_TO_RR7(pte->pte_chain);

	while (pte->pte_tag != tag) {
		if (pte->pte_chain) {
			pte = (struct ia64_lpte *) IA64_PHYS_TO_RR7(pte->pte_chain);
		} else {
			pte = 0;
			break;
		}
	}

 done:
	return pte;
}
	
/*
 * Remove an entry from the list of managed mappings.
 */
static int
pmap_remove_entry(pmap_t pmap, vm_page_t m, vm_offset_t va, pv_entry_t pv)
{
	if (!pv) {
		if (m->md.pv_list_count < pmap->pm_stats.resident_count) {
			TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
				if (pmap == pv->pv_pmap && va == pv->pv_va) 
					break;
			}
		} else {
			TAILQ_FOREACH(pv, &pmap->pm_pvlist, pv_plist) {
				if (va == pv->pv_va) 
					break;
			}
		}
	}

	if (pv) {
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_list);
		m->md.pv_list_count--;
		if (TAILQ_FIRST(&m->md.pv_list) == NULL)
			vm_page_flag_clear(m, PG_WRITEABLE);

		TAILQ_REMOVE(&pmap->pm_pvlist, pv, pv_plist);
		free_pv_entry(pv);
		return 0;
	} else {
		return ENOENT;
	}
}

/*
 * Create a pv entry for page at pa for
 * (pmap, va).
 */
static void
pmap_insert_entry(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	pv_entry_t pv;

	pv = get_pv_entry();
	pv->pv_pmap = pmap;
	pv->pv_va = va;

	TAILQ_INSERT_TAIL(&pmap->pm_pvlist, pv, pv_plist);
	TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_list);
	m->md.pv_list_count++;
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
vm_offset_t 
pmap_extract(pmap, va)
	register pmap_t pmap;
	vm_offset_t va;
{
	pmap_t oldpmap;
	vm_offset_t pa;

	oldpmap = pmap_install(pmap);
	pa = ia64_tpa(va);
	pmap_install(oldpmap);
	return pa;
}

/***************************************************
 * Low level mapping routines.....
 ***************************************************/

/*
 * Find the kernel lpte for mapping the given virtual address, which
 * must be in the part of region 5 which we can cover with our kernel
 * 'page tables'.
 */
static struct ia64_lpte *
pmap_find_kpte(vm_offset_t va)
{
	KASSERT((va >> 61) == 5,
		("kernel mapping 0x%lx not in region 5", va));
	KASSERT(IA64_RR_MASK(va) < (nkpt * PAGE_SIZE * NKPTEPG),
		("kernel mapping 0x%lx out of range", va));
	return &kptdir[KPTE_DIR_INDEX(va)][KPTE_PTE_INDEX(va)];
}

/*
 * Find a pte suitable for mapping a user-space address. If one exists 
 * in the VHPT, that one will be returned, otherwise a new pte is
 * allocated.
 */
static struct ia64_lpte *
pmap_find_pte(vm_offset_t va)
{
	struct ia64_lpte *pte;

	if (va >= VM_MAXUSER_ADDRESS)
		return pmap_find_kpte(va);

	pte = pmap_find_vhpt(va);
	if (!pte) {
		pte = uma_zalloc(ptezone, M_WAITOK);
		pte->pte_p = 0;
	}
	return pte;
}

/*
 * Free a pte which is now unused. This simply returns it to the zone
 * allocator if it is a user mapping. For kernel mappings, clear the
 * valid bit to make it clear that the mapping is not currently used.
 */
static void
pmap_free_pte(struct ia64_lpte *pte, vm_offset_t va)
{
	if (va < VM_MAXUSER_ADDRESS)
		uma_zfree(ptezone, pte);
	else
		pte->pte_p = 0;
}

/*
 * Set a pte to contain a valid mapping and enter it in the VHPT. If
 * the pte was orginally valid, then its assumed to already be in the
 * VHPT.
 */
static void
pmap_set_pte(struct ia64_lpte *pte, vm_offset_t va, vm_offset_t pa,
	     int ig, int pl, int ar)
{
	int wasvalid = pte->pte_p;

	pte->pte_p = 1;
	pte->pte_ma = PTE_MA_WB;
	if (ig & PTE_IG_MANAGED) {
		pte->pte_a = 0;
		pte->pte_d = 0;
	} else {
		pte->pte_a = 1;
		pte->pte_d = 1;
	}
	pte->pte_pl = pl;
	pte->pte_ar = ar;
	pte->pte_ppn = pa >> 12;
	pte->pte_ed = 0;
	pte->pte_ig = ig;

	pte->pte_ps = PAGE_SHIFT;
	pte->pte_key = 0;

	pte->pte_tag = ia64_ttag(va);

	if (wasvalid) {
		pmap_update_vhpt(pte, va);
	} else {
		pmap_enter_vhpt(pte, va);
	}
}

/*
 * If a pte contains a valid mapping, clear it and update the VHPT.
 */
static void
pmap_clear_pte(struct ia64_lpte *pte, vm_offset_t va)
{
	if (pte->pte_p) {
		pmap_remove_vhpt(va);
		ia64_ptc_g(va, PAGE_SHIFT << 2);
		pte->pte_p = 0;
	}
}

/*
 * Remove the (possibly managed) mapping represented by pte from the
 * given pmap.
 */
static int
pmap_remove_pte(pmap_t pmap, struct ia64_lpte *pte, vm_offset_t va,
		pv_entry_t pv, int freepte)
{
	int error;
	vm_page_t m;

	KASSERT((pmap == kernel_pmap || pmap == PCPU_GET(current_pmap)),
		("removing pte for non-current pmap"));

	/*
	 * First remove from the VHPT.
	 */
	error = pmap_remove_vhpt(va);
	if (error)
		return error;

	/*
	 * Make sure pmap_set_pte() knows it isn't in the VHPT.
	 */
	pte->pte_p = 0;

	if (pte->pte_ig & PTE_IG_WIRED)
		pmap->pm_stats.wired_count -= 1;

	pmap->pm_stats.resident_count -= 1;
	if (pte->pte_ig & PTE_IG_MANAGED) {
		m = PHYS_TO_VM_PAGE(pmap_pte_pa(pte));
		if (pte->pte_d)
			if (pmap_track_modified(va))
				vm_page_dirty(m);
		if (pte->pte_a)
			vm_page_flag_set(m, PG_REFERENCED);

		if (freepte)
			pmap_free_pte(pte, va);
		return pmap_remove_entry(pmap, m, va, pv);
	} else {
		if (freepte)
			pmap_free_pte(pte, va);
		return 0;
	}
}

/*
 * Add a list of wired pages to the kva
 * this routine is only used for temporary
 * kernel mappings that do not need to have
 * page modification or references recorded.
 * Note that old mappings are simply written
 * over.  The page *must* be wired.
 */
void
pmap_qenter(vm_offset_t va, vm_page_t *m, int count)
{
	int i;
	struct ia64_lpte *pte;

	for (i = 0; i < count; i++) {
		vm_offset_t tva = va + i * PAGE_SIZE;
		int wasvalid;
		pte = pmap_find_kpte(tva);
		wasvalid = pte->pte_p;
		pmap_set_pte(pte, tva, VM_PAGE_TO_PHYS(m[i]),
			     0, PTE_PL_KERN, PTE_AR_RWX);
		if (wasvalid)
			ia64_ptc_g(tva, PAGE_SHIFT << 2);
	}
}

/*
 * this routine jerks page mappings from the
 * kernel -- it is meant only for temporary mappings.
 */
void
pmap_qremove(vm_offset_t va, int count)
{
	int i;
	struct ia64_lpte *pte;

	for (i = 0; i < count; i++) {
		pte = pmap_find_kpte(va);
		pmap_clear_pte(pte, va);
		va += PAGE_SIZE;
	}
}

/*
 * Add a wired page to the kva.
 */
void 
pmap_kenter(vm_offset_t va, vm_offset_t pa)
{
	struct ia64_lpte *pte;
	int wasvalid;

	pte = pmap_find_kpte(va);
	wasvalid = pte->pte_p;
	pmap_set_pte(pte, va, pa, 0, PTE_PL_KERN, PTE_AR_RWX);
	if (wasvalid)
		ia64_ptc_g(va, PAGE_SHIFT << 2);
}

/*
 * Remove a page from the kva
 */
void
pmap_kremove(vm_offset_t va)
{
	struct ia64_lpte *pte;

	pte = pmap_find_kpte(va);
	pmap_clear_pte(pte, va);
}

/*
 *	Used to map a range of physical addresses into kernel
 *	virtual address space.
 *
 *	The value passed in '*virt' is a suggested virtual address for
 *	the mapping. Architectures which can support a direct-mapped
 *	physical to virtual region can return the appropriate address
 *	within that region, leaving '*virt' unchanged. Other
 *	architectures should map the pages starting at '*virt' and
 *	update '*virt' with the first usable address after the mapped
 *	region.
 */
vm_offset_t
pmap_map(vm_offset_t *virt, vm_offset_t start, vm_offset_t end, int prot)
{
	return IA64_PHYS_TO_RR7(start);
}

/*
 * This routine is very drastic, but can save the system
 * in a pinch.
 */
void
pmap_collect()
{
	int i;
	vm_page_t m;
	static int warningdone = 0;

	if (pmap_pagedaemon_waken == 0)
		return;

	if (warningdone < 5) {
		printf("pmap_collect: collecting pv entries -- suggest increasing PMAP_SHPGPERPROC\n");
		warningdone++;
	}

	for(i = 0; i < vm_page_array_size; i++) {
		m = &vm_page_array[i];
		if (m->wire_count || m->hold_count || m->busy ||
		    (m->flags & (PG_BUSY | PG_UNMANAGED)))
			continue;
		pmap_remove_all(m);
	}
	pmap_pagedaemon_waken = 0;
}

/*
 * Remove a single page from a process address space
 */
static void
pmap_remove_page(pmap_t pmap, vm_offset_t va)
{
	struct ia64_lpte *pte;

	KASSERT((pmap == kernel_pmap || pmap == PCPU_GET(current_pmap)),
		("removing page for non-current pmap"));

	pte = pmap_find_vhpt(va);
	if (pte) {
		pmap_remove_pte(pmap, pte, va, 0, 1);
		pmap_invalidate_page(pmap, va);
	}
	return;
}

/*
 *	Remove the given range of addresses from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the page size.
 */
void
pmap_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	pmap_t oldpmap;
	vm_offset_t va;
	pv_entry_t pv;
	struct ia64_lpte *pte;

	if (pmap == NULL)
		return;

	if (pmap->pm_stats.resident_count == 0)
		return;

	oldpmap = pmap_install(pmap);

	/*
	 * special handling of removing one page.  a very
	 * common operation and easy to short circuit some
	 * code.
	 */
	if (sva + PAGE_SIZE == eva) {
		pmap_remove_page(pmap, sva);
		pmap_install(oldpmap);
		return;
	}

	if (pmap->pm_stats.resident_count < ((eva - sva) >> PAGE_SHIFT)) {
		TAILQ_FOREACH(pv, &pmap->pm_pvlist, pv_plist) {
			va = pv->pv_va;
			if (va >= sva && va < eva) {
				pte = pmap_find_vhpt(va);
				pmap_remove_pte(pmap, pte, va, pv, 1);
				pmap_invalidate_page(pmap, va);
			}
		}
		
	} else {
		for (va = sva; va < eva; va = va += PAGE_SIZE) {
			pte = pmap_find_vhpt(va);
			if (pte) {
				pmap_remove_pte(pmap, pte, va, 0, 1);
				pmap_invalidate_page(pmap, va);
			}
		}
	}

	pmap_install(oldpmap);
}

/*
 *	Routine:	pmap_remove_all
 *	Function:
 *		Removes this physical page from
 *		all physical maps in which it resides.
 *		Reflects back modify bits to the pager.
 *
 *	Notes:
 *		Original versions of this routine were very
 *		inefficient because they iteratively called
 *		pmap_remove (slow...)
 */

static void
pmap_remove_all(vm_page_t m)
{
	pmap_t oldpmap;
	pv_entry_t pv;
	int s;

#if defined(PMAP_DIAGNOSTIC)
	/*
	 * XXX this makes pmap_page_protect(NONE) illegal for non-managed
	 * pages!
	 */
	if (!pmap_initialized || (m->flags & PG_FICTITIOUS)) {
		panic("pmap_page_protect: illegal for unmanaged page, va: 0x%lx", VM_PAGE_TO_PHYS(m));
	}
#endif

	s = splvm();

	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		struct ia64_lpte *pte;
		pmap_t pmap = pv->pv_pmap;
		vm_offset_t va = pv->pv_va;

		oldpmap = pmap_install(pmap);
		pte = pmap_find_vhpt(va);
		if (pmap_pte_pa(pte) != VM_PAGE_TO_PHYS(m))
			panic("pmap_remove_all: pv_table for %lx is inconsistent", VM_PAGE_TO_PHYS(m));
		pmap_remove_pte(pmap, pte, va, pv, 1);
		pmap_invalidate_page(pmap, va);
		pmap_install(oldpmap);
	}

	vm_page_flag_clear(m, PG_WRITEABLE);

	splx(s);
	return;
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	pmap_t oldpmap;
	struct ia64_lpte *pte;
	int newprot;

	if (pmap == NULL)
		return;

	oldpmap = pmap_install(pmap);

	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		pmap_install(oldpmap);
		return;
	}

	if (prot & VM_PROT_WRITE) {
		pmap_install(oldpmap);
		return;
	}

	newprot = pte_prot(pmap, prot);

	if ((sva & PAGE_MASK) || (eva & PAGE_MASK))
		panic("pmap_protect: unaligned addresses");

	while (sva < eva) {
		/* 
		 * If page is invalid, skip this page
		 */
		pte = pmap_find_vhpt(sva);
		if (!pte) {
			sva += PAGE_SIZE;
			continue;
		}

		if (pmap_pte_prot(pte) != newprot) {
			if (pte->pte_ig & PTE_IG_MANAGED) {
				vm_offset_t pa = pmap_pte_pa(pte);
				vm_page_t m = PHYS_TO_VM_PAGE(pa);
				if (pte->pte_d) {
					if (pmap_track_modified(sva))
						vm_page_dirty(m);
					pte->pte_d = 0;
				}
				if (pte->pte_a) {
					vm_page_flag_set(m, PG_REFERENCED);
					pte->pte_a = 0;
				}
			}
			pmap_pte_set_prot(pte, newprot);
			pmap_update_vhpt(pte, sva);
			pmap_invalidate_page(pmap, sva);
		}

		sva += PAGE_SIZE;
	}
	pmap_install(oldpmap);
}

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte can not be reclaimed.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
void
pmap_enter(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
	   boolean_t wired)
{
	pmap_t oldpmap;
	vm_offset_t pa;
	vm_offset_t opa;
	struct ia64_lpte origpte;
	struct ia64_lpte *pte;
	int managed;

	if (pmap == NULL)
		return;

	pmap_ensure_rid(pmap, va);

	oldpmap = pmap_install(pmap);

	va &= ~PAGE_MASK;
#ifdef PMAP_DIAGNOSTIC
	if (va > VM_MAX_KERNEL_ADDRESS)
		panic("pmap_enter: toobig");
#endif

	/*
	 * Find (or create) a pte for the given mapping.
	 */
	pte = pmap_find_pte(va);
	origpte = *pte;

	if (origpte.pte_p)
		opa = pmap_pte_pa(&origpte);
	else
		opa = 0;
	managed = 0;

	pa = VM_PAGE_TO_PHYS(m) & ~PAGE_MASK;

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if (origpte.pte_p && (opa == pa)) {
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is wired,
		 * the PT page will be also.
		 */
		if (wired && ((origpte.pte_ig & PTE_IG_WIRED) == 0))
			pmap->pm_stats.wired_count++;
		else if (!wired && (origpte.pte_ig & PTE_IG_WIRED))
			pmap->pm_stats.wired_count--;

		/*
		 * We might be turning off write access to the page,
		 * so we go ahead and sense modify status.
		 */
		if (origpte.pte_ig & PTE_IG_MANAGED) {
			if (origpte.pte_d && pmap_track_modified(va)) {
				vm_page_t om;
				om = PHYS_TO_VM_PAGE(opa);
				vm_page_dirty(om);
			}
		}

		managed = origpte.pte_ig & PTE_IG_MANAGED;
		goto validate;
	}
	/*
	 * Mapping has changed, invalidate old range and fall
	 * through to handle validating new mapping.
	 */
	if (opa) {
		int error;
		error = pmap_remove_pte(pmap, pte, va, 0, 0);
		if (error)
			panic("pmap_enter: pte vanished, va: 0x%lx", va);
	}

	/*
	 * Enter on the PV list if part of our managed memory.
	 */
	if (pmap_initialized && (m->flags & PG_FICTITIOUS) == 0) {
		pmap_insert_entry(pmap, va, m);
		managed |= PTE_IG_MANAGED;
	}

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;
	if (wired)
		pmap->pm_stats.wired_count++;

validate:

	/*
	 * Now validate mapping with desired protection/wiring. This
	 * adds the pte to the VHPT if necessary.
	 */
	pmap_set_pte(pte, va, pa, managed | (wired ? PTE_IG_WIRED : 0),
		     pte_prot_pl(pmap, prot), pte_prot_ar(pmap, prot));

	/*
	 * if the mapping or permission bits are different, we need
	 * to invalidate the page.
	 */
	if (!pmap_equal_pte(&origpte, pte))
		pmap_invalidate_page(pmap, va);

	pmap_install(oldpmap);
}

/*
 * this code makes some *MAJOR* assumptions:
 * 1. Current pmap & pmap exists.
 * 2. Not wired.
 * 3. Read access.
 * 4. No page table pages.
 * 5. Tlbflush is deferred to calling procedure.
 * 6. Page IS managed.
 * but is *MUCH* faster than pmap_enter...
 */

static void
pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	struct ia64_lpte *pte;
	pmap_t oldpmap;

	pmap_ensure_rid(pmap, va);

	oldpmap = pmap_install(pmap);

	pte = pmap_find_pte(va);
	if (pte->pte_p)
		return;

	/*
	 * Enter on the PV list since its part of our managed memory.
	 */
	pmap_insert_entry(pmap, va, m);

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;

	/*
	 * Initialise PTE with read-only protection and enter into VHPT.
	 */
	pmap_set_pte(pte, va, VM_PAGE_TO_PHYS(m),
		     PTE_IG_MANAGED,
		     PTE_PL_USER, PTE_AR_R);

	pmap_install(oldpmap);
}

/*
 * Make temporary mapping for a physical address. This is called
 * during dump.
 */
void *
pmap_kenter_temporary(vm_offset_t pa, int i)
{
	return (void *) IA64_PHYS_TO_RR7(pa - (i * PAGE_SIZE));
}

#define MAX_INIT_PT (96)
/*
 * pmap_object_init_pt preloads the ptes for a given object
 * into the specified pmap.  This eliminates the blast of soft
 * faults on process startup and immediately after an mmap.
 */
void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr,
		    vm_object_t object, vm_pindex_t pindex,
		    vm_size_t size, int limit)
{
	pmap_t oldpmap;
	vm_offset_t tmpidx;
	int psize;
	vm_page_t p;
	int objpgs;

	if (pmap == NULL || object == NULL)
		return;

	oldpmap = pmap_install(pmap);

	psize = ia64_btop(size);

	if ((object->type != OBJT_VNODE) ||
		((limit & MAP_PREFAULT_PARTIAL) && (psize > MAX_INIT_PT) &&
			(object->resident_page_count > MAX_INIT_PT))) {
		pmap_install(oldpmap);
		return;
	}

	if (psize + pindex > object->size) {
		if (object->size < pindex)
			return;
		psize = object->size - pindex;
	}

	/*
	 * if we are processing a major portion of the object, then scan the
	 * entire thing.
	 */
	if (psize > (object->resident_page_count >> 2)) {
		objpgs = psize;

		for (p = TAILQ_FIRST(&object->memq);
		    ((objpgs > 0) && (p != NULL));
		    p = TAILQ_NEXT(p, listq)) {

			tmpidx = p->pindex;
			if (tmpidx < pindex) {
				continue;
			}
			tmpidx -= pindex;
			if (tmpidx >= psize) {
				continue;
			}
			/*
			 * don't allow an madvise to blow away our really
			 * free pages allocating pv entries.
			 */
			if ((limit & MAP_PREFAULT_MADVISE) &&
			    cnt.v_free_count < cnt.v_free_reserved) {
				break;
			}
			vm_page_lock_queues();
			if (((p->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL) &&
				(p->busy == 0) &&
			    (p->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {
				if ((p->queue - p->pc) == PQ_CACHE)
					vm_page_deactivate(p);
				vm_page_busy(p);
				vm_page_unlock_queues();
				pmap_enter_quick(pmap,
						 addr + ia64_ptob(tmpidx), p);
				vm_page_lock_queues();
				vm_page_wakeup(p);
			}
			vm_page_unlock_queues();
			objpgs -= 1;
		}
	} else {
		/*
		 * else lookup the pages one-by-one.
		 */
		for (tmpidx = 0; tmpidx < psize; tmpidx += 1) {
			/*
			 * don't allow an madvise to blow away our really
			 * free pages allocating pv entries.
			 */
			if ((limit & MAP_PREFAULT_MADVISE) &&
			    cnt.v_free_count < cnt.v_free_reserved) {
				break;
			}
			p = vm_page_lookup(object, tmpidx + pindex);
			if (p == NULL)
				continue;
			vm_page_lock_queues();
			if ((p->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL &&
				(p->busy == 0) &&
			    (p->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {
				if ((p->queue - p->pc) == PQ_CACHE)
					vm_page_deactivate(p);
				vm_page_busy(p);
				vm_page_unlock_queues();
				pmap_enter_quick(pmap,
						 addr + ia64_ptob(tmpidx), p);
				vm_page_lock_queues();
				vm_page_wakeup(p);
			}
			vm_page_unlock_queues();
		}
	}
	pmap_install(oldpmap);
	return;
}

/*
 * pmap_prefault provides a quick way of clustering
 * pagefaults into a processes address space.  It is a "cousin"
 * of pmap_object_init_pt, except it runs at page fault time instead
 * of mmap time.
 */
#define PFBAK 4
#define PFFOR 4
#define PAGEORDER_SIZE (PFBAK+PFFOR)

static int pmap_prefault_pageorder[] = {
	-1 * PAGE_SIZE, 1 * PAGE_SIZE,
	-2 * PAGE_SIZE, 2 * PAGE_SIZE,
	-3 * PAGE_SIZE, 3 * PAGE_SIZE,
	-4 * PAGE_SIZE, 4 * PAGE_SIZE
};

void
pmap_prefault(pmap, addra, entry)
	pmap_t pmap;
	vm_offset_t addra;
	vm_map_entry_t entry;
{
	int i;
	vm_offset_t starta;
	vm_offset_t addr;
	vm_pindex_t pindex;
	vm_page_t m, mpte;
	vm_object_t object;

	if (!curthread || (pmap != vmspace_pmap(curthread->td_proc->p_vmspace)))
		return;

	object = entry->object.vm_object;

	starta = addra - PFBAK * PAGE_SIZE;
	if (starta < entry->start) {
		starta = entry->start;
	} else if (starta > addra) {
		starta = 0;
	}

	mpte = NULL;
	for (i = 0; i < PAGEORDER_SIZE; i++) {
		vm_object_t lobject;
		struct ia64_lpte *pte;

		addr = addra + pmap_prefault_pageorder[i];
		if (addr > addra + (PFFOR * PAGE_SIZE))
			addr = 0;

		if (addr < starta || addr >= entry->end)
			continue;

		pte = pmap_find_vhpt(addr);
		if (pte && pte->pte_p)
			continue;

		pindex = ((addr - entry->start) + entry->offset) >> PAGE_SHIFT;
		lobject = object;
		for (m = vm_page_lookup(lobject, pindex);
		    (!m && (lobject->type == OBJT_DEFAULT) && (lobject->backing_object));
		    lobject = lobject->backing_object) {
			if (lobject->backing_object_offset & PAGE_MASK)
				break;
			pindex += (lobject->backing_object_offset >> PAGE_SHIFT);
			m = vm_page_lookup(lobject->backing_object, pindex);
		}

		/*
		 * give-up when a page is not in memory
		 */
		if (m == NULL)
			break;
		vm_page_lock_queues();
		if (((m->valid & VM_PAGE_BITS_ALL) == VM_PAGE_BITS_ALL) &&
			(m->busy == 0) &&
		    (m->flags & (PG_BUSY | PG_FICTITIOUS)) == 0) {

			if ((m->queue - m->pc) == PQ_CACHE) {
				vm_page_deactivate(m);
			}
			vm_page_busy(m);
			vm_page_unlock_queues();
			pmap_enter_quick(pmap, addr, m);
			vm_page_lock_queues();
			vm_page_wakeup(m);
		}
		vm_page_unlock_queues();
	}
}

/*
 *	Routine:	pmap_change_wiring
 *	Function:	Change the wiring attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 */
void
pmap_change_wiring(pmap, va, wired)
	register pmap_t pmap;
	vm_offset_t va;
	boolean_t wired;
{
	pmap_t oldpmap;
	struct ia64_lpte *pte;

	if (pmap == NULL)
		return;

	oldpmap = pmap_install(pmap);

	pte = pmap_find_vhpt(va);

	if (wired && !pmap_pte_w(pte))
		pmap->pm_stats.wired_count++;
	else if (!wired && pmap_pte_w(pte))
		pmap->pm_stats.wired_count--;

	/*
	 * Wiring is not a hardware characteristic so there is no need to
	 * invalidate TLB.
	 */
	pmap_pte_set_w(pte, wired);

	pmap_install(oldpmap);
}



/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr, vm_size_t len,
	  vm_offset_t src_addr)
{
}	


/*
 *	pmap_zero_page zeros the specified hardware page by
 *	mapping it into virtual memory and using bzero to clear
 *	its contents.
 */

void
pmap_zero_page(vm_page_t m)
{
	vm_offset_t va = IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(m));
	bzero((caddr_t) va, PAGE_SIZE);
}


/*
 *	pmap_zero_page_area zeros the specified hardware page by
 *	mapping it into virtual memory and using bzero to clear
 *	its contents.
 *
 *	off and size must reside within a single page.
 */

void
pmap_zero_page_area(vm_page_t m, int off, int size)
{
	vm_offset_t va = IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(m));
	bzero((char *)(caddr_t)va + off, size);
}


/*
 *	pmap_zero_page_idle zeros the specified hardware page by
 *	mapping it into virtual memory and using bzero to clear
 *	its contents.  This is for the vm_idlezero process.
 */

void
pmap_zero_page_idle(vm_page_t m)
{
	vm_offset_t va = IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(m));
	bzero((caddr_t) va, PAGE_SIZE);
}


/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 */
void
pmap_copy_page(vm_page_t msrc, vm_page_t mdst)
{
	vm_offset_t src = IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(msrc));
	vm_offset_t dst = IA64_PHYS_TO_RR7(VM_PAGE_TO_PHYS(mdst));
	bcopy((caddr_t) src, (caddr_t) dst, PAGE_SIZE);
}

/*
 * Returns true if the pmap's pv is one of the first
 * 16 pvs linked to from this page.  This count may
 * be changed upwards or downwards in the future; it
 * is only necessary that true be returned for a small
 * subset of pmaps for proper page aging.
 */
boolean_t
pmap_page_exists_quick(pmap_t pmap, vm_page_t m)
{
	pv_entry_t pv;
	int loops = 0;
	int s;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return FALSE;

	s = splvm();

	/*
	 * Not found, check current mappings returning immediately if found.
	 */
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		if (pv->pv_pmap == pmap) {
			splx(s);
			return TRUE;
		}
		loops++;
		if (loops >= 16)
			break;
	}
	splx(s);
	return (FALSE);
}

#define PMAP_REMOVE_PAGES_CURPROC_ONLY
/*
 * Remove all pages from specified address space
 * this aids process exit speeds.  Also, this code
 * is special cased for current process only, but
 * can have the more generic (and slightly slower)
 * mode enabled.  This is much faster than pmap_remove
 * in the case of running down an entire address space.
 */
void
pmap_remove_pages(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	pv_entry_t pv, npv;
	int s;

#ifdef PMAP_REMOVE_PAGES_CURPROC_ONLY
	if (!curthread || (pmap != vmspace_pmap(curthread->td_proc->p_vmspace))) {
		printf("warning: pmap_remove_pages called with non-current pmap\n");
		return;
	}
#endif

	s = splvm();
	for (pv = TAILQ_FIRST(&pmap->pm_pvlist);
		pv;
		pv = npv) {
		struct ia64_lpte *pte;

		npv = TAILQ_NEXT(pv, pv_plist);

		if (pv->pv_va >= eva || pv->pv_va < sva) {
			continue;
		}

		pte = pmap_find_vhpt(pv->pv_va);
		if (!pte)
			panic("pmap_remove_pages: page on pm_pvlist has no pte\n");


/*
 * We cannot remove wired pages from a process' mapping at this time
 */
		if (pte->pte_ig & PTE_IG_WIRED) {
			continue;
		}

		pmap_remove_pte(pmap, pte, pv->pv_va, pv, 1);
	}
	splx(s);

	pmap_invalidate_all(pmap);
}

/*
 *      pmap_page_protect:
 *
 *      Lower the permission for all mappings to a given page.
 */
void
pmap_page_protect(vm_page_t m, vm_prot_t prot)
{
	pv_entry_t pv;

	if ((prot & VM_PROT_WRITE) != 0)
		return;
	if (prot & (VM_PROT_READ | VM_PROT_EXECUTE)) {
		TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
			int newprot = pte_prot(pv->pv_pmap, prot);
			pmap_t oldpmap = pmap_install(pv->pv_pmap);
			struct ia64_lpte *pte;
			pte = pmap_find_vhpt(pv->pv_va);
			pmap_pte_set_prot(pte, newprot);
			pmap_update_vhpt(pte, pv->pv_va);
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
			pmap_install(oldpmap);
		}
	} else {
		pmap_remove_all(m);
	}
}

vm_offset_t
pmap_phys_address(int ppn)
{
	return (ia64_ptob(ppn));
}

/*
 *	pmap_ts_referenced:
 *
 *	Return a count of reference bits for a page, clearing those bits.
 *	It is not necessary for every reference bit to be cleared, but it
 *	is necessary that 0 only be returned when there are truly no
 *	reference bits set.
 * 
 *	XXX: The exact number of bits to check and clear is a matter that
 *	should be tested and standardized at some point in the future for
 *	optimal aging of shared pages.
 */
int
pmap_ts_referenced(vm_page_t m)
{
	pv_entry_t pv;
	int count = 0;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return 0;

	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		pmap_t oldpmap = pmap_install(pv->pv_pmap);
		struct ia64_lpte *pte;
		pte = pmap_find_vhpt(pv->pv_va);
		if (pte->pte_a) {
			count++;
			pte->pte_a = 0;
			pmap_update_vhpt(pte, pv->pv_va);
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
		}
		pmap_install(oldpmap);
	}

	return count;
}

#if 0
/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page was referenced
 *	in any physical maps.
 */
static boolean_t
pmap_is_referenced(vm_page_t m)
{
	pv_entry_t pv;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return FALSE;

	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		pmap_t oldpmap = pmap_install(pv->pv_pmap);
		struct ia64_lpte *pte = pmap_find_vhpt(pv->pv_va);
		pmap_install(oldpmap);
		if (pte->pte_a)
			return 1;
	}

	return 0;
}
#endif

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page was modified
 *	in any physical maps.
 */
boolean_t
pmap_is_modified(vm_page_t m)
{
	pv_entry_t pv;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return FALSE;

	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		pmap_t oldpmap = pmap_install(pv->pv_pmap);
		struct ia64_lpte *pte = pmap_find_vhpt(pv->pv_va);
		pmap_install(oldpmap);
		if (pte->pte_d)
			return 1;
	}

	return 0;
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_page_t m)
{
	pv_entry_t pv;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return;

	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		pmap_t oldpmap = pmap_install(pv->pv_pmap);
		struct ia64_lpte *pte = pmap_find_vhpt(pv->pv_va);
		if (pte->pte_d) {
			pte->pte_d = 0;
			pmap_update_vhpt(pte, pv->pv_va);
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
		}
		pmap_install(oldpmap);
	}
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */
void
pmap_clear_reference(vm_page_t m)
{
	pv_entry_t pv;

	if (!pmap_initialized || (m->flags & PG_FICTITIOUS))
		return;

	TAILQ_FOREACH(pv, &m->md.pv_list, pv_list) {
		pmap_t oldpmap = pmap_install(pv->pv_pmap);
		struct ia64_lpte *pte = pmap_find_vhpt(pv->pv_va);
		if (pte->pte_a) {
			pte->pte_a = 0;
			pmap_update_vhpt(pte, pv->pv_va);
			pmap_invalidate_page(pv->pv_pmap, pv->pv_va);
		}
		pmap_install(oldpmap);
	}
}

/*
 * Miscellaneous support routines follow
 */

static void
ia64_protection_init()
{
	int prot, *kp, *up;

	kp = protection_codes[0];
	up = protection_codes[1];

	for (prot = 0; prot < 8; prot++) {
		switch (prot) {
		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_NONE:
			*kp++ = (PTE_AR_R << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_R << 2) | PTE_PL_KERN;
			break;

		case VM_PROT_NONE | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = (PTE_AR_X_RX << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_X_RX << 2) | PTE_PL_USER;
			break;

		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_NONE:
			*kp++ = (PTE_AR_RW << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_RW << 2) | PTE_PL_USER;
			break;

		case VM_PROT_NONE | VM_PROT_WRITE | VM_PROT_EXECUTE:
			*kp++ = (PTE_AR_RWX << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_RWX << 2) | PTE_PL_USER;
			break;

		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_NONE:
			*kp++ = (PTE_AR_R << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_R << 2) | PTE_PL_USER;
			break;

		case VM_PROT_READ | VM_PROT_NONE | VM_PROT_EXECUTE:
			*kp++ = (PTE_AR_RX << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_RX << 2) | PTE_PL_USER;
			break;

		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_NONE:
			*kp++ = (PTE_AR_RW << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_RW << 2) | PTE_PL_USER;
			break;

		case VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE:
			*kp++ = (PTE_AR_RWX << 2) | PTE_PL_KERN;
			*up++ = (PTE_AR_RWX << 2) | PTE_PL_USER;
			break;
		}
	}
}

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory.
 */
void *
pmap_mapdev(vm_offset_t pa, vm_size_t size)
{
	return (void*) IA64_PHYS_TO_RR6(pa);
}

/*
 * 'Unmap' a range mapped by pmap_mapdev().
 */
void
pmap_unmapdev(vm_offset_t va, vm_size_t size)
{
	return;
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap_t pmap, vm_offset_t addr)
{
	pmap_t oldpmap;
	struct ia64_lpte *pte;
	int val = 0;
	
	oldpmap = pmap_install(pmap);
	pte = pmap_find_vhpt(addr);
	pmap_install(oldpmap);

	if (!pte)
		return 0;

	if (pmap_pte_v(pte)) {
		vm_page_t m;
		vm_offset_t pa;

		val = MINCORE_INCORE;
		if ((pte->pte_ig & PTE_IG_MANAGED) == 0)
			return val;

		pa = pmap_pte_pa(pte);

		m = PHYS_TO_VM_PAGE(pa);

		/*
		 * Modified by us
		 */
		if (pte->pte_d)
			val |= MINCORE_MODIFIED|MINCORE_MODIFIED_OTHER;
		/*
		 * Modified by someone
		 */
		else if (pmap_is_modified(m))
			val |= MINCORE_MODIFIED_OTHER;
		/*
		 * Referenced by us
		 */
		if (pte->pte_a)
			val |= MINCORE_REFERENCED|MINCORE_REFERENCED_OTHER;

		/*
		 * Referenced by someone
		 */
		else if (pmap_ts_referenced(m)) {
			val |= MINCORE_REFERENCED_OTHER;
			vm_page_flag_set(m, PG_REFERENCED);
		}
	} 
	return val;
}

void
pmap_activate(struct thread *td)
{
	pmap_install(vmspace_pmap(td->td_proc->p_vmspace));
}

pmap_t
pmap_install(pmap_t pmap)
{
	pmap_t oldpmap;
	int i;

	critical_enter();

	oldpmap = PCPU_GET(current_pmap);

	if (pmap == oldpmap || pmap == kernel_pmap) {
		critical_exit();
		return pmap;
	}

	if (oldpmap) {
		atomic_clear_32(&pmap->pm_active, PCPU_GET(cpumask));
	}

	PCPU_SET(current_pmap, pmap);
	if (!pmap) {
		/*
		 * RIDs 0..4 have no mappings to make sure we generate 
		 * page faults on accesses.
		 */
		ia64_set_rr(IA64_RR_BASE(0), (0 << 8)|(PAGE_SHIFT << 2)|1);
		ia64_set_rr(IA64_RR_BASE(1), (1 << 8)|(PAGE_SHIFT << 2)|1);
		ia64_set_rr(IA64_RR_BASE(2), (2 << 8)|(PAGE_SHIFT << 2)|1);
		ia64_set_rr(IA64_RR_BASE(3), (3 << 8)|(PAGE_SHIFT << 2)|1);
		ia64_set_rr(IA64_RR_BASE(4), (4 << 8)|(PAGE_SHIFT << 2)|1);
		critical_exit();
		return oldpmap;
	}

	atomic_set_32(&pmap->pm_active, PCPU_GET(cpumask));

	for (i = 0; i < 5; i++)
		ia64_set_rr(IA64_RR_BASE(i),
			    (pmap->pm_rid[i] << 8)|(PAGE_SHIFT << 2)|1);

	critical_exit();
	return oldpmap;
}

vm_offset_t
pmap_addr_hint(vm_object_t obj, vm_offset_t addr, vm_size_t size)
{

	return addr;
}

#include "opt_ddb.h"

#ifdef DDB

#include <ddb/ddb.h>

static const char*	psnames[] = {
	"1B",	"2B",	"4B",	"8B",
	"16B",	"32B",	"64B",	"128B",
	"256B",	"512B",	"1K",	"2K",
	"4K",	"8K",	"16K",	"32K",
	"64K",	"128K",	"256K",	"512K",
	"1M",	"2M",	"4M",	"8M",
	"16M",	"32M",	"64M",	"128M",
	"256M",	"512M",	"1G",	"2G"
};

static void
print_trs(int type)
{
	struct ia64_pal_result	res;
	int			i, maxtr;
	struct {
		struct ia64_pte	pte;
		struct ia64_itir itir;
		struct ia64_ifa ifa;
		struct ia64_rr	rr;
	}			buf;
	static const char*	manames[] = {
		"WB",	"bad",	"bad",	"bad",
		"UC",	"UCE",	"WC",	"NaT",
		
	};

	res = ia64_call_pal_static(PAL_VM_SUMMARY, 0, 0, 0);
	if (res.pal_status != 0) {
		db_printf("Can't get VM summary\n");
		return;
	}

	if (type == 0)
		maxtr = (res.pal_result[0] >> 40) & 0xff;
	else
		maxtr = (res.pal_result[0] >> 32) & 0xff;

	db_printf("V RID    Virtual Page  Physical Page PgSz ED AR PL D A MA  P KEY\n");
	for (i = 0; i <= maxtr; i++) {
		bzero(&buf, sizeof(buf));
		res = ia64_call_pal_stacked_physical
			(PAL_VM_TR_READ, i, type, ia64_tpa((u_int64_t) &buf));
		if (!(res.pal_result[0] & 1))
			buf.pte.pte_ar = 0;
		if (!(res.pal_result[0] & 2))
			buf.pte.pte_pl = 0;
		if (!(res.pal_result[0] & 4))
			buf.pte.pte_d = 0;
		if (!(res.pal_result[0] & 8))
			buf.pte.pte_ma = 0;
		db_printf(
			"%d %06x %013lx %013lx %4s %d  %d  %d  %d %d %-3s %d %06x\n",
			buf.ifa.ifa_ig & 1,
			buf.rr.rr_rid,
			buf.ifa.ifa_vpn,
			buf.pte.pte_ppn,
			psnames[buf.itir.itir_ps],
			buf.pte.pte_ed,
			buf.pte.pte_ar,
			buf.pte.pte_pl,
			buf.pte.pte_d,
			buf.pte.pte_a,
			manames[buf.pte.pte_ma],
			buf.pte.pte_p,
			buf.itir.itir_key);
	}
}

DB_COMMAND(itr, db_itr)
{
	print_trs(0);
}

DB_COMMAND(dtr, db_dtr)
{
	print_trs(1);
}

DB_COMMAND(rr, db_rr)
{
	int i;
	u_int64_t t;
	struct ia64_rr rr;

	printf("RR RID    PgSz VE\n");
	for (i = 0; i < 8; i++) {
		__asm __volatile ("mov %0=rr[%1]"
				  : "=r"(t)
				  : "r"(IA64_RR_BASE(i)));
		*(u_int64_t *) &rr = t;
		printf("%d  %06x %4s %d\n",
		       i, rr.rr_rid, psnames[rr.rr_ps], rr.rr_ve);
	}
}

DB_COMMAND(thash, db_thash)
{
	if (!have_addr)
		return;

	db_printf("%p\n", (void *) ia64_thash(addr));
}

DB_COMMAND(ttag, db_ttag)
{
	if (!have_addr)
		return;

	db_printf("0x%lx\n", ia64_ttag(addr));
}

#endif
