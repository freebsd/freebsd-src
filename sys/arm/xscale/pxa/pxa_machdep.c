/*	$NetBSD: hpc_machdep.c,v 1.70 2003/09/16 08:18:22 agc Exp $	*/

/*-
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *      This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * machdep.c
 *
 * Machine dependant functions for kernel setup
 *
 * This file needs a lot of work.
 *
 * Created      : 17/09/94
 */

#include "opt_ddb.h"
#include "opt_kstack_pages.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/signalvar.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/linker.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/cons.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/kdb.h>
#include <sys/msgbuf.h>
#include <machine/reg.h>
#include <machine/cpu.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <machine/devmap.h>
#include <machine/vmparam.h>
#include <machine/pcb.h>
#include <machine/undefined.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/armreg.h>
#include <machine/bus.h>
#include <machine/physmem.h>
#include <sys/reboot.h>

#include <arm/xscale/pxa/pxareg.h>
#include <arm/xscale/pxa/pxavar.h>

#define KERNEL_PT_SYS		0	/* Page table for mapping proc0 zero page */
#define	KERNEL_PT_IOPXS		1
#define KERNEL_PT_BEFOREKERN	2
#define KERNEL_PT_AFKERNEL	3	/* L2 table for mapping after kernel */
#define	KERNEL_PT_AFKERNEL_NUM	9

/* this should be evenly divisable by PAGE_SIZE / L2_TABLE_SIZE_REAL (or 4) */
#define NUM_KERNEL_PTS		(KERNEL_PT_AFKERNEL + KERNEL_PT_AFKERNEL_NUM)

struct pv_addr kernel_pt_table[NUM_KERNEL_PTS];

/* Physical and virtual addresses for some global pages */

struct pv_addr systempage;
struct pv_addr msgbufpv;
struct pv_addr irqstack;
struct pv_addr undstack;
struct pv_addr abtstack;
struct pv_addr kernelstack;
struct pv_addr minidataclean;

static void	pxa_probe_sdram(bus_space_tag_t, bus_space_handle_t,
		    uint32_t *, uint32_t *);

/* Static device mappings. */
static const struct arm_devmap_entry pxa_devmap[] = {
	/*
	 * Map the on-board devices up into the KVA region so we don't muck
	 * up user-space.
	 */
	{
		PXA2X0_PERIPH_START + PXA2X0_PERIPH_OFFSET,
		PXA2X0_PERIPH_START,
		PXA250_PERIPH_END - PXA2X0_PERIPH_START,
		VM_PROT_READ|VM_PROT_WRITE,
		PTE_DEVICE,
	},
	{ 0, 0, 0, 0, 0, }
};

#define SDRAM_START 0xa0000000

extern vm_offset_t xscale_cache_clean_addr;

