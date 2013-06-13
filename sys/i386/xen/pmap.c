/*-
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 * Copyright (c) 2005 Alan L. Cox <alc@cs.rice.edu>
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
 */
/*-
 * Copyright (c) 2003 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Jake Burkholder,
 * Safeport Network Services, and Network Associates Laboratories, the
 * Security Research Division of Network Associates, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 *	Manages physical address maps.
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

#include "opt_cpu.h"
#include "opt_pmap.h"
#include "opt_smp.h"
#include "opt_xbox.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/sf_buf.h>
#include <sys/sx.h>
#include <sys/vmmeter.h>
#include <sys/sched.h>
#include <sys/sysctl.h>
#ifdef SMP
#include <sys/smp.h>
#else
#include <sys/cpuset.h>
#endif

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

#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/specialreg.h>
#ifdef SMP
#include <machine/smp.h>
#endif

#ifdef XBOX
#include <machine/xbox.h>
#endif

#include <xen/interface/xen.h>
#include <xen/hypervisor.h>
#include <machine/xen/hypercall.h>
#include <machine/xen/xenvar.h>
#include <machine/xen/xenfunc.h>

#if !defined(CPU_DISABLE_SSE) && defined(I686_CPU)
#define CPU_ENABLE_SSE
#endif

#ifndef PMAP_SHPGPERPROC
#define PMAP_SHPGPERPROC 200
#endif

#define DIAGNOSTIC

#if !defined(DIAGNOSTIC)
#ifdef __GNUC_GNU_INLINE__
#define PMAP_INLINE	__attribute__((__gnu_inline__)) inline
#else
#define PMAP_INLINE	extern inline
#endif
#else
#define PMAP_INLINE
#endif

#ifdef PV_STATS
#define PV_STAT(x)	do { x ; } while (0)
#else
#define PV_STAT(x)	do { } while (0)
#endif

/*
 * Get PDEs and PTEs for user/kernel address space
 */
#define	pmap_pde(m, v)	(&((m)->pm_pdir[(vm_offset_t)(v) >> PDRSHIFT]))
#define pdir_pde(m, v) (m[(vm_offset_t)(v) >> PDRSHIFT])

#define pmap_pde_v(pte)		((*(int *)pte & PG_V) != 0)
#define pmap_pte_w(pte)		((*(int *)pte & PG_W) != 0)
#define pmap_pte_m(pte)		((*(int *)pte & PG_M) != 0)
#define pmap_pte_u(pte)		((*(int *)pte & PG_A) != 0)
#define pmap_pte_v(pte)		((*(int *)pte & PG_V) != 0)

#define pmap_pte_set_prot(pte, v) ((*(int *)pte &= ~PG_PROT), (*(int *)pte |= (v)))

#define HAMFISTED_LOCKING
#ifdef HAMFISTED_LOCKING
static struct mtx createdelete_lock;
#endif

struct pmap kernel_pmap_store;
LIST_HEAD(pmaplist, pmap);
static struct pmaplist allpmaps;
static struct mtx allpmaps_lock;

vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */
int pgeflag = 0;		/* PG_G or-in */
int pseflag = 0;		/* PG_PS or-in */

int nkpt;
vm_offset_t kernel_vm_end;
extern u_int32_t KERNend;

#ifdef PAE
pt_entry_t pg_nx;
#endif

static SYSCTL_NODE(_vm, OID_AUTO, pmap, CTLFLAG_RD, 0, "VM/pmap parameters");

static int pat_works;			/* Is page attribute table sane? */

/*
 * This lock is defined as static in other pmap implementations.  It cannot,
 * however, be defined as static here, because it is (ab)used to serialize
 * queued page table changes in other sources files.
 */
struct rwlock pvh_global_lock;

/*
 * Data for the pv entry allocation mechanism
 */
static TAILQ_HEAD(pch, pv_chunk) pv_chunks = TAILQ_HEAD_INITIALIZER(pv_chunks);
static int pv_entry_count = 0, pv_entry_max = 0, pv_entry_high_water = 0;
static int shpgperproc = PMAP_SHPGPERPROC;

struct pv_chunk *pv_chunkbase;		/* KVA block for pv_chunks */
int pv_maxchunks;			/* How many chunks we have KVA for */
vm_offset_t pv_vafree;			/* freelist stored in the PTE */

/*
 * All those kernel PT submaps that BSD is so fond of
 */
struct sysmaps {
	struct	mtx lock;
	pt_entry_t *CMAP1;
	pt_entry_t *CMAP2;
	caddr_t	CADDR1;
	caddr_t	CADDR2;
};
static struct sysmaps sysmaps_pcpu[MAXCPU];
static pt_entry_t *CMAP3;
caddr_t ptvmmap = 0;
static caddr_t CADDR3;
struct msgbuf *msgbufp = 0;

/*
 * Crashdump maps.
 */
static caddr_t crashdumpmap;

static pt_entry_t *PMAP1 = 0, *PMAP2;
static pt_entry_t *PADDR1 = 0, *PADDR2;
#ifdef SMP
static int PMAP1cpu;
static int PMAP1changedcpu;
SYSCTL_INT(_debug, OID_AUTO, PMAP1changedcpu, CTLFLAG_RD, 
	   &PMAP1changedcpu, 0,
	   "Number of times pmap_pte_quick changed CPU with same PMAP1");
#endif
static int PMAP1changed;
SYSCTL_INT(_debug, OID_AUTO, PMAP1changed, CTLFLAG_RD, 
	   &PMAP1changed, 0,
	   "Number of times pmap_pte_quick changed PMAP1");
static int PMAP1unchanged;
SYSCTL_INT(_debug, OID_AUTO, PMAP1unchanged, CTLFLAG_RD, 
	   &PMAP1unchanged, 0,
	   "Number of times pmap_pte_quick didn't change PMAP1");
static struct mtx PMAP2mutex;

static void	free_pv_chunk(struct pv_chunk *pc);
static void	free_pv_entry(pmap_t pmap, pv_entry_t pv);
static pv_entry_t get_pv_entry(pmap_t pmap, boolean_t try);
static void	pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va);
static pv_entry_t pmap_pvh_remove(struct md_page *pvh, pmap_t pmap,
		    vm_offset_t va);

static vm_page_t pmap_enter_quick_locked(multicall_entry_t **mcl, int *count, pmap_t pmap, vm_offset_t va,
    vm_page_t m, vm_prot_t prot, vm_page_t mpte);
static void pmap_flush_page(vm_page_t m);
static void pmap_kenter_attr(vm_offset_t va, vm_paddr_t pa, int mode);
static int pmap_remove_pte(pmap_t pmap, pt_entry_t *ptq, vm_offset_t sva,
    vm_page_t *free);
static void pmap_remove_page(struct pmap *pmap, vm_offset_t va,
    vm_page_t *free);
static void pmap_remove_entry(struct pmap *pmap, vm_page_t m,
					vm_offset_t va);
static boolean_t pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va,
    vm_page_t m);

static vm_page_t pmap_allocpte(pmap_t pmap, vm_offset_t va, int flags);

static vm_page_t _pmap_allocpte(pmap_t pmap, u_int ptepindex, int flags);
static void _pmap_unwire_ptp(pmap_t pmap, vm_page_t m, vm_page_t *free);
static pt_entry_t *pmap_pte_quick(pmap_t pmap, vm_offset_t va);
static void pmap_pte_release(pt_entry_t *pte);
static int pmap_unuse_pt(pmap_t, vm_offset_t, vm_page_t *);
static boolean_t pmap_is_prefaultable_locked(pmap_t pmap, vm_offset_t addr);

static __inline void pagezero(void *page);

CTASSERT(1 << PDESHIFT == sizeof(pd_entry_t));
CTASSERT(1 << PTESHIFT == sizeof(pt_entry_t));

/*
 * If you get an error here, then you set KVA_PAGES wrong! See the
 * description of KVA_PAGES in sys/i386/include/pmap.h. It must be
 * multiple of 4 for a normal kernel, or a multiple of 8 for a PAE.
 */
CTASSERT(KERNBASE % (1 << 24) == 0);

void 
pd_set(struct pmap *pmap, int ptepindex, vm_paddr_t val, int type)
{
	vm_paddr_t pdir_ma = vtomach(&pmap->pm_pdir[ptepindex]);
	
	switch (type) {
	case SH_PD_SET_VA:
#if 0		
		xen_queue_pt_update(shadow_pdir_ma,
				    xpmap_ptom(val & ~(PG_RW)));
#endif		
		xen_queue_pt_update(pdir_ma,
				    xpmap_ptom(val)); 	
		break;
	case SH_PD_SET_VA_MA:
#if 0		
		xen_queue_pt_update(shadow_pdir_ma,
				    val & ~(PG_RW));
#endif		
		xen_queue_pt_update(pdir_ma, val); 	
		break;
	case SH_PD_SET_VA_CLEAR:
#if 0
		xen_queue_pt_update(shadow_pdir_ma, 0);
#endif		
		xen_queue_pt_update(pdir_ma, 0); 	
		break;
	}
}

/*
 *	Bootstrap the system enough to run with virtual memory.
 *
 *	On the i386 this is called after mapping has already been enabled
 *	and just syncs the pmap module with what has already been done.
 *	[We can't call it easily with mapping off since the kernel is not
 *	mapped with PA == VA, hence we would have to relocate every address
 *	from the linked base (virtual) address "KERNBASE" to the actual
 *	(physical) address starting relative to 0]
 */
void
pmap_bootstrap(vm_paddr_t firstaddr)
{
	vm_offset_t va;
	pt_entry_t *pte, *unused;
	struct sysmaps *sysmaps;
	int i;

	/*
	 * Initialize the first available kernel virtual address.  However,
	 * using "firstaddr" may waste a few pages of the kernel virtual
	 * address space, because locore may not have mapped every physical
	 * page that it allocated.  Preferably, locore would provide a first
	 * unused virtual address in addition to "firstaddr".
	 */
	virtual_avail = (vm_offset_t) KERNBASE + firstaddr;

	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Initialize the kernel pmap (which is statically allocated).
	 */
	PMAP_LOCK_INIT(kernel_pmap);
	kernel_pmap->pm_pdir = (pd_entry_t *) (KERNBASE + (u_int)IdlePTD);
#ifdef PAE
	kernel_pmap->pm_pdpt = (pdpt_entry_t *) (KERNBASE + (u_int)IdlePDPT);
#endif
	CPU_FILL(&kernel_pmap->pm_active);	/* don't allow deactivation */
	TAILQ_INIT(&kernel_pmap->pm_pvchunk);

 	/*
	 * Initialize the global pv list lock.
	 */
	rw_init_flags(&pvh_global_lock, "pmap pv global", RW_RECURSE);

	LIST_INIT(&allpmaps);
	mtx_init(&allpmaps_lock, "allpmaps", NULL, MTX_SPIN);
	mtx_lock_spin(&allpmaps_lock);
	LIST_INSERT_HEAD(&allpmaps, kernel_pmap, pm_list);
	mtx_unlock_spin(&allpmaps_lock);
	if (nkpt == 0)
		nkpt = NKPT;

	/*
	 * Reserve some special page table entries/VA space for temporary
	 * mapping of pages.
	 */
#define	SYSMAP(c, p, v, n)	\
	v = (c)va; va += ((n)*PAGE_SIZE); p = pte; pte += (n);

	va = virtual_avail;
	pte = vtopte(va);

	/*
	 * CMAP1/CMAP2 are used for zeroing and copying pages.
	 * CMAP3 is used for the idle process page zeroing.
	 */
	for (i = 0; i < MAXCPU; i++) {
		sysmaps = &sysmaps_pcpu[i];
		mtx_init(&sysmaps->lock, "SYSMAPS", NULL, MTX_DEF);
		SYSMAP(caddr_t, sysmaps->CMAP1, sysmaps->CADDR1, 1)
		SYSMAP(caddr_t, sysmaps->CMAP2, sysmaps->CADDR2, 1)
		PT_SET_MA(sysmaps->CADDR1, 0);
		PT_SET_MA(sysmaps->CADDR2, 0);
	}
	SYSMAP(caddr_t, CMAP3, CADDR3, 1)
	PT_SET_MA(CADDR3, 0);

	/*
	 * Crashdump maps.
	 */
	SYSMAP(caddr_t, unused, crashdumpmap, MAXDUMPPGS)

	/*
	 * ptvmmap is used for reading arbitrary physical pages via /dev/mem.
	 */
	SYSMAP(caddr_t, unused, ptvmmap, 1)

	/*
	 * msgbufp is used to map the system message buffer.
	 */
	SYSMAP(struct msgbuf *, unused, msgbufp, atop(round_page(msgbufsize)))

	/*
	 * PADDR1 and PADDR2 are used by pmap_pte_quick() and pmap_pte(),
	 * respectively.
	 */
	SYSMAP(pt_entry_t *, PMAP1, PADDR1, 1)
	SYSMAP(pt_entry_t *, PMAP2, PADDR2, 1)

	mtx_init(&PMAP2mutex, "PMAP2", NULL, MTX_DEF);

	virtual_avail = va;

	/*
	 * Leave in place an identity mapping (virt == phys) for the low 1 MB
	 * physical memory region that is used by the ACPI wakeup code.  This
	 * mapping must not have PG_G set. 
	 */
#ifndef XEN
	/*
	 * leave here deliberately to show that this is not supported
	 */
#ifdef XBOX
	/* FIXME: This is gross, but needed for the XBOX. Since we are in such
	 * an early stadium, we cannot yet neatly map video memory ... :-(
	 * Better fixes are very welcome! */
	if (!arch_i386_is_xbox)
#endif
	for (i = 1; i < NKPT; i++)
		PTD[i] = 0;

	/* Initialize the PAT MSR if present. */
	pmap_init_pat();

	/* Turn on PG_G on kernel page(s) */
	pmap_set_pg();
#endif

#ifdef HAMFISTED_LOCKING
	mtx_init(&createdelete_lock, "pmap create/delete", NULL, MTX_DEF);
#endif
}

/*
 * Setup the PAT MSR.
 */
void
pmap_init_pat(void)
{
	uint64_t pat_msr;

	/* Bail if this CPU doesn't implement PAT. */
	if (!(cpu_feature & CPUID_PAT))
		return;

	if (cpu_vendor_id != CPU_VENDOR_INTEL ||
	    (CPUID_TO_FAMILY(cpu_id) == 6 && CPUID_TO_MODEL(cpu_id) >= 0xe)) {
		/*
		 * Leave the indices 0-3 at the default of WB, WT, UC, and UC-.
		 * Program 4 and 5 as WP and WC.
		 * Leave 6 and 7 as UC and UC-.
		 */
		pat_msr = rdmsr(MSR_PAT);
		pat_msr &= ~(PAT_MASK(4) | PAT_MASK(5));
		pat_msr |= PAT_VALUE(4, PAT_WRITE_PROTECTED) |
		    PAT_VALUE(5, PAT_WRITE_COMBINING);
		pat_works = 1;
	} else {
		/*
		 * Due to some Intel errata, we can only safely use the lower 4
		 * PAT entries.  Thus, just replace PAT Index 2 with WC instead
		 * of UC-.
		 *
		 *   Intel Pentium III Processor Specification Update
		 * Errata E.27 (Upper Four PAT Entries Not Usable With Mode B
		 * or Mode C Paging)
		 *
		 *   Intel Pentium IV  Processor Specification Update
		 * Errata N46 (PAT Index MSB May Be Calculated Incorrectly)
		 */
		pat_msr = rdmsr(MSR_PAT);
		pat_msr &= ~PAT_MASK(2);
		pat_msr |= PAT_VALUE(2, PAT_WRITE_COMBINING);
		pat_works = 0;
	}
	wrmsr(MSR_PAT, pat_msr);
}

/*
 * Initialize a vm_page's machine-dependent fields.
 */
void
pmap_page_init(vm_page_t m)
{

	TAILQ_INIT(&m->md.pv_list);
	m->md.pat_mode = PAT_WRITE_BACK;
}

/*
 * ABuse the pte nodes for unmapped kva to thread a kva freelist through.
 * Requirements:
 *  - Must deal with pages in order to ensure that none of the PG_* bits
 *    are ever set, PG_V in particular.
 *  - Assumes we can write to ptes without pte_store() atomic ops, even
 *    on PAE systems.  This should be ok.
 *  - Assumes nothing will ever test these addresses for 0 to indicate
 *    no mapping instead of correctly checking PG_V.
 *  - Assumes a vm_offset_t will fit in a pte (true for i386).
 * Because PG_V is never set, there can be no mappings to invalidate.
 */
static int ptelist_count = 0;
static vm_offset_t
pmap_ptelist_alloc(vm_offset_t *head)
{
	vm_offset_t va;
	vm_offset_t *phead = (vm_offset_t *)*head;
	
	if (ptelist_count == 0) {
		printf("out of memory!!!!!!\n");
		return (0);	/* Out of memory */
	}
	ptelist_count--;
	va = phead[ptelist_count];
	return (va);
}

static void
pmap_ptelist_free(vm_offset_t *head, vm_offset_t va)
{
	vm_offset_t *phead = (vm_offset_t *)*head;

	phead[ptelist_count++] = va;
}

