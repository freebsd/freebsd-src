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

#include <arm/samsung/s3c2xx0/s3c24x0var.h>
#include <arm/samsung/s3c2xx0/s3c2410reg.h>
#include <arm/samsung/s3c2xx0/s3c2xx0board.h>

/* Page table for mapping proc0 zero page */
#define KERNEL_PT_SYS		0
#define KERNEL_PT_KERN		1	
#define KERNEL_PT_KERN_NUM	44
/* L2 table for mapping after kernel */
#define KERNEL_PT_AFKERNEL	KERNEL_PT_KERN + KERNEL_PT_KERN_NUM
#define	KERNEL_PT_AFKERNEL_NUM	5

/* this should be evenly divisable by PAGE_SIZE / L2_TABLE_SIZE_REAL (or 4) */
#define NUM_KERNEL_PTS		(KERNEL_PT_AFKERNEL + KERNEL_PT_AFKERNEL_NUM)

extern int s3c2410_pclk;

struct pv_addr kernel_pt_table[NUM_KERNEL_PTS];

/* Physical and virtual addresses for some global pages */

struct pv_addr systempage;
struct pv_addr msgbufpv;
struct pv_addr irqstack;
struct pv_addr undstack;
struct pv_addr abtstack;
struct pv_addr kernelstack;

#define	_A(a)	((a) & ~L1_S_OFFSET)
#define	_S(s)	(((s) + L1_S_SIZE - 1) & ~(L1_S_SIZE-1))

/* Static device mappings. */
static const struct arm_devmap_entry s3c24x0_devmap[] = {
	/*
	 * Map the devices we need early on.
	 */
	{
		_A(S3C24X0_CLKMAN_BASE),
		_A(S3C24X0_CLKMAN_PA_BASE),
		_S(S3C24X0_CLKMAN_SIZE),
		VM_PROT_READ|VM_PROT_WRITE,
		PTE_DEVICE,
	},
	{
		_A(S3C24X0_GPIO_BASE),
		_A(S3C24X0_GPIO_PA_BASE),
		_S(S3C2410_GPIO_SIZE),
		VM_PROT_READ|VM_PROT_WRITE,
		PTE_DEVICE,
	},
	{
		_A(S3C24X0_INTCTL_BASE),
		_A(S3C24X0_INTCTL_PA_BASE),
		_S(S3C24X0_INTCTL_SIZE),
		VM_PROT_READ|VM_PROT_WRITE,
		PTE_DEVICE,
	},
	{
		_A(S3C24X0_TIMER_BASE),
		_A(S3C24X0_TIMER_PA_BASE),
		_S(S3C24X0_TIMER_SIZE),
		VM_PROT_READ|VM_PROT_WRITE,
		PTE_DEVICE,
	},
	{
		_A(S3C24X0_UART0_BASE),
		_A(S3C24X0_UART0_PA_BASE),
		_S(S3C24X0_UART_PA_BASE(3) - S3C24X0_UART0_PA_BASE),
		VM_PROT_READ|VM_PROT_WRITE,
		PTE_DEVICE,
	},
	{
		_A(S3C24X0_WDT_BASE),
		_A(S3C24X0_WDT_PA_BASE),
		_S(S3C24X0_WDT_SIZE),
		VM_PROT_READ|VM_PROT_WRITE,
		PTE_DEVICE,
	},
	{
		0,
		0,
		0,
		0,
		0,
	}
};

#undef	_A
#undef	_S

#define	ioreg_read32(a)  	(*(volatile uint32_t *)(a))
#define	ioreg_write32(a,v)	(*(volatile uint32_t *)(a)=(v))

struct arm32_dma_range s3c24x0_range = {
	.dr_sysbase = 0,
	.dr_busbase = 0,
	.dr_len = 0,
};

struct arm32_dma_range *
bus_dma_get_range(void)
{

	if (s3c24x0_range.dr_len == 0) {
		s3c24x0_range.dr_sysbase = dump_avail[0];
		s3c24x0_range.dr_busbase = dump_avail[0];
		s3c24x0_range.dr_len = dump_avail[1] - dump_avail[0];
	}
	return (&s3c24x0_range);
}

int
bus_dma_get_range_nb(void)
{
	return (1);
}

