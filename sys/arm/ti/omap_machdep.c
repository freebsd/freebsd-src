/*-
 * Copyright (c) 2009 Guillaume Ballet
 * All rights reserved.
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

#define _ARM32_BUS_DMA_PRIVATE

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <sys/pcpu.h>
#include <sys/cons.h>
#include <sys/conf.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <machine/pmap.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <machine/pcb.h>
#include <machine/machdep.h>
#include <machine/undefined.h>
#include <machine/bus.h>
#include <machine/intr.h>
#include <sys/kdb.h>
#include <sys/reboot.h>
#include <sys/msgbuf.h>
#define DEBUG_INITARM
#define VERBOSE_INIT_ARM

#include <arm/ti/omapvar.h>
#include <arm/ti/omap_prcm.h>

#if defined(SOC_OMAP4)
#include <arm/ti/omap4/omap44xx_reg.h>
const struct pmap_devmap omap_devmap[] = {
	/*
	 * Add the main memory areas, 
	 */
/*
	{
		.pd_va    = OMAP44XX_L3_EMU_VBASE,
		.pd_pa    = OMAP44XX_L3_EMU_HWBASE,
		.pd_size  = OMAP44XX_L3_EMU_SIZE,
		.pd_prot  = VM_PROT_READ|VM_PROT_WRITE,
		.pd_cache = PTE_DEVICE
	},
*/

	/* SDRAM EMIF register set, mapped as read-only for now */
	{
		.pd_va    = OMAP44XX_L3_EMIF1_VBASE,
		.pd_pa    = OMAP44XX_L3_EMIF1_HWBASE,
		.pd_size  = OMAP44XX_L3_EMIF1_SIZE,
		.pd_prot  = VM_PROT_READ,
		.pd_cache = PTE_NOCACHE
	},
	{
		.pd_va    = OMAP44XX_L3_EMIF2_VBASE,
		.pd_pa    = OMAP44XX_L3_EMIF2_HWBASE,
		.pd_size  = OMAP44XX_L3_EMIF2_SIZE,
		.pd_prot  = VM_PROT_READ,
		.pd_cache = PTE_NOCACHE
	},

	/* General memory areas */
	{
		.pd_va    = OMAP44XX_L4_CORE_VBASE,
		.pd_pa    = OMAP44XX_L4_CORE_HWBASE,
		.pd_size  = OMAP44XX_L4_CORE_SIZE,
		.pd_prot  = VM_PROT_READ|VM_PROT_WRITE,
		.pd_cache = PTE_NOCACHE
	},
	{
		.pd_va    = OMAP44XX_L4_PERIPH_VBASE,
		.pd_pa    = OMAP44XX_L4_PERIPH_HWBASE,
		.pd_size  = OMAP44XX_L4_PERIPH_SIZE,
		.pd_prot  = VM_PROT_READ|VM_PROT_WRITE,
		.pd_cache = PTE_NOCACHE
	},
	{
		.pd_va    = OMAP44XX_L4_ABE_VBASE,
		.pd_pa    = OMAP44XX_L4_ABE_HWBASE,
		.pd_size  = OMAP44XX_L4_ABE_SIZE,
		.pd_prot  = VM_PROT_READ|VM_PROT_WRITE,
		.pd_cache = PTE_NOCACHE
	},
	{ 0, 0, 0, 0, 0 }	/* Array terminator */
};