static void
pmap_ptelist_init(vm_offset_t *head, void *base, int npages)
{
	int i, nstackpages;
	vm_offset_t va;
	vm_page_t m;
	
	nstackpages = (npages + PAGE_SIZE/sizeof(vm_offset_t) - 1)/ (PAGE_SIZE/sizeof(vm_offset_t));
	for (i = 0; i < nstackpages; i++) {
		va = (vm_offset_t)base + i * PAGE_SIZE;
		m = vm_page_alloc(NULL, i,
		    VM_ALLOC_NORMAL | VM_ALLOC_NOOBJ | VM_ALLOC_WIRED |
		    VM_ALLOC_ZERO);
		pmap_qenter(va, &m, 1);
	}

	*head = (vm_offset_t)base;
	for (i = npages - 1; i >= nstackpages; i--) {
		va = (vm_offset_t)base + i * PAGE_SIZE;
		pmap_ptelist_free(head, va);
	}
}


/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void
pmap_init(void)
{

	/*
	 * Initialize the address space (zone) for the pv entries.  Set a
	 * high water mark so that the system can recover from excessive
	 * numbers of pv entries.
	 */
	TUNABLE_INT_FETCH("vm.pmap.shpgperproc", &shpgperproc);
	pv_entry_max = shpgperproc * maxproc + cnt.v_page_count;
	TUNABLE_INT_FETCH("vm.pmap.pv_entries", &pv_entry_max);
	pv_entry_max = roundup(pv_entry_max, _NPCPV);
	pv_entry_high_water = 9 * (pv_entry_max / 10);

	pv_maxchunks = MAX(pv_entry_max / _NPCPV, maxproc);
	pv_chunkbase = (struct pv_chunk *)kmem_alloc_nofault(kernel_map,
	    PAGE_SIZE * pv_maxchunks);
	if (pv_chunkbase == NULL)
		panic("pmap_init: not enough kvm for pv chunks");
	pmap_ptelist_init(&pv_vafree, pv_chunkbase, pv_maxchunks);
}


SYSCTL_INT(_vm_pmap, OID_AUTO, pv_entry_max, CTLFLAG_RD, &pv_entry_max, 0,
	"Max number of PV entries");
SYSCTL_INT(_vm_pmap, OID_AUTO, shpgperproc, CTLFLAG_RD, &shpgperproc, 0,
	"Page share factor per proc");

static SYSCTL_NODE(_vm_pmap, OID_AUTO, pde, CTLFLAG_RD, 0,
    "2/4MB page mapping counters");

static u_long pmap_pde_mappings;
SYSCTL_ULONG(_vm_pmap_pde, OID_AUTO, mappings, CTLFLAG_RD,
    &pmap_pde_mappings, 0, "2/4MB page mappings");

/***************************************************
 * Low level helper routines.....
 ***************************************************/

/*
 * Determine the appropriate bits to set in a PTE or PDE for a specified
 * caching mode.
 */
int
pmap_cache_bits(int mode, boolean_t is_pde)
{
	int pat_flag, pat_index, cache_bits;

	/* The PAT bit is different for PTE's and PDE's. */
	pat_flag = is_pde ? PG_PDE_PAT : PG_PTE_PAT;

	/* If we don't support PAT, map extended modes to older ones. */
	if (!(cpu_feature & CPUID_PAT)) {
		switch (mode) {
		case PAT_UNCACHEABLE:
		case PAT_WRITE_THROUGH:
		case PAT_WRITE_BACK:
			break;
		case PAT_UNCACHED:
		case PAT_WRITE_COMBINING:
		case PAT_WRITE_PROTECTED:
			mode = PAT_UNCACHEABLE;
			break;
		}
	}
	
	/* Map the caching mode to a PAT index. */
	if (pat_works) {
		switch (mode) {
			case PAT_UNCACHEABLE:
				pat_index = 3;
				break;
			case PAT_WRITE_THROUGH:
				pat_index = 1;
				break;
			case PAT_WRITE_BACK:
				pat_index = 0;
				break;
			case PAT_UNCACHED:
				pat_index = 2;
				break;
			case PAT_WRITE_COMBINING:
				pat_index = 5;
				break;
			case PAT_WRITE_PROTECTED:
				pat_index = 4;
				break;
			default:
				panic("Unknown caching mode %d\n", mode);
		}
	} else {
		switch (mode) {
			case PAT_UNCACHED:
			case PAT_UNCACHEABLE:
			case PAT_WRITE_PROTECTED:
				pat_index = 3;
				break;
			case PAT_WRITE_THROUGH:
				pat_index = 1;
				break;
			case PAT_WRITE_BACK:
				pat_index = 0;
				break;
			case PAT_WRITE_COMBINING:
				pat_index = 2;
				break;
			default:
				panic("Unknown caching mode %d\n", mode);
		}
	}	

	/* Map the 3-bit index value into the PAT, PCD, and PWT bits. */
	cache_bits = 0;
	if (pat_index & 0x4)
		cache_bits |= pat_flag;
	if (pat_index & 0x2)
		cache_bits |= PG_NC_PCD;
	if (pat_index & 0x1)
		cache_bits |= PG_NC_PWT;
	return (cache_bits);
}
#ifdef SMP
/*
 * For SMP, these functions have to use the IPI mechanism for coherence.
 *
 * N.B.: Before calling any of the following TLB invalidation functions,
 * the calling processor must ensure that all stores updating a non-
 * kernel page table are globally performed.  Otherwise, another
 * processor could cache an old, pre-update entry without being
 * invalidated.  This can happen one of two ways: (1) The pmap becomes
 * active on another processor after its pm_active field is checked by
 * one of the following functions but before a store updating the page
 * table is globally performed. (2) The pmap becomes active on another
 * processor before its pm_active field is checked but due to
 * speculative loads one of the following functions stills reads the
 * pmap as inactive on the other processor.
 * 
 * The kernel page table is exempt because its pm_active field is
 * immutable.  The kernel page table is always active on every
 * processor.
 */
void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{
	cpuset_t other_cpus;
	u_int cpuid;

	CTR2(KTR_PMAP, "pmap_invalidate_page: pmap=%p va=0x%x",
	    pmap, va);
	
	sched_pin();
	if (pmap == kernel_pmap || !CPU_CMP(&pmap->pm_active, &all_cpus)) {
		invlpg(va);
		smp_invlpg(va);
	} else {
		cpuid = PCPU_GET(cpuid);
		other_cpus = all_cpus;
		CPU_CLR(cpuid, &other_cpus);
		if (CPU_ISSET(cpuid, &pmap->pm_active))
			invlpg(va);
		CPU_AND(&other_cpus, &pmap->pm_active);
		if (!CPU_EMPTY(&other_cpus))
			smp_masked_invlpg(other_cpus, va);
	}
	sched_unpin();
	PT_UPDATES_FLUSH();
}

void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	cpuset_t other_cpus;
	vm_offset_t addr;
	u_int cpuid;

	CTR3(KTR_PMAP, "pmap_invalidate_page: pmap=%p eva=0x%x sva=0x%x",
	    pmap, sva, eva);

	sched_pin();
	if (pmap == kernel_pmap || !CPU_CMP(&pmap->pm_active, &all_cpus)) {
		for (addr = sva; addr < eva; addr += PAGE_SIZE)
			invlpg(addr);
		smp_invlpg_range(sva, eva);
	} else {
		cpuid = PCPU_GET(cpuid);
		other_cpus = all_cpus;
		CPU_CLR(cpuid, &other_cpus);
		if (CPU_ISSET(cpuid, &pmap->pm_active))
			for (addr = sva; addr < eva; addr += PAGE_SIZE)
				invlpg(addr);
		CPU_AND(&other_cpus, &pmap->pm_active);
		if (!CPU_EMPTY(&other_cpus))
			smp_masked_invlpg_range(other_cpus, sva, eva);
	}
	sched_unpin();
	PT_UPDATES_FLUSH();
}

void
pmap_invalidate_all(pmap_t pmap)
{
	cpuset_t other_cpus;
	u_int cpuid;

	CTR1(KTR_PMAP, "pmap_invalidate_page: pmap=%p", pmap);

	sched_pin();
	if (pmap == kernel_pmap || !CPU_CMP(&pmap->pm_active, &all_cpus)) {
		invltlb();
		smp_invltlb();
	} else {
		cpuid = PCPU_GET(cpuid);
		other_cpus = all_cpus;
		CPU_CLR(cpuid, &other_cpus);
		if (CPU_ISSET(cpuid, &pmap->pm_active))
			invltlb();
		CPU_AND(&other_cpus, &pmap->pm_active);
		if (!CPU_EMPTY(&other_cpus))
			smp_masked_invltlb(other_cpus);
	}
	sched_unpin();
}

void
pmap_invalidate_cache(void)
{

	sched_pin();
	wbinvd();
	smp_cache_flush();
	sched_unpin();
}
#else /* !SMP */
/*
 * Normal, non-SMP, 486+ invalidation functions.
 * We inline these within pmap.c for speed.
 */
PMAP_INLINE void
pmap_invalidate_page(pmap_t pmap, vm_offset_t va)
{
	CTR2(KTR_PMAP, "pmap_invalidate_page: pmap=%p va=0x%x",
	    pmap, va);

	if (pmap == kernel_pmap || !CPU_EMPTY(&pmap->pm_active))
		invlpg(va);
	PT_UPDATES_FLUSH();
}

PMAP_INLINE void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	vm_offset_t addr;

	if (eva - sva > PAGE_SIZE)
		CTR3(KTR_PMAP, "pmap_invalidate_range: pmap=%p sva=0x%x eva=0x%x",
		    pmap, sva, eva);

	if (pmap == kernel_pmap || !CPU_EMPTY(&pmap->pm_active))
		for (addr = sva; addr < eva; addr += PAGE_SIZE)
			invlpg(addr);
	PT_UPDATES_FLUSH();
}

PMAP_INLINE void
pmap_invalidate_all(pmap_t pmap)
{

	CTR1(KTR_PMAP, "pmap_invalidate_all: pmap=%p", pmap);
	
	if (pmap == kernel_pmap || !CPU_EMPTY(&pmap->pm_active))
		invltlb();
}

PMAP_INLINE void
pmap_invalidate_cache(void)
{

	wbinvd();
}
#endif /* !SMP */

#define	PMAP_CLFLUSH_THRESHOLD	(2 * 1024 * 1024)

void
pmap_invalidate_cache_range(vm_offset_t sva, vm_offset_t eva)
{

	KASSERT((sva & PAGE_MASK) == 0,
	    ("pmap_invalidate_cache_range: sva not page-aligned"));
	KASSERT((eva & PAGE_MASK) == 0,
	    ("pmap_invalidate_cache_range: eva not page-aligned"));

	if (cpu_feature & CPUID_SS)
		; /* If "Self Snoop" is supported, do nothing. */
	else if ((cpu_feature & CPUID_CLFSH) != 0 &&
	    eva - sva < PMAP_CLFLUSH_THRESHOLD) {

		/*
		 * Otherwise, do per-cache line flush.  Use the mfence
		 * instruction to insure that previous stores are
		 * included in the write-back.  The processor
		 * propagates flush to other processors in the cache
		 * coherence domain.
		 */
		mfence();
		for (; sva < eva; sva += cpu_clflush_line_size)
			clflush(sva);
		mfence();
	} else {

		/*
		 * No targeted cache flush methods are supported by CPU,
		 * or the supplied range is bigger than 2MB.
		 * Globally invalidate cache.
		 */
		pmap_invalidate_cache();
	}
}

void
pmap_invalidate_cache_pages(vm_page_t *pages, int count)
{
	int i;

	if (count >= PMAP_CLFLUSH_THRESHOLD / PAGE_SIZE ||
	    (cpu_feature & CPUID_CLFSH) == 0) {
		pmap_invalidate_cache();
	} else {
		for (i = 0; i < count; i++)
			pmap_flush_page(pages[i]);
	}
}

/*
 * Are we current address space or kernel?  N.B. We return FALSE when
 * a pmap's page table is in use because a kernel thread is borrowing
 * it.  The borrowed page table can change spontaneously, making any
 * dependence on its continued use subject to a race condition.
 */
static __inline int
pmap_is_current(pmap_t pmap)
{

	return (pmap == kernel_pmap ||
	    (pmap == vmspace_pmap(curthread->td_proc->p_vmspace) &&
	    (pmap->pm_pdir[PTDPTDI] & PG_FRAME) == (PTDpde[0] & PG_FRAME)));
}

/*
 * If the given pmap is not the current or kernel pmap, the returned pte must
 * be released by passing it to pmap_pte_release().
 */
pt_entry_t *
pmap_pte(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t newpf;
	pd_entry_t *pde;

	pde = pmap_pde(pmap, va);
	if (*pde & PG_PS)
		return (pde);
	if (*pde != 0) {
		/* are we current address space or kernel? */
		if (pmap_is_current(pmap))
			return (vtopte(va));
		mtx_lock(&PMAP2mutex);
		newpf = *pde & PG_FRAME;
		if ((*PMAP2 & PG_FRAME) != newpf) {
			PT_SET_MA(PADDR2, newpf | PG_V | PG_A | PG_M);
			CTR3(KTR_PMAP, "pmap_pte: pmap=%p va=0x%x newpte=0x%08x",
			    pmap, va, (*PMAP2 & 0xffffffff));
		}
		return (PADDR2 + (i386_btop(va) & (NPTEPG - 1)));
	}
	return (NULL);
}

/*
 * Releases a pte that was obtained from pmap_pte().  Be prepared for the pte
 * being NULL.
 */
static __inline void
pmap_pte_release(pt_entry_t *pte)
{

	if ((pt_entry_t *)((vm_offset_t)pte & ~PAGE_MASK) == PADDR2) {
		CTR1(KTR_PMAP, "pmap_pte_release: pte=0x%jx",
		    *PMAP2);
		rw_wlock(&pvh_global_lock);
		PT_SET_VA(PMAP2, 0, TRUE);
		rw_wunlock(&pvh_global_lock);
		mtx_unlock(&PMAP2mutex);
	}
}

static __inline void
invlcaddr(void *caddr)
{

	invlpg((u_int)caddr);
	PT_UPDATES_FLUSH();
}

/*
 * Super fast pmap_pte routine best used when scanning
 * the pv lists.  This eliminates many coarse-grained
 * invltlb calls.  Note that many of the pv list
 * scans are across different pmaps.  It is very wasteful
 * to do an entire invltlb for checking a single mapping.
 *
 * If the given pmap is not the current pmap, pvh_global_lock
 * must be held and curthread pinned to a CPU.
 */
static pt_entry_t *
pmap_pte_quick(pmap_t pmap, vm_offset_t va)
{
	pd_entry_t newpf;
	pd_entry_t *pde;

	pde = pmap_pde(pmap, va);
	if (*pde & PG_PS)
		return (pde);
	if (*pde != 0) {
		/* are we current address space or kernel? */
		if (pmap_is_current(pmap))
			return (vtopte(va));
		rw_assert(&pvh_global_lock, RA_WLOCKED);
		KASSERT(curthread->td_pinned > 0, ("curthread not pinned"));
		newpf = *pde & PG_FRAME;
		if ((*PMAP1 & PG_FRAME) != newpf) {
			PT_SET_MA(PADDR1, newpf | PG_V | PG_A | PG_M);
			CTR3(KTR_PMAP, "pmap_pte_quick: pmap=%p va=0x%x newpte=0x%08x",
			    pmap, va, (u_long)*PMAP1);
			
#ifdef SMP
			PMAP1cpu = PCPU_GET(cpuid);
#endif
			PMAP1changed++;
		} else
#ifdef SMP
		if (PMAP1cpu != PCPU_GET(cpuid)) {
			PMAP1cpu = PCPU_GET(cpuid);
			invlcaddr(PADDR1);
			PMAP1changedcpu++;
		} else
#endif
			PMAP1unchanged++;
		return (PADDR1 + (i386_btop(va) & (NPTEPG - 1)));
	}
	return (0);
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */
vm_paddr_t 
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	vm_paddr_t rtval;
	pt_entry_t *pte;
	pd_entry_t pde;
	pt_entry_t pteval;

	rtval = 0;
	PMAP_LOCK(pmap);
	pde = pmap->pm_pdir[va >> PDRSHIFT];
	if (pde != 0) {
		if ((pde & PG_PS) != 0) {
			rtval = xpmap_mtop(pde & PG_PS_FRAME) | (va & PDRMASK);
			PMAP_UNLOCK(pmap);
			return rtval;
		}
		pte = pmap_pte(pmap, va);
		pteval = *pte ? xpmap_mtop(*pte) : 0;
		rtval = (pteval & PG_FRAME) | (va & PAGE_MASK);
		pmap_pte_release(pte);
	}
	PMAP_UNLOCK(pmap);
	return (rtval);
}

/*
 *	Routine:	pmap_extract_ma
 *	Function:
 *		Like pmap_extract, but returns machine address
 */
