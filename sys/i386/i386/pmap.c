/*-
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 * Copyright (c) 1994 John S. Dyson
 * All rights reserved.
 * Copyright (c) 1994 David Greenman
 * All rights reserved.
 * Copyright (c) 2005-2010 Alan L. Cox <alc@cs.rice.edu>
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

#include "opt_apic.h"
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
#include <vm/vm_phys.h>
#include <vm/vm_radix.h>
#include <vm/vm_reserv.h>
#include <vm/uma.h>

#ifdef DEV_APIC
#include <sys/bus.h>
#include <machine/intr_machdep.h>
#include <machine/apicvar.h>
#endif
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

#if !defined(CPU_DISABLE_SSE) && defined(I686_CPU)
#define CPU_ENABLE_SSE
#endif

#ifndef PMAP_SHPGPERPROC
#define PMAP_SHPGPERPROC 200
#endif

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

#define	pa_index(pa)	((pa) >> PDRSHIFT)
#define	pa_to_pvh(pa)	(&pv_table[pa_index(pa)])

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

#define pmap_pte_set_w(pte, v)	((v) ? atomic_set_int((u_int *)(pte), PG_W) : \
    atomic_clear_int((u_int *)(pte), PG_W))
#define pmap_pte_set_prot(pte, v) ((*(int *)pte &= ~PG_PROT), (*(int *)pte |= (v)))

struct pmap kernel_pmap_store;
LIST_HEAD(pmaplist, pmap);
static struct pmaplist allpmaps;
static struct mtx allpmaps_lock;

vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */
int pgeflag = 0;		/* PG_G or-in */
int pseflag = 0;		/* PG_PS or-in */

static int nkpt = NKPT;
vm_offset_t kernel_vm_end = KERNBASE + NKPT * NBPDR;
extern u_int32_t KERNend;
extern u_int32_t KPTphys;

#if defined(PAE) || defined(PAE_TABLES)
pt_entry_t pg_nx;
static uma_zone_t pdptzone;
#endif

static SYSCTL_NODE(_vm, OID_AUTO, pmap, CTLFLAG_RD, 0, "VM/pmap parameters");

static int pat_works = 1;
SYSCTL_INT(_vm_pmap, OID_AUTO, pat_works, CTLFLAG_RD, &pat_works, 1,
    "Is page attribute table fully functional?");

static int pg_ps_enabled = 1;
SYSCTL_INT(_vm_pmap, OID_AUTO, pg_ps_enabled, CTLFLAG_RDTUN, &pg_ps_enabled, 0,
    "Are large page mappings enabled?");

#define	PAT_INDEX_SIZE	8
static int pat_index[PAT_INDEX_SIZE];	/* cache mode to PAT index conversion */

/*
 * pmap_mapdev support pre initialization (i.e. console)
 */
#define	PMAP_PREINIT_MAPPING_COUNT	8
static struct pmap_preinit_mapping {
	vm_paddr_t	pa;
	vm_offset_t	va;
	vm_size_t	sz;
	int		mode;
} pmap_preinit_mapping[PMAP_PREINIT_MAPPING_COUNT];
static int pmap_initialized;

static struct rwlock_padalign pvh_global_lock;

/*
 * Data for the pv entry allocation mechanism
 */
static TAILQ_HEAD(pch, pv_chunk) pv_chunks = TAILQ_HEAD_INITIALIZER(pv_chunks);
static int pv_entry_count = 0, pv_entry_max = 0, pv_entry_high_water = 0;
static struct md_page *pv_table;
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
pt_entry_t *CMAP3;
static pd_entry_t *KPTD;
caddr_t ptvmmap = 0;
caddr_t CADDR3;
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
static void	pmap_pv_demote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa);
static boolean_t pmap_pv_insert_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa);
static void	pmap_pv_promote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa);
static void	pmap_pvh_free(struct md_page *pvh, pmap_t pmap, vm_offset_t va);
static pv_entry_t pmap_pvh_remove(struct md_page *pvh, pmap_t pmap,
		    vm_offset_t va);
static int	pmap_pvh_wired_mappings(struct md_page *pvh, int count);

static boolean_t pmap_demote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va);
static boolean_t pmap_enter_pde(pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot);
static vm_page_t pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va,
    vm_page_t m, vm_prot_t prot, vm_page_t mpte);
static void pmap_flush_page(vm_page_t m);
static int pmap_insert_pt_page(pmap_t pmap, vm_page_t mpte);
static void pmap_fill_ptp(pt_entry_t *firstpte, pt_entry_t newpte);
static boolean_t pmap_is_modified_pvh(struct md_page *pvh);
static boolean_t pmap_is_referenced_pvh(struct md_page *pvh);
static void pmap_kenter_attr(vm_offset_t va, vm_paddr_t pa, int mode);
static void pmap_kenter_pde(vm_offset_t va, pd_entry_t newpde);
static vm_page_t pmap_lookup_pt_page(pmap_t pmap, vm_offset_t va);
static void pmap_pde_attr(pd_entry_t *pde, int cache_bits);
static void pmap_promote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va);
static boolean_t pmap_protect_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t sva,
    vm_prot_t prot);
static void pmap_pte_attr(pt_entry_t *pte, int cache_bits);
static void pmap_remove_pde(pmap_t pmap, pd_entry_t *pdq, vm_offset_t sva,
    struct spglist *free);
static int pmap_remove_pte(pmap_t pmap, pt_entry_t *ptq, vm_offset_t sva,
    struct spglist *free);
static void pmap_remove_pt_page(pmap_t pmap, vm_page_t mpte);
static void pmap_remove_page(struct pmap *pmap, vm_offset_t va,
    struct spglist *free);
static void pmap_remove_entry(struct pmap *pmap, vm_page_t m,
					vm_offset_t va);
static void pmap_insert_entry(pmap_t pmap, vm_offset_t va, vm_page_t m);
static boolean_t pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va,
    vm_page_t m);
static void pmap_update_pde(pmap_t pmap, vm_offset_t va, pd_entry_t *pde,
    pd_entry_t newpde);
static void pmap_update_pde_invalidate(vm_offset_t va, pd_entry_t newpde);

static vm_page_t pmap_allocpte(pmap_t pmap, vm_offset_t va, u_int flags);

static vm_page_t _pmap_allocpte(pmap_t pmap, u_int ptepindex, u_int flags);
static void _pmap_unwire_ptp(pmap_t pmap, vm_page_t m, struct spglist *free);
static pt_entry_t *pmap_pte_quick(pmap_t pmap, vm_offset_t va);
static void pmap_pte_release(pt_entry_t *pte);
static int pmap_unuse_pt(pmap_t, vm_offset_t, struct spglist *);
#if defined(PAE) || defined(PAE_TABLES)
static void *pmap_pdpt_allocf(uma_zone_t zone, vm_size_t bytes, uint8_t *flags,
    int wait);
#endif
static void pmap_set_pg(void);

static __inline void pagezero(void *page);

CTASSERT(1 << PDESHIFT == sizeof(pd_entry_t));
CTASSERT(1 << PTESHIFT == sizeof(pt_entry_t));

/*
 * If you get an error here, then you set KVA_PAGES wrong! See the
 * description of KVA_PAGES in sys/i386/include/pmap.h. It must be
 * multiple of 4 for a normal kernel, or a multiple of 8 for a PAE.
 */
CTASSERT(KERNBASE % (1 << 24) == 0);

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
	 * Add a physical memory segment (vm_phys_seg) corresponding to the
	 * preallocated kernel page table pages so that vm_page structures
	 * representing these pages will be created.  The vm_page structures
	 * are required for promotion of the corresponding kernel virtual
	 * addresses to superpage mappings.
	 */
	vm_phys_add_seg(KPTphys, KPTphys + ptoa(nkpt));

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
#if defined(PAE) || defined(PAE_TABLES)
	kernel_pmap->pm_pdpt = (pdpt_entry_t *) (KERNBASE + (u_int)IdlePDPT);
#endif
	CPU_FILL(&kernel_pmap->pm_active);	/* don't allow deactivation */
	TAILQ_INIT(&kernel_pmap->pm_pvchunk);

 	/*
	 * Initialize the global pv list lock.
	 */
	rw_init(&pvh_global_lock, "pmap pv global");

	LIST_INIT(&allpmaps);

	/*
	 * Request a spin mutex so that changes to allpmaps cannot be
	 * preempted by smp_rendezvous_cpus().  Otherwise,
	 * pmap_update_pde_kernel() could access allpmaps while it is
	 * being changed.
	 */
	mtx_init(&allpmaps_lock, "allpmaps", NULL, MTX_SPIN);
	mtx_lock_spin(&allpmaps_lock);
	LIST_INSERT_HEAD(&allpmaps, kernel_pmap, pm_list);
	mtx_unlock_spin(&allpmaps_lock);

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
	}
	SYSMAP(caddr_t, CMAP3, CADDR3, 1)

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
	 * KPTmap is used by pmap_kextract().
	 *
	 * KPTmap is first initialized by locore.  However, that initial
	 * KPTmap can only support NKPT page table pages.  Here, a larger
	 * KPTmap is created that can support KVA_PAGES page table pages.
	 */
	SYSMAP(pt_entry_t *, KPTD, KPTmap, KVA_PAGES)

	for (i = 0; i < NKPT; i++)
		KPTD[i] = (KPTphys + (i << PAGE_SHIFT)) | pgeflag | PG_RW | PG_V;

	/*
	 * Adjust the start of the KPTD and KPTmap so that the implementation
	 * of pmap_kextract() and pmap_growkernel() can be made simpler.
	 */
	KPTD -= KPTDI;
	KPTmap -= i386_btop(KPTDI << PDRSHIFT);

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
}

/*
 * Setup the PAT MSR.
 */
void
pmap_init_pat(void)
{
	int pat_table[PAT_INDEX_SIZE];
	uint64_t pat_msr;
	u_long cr0, cr4;
	int i;

	/* Set default PAT index table. */
	for (i = 0; i < PAT_INDEX_SIZE; i++)
		pat_table[i] = -1;
	pat_table[PAT_WRITE_BACK] = 0;
	pat_table[PAT_WRITE_THROUGH] = 1;
	pat_table[PAT_UNCACHEABLE] = 3;
	pat_table[PAT_WRITE_COMBINING] = 3;
	pat_table[PAT_WRITE_PROTECTED] = 3;
	pat_table[PAT_UNCACHED] = 3;

	/* Bail if this CPU doesn't implement PAT. */
	if ((cpu_feature & CPUID_PAT) == 0) {
		for (i = 0; i < PAT_INDEX_SIZE; i++)
			pat_index[i] = pat_table[i];
		pat_works = 0;
		return;
	}

	/*
	 * Due to some Intel errata, we can only safely use the lower 4
	 * PAT entries.
	 *
	 *   Intel Pentium III Processor Specification Update
	 * Errata E.27 (Upper Four PAT Entries Not Usable With Mode B
	 * or Mode C Paging)
	 *
	 *   Intel Pentium IV  Processor Specification Update
	 * Errata N46 (PAT Index MSB May Be Calculated Incorrectly)
	 */
	if (cpu_vendor_id == CPU_VENDOR_INTEL &&
	    !(CPUID_TO_FAMILY(cpu_id) == 6 && CPUID_TO_MODEL(cpu_id) >= 0xe))
		pat_works = 0;

	/* Initialize default PAT entries. */
	pat_msr = PAT_VALUE(0, PAT_WRITE_BACK) |
	    PAT_VALUE(1, PAT_WRITE_THROUGH) |
	    PAT_VALUE(2, PAT_UNCACHED) |
	    PAT_VALUE(3, PAT_UNCACHEABLE) |
	    PAT_VALUE(4, PAT_WRITE_BACK) |
	    PAT_VALUE(5, PAT_WRITE_THROUGH) |
	    PAT_VALUE(6, PAT_UNCACHED) |
	    PAT_VALUE(7, PAT_UNCACHEABLE);

	if (pat_works) {
		/*
		 * Leave the indices 0-3 at the default of WB, WT, UC-, and UC.
		 * Program 5 and 6 as WP and WC.
		 * Leave 4 and 7 as WB and UC.
		 */
		pat_msr &= ~(PAT_MASK(5) | PAT_MASK(6));
		pat_msr |= PAT_VALUE(5, PAT_WRITE_PROTECTED) |
		    PAT_VALUE(6, PAT_WRITE_COMBINING);
		pat_table[PAT_UNCACHED] = 2;
		pat_table[PAT_WRITE_PROTECTED] = 5;
		pat_table[PAT_WRITE_COMBINING] = 6;
	} else {
		/*
		 * Just replace PAT Index 2 with WC instead of UC-.
		 */
		pat_msr &= ~PAT_MASK(2);
		pat_msr |= PAT_VALUE(2, PAT_WRITE_COMBINING);
		pat_table[PAT_WRITE_COMBINING] = 2;
	}

	/* Disable PGE. */
	cr4 = rcr4();
	load_cr4(cr4 & ~CR4_PGE);

	/* Disable caches (CD = 1, NW = 0). */
	cr0 = rcr0();
	load_cr0((cr0 & ~CR0_NW) | CR0_CD);

	/* Flushes caches and TLBs. */
	wbinvd();
	invltlb();

	/* Update PAT and index table. */
	wrmsr(MSR_PAT, pat_msr);
	for (i = 0; i < PAT_INDEX_SIZE; i++)
		pat_index[i] = pat_table[i];

	/* Flush caches and TLBs again. */
	wbinvd();
	invltlb();

	/* Restore caches and PGE. */
	load_cr0(cr0);
	load_cr4(cr4);
}

/*
 * Set PG_G on kernel pages.  Only the BSP calls this when SMP is turned on.
 */
