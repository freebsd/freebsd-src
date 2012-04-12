/* XXX: Copyright */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_cpu.h"
#include "opt_pmap.h"
#include "opt_smp.h"

#include <sys/param.h>
#include <sys/systm.h>

#ifdef SMP
#include <sys/smp.h>
#endif

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/md_var.h>

#include <xen/hypervisor.h>
#include <machine/xen/xenvar.h>

struct pmap kernel_pmap_store;

vm_offset_t virtual_avail;	/* VA of first avail page (after kernel bss) */
vm_offset_t virtual_end;	/* VA of last avail page (end of kernel AS) */

#ifdef SUPERPAGESUPPORT
static int ndmpdp;
static vm_paddr_t dmaplimit;
#endif /* SUPERPAGESUPPORT */

vm_offset_t kernel_vm_end = VM_MIN_KERNEL_ADDRESS;
pt_entry_t pg_nx; /* XXX: do we need this ? */

struct msgbuf *msgbufp = 0;

static u_int64_t	KPTphys;	/* phys addr of kernel level 1 */
static u_int64_t	KPDphys;	/* phys addr of kernel level 2 */
u_int64_t		KPDPphys;	/* phys addr of kernel level 3 */
u_int64_t		KPML4phys;	/* phys addr of kernel level 4 */

#ifdef SUPERPAGESUPPORT
static u_int64_t	DMPDphys;	/* phys addr of direct mapped level 2 */
static u_int64_t	DMPDPphys;	/* phys addr of direct mapped level 3 */
#endif /* SUPERPAGESUPPORT */

static vm_paddr_t	boot_ptphys;	/* phys addr of start of
					 * kernel bootstrap tables
					 */
static vm_paddr_t	boot_ptendphys;	/* phys addr of end of kernel
					 * bootstrap page tables
					 */

extern uint64_t xenstack; /* The stack Xen gives us at boot */

/* return kernel virtual address of  'n' claimed physical pages at boot. */
static vm_offset_t
vallocpages(vm_paddr_t *firstaddr, int n)
{
	u_int64_t ret;

	ret = *firstaddr + KERNBASE;
	bzero((void *)ret, n * PAGE_SIZE);
	*firstaddr += n * PAGE_SIZE;
	return (ret);
}

/* 
 * At boot, xen guarantees us a 512kB padding area that is passed to
 * us. We must be careful not to spill the tables we create here
 * beyond this.
 */

/* Set page addressed by va to r/o */
static void
pmap_xen_setpages_ro(vm_offset_t va, vm_size_t npages)
{
	vm_size_t i;
	for (i = 0; i < npages; i++) {
		PT_SET_MA(va + PAGE_SIZE * i, 
			  phystomach(VTOP(va + PAGE_SIZE * i)) | PG_U | PG_V);
	}
}

/* Set page addressed by va to r/w */
static void
pmap_xen_setpages_rw(vm_offset_t va, vm_size_t npages)
{
	vm_size_t i;
	for (i = 0; i < npages; i++) {
		PT_SET_MA(va + PAGE_SIZE * i, 
			  phystomach(VTOP(va + PAGE_SIZE * i)) | PG_U | PG_V | PG_RW);
	}
}

extern int etext;	/* End of kernel text (virtual address) */
extern int end;		/* End of kernel binary (virtual address) */
/* Return pte flags according to kernel va access restrictions */
static pt_entry_t
pmap_xen_kernel_vaflags(vm_offset_t va)
{
	/* See: interface/xen.h */
	/* 
	 * i) si->mfn_list 
	 * ii) start info page
	 * iii) bootstrap stack
	 * iv) Everything in and above the "scratch area
	 * Everything else is r/o
	 */

	if ((va > (vm_offset_t) &etext && /* .data, .bss et. al */
	     (va < (vm_offset_t) &end))
	    ||
	    (va > (vm_offset_t)(xen_start_info->pt_base +
	    			xen_start_info->nr_pt_frames * PAGE_SIZE) &&
	     va < xenstack)
	    ||
	    va >= PTOV(boot_ptendphys + 1)) {
		return PG_RW;
	}

	return 0;
}