vm_paddr_t 
pmap_extract_ma(pmap_t pmap, vm_offset_t va)
{
	vm_paddr_t rtval;
	pt_entry_t *pte;
	pd_entry_t pde;

	rtval = 0;
	PMAP_LOCK(pmap);
	pde = pmap->pm_pdir[va >> PDRSHIFT];
	if (pde != 0) {
		if ((pde & PG_PS) != 0) {
			rtval = (pde & ~PDRMASK) | (va & PDRMASK);
			PMAP_UNLOCK(pmap);
			return rtval;
		}
		pte = pmap_pte(pmap, va);
		rtval = (*pte & PG_FRAME) | (va & PAGE_MASK);
		pmap_pte_release(pte);
	}
	PMAP_UNLOCK(pmap);
	return (rtval);
}

/*
 *	Routine:	pmap_extract_and_hold
 *	Function:
 *		Atomically extract and hold the physical page
 *		with the given pmap and virtual address pair
 *		if that mapping permits the given protection.
 */
vm_page_t
pmap_extract_and_hold(pmap_t pmap, vm_offset_t va, vm_prot_t prot)
{
	pd_entry_t pde;
	pt_entry_t pte, *ptep;
	vm_page_t m;
	vm_paddr_t pa;

	pa = 0;
	m = NULL;
	PMAP_LOCK(pmap);
retry:
	pde = PT_GET(pmap_pde(pmap, va));
	if (pde != 0) {
		if (pde & PG_PS) {
			if ((pde & PG_RW) || (prot & VM_PROT_WRITE) == 0) {
				if (vm_page_pa_tryrelock(pmap, (pde &
				    PG_PS_FRAME) | (va & PDRMASK), &pa))
					goto retry;
				m = PHYS_TO_VM_PAGE((pde & PG_PS_FRAME) |
				    (va & PDRMASK));
				vm_page_hold(m);
			}
		} else {
			ptep = pmap_pte(pmap, va);
			pte = PT_GET(ptep);
			pmap_pte_release(ptep);
			if (pte != 0 &&
			    ((pte & PG_RW) || (prot & VM_PROT_WRITE) == 0)) {
				if (vm_page_pa_tryrelock(pmap, pte & PG_FRAME,
				    &pa))
					goto retry;
				m = PHYS_TO_VM_PAGE(pte & PG_FRAME);
				vm_page_hold(m);
			}
		}
	}
	PA_UNLOCK_COND(pa);
	PMAP_UNLOCK(pmap);
	return (m);
}

/***************************************************
 * Low level mapping routines.....
 ***************************************************/

/*
 * Add a wired page to the kva.
 * Note: not SMP coherent.
 *
 * This function may be used before pmap_bootstrap() is called.
 */
void 
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{

	PT_SET_MA(va, xpmap_ptom(pa)| PG_RW | PG_V | pgeflag);
}

void 
pmap_kenter_ma(vm_offset_t va, vm_paddr_t ma)
{
	pt_entry_t *pte;

	pte = vtopte(va);
	pte_store_ma(pte, ma | PG_RW | PG_V | pgeflag);
}

static __inline void
pmap_kenter_attr(vm_offset_t va, vm_paddr_t pa, int mode)
{

	PT_SET_MA(va, pa | PG_RW | PG_V | pgeflag | pmap_cache_bits(mode, 0));
}

/*
 * Remove a page from the kernel pagetables.
 * Note: not SMP coherent.
 *
 * This function may be used before pmap_bootstrap() is called.
 */
PMAP_INLINE void
pmap_kremove(vm_offset_t va)
{
	pt_entry_t *pte;

	pte = vtopte(va);
	PT_CLEAR_VA(pte, FALSE);
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
pmap_map(vm_offset_t *virt, vm_paddr_t start, vm_paddr_t end, int prot)
{
	vm_offset_t va, sva;

	va = sva = *virt;
	CTR4(KTR_PMAP, "pmap_map: va=0x%x start=0x%jx end=0x%jx prot=0x%x",
	    va, start, end, prot);
	while (start < end) {
		pmap_kenter(va, start);
		va += PAGE_SIZE;
		start += PAGE_SIZE;
	}
	pmap_invalidate_range(kernel_pmap, sva, va);
	*virt = va;
	return (sva);
}


/*
 * Add a list of wired pages to the kva
 * this routine is only used for temporary
 * kernel mappings that do not need to have
 * page modification or references recorded.
 * Note that old mappings are simply written
 * over.  The page *must* be wired.
 * Note: SMP coherent.  Uses a ranged shootdown IPI.
 */
void
pmap_qenter(vm_offset_t sva, vm_page_t *ma, int count)
{
	pt_entry_t *endpte, *pte;
	vm_paddr_t pa;
	vm_offset_t va = sva;
	int mclcount = 0;
	multicall_entry_t mcl[16];
	multicall_entry_t *mclp = mcl;
	int error;

	CTR2(KTR_PMAP, "pmap_qenter:sva=0x%x count=%d", va, count);
	pte = vtopte(sva);
	endpte = pte + count;
	while (pte < endpte) {
		pa = VM_PAGE_TO_MACH(*ma) | pgeflag | PG_RW | PG_V | PG_M | PG_A;

		mclp->op = __HYPERVISOR_update_va_mapping;
		mclp->args[0] = va;
		mclp->args[1] = (uint32_t)(pa & 0xffffffff);
		mclp->args[2] = (uint32_t)(pa >> 32);
		mclp->args[3] = (*pte & PG_V) ? UVMF_INVLPG|UVMF_ALL : 0;
	
		va += PAGE_SIZE;
		pte++;
		ma++;
		mclp++;
		mclcount++;
		if (mclcount == 16) {
			error = HYPERVISOR_multicall(mcl, mclcount);
			mclp = mcl;
			mclcount = 0;
			KASSERT(error == 0, ("bad multicall %d", error));
		}		
	}
	if (mclcount) {
		error = HYPERVISOR_multicall(mcl, mclcount);
		KASSERT(error == 0, ("bad multicall %d", error));
	}
	
#ifdef INVARIANTS
	for (pte = vtopte(sva), mclcount = 0; mclcount < count; mclcount++, pte++)
		KASSERT(*pte, ("pte not set for va=0x%x", sva + mclcount*PAGE_SIZE));
#endif	
}

/*
 * This routine tears out page mappings from the
 * kernel -- it is meant only for temporary mappings.
 * Note: SMP coherent.  Uses a ranged shootdown IPI.
 */
void
pmap_qremove(vm_offset_t sva, int count)
{
	vm_offset_t va;

	CTR2(KTR_PMAP, "pmap_qremove: sva=0x%x count=%d", sva, count);
	va = sva;
	rw_wlock(&pvh_global_lock);
	critical_enter();
	while (count-- > 0) {
		pmap_kremove(va);
		va += PAGE_SIZE;
	}
	PT_UPDATES_FLUSH();
	pmap_invalidate_range(kernel_pmap, sva, va);
	critical_exit();
	rw_wunlock(&pvh_global_lock);
}

/***************************************************
 * Page table page management routines.....
 ***************************************************/
static __inline void
pmap_free_zero_pages(vm_page_t free)
{
	vm_page_t m;

	while (free != NULL) {
		m = free;
		free = (void *)m->object;
		m->object = NULL;
		vm_page_free_zero(m);
	}
}

/*
 * Decrements a page table page's wire count, which is used to record the
 * number of valid page table entries within the page.  If the wire count
 * drops to zero, then the page table page is unmapped.  Returns TRUE if the
 * page table page was unmapped and FALSE otherwise.
 */
static inline boolean_t
pmap_unwire_ptp(pmap_t pmap, vm_page_t m, vm_page_t *free)
{

	--m->wire_count;
	if (m->wire_count == 0) {
		_pmap_unwire_ptp(pmap, m, free);
		return (TRUE);
	} else
		return (FALSE);
}

static void
_pmap_unwire_ptp(pmap_t pmap, vm_page_t m, vm_page_t *free)
{
	vm_offset_t pteva;

	PT_UPDATES_FLUSH();
	/*
	 * unmap the page table page
	 */
	xen_pt_unpin(pmap->pm_pdir[m->pindex]);
	/*
	 * page *might* contain residual mapping :-/  
	 */
	PD_CLEAR_VA(pmap, m->pindex, TRUE);
	pmap_zero_page(m);
	--pmap->pm_stats.resident_count;

	/*
	 * This is a release store so that the ordinary store unmapping
	 * the page table page is globally performed before TLB shoot-
	 * down is begun.
	 */
	atomic_subtract_rel_int(&cnt.v_wire_count, 1);

	/*
	 * Do an invltlb to make the invalidated mapping
	 * take effect immediately.
	 */
	pteva = VM_MAXUSER_ADDRESS + i386_ptob(m->pindex);
	pmap_invalidate_page(pmap, pteva);

	/* 
	 * Put page on a list so that it is released after
	 * *ALL* TLB shootdown is done
	 */
	m->object = (void *)*free;
	*free = m;
}

/*
 * After removing a page table entry, this routine is used to
 * conditionally free the page, and manage the hold/wire counts.
 */
static int
pmap_unuse_pt(pmap_t pmap, vm_offset_t va, vm_page_t *free)
{
	pd_entry_t ptepde;
	vm_page_t mpte;

	if (va >= VM_MAXUSER_ADDRESS)
		return (0);
	ptepde = PT_GET(pmap_pde(pmap, va));
	mpte = PHYS_TO_VM_PAGE(ptepde & PG_FRAME);
	return (pmap_unwire_ptp(pmap, mpte, free));
}

/*
 * Initialize the pmap for the swapper process.
 */
void
pmap_pinit0(pmap_t pmap)
{

	PMAP_LOCK_INIT(pmap);
	/*
	 * Since the page table directory is shared with the kernel pmap,
	 * which is already included in the list "allpmaps", this pmap does
	 * not need to be inserted into that list.
	 */
	pmap->pm_pdir = (pd_entry_t *)(KERNBASE + (vm_offset_t)IdlePTD);
#ifdef PAE
	pmap->pm_pdpt = (pdpt_entry_t *)(KERNBASE + (vm_offset_t)IdlePDPT);
#endif
	CPU_ZERO(&pmap->pm_active);
	PCPU_SET(curpmap, pmap);
	TAILQ_INIT(&pmap->pm_pvchunk);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
int
pmap_pinit(pmap_t pmap)
{
	vm_page_t m, ptdpg[NPGPTD + 1];
	int npgptd = NPGPTD + 1;
	int i;

#ifdef HAMFISTED_LOCKING
	mtx_lock(&createdelete_lock);
#endif

	PMAP_LOCK_INIT(pmap);

	/*
	 * No need to allocate page table space yet but we do need a valid
	 * page directory table.
	 */
	if (pmap->pm_pdir == NULL) {
		pmap->pm_pdir = (pd_entry_t *)kmem_alloc_nofault(kernel_map,
		    NBPTD);
		if (pmap->pm_pdir == NULL) {
			PMAP_LOCK_DESTROY(pmap);
#ifdef HAMFISTED_LOCKING
			mtx_unlock(&createdelete_lock);
#endif
			return (0);
		}
#ifdef PAE
		pmap->pm_pdpt = (pd_entry_t *)kmem_alloc_nofault(kernel_map, 1);
#endif
	}

	/*
	 * allocate the page directory page(s)
	 */
	for (i = 0; i < npgptd;) {
		m = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL | VM_ALLOC_NOOBJ |
		    VM_ALLOC_WIRED | VM_ALLOC_ZERO);
		if (m == NULL)
			VM_WAIT;
		else {
			ptdpg[i++] = m;
		}
	}

	pmap_qenter((vm_offset_t)pmap->pm_pdir, ptdpg, NPGPTD);

	for (i = 0; i < NPGPTD; i++)
		if ((ptdpg[i]->flags & PG_ZERO) == 0)
			pagezero(pmap->pm_pdir + (i * NPDEPG));

	mtx_lock_spin(&allpmaps_lock);
	LIST_INSERT_HEAD(&allpmaps, pmap, pm_list);
	/* Copy the kernel page table directory entries. */
	bcopy(PTD + KPTDI, pmap->pm_pdir + KPTDI, nkpt * sizeof(pd_entry_t));
	mtx_unlock_spin(&allpmaps_lock);

#ifdef PAE
	pmap_qenter((vm_offset_t)pmap->pm_pdpt, &ptdpg[NPGPTD], 1);
	if ((ptdpg[NPGPTD]->flags & PG_ZERO) == 0)
		bzero(pmap->pm_pdpt, PAGE_SIZE);
	for (i = 0; i < NPGPTD; i++) {
		vm_paddr_t ma;
		
		ma = VM_PAGE_TO_MACH(ptdpg[i]);
		pmap->pm_pdpt[i] = ma | PG_V;

	}
#endif	
	for (i = 0; i < NPGPTD; i++) {
		pt_entry_t *pd;
		vm_paddr_t ma;
		
		ma = VM_PAGE_TO_MACH(ptdpg[i]);
		pd = pmap->pm_pdir + (i * NPDEPG);
		PT_SET_MA(pd, *vtopte((vm_offset_t)pd) & ~(PG_M|PG_A|PG_U|PG_RW));
#if 0		
		xen_pgd_pin(ma);
#endif		
	}
	
#ifdef PAE	
	PT_SET_MA(pmap->pm_pdpt, *vtopte((vm_offset_t)pmap->pm_pdpt) & ~PG_RW);
#endif
	rw_wlock(&pvh_global_lock);
	xen_flush_queue();
	xen_pgdpt_pin(VM_PAGE_TO_MACH(ptdpg[NPGPTD]));
	for (i = 0; i < NPGPTD; i++) {
		vm_paddr_t ma = VM_PAGE_TO_MACH(ptdpg[i]);
		PT_SET_VA_MA(&pmap->pm_pdir[PTDPTDI + i], ma | PG_V | PG_A, FALSE);
	}
	xen_flush_queue();
	rw_wunlock(&pvh_global_lock);
	CPU_ZERO(&pmap->pm_active);
	TAILQ_INIT(&pmap->pm_pvchunk);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);

#ifdef HAMFISTED_LOCKING
	mtx_unlock(&createdelete_lock);
#endif
	return (1);
}

/*
 * this routine is called if the page table page is not
 * mapped correctly.
 */
static vm_page_t
_pmap_allocpte(pmap_t pmap, u_int ptepindex, int flags)
{
	vm_paddr_t ptema;
	vm_page_t m;

	KASSERT((flags & (M_NOWAIT | M_WAITOK)) == M_NOWAIT ||
	    (flags & (M_NOWAIT | M_WAITOK)) == M_WAITOK,
	    ("_pmap_allocpte: flags is neither M_NOWAIT nor M_WAITOK"));

	/*
	 * Allocate a page table page.
	 */
	if ((m = vm_page_alloc(NULL, ptepindex, VM_ALLOC_NOOBJ |
	    VM_ALLOC_WIRED | VM_ALLOC_ZERO)) == NULL) {
		if (flags & M_WAITOK) {
			PMAP_UNLOCK(pmap);
			rw_wunlock(&pvh_global_lock);
			VM_WAIT;
			rw_wlock(&pvh_global_lock);
			PMAP_LOCK(pmap);
		}

		/*
		 * Indicate the need to retry.  While waiting, the page table
		 * page may have been allocated.
		 */
		return (NULL);
	}
	if ((m->flags & PG_ZERO) == 0)
		pmap_zero_page(m);

	/*
	 * Map the pagetable page into the process address space, if
	 * it isn't already there.
	 */

	pmap->pm_stats.resident_count++;

	ptema = VM_PAGE_TO_MACH(m);
	xen_pt_pin(ptema);
	PT_SET_VA_MA(&pmap->pm_pdir[ptepindex],
		(ptema | PG_U | PG_RW | PG_V | PG_A | PG_M), TRUE);
	
	KASSERT(pmap->pm_pdir[ptepindex],
	    ("_pmap_allocpte: ptepindex=%d did not get mapped", ptepindex));
	return (m);
}

static vm_page_t
pmap_allocpte(pmap_t pmap, vm_offset_t va, int flags)
{
	u_int ptepindex;
	pd_entry_t ptema;
	vm_page_t m;

	KASSERT((flags & (M_NOWAIT | M_WAITOK)) == M_NOWAIT ||
	    (flags & (M_NOWAIT | M_WAITOK)) == M_WAITOK,
	    ("pmap_allocpte: flags is neither M_NOWAIT nor M_WAITOK"));

	/*
	 * Calculate pagetable page index
	 */
	ptepindex = va >> PDRSHIFT;
retry:
	/*
	 * Get the page directory entry
	 */
	ptema = pmap->pm_pdir[ptepindex];

	/*
	 * This supports switching from a 4MB page to a
	 * normal 4K page.
	 */
	if (ptema & PG_PS) {
		/*
		 * XXX 
		 */
		pmap->pm_pdir[ptepindex] = 0;
		ptema = 0;
		pmap->pm_stats.resident_count -= NBPDR / PAGE_SIZE;
		pmap_invalidate_all(kernel_pmap);
	}

	/*
	 * If the page table page is mapped, we just increment the
	 * hold count, and activate it.
	 */
	if (ptema & PG_V) {
		m = PHYS_TO_VM_PAGE(xpmap_mtop(ptema) & PG_FRAME);
		m->wire_count++;
	} else {
		/*
		 * Here if the pte page isn't mapped, or if it has
		 * been deallocated. 
		 */
		CTR3(KTR_PMAP, "pmap_allocpte: pmap=%p va=0x%08x flags=0x%x",
		    pmap, va, flags);
		m = _pmap_allocpte(pmap, ptepindex, flags);
		if (m == NULL && (flags & M_WAITOK))
			goto retry;

		KASSERT(pmap->pm_pdir[ptepindex], ("ptepindex=%d did not get mapped", ptepindex));
	}
	return (m);
}