static void
pmap_set_pg(void)
{
	pt_entry_t *pte;
	vm_offset_t va, endva;

	if (pgeflag == 0)
		return;

	endva = KERNBASE + KERNend;

	if (pseflag) {
		va = KERNBASE + KERNLOAD;
		while (va  < endva) {
			pdir_pde(PTD, va) |= pgeflag;
			invltlb();	/* Play it safe, invltlb() every time */
			va += NBPDR;
		}
	} else {
		va = (vm_offset_t)btext;
		while (va < endva) {
			pte = vtopte(va);
			if (*pte)
				*pte |= pgeflag;
			invltlb();	/* Play it safe, invltlb() every time */
			va += PAGE_SIZE;
		}
	}
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

#if defined(PAE) || defined(PAE_TABLES)
static void *
pmap_pdpt_allocf(uma_zone_t zone, vm_size_t bytes, uint8_t *flags, int wait)
{

	/* Inform UMA that this allocator uses kernel_map/object. */
	*flags = UMA_SLAB_KERNEL;
	return ((void *)kmem_alloc_contig(kernel_arena, bytes, wait, 0x0ULL,
	    0xffffffffULL, 1, 0, VM_MEMATTR_DEFAULT));
}
#endif

/*
 * Abuse the pte nodes for unmapped kva to thread a kva freelist through.
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
static vm_offset_t
pmap_ptelist_alloc(vm_offset_t *head)
{
	pt_entry_t *pte;
	vm_offset_t va;

	va = *head;
	if (va == 0)
		panic("pmap_ptelist_alloc: exhausted ptelist KVA");
	pte = vtopte(va);
	*head = *pte;
	if (*head & PG_V)
		panic("pmap_ptelist_alloc: va with PG_V set!");
	*pte = 0;
	return (va);
}

static void
pmap_ptelist_free(vm_offset_t *head, vm_offset_t va)
{
	pt_entry_t *pte;

	if (va & PG_V)
		panic("pmap_ptelist_free: freeing va with PG_V set!");
	pte = vtopte(va);
	*pte = *head;		/* virtual! PG_V is 0 though */
	*head = va;
}

static void
pmap_ptelist_init(vm_offset_t *head, void *base, int npages)
{
	int i;
	vm_offset_t va;

	*head = 0;
	for (i = npages - 1; i >= 0; i--) {
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
	struct pmap_preinit_mapping *ppim;
	vm_page_t mpte;
	vm_size_t s;
	int i, pv_npg;

	/*
	 * Initialize the vm page array entries for the kernel pmap's
	 * page table pages.
	 */ 
	for (i = 0; i < NKPT; i++) {
		mpte = PHYS_TO_VM_PAGE(KPTphys + (i << PAGE_SHIFT));
		KASSERT(mpte >= vm_page_array &&
		    mpte < &vm_page_array[vm_page_array_size],
		    ("pmap_init: page table page is out of range"));
		mpte->pindex = i + KPTDI;
		mpte->phys_addr = KPTphys + (i << PAGE_SHIFT);
	}

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

	/*
	 * If the kernel is running on a virtual machine, then it must assume
	 * that MCA is enabled by the hypervisor.  Moreover, the kernel must
	 * be prepared for the hypervisor changing the vendor and family that
	 * are reported by CPUID.  Consequently, the workaround for AMD Family
	 * 10h Erratum 383 is enabled if the processor's feature set does not
	 * include at least one feature that is only supported by older Intel
	 * or newer AMD processors.
	 */
	if (vm_guest == VM_GUEST_VM && (cpu_feature & CPUID_SS) == 0 &&
	    (cpu_feature2 & (CPUID2_SSSE3 | CPUID2_SSE41 | CPUID2_AESNI |
	    CPUID2_AVX | CPUID2_XSAVE)) == 0 && (amd_feature2 & (AMDID2_XOP |
	    AMDID2_FMA4)) == 0)
		workaround_erratum383 = 1;

	/*
	 * Are large page mappings supported and enabled?
	 */
	TUNABLE_INT_FETCH("vm.pmap.pg_ps_enabled", &pg_ps_enabled);
	if (pseflag == 0)
		pg_ps_enabled = 0;
	else if (pg_ps_enabled) {
		KASSERT(MAXPAGESIZES > 1 && pagesizes[1] == 0,
		    ("pmap_init: can't assign to pagesizes[1]"));
		pagesizes[1] = NBPDR;
	}

	/*
	 * Calculate the size of the pv head table for superpages.
	 * Handle the possibility that "vm_phys_segs[...].end" is zero.
	 */
	pv_npg = trunc_4mpage(vm_phys_segs[vm_phys_nsegs - 1].end -
	    PAGE_SIZE) / NBPDR + 1;

	/*
	 * Allocate memory for the pv head table for superpages.
	 */
	s = (vm_size_t)(pv_npg * sizeof(struct md_page));
	s = round_page(s);
	pv_table = (struct md_page *)kmem_malloc(kernel_arena, s,
	    M_WAITOK | M_ZERO);
	for (i = 0; i < pv_npg; i++)
		TAILQ_INIT(&pv_table[i].pv_list);

	pv_maxchunks = MAX(pv_entry_max / _NPCPV, maxproc);
	pv_chunkbase = (struct pv_chunk *)kva_alloc(PAGE_SIZE * pv_maxchunks);
	if (pv_chunkbase == NULL)
		panic("pmap_init: not enough kvm for pv chunks");
	pmap_ptelist_init(&pv_vafree, pv_chunkbase, pv_maxchunks);
#if defined(PAE) || defined(PAE_TABLES)
	pdptzone = uma_zcreate("PDPT", NPGPTD * sizeof(pdpt_entry_t), NULL,
	    NULL, NULL, NULL, (NPGPTD * sizeof(pdpt_entry_t)) - 1,
	    UMA_ZONE_VM | UMA_ZONE_NOFREE);
	uma_zone_set_allocf(pdptzone, pmap_pdpt_allocf);
#endif

	pmap_initialized = 1;
	if (!bootverbose)
		return;
	for (i = 0; i < PMAP_PREINIT_MAPPING_COUNT; i++) {
		ppim = pmap_preinit_mapping + i;
		if (ppim->va == 0)
			continue;
		printf("PPIM %u: PA=%#jx, VA=%#x, size=%#x, mode=%#x\n", i,
		    (uintmax_t)ppim->pa, ppim->va, ppim->sz, ppim->mode);
	}
}


SYSCTL_INT(_vm_pmap, OID_AUTO, pv_entry_max, CTLFLAG_RD, &pv_entry_max, 0,
	"Max number of PV entries");
SYSCTL_INT(_vm_pmap, OID_AUTO, shpgperproc, CTLFLAG_RD, &shpgperproc, 0,
	"Page share factor per proc");

static SYSCTL_NODE(_vm_pmap, OID_AUTO, pde, CTLFLAG_RD, 0,
    "2/4MB page mapping counters");

static u_long pmap_pde_demotions;
SYSCTL_ULONG(_vm_pmap_pde, OID_AUTO, demotions, CTLFLAG_RD,
    &pmap_pde_demotions, 0, "2/4MB page demotions");

static u_long pmap_pde_mappings;
SYSCTL_ULONG(_vm_pmap_pde, OID_AUTO, mappings, CTLFLAG_RD,
    &pmap_pde_mappings, 0, "2/4MB page mappings");

static u_long pmap_pde_p_failures;
SYSCTL_ULONG(_vm_pmap_pde, OID_AUTO, p_failures, CTLFLAG_RD,
    &pmap_pde_p_failures, 0, "2/4MB page promotion failures");

static u_long pmap_pde_promotions;
SYSCTL_ULONG(_vm_pmap_pde, OID_AUTO, promotions, CTLFLAG_RD,
    &pmap_pde_promotions, 0, "2/4MB page promotions");

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
	int cache_bits, pat_flag, pat_idx;

	if (mode < 0 || mode >= PAT_INDEX_SIZE || pat_index[mode] < 0)
		panic("Unknown caching mode %d\n", mode);

	/* The PAT bit is different for PTE's and PDE's. */
	pat_flag = is_pde ? PG_PDE_PAT : PG_PTE_PAT;

	/* Map the caching mode to a PAT index. */
	pat_idx = pat_index[mode];

	/* Map the 3-bit index value into the PAT, PCD, and PWT bits. */
	cache_bits = 0;
	if (pat_idx & 0x4)
		cache_bits |= pat_flag;
	if (pat_idx & 0x2)
		cache_bits |= PG_NC_PCD;
	if (pat_idx & 0x1)
		cache_bits |= PG_NC_PWT;
	return (cache_bits);
}

/*
 * The caller is responsible for maintaining TLB consistency.
 */
static void
pmap_kenter_pde(vm_offset_t va, pd_entry_t newpde)
{
	pd_entry_t *pde;
	pmap_t pmap;
	boolean_t PTD_updated;

	PTD_updated = FALSE;
	mtx_lock_spin(&allpmaps_lock);
	LIST_FOREACH(pmap, &allpmaps, pm_list) {
		if ((pmap->pm_pdir[PTDPTDI] & PG_FRAME) == (PTDpde[0] &
		    PG_FRAME))
			PTD_updated = TRUE;
		pde = pmap_pde(pmap, va);
		pde_store(pde, newpde);
	}
	mtx_unlock_spin(&allpmaps_lock);
	KASSERT(PTD_updated,
	    ("pmap_kenter_pde: current page table is not in allpmaps"));
}

/*
 * After changing the page size for the specified virtual address in the page
 * table, flush the corresponding entries from the processor's TLB.  Only the
 * calling processor's TLB is affected.
 *
 * The calling thread must be pinned to a processor.
 */
static void
pmap_update_pde_invalidate(vm_offset_t va, pd_entry_t newpde)
{
	u_long cr4;

	if ((newpde & PG_PS) == 0)
		/* Demotion: flush a specific 2MB page mapping. */
		invlpg(va);
	else if ((newpde & PG_G) == 0)
		/*
		 * Promotion: flush every 4KB page mapping from the TLB
		 * because there are too many to flush individually.
		 */
		invltlb();
	else {
		/*
		 * Promotion: flush every 4KB page mapping from the TLB,
		 * including any global (PG_G) mappings.
		 */
		cr4 = rcr4();
		load_cr4(cr4 & ~CR4_PGE);
		/*
		 * Although preemption at this point could be detrimental to
		 * performance, it would not lead to an error.  PG_G is simply
		 * ignored if CR4.PGE is clear.  Moreover, in case this block
		 * is re-entered, the load_cr4() either above or below will
		 * modify CR4.PGE flushing the TLB.
		 */
		load_cr4(cr4 | CR4_PGE);
	}
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
}

void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	cpuset_t other_cpus;
	vm_offset_t addr;
	u_int cpuid;

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
}

void
pmap_invalidate_all(pmap_t pmap)
{
	cpuset_t other_cpus;
	u_int cpuid;

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

struct pde_action {
	cpuset_t invalidate;	/* processors that invalidate their TLB */
	vm_offset_t va;
	pd_entry_t *pde;
	pd_entry_t newpde;
	u_int store;		/* processor that updates the PDE */
};

static void
pmap_update_pde_kernel(void *arg)
{
	struct pde_action *act = arg;
	pd_entry_t *pde;
	pmap_t pmap;

	if (act->store == PCPU_GET(cpuid)) {

		/*
		 * Elsewhere, this operation requires allpmaps_lock for
		 * synchronization.  Here, it does not because it is being
		 * performed in the context of an all_cpus rendezvous.
		 */
		LIST_FOREACH(pmap, &allpmaps, pm_list) {
			pde = pmap_pde(pmap, act->va);
			pde_store(pde, act->newpde);
		}
	}
}

static void
pmap_update_pde_user(void *arg)
{
	struct pde_action *act = arg;

	if (act->store == PCPU_GET(cpuid))
		pde_store(act->pde, act->newpde);
}

static void
pmap_update_pde_teardown(void *arg)
{
	struct pde_action *act = arg;

	if (CPU_ISSET(PCPU_GET(cpuid), &act->invalidate))
		pmap_update_pde_invalidate(act->va, act->newpde);
}

/*
 * Change the page size for the specified virtual address in a way that
 * prevents any possibility of the TLB ever having two entries that map the
 * same virtual address using different page sizes.  This is the recommended
 * workaround for Erratum 383 on AMD Family 10h processors.  It prevents a
 * machine check exception for a TLB state that is improperly diagnosed as a
 * hardware error.
 */
static void
pmap_update_pde(pmap_t pmap, vm_offset_t va, pd_entry_t *pde, pd_entry_t newpde)
{
	struct pde_action act;
	cpuset_t active, other_cpus;
	u_int cpuid;

	sched_pin();
	cpuid = PCPU_GET(cpuid);
	other_cpus = all_cpus;
	CPU_CLR(cpuid, &other_cpus);
	if (pmap == kernel_pmap)
		active = all_cpus;
	else
		active = pmap->pm_active;
	if (CPU_OVERLAP(&active, &other_cpus)) {
		act.store = cpuid;
		act.invalidate = active;
		act.va = va;
		act.pde = pde;
		act.newpde = newpde;
		CPU_SET(cpuid, &active);
		smp_rendezvous_cpus(active,
		    smp_no_rendevous_barrier, pmap == kernel_pmap ?
		    pmap_update_pde_kernel : pmap_update_pde_user,
		    pmap_update_pde_teardown, &act);
	} else {
		if (pmap == kernel_pmap)
			pmap_kenter_pde(va, newpde);
		else
			pde_store(pde, newpde);
		if (CPU_ISSET(cpuid, &active))
			pmap_update_pde_invalidate(va, newpde);
	}
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

	if (pmap == kernel_pmap || !CPU_EMPTY(&pmap->pm_active))
		invlpg(va);
}

PMAP_INLINE void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	vm_offset_t addr;

	if (pmap == kernel_pmap || !CPU_EMPTY(&pmap->pm_active))
		for (addr = sva; addr < eva; addr += PAGE_SIZE)
			invlpg(addr);
}

PMAP_INLINE void
pmap_invalidate_all(pmap_t pmap)
{

	if (pmap == kernel_pmap || !CPU_EMPTY(&pmap->pm_active))
		invltlb();
}

PMAP_INLINE void
pmap_invalidate_cache(void)
{

	wbinvd();
}

static void
pmap_update_pde(pmap_t pmap, vm_offset_t va, pd_entry_t *pde, pd_entry_t newpde)
{

	if (pmap == kernel_pmap)
		pmap_kenter_pde(va, newpde);
	else
		pde_store(pde, newpde);
	if (pmap == kernel_pmap || !CPU_EMPTY(&pmap->pm_active))
		pmap_update_pde_invalidate(va, newpde);
}
#endif /* !SMP */

#define	PMAP_CLFLUSH_THRESHOLD	(2 * 1024 * 1024)