void *
initarm(struct arm_boot_params *abp)
{
	struct pv_addr  kernel_l1pt;
	struct pv_addr  dpcpu;
	int loop;
	u_int l1pagetable;
	vm_offset_t freemempos;
	vm_offset_t freemem_pt;
	vm_offset_t afterkern;
	vm_offset_t freemem_after;
	vm_offset_t lastaddr;
	int i, j;
	uint32_t memsize[PXA2X0_SDRAM_BANKS], memstart[PXA2X0_SDRAM_BANKS];

	lastaddr = parse_boot_param(abp);
	arm_physmem_kernaddr = abp->abp_physaddr;
	set_cpufuncs();
	pcpu_init(pcpup, 0, sizeof(struct pcpu));
	PCPU_SET(curthread, &thread0);

	/* Do basic tuning, hz etc */
	init_param1();

	freemempos = 0xa0200000;
	/* Define a macro to simplify memory allocation */
#define	valloc_pages(var, np)			\
	alloc_pages((var).pv_pa, (np));		\
	(var).pv_va = (var).pv_pa + 0x20000000;

#define alloc_pages(var, np)			\
	freemempos -= (np * PAGE_SIZE);		\
	(var) = freemempos;		\
	memset((char *)(var), 0, ((np) * PAGE_SIZE));

	while (((freemempos - L1_TABLE_SIZE) & (L1_TABLE_SIZE - 1)) != 0)
		freemempos -= PAGE_SIZE;
	valloc_pages(kernel_l1pt, L1_TABLE_SIZE / PAGE_SIZE);
	for (loop = 0; loop < NUM_KERNEL_PTS; ++loop) {
		if (!(loop % (PAGE_SIZE / L2_TABLE_SIZE_REAL))) {
			valloc_pages(kernel_pt_table[loop],
			    L2_TABLE_SIZE / PAGE_SIZE);
		} else {
			kernel_pt_table[loop].pv_pa = freemempos +
			    (loop % (PAGE_SIZE / L2_TABLE_SIZE_REAL)) *
			    L2_TABLE_SIZE_REAL;
			kernel_pt_table[loop].pv_va =
			    kernel_pt_table[loop].pv_pa + 0x20000000;
		}
	}
	freemem_pt = freemempos;
	freemempos = 0xa0100000;
	/*
	 * Allocate a page for the system page mapped to V0x00000000
	 * This page will just contain the system vectors and can be
	 * shared by all processes.
	 */
	valloc_pages(systempage, 1);

	/* Allocate dynamic per-cpu area. */
	valloc_pages(dpcpu, DPCPU_SIZE / PAGE_SIZE);
	dpcpu_init((void *)dpcpu.pv_va, 0);

	/* Allocate stacks for all modes */
	valloc_pages(irqstack, IRQ_STACK_SIZE);
	valloc_pages(abtstack, ABT_STACK_SIZE);
	valloc_pages(undstack, UND_STACK_SIZE);
	valloc_pages(kernelstack, kstack_pages);
	alloc_pages(minidataclean.pv_pa, 1);
	valloc_pages(msgbufpv, round_page(msgbufsize) / PAGE_SIZE);
	/*
	 * Allocate memory for the l1 and l2 page tables. The scheme to avoid
	 * wasting memory by allocating the l1pt on the first 16k memory was
	 * taken from NetBSD rpc_machdep.c. NKPT should be greater than 12 for
	 * this to work (which is supposed to be the case).
	 */

	/*
	 * Now we start construction of the L1 page table
	 * We start by mapping the L2 page tables into the L1.
	 * This means that we can replace L1 mappings later on if necessary
	 */
	l1pagetable = kernel_l1pt.pv_va;

	/* Map the L2 pages tables in the L1 page table */
	pmap_link_l2pt(l1pagetable, ARM_VECTORS_HIGH & ~(0x00100000 - 1),
	    &kernel_pt_table[KERNEL_PT_SYS]);
#if 0 /* XXXBJR: What is this?  Don't know if there's an analogue. */
	pmap_link_l2pt(l1pagetable, IQ80321_IOPXS_VBASE,
	                &kernel_pt_table[KERNEL_PT_IOPXS]);
#endif
	pmap_link_l2pt(l1pagetable, KERNBASE,
	    &kernel_pt_table[KERNEL_PT_BEFOREKERN]);
	pmap_map_chunk(l1pagetable, KERNBASE, SDRAM_START, 0x100000,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, KERNBASE + 0x100000, SDRAM_START + 0x100000,
	    0x100000, VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);
	pmap_map_chunk(l1pagetable, KERNBASE + 0x200000, SDRAM_START + 0x200000,
	   (((uint32_t)(lastaddr) - KERNBASE - 0x200000) + L1_S_SIZE) & ~(L1_S_SIZE - 1),
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	freemem_after = ((int)lastaddr + PAGE_SIZE) & ~(PAGE_SIZE - 1);
	afterkern = round_page(((vm_offset_t)lastaddr + L1_S_SIZE) &
	    ~(L1_S_SIZE - 1));
	for (i = 0; i < KERNEL_PT_AFKERNEL_NUM; i++) {
		pmap_link_l2pt(l1pagetable, afterkern + i * 0x00100000,
		    &kernel_pt_table[KERNEL_PT_AFKERNEL + i]);
	}
	pmap_map_entry(l1pagetable, afterkern, minidataclean.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);


	/* Map the Mini-Data cache clean area. */
	xscale_setup_minidata(l1pagetable, afterkern,
	    minidataclean.pv_pa);

	/* Map the vector page. */
	pmap_map_entry(l1pagetable, ARM_VECTORS_HIGH, systempage.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	arm_devmap_bootstrap(l1pagetable, pxa_devmap);

	/*
	 * Give the XScale global cache clean code an appropriately
	 * sized chunk of unmapped VA space starting at 0xff000000
	 * (our device mappings end before this address).
	 */
	xscale_cache_clean_addr = 0xff000000U;

	cpu_domains((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2)) | DOMAIN_CLIENT);
	setttb(kernel_l1pt.pv_pa);
	cpu_tlb_flushID();
	cpu_domains(DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2));

	/*
	 * Pages were allocated during the secondary bootstrap for the
	 * stacks for different CPU modes.
	 * We must now set the r13 registers in the different CPU modes to
	 * point to these stacks.
	 * Since the ARM stacks use STMFD etc. we must set r13 to the top end
	 * of the stack memory.
	 */
	set_stackptrs(0);

	/*
	 * We must now clean the cache again....
	 * Cleaning may be done by reading new data to displace any
	 * dirty data in the cache. This will have happened in setttb()
	 * but since we are boot strapping the addresses used for the read
	 * may have just been remapped and thus the cache could be out
	 * of sync. A re-clean after the switch will cure this.
	 * After booting there are no gross relocations of the kernel thus
	 * this problem will not occur after initarm().
	 */
	cpu_idcache_wbinv_all();
	cpu_setup();

	/*
	 * Sort out bus_space for on-board devices.
	 */
	pxa_obio_tag_init();

	/*
	 * Fetch the SDRAM start/size from the PXA2X0 SDRAM configration
	 * registers.
	 */
	pxa_probe_sdram(obio_tag, PXA2X0_MEMCTL_BASE, memstart, memsize);

	/* Fire up consoles. */
	cninit();

	undefined_init();

	init_proc0(kernelstack.pv_va);

	/* Enable MMU, I-cache, D-cache, write buffer. */
	arm_vector_init(ARM_VECTORS_HIGH, ARM_VEC_ALL);

	pmap_curmaxkvaddr = afterkern + PAGE_SIZE;
	vm_max_kernel_address = 0xe0000000;
	pmap_bootstrap(pmap_curmaxkvaddr, &kernel_l1pt);
	msgbufp = (void*)msgbufpv.pv_va;
	msgbufinit(msgbufp, msgbufsize);
	mutex_init();

	/*
	 * Add the physical ram we have available.
	 *
	 * Exclude the kernel (and all the things we allocated which immediately
	 * follow the kernel) from the VM allocation pool but not from crash
	 * dumps.  virtual_avail is a global variable which tracks the kva we've
	 * "allocated" while setting up pmaps.
	 *
	 * Prepare the list of physical memory available to the vm subsystem.
	 */
	for (j = 0; j < PXA2X0_SDRAM_BANKS; j++) {
		if (memsize[j] > 0)
			arm_physmem_hardware_region(memstart[j], memsize[j]);
	}
	arm_physmem_exclude_region(freemem_pt, KERNPHYSADDR -
	    freemem_pt, EXFLAG_NOALLOC);
	arm_physmem_exclude_region(freemempos, KERNPHYSADDR - 0x100000 -
	    freemempos, EXFLAG_NOALLOC);
	arm_physmem_exclude_region(abp->abp_physaddr, 
	    virtual_avail - KERNVIRTADDR, EXFLAG_NOALLOC);
	arm_physmem_init_kernel_globals();

	init_param2(physmem);
	kdb_init();
	return ((void *)(kernelstack.pv_va + USPACE_SVC_STACK_TOP -
	    sizeof(struct pcb)));
}