/***************************************************
* Pmap allocation/deallocation routines.
 ***************************************************/

#ifdef SMP
/*
 * Deal with a SMP shootdown of other users of the pmap that we are
 * trying to dispose of.  This can be a bit hairy.
 */
static cpuset_t *lazymask;
static u_int lazyptd;
static volatile u_int lazywait;

void pmap_lazyfix_action(void);

void
pmap_lazyfix_action(void)
{

#ifdef COUNT_IPIS
	(*ipi_lazypmap_counts[PCPU_GET(cpuid)])++;
#endif
	if (rcr3() == lazyptd)
		load_cr3(PCPU_GET(curpcb)->pcb_cr3);
	CPU_CLR_ATOMIC(PCPU_GET(cpuid), lazymask);
	atomic_store_rel_int(&lazywait, 1);
}

static void
pmap_lazyfix_self(u_int cpuid)
{

	if (rcr3() == lazyptd)
		load_cr3(PCPU_GET(curpcb)->pcb_cr3);
	CPU_CLR_ATOMIC(cpuid, lazymask);
}


static void
pmap_lazyfix(pmap_t pmap)
{
	cpuset_t mymask, mask;
	u_int cpuid, spins;
	int lsb;

	mask = pmap->pm_active;
	while (!CPU_EMPTY(&mask)) {
		spins = 50000000;

		/* Find least significant set bit. */
		lsb = CPU_FFS(&mask);
		MPASS(lsb != 0);
		lsb--;
		CPU_SETOF(lsb, &mask);
		mtx_lock_spin(&smp_ipi_mtx);
#ifdef PAE
		lazyptd = vtophys(pmap->pm_pdpt);
#else
		lazyptd = vtophys(pmap->pm_pdir);
#endif
		cpuid = PCPU_GET(cpuid);

		/* Use a cpuset just for having an easy check. */
		CPU_SETOF(cpuid, &mymask);
		if (!CPU_CMP(&mask, &mymask)) {
			lazymask = &pmap->pm_active;
			pmap_lazyfix_self(cpuid);
		} else {
			atomic_store_rel_int((u_int *)&lazymask,
			    (u_int)&pmap->pm_active);
			atomic_store_rel_int(&lazywait, 0);
			ipi_selected(mask, IPI_LAZYPMAP);
			while (lazywait == 0) {
				ia32_pause();
				if (--spins == 0)
					break;
			}
		}
		mtx_unlock_spin(&smp_ipi_mtx);
		if (spins == 0)
			printf("pmap_lazyfix: spun for 50000000\n");
		mask = pmap->pm_active;
	}
}

#else	/* SMP */

/*
 * Cleaning up on uniprocessor is easy.  For various reasons, we're
 * unlikely to have to even execute this code, including the fact
 * that the cleanup is deferred until the parent does a wait(2), which
 * means that another userland process has run.
 */
static void
pmap_lazyfix(pmap_t pmap)
{
	u_int cr3;

	cr3 = vtophys(pmap->pm_pdir);
	if (cr3 == rcr3()) {
		load_cr3(PCPU_GET(curpcb)->pcb_cr3);
		CPU_CLR(PCPU_GET(cpuid), &pmap->pm_active);
	}
}
#endif	/* SMP */

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
pmap_release(pmap_t pmap)
{
	vm_page_t m, ptdpg[2*NPGPTD+1];
	vm_paddr_t ma;
	int i;
#ifdef PAE	
	int npgptd = NPGPTD + 1;
#else
	int npgptd = NPGPTD;
#endif

	KASSERT(pmap->pm_stats.resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->pm_stats.resident_count));
	PT_UPDATES_FLUSH();

#ifdef HAMFISTED_LOCKING
	mtx_lock(&createdelete_lock);
#endif

	pmap_lazyfix(pmap);
	mtx_lock_spin(&allpmaps_lock);
	LIST_REMOVE(pmap, pm_list);
	mtx_unlock_spin(&allpmaps_lock);

	for (i = 0; i < NPGPTD; i++)
		ptdpg[i] = PHYS_TO_VM_PAGE(vtophys(pmap->pm_pdir + (i*NPDEPG)) & PG_FRAME);
	pmap_qremove((vm_offset_t)pmap->pm_pdir, NPGPTD);
#ifdef PAE
	ptdpg[NPGPTD] = PHYS_TO_VM_PAGE(vtophys(pmap->pm_pdpt));
#endif	

	for (i = 0; i < npgptd; i++) {
		m = ptdpg[i];
		ma = VM_PAGE_TO_MACH(m);
		/* unpinning L1 and L2 treated the same */
#if 0
                xen_pgd_unpin(ma);
#else
		if (i == NPGPTD)
	                xen_pgd_unpin(ma);
#endif
#ifdef PAE
		if (i < NPGPTD)
			KASSERT(VM_PAGE_TO_MACH(m) == (pmap->pm_pdpt[i] & PG_FRAME),
			    ("pmap_release: got wrong ptd page"));
#endif
		m->wire_count--;
		atomic_subtract_int(&cnt.v_wire_count, 1);
		vm_page_free(m);
	}
#ifdef PAE
	pmap_qremove((vm_offset_t)pmap->pm_pdpt, 1);
#endif
	PMAP_LOCK_DESTROY(pmap);

#ifdef HAMFISTED_LOCKING
	mtx_unlock(&createdelete_lock);
#endif
}

static int
kvm_size(SYSCTL_HANDLER_ARGS)
{
	unsigned long ksize = VM_MAX_KERNEL_ADDRESS - KERNBASE;

	return (sysctl_handle_long(oidp, &ksize, 0, req));
}
SYSCTL_PROC(_vm, OID_AUTO, kvm_size, CTLTYPE_LONG|CTLFLAG_RD, 
    0, 0, kvm_size, "IU", "Size of KVM");

static int
kvm_free(SYSCTL_HANDLER_ARGS)
{
	unsigned long kfree = VM_MAX_KERNEL_ADDRESS - kernel_vm_end;

	return (sysctl_handle_long(oidp, &kfree, 0, req));
}
SYSCTL_PROC(_vm, OID_AUTO, kvm_free, CTLTYPE_LONG|CTLFLAG_RD, 
    0, 0, kvm_free, "IU", "Amount of KVM free");

/*
 * grow the number of kernel page table entries, if needed
 */
void
pmap_growkernel(vm_offset_t addr)
{
	struct pmap *pmap;
	vm_paddr_t ptppaddr;
	vm_page_t nkpg;
	pd_entry_t newpdir;

	mtx_assert(&kernel_map->system_mtx, MA_OWNED);
	if (kernel_vm_end == 0) {
		kernel_vm_end = KERNBASE;
		nkpt = 0;
		while (pdir_pde(PTD, kernel_vm_end)) {
			kernel_vm_end = (kernel_vm_end + PAGE_SIZE * NPTEPG) & ~(PAGE_SIZE * NPTEPG - 1);
			nkpt++;
			if (kernel_vm_end - 1 >= kernel_map->max_offset) {
				kernel_vm_end = kernel_map->max_offset;
				break;
			}
		}
	}
	addr = roundup2(addr, NBPDR);
	if (addr - 1 >= kernel_map->max_offset)
		addr = kernel_map->max_offset;
	while (kernel_vm_end < addr) {
		if (pdir_pde(PTD, kernel_vm_end)) {
			kernel_vm_end = (kernel_vm_end + NBPDR) & ~PDRMASK;
			if (kernel_vm_end - 1 >= kernel_map->max_offset) {
				kernel_vm_end = kernel_map->max_offset;
				break;
			}
			continue;
		}

		nkpg = vm_page_alloc(NULL, kernel_vm_end >> PDRSHIFT,
		    VM_ALLOC_INTERRUPT | VM_ALLOC_NOOBJ | VM_ALLOC_WIRED |
		    VM_ALLOC_ZERO);
		if (nkpg == NULL)
			panic("pmap_growkernel: no memory to grow kernel");

		nkpt++;

		if ((nkpg->flags & PG_ZERO) == 0)
			pmap_zero_page(nkpg);
		ptppaddr = VM_PAGE_TO_PHYS(nkpg);
		newpdir = (pd_entry_t) (ptppaddr | PG_V | PG_RW | PG_A | PG_M);
		rw_wlock(&pvh_global_lock);
		PD_SET_VA(kernel_pmap, (kernel_vm_end >> PDRSHIFT), newpdir, TRUE);
		mtx_lock_spin(&allpmaps_lock);
		LIST_FOREACH(pmap, &allpmaps, pm_list)
			PD_SET_VA(pmap, (kernel_vm_end >> PDRSHIFT), newpdir, TRUE);

		mtx_unlock_spin(&allpmaps_lock);
		rw_wunlock(&pvh_global_lock);

		kernel_vm_end = (kernel_vm_end + NBPDR) & ~PDRMASK;
		if (kernel_vm_end - 1 >= kernel_map->max_offset) {
			kernel_vm_end = kernel_map->max_offset;
			break;
		}
	}
}


/***************************************************
 * page management routines.
 ***************************************************/

CTASSERT(sizeof(struct pv_chunk) == PAGE_SIZE);
CTASSERT(_NPCM == 11);
CTASSERT(_NPCPV == 336);

static __inline struct pv_chunk *
pv_to_chunk(pv_entry_t pv)
{

	return ((struct pv_chunk *)((uintptr_t)pv & ~(uintptr_t)PAGE_MASK));
}

#define PV_PMAP(pv) (pv_to_chunk(pv)->pc_pmap)

#define	PC_FREE0_9	0xfffffffful	/* Free values for index 0 through 9 */
#define	PC_FREE10	0x0000fffful	/* Free values for index 10 */

static const uint32_t pc_freemask[_NPCM] = {
	PC_FREE0_9, PC_FREE0_9, PC_FREE0_9,
	PC_FREE0_9, PC_FREE0_9, PC_FREE0_9,
	PC_FREE0_9, PC_FREE0_9, PC_FREE0_9,
	PC_FREE0_9, PC_FREE10
};

SYSCTL_INT(_vm_pmap, OID_AUTO, pv_entry_count, CTLFLAG_RD, &pv_entry_count, 0,
	"Current number of pv entries");

#ifdef PV_STATS
static int pc_chunk_count, pc_chunk_allocs, pc_chunk_frees, pc_chunk_tryfail;

SYSCTL_INT(_vm_pmap, OID_AUTO, pc_chunk_count, CTLFLAG_RD, &pc_chunk_count, 0,
	"Current number of pv entry chunks");
SYSCTL_INT(_vm_pmap, OID_AUTO, pc_chunk_allocs, CTLFLAG_RD, &pc_chunk_allocs, 0,
	"Current number of pv entry chunks allocated");
SYSCTL_INT(_vm_pmap, OID_AUTO, pc_chunk_frees, CTLFLAG_RD, &pc_chunk_frees, 0,
	"Current number of pv entry chunks frees");
SYSCTL_INT(_vm_pmap, OID_AUTO, pc_chunk_tryfail, CTLFLAG_RD, &pc_chunk_tryfail, 0,
	"Number of times tried to get a chunk page but failed.");

static long pv_entry_frees, pv_entry_allocs;
static int pv_entry_spare;

SYSCTL_LONG(_vm_pmap, OID_AUTO, pv_entry_frees, CTLFLAG_RD, &pv_entry_frees, 0,
	"Current number of pv entry frees");
SYSCTL_LONG(_vm_pmap, OID_AUTO, pv_entry_allocs, CTLFLAG_RD, &pv_entry_allocs, 0,
	"Current number of pv entry allocs");
SYSCTL_INT(_vm_pmap, OID_AUTO, pv_entry_spare, CTLFLAG_RD, &pv_entry_spare, 0,
	"Current number of spare pv entries");
#endif

/*
 * We are in a serious low memory condition.  Resort to
 * drastic measures to free some pages so we can allocate
 * another pv entry chunk.
 */
static vm_page_t
pmap_pv_reclaim(pmap_t locked_pmap)
{
	struct pch newtail;
	struct pv_chunk *pc;
	pmap_t pmap;
	pt_entry_t *pte, tpte;
	pv_entry_t pv;
	vm_offset_t va;
	vm_page_t free, m, m_pc;
	uint32_t inuse;
	int bit, field, freed;

	PMAP_LOCK_ASSERT(locked_pmap, MA_OWNED);
	pmap = NULL;
	free = m_pc = NULL;
	TAILQ_INIT(&newtail);
	while ((pc = TAILQ_FIRST(&pv_chunks)) != NULL && (pv_vafree == 0 ||
	    free == NULL)) {
		TAILQ_REMOVE(&pv_chunks, pc, pc_lru);
		if (pmap != pc->pc_pmap) {
			if (pmap != NULL) {
				pmap_invalidate_all(pmap);
				if (pmap != locked_pmap)
					PMAP_UNLOCK(pmap);
			}
			pmap = pc->pc_pmap;
			/* Avoid deadlock and lock recursion. */
			if (pmap > locked_pmap)
				PMAP_LOCK(pmap);
			else if (pmap != locked_pmap && !PMAP_TRYLOCK(pmap)) {
				pmap = NULL;
				TAILQ_INSERT_TAIL(&newtail, pc, pc_lru);
				continue;
			}
		}

		/*
		 * Destroy every non-wired, 4 KB page mapping in the chunk.
		 */
		freed = 0;
		for (field = 0; field < _NPCM; field++) {
			for (inuse = ~pc->pc_map[field] & pc_freemask[field];
			    inuse != 0; inuse &= ~(1UL << bit)) {
				bit = bsfl(inuse);
				pv = &pc->pc_pventry[field * 32 + bit];
				va = pv->pv_va;
				pte = pmap_pte(pmap, va);
				tpte = *pte;
				if ((tpte & PG_W) == 0)
					tpte = pte_load_clear(pte);
				pmap_pte_release(pte);
				if ((tpte & PG_W) != 0)
					continue;
				KASSERT(tpte != 0,
				    ("pmap_pv_reclaim: pmap %p va %x zero pte",
				    pmap, va));
				if ((tpte & PG_G) != 0)
					pmap_invalidate_page(pmap, va);
				m = PHYS_TO_VM_PAGE(tpte & PG_FRAME);
				if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
					vm_page_dirty(m);
				if ((tpte & PG_A) != 0)
					vm_page_aflag_set(m, PGA_REFERENCED);
				TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
				if (TAILQ_EMPTY(&m->md.pv_list))
					vm_page_aflag_clear(m, PGA_WRITEABLE);
				pc->pc_map[field] |= 1UL << bit;
				pmap_unuse_pt(pmap, va, &free);
				freed++;
			}
		}
		if (freed == 0) {
			TAILQ_INSERT_TAIL(&newtail, pc, pc_lru);
			continue;
		}
		/* Every freed mapping is for a 4 KB page. */
		pmap->pm_stats.resident_count -= freed;
		PV_STAT(pv_entry_frees += freed);
		PV_STAT(pv_entry_spare += freed);
		pv_entry_count -= freed;
		TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
		for (field = 0; field < _NPCM; field++)
			if (pc->pc_map[field] != pc_freemask[field]) {
				TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc,
				    pc_list);
				TAILQ_INSERT_TAIL(&newtail, pc, pc_lru);

				/*
				 * One freed pv entry in locked_pmap is
				 * sufficient.
				 */
				if (pmap == locked_pmap)
					goto out;
				break;
			}
		if (field == _NPCM) {
			PV_STAT(pv_entry_spare -= _NPCPV);
			PV_STAT(pc_chunk_count--);
			PV_STAT(pc_chunk_frees++);
			/* Entire chunk is free; return it. */
			m_pc = PHYS_TO_VM_PAGE(pmap_kextract((vm_offset_t)pc));
			pmap_qremove((vm_offset_t)pc, 1);
			pmap_ptelist_free(&pv_vafree, (vm_offset_t)pc);
			break;
		}
	}
out:
	TAILQ_CONCAT(&pv_chunks, &newtail, pc_lru);
	if (pmap != NULL) {
		pmap_invalidate_all(pmap);
		if (pmap != locked_pmap)
			PMAP_UNLOCK(pmap);
	}
	if (m_pc == NULL && pv_vafree != 0 && free != NULL) {
		m_pc = free;
		free = (void *)m_pc->object;
		/* Recycle a freed page table page. */
		m_pc->wire_count = 1;
		atomic_add_int(&cnt.v_wire_count, 1);
	}
	pmap_free_zero_pages(free);
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

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	PV_STAT(pv_entry_frees++);
	PV_STAT(pv_entry_spare++);
	pv_entry_count--;
	pc = pv_to_chunk(pv);
	idx = pv - &pc->pc_pventry[0];
	field = idx / 32;
	bit = idx % 32;
	pc->pc_map[field] |= 1ul << bit;
	for (idx = 0; idx < _NPCM; idx++)
		if (pc->pc_map[idx] != pc_freemask[idx]) {
			/*
			 * 98% of the time, pc is already at the head of the
			 * list.  If it isn't already, move it to the head.
			 */
			if (__predict_false(TAILQ_FIRST(&pmap->pm_pvchunk) !=
			    pc)) {
				TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
				TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc,
				    pc_list);
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

 	TAILQ_REMOVE(&pv_chunks, pc, pc_lru);
	PV_STAT(pv_entry_spare -= _NPCPV);
	PV_STAT(pc_chunk_count--);
	PV_STAT(pc_chunk_frees++);
	/* entire chunk is free, return it */
	m = PHYS_TO_VM_PAGE(pmap_kextract((vm_offset_t)pc));
	pmap_qremove((vm_offset_t)pc, 1);
	vm_page_unwire(m, 0);
	vm_page_free(m);
	pmap_ptelist_free(&pv_vafree, (vm_offset_t)pc);
}