static void
create_boot_pagetables(vm_paddr_t *firstaddr)
{
	int i;
	int nkpt, nkpdpe;
	int mapspan = (xenstack - KERNBASE + 
		       512 * 1024 + PAGE_SIZE) / PAGE_SIZE;

	boot_ptphys = *firstaddr; /* lowest available r/w area */

	/* Allocate pseudo-physical pages for kernel page tables. */
	nkpt = howmany(mapspan, NPTEPG);
	nkpdpe = howmany(nkpt, NPDEPG);
	KPML4phys = vallocpages(firstaddr, 1);
	KPDPphys = vallocpages(firstaddr, NKPML4E);
	KPDphys = vallocpages(firstaddr, nkpdpe);
	KPTphys = vallocpages(firstaddr, nkpt);

#ifdef SUPERPAGESUPPORT
	int ndm1g;
	ndmpdp = (ptoa(Maxmem) + NBPDP - 1) >> PDPSHIFT;
	if (ndmpdp < 4)		/* Minimum 4GB of dirmap */
		ndmpdp = 4;
	DMPDPphys = vallocpages(firstaddr, NDMPML4E);
	ndm1g = 0;
	if ((amd_feature & AMDID_PAGE1GB) != 0)
		ndm1g = ptoa(Maxmem) >> PDPSHIFT;

	if (ndm1g < ndmpdp)
		DMPDphys = vallocpages(firstaddr, ndmpdp - ndm1g);
	dmaplimit = (vm_paddr_t)ndmpdp << PDPSHIFT;
#endif /* SUPERPAGESUPPORT */


	boot_ptendphys = *firstaddr - 1;

	/* We can't spill over beyond the 512kB padding */
	KASSERT(((boot_ptendphys - boot_ptphys) / 1024) <= 512,
		("bootstrap mapped memory insufficient.\n"));

	/* Fill in the underlying page table pages */
	/* Read-only from zero to physfree */
	/* XXX not fully used, underneath 2M pages */
	for (i = 0; (i << PAGE_SHIFT) < (mapspan * PAGE_SIZE); i++) {
		((pt_entry_t *)KPTphys)[i] = phystomach(i << PAGE_SHIFT);
		((pt_entry_t *)KPTphys)[i] |= PG_V | PG_G | PG_U;
		((pt_entry_t *)KPTphys)[i] |= 
			pmap_xen_kernel_vaflags(PTOV(i << PAGE_SHIFT));	
	}
	
	pmap_xen_setpages_ro(KPTphys, (i - 1)/ NPTEPG + 1);

	/* Now map the page tables at their location within PTmap */
	for (i = 0; i < nkpt; i++) {
		((pd_entry_t *)KPDphys)[i] = phystomach(VTOP(KPTphys) +
							(i << PAGE_SHIFT));
		((pd_entry_t *)KPDphys)[i] |= PG_RW | PG_V | PG_U;

	}

	pmap_xen_setpages_ro(KPDphys, (nkpt - 1) / NPDEPG + 1);

#ifdef SUPERPAGESUPPORT /* XXX: work out r/o overlaps and 2M machine pages*/
	/* Map from zero to end of allocations under 2M pages */
	/* This replaces some of the KPTphys entries above */
	for (i = 0; (i << PDRSHIFT) < *firstaddr; i++) {
		((pd_entry_t *)KPDphys)[i] = phystomach(i << PDRSHIFT);
		((pd_entry_t *)KPDphys)[i] |= PG_U | PG_RW | PG_V | PG_PS | PG_G;
	}
#endif

	/* And connect up the PD to the PDP */
	for (i = 0; i < nkpdpe; i++) {
		((pdp_entry_t *)KPDPphys)[i + KPDPI] = phystomach(VTOP(KPDphys) +
			(i << PAGE_SHIFT));
		((pdp_entry_t *)KPDPphys)[i + KPDPI] |= PG_RW | PG_V | PG_U;
	}

	pmap_xen_setpages_ro(KPDPphys, (nkpdpe - 1) / NPDPEPG + 1);

#ifdef SUPERPAGESUPPORT
	int j;

	/*
	 * Now, set up the direct map region using 2MB and/or 1GB pages.  If
	 * the end of physical memory is not aligned to a 1GB page boundary,
	 * then the residual physical memory is mapped with 2MB pages.  Later,
	 * if pmap_mapdev{_attr}() uses the direct map for non-write-back
	 * memory, pmap_change_attr() will demote any 2MB or 1GB page mappings
	 * that are partially used. 
	 */

	for (i = NPDEPG * ndm1g, j = 0; i < NPDEPG * ndmpdp; i++, j++) {
		if ((i << PDRSHIFT) > ptoa(Maxmem)) {
			/* 
			 * Since the page is zeroed out at
			 * vallocpages(), the remaining ptes will be
			 * invalid.
			 */
			 
			break;
		}
		((pd_entry_t *)DMPDphys)[j] = (vm_paddr_t)(phystomach(i << PDRSHIFT));
		/* Preset PG_M and PG_A because demotion expects it. */
		((pd_entry_t *)DMPDphys)[j] |= PG_U | PG_V | PG_PS /* | PG_G */ |
		    PG_M | PG_A;
	}
	/* Mark pages R/O */
	pmap_xen_setpages_ro(DMPDphys, ndmpdp - ndm1g);

	/* Setup 1G pages, if available */
	for (i = 0; i < ndm1g; i++) {
		if ((i << PDPSHIFT) > ptoa(Maxmem)) {
			/* 
			 * Since the page is zeroed out at
			 * vallocpages(), the remaining ptes will be
			 * invalid.
			 */
			 
			break;
		}

		((pdp_entry_t *)DMPDPphys)[i] = (vm_paddr_t)phystomach(i << PDPSHIFT);
		/* Preset PG_M and PG_A because demotion expects it. */
		((pdp_entry_t *)DMPDPphys)[i] |= PG_U | PG_V | PG_PS | PG_G |
		    PG_M | PG_A;
	}

	for (j = 0; i < ndmpdp; i++, j++) {
		((pdp_entry_t *)DMPDPphys)[i] = phystomach(VTOP(DMPDphys) + (j << PAGE_SHIFT));
		((pdp_entry_t *)DMPDPphys)[i] |= PG_V | PG_U;
	}

	pmap_xen_setpages_ro(DMPDPphys, NDMPML4E);

	/* Connect the Direct Map slot(s) up to the PML4. */
	for (i = 0; i < NDMPML4E; i++) {
		((pdp_entry_t *)KPML4phys)[DMPML4I + i] = phystomach(VTOP(DMPDPphys) +
			(i << PAGE_SHIFT));
		((pdp_entry_t *)KPML4phys)[DMPML4I + i] |= PG_V | PG_U;
	}
#endif /* SUPERPAGESUPPORT */

	/* And recursively map PML4 to itself in order to get PTmap */
	((pdp_entry_t *)KPML4phys)[PML4PML4I] = phystomach(VTOP(KPML4phys));
	((pdp_entry_t *)KPML4phys)[PML4PML4I] |= PG_V | PG_U;

	/* Connect the KVA slot up to the PML4 */
	((pdp_entry_t *)KPML4phys)[KPML4I] = phystomach(VTOP(KPDPphys));
	((pdp_entry_t *)KPML4phys)[KPML4I] |= PG_RW | PG_V | PG_U;

	pmap_xen_setpages_ro(KPML4phys, 1);

	xen_pgdir_pin(phystomach(VTOP(KPML4phys)));
}