void
pmap_invalidate_cache_range(vm_offset_t sva, vm_offset_t eva, boolean_t force)
{

	if (force) {
		sva &= ~(vm_offset_t)cpu_clflush_line_size;
	} else {
		KASSERT((sva & PAGE_MASK) == 0,
		    ("pmap_invalidate_cache_range: sva not page-aligned"));
		KASSERT((eva & PAGE_MASK) == 0,
		    ("pmap_invalidate_cache_range: eva not page-aligned"));
	}

	if ((cpu_feature & CPUID_SS) != 0 && !force)
		; /* If "Self Snoop" is supported and allowed, do nothing. */
	else if ((cpu_feature & CPUID_CLFSH) != 0 &&
	    eva - sva < PMAP_CLFLUSH_THRESHOLD) {

#ifdef DEV_APIC
		/*
		 * XXX: Some CPUs fault, hang, or trash the local APIC
		 * registers if we use CLFLUSH on the local APIC
		 * range.  The local APIC is always uncached, so we
		 * don't need to flush for that range anyway.
		 */
		if (pmap_kextract(sva) == lapic_paddr)
			return;
#endif
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
			*PMAP2 = newpf | PG_RW | PG_V | PG_A | PG_M;
			pmap_invalidate_page(kernel_pmap, (vm_offset_t)PADDR2);
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

	if ((pt_entry_t *)((vm_offset_t)pte & ~PAGE_MASK) == PADDR2)
		mtx_unlock(&PMAP2mutex);
}

/*
 * NB:  The sequence of updating a page table followed by accesses to the
 * corresponding pages is subject to the situation described in the "AMD64
 * Architecture Programmer's Manual Volume 2: System Programming" rev. 3.23,
 * "7.3.1 Special Coherency Considerations".  Therefore, issuing the INVLPG
 * right after modifying the PTE bits is crucial.
 */
static __inline void
invlcaddr(void *caddr)
{

	invlpg((u_int)caddr);
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
			*PMAP1 = newpf | PG_RW | PG_V | PG_A | PG_M;
#ifdef SMP
			PMAP1cpu = PCPU_GET(cpuid);
#endif
			invlcaddr(PADDR1);
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

	rtval = 0;
	PMAP_LOCK(pmap);
	pde = pmap->pm_pdir[va >> PDRSHIFT];
	if (pde != 0) {
		if ((pde & PG_PS) != 0)
			rtval = (pde & PG_PS_FRAME) | (va & PDRMASK);
		else {
			pte = pmap_pte(pmap, va);
			rtval = (*pte & PG_FRAME) | (va & PAGE_MASK);
			pmap_pte_release(pte);
		}
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
	pde = *pmap_pde(pmap, va);
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
			pte = *ptep;
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
PMAP_INLINE void 
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{
	pt_entry_t *pte;

	pte = vtopte(va);
	pte_store(pte, pa | PG_RW | PG_V | pgeflag);
}

static __inline void
pmap_kenter_attr(vm_offset_t va, vm_paddr_t pa, int mode)
{
	pt_entry_t *pte;

	pte = vtopte(va);
	pte_store(pte, pa | PG_RW | PG_V | pgeflag | pmap_cache_bits(mode, 0));
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
	pte_clear(pte);
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
	vm_paddr_t superpage_offset;
	pd_entry_t newpde;

	va = *virt;
	/*
	 * Does the physical address range's size and alignment permit at
	 * least one superpage mapping to be created?
	 */ 
	superpage_offset = start & PDRMASK;
	if ((end - start) - ((NBPDR - superpage_offset) & PDRMASK) >= NBPDR) {
		/*
		 * Increase the starting virtual address so that its alignment
		 * does not preclude the use of superpage mappings.
		 */
		if ((va & PDRMASK) < superpage_offset)
			va = (va & ~PDRMASK) + superpage_offset;
		else if ((va & PDRMASK) > superpage_offset)
			va = ((va + PDRMASK) & ~PDRMASK) + superpage_offset;
	}
	sva = va;
	while (start < end) {
		if ((start & PDRMASK) == 0 && end - start >= NBPDR &&
		    pseflag) {
			KASSERT((va & PDRMASK) == 0,
			    ("pmap_map: misaligned va %#x", va));
			newpde = start | PG_PS | pgeflag | PG_RW | PG_V;
			pmap_kenter_pde(va, newpde);
			va += NBPDR;
			start += NBPDR;
		} else {
			pmap_kenter(va, start);
			va += PAGE_SIZE;
			start += PAGE_SIZE;
		}
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
	pt_entry_t *endpte, oldpte, pa, *pte;
	vm_page_t m;

	oldpte = 0;
	pte = vtopte(sva);
	endpte = pte + count;
	while (pte < endpte) {
		m = *ma++;
		pa = VM_PAGE_TO_PHYS(m) | pmap_cache_bits(m->md.pat_mode, 0);
		if ((*pte & (PG_FRAME | PG_PTE_CACHE)) != pa) {
			oldpte |= *pte;
			pte_store(pte, pa | pgeflag | PG_RW | PG_V);
		}
		pte++;
	}
	if (__predict_false((oldpte & PG_V) != 0))
		pmap_invalidate_range(kernel_pmap, sva, sva + count *
		    PAGE_SIZE);
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

	va = sva;
	while (count-- > 0) {
		pmap_kremove(va);
		va += PAGE_SIZE;
	}
	pmap_invalidate_range(kernel_pmap, sva, va);
}

/***************************************************
 * Page table page management routines.....
 ***************************************************/
static __inline void
pmap_free_zero_pages(struct spglist *free)
{
	vm_page_t m;

	while ((m = SLIST_FIRST(free)) != NULL) {
		SLIST_REMOVE_HEAD(free, plinks.s.ss);
		/* Preserve the page's PG_ZERO setting. */
		vm_page_free_toq(m);
	}
}

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
	return (vm_radix_insert(&pmap->pm_root, mpte));
}

/*
 * Looks for a page table page mapping the specified virtual address in the
 * specified pmap's collection of idle page table pages.  Returns NULL if there
 * is no page table page corresponding to the specified virtual address.
 */
static __inline vm_page_t
pmap_lookup_pt_page(pmap_t pmap, vm_offset_t va)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	return (vm_radix_lookup(&pmap->pm_root, va >> PDRSHIFT));
}

/*
 * Removes the specified page table page from the specified pmap's collection
 * of idle page table pages.  The specified page table page must be a member of
 * the pmap's collection.
 */
static __inline void
pmap_remove_pt_page(pmap_t pmap, vm_page_t mpte)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	vm_radix_remove(&pmap->pm_root, mpte->pindex);
}

/*
 * Decrements a page table page's wire count, which is used to record the
 * number of valid page table entries within the page.  If the wire count
 * drops to zero, then the page table page is unmapped.  Returns TRUE if the
 * page table page was unmapped and FALSE otherwise.
 */
static inline boolean_t
pmap_unwire_ptp(pmap_t pmap, vm_page_t m, struct spglist *free)
{

	--m->wire_count;
	if (m->wire_count == 0) {
		_pmap_unwire_ptp(pmap, m, free);
		return (TRUE);
	} else
		return (FALSE);
}

static void
_pmap_unwire_ptp(pmap_t pmap, vm_page_t m, struct spglist *free)
{
	vm_offset_t pteva;

	/*
	 * unmap the page table page
	 */
	pmap->pm_pdir[m->pindex] = 0;
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
	pmap_add_delayed_free_list(m, free, TRUE);
}

/*
 * After removing a page table entry, this routine is used to
 * conditionally free the page, and manage the hold/wire counts.
 */
static int
pmap_unuse_pt(pmap_t pmap, vm_offset_t va, struct spglist *free)
{
	pd_entry_t ptepde;
	vm_page_t mpte;

	if (va >= VM_MAXUSER_ADDRESS)
		return (0);
	ptepde = *pmap_pde(pmap, va);
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
#if defined(PAE) || defined(PAE_TABLES)
	pmap->pm_pdpt = (pdpt_entry_t *)(KERNBASE + (vm_offset_t)IdlePDPT);
#endif
	pmap->pm_root.rt_root = 0;
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
	vm_page_t m, ptdpg[NPGPTD];
	vm_paddr_t pa;
	int i;

	/*
	 * No need to allocate page table space yet but we do need a valid
	 * page directory table.
	 */
	if (pmap->pm_pdir == NULL) {
		pmap->pm_pdir = (pd_entry_t *)kva_alloc(NBPTD);
		if (pmap->pm_pdir == NULL)
			return (0);
#if defined(PAE) || defined(PAE_TABLES)
		pmap->pm_pdpt = uma_zalloc(pdptzone, M_WAITOK | M_ZERO);
		KASSERT(((vm_offset_t)pmap->pm_pdpt &
		    ((NPGPTD * sizeof(pdpt_entry_t)) - 1)) == 0,
		    ("pmap_pinit: pdpt misaligned"));
		KASSERT(pmap_kextract((vm_offset_t)pmap->pm_pdpt) < (4ULL<<30),
		    ("pmap_pinit: pdpt above 4g"));
#endif
		pmap->pm_root.rt_root = 0;
	}
	KASSERT(vm_radix_is_empty(&pmap->pm_root),
	    ("pmap_pinit: pmap has reserved page table page(s)"));

	/*
	 * allocate the page directory page(s)
	 */
	for (i = 0; i < NPGPTD;) {
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

	/* install self-referential address mapping entry(s) */
	for (i = 0; i < NPGPTD; i++) {
		pa = VM_PAGE_TO_PHYS(ptdpg[i]);
		pmap->pm_pdir[PTDPTDI + i] = pa | PG_V | PG_RW | PG_A | PG_M;
#if defined(PAE) || defined(PAE_TABLES)
		pmap->pm_pdpt[i] = pa | PG_V;
#endif
	}

	CPU_ZERO(&pmap->pm_active);
	TAILQ_INIT(&pmap->pm_pvchunk);
	bzero(&pmap->pm_stats, sizeof pmap->pm_stats);

	return (1);
}

/*
 * this routine is called if the page table page is not
 * mapped correctly.
 */
static vm_page_t
_pmap_allocpte(pmap_t pmap, u_int ptepindex, u_int flags)
{
	vm_paddr_t ptepa;
	vm_page_t m;

	/*
	 * Allocate a page table page.
	 */
	if ((m = vm_page_alloc(NULL, ptepindex, VM_ALLOC_NOOBJ |
	    VM_ALLOC_WIRED | VM_ALLOC_ZERO)) == NULL) {
		if ((flags & PMAP_ENTER_NOSLEEP) == 0) {
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

	ptepa = VM_PAGE_TO_PHYS(m);
	pmap->pm_pdir[ptepindex] =
		(pd_entry_t) (ptepa | PG_U | PG_RW | PG_V | PG_A | PG_M);

	return (m);
}

static vm_page_t
pmap_allocpte(pmap_t pmap, vm_offset_t va, u_int flags)
{
	u_int ptepindex;
	pd_entry_t ptepa;
	vm_page_t m;

	/*
	 * Calculate pagetable page index
	 */
	ptepindex = va >> PDRSHIFT;
retry:
	/*
	 * Get the page directory entry
	 */
	ptepa = pmap->pm_pdir[ptepindex];

	/*
	 * This supports switching from a 4MB page to a
	 * normal 4K page.
	 */
	if (ptepa & PG_PS) {
		(void)pmap_demote_pde(pmap, &pmap->pm_pdir[ptepindex], va);
		ptepa = pmap->pm_pdir[ptepindex];
	}

	/*
	 * If the page table page is mapped, we just increment the
	 * hold count, and activate it.
	 */
	if (ptepa) {
		m = PHYS_TO_VM_PAGE(ptepa & PG_FRAME);
		m->wire_count++;
	} else {
		/*
		 * Here if the pte page isn't mapped, or if it has
		 * been deallocated. 
		 */
		m = _pmap_allocpte(pmap, ptepindex, flags);
		if (m == NULL && (flags & PMAP_ENTER_NOSLEEP) == 0)
			goto retry;
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
		load_cr3(curpcb->pcb_cr3);
	CPU_CLR_ATOMIC(PCPU_GET(cpuid), lazymask);
	atomic_store_rel_int(&lazywait, 1);
}

static void
pmap_lazyfix_self(u_int cpuid)
{

	if (rcr3() == lazyptd)
		load_cr3(curpcb->pcb_cr3);
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
#if defined(PAE) || defined(PAE_TABLES)
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
		load_cr3(curpcb->pcb_cr3);
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
	vm_page_t m, ptdpg[NPGPTD];
	int i;

	KASSERT(pmap->pm_stats.resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->pm_stats.resident_count));
	KASSERT(vm_radix_is_empty(&pmap->pm_root),
	    ("pmap_release: pmap has reserved page table page(s)"));

	pmap_lazyfix(pmap);
	mtx_lock_spin(&allpmaps_lock);
	LIST_REMOVE(pmap, pm_list);
	mtx_unlock_spin(&allpmaps_lock);

	for (i = 0; i < NPGPTD; i++)
		ptdpg[i] = PHYS_TO_VM_PAGE(pmap->pm_pdir[PTDPTDI + i] &
		    PG_FRAME);

	bzero(pmap->pm_pdir + PTDPTDI, (nkpt + NPGPTD) *
	    sizeof(*pmap->pm_pdir));

	pmap_qremove((vm_offset_t)pmap->pm_pdir, NPGPTD);

	for (i = 0; i < NPGPTD; i++) {
		m = ptdpg[i];
#if defined(PAE) || defined(PAE_TABLES)
		KASSERT(VM_PAGE_TO_PHYS(m) == (pmap->pm_pdpt[i] & PG_FRAME),
		    ("pmap_release: got wrong ptd page"));
#endif
		m->wire_count--;
		atomic_subtract_int(&cnt.v_wire_count, 1);
		vm_page_free_zero(m);
	}
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
	vm_paddr_t ptppaddr;
	vm_page_t nkpg;
	pd_entry_t newpdir;

	mtx_assert(&kernel_map->system_mtx, MA_OWNED);
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
		pdir_pde(KPTD, kernel_vm_end) = pgeflag | newpdir;

		pmap_kenter_pde(kernel_vm_end, newpdir);
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
	struct md_page *pvh;
	pd_entry_t *pde;
	pmap_t pmap;
	pt_entry_t *pte, tpte;
	pv_entry_t pv;
	vm_offset_t va;
	vm_page_t m, m_pc;
	struct spglist free;
	uint32_t inuse;
	int bit, field, freed;

	PMAP_LOCK_ASSERT(locked_pmap, MA_OWNED);
	pmap = NULL;
	m_pc = NULL;
	SLIST_INIT(&free);
	TAILQ_INIT(&newtail);
	while ((pc = TAILQ_FIRST(&pv_chunks)) != NULL && (pv_vafree == 0 ||
	    SLIST_EMPTY(&free))) {
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
				pde = pmap_pde(pmap, va);
				if ((*pde & PG_PS) != 0)
					continue;
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
				if (TAILQ_EMPTY(&m->md.pv_list) &&
				    (m->flags & PG_FICTITIOUS) == 0) {
					pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
					if (TAILQ_EMPTY(&pvh->pv_list)) {
						vm_page_aflag_clear(m,
						    PGA_WRITEABLE);
					}
				}
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
	if (m_pc == NULL && pv_vafree != 0 && SLIST_EMPTY(&free)) {
		m_pc = SLIST_FIRST(&free);
		SLIST_REMOVE_HEAD(&free, plinks.s.ss);
		/* Recycle a freed page table page. */
		m_pc->wire_count = 1;
		atomic_add_int(&cnt.v_wire_count, 1);
	}
	pmap_free_zero_pages(&free);
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

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
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
	 * Access to the ptelist "pv_vafree" is synchronized by the pvh
	 * global lock.  If "pv_vafree" is currently non-empty, it will
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
pmap_pv_demote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa)
{
	struct md_page *pvh;
	pv_entry_t pv;
	vm_offset_t va_last;
	vm_page_t m;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	KASSERT((pa & PDRMASK) == 0,
	    ("pmap_pv_demote_pde: pa is not 4mpage aligned"));

	/*
	 * Transfer the 4mpage's pv entry for this mapping to the first
	 * page's pv list.
	 */
	pvh = pa_to_pvh(pa);
	va = trunc_4mpage(va);
	pv = pmap_pvh_remove(pvh, pmap, va);
	KASSERT(pv != NULL, ("pmap_pv_demote_pde: pv not found"));
	m = PHYS_TO_VM_PAGE(pa);
	TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
	/* Instantiate the remaining NPTEPG - 1 pv entries. */
	va_last = va + NBPDR - PAGE_SIZE;
	do {
		m++;
		KASSERT((m->oflags & VPO_UNMANAGED) == 0,
		    ("pmap_pv_demote_pde: page %p is not managed", m));
		va += PAGE_SIZE;
		pmap_insert_entry(pmap, va, m);
	} while (va < va_last);
}

static void
pmap_pv_promote_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa)
{
	struct md_page *pvh;
	pv_entry_t pv;
	vm_offset_t va_last;
	vm_page_t m;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	KASSERT((pa & PDRMASK) == 0,
	    ("pmap_pv_promote_pde: pa is not 4mpage aligned"));

	/*
	 * Transfer the first page's pv entry for this mapping to the
	 * 4mpage's pv list.  Aside from avoiding the cost of a call
	 * to get_pv_entry(), a transfer avoids the possibility that
	 * get_pv_entry() calls pmap_collect() and that pmap_collect()
	 * removes one of the mappings that is being promoted.
	 */
	m = PHYS_TO_VM_PAGE(pa);
	va = trunc_4mpage(va);
	pv = pmap_pvh_remove(&m->md, pmap, va);
	KASSERT(pv != NULL, ("pmap_pv_promote_pde: pv not found"));
	pvh = pa_to_pvh(pa);
	TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_next);
	/* Free the remaining NPTEPG - 1 pv entries. */
	va_last = va + NBPDR - PAGE_SIZE;
	do {
		m++;
		va += PAGE_SIZE;
		pmap_pvh_free(&m->md, pmap, va);
	} while (va < va_last);
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
	struct md_page *pvh;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	pmap_pvh_free(&m->md, pmap, va);
	if (TAILQ_EMPTY(&m->md.pv_list) && (m->flags & PG_FICTITIOUS) == 0) {
		pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
		if (TAILQ_EMPTY(&pvh->pv_list))
			vm_page_aflag_clear(m, PGA_WRITEABLE);
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

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	pv = get_pv_entry(pmap, FALSE);
	pv->pv_va = va;
	TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
}

/*
 * Conditionally create a pv entry.
 */
static boolean_t
pmap_try_insert_pv_entry(pmap_t pmap, vm_offset_t va, vm_page_t m)
{
	pv_entry_t pv;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if (pv_entry_count < pv_entry_high_water && 
	    (pv = get_pv_entry(pmap, TRUE)) != NULL) {
		pv->pv_va = va;
		TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
		return (TRUE);
	} else
		return (FALSE);
}

/*
 * Create the pv entries for each of the pages within a superpage.
 */
static boolean_t
pmap_pv_insert_pde(pmap_t pmap, vm_offset_t va, vm_paddr_t pa)
{
	struct md_page *pvh;
	pv_entry_t pv;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	if (pv_entry_count < pv_entry_high_water && 
	    (pv = get_pv_entry(pmap, TRUE)) != NULL) {
		pv->pv_va = va;
		pvh = pa_to_pvh(pa);
		TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_next);
		return (TRUE);
	} else
		return (FALSE);
}

/*
 * Fills a page table page with mappings to consecutive physical pages.
 */
static void
pmap_fill_ptp(pt_entry_t *firstpte, pt_entry_t newpte)
{
	pt_entry_t *pte;

	for (pte = firstpte; pte < firstpte + NPTEPG; pte++) {
		*pte = newpte;	
		newpte += PAGE_SIZE;
	}
}

/*
 * Tries to demote a 2- or 4MB page mapping.  If demotion fails, the
 * 2- or 4MB page mapping is invalidated.
 */
static boolean_t
pmap_demote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va)
{
	pd_entry_t newpde, oldpde;
	pt_entry_t *firstpte, newpte;
	vm_paddr_t mptepa;
	vm_page_t mpte;
	struct spglist free;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	oldpde = *pde;
	KASSERT((oldpde & (PG_PS | PG_V)) == (PG_PS | PG_V),
	    ("pmap_demote_pde: oldpde is missing PG_PS and/or PG_V"));
	if ((oldpde & PG_A) != 0 && (mpte = pmap_lookup_pt_page(pmap, va)) !=
	    NULL)
		pmap_remove_pt_page(pmap, mpte);
	else {
		KASSERT((oldpde & PG_W) == 0,
		    ("pmap_demote_pde: page table page for a wired mapping"
		    " is missing"));

		/*
		 * Invalidate the 2- or 4MB page mapping and return
		 * "failure" if the mapping was never accessed or the
		 * allocation of the new page table page fails.
		 */
		if ((oldpde & PG_A) == 0 || (mpte = vm_page_alloc(NULL,
		    va >> PDRSHIFT, VM_ALLOC_NOOBJ | VM_ALLOC_NORMAL |
		    VM_ALLOC_WIRED)) == NULL) {
			SLIST_INIT(&free);
			pmap_remove_pde(pmap, pde, trunc_4mpage(va), &free);
			pmap_invalidate_page(pmap, trunc_4mpage(va));
			pmap_free_zero_pages(&free);
			CTR2(KTR_PMAP, "pmap_demote_pde: failure for va %#x"
			    " in pmap %p", va, pmap);
			return (FALSE);
		}
		if (va < VM_MAXUSER_ADDRESS)
			pmap->pm_stats.resident_count++;
	}
	mptepa = VM_PAGE_TO_PHYS(mpte);

	/*
	 * If the page mapping is in the kernel's address space, then the
	 * KPTmap can provide access to the page table page.  Otherwise,
	 * temporarily map the page table page (mpte) into the kernel's
	 * address space at either PADDR1 or PADDR2. 
	 */
	if (va >= KERNBASE)
		firstpte = &KPTmap[i386_btop(trunc_4mpage(va))];
	else if (curthread->td_pinned > 0 && rw_wowned(&pvh_global_lock)) {
		if ((*PMAP1 & PG_FRAME) != mptepa) {
			*PMAP1 = mptepa | PG_RW | PG_V | PG_A | PG_M;
#ifdef SMP
			PMAP1cpu = PCPU_GET(cpuid);
#endif
			invlcaddr(PADDR1);
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
		firstpte = PADDR1;
	} else {
		mtx_lock(&PMAP2mutex);
		if ((*PMAP2 & PG_FRAME) != mptepa) {
			*PMAP2 = mptepa | PG_RW | PG_V | PG_A | PG_M;
			pmap_invalidate_page(kernel_pmap, (vm_offset_t)PADDR2);
		}
		firstpte = PADDR2;
	}
	newpde = mptepa | PG_M | PG_A | (oldpde & PG_U) | PG_RW | PG_V;
	KASSERT((oldpde & PG_A) != 0,
	    ("pmap_demote_pde: oldpde is missing PG_A"));
	KASSERT((oldpde & (PG_M | PG_RW)) != PG_RW,
	    ("pmap_demote_pde: oldpde is missing PG_M"));
	newpte = oldpde & ~PG_PS;
	if ((newpte & PG_PDE_PAT) != 0)
		newpte ^= PG_PDE_PAT | PG_PTE_PAT;

	/*
	 * If the page table page is new, initialize it.
	 */
	if (mpte->wire_count == 1) {
		mpte->wire_count = NPTEPG;
		pmap_fill_ptp(firstpte, newpte);
	}
	KASSERT((*firstpte & PG_FRAME) == (newpte & PG_FRAME),
	    ("pmap_demote_pde: firstpte and newpte map different physical"
	    " addresses"));

	/*
	 * If the mapping has changed attributes, update the page table
	 * entries.
	 */ 
	if ((*firstpte & PG_PTE_PROMOTE) != (newpte & PG_PTE_PROMOTE))
		pmap_fill_ptp(firstpte, newpte);
	
	/*
	 * Demote the mapping.  This pmap is locked.  The old PDE has
	 * PG_A set.  If the old PDE has PG_RW set, it also has PG_M
	 * set.  Thus, there is no danger of a race with another
	 * processor changing the setting of PG_A and/or PG_M between
	 * the read above and the store below. 
	 */
	if (workaround_erratum383)
		pmap_update_pde(pmap, va, pde, newpde);
	else if (pmap == kernel_pmap)
		pmap_kenter_pde(va, newpde);
	else
		pde_store(pde, newpde);	
	if (firstpte == PADDR2)
		mtx_unlock(&PMAP2mutex);

	/*
	 * Invalidate the recursive mapping of the page table page.
	 */
	pmap_invalidate_page(pmap, (vm_offset_t)vtopte(va));

	/*
	 * Demote the pv entry.  This depends on the earlier demotion
	 * of the mapping.  Specifically, the (re)creation of a per-
	 * page pv entry might trigger the execution of pmap_collect(),
	 * which might reclaim a newly (re)created per-page pv entry
	 * and destroy the associated mapping.  In order to destroy
	 * the mapping, the PDE must have already changed from mapping
	 * the 2mpage to referencing the page table page.
	 */
	if ((oldpde & PG_MANAGED) != 0)
		pmap_pv_demote_pde(pmap, va, oldpde & PG_PS_FRAME);

	pmap_pde_demotions++;
	CTR2(KTR_PMAP, "pmap_demote_pde: success for va %#x"
	    " in pmap %p", va, pmap);
	return (TRUE);
}

/*
 * Removes a 2- or 4MB page mapping from the kernel pmap.
 */
static void
pmap_remove_kernel_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va)
{
	pd_entry_t newpde;
	vm_paddr_t mptepa;
	vm_page_t mpte;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	mpte = pmap_lookup_pt_page(pmap, va);
	if (mpte == NULL)
		panic("pmap_remove_kernel_pde: Missing pt page.");

	pmap_remove_pt_page(pmap, mpte);
	mptepa = VM_PAGE_TO_PHYS(mpte);
	newpde = mptepa | PG_M | PG_A | PG_RW | PG_V;

	/*
	 * Initialize the page table page.
	 */
	pagezero((void *)&KPTmap[i386_btop(trunc_4mpage(va))]);

	/*
	 * Remove the mapping.
	 */
	if (workaround_erratum383)
		pmap_update_pde(pmap, va, pde, newpde);
	else 
		pmap_kenter_pde(va, newpde);

	/*
	 * Invalidate the recursive mapping of the page table page.
	 */
	pmap_invalidate_page(pmap, (vm_offset_t)vtopte(va));
}

/*
 * pmap_remove_pde: do the things to unmap a superpage in a process
 */
static void
pmap_remove_pde(pmap_t pmap, pd_entry_t *pdq, vm_offset_t sva,
    struct spglist *free)
{
	struct md_page *pvh;
	pd_entry_t oldpde;
	vm_offset_t eva, va;
	vm_page_t m, mpte;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((sva & PDRMASK) == 0,
	    ("pmap_remove_pde: sva is not 4mpage aligned"));
	oldpde = pte_load_clear(pdq);
	if (oldpde & PG_W)
		pmap->pm_stats.wired_count -= NBPDR / PAGE_SIZE;

	/*
	 * Machines that don't support invlpg, also don't support
	 * PG_G.
	 */
	if (oldpde & PG_G)
		pmap_invalidate_page(kernel_pmap, sva);
	pmap->pm_stats.resident_count -= NBPDR / PAGE_SIZE;
	if (oldpde & PG_MANAGED) {
		pvh = pa_to_pvh(oldpde & PG_PS_FRAME);
		pmap_pvh_free(pvh, pmap, sva);
		eva = sva + NBPDR;
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
		pmap_remove_kernel_pde(pmap, pdq, sva);
	} else {
		mpte = pmap_lookup_pt_page(pmap, sva);
		if (mpte != NULL) {
			pmap_remove_pt_page(pmap, mpte);
			pmap->pm_stats.resident_count--;
			KASSERT(mpte->wire_count == NPTEPG,
			    ("pmap_remove_pde: pte page wire count error"));
			mpte->wire_count = 0;
			pmap_add_delayed_free_list(mpte, free, FALSE);
			atomic_subtract_int(&cnt.v_wire_count, 1);
		}
	}
}

/*
 * pmap_remove_pte: do the things to unmap a page in a process
 */
static int
pmap_remove_pte(pmap_t pmap, pt_entry_t *ptq, vm_offset_t va,
    struct spglist *free)
{
	pt_entry_t oldpte;
	vm_page_t m;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	oldpte = pte_load_clear(ptq);
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
		m = PHYS_TO_VM_PAGE(oldpte & PG_FRAME);
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
pmap_remove_page(pmap_t pmap, vm_offset_t va, struct spglist *free)
{
	pt_entry_t *pte;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	KASSERT(curthread->td_pinned > 0, ("curthread not pinned"));
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	if ((pte = pmap_pte_quick(pmap, va)) == NULL || *pte == 0)
		return;
	pmap_remove_pte(pmap, pte, va, free);
	pmap_invalidate_page(pmap, va);
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
	struct spglist free;
	int anyvalid;

	/*
	 * Perform an unsynchronized read.  This is, however, safe.
	 */
	if (pmap->pm_stats.resident_count == 0)
		return;

	anyvalid = 0;
	SLIST_INIT(&free);

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
			/*
			 * Are we removing the entire large page?  If not,
			 * demote the mapping and fall through.
			 */
			if (sva + NBPDR == pdnxt && eva >= pdnxt) {
				/*
				 * The TLB entry for a PG_G mapping is
				 * invalidated by pmap_remove_pde().
				 */
				if ((ptpaddr & PG_G) == 0)
					anyvalid = 1;
				pmap_remove_pde(pmap,
				    &pmap->pm_pdir[pdirindex], sva, &free);
				continue;
			} else if (!pmap_demote_pde(pmap,
			    &pmap->pm_pdir[pdirindex], sva)) {
				/* The large page mapping was destroyed. */
				continue;
			}
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
			if (*pte == 0)
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
out:
	sched_unpin();
	if (anyvalid)
		pmap_invalidate_all(pmap);
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
	pmap_free_zero_pages(&free);
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
	struct md_page *pvh;
	pv_entry_t pv;
	pmap_t pmap;
	pt_entry_t *pte, tpte;
	pd_entry_t *pde;
	vm_offset_t va;
	struct spglist free;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_all: page %p is not managed", m));
	SLIST_INIT(&free);
	rw_wlock(&pvh_global_lock);
	sched_pin();
	if ((m->flags & PG_FICTITIOUS) != 0)
		goto small_mappings;
	pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
	while ((pv = TAILQ_FIRST(&pvh->pv_list)) != NULL) {
		va = pv->pv_va;
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pde = pmap_pde(pmap, va);
		(void)pmap_demote_pde(pmap, pde, va);
		PMAP_UNLOCK(pmap);
	}
small_mappings:
	while ((pv = TAILQ_FIRST(&m->md.pv_list)) != NULL) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pmap->pm_stats.resident_count--;
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT((*pde & PG_PS) == 0, ("pmap_remove_all: found"
		    " a 4mpage in page %p's pv list", m));
		pte = pmap_pte_quick(pmap, pv->pv_va);
		tpte = pte_load_clear(pte);
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
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
	pmap_free_zero_pages(&free);
}

/*
 * pmap_protect_pde: do the things to protect a 4mpage in a process
 */
static boolean_t
pmap_protect_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t sva, vm_prot_t prot)
{
	pd_entry_t newpde, oldpde;
	vm_offset_t eva, va;
	vm_page_t m;
	boolean_t anychanged;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT((sva & PDRMASK) == 0,
	    ("pmap_protect_pde: sva is not 4mpage aligned"));
	anychanged = FALSE;
retry:
	oldpde = newpde = *pde;
	if (oldpde & PG_MANAGED) {
		eva = sva + NBPDR;
		for (va = sva, m = PHYS_TO_VM_PAGE(oldpde & PG_PS_FRAME);
		    va < eva; va += PAGE_SIZE, m++)
			if ((oldpde & (PG_M | PG_RW)) == (PG_M | PG_RW))
				vm_page_dirty(m);
	}
	if ((prot & VM_PROT_WRITE) == 0)
		newpde &= ~(PG_RW | PG_M);
#if defined(PAE) || defined(PAE_TABLES)
	if ((prot & VM_PROT_EXECUTE) == 0)
		newpde |= pg_nx;
#endif
	if (newpde != oldpde) {
		if (!pde_cmpset(pde, oldpde, newpde))
			goto retry;
		if (oldpde & PG_G)
			pmap_invalidate_page(pmap, sva);
		else
			anychanged = TRUE;
	}
	return (anychanged);
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
	boolean_t anychanged, pv_lists_locked;

	KASSERT((prot & ~VM_PROT_ALL) == 0, ("invalid prot %x", prot));
	if (prot == VM_PROT_NONE) {
		pmap_remove(pmap, sva, eva);
		return;
	}

#if defined(PAE) || defined(PAE_TABLES)
	if ((prot & (VM_PROT_WRITE|VM_PROT_EXECUTE)) ==
	    (VM_PROT_WRITE|VM_PROT_EXECUTE))
		return;
#else
	if (prot & VM_PROT_WRITE)
		return;
#endif

	if (pmap_is_current(pmap))
		pv_lists_locked = FALSE;
	else {
		pv_lists_locked = TRUE;
resume:
		rw_wlock(&pvh_global_lock);
		sched_pin();
	}
	anychanged = FALSE;

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
			/*
			 * Are we protecting the entire large page?  If not,
			 * demote the mapping and fall through.
			 */
			if (sva + NBPDR == pdnxt && eva >= pdnxt) {
				/*
				 * The TLB entry for a PG_G mapping is
				 * invalidated by pmap_protect_pde().
				 */
				if (pmap_protect_pde(pmap,
				    &pmap->pm_pdir[pdirindex], sva, prot))
					anychanged = TRUE;
				continue;
			} else {
				if (!pv_lists_locked) {
					pv_lists_locked = TRUE;
					if (!rw_try_wlock(&pvh_global_lock)) {
						if (anychanged)
							pmap_invalidate_all(
							    pmap);
						PMAP_UNLOCK(pmap);
						goto resume;
					}
					sched_pin();
				}
				if (!pmap_demote_pde(pmap,
				    &pmap->pm_pdir[pdirindex], sva)) {
					/*
					 * The large page mapping was
					 * destroyed.
					 */
					continue;
				}
			}
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
					m = PHYS_TO_VM_PAGE(pbits & PG_FRAME);
					vm_page_dirty(m);
				}
				pbits &= ~(PG_RW | PG_M);
			}
#if defined(PAE) || defined(PAE_TABLES)
			if ((prot & VM_PROT_EXECUTE) == 0)
				pbits |= pg_nx;
#endif

			if (pbits != obits) {
#if defined(PAE) || defined(PAE_TABLES)
				if (!atomic_cmpset_64(pte, obits, pbits))
					goto retry;
#else
				if (!atomic_cmpset_int((u_int *)pte, obits,
				    pbits))
					goto retry;
#endif
				if (obits & PG_G)
					pmap_invalidate_page(pmap, sva);
				else
					anychanged = TRUE;
			}
		}
	}
	if (anychanged)
		pmap_invalidate_all(pmap);
	if (pv_lists_locked) {
		sched_unpin();
		rw_wunlock(&pvh_global_lock);
	}
	PMAP_UNLOCK(pmap);
}