/*
 * get a new pv_entry, allocating a block from the system
 * when needed.
 */
static pv_entry_t
get_pv_entry(pmap_t pmap, boolean_t try)
{
	static const struct timeval printinterval = { 60, 0 };
	static struct timeval lastprint;
	int bit, field;
	pv_entry_t pv;
	struct pv_chunk *pc;
	vm_page_t m;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	rw_assert(&pvh_global_lock, RA_WLOCKED);
	PV_STAT(pv_entry_allocs++);
	pv_entry_count++;
	if (pv_entry_count > pv_entry_high_water)
		if (ratecheck(&lastprint, &printinterval))
			printf("Approaching the limit on PV entries, consider "
			    "increasing either the vm.pmap.shpgperproc or the "
			    "vm.pmap.pv_entry_max tunable.\n");
retry:
	pc = TAILQ_FIRST(&pmap->pm_pvchunk);
	if (pc != NULL) {
		for (field = 0; field < _NPCM; field++) {
			if (pc->pc_map[field]) {
				bit = bsfl(pc->pc_map[field]);
				break;
			}
		}
		if (field < _NPCM) {
			pv = &pc->pc_pventry[field * 32 + bit];
			pc->pc_map[field] &= ~(1ul << bit);
			/* If this was the last item, move it to tail */
			for (field = 0; field < _NPCM; field++)
				if (pc->pc_map[field] != 0) {
					PV_STAT(pv_entry_spare--);
					return (pv);	/* not full, return */
				}
			TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
			TAILQ_INSERT_TAIL(&pmap->pm_pvchunk, pc, pc_list);
			PV_STAT(pv_entry_spare--);
			return (pv);
		}
	}
	/*
	 * Access to the ptelist "pv_vafree" is synchronized by the page
	 * queues lock.  If "pv_vafree" is currently non-empty, it will
	 * remain non-empty until pmap_ptelist_alloc() completes.
	 */
	if (pv_vafree == 0 || (m = vm_page_alloc(NULL, 0, VM_ALLOC_NORMAL |
	    VM_ALLOC_NOOBJ | VM_ALLOC_WIRED)) == NULL) {
		if (try) {
			pv_entry_count--;
			PV_STAT(pc_chunk_tryfail++);
			return (NULL);
		}
		m = pmap_pv_reclaim(pmap);
		if (m == NULL)
			goto retry;
	}
	PV_STAT(pc_chunk_count++);
	PV_STAT(pc_chunk_allocs++);
	pc = (struct pv_chunk *)pmap_ptelist_alloc(&pv_vafree);
	pmap_qenter((vm_offset_t)pc, &m, 1);
	if ((m->flags & PG_ZERO) == 0)
		pagezero(pc);
	pc->pc_pmap = pmap;
	pc->pc_map[0] = pc_freemask[0] & ~1ul;	/* preallocated bit 0 */
	for (field = 1; field < _NPCM; field++)
		pc->pc_map[field] = pc_freemask[field];
	TAILQ_INSERT_TAIL(&pv_chunks, pc, pc_lru);
	pv = &pc->pc_pventry[0];
	TAILQ_INSERT_HEAD(&pmap->pm_pvchunk, pc, pc_list);
	PV_STAT(pv_entry_spare += _NPCPV - 1);
	return (pv);
}

static __inline pv_entry_t
pmap_pvh_remove(struct md_page *pvh, pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
		if (pmap == PV_PMAP(pv) && va == pv->pv_va) {
			TAILQ_REMOVE(&pvh->pv_list, pv, pv_next);
			break;
		}
	}
	return (pv);
}

static void
pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va)
{
	pv_entry_t pv;

	pv = pmap_pvh_remove(pvh, pmap, va);
	KASSERT(pv != NULL, ("pmap_pvh_free: pv not found"));
	free_pv_entry(pmap, pv);
}

static void
pmap_remove_entry(pmap_t pmap, vm_page_t m, vm_offset_t va)
{

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	pmap_pvh_free(&m->md, pmap, va);
	if (TAILQ_EMPTY(&m->md.pv_list))
		vm_page_aflag_clear(m, PGA_WRITEABLE);
}

/*
 * Conditionally create a pv entry.
 */
static boolean_t
pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	pv_entry_t pv;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	rw_assert(&pvh_global_lock, RA_WLOCKED);
	if (pv_entry_count < pv_entry_high_water && 
	    (pv = get_pv_entry(pmap, TRUE)) != NULL) {
		pv->pv_va = va;
		TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
		return (TRUE);
	} else
		return (FALSE);
}

/*
 * pmap_remove_pte: do the things to unmap a page in a process
 */
static int
pmap_remove_pte(pmap_t pmap, pt_entry_t *ptq, vm_offset_t va, vm_page_t *free)
{
	pt_entry_t oldpte;
	vm_page_t m;

	CTR3(KTR_PMAP, "pmap_remove_pte: pmap=%p *ptq=0x%x va=0x%x",
	    pmap, (u_long)*ptq, va);
	
	rw_assert(&pvh_global_lock, RA_WLOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	oldpte = *ptq;
	PT_SET_VA_MA(ptq, 0, TRUE);
	KASSERT(oldpte != 0,
	    ("pmap_remove_pte: pmap %p va %x zero pte", pmap, va));
	if (oldpte & PG_W)
		pmap->pm_stats.wired_count -= 1;
	/*
	 * Machines that don't support invlpg, also don't support
	 * PG_G.
	 */
	if (oldpte & PG_G)
		pmap_invalidate_page(kernel_pmap, va);
	pmap->pm_stats.resident_count -= 1;
	if (oldpte & PG_MANAGED) {
		m = PHYS_TO_VM_PAGE(xpmap_mtop(oldpte) & PG_FRAME);
		if ((oldpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
			vm_page_dirty(m);
		if (oldpte & PG_A)
			vm_page_aflag_set(m, PGA_REFERENCED);
		pmap_remove_entry(pmap, m, va);
	}
	return (pmap_unuse_pt(pmap, va, free));
}

/*
 * Remove a single page from a process address space
 */
static void
pmap_remove_page(pmap_t pmap, vm_offset_t va, vm_page_t *free)
{
	pt_entry_t *pte;

	CTR2(KTR_PMAP, "pmap_remove_page: pmap=%p va=0x%x",
	    pmap, va);
	
	rw_assert(&pvh_global_lock, RA_WLOCKED);
	KASSERT(curthread->td_pinned > 0, ("curthread not pinned"));
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if ((pte = pmap_pte_quick(pmap, va)) == NULL || (*pte & PG_V) == 0)
		return;
	pmap_remove_pte(pmap, pte, va, free);
	pmap_invalidate_page(pmap, va);
	if (*PMAP1)
		PT_SET_MA(PADDR1, 0);

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
	vm_offset_t pdnxt;
	pd_entry_t ptpaddr;
	pt_entry_t *pte;
	vm_page_t free = NULL;
	int anyvalid;

	CTR3(KTR_PMAP, "pmap_remove: pmap=%p sva=0x%x eva=0x%x",
	    pmap, sva, eva);

	/*
	 * Perform an unsynchronized read.  This is, however, safe.
	 */
	if (pmap->pm_stats.resident_count == 0)
		return;

	anyvalid = 0;

	rw_wlock(&pvh_global_lock);
	sched_pin();
	PMAP_LOCK(pmap);

	/*
	 * special handling of removing one page.  a very
	 * common operation and easy to short circuit some
	 * code.
	 */
	if ((sva + PAGE_SIZE == eva) && 
	    ((pmap->pm_pdir[(sva >> PDRSHIFT)] & PG_PS) == 0)) {
		pmap_remove_page(pmap, sva, &free);
		goto out;
	}

	for (; sva < eva; sva = pdnxt) {
		u_int pdirindex;

		/*
		 * Calculate index for next page table.
		 */
		pdnxt = (sva + NBPDR) & ~PDRMASK;
		if (pdnxt < sva)
			pdnxt = eva;
		if (pmap->pm_stats.resident_count == 0)
			break;

		pdirindex = sva >> PDRSHIFT;
		ptpaddr = pmap->pm_pdir[pdirindex];

		/*
		 * Weed out invalid mappings. Note: we assume that the page
		 * directory table is always allocated, and in kernel virtual.
		 */
		if (ptpaddr == 0)
			continue;

		/*
		 * Check for large page.
		 */
		if ((ptpaddr & PG_PS) != 0) {
			PD_CLEAR_VA(pmap, pdirindex, TRUE);
			pmap->pm_stats.resident_count -= NBPDR / PAGE_SIZE;
			anyvalid = 1;
			continue;
		}

		/*
		 * Limit our scan to either the end of the va represented
		 * by the current page table page, or to the end of the
		 * range being removed.
		 */
		if (pdnxt > eva)
			pdnxt = eva;

		for (pte = pmap_pte_quick(pmap, sva); sva != pdnxt; pte++,
		    sva += PAGE_SIZE) {
			if ((*pte & PG_V) == 0)
				continue;

			/*
			 * The TLB entry for a PG_G mapping is invalidated
			 * by pmap_remove_pte().
			 */
			if ((*pte & PG_G) == 0)
				anyvalid = 1;
			if (pmap_remove_pte(pmap, pte, sva, &free))
				break;
		}
	}
	PT_UPDATES_FLUSH();
	if (*PMAP1)
		PT_SET_VA_MA(PMAP1, 0, TRUE);
out:
	if (anyvalid)
		pmap_invalidate_all(pmap);
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
	pmap_free_zero_pages(free);
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

void
pmap_remove_all(vm_page_t m)
{
	pv_entry_t pv;
	pmap_t pmap;
	pt_entry_t *pte, tpte;
	vm_page_t free;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_all: page %p is not managed", m));
	free = NULL;
	rw_wlock(&pvh_global_lock);
	sched_pin();
	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pmap->pm_stats.resident_count--;
		pte = pmap_pte_quick(pmap, pv->pv_va);
		tpte = *pte;
		PT_SET_VA_MA(pte, 0, TRUE);
		KASSERT(tpte != 0, ("pmap_remove_all: pmap %p va %x zero pte",
		    pmap, pv->pv_va));
		if (tpte & PG_W)
			pmap->pm_stats.wired_count--;
		if (tpte & PG_A)
			vm_page_aflag_set(m, PGA_REFERENCED);

		/*
		 * Update the vm_page_t clean and reference bits.
		 */
		if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW))
			vm_page_dirty(m);
		pmap_unuse_pt(pmap, pv->pv_va, &free);
		pmap_invalidate_page(pmap, pv->pv_va);
		TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
		free_pv_entry(pmap, pv);
		PMAP_UNLOCK(pmap);
	}
	vm_page_aflag_clear(m, PGA_WRITEABLE);
	PT_UPDATES_FLUSH();
	if (*PMAP1)
		PT_SET_MA(PADDR1, 0);
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
	pmap_free_zero_pages(free);
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 */
void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	vm_offset_t pdnxt;
	pd_entry_t ptpaddr;
	pt_entry_t *pte;
	int anychanged;

	CTR4(KTR_PMAP, "pmap_protect: pmap=%p sva=0x%x eva=0x%x prot=0x%x",
	    pmap, sva, eva, prot);
	
	if ((prot & VM_PROT_READ) == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

#ifdef PAE
	if ((prot & (VM_PROT_WRITE|VM_PROT_EXECUTE)) ==
	    (VM_PROT_WRITE|VM_PROT_EXECUTE))
		return;
#else
	if (prot & VM_PROT_WRITE)
		return;
#endif

	anychanged = 0;

	rw_wlock(&pvh_global_lock);
	sched_pin();
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = pdnxt) {
		pt_entry_t obits, pbits;
		u_int pdirindex;

		pdnxt = (sva + NBPDR) & ~PDRMASK;
		if (pdnxt < sva)
			pdnxt = eva;

		pdirindex = sva >> PDRSHIFT;
		ptpaddr = pmap->pm_pdir[pdirindex];

		/*
		 * Weed out invalid mappings. Note: we assume that the page
		 * directory table is always allocated, and in kernel virtual.
		 */
		if (ptpaddr == 0)
			continue;

		/*
		 * Check for large page.
		 */
		if ((ptpaddr & PG_PS) != 0) {
			if ((prot & VM_PROT_WRITE) == 0)
				pmap->pm_pdir[pdirindex] &= ~(PG_M|PG_RW);
#ifdef PAE
			if ((prot & VM_PROT_EXECUTE) == 0)
				pmap->pm_pdir[pdirindex] |= pg_nx;
#endif
			anychanged = 1;
			continue;
		}

		if (pdnxt > eva)
			pdnxt = eva;

		for (pte = pmap_pte_quick(pmap, sva); sva != pdnxt; pte++,
		    sva += PAGE_SIZE) {
			vm_page_t m;

retry:
			/*
			 * Regardless of whether a pte is 32 or 64 bits in
			 * size, PG_RW, PG_A, and PG_M are among the least
			 * significant 32 bits.
			 */
			obits = pbits = *pte;
			if ((pbits & PG_V) == 0)
				continue;

			if ((prot & VM_PROT_WRITE) == 0) {
				if ((pbits & (PG_MANAGED | PG_M | PG_RW)) ==
				    (PG_MANAGED | PG_M | PG_RW)) {
					m = PHYS_TO_VM_PAGE(xpmap_mtop(pbits) &
					    PG_FRAME);
					vm_page_dirty(m);
				}
				pbits &= ~(PG_RW | PG_M);
			}
#ifdef PAE
			if ((prot & VM_PROT_EXECUTE) == 0)
				pbits |= pg_nx;
#endif

			if (pbits != obits) {
				obits = *pte;
				PT_SET_VA_MA(pte, pbits, TRUE);
				if (*pte != pbits)
					goto retry;
				if (obits & PG_G)
					pmap_invalidate_page(pmap, sva);
				else
					anychanged = 1;
			}
		}
	}
	PT_UPDATES_FLUSH();
	if (*PMAP1)
		PT_SET_VA_MA(PMAP1, 0, TRUE);
	if (anychanged)
		pmap_invalidate_all(pmap);
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
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
pmap_enter(pmap_t pmap, vm_offset_t va, vm_prot_t access, vm_page_t m,
    vm_prot_t prot, boolean_t wired)
{
	pd_entry_t *pde;
	pt_entry_t *pte;
	pt_entry_t newpte, origpte;
	pv_entry_t pv;
	vm_paddr_t opa, pa;
	vm_page_t mpte, om;
	boolean_t invlva;

	CTR6(KTR_PMAP, "pmap_enter: pmap=%08p va=0x%08x access=0x%x ma=0x%08x prot=0x%x wired=%d",
	    pmap, va, access, VM_PAGE_TO_MACH(m), prot, wired);
	va = trunc_page(va);
	KASSERT(va <= VM_MAX_KERNEL_ADDRESS, ("pmap_enter: toobig"));
	KASSERT(va < UPT_MIN_ADDRESS || va >= UPT_MAX_ADDRESS,
	    ("pmap_enter: invalid to pmap_enter page table pages (va: 0x%x)",
	    va));
	if ((m->oflags & (VPO_UNMANAGED | VPO_BUSY)) == 0)
		VM_OBJECT_ASSERT_WLOCKED(m->object);

	mpte = NULL;

	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	sched_pin();

	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		mpte = pmap_allocpte(pmap, va, M_WAITOK);
	}

	pde = pmap_pde(pmap, va);
	if ((*pde & PG_PS) != 0)
		panic("pmap_enter: attempted pmap_enter on 4MB page");
	pte = pmap_pte_quick(pmap, va);

	/*
	 * Page Directory table entry not valid, we need a new PT page
	 */
	if (pte == NULL) {
		panic("pmap_enter: invalid page directory pdir=%#jx, va=%#x",
			(uintmax_t)pmap->pm_pdir[va >> PDRSHIFT], va);
	}

	pa = VM_PAGE_TO_PHYS(m);
	om = NULL;
	opa = origpte = 0;

#if 0
	KASSERT((*pte & PG_V) || (*pte == 0), ("address set but not valid pte=%p *pte=0x%016jx",
		pte, *pte));
#endif
	origpte = *pte;
	if (origpte)
		origpte = xpmap_mtop(origpte);
	opa = origpte & PG_FRAME;

	/*
	 * Mapping has not changed, must be protection or wiring change.
	 */
	if (origpte && (opa == pa)) {
		/*
		 * Wiring change, just update stats. We don't worry about
		 * wiring PT pages as they remain resident as long as there
		 * are valid mappings in them. Hence, if a user page is wired,
		 * the PT page will be also.
		 */
		if (wired && ((origpte & PG_W) == 0))
			pmap->pm_stats.wired_count++;
		else if (!wired && (origpte & PG_W))
			pmap->pm_stats.wired_count--;

		/*
		 * Remove extra pte reference
		 */
		if (mpte)
			mpte->wire_count--;

		if (origpte & PG_MANAGED) {
			om = m;
			pa |= PG_MANAGED;
		}
		goto validate;
	} 

	pv = NULL;

	/*
	 * Mapping has changed, invalidate old range and fall through to
	 * handle validating new mapping.
	 */
	if (opa) {
		if (origpte & PG_W)
			pmap->pm_stats.wired_count--;
		if (origpte & PG_MANAGED) {
			om = PHYS_TO_VM_PAGE(opa);
			pv = pmap_pvh_remove(&om->md, pmap, va);
		} else if (va < VM_MAXUSER_ADDRESS) 
			printf("va=0x%x is unmanaged :-( \n", va);
			
		if (mpte != NULL) {
			mpte->wire_count--;
			KASSERT(mpte->wire_count > 0,
			    ("pmap_enter: missing reference to page table page,"
			     " va: 0x%x", va));
		}
	} else
		pmap->pm_stats.resident_count++;

	/*
	 * Enter on the PV list if part of our managed memory.
	 */
	if ((m->oflags & VPO_UNMANAGED) == 0) {
		KASSERT(va < kmi.clean_sva || va >= kmi.clean_eva,
		    ("pmap_enter: managed mapping within the clean submap"));
		if (pv == NULL)
			pv = get_pv_entry(pmap, FALSE);
		pv->pv_va = va;
		TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
		pa |= PG_MANAGED;
	} else if (pv != NULL)
		free_pv_entry(pmap, pv);

	/*
	 * Increment counters
	 */
	if (wired)
		pmap->pm_stats.wired_count++;

validate:
	/*
	 * Now validate mapping with desired protection/wiring.
	 */
	newpte = (pt_entry_t)(pa | PG_V);
	if ((prot & VM_PROT_WRITE) != 0) {
		newpte |= PG_RW;
		if ((newpte & PG_MANAGED) != 0)
			vm_page_aflag_set(m, PGA_WRITEABLE);
	}
#ifdef PAE
	if ((prot & VM_PROT_EXECUTE) == 0)
		newpte |= pg_nx;
#endif
	if (wired)
		newpte |= PG_W;
	if (va < VM_MAXUSER_ADDRESS)
		newpte |= PG_U;
	if (pmap == kernel_pmap)
		newpte |= pgeflag;

	critical_enter();
	/*
	 * if the mapping or permission bits are different, we need
	 * to update the pte.
	 */
	if ((origpte & ~(PG_M|PG_A)) != newpte) {
		if (origpte) {
			invlva = FALSE;
			origpte = *pte;
			PT_SET_VA(pte, newpte | PG_A, FALSE);
			if (origpte & PG_A) {
				if (origpte & PG_MANAGED)
					vm_page_aflag_set(om, PGA_REFERENCED);
				if (opa != VM_PAGE_TO_PHYS(m))
					invlva = TRUE;
#ifdef PAE
				if ((origpte & PG_NX) == 0 &&
				    (newpte & PG_NX) != 0)
					invlva = TRUE;
#endif
			}
			if ((origpte & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
				if ((origpte & PG_MANAGED) != 0)
					vm_page_dirty(om);
				if ((prot & VM_PROT_WRITE) == 0)
					invlva = TRUE;
			}
			if ((origpte & PG_MANAGED) != 0 &&
			    TAILQ_EMPTY(&om->md.pv_list))
				vm_page_aflag_clear(om, PGA_WRITEABLE);
			if (invlva)
				pmap_invalidate_page(pmap, va);
		} else{
			PT_SET_VA(pte, newpte | PG_A, FALSE);
		}
		
	}
	PT_UPDATES_FLUSH();
	critical_exit();
	if (*PMAP1)
		PT_SET_VA_MA(PMAP1, 0, TRUE);
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
}

/*
 * Maps a sequence of resident pages belonging to the same object.
 * The sequence begins with the given page m_start.  This page is
 * mapped at the given virtual address start.  Each subsequent page is
 * mapped at a virtual address that is offset from start by the same
 * amount as the page is offset from m_start within the object.  The
 * last page in the sequence is the page with the largest offset from
 * m_start that can be mapped at a virtual address less than the given
 * virtual address end.  Not every virtual page between start and end
 * is mapped; only those for which a resident page exists with the
 * corresponding offset from m_start are mapped.
 */
void
pmap_enter_object(pmap_t pmap, vm_offset_t start, vm_offset_t end,
    vm_page_t m_start, vm_prot_t prot)
{
	vm_page_t m, mpte;
	vm_pindex_t diff, psize;
	multicall_entry_t mcl[16];
	multicall_entry_t *mclp = mcl;
	int error, count = 0;

	VM_OBJECT_ASSERT_LOCKED(m_start->object);

	psize = atop(end - start);
	mpte = NULL;
	m = m_start;
	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
		mpte = pmap_enter_quick_locked(&mclp, &count, pmap, start + ptoa(diff), m,
		    prot, mpte);
		m = TAILQ_NEXT(m, listq);
		if (count == 16) {
			error = HYPERVISOR_multicall(mcl, count);
			KASSERT(error == 0, ("bad multicall %d", error));
			mclp = mcl;
			count = 0;
		}
	}
	if (count) {
		error = HYPERVISOR_multicall(mcl, count);
		KASSERT(error == 0, ("bad multicall %d", error));
	}
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
}

/*
 * this code makes some *MAJOR* assumptions:
 * 1. Current pmap & pmap exists.
 * 2. Not wired.
 * 3. Read access.
 * 4. No page table pages.
 * but is *MUCH* faster than pmap_enter...
 */

void
pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot)
{
	multicall_entry_t mcl, *mclp;
	int count = 0;
	mclp = &mcl;

	CTR4(KTR_PMAP, "pmap_enter_quick: pmap=%p va=0x%x m=%p prot=0x%x",
	    pmap, va, m, prot);
	
	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	(void)pmap_enter_quick_locked(&mclp, &count, pmap, va, m, prot, NULL);
	if (count)
		HYPERVISOR_multicall(&mcl, count);
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
}