#elif defined(SOC_OMAP3)
#include <arm/ti/omap3/omap35xx_reg.h>
const struct pmap_devmap omap_devmap[] = {
	/*
	 * For the moment, map devices with PA==VA.
	 */
	{
		/* 16MB of L4, covering all L4 core registers */
		.pd_va    = OMAP35XX_L4_CORE_VBASE,
		.pd_pa    = OMAP35XX_L4_CORE_HWBASE,
		.pd_size  = OMAP35XX_L4_CORE_SIZE,
		.pd_prot  = VM_PROT_READ|VM_PROT_WRITE,
		.pd_cache = PTE_NOCACHE
	},
	{
		/* 1MB of L4, covering the periph registers */
		.pd_va    = OMAP35XX_L4_PERIPH_VBASE,
		.pd_pa    = OMAP35XX_L4_PERIPH_HWBASE,
		.pd_size  = OMAP35XX_L4_PERIPH_SIZE,
		.pd_prot  = VM_PROT_READ|VM_PROT_WRITE,
		.pd_cache = PTE_NOCACHE
	},
	{
		/* 64Kb of L3, covering the SDRAM controller registers */
		.pd_va    = OMAP35XX_SDRC_VBASE,
		.pd_pa    = OMAP35XX_SDRC_HWBASE,
		.pd_size  = OMAP35XX_SDRC_SIZE,
		.pd_prot  = VM_PROT_READ|VM_PROT_WRITE,
		.pd_cache = PTE_NOCACHE
	},
#if 0	
	{
		/* 64Kb of L3, covering the SDRAM controller registers */
		.pd_va    = OMAP35XX_L3_VBASE,
		.pd_pa    = OMAP35XX_L3_HWBASE,
		.pd_size  = OMAP35XX_L3_SIZE,
		.pd_prot  = VM_PROT_READ|VM_PROT_WRITE,
		.pd_cache = PTE_NOCACHE
	},
#endif
	{ 0, 0, 0, 0, 0 }	/* Array terminator */
};

#else
#error OMAP chip type not defined

#endif

#ifdef VERBOSE_INIT_ARM
#define  DPRINTF(x)
#else
#define  DPRINTF(x)
#endif

/* Physical page ranges */
vm_paddr_t	phys_avail[4];
vm_paddr_t	dump_avail[4];

struct pv_addr systempage;

#define FIQ_STACK_SIZE	1
#define IRQ_STACK_SIZE	1
#define ABT_STACK_SIZE	1
#define UND_STACK_SIZE	1

extern int _start[];
extern int _end[];

extern void early_putchar(unsigned char);
extern void early_putstr(unsigned char *);
extern void early_printf(const char *fmt, ...);
extern void early_print_init(uint32_t hw_addr, uint32_t virt_addr);

extern u_int data_abort_handler_address;
extern u_int prefetch_abort_handler_address;
extern u_int undefined_handler_address;

static struct pv_addr	fiqstack;				/* Stack page descriptors for all modes */
static struct pv_addr   msgbuf;
static struct pv_addr	irqstack;
static struct pv_addr	undstack;
static struct pv_addr	abtstack;
static struct pv_addr	kernelstack;
static struct pv_addr	kernel_l1pt;				/* Level-1 page table entry */

#define KERNEL_PT_SYS		0	/* Page table for mapping proc0 zero page */
#define KERNEL_PT_KERN		1
#define KERNEL_PT_KERN_NUM	22
#define KERNEL_PT_AFKERNEL	KERNEL_PT_KERN + KERNEL_PT_KERN_NUM	/* L2 table for mapping after kernel */
#define	KERNEL_PT_AFKERNEL_NUM	5

/* this should be evenly divisable by PAGE_SIZE / L2_TABLE_SIZE_REAL (or 4) */
#define NUM_KERNEL_PTS		(KERNEL_PT_AFKERNEL + KERNEL_PT_AFKERNEL_NUM)

static struct pv_addr kernel_page_tables[NUM_KERNEL_PTS];	/* Level-2 page table entries for the kernel */
static struct trapframe	proc0_tf;

#define PHYS2VIRT(x)	((x - KERNPHYSADDR) + KERNVIRTADDR)
#define VIRT2PHYS(x)	((x - KERNVIRTADDR) + KERNPHYSADDR)

/* Macro stolen from the Xscale part, used to simplify TLB allocation */
#define valloc_pages(var, np)	do {			\
	alloc_pages((var).pv_pa, (np));         	\
	(var).pv_va = PHYS2VIRT((var).pv_pa);		\
	DPRINTF(("va=%p pa=%p\n", (void*)(var).pv_va,	\
	    (void*)(var).pv_pa)); 			\
} while(0)