void *
initarm(struct arm_boot_params *abp)
{
	struct pv_addr	kernel_l1pt;
	int loop;
	u_int l1pagetable;
	vm_offset_t freemempos;
	vm_offset_t afterkern;
	vm_offset_t lastaddr;

	int i;
	uint32_t memsize;

	boothowto = 0;  /* Likely not needed */
	lastaddr = parse_boot_param(abp);
	arm_physmem_kernaddr = abp->abp_physaddr;
	i = 0;
	set_cpufuncs();
	cpufuncs.cf_sleep = s3c24x0_sleep;

	pcpu0_init();

	/* Do basic tuning, hz etc */
	init_param1();

#define KERNEL_TEXT_BASE (KERNBASE)
	freemempos = (lastaddr + PAGE_MASK) & ~PAGE_MASK;
	/* Define a macro to simplify memory allocation */
#define valloc_pages(var, np)			\
	alloc_pages((var).pv_va, (np));		\
	(var).pv_pa = (var).pv_va + (abp->abp_physaddr - KERNVIRTADDR);

#define alloc_pages(var, np)			\
	(var) = freemempos;			\
	freemempos += (np * PAGE_SIZE);		\
	memset((char *)(var), 0, ((np) * PAGE_SIZE));

	while (((freemempos - L1_TABLE_SIZE) & (L1_TABLE_SIZE - 1)) != 0)
		freemempos += PAGE_SIZE;
	valloc_pages(kernel_l1pt, L1_TABLE_SIZE / PAGE_SIZE);
	for (loop = 0; loop < NUM_KERNEL_PTS; ++loop) {
		if (!(loop % (PAGE_SIZE / L2_TABLE_SIZE_REAL))) {
			valloc_pages(kernel_pt_table[loop],
			    L2_TABLE_SIZE / PAGE_SIZE);
		} else {
			kernel_pt_table[loop].pv_va = freemempos -
			    (loop % (PAGE_SIZE / L2_TABLE_SIZE_REAL)) *
			    L2_TABLE_SIZE_REAL;
			kernel_pt_table[loop].pv_pa =
			    kernel_pt_table[loop].pv_va - KERNVIRTADDR +
			    abp->abp_physaddr;
		}
	}
	/*
	 * Allocate a page for the system page mapped to V0x00000000
	 * This page will just contain the system vectors and can be
	 * shared by all processes.
	 */
	valloc_pages(systempage, 1);

	/* Allocate stacks for all modes */
	valloc_pages(irqstack, IRQ_STACK_SIZE);
	valloc_pages(abtstack, ABT_STACK_SIZE);
	valloc_pages(undstack, UND_STACK_SIZE);
	valloc_pages(kernelstack, kstack_pages);
	valloc_pages(msgbufpv, round_page(msgbufsize) / PAGE_SIZE);
	/*
	 * Now we start construction of the L1 page table
	 * We start by mapping the L2 page tables into the L1.
	 * This means that we can replace L1 mappings later on if necessary
	 */
	l1pagetable = kernel_l1pt.pv_va;

	/* Map the L2 pages tables in the L1 page table */
	pmap_link_l2pt(l1pagetable, ARM_VECTORS_HIGH,
	    &kernel_pt_table[KERNEL_PT_SYS]);
	for (i = 0; i < KERNEL_PT_KERN_NUM; i++)
		pmap_link_l2pt(l1pagetable, KERNBASE + i * L1_S_SIZE,
		    &kernel_pt_table[KERNEL_PT_KERN + i]);
	pmap_map_chunk(l1pagetable, KERNBASE, PHYSADDR,
	   (((uint32_t)(lastaddr) - KERNBASE) + PAGE_SIZE) & ~(PAGE_SIZE - 1),
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	afterkern = round_page((lastaddr + L1_S_SIZE) & ~(L1_S_SIZE
	    - 1));
	for (i = 0; i < KERNEL_PT_AFKERNEL_NUM; i++) {
		pmap_link_l2pt(l1pagetable, afterkern + i * L1_S_SIZE,
		    &kernel_pt_table[KERNEL_PT_AFKERNEL + i]);
	}

	/* Map the vector page. */
	pmap_map_entry(l1pagetable, ARM_VECTORS_HIGH, systempage.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	/* Map the stack pages */
	pmap_map_chunk(l1pagetable, irqstack.pv_va, irqstack.pv_pa,
	    IRQ_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, abtstack.pv_va, abtstack.pv_pa,
	    ABT_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, undstack.pv_va, undstack.pv_pa,
	    UND_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, kernelstack.pv_va, kernelstack.pv_pa,
	    kstack_pages * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

	pmap_map_chunk(l1pagetable, kernel_l1pt.pv_va, kernel_l1pt.pv_pa,
	    L1_TABLE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);
	pmap_map_chunk(l1pagetable, msgbufpv.pv_va, msgbufpv.pv_pa,
	    msgbufsize, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);


	for (loop = 0; loop < NUM_KERNEL_PTS; ++loop) {
		pmap_map_chunk(l1pagetable, kernel_pt_table[loop].pv_va,
		    kernel_pt_table[loop].pv_pa, L2_TABLE_SIZE,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);
	}

	arm_devmap_bootstrap(l1pagetable, s3c24x0_devmap);

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

	cpu_control(CPU_CONTROL_MMU_ENABLE, CPU_CONTROL_MMU_ENABLE);
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

	/* Disable all peripheral interrupts */
	ioreg_write32(S3C24X0_INTCTL_BASE + INTCTL_INTMSK, ~0);
	memsize = board_init();
	/* Find pclk for uart */
	switch(ioreg_read32(S3C24X0_GPIO_BASE + GPIO_GSTATUS1) >> 16) {
	case 0x3241:
		s3c2410_clock_freq2(S3C24X0_CLKMAN_BASE, NULL, NULL,
		    &s3c2410_pclk);
		break;
	case 0x3244:
		s3c2440_clock_freq2(S3C24X0_CLKMAN_BASE, NULL, NULL,
		    &s3c2410_pclk);
		break;
	}
	cninit();

	undefined_init();
	
	init_proc0(kernelstack.pv_va);			
	
	arm_vector_init(ARM_VECTORS_HIGH, ARM_VEC_ALL);

	pmap_curmaxkvaddr = afterkern + 0x100000 * (KERNEL_PT_KERN_NUM - 1);
	vm_max_kernel_address = KERNVIRTADDR + 3 * memsize;
	pmap_bootstrap(freemempos, &kernel_l1pt);
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
	arm_physmem_exclude_region(abp->abp_physaddr, 
	    virtual_avail - KERNVIRTADDR, EXFLAG_NOALLOC);
	arm_physmem_init_kernel_globals();

	init_param2(physmem);
	kdb_init();

	return ((void *)(kernelstack.pv_va + USPACE_SVC_STACK_TOP -
	    sizeof(struct pcb)));
}