/* 
 * Note: pmap_xen_bootpages assumes and asserts for the fact that the
 * kernel virtual start and end values have been initialised.
 *
 * Map in the xen provided shared pages. They are:
 * - shared info page
 * - console page (XXX:)
 * - XXX:
 */

static void
pmap_xen_bootpages(vm_paddr_t *firstaddr)
{
	vm_offset_t va;
	vm_paddr_t ma;

	KASSERT(virtual_avail != 0, 
		("kernel virtual address space un-initialised!"));
	KASSERT(virtual_avail >= (KERNBASE + physmem), 
		("kernel virtual address space inconsistent!"));

	/* Share info */
	ma = xen_start_info->shared_info;

	/* This is a bit of a hack right now - we waste a physical
	 * page by overwriting its original mapping to point to
	 * the page we want ( thereby losing access to the
	 * original page ).
	 *
	 * The clean solution would have been to map it in at 
	 * KERNBASE + pa, where pa is the "pseudo-physical" address of
	 * the shared page that xen gives us. We can't seem to be able
	 * to use the pseudo-physical address in this way because the
	 * linear mapped virtual address seems to be outside of the
	 * range of PTEs that we have available during bootup (ptes
	 * take virtual address space which is limited to under 
	 * (512KB - (kernal binaries, stack et al.)) during xen
	 * bootup).
	 */

	va = vallocpages(firstaddr, 1);
	PT_SET_MA(va, ma | PG_RW | PG_V | PG_U);


	HYPERVISOR_shared_info = (void *) va;
}

void
pmap_bootstrap(vm_paddr_t *firstaddr)
{

	create_boot_pagetables(firstaddr);

	/* Switch to the new kernel tables */
	xen_pt_switch(VTOP(KPML4phys));

	/* Unpin old page table hierarchy, and mark all its pages r/w */
	xen_pgdir_unpin(phystomach(VTOP(xen_start_info->pt_base)));
	pmap_xen_setpages_rw(xen_start_info->pt_base,
			     xen_start_info->nr_pt_frames);

	/* 
	 * gc newly free pages (bootstrap PTs and bootstrap stack,
	 * mostly, I think.).
	 */
	virtual_avail = (vm_offset_t) KERNBASE + *firstaddr;
	virtual_end = VM_MAX_KERNEL_ADDRESS; /* XXX: Check we don't
						overlap xen pgdir entries. */

	/* Map in Xen related pages into VA space */
	pmap_xen_bootpages(firstaddr);

}