#define alloc_pages(var, np)	do {			\
	(var) = freemempos;				\
	freemempos += (np * PAGE_SIZE);			\
	memset((char *)(var), 0, ((np) * PAGE_SIZE));	\
} while(0)

#define round_L_page(x) (((x) + L2_L_OFFSET) & L2_L_FRAME)

#define VERBOSE_INIT_ARM

static void *
initarm_boilerplate(void *arg1, void *arg2)
{
	vm_offset_t freemempos;
	vm_offset_t freemem_pt;
	vm_offset_t lastaddr;
	vm_offset_t offset;
	uint32_t i, j;
	uint32_t sdram_size;
	uint32_t l1pagetable;
	size_t textsize;
	size_t totalsize;
	
	/*
	 * Sets the CPU functions based on the CPU ID, this code is all
	 * in the cpufunc.c file. The function sets the cpufunc structure
	 * to match the machine and also initialises the pmap/pte code.
	 */
	set_cpufuncs();
	
	/*
	 * fake_preload_metadata() creates a fake boot descriptor table and
	 * returns the last address in the kernel.
	 */
	lastaddr = fake_preload_metadata();
	
	/*
	 * Initialize the MI portions of a struct per cpu structure.
	 */
	pcpu_init(pcpup, 0, sizeof(struct pcpu));
	PCPU_SET(curthread, &thread0);
	init_param1();
	
	/*
	 * Initialize freemempos. Pages are allocated from the end of the RAM's
	 * first 64MB, as it is what is covered by the default TLB in locore.S.
	 */
	freemempos = VIRT2PHYS(round_L_page(lastaddr));

	/* Reserve L1 table pages now, as freemempos is 64K-aligned */
	valloc_pages(kernel_l1pt, L1_TABLE_SIZE / PAGE_SIZE);

	/* 
	 * Reserve the paging system pages, page #0 is reserved as a L2 table for
	 * the exception vector. Now allocate L2 page tables; they are linked to L1 below
	 */
	for (i = 0; i < NUM_KERNEL_PTS; ++i) {
		j = (i % (PAGE_SIZE / L2_TABLE_SIZE_REAL));
		
		if (j == 0)
			valloc_pages(kernel_page_tables[i], L2_TABLE_SIZE / PAGE_SIZE);
		else {
			kernel_page_tables[i].pv_pa	= kernel_page_tables[i - j].pv_pa 
			                              + (j * L2_TABLE_SIZE_REAL);
			kernel_page_tables[i].pv_va	= kernel_page_tables[i - j].pv_va
			                              + (j * L2_TABLE_SIZE_REAL);
		}
	}

	/* base of allocated pt's */
	freemem_pt = freemempos;
	
	/*
	 * Allocate a page for the system page mapped to V0x00000000. This page
	 * will just contain the system vectors and can be shared by all processes.
	 * This is where the interrupt vector is stored.
	 */
	valloc_pages(systempage, 1);
	systempage.pv_va = ARM_VECTORS_HIGH;
	
	/* Allocate dynamic per-cpu area. */
	/* TODO: valloc_pages(dpcpu, DPCPU_SIZE / PAGE_SIZE); */
	/* TODO: dpcpu_init((void *)dpcpu.pv_va, 0); */
	
	
	/* Allocate stacks for all modes */
	valloc_pages(fiqstack, FIQ_STACK_SIZE);
	valloc_pages(irqstack, IRQ_STACK_SIZE);
	valloc_pages(abtstack, ABT_STACK_SIZE);
	valloc_pages(undstack, UND_STACK_SIZE);
	valloc_pages(kernelstack, KSTACK_PAGES + 1);
	valloc_pages(msgbuf, round_page(msgbufsize) / PAGE_SIZE);
	 
	/* Build the TLBs */
	
	/*
	 * Now construct the L1 page table.  First map the L2
	 * page tables into the L1 so we can replace L1 mappings
	 * later on if necessary
	 */
	l1pagetable = kernel_l1pt.pv_va;
	
	/* L2 table for the exception vector */
	pmap_link_l2pt(l1pagetable, ARM_VECTORS_HIGH & ~(0x100000 - 1),
	               &kernel_page_tables[0]);
	
	/* Insert a reference to the kernel L2 page tables into the L1 page. */
	for (i=1; i<NUM_KERNEL_PTS; i++) {
		pmap_link_l2pt(l1pagetable,
		    KERNVIRTADDR + (i-1) * L1_S_SIZE,
		    &kernel_page_tables[i]);
	}
	
	/*
	 * Map the kernel
	 */

	textsize = round_L_page((unsigned long)etext - KERNVIRTADDR);
	totalsize = round_L_page(lastaddr - KERNVIRTADDR);
	
	offset = 0;
	offset += pmap_map_chunk(l1pagetable, KERNVIRTADDR + offset,
	    KERNPHYSADDR + offset, textsize,
	    VM_PROT_READ|VM_PROT_EXECUTE, PTE_CACHE);
	
	offset += pmap_map_chunk(l1pagetable, KERNVIRTADDR + offset,
	    KERNPHYSADDR + offset, totalsize - textsize,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

	/* Map the L1 page table of the kernel */
	pmap_map_chunk(l1pagetable, kernel_l1pt.pv_va, kernel_l1pt.pv_pa,
	    L1_TABLE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);
	
	/* Map the L2 page tables of the kernel */
	for (i=0; i<NUM_KERNEL_PTS; i++) {
		if ((i % (PAGE_SIZE / L2_TABLE_SIZE_REAL)) == 0) { 
			pmap_map_chunk(l1pagetable,
			    kernel_page_tables[i].pv_va,
			    kernel_page_tables[i].pv_pa,
			    L2_TABLE_SIZE, VM_PROT_READ|VM_PROT_WRITE,
			    PTE_PAGETABLE);
		}
	}
	
	/*
	 * Map the vector page
	 */
	pmap_map_entry(l1pagetable, ARM_VECTORS_HIGH, systempage.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE, PTE_CACHE);
	
	
	/*
	 * Map the stack pages
	 */
	pmap_map_chunk(l1pagetable, fiqstack.pv_va, fiqstack.pv_pa,
	    FIQ_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, irqstack.pv_va, irqstack.pv_pa,
	    IRQ_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, abtstack.pv_va, abtstack.pv_pa,
	    ABT_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, undstack.pv_va, undstack.pv_pa,
	    UND_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, kernelstack.pv_va, kernelstack.pv_pa,
	    (KSTACK_PAGES+1) * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, msgbuf.pv_va, msgbuf.pv_pa, 
	    round_page(msgbufsize), VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	
	/*
	 * Device map
	 */
	pmap_devmap_bootstrap(l1pagetable, omap_devmap);
	
	/* Switch L1 TLB table */
	cpu_domains((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2)) | DOMAIN_CLIENT);

	setttb(kernel_l1pt.pv_pa);

	cpu_tlb_flushID();

	cpu_domains((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2)));
	
	/*
	 * Pages were allocated during the secondary bootstrap for the
	 * stacks for different CPU modes.
	 * We must now set the r13 registers in the different CPU modes to
	 * point to these stacks.
	 * Since the ARM stacks use STMFD etc. we must set r13 to the top end
	 * of the stack memory.
	 */
	set_stackptr(PSR_FIQ32_MODE, fiqstack.pv_va + FIQ_STACK_SIZE * PAGE_SIZE);
	set_stackptr(PSR_IRQ32_MODE, irqstack.pv_va + IRQ_STACK_SIZE * PAGE_SIZE);
	set_stackptr(PSR_ABT32_MODE, abtstack.pv_va + ABT_STACK_SIZE * PAGE_SIZE);
	set_stackptr(PSR_UND32_MODE, undstack.pv_va + UND_STACK_SIZE * PAGE_SIZE);
	
	boothowto |= RB_VERBOSE;
	
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
	
	
	/* ready to setup the console (XXX move earlier if possible) */
	cninit();
	
	/* Miscellaneous */
	
	/* Exception handlers */
	data_abort_handler_address = (u_int) data_abort_handler;
	prefetch_abort_handler_address = (u_int) prefetch_abort_handler;
	undefined_handler_address = (u_int) undefinedinstruction_bounce;
	undefined_init();
	
	/* Prepares the context of the first process */
	proc_linkup(&proc0, &thread0);
	thread0.td_kstack = kernelstack.pv_va;
	thread0.td_pcb = (struct pcb *) (thread0.td_kstack + (KSTACK_PAGES * PAGE_SIZE)) - 1;
	thread0.td_pcb->pcb_flags = 0;
	thread0.td_frame = &proc0_tf;
	pcpup->pc_curpcb = thread0.td_pcb;

#ifdef VERBOSE_INIT_ARM 
	printf("\nThread-0 stack=0x%08x pcb=0x%08x\n", (unsigned int)thread0.td_kstack, (unsigned int)thread0.td_pcb);
#endif
	
	/* Exception vector */
	arm_vector_init(ARM_VECTORS_HIGH, ARM_VEC_ALL);
	
#ifdef VERBOSE_INIT_ARM 
	printf("Exception vector at (0x%08x)\n", ((unsigned int *)systempage.pv_va)[0]);
#endif
	
	/* First unbacked address of KVM */
	pmap_curmaxkvaddr = KERNVIRTADDR + 0x100000 * NUM_KERNEL_PTS;
#ifdef VERBOSE_INIT_ARM 
	printf("pmap_curmaxkvaddr = 0x%08x\n", pmap_curmaxkvaddr);
#endif
	
	/* Get the size of the SDRAM, this is typically defined in the SoC specific
	 * files (i.e. omap44xx.c)
	 */
	sdram_size = omap_sdram_size();
#ifdef VERBOSE_INIT_ARM 
	printf("\nSDRAM size 0x%08X, %dMB\n", sdram_size, (sdram_size / 1024 / 1024));
#endif
	
	/* Physical ranges of available memory. */
	phys_avail[0] = freemempos;
	phys_avail[1] = PHYSADDR + sdram_size;
	phys_avail[2] = 0;
	phys_avail[3] = 0;
	
	dump_avail[0] = PHYSADDR;
	dump_avail[1] = PHYSADDR + sdram_size;
	dump_avail[2] = 0;
	dump_avail[3] = 0;
	
	physmem	 = sdram_size / PAGE_SIZE;

	pmap_bootstrap((freemempos&0x00ffffff)|0xc0000000,   /* start address */
	    0xe8000000,  /* end address, min adress for device stuff */
	    &kernel_l1pt);

	init_param2(physmem);
		
	/* Locking system */
	mutex_init();
	
	msgbufp = (void*)msgbuf.pv_va;
	msgbufinit(msgbufp, msgbufsize);
				
	/* Kernel debugger */
	kdb_init();
	vector_page_setprot(VM_PROT_READ);
	
	/* initarm returns the address of the kernel stack */
	return ((void *)(kernelstack.pv_va + USPACE_SVC_STACK_TOP -
	    sizeof(struct pcb)));
}

void *
initarm(void *arg1, void *arg2)
{
	return initarm_boilerplate(arg1, arg2);
}

struct arm32_dma_range *
bus_dma_get_range(void)
{
	/* TODO: Need implementation ? */
	return (NULL);
}

int
bus_dma_get_range_nb(void)
{
	/* TODO: Need implementation ? */
	return (0);
}

/**
 * cpu_reset
 *
 * Called by the system to reset the CPU - obvious really
 *
 */
void
cpu_reset()
{
	omap_prcm_reset();
	printf("Reset failed!\n");
	while (1);
}