static void
pxa_probe_sdram(bus_space_tag_t bst, bus_space_handle_t bsh,
    uint32_t *memstart, uint32_t *memsize)
{
	uint32_t	mdcnfg, dwid, dcac, drac, dnb;
	int		i;

	mdcnfg = bus_space_read_4(bst, bsh, MEMCTL_MDCNFG);

	/*
	 * Scan all 4 SDRAM banks
	 */
	for (i = 0; i < PXA2X0_SDRAM_BANKS; i++) {
		memstart[i] = 0;
		memsize[i] = 0;

		switch (i) {
		case 0:
		case 1:
			if ((i == 0 && (mdcnfg & MDCNFG_DE0) == 0) ||
			    (i == 1 && (mdcnfg & MDCNFG_DE1) == 0))
				continue;
			dwid = mdcnfg >> MDCNFD_DWID01_SHIFT;
			dcac = mdcnfg >> MDCNFD_DCAC01_SHIFT;
			drac = mdcnfg >> MDCNFD_DRAC01_SHIFT;
			dnb = mdcnfg >> MDCNFD_DNB01_SHIFT;
			break;

		case 2:
		case 3:
			if ((i == 2 && (mdcnfg & MDCNFG_DE2) == 0) ||
			    (i == 3 && (mdcnfg & MDCNFG_DE3) == 0))
				continue;
			dwid = mdcnfg >> MDCNFD_DWID23_SHIFT;
			dcac = mdcnfg >> MDCNFD_DCAC23_SHIFT;
			drac = mdcnfg >> MDCNFD_DRAC23_SHIFT;
			dnb = mdcnfg >> MDCNFD_DNB23_SHIFT;
			break;
		default:
			panic("pxa_probe_sdram: impossible");
		}

		dwid = 2 << (1 - (dwid & MDCNFD_DWID_MASK));  /* 16/32 width */
		dcac = 1 << ((dcac & MDCNFD_DCAC_MASK) + 8);  /* 8-11 columns */
		drac = 1 << ((drac & MDCNFD_DRAC_MASK) + 11); /* 11-13 rows */
		dnb = 2 << (dnb & MDCNFD_DNB_MASK);	      /* # of banks */

		memsize[i] = dwid * dcac * drac * dnb;
		memstart[i] = PXA2X0_SDRAM0_START +
		    (i * PXA2X0_SDRAM_BANK_SIZE);
	}
}

#define	TIMER_FREQUENCY	3686400
#define	UNIMPLEMENTED	panic("%s: unimplemented", __func__)

/* XXXBJR: Belongs with DELAY in a timer.c of some sort. */
void
cpu_startprofclock(void)
{
	UNIMPLEMENTED;
}

void
cpu_stopprofclock(void)
{
	UNIMPLEMENTED;
}

static struct arm32_dma_range pxa_range = {
	.dr_sysbase = 0,
	.dr_busbase = 0,
	.dr_len = ~0u,
};

struct arm32_dma_range *
bus_dma_get_range(void)
{

	return (&pxa_range);
}

int
bus_dma_get_range_nb(void)
{

	return (1);
}