#ifdef notyet
void
pmap_enter_quick_range(pmap_t pmap, vm_offset_t *addrs, vm_page_t *pages, vm_prot_t *prots, int count)
{
	int i, error, index = 0;
	multicall_entry_t mcl[16];
	multicall_entry_t *mclp = mcl;
		
	PMAP_LOCK(pmap);
	for (i = 0; i < count; i++, addrs++, pages++, prots++) {
		if (!pmap_is_prefaultable_locked(pmap, *addrs))
			continue;

		(void) pmap_enter_quick_locked(&mclp, &index, pmap, *addrs, *pages, *prots, NULL);
		if (index == 16) {
			error = HYPERVISOR_multicall(mcl, index);
			mclp = mcl;
			index = 0;
			KASSERT(error == 0, ("bad multicall %d", error));
		}
	}
	if (index) {
		error = HYPERVISOR_multicall(mcl, index);
		KASSERT(error == 0, ("bad multicall %d", error));
	}
	
	PMAP_UNLOCK(pmap);
}
#endif

static vm_page_t
pmap_enter_quick_locked(multicall_entry_t **mclpp, int *count, pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot, vm_page_t mpte)
{
	pt_entry_t *pte;
	vm_paddr_t pa;
	vm_page_t free;
	multicall_entry_t *mcl = *mclpp;

	KASSERT(va < kmi.clean_sva || va >= kmi.clean_eva ||
	    (m->oflags & VPO_UNMANAGED) != 0,
	    ("pmap_enter_quick_locked: managed mapping within the clean submap"));
	rw_assert(&pvh_global_lock, RA_WLOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		u_int ptepindex;
		pd_entry_t ptema;

		/*
		 * Calculate pagetable page index
		 */
		ptepindex = va >> PDRSHIFT;
		if (mpte && (mpte->pindex == ptepindex)) {
			mpte->wire_count++;
		} else {
			/*
			 * Get the page directory entry
			 */
			ptema = pmap->pm_pdir[ptepindex];

			/*
			 * If the page table page is mapped, we just increment
			 * the hold count, and activate it.
			 */
			if (ptema & PG_V) {
				if (ptema & PG_PS)
					panic("pmap_enter_quick: unexpected mapping into 4MB page");
				mpte = PHYS_TO_VM_PAGE(xpmap_mtop(ptema) & PG_FRAME);
				mpte->wire_count++;
			} else {
				mpte = _pmap_allocpte(pmap, ptepindex,
				    M_NOWAIT);
				if (mpte == NULL)
					return (mpte);
			}
		}
	} else {
		mpte = NULL;
	}

	/*
	 * This call to vtopte makes the assumption that we are
	 * entering the page into the current pmap.  In order to support
	 * quick entry into any pmap, one would likely use pmap_pte_quick.
	 * But that isn't as quick as vtopte.
	 */
	KASSERT(pmap_is_current(pmap), ("entering pages in non-current pmap"));
	pte = vtopte(va);
	if (*pte & PG_V) {
		if (mpte != NULL) {
			mpte->wire_count--;
			mpte = NULL;
		}
		return (mpte);
	}

	/*
	 * Enter on the PV list if part of our managed memory.
	 */
	if ((m->oflags & VPO_UNMANAGED) == 0 &&
	    !pmap_try_insert_pv_entry(pmap, va, m)) {
		if (mpte != NULL) {
			free = NULL;
			if (pmap_unwire_ptp(pmap, mpte, &free)) {
				pmap_invalidate_page(pmap, va);
				pmap_free_zero_pages(free);
			}
			
			mpte = NULL;
		}
		return (mpte);
	}

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;

	pa = VM_PAGE_TO_PHYS(m);
#ifdef PAE
	if ((prot & VM_PROT_EXECUTE) == 0)
		pa |= pg_nx;
#endif

#if 0
	/*
	 * Now validate mapping with RO protection
	 */
	if ((m->oflags & VPO_UNMANAGED) != 0)
		pte_store(pte, pa | PG_V | PG_U);
	else
		pte_store(pte, pa | PG_V | PG_U | PG_MANAGED);
#else
	/*
	 * Now validate mapping with RO protection
	 */
	if ((m->oflags & VPO_UNMANAGED) != 0)
		pa = 	xpmap_ptom(pa | PG_V | PG_U);
	else
		pa = xpmap_ptom(pa | PG_V | PG_U | PG_MANAGED);

	mcl->op = __HYPERVISOR_update_va_mapping;
	mcl->args[0] = va;
	mcl->args[1] = (uint32_t)(pa & 0xffffffff);
	mcl->args[2] = (uint32_t)(pa >> 32);
	mcl->args[3] = 0;
	*mclpp = mcl + 1;
	*count = *count + 1;
#endif	
	return (mpte);
}

/*
 * Make a temporary mapping for a physical address.  This is only intended
 * to be used for panic dumps.
 */
void *
pmap_kenter_temporary(vm_paddr_t pa, int i)
{
	vm_offset_t va;
	vm_paddr_t ma = xpmap_ptom(pa);

	va = (vm_offset_t)crashdumpmap + (i * PAGE_SIZE);
	PT_SET_MA(va, (ma & ~PAGE_MASK) | PG_V | pgeflag);
	invlpg(va);
	return ((void *)crashdumpmap);
}

/*
 * This code maps large physical mmap regions into the
 * processor address space.  Note that some shortcuts
 * are taken, but the code works.
 */
void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr, vm_object_t object,
    vm_pindex_t pindex, vm_size_t size)
{
	pd_entry_t *pde;
	vm_paddr_t pa, ptepa;
	vm_page_t p;
	int pat_mode;

	VM_OBJECT_ASSERT_WLOCKED(object);
	KASSERT(object->type == OBJT_DEVICE || object->type == OBJT_SG,
	    ("pmap_object_init_pt: non-device object"));
	if (pseflag && 
	    (addr & (NBPDR - 1)) == 0 && (size & (NBPDR - 1)) == 0) {
		if (!vm_object_populate(object, pindex, pindex + atop(size)))
			return;
		p = vm_page_lookup(object, pindex);
		KASSERT(p->valid == VM_PAGE_BITS_ALL,
		    ("pmap_object_init_pt: invalid page %p", p));
		pat_mode = p->md.pat_mode;

		/*
		 * Abort the mapping if the first page is not physically
		 * aligned to a 2/4MB page boundary.
		 */
		ptepa = VM_PAGE_TO_PHYS(p);
		if (ptepa & (NBPDR - 1))
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
			    pat_mode != p->md.pat_mode)
				return;
			p = TAILQ_NEXT(p, listq);
		}

		/*
		 * Map using 2/4MB pages.  Since "ptepa" is 2/4M aligned and
		 * "size" is a multiple of 2/4M, adding the PAT setting to
		 * "pa" will not affect the termination of this loop.
		 */
		PMAP_LOCK(pmap);
		for (pa = ptepa | pmap_cache_bits(pat_mode, 1); pa < ptepa +
		    size; pa += NBPDR) {
			pde = pmap_pde(pmap, addr);
			if (*pde == 0) {
				pde_store(pde, pa | PG_PS | PG_M | PG_A |
				    PG_U | PG_RW | PG_V);
				pmap->pm_stats.resident_count += NBPDR /
				    PAGE_SIZE;
				pmap_pde_mappings++;
			}
			/* Else continue on if the PDE is already valid. */
			addr += NBPDR;
		}
		PMAP_UNLOCK(pmap);
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
pmap_change_wiring(pmap_t pmap, vm_offset_t va, boolean_t wired)
{
	pt_entry_t *pte;

	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	pte = pmap_pte(pmap, va);

	if (wired && !pmap_pte_w(pte)) {
		PT_SET_VA_MA((pte), *(pte) | PG_W, TRUE);
		pmap->pm_stats.wired_count++;
	} else if (!wired && pmap_pte_w(pte)) {
		PT_SET_VA_MA((pte), *(pte) & ~PG_W, TRUE);
		pmap->pm_stats.wired_count--;
	}
	
	/*
	 * Wiring is not a hardware characteristic so there is no need to
	 * invalidate TLB.
	 */
	pmap_pte_release(pte);
	PMAP_UNLOCK(pmap);
	rw_wunlock(&pvh_global_lock);
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
	vm_page_t   free;
	vm_offset_t addr;
	vm_offset_t end_addr = src_addr + len;
	vm_offset_t pdnxt;

	if (dst_addr != src_addr)
		return;

	if (!pmap_is_current(src_pmap)) {
		CTR2(KTR_PMAP,
		    "pmap_copy, skipping: pdir[PTDPTDI]=0x%jx PTDpde[0]=0x%jx",
		    (src_pmap->pm_pdir[PTDPTDI] & PG_FRAME), (PTDpde[0] & PG_FRAME));
		
		return;
	}
	CTR5(KTR_PMAP, "pmap_copy:  dst_pmap=%p src_pmap=%p dst_addr=0x%x len=%d src_addr=0x%x",
	    dst_pmap, src_pmap, dst_addr, len, src_addr);
	
#ifdef HAMFISTED_LOCKING
	mtx_lock(&createdelete_lock);
#endif

	rw_wlock(&pvh_global_lock);
	if (dst_pmap < src_pmap) {
		PMAP_LOCK(dst_pmap);
		PMAP_LOCK(src_pmap);
	} else {
		PMAP_LOCK(src_pmap);
		PMAP_LOCK(dst_pmap);
	}
	sched_pin();
	for (addr = src_addr; addr < end_addr; addr = pdnxt) {
		pt_entry_t *src_pte, *dst_pte;
		vm_page_t dstmpte, srcmpte;
		pd_entry_t srcptepaddr;
		u_int ptepindex;

		KASSERT(addr < UPT_MIN_ADDRESS,
		    ("pmap_copy: invalid to pmap_copy page tables"));

		pdnxt = (addr + NBPDR) & ~PDRMASK;
		if (pdnxt < addr)
			pdnxt = end_addr;
		ptepindex = addr >> PDRSHIFT;

		srcptepaddr = PT_GET(&src_pmap->pm_pdir[ptepindex]);
		if (srcptepaddr == 0)
			continue;
			
		if (srcptepaddr & PG_PS) {
			if (dst_pmap->pm_pdir[ptepindex] == 0) {
				PD_SET_VA(dst_pmap, ptepindex, srcptepaddr & ~PG_W, TRUE);
				dst_pmap->pm_stats.resident_count +=
				    NBPDR / PAGE_SIZE;
			}
			continue;
		}

		srcmpte = PHYS_TO_VM_PAGE(srcptepaddr & PG_FRAME);
		KASSERT(srcmpte->wire_count > 0,
		    ("pmap_copy: source page table page is unused"));

		if (pdnxt > end_addr)
			pdnxt = end_addr;

		src_pte = vtopte(addr);
		while (addr < pdnxt) {
			pt_entry_t ptetemp;
			ptetemp = *src_pte;
			/*
			 * we only virtual copy managed pages
			 */
			if ((ptetemp & PG_MANAGED) != 0) {
				dstmpte = pmap_allocpte(dst_pmap, addr,
				    M_NOWAIT);
				if (dstmpte == NULL)
					goto out;
				dst_pte = pmap_pte_quick(dst_pmap, addr);
				if (*dst_pte == 0 &&
				    pmap_try_insert_pv_entry(dst_pmap, addr,
				    PHYS_TO_VM_PAGE(xpmap_mtop(ptetemp) & PG_FRAME))) {
					/*
					 * Clear the wired, modified, and
					 * accessed (referenced) bits
					 * during the copy.
					 */
					KASSERT(ptetemp != 0, ("src_pte not set"));
					PT_SET_VA_MA(dst_pte, ptetemp & ~(PG_W | PG_M | PG_A), TRUE /* XXX debug */);
					KASSERT(*dst_pte == (ptetemp & ~(PG_W | PG_M | PG_A)),
					    ("no pmap copy expected: 0x%jx saw: 0x%jx",
						ptetemp &  ~(PG_W | PG_M | PG_A), *dst_pte));
					dst_pmap->pm_stats.resident_count++;
	 			} else {
					free = NULL;
					if (pmap_unwire_ptp(dst_pmap, dstmpte,
					    &free)) {
						pmap_invalidate_page(dst_pmap,
						    addr);
						pmap_free_zero_pages(free);
					}
					goto out;
				}
				if (dstmpte->wire_count >= srcmpte->wire_count)
					break;
			}
			addr += PAGE_SIZE;
			src_pte++;
		}
	}
out:
	PT_UPDATES_FLUSH();
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(src_pmap);
	PMAP_UNLOCK(dst_pmap);

#ifdef HAMFISTED_LOCKING
	mtx_unlock(&createdelete_lock);
#endif
}	