/*
 * Tries to promote the 512 or 1024, contiguous 4KB page mappings that are
 * within a single page table page (PTP) to a single 2- or 4MB page mapping.
 * For promotion to occur, two conditions must be met: (1) the 4KB page
 * mappings must map aligned, contiguous physical memory and (2) the 4KB page
 * mappings must have identical characteristics.
 *
 * Managed (PG_MANAGED) mappings within the kernel address space are not
 * promoted.  The reason is that kernel PDEs are replicated in each pmap but
 * pmap_clear_ptes() and pmap_ts_referenced() only read the PDE from the kernel
 * pmap.
 */
static void
pmap_promote_pde(pmap_t pmap, pd_entry_t *pde, vm_offset_t va)
{
	pd_entry_t newpde;
	pt_entry_t *firstpte, oldpte, pa, *pte;
	vm_offset_t oldpteva;
	vm_page_t mpte;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * Examine the first PTE in the specified PTP.  Abort if this PTE is
	 * either invalid, unused, or does not map the first 4KB physical page
	 * within a 2- or 4MB page.
	 */
	firstpte = pmap_pte_quick(pmap, trunc_4mpage(va));
setpde:
	newpde = *firstpte;
	if ((newpde & ((PG_FRAME & PDRMASK) | PG_A | PG_V)) != (PG_A | PG_V)) {
		pmap_pde_p_failures++;
		CTR2(KTR_PMAP, "pmap_promote_pde: failure for va %#x"
		    " in pmap %p", va, pmap);
		return;
	}
	if ((*firstpte & PG_MANAGED) != 0 && pmap == kernel_pmap) {
		pmap_pde_p_failures++;
		CTR2(KTR_PMAP, "pmap_promote_pde: failure for va %#x"
		    " in pmap %p", va, pmap);
		return;
	}
	if ((newpde & (PG_M | PG_RW)) == PG_RW) {
		/*
		 * When PG_M is already clear, PG_RW can be cleared without
		 * a TLB invalidation.
		 */
		if (!atomic_cmpset_int((u_int *)firstpte, newpde, newpde &
		    ~PG_RW))  
			goto setpde;
		newpde &= ~PG_RW;
	}

	/* 
	 * Examine each of the other PTEs in the specified PTP.  Abort if this
	 * PTE maps an unexpected 4KB physical page or does not have identical
	 * characteristics to the first PTE.
	 */
	pa = (newpde & (PG_PS_FRAME | PG_A | PG_V)) + NBPDR - PAGE_SIZE;
	for (pte = firstpte + NPTEPG - 1; pte > firstpte; pte--) {
setpte:
		oldpte = *pte;
		if ((oldpte & (PG_FRAME | PG_A | PG_V)) != pa) {
			pmap_pde_p_failures++;
			CTR2(KTR_PMAP, "pmap_promote_pde: failure for va %#x"
			    " in pmap %p", va, pmap);
			return;
		}
		if ((oldpte & (PG_M | PG_RW)) == PG_RW) {
			/*
			 * When PG_M is already clear, PG_RW can be cleared
			 * without a TLB invalidation.
			 */
			if (!atomic_cmpset_int((u_int *)pte, oldpte,
			    oldpte & ~PG_RW))
				goto setpte;
			oldpte &= ~PG_RW;
			oldpteva = (oldpte & PG_FRAME & PDRMASK) |
			    (va & ~PDRMASK);
			CTR2(KTR_PMAP, "pmap_promote_pde: protect for va %#x"
			    " in pmap %p", oldpteva, pmap);
		}
		if ((oldpte & PG_PTE_PROMOTE) != (newpde & PG_PTE_PROMOTE)) {
			pmap_pde_p_failures++;
			CTR2(KTR_PMAP, "pmap_promote_pde: failure for va %#x"
			    " in pmap %p", va, pmap);
			return;
		}
		pa -= PAGE_SIZE;
	}

	/*
	 * Save the page table page in its current state until the PDE
	 * mapping the superpage is demoted by pmap_demote_pde() or
	 * destroyed by pmap_remove_pde(). 
	 */
	mpte = PHYS_TO_VM_PAGE(*pde & PG_FRAME);
	KASSERT(mpte >= vm_page_array &&
	    mpte < &vm_page_array[vm_page_array_size],
	    ("pmap_promote_pde: page table page is out of range"));
	KASSERT(mpte->pindex == va >> PDRSHIFT,
	    ("pmap_promote_pde: page table page's pindex is wrong"));
	if (pmap_insert_pt_page(pmap, mpte)) {
		pmap_pde_p_failures++;
		CTR2(KTR_PMAP,
		    "pmap_promote_pde: failure for va %#x in pmap %p", va,
		    pmap);
		return;
	}

	/*
	 * Promote the pv entries.
	 */
	if ((newpde & PG_MANAGED) != 0)
		pmap_pv_promote_pde(pmap, va, newpde & PG_PS_FRAME);

	/*
	 * Propagate the PAT index to its proper position.
	 */
	if ((newpde & PG_PTE_PAT) != 0)
		newpde ^= PG_PDE_PAT | PG_PTE_PAT;

	/*
	 * Map the superpage.
	 */
	if (workaround_erratum383)
		pmap_update_pde(pmap, va, pde, PG_PS | newpde);
	else if (pmap == kernel_pmap)
		pmap_kenter_pde(va, PG_PS | newpde);
	else
		pde_store(pde, PG_PS | newpde);

	pmap_pde_promotions++;
	CTR2(KTR_PMAP, "pmap_promote_pde: success for va %#x"
	    " in pmap %p", va, pmap);
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
int
pmap_enter(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot,
    u_int flags, int8_t psind)
{
	pd_entry_t *pde;
	pt_entry_t *pte;
	pt_entry_t newpte, origpte;
	pv_entry_t pv;
	vm_paddr_t opa, pa;
	vm_page_t mpte, om;
	boolean_t invlva, wired;

	va = trunc_page(va);
	mpte = NULL;
	wired = (flags & PMAP_ENTER_WIRED) != 0;

	KASSERT(va <= VM_MAX_KERNEL_ADDRESS, ("pmap_enter: toobig"));
	KASSERT(va < UPT_MIN_ADDRESS || va >= UPT_MAX_ADDRESS,
	    ("pmap_enter: invalid to pmap_enter page table pages (va: 0x%x)",
	    va));
	if ((m->oflags & VPO_UNMANAGED) == 0 && !vm_page_xbusied(m))
		VM_OBJECT_ASSERT_LOCKED(m->object);

	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	sched_pin();

	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
	if (va < VM_MAXUSER_ADDRESS) {
		mpte = pmap_allocpte(pmap, va, flags);
		if (mpte == NULL) {
			KASSERT((flags & PMAP_ENTER_NOSLEEP) != 0,
			    ("pmap_allocpte failed with sleep allowed"));
			sched_unpin();
			rw_wunlock(&pvh_global_lock);
			PMAP_UNLOCK(pmap);
			return (KERN_RESOURCE_SHORTAGE);
		}
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
			(uintmax_t)pmap->pm_pdir[PTDPTDI], va);
	}

	pa = VM_PAGE_TO_PHYS(m);
	om = NULL;
	origpte = *pte;
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
		}
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
	newpte = (pt_entry_t)(pa | pmap_cache_bits(m->md.pat_mode, 0) | PG_V);
	if ((prot & VM_PROT_WRITE) != 0) {
		newpte |= PG_RW;
		if ((newpte & PG_MANAGED) != 0)
			vm_page_aflag_set(m, PGA_WRITEABLE);
	}