void
pmap_page_init(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_growkernel(vm_offset_t addr)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_init(void)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_pinit0(pmap_t pmap)
{
	KASSERT(0, ("XXX: TODO\n"));
}

int
pmap_pinit(pmap_t pmap)
{
	KASSERT(0, ("XXX: TODO\n"));
	return -1;
}

void
pmap_release(pmap_t pmap)
{
	KASSERT(0, ("XXX: TODO\n"));
}

__inline pt_entry_t *
vtopte(vm_offset_t va)
{
	KASSERT(0, ("XXX: REVIEW\n"));
	u_int64_t mask = ((1ul << (NPTEPGSHIFT + NPDEPGSHIFT + NPDPEPGSHIFT + NPML4EPGSHIFT)) - 1);

	return (PTmap + ((va >> PAGE_SHIFT) & mask));
}

#ifdef SMP
void pmap_lazyfix_action(void);

void
pmap_lazyfix_action(void)
{
	KASSERT(0, ("XXX: TODO\n"));
}
#endif /* SMP */

void
pmap_qenter(vm_offset_t sva, vm_page_t *ma, int count)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_qremove(vm_offset_t sva, int count)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_enter(pmap_t pmap, vm_offset_t va, vm_prot_t access, vm_page_t m,
    vm_prot_t prot, boolean_t wired)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_enter_object(pmap_t pmap, vm_offset_t start, vm_offset_t end,
    vm_page_t m_start, vm_prot_t prot)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_enter_quick(pmap_t pmap, vm_offset_t va, vm_page_t m, vm_prot_t prot)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void *
pmap_kenter_temporary(vm_paddr_t pa, int i)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_object_init_pt(pmap_t pmap, vm_offset_t addr,
		    vm_object_t object, vm_pindex_t pindex,
		    vm_size_t size)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_remove(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_remove_all(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
}

vm_paddr_t 
pmap_extract(pmap_t pmap, vm_offset_t va)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

vm_page_t
pmap_extract_and_hold(pmap_t pmap, vm_offset_t va, vm_prot_t prot)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

void 
pmap_kenter(vm_offset_t va, vm_paddr_t pa)
{
	KASSERT(0, ("XXX: TODO\n"));
}

__inline void
pmap_kremove(vm_offset_t va)
{
	KASSERT(0, ("XXX: TODO\n"));
}

vm_offset_t
pmap_map(vm_offset_t *virt, vm_paddr_t start, vm_paddr_t end, int prot)
{
	KASSERT(0, ("XXX: TODO\n"));
	return -1;
}

void
pmap_protect(pmap_t pmap, vm_offset_t sva, vm_offset_t eva, vm_prot_t prot)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_invalidate_range(pmap_t pmap, vm_offset_t sva, vm_offset_t eva)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_change_wiring(pmap_t pmap, vm_offset_t va, boolean_t wired)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_copy(pmap_t dst_pmap, pmap_t src_pmap, vm_offset_t dst_addr, 
	  vm_size_t len, vm_offset_t src_addr)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_copy_page(vm_page_t src, vm_page_t dst)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_zero_page(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_zero_page_area(vm_page_t m, int off, int size)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_zero_page_idle(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_activate(struct thread *td)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_remove_pages(pmap_t pmap)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_page_set_memattr(vm_page_t m, vm_memattr_t ma)
{
	KASSERT(0, ("XXX: TODO\n"));
}

boolean_t
pmap_page_is_mapped(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

boolean_t
pmap_page_exists_quick(pmap_t pmap, vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

int
pmap_page_wired_mappings(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
	return -1;
}

boolean_t
pmap_is_modified(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

boolean_t
pmap_is_referenced(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

boolean_t
pmap_is_prefaultable(pmap_t pmap, vm_offset_t addr)
{
	KASSERT(0, ("XXX: TODO\n"));
	return 0;
}

void
pmap_clear_modify(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_clear_reference(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_remove_write(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
}

int
pmap_ts_referenced(vm_page_t m)
{
	KASSERT(0, ("XXX: TODO\n"));
	return -1;
}

void
pmap_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz)
{
	KASSERT(0, ("XXX: TODO\n"));
}

void
pmap_align_superpage(vm_object_t object, vm_ooffset_t offset,
    vm_offset_t *addr, vm_size_t size)
{
	KASSERT(0, ("XXX: TODO\n"));
}

int
pmap_mincore(pmap_t pmap, vm_offset_t addr, vm_paddr_t *locked_pa)
{	
	KASSERT(0, ("XXX: TODO\n"));
	return -1;
}

void *
pmap_mapdev(vm_paddr_t pa, vm_size_t size)
{
  	KASSERT(0, ("XXX: TODO\n"));
	return NULL;
}

void
pmap_unmapdev(vm_offset_t va, vm_size_t size)
{
	KASSERT(0, ("XXX: TODO\n"));
}

int
pmap_change_attr(vm_offset_t va, vm_size_t size, int mode)
{
		KASSERT(0, ("XXX: TODO\n"));
		return -1;
}