static __inline void
pagezero(void *page)
{
#if defined(I686_CPU)
	if (cpu_class == CPUCLASS_686) {
#if defined(CPU_ENABLE_SSE)
		if (cpu_feature & CPUID_SSE2)
			sse2_pagezero(page);
		else
#endif
			i686_pagezero(page);
	} else
#endif
		bzero(page, PAGE_SIZE);
}

/*
 *	pmap_zero_page zeros the specified hardware page by mapping 
 *	the page into KVM and using bzero to clear its contents.
 */
void
pmap_zero_page(vm_page_t m)
{
	struct sysmaps *sysmaps;

	sysmaps = &sysmaps_pcpu[PCPU_GET(cpuid)];
	mtx_lock(&sysmaps->lock);
	if (*sysmaps->CMAP2)
		panic("pmap_zero_page: CMAP2 busy");
	sched_pin();
	PT_SET_MA(sysmaps->CADDR2, PG_V | PG_RW | VM_PAGE_TO_MACH(m) | PG_A | PG_M);
	pagezero(sysmaps->CADDR2);
	PT_SET_MA(sysmaps->CADDR2, 0);
	sched_unpin();
	mtx_unlock(&sysmaps->lock);
}

/*
 *	pmap_zero_page_area zeros the specified hardware page by mapping 
 *	the page into KVM and using bzero to clear its contents.
 *
 *	off and size may not cover an area beyond a single hardware page.
 */
void
pmap_zero_page_area(vm_page_t m, int off, int size)
{
	struct sysmaps *sysmaps;

	sysmaps = &sysmaps_pcpu[PCPU_GET(cpuid)];
	mtx_lock(&sysmaps->lock);
	if (*sysmaps->CMAP2)
		panic("pmap_zero_page_area: CMAP2 busy");
	sched_pin();
	PT_SET_MA(sysmaps->CADDR2, PG_V | PG_RW | VM_PAGE_TO_MACH(m) | PG_A | PG_M);

	if (off == 0 && size == PAGE_SIZE) 
		pagezero(sysmaps->CADDR2);
	else
		bzero((char *)sysmaps->CADDR2 + off, size);
	PT_SET_MA(sysmaps->CADDR2, 0);
	sched_unpin();
	mtx_unlock(&sysmaps->lock);
}

/*
 *	pmap_zero_page_idle zeros the specified hardware page by mapping 
 *	the page into KVM and using bzero to clear its contents.  This
 *	is intended to be called from the vm_pagezero process only and
 *	outside of Giant.
 */
void
pmap_zero_page_idle(vm_page_t m)
{

	if (*CMAP3)
		panic("pmap_zero_page_idle: CMAP3 busy");
	sched_pin();
	PT_SET_MA(CADDR3, PG_V | PG_RW | VM_PAGE_TO_MACH(m) | PG_A | PG_M);
	pagezero(CADDR3);
	PT_SET_MA(CADDR3, 0);
	sched_unpin();
}

/*
 *	pmap_copy_page copies the specified (machine independent)
 *	page by mapping the page into virtual memory and using
 *	bcopy to copy the page, one machine dependent page at a
 *	time.
 */
void
pmap_copy_page(vm_page_t src, vm_page_t dst)
{
	struct sysmaps *sysmaps;

	sysmaps = &sysmaps_pcpu[PCPU_GET(cpuid)];
	mtx_lock(&sysmaps->lock);
	if (*sysmaps->CMAP1)
		panic("pmap_copy_page: CMAP1 busy");
	if (*sysmaps->CMAP2)
		panic("pmap_copy_page: CMAP2 busy");
	sched_pin();
	PT_SET_MA(sysmaps->CADDR1, PG_V | VM_PAGE_TO_MACH(src) | PG_A);
	PT_SET_MA(sysmaps->CADDR2, PG_V | PG_RW | VM_PAGE_TO_MACH(dst) | PG_A | PG_M);
	bcopy(sysmaps->CADDR1, sysmaps->CADDR2, PAGE_SIZE);
	PT_SET_MA(sysmaps->CADDR1, 0);
	PT_SET_MA(sysmaps->CADDR2, 0);
	sched_unpin();
	mtx_unlock(&sysmaps->lock);
}

int unmapped_buf_allowed = 1;

void
pmap_copy_pages(vm_page_t ma[], vm_offset_t a_offset, vm_page_t mb[],
    vm_offset_t b_offset, int xfersize)
{
	struct sysmaps *sysmaps;
	vm_page_t a_pg, b_pg;
	char *a_cp, *b_cp;
	vm_offset_t a_pg_offset, b_pg_offset;
	int cnt;

	sysmaps = &sysmaps_pcpu[PCPU_GET(cpuid)];
	mtx_lock(&sysmaps->lock);
	if (*sysmaps->CMAP1 != 0)
		panic("pmap_copy_pages: CMAP1 busy");
	if (*sysmaps->CMAP2 != 0)
		panic("pmap_copy_pages: CMAP2 busy");
	sched_pin();
	while (xfersize > 0) {
		a_pg = ma[a_offset >> PAGE_SHIFT];
		a_pg_offset = a_offset & PAGE_MASK;
		cnt = min(xfersize, PAGE_SIZE - a_pg_offset);
		b_pg = mb[b_offset >> PAGE_SHIFT];
		b_pg_offset = b_offset & PAGE_MASK;
		cnt = min(cnt, PAGE_SIZE - b_pg_offset);
		PT_SET_MA(sysmaps->CADDR1, PG_V | VM_PAGE_TO_MACH(a_pg) | PG_A);
		PT_SET_MA(sysmaps->CADDR2, PG_V | PG_RW |
		    VM_PAGE_TO_MACH(b_pg) | PG_A | PG_M);
		a_cp = sysmaps->CADDR1 + a_pg_offset;
		b_cp = sysmaps->CADDR2 + b_pg_offset;
		bcopy(a_cp, b_cp, cnt);
		a_offset += cnt;
		b_offset += cnt;
		xfersize -= cnt;
	}
	PT_SET_MA(sysmaps->CADDR1, 0);
	PT_SET_MA(sysmaps->CADDR2, 0);
	sched_unpin();
	mtx_unlock(&sysmaps->lock);
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
	boolean_t rv;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_page_exists_quick: page %p is not managed", m));
	rv = FALSE;
	rw_wlock(&pvh_global_lock);
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		if (PV_PMAP(pv) == pmap) {
			rv = TRUE;
			break;
		}
		loops++;
		if (loops >= 16)
			break;
	}
	rw_wunlock(&pvh_global_lock);
	return (rv);
}

/*
 *	pmap_page_wired_mappings:
 *
 *	Return the number of managed mappings to the given physical page
 *	that are wired.
 */
int
pmap_page_wired_mappings(vm_page_t m)
{
	pv_entry_t pv;
	pt_entry_t *pte;
	pmap_t pmap;
	int count;

	count = 0;
	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (count);
	rw_wlock(&pvh_global_lock);
	sched_pin();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pte = pmap_pte_quick(pmap, pv->pv_va);
		if ((*pte & PG_W) != 0)
			count++;
		PMAP_UNLOCK(pmap);
	}
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
	return (count);
}

/*
 * Returns TRUE if the given page is mapped.  Otherwise, returns FALSE.
 */
boolean_t
pmap_page_is_mapped(vm_page_t m)
{

	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (FALSE);
	return (!TAILQ_EMPTY(&m->md.pv_list));
}

/*
 * Remove all pages from specified address space
 * this aids process exit speeds.  Also, this code
 * is special cased for current process only, but
 * can have the more generic (and slightly slower)
 * mode enabled.  This is much faster than pmap_remove
 * in the case of running down an entire address space.
 */
void
pmap_remove_pages(pmap_t pmap)
{
	pt_entry_t *pte, tpte;
	vm_page_t m, free = NULL;
	pv_entry_t pv;
	struct pv_chunk *pc, *npc;
	int field, idx;
	int32_t bit;
	uint32_t inuse, bitmask;
	int allfree;

	CTR1(KTR_PMAP, "pmap_remove_pages: pmap=%p", pmap);
	
	if (pmap != vmspace_pmap(curthread->td_proc->p_vmspace)) {
		printf("warning: pmap_remove_pages called with non-current pmap\n");
		return;
	}
	rw_wlock(&pvh_global_lock);
	KASSERT(pmap_is_current(pmap), ("removing pages from non-current pmap"));
	PMAP_LOCK(pmap);
	sched_pin();
	TAILQ_FOREACH_SAFE(pc, &pmap->pm_pvchunk, pc_list, npc) {
		KASSERT(pc->pc_pmap == pmap, ("Wrong pmap %p %p", pmap,
		    pc->pc_pmap));
		allfree = 1;
		for (field = 0; field < _NPCM; field++) {
			inuse = ~pc->pc_map[field] & pc_freemask[field];
			while (inuse != 0) {
				bit = bsfl(inuse);
				bitmask = 1UL << bit;
				idx = field * 32 + bit;
				pv = &pc->pc_pventry[idx];
				inuse &= ~bitmask;

				pte = vtopte(pv->pv_va);
				tpte = *pte ? xpmap_mtop(*pte) : 0;

				if (tpte == 0) {
					printf(
					    "TPTE at %p  IS ZERO @ VA %08x\n",
					    pte, pv->pv_va);
					panic("bad pte");
				}

/*
 * We cannot remove wired pages from a process' mapping at this time
 */
				if (tpte & PG_W) {
					allfree = 0;
					continue;
				}

				m = PHYS_TO_VM_PAGE(tpte & PG_FRAME);
				KASSERT(m->phys_addr == (tpte & PG_FRAME),
				    ("vm_page_t %p phys_addr mismatch %016jx %016jx",
				    m, (uintmax_t)m->phys_addr,
				    (uintmax_t)tpte));

				KASSERT(m < &vm_page_array[vm_page_array_size],
					("pmap_remove_pages: bad tpte %#jx",
					(uintmax_t)tpte));


				PT_CLEAR_VA(pte, FALSE);
				
				/*
				 * Update the vm_page_t clean/reference bits.
				 */
				if (tpte & PG_M)
					vm_page_dirty(m);

				TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
				if (TAILQ_EMPTY(&m->md.pv_list))
					vm_page_aflag_clear(m, PGA_WRITEABLE);

				pmap_unuse_pt(pmap, pv->pv_va, &free);

				/* Mark free */
				PV_STAT(pv_entry_frees++);
				PV_STAT(pv_entry_spare++);
				pv_entry_count--;
				pc->pc_map[field] |= bitmask;
				pmap->pm_stats.resident_count--;			
			}
		}
		PT_UPDATES_FLUSH();
		if (allfree) {
			TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
			free_pv_chunk(pc);
		}
	}
	PT_UPDATES_FLUSH();
	if (*PMAP1)
		PT_SET_MA(PADDR1, 0);

	sched_unpin();
	pmap_invalidate_all(pmap);
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
	pmap_free_zero_pages(free);
}

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
	pt_entry_t *pte;
	pmap_t pmap;
	boolean_t rv;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_is_modified: page %p is not managed", m));
	rv = FALSE;

	/*
	 * If the page is not VPO_BUSY, then PGA_WRITEABLE cannot be
	 * concurrently set while the object is locked.  Thus, if PGA_WRITEABLE
	 * is clear, no PTEs can have PG_M set.
	 */
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if ((m->oflags & VPO_BUSY) == 0 &&
	    (m->aflags & PGA_WRITEABLE) == 0)
		return (rv);
	rw_wlock(&pvh_global_lock);
	sched_pin();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pte = pmap_pte_quick(pmap, pv->pv_va);
		rv = (*pte & PG_M) != 0;
		PMAP_UNLOCK(pmap);
		if (rv)
			break;
	}
	if (*PMAP1)
		PT_SET_MA(PADDR1, 0);
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
	return (rv);
}

/*
 *	pmap_is_prefaultable:
 *
 *	Return whether or not the specified virtual address is elgible
 *	for prefault.
 */
static boolean_t
pmap_is_prefaultable_locked(pmap_t pmap, vm_offset_t addr)
{
	pt_entry_t *pte;
	boolean_t rv = FALSE;

	return (rv);
	
	if (pmap_is_current(pmap) && *pmap_pde(pmap, addr)) {
		pte = vtopte(addr);
		rv = (*pte == 0);
	}
	return (rv);
}

boolean_t
pmap_is_prefaultable(pmap_t pmap, vm_offset_t addr)
{
	boolean_t rv;
	
	PMAP_LOCK(pmap);
	rv = pmap_is_prefaultable_locked(pmap, addr);
	PMAP_UNLOCK(pmap);
	return (rv);
}

boolean_t
pmap_is_referenced(vm_page_t m)
{
	pv_entry_t pv;
	pt_entry_t *pte;
	pmap_t pmap;
	boolean_t rv;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_is_referenced: page %p is not managed", m));
	rv = FALSE;
	rw_wlock(&pvh_global_lock);
	sched_pin();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pte = pmap_pte_quick(pmap, pv->pv_va);
		rv = (*pte & (PG_A | PG_V)) == (PG_A | PG_V);
		PMAP_UNLOCK(pmap);
		if (rv)
			break;
	}
	if (*PMAP1)
		PT_SET_MA(PADDR1, 0);
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
	return (rv);
}

void
pmap_map_readonly(pmap_t pmap, vm_offset_t va, int len)
{
	int i, npages = round_page(len) >> PAGE_SHIFT;
	for (i = 0; i < npages; i++) {
		pt_entry_t *pte;
		pte = pmap_pte(pmap, (vm_offset_t)(va + i*PAGE_SIZE));
		rw_wlock(&pvh_global_lock);
		pte_store(pte, xpmap_mtop(*pte & ~(PG_RW|PG_M)));
		rw_wunlock(&pvh_global_lock);
		PMAP_MARK_PRIV(xpmap_mtop(*pte));
		pmap_pte_release(pte);
	}
}

void
pmap_map_readwrite(pmap_t pmap, vm_offset_t va, int len)
{
	int i, npages = round_page(len) >> PAGE_SHIFT;
	for (i = 0; i < npages; i++) {
		pt_entry_t *pte;
		pte = pmap_pte(pmap, (vm_offset_t)(va + i*PAGE_SIZE));
		PMAP_MARK_UNPRIV(xpmap_mtop(*pte));
		rw_wlock(&pvh_global_lock);
		pte_store(pte, xpmap_mtop(*pte) | (PG_RW|PG_M));
		rw_wunlock(&pvh_global_lock);
		pmap_pte_release(pte);
	}
}

/*
 * Clear the write and modified bits in each of the given page's mappings.
 */