#if defined(PAE) || defined(PAE_TABLES)
	if ((prot & VM_PROT_EXECUTE) == 0)
		newpte |= pg_nx;
#endif
	if (wired)
		newpte |= PG_W;
	if (va < VM_MAXUSER_ADDRESS)
		newpte |= PG_U;
	if (pmap == kernel_pmap)
		newpte |= pgeflag;

	/*
	 * if the mapping or permission bits are different, we need
	 * to update the pte.
	 */
	if ((origpte & ~(PG_M|PG_A)) != newpte) {
		newpte |= PG_A;
		if ((flags & VM_PROT_WRITE) != 0)
			newpte |= PG_M;
		if (origpte & PG_V) {
			invlva = FALSE;
			origpte = pte_load_store(pte, newpte);
			if (origpte & PG_A) {
				if (origpte & PG_MANAGED)
					vm_page_aflag_set(om, PGA_REFERENCED);
				if (opa != VM_PAGE_TO_PHYS(m))
					invlva = TRUE;
#if defined(PAE) || defined(PAE_TABLES)
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
			    TAILQ_EMPTY(&om->md.pv_list) &&
			    ((om->flags & PG_FICTITIOUS) != 0 ||
			    TAILQ_EMPTY(&pa_to_pvh(opa)->pv_list)))
				vm_page_aflag_clear(om, PGA_WRITEABLE);
			if (invlva)
				pmap_invalidate_page(pmap, va);
		} else
			pte_store(pte, newpte);
	}

	/*
	 * If both the page table page and the reservation are fully
	 * populated, then attempt promotion.
	 */
	if ((mpte == NULL || mpte->wire_count == NPTEPG) &&
	    pg_ps_enabled && (m->flags & PG_FICTITIOUS) == 0 &&
	    vm_reserv_level_iffullpop(m) == 0)
		pmap_promote_pde(pmap, pde, va);

	sched_unpin();
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
	return (KERN_SUCCESS);
}

