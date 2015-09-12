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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kstack_pages.h"

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
#include <machine/physmem.h>
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
#include <sys/reboot.h>

#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

#define KERNEL_PT_SYS		0	/* Page table for mapping proc0 zero page */
#define	KERNEL_PT_IO		1
#define KERNEL_PT_IO_NUM	3
#define KERNEL_PT_BEFOREKERN	KERNEL_PT_IO + KERNEL_PT_IO_NUM
#define KERNEL_PT_AFKERNEL	KERNEL_PT_BEFOREKERN + 1	/* L2 table for mapping after kernel */
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

/* Static device mappings. */
static const struct arm_devmap_entry ixp425_devmap[] = {
	/* Physical/Virtual address for I/O space */
    { IXP425_IO_VBASE, IXP425_IO_HWBASE, IXP425_IO_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* Expansion Bus */
    { IXP425_EXP_VBASE, IXP425_EXP_HWBASE, IXP425_EXP_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* CFI Flash on the Expansion Bus */
    { IXP425_EXP_BUS_CS0_VBASE, IXP425_EXP_BUS_CS0_HWBASE,
      IXP425_EXP_BUS_CS0_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* IXP425 PCI Configuration */
    { IXP425_PCI_VBASE, IXP425_PCI_HWBASE, IXP425_PCI_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* SDRAM Controller */
    { IXP425_MCU_VBASE, IXP425_MCU_HWBASE, IXP425_MCU_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* PCI Memory Space */
    { IXP425_PCI_MEM_VBASE, IXP425_PCI_MEM_HWBASE, IXP425_PCI_MEM_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* Q-Mgr Memory Space */
    { IXP425_QMGR_VBASE, IXP425_QMGR_HWBASE, IXP425_QMGR_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

    { 0 },
};

/* Static device mappings. */
static const struct arm_devmap_entry ixp435_devmap[] = {
	/* Physical/Virtual address for I/O space */
    { IXP425_IO_VBASE, IXP425_IO_HWBASE, IXP425_IO_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

    { IXP425_EXP_VBASE, IXP425_EXP_HWBASE, IXP425_EXP_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* IXP425 PCI Configuration */
    { IXP425_PCI_VBASE, IXP425_PCI_HWBASE, IXP425_PCI_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* DDRII Controller NB: mapped same place as IXP425 */
    { IXP425_MCU_VBASE, IXP435_MCU_HWBASE, IXP425_MCU_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* PCI Memory Space */
    { IXP425_PCI_MEM_VBASE, IXP425_PCI_MEM_HWBASE, IXP425_PCI_MEM_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* Q-Mgr Memory Space */
    { IXP425_QMGR_VBASE, IXP425_QMGR_HWBASE, IXP425_QMGR_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* CFI Flash on the Expansion Bus */
    { IXP425_EXP_BUS_CS0_VBASE, IXP425_EXP_BUS_CS0_HWBASE,
      IXP425_EXP_BUS_CS0_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* USB1 Memory Space */
    { IXP435_USB1_VBASE, IXP435_USB1_HWBASE, IXP435_USB1_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },
	/* USB2 Memory Space */
    { IXP435_USB2_VBASE, IXP435_USB2_HWBASE, IXP435_USB2_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* GPS Memory Space */
    { CAMBRIA_GPS_VBASE, CAMBRIA_GPS_HWBASE, CAMBRIA_GPS_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

	/* RS485 Memory Space */
    { CAMBRIA_RS485_VBASE, CAMBRIA_RS485_HWBASE, CAMBRIA_RS485_SIZE,
      VM_PROT_READ|VM_PROT_WRITE, PTE_DEVICE, },

    { 0 }
};

extern vm_offset_t xscale_cache_clean_addr;

void *
initarm(struct arm_boot_params *abp)
{
#define	next_chunk2(a,b)	(((a) + (b)) &~ ((b)-1))
#define	next_page(a)		next_chunk2(a,PAGE_SIZE)
	struct pv_addr  kernel_l1pt;
	struct pv_addr  dpcpu;
	int loop, i;
	u_int l1pagetable;
	vm_offset_t freemempos;
	vm_offset_t freemem_pt;
	vm_offset_t afterkern;
	vm_offset_t freemem_after;
	vm_offset_t lastaddr;
	uint32_t memsize;

	/* kernel text starts where we were loaded at boot */
#define	KERNEL_TEXT_OFF		(abp->abp_physaddr  - PHYSADDR)
#define	KERNEL_TEXT_BASE	(KERNBASE + KERNEL_TEXT_OFF)
#define	KERNEL_TEXT_PHYS	(PHYSADDR + KERNEL_TEXT_OFF)

	lastaddr = parse_boot_param(abp);
	arm_physmem_kernaddr = abp->abp_physaddr;
	set_cpufuncs();		/* NB: sets cputype */
	pcpu_init(pcpup, 0, sizeof(struct pcpu));
	PCPU_SET(curthread, &thread0);

	if (envmode == 1)
		kern_envp = static_env;
	/* Do basic tuning, hz etc */
      	init_param1();
		
	/*
	 * We allocate memory downwards from where we were loaded
	 * by RedBoot; first the L1 page table, then NUM_KERNEL_PTS
	 * entries in the L2 page table.  Past that we re-align the
	 * allocation boundary so later data structures (stacks, etc)
	 * can be mapped with different attributes (write-back vs
	 * write-through).  Note this leaves a gap for expansion
	 * (or might be repurposed).
	 */
	freemempos = abp->abp_physaddr;

	/* macros to simplify initial memory allocation */
#define alloc_pages(var, np) do {					\
	freemempos -= (np * PAGE_SIZE);					\
	(var) = freemempos;						\
	/* NB: this works because locore maps PA=VA */			\
	memset((char *)(var), 0, ((np) * PAGE_SIZE));			\
} while (0)
#define	valloc_pages(var, np) do {					\
	alloc_pages((var).pv_pa, (np));					\
	(var).pv_va = (var).pv_pa + (KERNVIRTADDR - abp->abp_physaddr);	\
} while (0)

	/* force L1 page table alignment */
	while (((freemempos - L1_TABLE_SIZE) & (L1_TABLE_SIZE - 1)) != 0)
		freemempos -= PAGE_SIZE;
	/* allocate contiguous L1 page table */
	valloc_pages(kernel_l1pt, L1_TABLE_SIZE / PAGE_SIZE);
	/* now allocate L2 page tables; they are linked to L1 below */
	for (loop = 0; loop < NUM_KERNEL_PTS; ++loop) {
		if (!(loop % (PAGE_SIZE / L2_TABLE_SIZE_REAL))) {
			valloc_pages(kernel_pt_table[loop],
			    L2_TABLE_SIZE / PAGE_SIZE);
		} else {
			kernel_pt_table[loop].pv_pa = freemempos +
			    (loop % (PAGE_SIZE / L2_TABLE_SIZE_REAL)) *
			    L2_TABLE_SIZE_REAL;
			kernel_pt_table[loop].pv_va =
			    kernel_pt_table[loop].pv_pa +
				(KERNVIRTADDR - abp->abp_physaddr);
		}
	}
	freemem_pt = freemempos;		/* base of allocated pt's */

	/*
	 * Re-align allocation boundary so we can map the area
	 * write-back instead of write-through for the stacks and
	 * related structures allocated below.
	 */
	freemempos = PHYSADDR + 0x100000;
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
	valloc_pages(kernelstack, KSTACK_PAGES);
	alloc_pages(minidataclean.pv_pa, 1);
	valloc_pages(msgbufpv, round_page(msgbufsize) / PAGE_SIZE);

	/*
	 * Now construct the L1 page table.  First map the L2
	 * page tables into the L1 so we can replace L1 mappings
	 * later on if necessary
	 */
	l1pagetable = kernel_l1pt.pv_va;

	/* Map the L2 pages tables in the L1 page table */
	pmap_link_l2pt(l1pagetable, ARM_VECTORS_HIGH & ~(0x00100000 - 1),
	    &kernel_pt_table[KERNEL_PT_SYS]);
	pmap_link_l2pt(l1pagetable, IXP425_IO_VBASE,
	    &kernel_pt_table[KERNEL_PT_IO]);
	pmap_link_l2pt(l1pagetable, IXP425_MCU_VBASE,
	    &kernel_pt_table[KERNEL_PT_IO + 1]);
	pmap_link_l2pt(l1pagetable, IXP425_PCI_MEM_VBASE,
	    &kernel_pt_table[KERNEL_PT_IO + 2]);
	pmap_link_l2pt(l1pagetable, KERNBASE,
	    &kernel_pt_table[KERNEL_PT_BEFOREKERN]);
	pmap_map_chunk(l1pagetable, KERNBASE, PHYSADDR, 0x100000,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, KERNBASE + 0x100000, PHYSADDR + 0x100000,
	    0x100000, VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);
	pmap_map_chunk(l1pagetable, KERNEL_TEXT_BASE, KERNEL_TEXT_PHYS,
	    next_chunk2(((uint32_t)lastaddr) - KERNEL_TEXT_BASE, L1_S_SIZE),
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	freemem_after = next_page((int)lastaddr);
	afterkern = round_page(next_chunk2((vm_offset_t)lastaddr, L1_S_SIZE));
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
	if (cpu_is_ixp43x())
		arm_devmap_bootstrap(l1pagetable, ixp435_devmap);
	else
		arm_devmap_bootstrap(l1pagetable, ixp425_devmap);
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

	/* ready to setup the console (XXX move earlier if possible) */
	cninit();
	/*
	 * Fetch the RAM size from the MCU registers.  The
	 * expansion bus was mapped above so we can now read 'em.
	 */
	if (cpu_is_ixp43x())
		memsize = ixp435_ddram_size();
	else
		memsize = ixp425_sdram_size();

	undefined_init();

	init_proc0(kernelstack.pv_va);

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
	 * Exclude the kernel, and all the things we allocated which immediately
	 * follow the kernel, from the VM allocation pool but not from crash
	 * dumps.  virtual_avail is a global variable which tracks the kva we've
	 * "allocated" while setting up pmaps.
	 *
	 * Prepare the list of physical memory available to the vm subsystem.
	 */
	arm_physmem_hardware_region(PHYSADDR, memsize);
	arm_physmem_exclude_region(freemem_pt, KERNPHYSADDR -
	    freemem_pt, EXFLAG_NOALLOC);
	arm_physmem_exclude_region(freemempos, KERNPHYSADDR - 0x100000 -
	    freemempos, EXFLAG_NOALLOC);
	arm_physmem_exclude_region(abp->abp_physaddr, 
	    virtual_avail - KERNVIRTADDR, EXFLAG_NOALLOC);
	arm_physmem_init_kernel_globals();

	init_param2(physmem);
	kdb_init();

	/* use static kernel environment if so configured */
	if (envmode == 1)
		kern_envp = static_env;

	return ((void *)(kernelstack.pv_va + USPACE_SVC_STACK_TOP -
	    sizeof(struct pcb)));
#undef next_page
#undef next_chunk2
}