void
pmap_remove_write(vm_page_t m)
{
	pv_entry_t pv;
	pmap_t pmap;
	pt_entry_t oldpte, *pte;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_write: page %p is not managed", m));

	/*
	 * If the page is not VPO_BUSY, then PGA_WRITEABLE cannot be set by
	 * another thread while the object is locked.  Thus, if PGA_WRITEABLE
	 * is clear, no page table entries need updating.
	 */
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if ((m->oflags & VPO_BUSY) == 0 &&
	    (m->aflags & PGA_WRITEABLE) == 0)
		return;
	rw_wlock(&pvh_global_lock);
	sched_pin();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pte = pmap_pte_quick(pmap, pv->pv_va);
retry:
		oldpte = *pte;
		if ((oldpte & PG_RW) != 0) {
			vm_paddr_t newpte = oldpte & ~(PG_RW | PG_M);
			
			/*
			 * Regardless of whether a pte is 32 or 64 bits
			 * in size, PG_RW and PG_M are among the least
			 * significant 32 bits.
			 */
			PT_SET_VA_MA(pte, newpte, TRUE);
			if (*pte != newpte)
				goto retry;
			
			if ((oldpte & PG_M) != 0)
				vm_page_dirty(m);
			pmap_invalidate_page(pmap, pv->pv_va);
		}
		PMAP_UNLOCK(pmap);
	}
	vm_page_aflag_clear(m, PGA_WRITEABLE);
	PT_UPDATES_FLUSH();
	if (*PMAP1)
		PT_SET_MA(PADDR1, 0);
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
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
	pv_entry_t pv, pvf, pvn;
	pmap_t pmap;
	pt_entry_t *pte;
	int rtval = 0;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_ts_referenced: page %p is not managed", m));
	rw_wlock(&pvh_global_lock);
	sched_pin();
	if ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		pvf = pv;
		do {
			pvn = TAILQ_NEXT(pv, pv_next);
			TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
			TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
			pmap = PV_PMAP(pv);
			PMAP_LOCK(pmap);
			pte = pmap_pte_quick(pmap, pv->pv_va);
			if ((*pte & PG_A) != 0) {
				PT_SET_VA_MA(pte, *pte & ~PG_A, FALSE);
				pmap_invalidate_page(pmap, pv->pv_va);
				rtval++;
				if (rtval > 4)
					pvn = NULL;
			}
			PMAP_UNLOCK(pmap);
		} while ((pv = pvn) != NULL && pv != pvf);
	}
	PT_UPDATES_FLUSH();
	if (*PMAP1)
		PT_SET_MA(PADDR1, 0);
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
	return (rtval);
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_page_t m)
{
	pv_entry_t pv;
	pmap_t pmap;
	pt_entry_t *pte;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_clear_modify: page %p is not managed", m));
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	KASSERT((m->oflags & VPO_BUSY) == 0,
	    ("pmap_clear_modify: page %p is busy", m));

	/*
	 * If the page is not PGA_WRITEABLE, then no PTEs can have PG_M set.
	 * If the object containing the page is locked and the page is not
	 * VPO_BUSY, then PGA_WRITEABLE cannot be concurrently set.
	 */
	if ((m->aflags & PGA_WRITEABLE) == 0)
		return;
	rw_wlock(&pvh_global_lock);
	sched_pin();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pte = pmap_pte_quick(pmap, pv->pv_va);
		if ((*pte & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
			/*
			 * Regardless of whether a pte is 32 or 64 bits
			 * in size, PG_M is among the least significant
			 * 32 bits. 
			 */
			PT_SET_VA_MA(pte, *pte & ~PG_M, FALSE);
			pmap_invalidate_page(pmap, pv->pv_va);
		}
		PMAP_UNLOCK(pmap);
	}
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
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
	pmap_t pmap;
	pt_entry_t *pte;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_clear_reference: page %p is not managed", m));
	rw_wlock(&pvh_global_lock);
	sched_pin();
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pte = pmap_pte_quick(pmap, pv->pv_va);
		if ((*pte & PG_A) != 0) {
			/*
			 * Regardless of whether a pte is 32 or 64 bits
			 * in size, PG_A is among the least significant
			 * 32 bits. 
			 */
			PT_SET_VA_MA(pte, *pte & ~PG_A, FALSE);
			pmap_invalidate_page(pmap, pv->pv_va);
		}
		PMAP_UNLOCK(pmap);
	}
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
}

/*
 * Miscellaneous support routines follow
 */

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory.
 */
void *
pmap_mapdev_attr(vm_paddr_t pa, vm_size_t size, int mode)
{
	vm_offset_t va, offset;
	vm_size_t tmpsize;

	offset = pa & PAGE_MASK;
	size = round_page(offset + size);
	pa = pa & PG_FRAME;

	if (pa < KERNLOAD && pa + size <= KERNLOAD)
		va = KERNBASE + pa;
	else
		va = kmem_alloc_nofault(kernel_map, size);
	if (!va)
		panic("pmap_mapdev: Couldn't alloc kernel virtual memory");

	for (tmpsize = 0; tmpsize < size; tmpsize += PAGE_SIZE)
		pmap_kenter_attr(va + tmpsize, pa + tmpsize, mode);
	pmap_invalidate_range(kernel_pmap, va, va + tmpsize);
	pmap_invalidate_cache_range(va, va + size);
	return ((void *)(va + offset));
}

void *
pmap_mapdev(vm_paddr_t pa, vm_size_t size)
{

	return (pmap_mapdev_attr(pa, size, PAT_UNCACHEABLE));
}

void *
pmap_mapbios(vm_paddr_t pa, vm_size_t size)
{

	return (pmap_mapdev_attr(pa, size, PAT_WRITE_BACK));
}

void
pmap_unmapdev(vm_offset_t va, vm_size_t size)
{
	vm_offset_t base, offset;

	if (va >= KERNBASE && va + size <= KERNBASE + KERNLOAD)
		return;
	base = trunc_page(va);
	offset = va & PAGE_MASK;
	size = round_page(offset + size);
	kmem_free(kernel_map, base, size);
}

/*
 * Sets the memory attribute for the specified page.
 */
void
pmap_page_set_memattr(vm_page_t m, vm_memattr_t ma)
{

	m->md.pat_mode = ma;
	if ((m->flags & PG_FICTITIOUS) != 0)
		return;

	/*
	 * If "m" is a normal page, flush it from the cache.
	 * See pmap_invalidate_cache_range().
	 *
	 * First, try to find an existing mapping of the page by sf
	 * buffer. sf_buf_invalidate_cache() modifies mapping and
	 * flushes the cache.
	 */    
	if (sf_buf_invalidate_cache(m))
		return;

	/*
	 * If page is not mapped by sf buffer, but CPU does not
	 * support self snoop, map the page transient and do
	 * invalidation. In the worst case, whole cache is flushed by
	 * pmap_invalidate_cache_range().
	 */
	if ((cpu_feature & CPUID_SS) == 0)
		pmap_flush_page(m);
}

static void
pmap_flush_page(vm_page_t m)
{
	struct sysmaps *sysmaps;
	vm_offset_t sva, eva;

	if ((cpu_feature & CPUID_CLFSH) != 0) {
		sysmaps = &sysmaps_pcpu[PCPU_GET(cpuid)];
		mtx_lock(&sysmaps->lock);
		if (*sysmaps->CMAP2)
			panic("pmap_flush_page: CMAP2 busy");
		sched_pin();
		PT_SET_MA(sysmaps->CADDR2, PG_V | PG_RW |
		    VM_PAGE_TO_MACH(m) | PG_A | PG_M |
		    pmap_cache_bits(m->md.pat_mode, 0));
		invlcaddr(sysmaps->CADDR2);
		sva = (vm_offset_t)sysmaps->CADDR2;
		eva = sva + PAGE_SIZE;

		/*
		 * Use mfence despite the ordering implied by
		 * mtx_{un,}lock() because clflush is not guaranteed
		 * to be ordered by any other instruction.
		 */
		mfence();
		for (; sva < eva; sva += cpu_clflush_line_size)
			clflush(sva);
		mfence();
		PT_SET_MA(sysmaps->CADDR2, 0);
		sched_unpin();
		mtx_unlock(&sysmaps->lock);
	} else
		pmap_invalidate_cache();
}

/*
 * Changes the specified virtual address range's memory type to that given by
 * the parameter "mode".  The specified virtual address range must be
 * completely contained within either the kernel map.
 *
 * Returns zero if the change completed successfully, and either EINVAL or
 * ENOMEM if the change failed.  Specifically, EINVAL is returned if some part
 * of the virtual address range was not mapped, and ENOMEM is returned if
 * there was insufficient memory available to complete the change.
 */
int
pmap_change_attr(vm_offset_t va, vm_size_t size, int mode)
{
	vm_offset_t base, offset, tmpva;
	pt_entry_t *pte;
	u_int opte, npte;
	pd_entry_t *pde;
	boolean_t changed;

	base = trunc_page(va);
	offset = va & PAGE_MASK;
	size = round_page(offset + size);

	/* Only supported on kernel virtual addresses. */
	if (base <= VM_MAXUSER_ADDRESS)
		return (EINVAL);

	/* 4MB pages and pages that aren't mapped aren't supported. */
	for (tmpva = base; tmpva < (base + size); tmpva += PAGE_SIZE) {
		pde = pmap_pde(kernel_pmap, tmpva);
		if (*pde & PG_PS)
			return (EINVAL);
		if ((*pde & PG_V) == 0)
			return (EINVAL);
		pte = vtopte(va);
		if ((*pte & PG_V) == 0)
			return (EINVAL);
	}

	changed = FALSE;

	/*
	 * Ok, all the pages exist and are 4k, so run through them updating
	 * their cache mode.
	 */
	for (tmpva = base; size > 0; ) {
		pte = vtopte(tmpva);

		/*
		 * The cache mode bits are all in the low 32-bits of the
		 * PTE, so we can just spin on updating the low 32-bits.
		 */
		do {
			opte = *(u_int *)pte;
			npte = opte & ~(PG_PTE_PAT | PG_NC_PCD | PG_NC_PWT);
			npte |= pmap_cache_bits(mode, 0);
			PT_SET_VA_MA(pte, npte, TRUE);
		} while (npte != opte && (*pte != npte));
		if (npte != opte)
			changed = TRUE;
		tmpva += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	/*
	 * Flush CPU caches to make sure any data isn't cached that
	 * shouldn't be, etc.
	 */
	if (changed) {
		pmap_invalidate_range(kernel_pmap, base, tmpva);
		pmap_invalidate_cache_range(base, tmpva);
	}
	return (0);
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{
	pt_entry_t *ptep, pte;
	vm_paddr_t pa;
	int val;

	PMAP_LOCK(pmap);
retry:
	ptep = pmap_pte(pmap, addr);
	pte = (ptep != NULL) ? PT_GET(ptep) : 0;
	pmap_pte_release(ptep);
	val = 0;
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
		pa = pte & PG_FRAME;
		/* Ensure that "PHYS_TO_VM_PAGE(pa)->object" doesn't change. */
		if (vm_page_pa_tryrelock(pmap, pa, locked_pa))
			goto retry;
	} else
		PA_UNLOCK_COND(*locked_pa);
	PMAP_UNLOCK(pmap);
	return (val);
}

void
pmap_activate(struct thread *td)
{
	pmap_t	pmap, oldpmap;
	u_int	cpuid;
	u_int32_t  cr3;

	critical_enter();
	pmap = vmspace_pmap(td->td_proc->p_vmspace);
	oldpmap = PCPU_GET(curpmap);
	cpuid = PCPU_GET(cpuid);
#if defined(SMP)
	CPU_CLR_ATOMIC(cpuid, &oldpmap->pm_active);
	CPU_SET_ATOMIC(cpuid, &pmap->pm_active);
#else
	CPU_CLR(cpuid, &oldpmap->pm_active);
	CPU_SET(cpuid, &pmap->pm_active);
#endif
#ifdef PAE
	cr3 = vtophys(pmap->pm_pdpt);
#else
	cr3 = vtophys(pmap->pm_pdir);
#endif
	/*
	 * pmap_activate is for the current thread on the current cpu
	 */
	td->td_pcb->pcb_cr3 = cr3;
	PT_UPDATES_FLUSH();
	load_cr3(cr3);
	PCPU_SET(curpmap, pmap);
	critical_exit();
}

void
pmap_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz)
{
}

/*
 *	Increase the starting virtual address of the given mapping if a
 *	different alignment might result in more superpage mappings.
 */
void
pmap_align_superpage(vm_object_t object, vm_ooffset_t offset,
    vm_offset_t *addr, vm_size_t size)
{
	vm_offset_t superpage_offset;

	if (size < NBPDR)
		return;
	if (object != NULL && (object->flags & OBJ_COLORED) != 0)
		offset += ptoa(object->pg_color);
	superpage_offset = offset & PDRMASK;
	if (size - ((NBPDR - superpage_offset) & PDRMASK) < NBPDR ||
	    (*addr & PDRMASK) == superpage_offset)
		return;
	if ((*addr & PDRMASK) < superpage_offset)
		*addr = (*addr & ~PDRMASK) + superpage_offset;
	else
		*addr = ((*addr + PDRMASK) & ~PDRMASK) + superpage_offset;
}

void
pmap_suspend()
{
	pmap_t pmap;
	int i, pdir, offset;
	vm_paddr_t pdirma;
	mmu_update_t mu[4];

	/*
	 * We need to remove the recursive mapping structure from all
	 * our pmaps so that Xen doesn't get confused when it restores
	 * the page tables. The recursive map lives at page directory
	 * index PTDPTDI. We assume that the suspend code has stopped
	 * the other vcpus (if any).
	 */
	LIST_FOREACH(pmap, &allpmaps, pm_list) {
		for (i = 0; i < 4; i++) {
			/*
			 * Figure out which page directory (L2) page
			 * contains this bit of the recursive map and
			 * the offset within that page of the map
			 * entry
			 */
			pdir = (PTDPTDI + i) / NPDEPG;
			offset = (PTDPTDI + i) % NPDEPG;
			pdirma = pmap->pm_pdpt[pdir] & PG_FRAME;
			mu[i].ptr = pdirma + offset * sizeof(pd_entry_t);
			mu[i].val = 0;
		}
		HYPERVISOR_mmu_update(mu, 4, NULL, DOMID_SELF);
	}
}

void
pmap_resume()
{
	pmap_t pmap;
	int i, pdir, offset;
	vm_paddr_t pdirma;
	mmu_update_t mu[4];

	/*
	 * Restore the recursive map that we removed on suspend.
	 */
	LIST_FOREACH(pmap, &allpmaps, pm_list) {
		for (i = 0; i < 4; i++) {
			/*
			 * Figure out which page directory (L2) page
			 * contains this bit of the recursive map and
			 * the offset within that page of the map
			 * entry
			 */
			pdir = (PTDPTDI + i) / NPDEPG;
			offset = (PTDPTDI + i) % NPDEPG;
			pdirma = pmap->pm_pdpt[pdir] & PG_FRAME;
			mu[i].ptr = pdirma + offset * sizeof(pd_entry_t);
			mu[i].val = (pmap->pm_pdpt[i] & PG_FRAME) | PG_V;
		}
		HYPERVISOR_mmu_update(mu, 4, NULL, DOMID_SELF);
	}
}

#if defined(PMAP_DEBUG)
pmap_pid_dump(int pid)
{
	pmap_t pmap;
	struct proc *p;
	int npte = 0;
	int index;

	sx_slock(&allproc_lock);
	FOREACH_PROC_IN_SYSTEM(p) {
		if (p->p_pid != pid)
			continue;

		if (p->p_vmspace) {
			int i,j;
			index = 0;
			pmap = vmspace_pmap(p->p_vmspace);
			for (i = 0; i < NPDEPTD; i++) {
				pd_entry_t *pde;
				pt_entry_t *pte;
				vm_offset_t base = i << PDRSHIFT;
				
				pde = &pmap->pm_pdir[i];
				if (pde && pmap_pde_v(pde)) {
					for (j = 0; j < NPTEPG; j++) {
						vm_offset_t va = base + (j << PAGE_SHIFT);
						if (va >= (vm_offset_t) VM_MIN_KERNEL_ADDRESS) {
							if (index) {
								index = 0;
								printf("\n");
							}
							sx_sunlock(&allproc_lock);
							return (npte);
						}
						pte = pmap_pte(pmap, va);
						if (pte && pmap_pte_v(pte)) {
							pt_entry_t pa;
							vm_page_t m;
							pa = PT_GET(pte);
							m = PHYS_TO_VM_PAGE(pa & PG_FRAME);
							printf("va: 0x%x, pt: 0x%x, h: %d, w: %d, f: 0x%x",
								va, pa, m->hold_count, m->wire_count, m->flags);
							npte++;
							index++;
							if (index >= 2) {
								index = 0;
								printf("\n");
							} else {
								printf(" ");
							}
						}
					}
				}
			}
		}
	}
	sx_sunlock(&allproc_lock);
	return (npte);
}
#endif

#if defined(DEBUG)

static void	pads(pmap_t pm);
void		pmap_pvdump(vm_paddr_t pa);

/* print address space of pmap*/
static void
pads(pmap_t pm)
{
	int i, j;
	vm_paddr_t va;
	pt_entry_t *ptep;

	if (pm == kernel_pmap)
		return;
	for (i = 0; i < NPDEPTD; i++)
		if (pm->pm_pdir[i])
			for (j = 0; j < NPTEPG; j++) {
				va = (i << PDRSHIFT) + (j << PAGE_SHIFT);
				if (pm == kernel_pmap && va < KERNBASE)
					continue;
				if (pm != kernel_pmap && va > UPT_MAX_ADDRESS)
					continue;
				ptep = pmap_pte(pm, va);
				if (pmap_pte_v(ptep))
					printf("%x:%x ", va, *ptep);
			};

}

void
pmap_pvdump(vm_paddr_t pa)
{
	pv_entry_t pv;
	pmap_t pmap;
	vm_page_t m;

	printf("pa %x", pa);
	m = PHYS_TO_VM_PAGE(pa);
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		printf(" -> pmap %p, va %x", (void *)pmap, pv->pv_va);
		pads(pmap);
	}
	printf(" ");
}
#endif