/*
 * Tries to create a 2- or 4MB page mapping.  Returns TRUE if successful and
 * FALSE otherwise.  Fails if (1) a page table page cannot be allocated without
 * blocking, (2) a mapping already exists at the specified virtual address, or
 * (3) a pv entry cannot be allocated without reclaiming another pv entry. 
 */
static boolean_t
pmap_enter_pde(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot)
{
	pd_entry_t *pde, newpde;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	pde = pmap_pde(pmap, va);
	if (*pde != 0) {
		CTR2(KTR_PMAP, "pmap_enter_pde: failure for va %#lx"
		    " in pmap %p", va, pmap);
		return (FALSE);
	}
	newpde = VM_PAGE_TO_PHYS(m) | pmap_cache_bits(m->md.pat_mode, 1) |
	    PG_PS | PG_V;
	if ((m->oflags & VPO_UNMANAGED) == 0) {
		newpde |= PG_MANAGED;

		/*
		 * Abort this mapping if its PV entry could not be created.
		 */
		if (!pmap_pv_insert_pde(pmap, va, VM_PAGE_TO_PHYS(m))) {
			CTR2(KTR_PMAP, "pmap_enter_pde: failure for va %#lx"
			    " in pmap %p", va, pmap);
			return (FALSE);
		}
	}
#if defined(PAE) || defined(PAE_TABLES)
	if ((prot & VM_PROT_EXECUTE) == 0)
		newpde |= pg_nx;
#endif
	if (va < VM_MAXUSER_ADDRESS)
		newpde |= PG_U;

	/*
	 * Increment counters.
	 */
	pmap->pm_stats.resident_count += NBPDR / PAGE_SIZE;

	/*
	 * Map the superpage.
	 */
	pde_store(pde, newpde);

	pmap_pde_mappings++;
	CTR2(KTR_PMAP, "pmap_enter_pde: success for va %#lx"
	    " in pmap %p", va, pmap);
	return (TRUE);
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
	vm_offset_t va;
	vm_page_t m, mpte;
	vm_pindex_t diff, psize;

	VM_OBJECT_ASSERT_LOCKED(m_start->object);

	psize = atop(end - start);
	mpte = NULL;
	m = m_start;
	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	while (m != NULL && (diff = m->pindex - m_start->pindex) < psize) {
		va = start + ptoa(diff);
		if ((va & PDRMASK) == 0 && va + NBPDR <= end &&
		    m->psind == 1 && pg_ps_enabled &&
		    pmap_enter_pde(pmap, va, m, prot))
			m = &m[NBPDR / PAGE_SIZE - 1];
		else
			mpte = pmap_enter_quick_locked(pmap, va, m, prot,
			    mpte);
		m = TAILQ_NEXT(m, listq);
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

	rw_wlock(&pvh_global_lock);
	PMAP_LOCK(pmap);
	(void)pmap_enter_quick_locked(pmap, va, m, prot, NULL);
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
}

static vm_page_t
pmap_enter_quick_locked(pmap_t pmap, vm_offset_t va, vm_page_t m,
    vm_prot_t prot, vm_page_t mpte)
{
	pt_entry_t *pte;
	vm_paddr_t pa;
	struct spglist free;

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
		pd_entry_t ptepa;

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
			ptepa = pmap->pm_pdir[ptepindex];

			/*
			 * If the page table page is mapped, we just increment
			 * the hold count, and activate it.
			 */
			if (ptepa) {
				if (ptepa & PG_PS)
					return (NULL);
				mpte = PHYS_TO_VM_PAGE(ptepa & PG_FRAME);
				mpte->wire_count++;
			} else {
				mpte = _pmap_allocpte(pmap, ptepindex,
				    PMAP_ENTER_NOSLEEP);
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
	pte = vtopte(va);
	if (*pte) {
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
			SLIST_INIT(&free);
			if (pmap_unwire_ptp(pmap, mpte, &free)) {
				pmap_invalidate_page(pmap, va);
				pmap_free_zero_pages(&free);
			}
			
			mpte = NULL;
		}
		return (mpte);
	}

	/*
	 * Increment counters
	 */
	pmap->pm_stats.resident_count++;

	pa = VM_PAGE_TO_PHYS(m) | pmap_cache_bits(m->md.pat_mode, 0);
#if defined(PAE) || defined(PAE_TABLES)
	if ((prot & VM_PROT_EXECUTE) == 0)
		pa |= pg_nx;
#endif

	/*
	 * Now validate mapping with RO protection
	 */
	if ((m->oflags & VPO_UNMANAGED) != 0)
		pte_store(pte, pa | PG_V | PG_U);
	else
		pte_store(pte, pa | PG_V | PG_U | PG_MANAGED);
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

	va = (vm_offset_t)crashdumpmap + (i * PAGE_SIZE);
	pmap_kenter(va, pa);
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
 *	Clear the wired attribute from the mappings for the specified range of
 *	addresses in the given pmap.  Every valid mapping within that range
 *	must have the wired attribute set.  In contrast, invalid mappings
 *	cannot have the wired attribute set, so they are ignored.
 *
 *	The wired attribute of the page table entry is not a hardware feature,
 *	so there is no need to invalidate any TLB entries.
 */
void
pmap_unwire(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	vm_offset_t pdnxt;
	pd_entry_t *pde;
	pt_entry_t *pte;
	boolean_t pv_lists_locked;

	if (pmap_is_current(pmap))
		pv_lists_locked = FALSE;
	else {
		pv_lists_locked = TRUE;
resume:
		rw_wlock(&pvh_global_lock);
		sched_pin();
	}
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = pdnxt) {
		pdnxt = (sva + NBPDR) & ~PDRMASK;
		if (pdnxt < sva)
			pdnxt = eva;
		pde = pmap_pde(pmap, sva);
		if ((*pde & PG_V) == 0)
			continue;
		if ((*pde & PG_PS) != 0) {
			if ((*pde & PG_W) == 0)
				panic("pmap_unwire: pde %#jx is missing PG_W",
				    (uintmax_t)*pde);

			/*
			 * Are we unwiring the entire large page?  If not,
			 * demote the mapping and fall through.
			 */
			if (sva + NBPDR == pdnxt && eva >= pdnxt) {
				/*
				 * Regardless of whether a pde (or pte) is 32
				 * or 64 bits in size, PG_W is among the least
				 * significant 32 bits.
				 */
				atomic_clear_int((u_int *)pde, PG_W);
				pmap->pm_stats.wired_count -= NBPDR /
				    PAGE_SIZE;
				continue;
			} else {
				if (!pv_lists_locked) {
					pv_lists_locked = TRUE;
					if (!rw_try_wlock(&pvh_global_lock)) {
						PMAP_UNLOCK(pmap);
						/* Repeat sva. */
						goto resume;
					}
					sched_pin();
				}
				if (!pmap_demote_pde(pmap, pde, sva))
					panic("pmap_unwire: demotion failed");
			}
		}
		if (pdnxt > eva)
			pdnxt = eva;
		for (pte = pmap_pte_quick(pmap, sva); sva != pdnxt; pte++,
		    sva += PAGE_SIZE) {
			if ((*pte & PG_V) == 0)
				continue;
			if ((*pte & PG_W) == 0)
				panic("pmap_unwire: pte %#jx is missing PG_W",
				    (uintmax_t)*pte);

			/*
			 * PG_W must be cleared atomically.  Although the pmap
			 * lock synchronizes access to PG_W, another processor
			 * could be setting PG_M and/or PG_A concurrently.
			 *
			 * PG_W is among the least significant 32 bits.
			 */
			atomic_clear_int((u_int *)pte, PG_W);
			pmap->pm_stats.wired_count--;
		}
	}
	if (pv_lists_locked) {
		sched_unpin();
		rw_wunlock(&pvh_global_lock);
	}
	PMAP_UNLOCK(pmap);
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
	struct spglist free;
	vm_offset_t addr;
	vm_offset_t end_addr = src_addr + len;
	vm_offset_t pdnxt;

	if (dst_addr != src_addr)
		return;

	if (!pmap_is_current(src_pmap))
		return;

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

		srcptepaddr = src_pmap->pm_pdir[ptepindex];
		if (srcptepaddr == 0)
			continue;
			
		if (srcptepaddr & PG_PS) {
			if ((addr & PDRMASK) != 0 || addr + NBPDR > end_addr)
				continue;
			if (dst_pmap->pm_pdir[ptepindex] == 0 &&
			    ((srcptepaddr & PG_MANAGED) == 0 ||
			    pmap_pv_insert_pde(dst_pmap, addr, srcptepaddr &
			    PG_PS_FRAME))) {
				dst_pmap->pm_pdir[ptepindex] = srcptepaddr &
				    ~PG_W;
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
				    PMAP_ENTER_NOSLEEP);
				if (dstmpte == NULL)
					goto out;
				dst_pte = pmap_pte_quick(dst_pmap, addr);
				if (*dst_pte == 0 &&
				    pmap_try_insert_pv_entry(dst_pmap, addr,
				    PHYS_TO_VM_PAGE(ptetemp & PG_FRAME))) {
					/*
					 * Clear the wired, modified, and
					 * accessed (referenced) bits
					 * during the copy.
					 */
					*dst_pte = ptetemp & ~(PG_W | PG_M |
					    PG_A);
					dst_pmap->pm_stats.resident_count++;
	 			} else {
					SLIST_INIT(&free);
					if (pmap_unwire_ptp(dst_pmap, dstmpte,
					    &free)) {
						pmap_invalidate_page(dst_pmap,
						    addr);
						pmap_free_zero_pages(&free);
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
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(src_pmap);
	PMAP_UNLOCK(dst_pmap);
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
	*sysmaps->CMAP2 = PG_V | PG_RW | VM_PAGE_TO_PHYS(m) | PG_A | PG_M |
	    pmap_cache_bits(m->md.pat_mode, 0);
	invlcaddr(sysmaps->CADDR2);
	pagezero(sysmaps->CADDR2);
	*sysmaps->CMAP2 = 0;
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
	*sysmaps->CMAP2 = PG_V | PG_RW | VM_PAGE_TO_PHYS(m) | PG_A | PG_M |
	    pmap_cache_bits(m->md.pat_mode, 0);
	invlcaddr(sysmaps->CADDR2);
	if (off == 0 && size == PAGE_SIZE) 
		pagezero(sysmaps->CADDR2);
	else
		bzero((char *)sysmaps->CADDR2 + off, size);
	*sysmaps->CMAP2 = 0;
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
	*CMAP3 = PG_V | PG_RW | VM_PAGE_TO_PHYS(m) | PG_A | PG_M |
	    pmap_cache_bits(m->md.pat_mode, 0);
	invlcaddr(CADDR3);
	pagezero(CADDR3);
	*CMAP3 = 0;
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
	*sysmaps->CMAP1 = PG_V | VM_PAGE_TO_PHYS(src) | PG_A |
	    pmap_cache_bits(src->md.pat_mode, 0);
	invlcaddr(sysmaps->CADDR1);
	*sysmaps->CMAP2 = PG_V | PG_RW | VM_PAGE_TO_PHYS(dst) | PG_A | PG_M |
	    pmap_cache_bits(dst->md.pat_mode, 0);
	invlcaddr(sysmaps->CADDR2);
	bcopy(sysmaps->CADDR1, sysmaps->CADDR2, PAGE_SIZE);
	*sysmaps->CMAP1 = 0;
	*sysmaps->CMAP2 = 0;
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
		*sysmaps->CMAP1 = PG_V | VM_PAGE_TO_PHYS(a_pg) | PG_A |
		    pmap_cache_bits(a_pg->md.pat_mode, 0);
		invlcaddr(sysmaps->CADDR1);
		*sysmaps->CMAP2 = PG_V | PG_RW | VM_PAGE_TO_PHYS(b_pg) | PG_A |
		    PG_M | pmap_cache_bits(b_pg->md.pat_mode, 0);
		invlcaddr(sysmaps->CADDR2);
		a_cp = sysmaps->CADDR1 + a_pg_offset;
		b_cp = sysmaps->CADDR2 + b_pg_offset;
		bcopy(a_cp, b_cp, cnt);
		a_offset += cnt;
		b_offset += cnt;
		xfersize -= cnt;
	}
	*sysmaps->CMAP1 = 0;
	*sysmaps->CMAP2 = 0;
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
	struct md_page *pvh;
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
	if (!rv && loops < 16 && (m->flags & PG_FICTITIOUS) == 0) {
		pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
		TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
			if (PV_PMAP(pv) == pmap) {
				rv = TRUE;
				break;
			}
			loops++;
			if (loops >= 16)
				break;
		}
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
	int count;

	count = 0;
	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (count);
	rw_wlock(&pvh_global_lock);
	count = pmap_pvh_wired_mappings(&m->md, count);
	if ((m->flags & PG_FICTITIOUS) == 0) {
	    count = pmap_pvh_wired_mappings(pa_to_pvh(VM_PAGE_TO_PHYS(m)),
	        count);
	}
	rw_wunlock(&pvh_global_lock);
	return (count);
}

/*
 *	pmap_pvh_wired_mappings:
 *
 *	Return the updated number "count" of managed mappings that are wired.
 */
static int
pmap_pvh_wired_mappings(struct md_page *pvh, int count)
{
	pmap_t pmap;
	pt_entry_t *pte;
	pv_entry_t pv;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	sched_pin();
	TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pte = pmap_pte_quick(pmap, pv->pv_va);
		if ((*pte & PG_W) != 0)
			count++;
		PMAP_UNLOCK(pmap);
	}
	sched_unpin();
	return (count);
}

/*
 * Returns TRUE if the given page is mapped individually or as part of
 * a 4mpage.  Otherwise, returns FALSE.
 */
boolean_t
pmap_page_is_mapped(vm_page_t m)
{
	boolean_t rv;

	if ((m->oflags & VPO_UNMANAGED) != 0)
		return (FALSE);
	rw_wlock(&pvh_global_lock);
	rv = !TAILQ_EMPTY(&m->md.pv_list) ||
	    ((m->flags & PG_FICTITIOUS) == 0 &&
	    !TAILQ_EMPTY(&pa_to_pvh(VM_PAGE_TO_PHYS(m))->pv_list));
	rw_wunlock(&pvh_global_lock);
	return (rv);
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
	vm_page_t m, mpte, mt;
	pv_entry_t pv;
	struct md_page *pvh;
	struct pv_chunk *pc, *npc;
	struct spglist free;
	int field, idx;
	int32_t bit;
	uint32_t inuse, bitmask;
	int allfree;

	if (pmap != PCPU_GET(curpmap)) {
		printf("warning: pmap_remove_pages called with non-current pmap\n");
		return;
	}
	SLIST_INIT(&free);
	rw_wlock(&pvh_global_lock);
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

				pte = pmap_pde(pmap, pv->pv_va);
				tpte = *pte;
				if ((tpte & PG_PS) == 0) {
					pte = vtopte(pv->pv_va);
					tpte = *pte & ~PG_PTE_PAT;
				}

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

				KASSERT((m->flags & PG_FICTITIOUS) != 0 ||
				    m < &vm_page_array[vm_page_array_size],
				    ("pmap_remove_pages: bad tpte %#jx",
				    (uintmax_t)tpte));

				pte_clear(pte);

				/*
				 * Update the vm_page_t clean/reference bits.
				 */
				if ((tpte & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
					if ((tpte & PG_PS) != 0) {
						for (mt = m; mt < &m[NBPDR / PAGE_SIZE]; mt++)
							vm_page_dirty(mt);
					} else
						vm_page_dirty(m);
				}

				/* Mark free */
				PV_STAT(pv_entry_frees++);
				PV_STAT(pv_entry_spare++);
				pv_entry_count--;
				pc->pc_map[field] |= bitmask;
				if ((tpte & PG_PS) != 0) {
					pmap->pm_stats.resident_count -= NBPDR / PAGE_SIZE;
					pvh = pa_to_pvh(tpte & PG_PS_FRAME);
					TAILQ_REMOVE(&pvh->pv_list, pv, pv_next);
					if (TAILQ_EMPTY(&pvh->pv_list)) {
						for (mt = m; mt < &m[NBPDR / PAGE_SIZE]; mt++)
							if (TAILQ_EMPTY(&mt->md.pv_list))
								vm_page_aflag_clear(mt, PGA_WRITEABLE);
					}
					mpte = pmap_lookup_pt_page(pmap, pv->pv_va);
					if (mpte != NULL) {
						pmap_remove_pt_page(pmap, mpte);
						pmap->pm_stats.resident_count--;
						KASSERT(mpte->wire_count == NPTEPG,
						    ("pmap_remove_pages: pte page wire count error"));
						mpte->wire_count = 0;
						pmap_add_delayed_free_list(mpte, &free, FALSE);
						atomic_subtract_int(&cnt.v_wire_count, 1);
					}
				} else {
					pmap->pm_stats.resident_count--;
					TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
					if (TAILQ_EMPTY(&m->md.pv_list) &&
					    (m->flags & PG_FICTITIOUS) == 0) {
						pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
						if (TAILQ_EMPTY(&pvh->pv_list))
							vm_page_aflag_clear(m, PGA_WRITEABLE);
					}
					pmap_unuse_pt(pmap, pv->pv_va, &free);
				}
			}
		}
		if (allfree) {
			TAILQ_REMOVE(&pmap->pm_pvchunk, pc, pc_list);
			free_pv_chunk(pc);
		}
	}
	sched_unpin();
	pmap_invalidate_all(pmap);
	rw_wunlock(&pvh_global_lock);
	PMAP_UNLOCK(pmap);
	pmap_free_zero_pages(&free);
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
	boolean_t rv;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_is_modified: page %p is not managed", m));

	/*
	 * If the page is not exclusive busied, then PGA_WRITEABLE cannot be
	 * concurrently set while the object is locked.  Thus, if PGA_WRITEABLE
	 * is clear, no PTEs can have PG_M set.
	 */
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (!vm_page_xbusied(m) && (m->aflags & PGA_WRITEABLE) == 0)
		return (FALSE);
	rw_wlock(&pvh_global_lock);
	rv = pmap_is_modified_pvh(&m->md) ||
	    ((m->flags & PG_FICTITIOUS) == 0 &&
	    pmap_is_modified_pvh(pa_to_pvh(VM_PAGE_TO_PHYS(m))));
	rw_wunlock(&pvh_global_lock);
	return (rv);
}

/*
 * Returns TRUE if any of the given mappings were used to modify
 * physical memory.  Otherwise, returns FALSE.  Both page and 2mpage
 * mappings are supported.
 */
static boolean_t
pmap_is_modified_pvh(struct md_page *pvh)
{
	pv_entry_t pv;
	pt_entry_t *pte;
	pmap_t pmap;
	boolean_t rv;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	rv = FALSE;
	sched_pin();
	TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pte = pmap_pte_quick(pmap, pv->pv_va);
		rv = (*pte & (PG_M | PG_RW)) == (PG_M | PG_RW);
		PMAP_UNLOCK(pmap);
		if (rv)
			break;
	}
	sched_unpin();
	return (rv);
}

/*
 *	pmap_is_prefaultable:
 *
 *	Return whether or not the specified virtual address is elgible
 *	for prefault.
 */
boolean_t
pmap_is_prefaultable(pmap_t pmap, vm_offset_t addr)
{
	pd_entry_t *pde;
	pt_entry_t *pte;
	boolean_t rv;

	rv = FALSE;
	PMAP_LOCK(pmap);
	pde = pmap_pde(pmap, addr);
	if (*pde != 0 && (*pde & PG_PS) == 0) {
		pte = vtopte(addr);
		rv = *pte == 0;
	}
	PMAP_UNLOCK(pmap);
	return (rv);
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page was referenced
 *	in any physical maps.
 */
boolean_t
pmap_is_referenced(vm_page_t m)
{
	boolean_t rv;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_is_referenced: page %p is not managed", m));
	rw_wlock(&pvh_global_lock);
	rv = pmap_is_referenced_pvh(&m->md) ||
	    ((m->flags & PG_FICTITIOUS) == 0 &&
	    pmap_is_referenced_pvh(pa_to_pvh(VM_PAGE_TO_PHYS(m))));
	rw_wunlock(&pvh_global_lock);
	return (rv);
}

/*
 * Returns TRUE if any of the given mappings were referenced and FALSE
 * otherwise.  Both page and 4mpage mappings are supported.
 */
static boolean_t
pmap_is_referenced_pvh(struct md_page *pvh)
{
	pv_entry_t pv;
	pt_entry_t *pte;
	pmap_t pmap;
	boolean_t rv;

	rw_assert(&pvh_global_lock, RA_WLOCKED);
	rv = FALSE;
	sched_pin();
	TAILQ_FOREACH(pv, &pvh->pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pte = pmap_pte_quick(pmap, pv->pv_va);
		rv = (*pte & (PG_A | PG_V)) == (PG_A | PG_V);
		PMAP_UNLOCK(pmap);
		if (rv)
			break;
	}
	sched_unpin();
	return (rv);
}

/*
 * Clear the write and modified bits in each of the given page's mappings.
 */
void
pmap_remove_write(vm_page_t m)
{
	struct md_page *pvh;
	pv_entry_t next_pv, pv;
	pmap_t pmap;
	pd_entry_t *pde;
	pt_entry_t oldpte, *pte;
	vm_offset_t va;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_remove_write: page %p is not managed", m));

	/*
	 * If the page is not exclusive busied, then PGA_WRITEABLE cannot be
	 * set by another thread while the object is locked.  Thus,
	 * if PGA_WRITEABLE is clear, no page table entries need updating.
	 */
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	if (!vm_page_xbusied(m) && (m->aflags & PGA_WRITEABLE) == 0)
		return;
	rw_wlock(&pvh_global_lock);
	sched_pin();
	if ((m->flags & PG_FICTITIOUS) != 0)
		goto small_mappings;
	pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
	TAILQ_FOREACH_SAFE(pv, &pvh->pv_list, pv_next, next_pv) {
		va = pv->pv_va;
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pde = pmap_pde(pmap, va);
		if ((*pde & PG_RW) != 0)
			(void)pmap_demote_pde(pmap, pde, va);
		PMAP_UNLOCK(pmap);
	}
small_mappings:
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT((*pde & PG_PS) == 0, ("pmap_clear_write: found"
		    " a 4mpage in page %p's pv list", m));
		pte = pmap_pte_quick(pmap, pv->pv_va);
retry:
		oldpte = *pte;
		if ((oldpte & PG_RW) != 0) {
			/*
			 * Regardless of whether a pte is 32 or 64 bits
			 * in size, PG_RW and PG_M are among the least
			 * significant 32 bits.
			 */
			if (!atomic_cmpset_int((u_int *)pte, oldpte,
			    oldpte & ~(PG_RW | PG_M)))
				goto retry;
			if ((oldpte & PG_M) != 0)
				vm_page_dirty(m);
			pmap_invalidate_page(pmap, pv->pv_va);
		}
		PMAP_UNLOCK(pmap);
	}
	vm_page_aflag_clear(m, PGA_WRITEABLE);
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
}

#define	PMAP_TS_REFERENCED_MAX	5

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
	struct md_page *pvh;
	pv_entry_t pv, pvf;
	pmap_t pmap;
	pd_entry_t *pde;
	pt_entry_t *pte;
	vm_paddr_t pa;
	int rtval = 0;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_ts_referenced: page %p is not managed", m));
	pa = VM_PAGE_TO_PHYS(m);
	pvh = pa_to_pvh(pa);
	rw_wlock(&pvh_global_lock);
	sched_pin();
	if ((m->flags & PG_FICTITIOUS) != 0 ||
	    (pvf = TAILQ_FIRST(&pvh->pv_list)) == NULL)
		goto small_mappings;
	pv = pvf;
	do {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pde = pmap_pde(pmap, pv->pv_va);
		if ((*pde & PG_A) != 0) {
			/*
			 * Since this reference bit is shared by either 1024
			 * or 512 4KB pages, it should not be cleared every
			 * time it is tested.  Apply a simple "hash" function
			 * on the physical page number, the virtual superpage
			 * number, and the pmap address to select one 4KB page
			 * out of the 1024 or 512 on which testing the
			 * reference bit will result in clearing that bit.
			 * This function is designed to avoid the selection of
			 * the same 4KB page for every 2- or 4MB page mapping.
			 *
			 * On demotion, a mapping that hasn't been referenced
			 * is simply destroyed.  To avoid the possibility of a
			 * subsequent page fault on a demoted wired mapping,
			 * always leave its reference bit set.  Moreover,
			 * since the superpage is wired, the current state of
			 * its reference bit won't affect page replacement.
			 */
			if ((((pa >> PAGE_SHIFT) ^ (pv->pv_va >> PDRSHIFT) ^
			    (uintptr_t)pmap) & (NPTEPG - 1)) == 0 &&
			    (*pde & PG_W) == 0) {
				atomic_clear_int((u_int *)pde, PG_A);
				pmap_invalidate_page(pmap, pv->pv_va);
			}
			rtval++;
		}
		PMAP_UNLOCK(pmap);
		/* Rotate the PV list if it has more than one entry. */
		if (TAILQ_NEXT(pv, pv_next) != NULL) {
			TAILQ_REMOVE(&pvh->pv_list, pv, pv_next);
			TAILQ_INSERT_TAIL(&pvh->pv_list, pv, pv_next);
		}
		if (rtval >= PMAP_TS_REFERENCED_MAX)
			goto out;
	} while ((pv = TAILQ_FIRST(&pvh->pv_list)) != pvf);
small_mappings:
	if ((pvf = TAILQ_FIRST(&m->md.pv_list)) == NULL)
		goto out;
	pv = pvf;
	do {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT((*pde & PG_PS) == 0,
		    ("pmap_ts_referenced: found a 4mpage in page %p's pv list",
		    m));
		pte = pmap_pte_quick(pmap, pv->pv_va);
		if ((*pte & PG_A) != 0) {
			atomic_clear_int((u_int *)pte, PG_A);
			pmap_invalidate_page(pmap, pv->pv_va);
			rtval++;
		}
		PMAP_UNLOCK(pmap);
		/* Rotate the PV list if it has more than one entry. */
		if (TAILQ_NEXT(pv, pv_next) != NULL) {
			TAILQ_REMOVE(&m->md.pv_list, pv, pv_next);
			TAILQ_INSERT_TAIL(&m->md.pv_list, pv, pv_next);
		}
	} while ((pv = TAILQ_FIRST(&m->md.pv_list)) != pvf && rtval <
	    PMAP_TS_REFERENCED_MAX);
out:
	sched_unpin();
	rw_wunlock(&pvh_global_lock);
	return (rtval);
}

/*
 *	Apply the given advice to the specified range of addresses within the
 *	given pmap.  Depending on the advice, clear the referenced and/or
 *	modified flags in each mapping and set the mapped page's dirty field.
 */
void
pmap_advise(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, int advice)
{
	pd_entry_t oldpde, *pde;
	pt_entry_t *pte;
	vm_offset_t pdnxt;
	vm_page_t m;
	boolean_t anychanged, pv_lists_locked;

	if (advice != MADV_DONTNEED && advice != MADV_FREE)
		return;
	if (pmap_is_current(pmap))
		pv_lists_locked = FALSE;
	else {
		pv_lists_locked = TRUE;
resume:
		rw_wlock(&pvh_global_lock);
		sched_pin();
	}
	anychanged = FALSE;
	PMAP_LOCK(pmap);
	for (; sva < eva; sva = pdnxt) {
		pdnxt = (sva + NBPDR) & ~PDRMASK;
		if (pdnxt < sva)
			pdnxt = eva;
		pde = pmap_pde(pmap, sva);
		oldpde = *pde;
		if ((oldpde & PG_V) == 0)
			continue;
		else if ((oldpde & PG_PS) != 0) {
			if ((oldpde & PG_MANAGED) == 0)
				continue;
			if (!pv_lists_locked) {
				pv_lists_locked = TRUE;
				if (!rw_try_wlock(&pvh_global_lock)) {
					if (anychanged)
						pmap_invalidate_all(pmap);
					PMAP_UNLOCK(pmap);
					goto resume;
				}
				sched_pin();
			}
			if (!pmap_demote_pde(pmap, pde, sva)) {
				/*
				 * The large page mapping was destroyed.
				 */
				continue;
			}

			/*
			 * Unless the page mappings are wired, remove the
			 * mapping to a single page so that a subsequent
			 * access may repromote.  Since the underlying page
			 * table page is fully populated, this removal never
			 * frees a page table page.
			 */
			if ((oldpde & PG_W) == 0) {
				pte = pmap_pte_quick(pmap, sva);
				KASSERT((*pte & PG_V) != 0,
				    ("pmap_advise: invalid PTE"));
				pmap_remove_pte(pmap, pte, sva, NULL);
				anychanged = TRUE;
			}
		}
		if (pdnxt > eva)
			pdnxt = eva;
		for (pte = pmap_pte_quick(pmap, sva); sva != pdnxt; pte++,
		    sva += PAGE_SIZE) {
			if ((*pte & (PG_MANAGED | PG_V)) != (PG_MANAGED |
			    PG_V))
				continue;
			else if ((*pte & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
				if (advice == MADV_DONTNEED) {
					/*
					 * Future calls to pmap_is_modified()
					 * can be avoided by making the page
					 * dirty now.
					 */
					m = PHYS_TO_VM_PAGE(*pte & PG_FRAME);
					vm_page_dirty(m);
				}
				atomic_clear_int((u_int *)pte, PG_M | PG_A);
			} else if ((*pte & PG_A) != 0)
				atomic_clear_int((u_int *)pte, PG_A);
			else
				continue;
			if ((*pte & PG_G) != 0)
				pmap_invalidate_page(pmap, sva);
			else
				anychanged = TRUE;
		}
	}
	if (anychanged)
		pmap_invalidate_all(pmap);
	if (pv_lists_locked) {
		sched_unpin();
		rw_wunlock(&pvh_global_lock);
	}
	PMAP_UNLOCK(pmap);
}

/*
 *	Clear the modify bits on the specified physical page.
 */
void
pmap_clear_modify(vm_page_t m)
{
	struct md_page *pvh;
	pv_entry_t next_pv, pv;
	pmap_t pmap;
	pd_entry_t oldpde, *pde;
	pt_entry_t oldpte, *pte;
	vm_offset_t va;

	KASSERT((m->oflags & VPO_UNMANAGED) == 0,
	    ("pmap_clear_modify: page %p is not managed", m));
	VM_OBJECT_ASSERT_WLOCKED(m->object);
	KASSERT(!vm_page_xbusied(m),
	    ("pmap_clear_modify: page %p is exclusive busied", m));

	/*
	 * If the page is not PGA_WRITEABLE, then no PTEs can have PG_M set.
	 * If the object containing the page is locked and the page is not
	 * exclusive busied, then PGA_WRITEABLE cannot be concurrently set.
	 */
	if ((m->aflags & PGA_WRITEABLE) == 0)
		return;
	rw_wlock(&pvh_global_lock);
	sched_pin();
	if ((m->flags & PG_FICTITIOUS) != 0)
		goto small_mappings;
	pvh = pa_to_pvh(VM_PAGE_TO_PHYS(m));
	TAILQ_FOREACH_SAFE(pv, &pvh->pv_list, pv_next, next_pv) {
		va = pv->pv_va;
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pde = pmap_pde(pmap, va);
		oldpde = *pde;
		if ((oldpde & PG_RW) != 0) {
			if (pmap_demote_pde(pmap, pde, va)) {
				if ((oldpde & PG_W) == 0) {
					/*
					 * Write protect the mapping to a
					 * single page so that a subsequent
					 * write access may repromote.
					 */
					va += VM_PAGE_TO_PHYS(m) - (oldpde &
					    PG_PS_FRAME);
					pte = pmap_pte_quick(pmap, va);
					oldpte = *pte;
					if ((oldpte & PG_V) != 0) {
						/*
						 * Regardless of whether a pte is 32 or 64 bits
						 * in size, PG_RW and PG_M are among the least
						 * significant 32 bits.
						 */
						while (!atomic_cmpset_int((u_int *)pte,
						    oldpte,
						    oldpte & ~(PG_M | PG_RW)))
							oldpte = *pte;
						vm_page_dirty(m);
						pmap_invalidate_page(pmap, va);
					}
				}
			}
		}
		PMAP_UNLOCK(pmap);
	}
small_mappings:
	TAILQ_FOREACH(pv, &m->md.pv_list, pv_next) {
		pmap = PV_PMAP(pv);
		PMAP_LOCK(pmap);
		pde = pmap_pde(pmap, pv->pv_va);
		KASSERT((*pde & PG_PS) == 0, ("pmap_clear_modify: found"
		    " a 4mpage in page %p's pv list", m));
		pte = pmap_pte_quick(pmap, pv->pv_va);
		if ((*pte & (PG_M | PG_RW)) == (PG_M | PG_RW)) {
			/*
			 * Regardless of whether a pte is 32 or 64 bits
			 * in size, PG_M is among the least significant
			 * 32 bits. 
			 */
			atomic_clear_int((u_int *)pte, PG_M);
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

/* Adjust the cache mode for a 4KB page mapped via a PTE. */
static __inline void
pmap_pte_attr(pt_entry_t *pte, int cache_bits)
{
	u_int opte, npte;

	/*
	 * The cache mode bits are all in the low 32-bits of the
	 * PTE, so we can just spin on updating the low 32-bits.
	 */
	do {
		opte = *(u_int *)pte;
		npte = opte & ~PG_PTE_CACHE;
		npte |= cache_bits;
	} while (npte != opte && !atomic_cmpset_int((u_int *)pte, opte, npte));
}

/* Adjust the cache mode for a 2/4MB page mapped via a PDE. */
static __inline void
pmap_pde_attr(pd_entry_t *pde, int cache_bits)
{
	u_int opde, npde;

	/*
	 * The cache mode bits are all in the low 32-bits of the
	 * PDE, so we can just spin on updating the low 32-bits.
	 */
	do {
		opde = *(u_int *)pde;
		npde = opde & ~PG_PDE_CACHE;
		npde |= cache_bits;
	} while (npde != opde && !atomic_cmpset_int((u_int *)pde, opde, npde));
}

/*
 * Map a set of physical memory pages into the kernel virtual
 * address space. Return a pointer to where it is mapped. This
 * routine is intended to be used for mapping device memory,
 * NOT real memory.
 */
void *
pmap_mapdev_attr(vm_paddr_t pa, vm_size_t size, int mode)
{
	struct pmap_preinit_mapping *ppim;
	vm_offset_t va, offset;
	vm_size_t tmpsize;
	int i;

	offset = pa & PAGE_MASK;
	size = round_page(offset + size);
	pa = pa & PG_FRAME;

	if (pa < KERNLOAD && pa + size <= KERNLOAD)
		va = KERNBASE + pa;
	else if (!pmap_initialized) {
		va = 0;
		for (i = 0; i < PMAP_PREINIT_MAPPING_COUNT; i++) {
			ppim = pmap_preinit_mapping + i;
			if (ppim->va == 0) {
				ppim->pa = pa;
				ppim->sz = size;
				ppim->mode = mode;
				ppim->va = virtual_avail;
				virtual_avail += size;
				va = ppim->va;
				break;
			}
		}
		if (va == 0)
			panic("%s: too many preinit mappings", __func__);
	} else {
		/*
		 * If we have a preinit mapping, re-use it.
		 */
		for (i = 0; i < PMAP_PREINIT_MAPPING_COUNT; i++) {
			ppim = pmap_preinit_mapping + i;
			if (ppim->pa == pa && ppim->sz == size &&
			    ppim->mode == mode)
				return ((void *)(ppim->va + offset));
		}
		va = kva_alloc(size);
		if (va == 0)
			panic("%s: Couldn't allocate KVA", __func__);
	}
	for (tmpsize = 0; tmpsize < size; tmpsize += PAGE_SIZE)
		pmap_kenter_attr(va + tmpsize, pa + tmpsize, mode);
	pmap_invalidate_range(kernel_pmap, va, va + tmpsize);
	pmap_invalidate_cache_range(va, va + size, FALSE);
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
	struct pmap_preinit_mapping *ppim;
	vm_offset_t offset;
	int i;

	if (va >= KERNBASE && va + size <= KERNBASE + KERNLOAD)
		return;
	offset = va & PAGE_MASK;
	size = round_page(offset + size);
	va = trunc_page(va);
	for (i = 0; i < PMAP_PREINIT_MAPPING_COUNT; i++) {
		ppim = pmap_preinit_mapping + i;
		if (ppim->va == va && ppim->sz == size) {
			if (pmap_initialized)
				return;
			ppim->pa = 0;
			ppim->va = 0;
			ppim->sz = 0;
			ppim->mode = 0;
			if (va + size == virtual_avail)
				virtual_avail = va;
			return;
		}
	}
	if (pmap_initialized)
		kva_free(va, size);
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
		*sysmaps->CMAP2 = PG_V | PG_RW | VM_PAGE_TO_PHYS(m) |
		    PG_A | PG_M | pmap_cache_bits(m->md.pat_mode, 0);
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
		*sysmaps->CMAP2 = 0;
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
	pd_entry_t *pde;
	pt_entry_t *pte;
	int cache_bits_pte, cache_bits_pde;
	boolean_t changed;

	base = trunc_page(va);
	offset = va & PAGE_MASK;
	size = round_page(offset + size);

	/*
	 * Only supported on kernel virtual addresses above the recursive map.
	 */
	if (base < VM_MIN_KERNEL_ADDRESS)
		return (EINVAL);

	cache_bits_pde = pmap_cache_bits(mode, 1);
	cache_bits_pte = pmap_cache_bits(mode, 0);
	changed = FALSE;

	/*
	 * Pages that aren't mapped aren't supported.  Also break down
	 * 2/4MB pages into 4KB pages if required.
	 */
	PMAP_LOCK(kernel_pmap);
	for (tmpva = base; tmpva < base + size; ) {
		pde = pmap_pde(kernel_pmap, tmpva);
		if (*pde == 0) {
			PMAP_UNLOCK(kernel_pmap);
			return (EINVAL);
		}
		if (*pde & PG_PS) {
			/*
			 * If the current 2/4MB page already has
			 * the required memory type, then we need not
			 * demote this page.  Just increment tmpva to
			 * the next 2/4MB page frame.
			 */
			if ((*pde & PG_PDE_CACHE) == cache_bits_pde) {
				tmpva = trunc_4mpage(tmpva) + NBPDR;
				continue;
			}

			/*
			 * If the current offset aligns with a 2/4MB
			 * page frame and there is at least 2/4MB left
			 * within the range, then we need not break
			 * down this page into 4KB pages.
			 */
			if ((tmpva & PDRMASK) == 0 &&
			    tmpva + PDRMASK < base + size) {
				tmpva += NBPDR;
				continue;
			}
			if (!pmap_demote_pde(kernel_pmap, pde, tmpva)) {
				PMAP_UNLOCK(kernel_pmap);
				return (ENOMEM);
			}
		}
		pte = vtopte(tmpva);
		if (*pte == 0) {
			PMAP_UNLOCK(kernel_pmap);
			return (EINVAL);
		}
		tmpva += PAGE_SIZE;
	}
	PMAP_UNLOCK(kernel_pmap);

	/*
	 * Ok, all the pages exist, so run through them updating their
	 * cache mode if required.
	 */
	for (tmpva = base; tmpva < base + size; ) {
		pde = pmap_pde(kernel_pmap, tmpva);
		if (*pde & PG_PS) {
			if ((*pde & PG_PDE_CACHE) != cache_bits_pde) {
				pmap_pde_attr(pde, cache_bits_pde);
				changed = TRUE;
			}
			tmpva = trunc_4mpage(tmpva) + NBPDR;
		} else {
			pte = vtopte(tmpva);
			if ((*pte & PG_PTE_CACHE) != cache_bits_pte) {
				pmap_pte_attr(pte, cache_bits_pte);
				changed = TRUE;
			}
			tmpva += PAGE_SIZE;
		}
	}

	/*
	 * Flush CPU caches to make sure any data isn't cached that
	 * shouldn't be, etc.
	 */
	if (changed) {
		pmap_invalidate_range(kernel_pmap, base, tmpva);
		pmap_invalidate_cache_range(base, tmpva, FALSE);
	}
	return (0);
}

/*
 * perform the pmap work for mincore
 */
int
pmap_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{
	pd_entry_t *pdep;
	pt_entry_t *ptep, pte;
	vm_paddr_t pa;
	int val;

	PMAP_LOCK(pmap);
retry:
	pdep = pmap_pde(pmap, addr);
	if (*pdep != 0) {
		if (*pdep & PG_PS) {
			pte = *pdep;
			/* Compute the physical address of the 4KB page. */
			pa = ((*pdep & PG_PS_FRAME) | (addr & PDRMASK)) &
			    PG_FRAME;
			val = MINCORE_SUPER;
		} else {
			ptep = pmap_pte(pmap, addr);
			pte = *ptep;
			pmap_pte_release(ptep);
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
#if defined(PAE) || defined(PAE_TABLES)
	cr3 = vtophys(pmap->pm_pdpt);
#else
	cr3 = vtophys(pmap->pm_pdir);
#endif
	/*
	 * pmap_activate is for the current thread on the current cpu
	 */
	td->td_pcb->pcb_cr3 = cr3;
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
							pa = *pte;
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
