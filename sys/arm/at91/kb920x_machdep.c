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

#include "opt_msgbuf.h"
#include "opt_ddb.h"
#include "opt_at91.h"

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
#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_map.h>
#include <vm/vnode_pager.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/pcb.h>
#include <machine/undefined.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/armreg.h>
#include <machine/bus.h>
#include <sys/reboot.h>

#include <arm/at91/at91rm92reg.h>
#include <arm/at91/at91_piovar.h>
#include <arm/at91/at91_pio_rm9200.h>

#define KERNEL_PT_SYS		0	/* Page table for mapping proc0 zero page */
#define KERNEL_PT_KERN		1	
#define KERNEL_PT_KERN_NUM	22
#define KERNEL_PT_AFKERNEL	KERNEL_PT_KERN + KERNEL_PT_KERN_NUM	/* L2 table for mapping after kernel */
#define	KERNEL_PT_AFKERNEL_NUM	5

/* this should be evenly divisable by PAGE_SIZE / L2_TABLE_SIZE_REAL (or 4) */
#define NUM_KERNEL_PTS		(KERNEL_PT_AFKERNEL + KERNEL_PT_AFKERNEL_NUM)

/* Define various stack sizes in pages */
#define IRQ_STACK_SIZE	1
#define ABT_STACK_SIZE	1
#define UND_STACK_SIZE	1

extern u_int data_abort_handler_address;
extern u_int prefetch_abort_handler_address;
extern u_int undefined_handler_address;

struct pv_addr kernel_pt_table[NUM_KERNEL_PTS];

extern void *_end;

extern int *end;

struct pcpu __pcpu;
struct pcpu *pcpup = &__pcpu;

/* Physical and virtual addresses for some global pages */

vm_paddr_t phys_avail[10];
vm_paddr_t dump_avail[4];
vm_offset_t physical_pages;
vm_offset_t clean_sva, clean_eva;

struct pv_addr systempage;
struct pv_addr msgbufpv;
struct pv_addr irqstack;
struct pv_addr undstack;
struct pv_addr abtstack;
struct pv_addr kernelstack;
struct pv_addr minidataclean;

static struct trapframe proc0_tf;

/* Static device mappings. */
static const struct pmap_devmap kb920x_devmap[] = {
	/* 
	 * Map the on-board devices VA == PA so that we can access them
	 * with the MMU on or off.
	 */
	{
		/*
		 * This at least maps the interrupt controller, the UART
		 * and the timer. Other devices should use newbus to
		 * map their memory anyway.
		 */
		0xdff00000,
		0xfff00000,
		0x100000,
		VM_PROT_READ|VM_PROT_WRITE,                             
		PTE_NOCACHE,
	},
	/*
	 * We can't just map the OHCI registers VA == PA, because
	 * AT91RM92_OHCI_BASE belongs to the userland address space.
	 * We could just choose a different virtual address, but a better
	 * solution would probably be to just use pmap_mapdev() to allocate
	 * KVA, as we don't need the OHCI controller before the vm
	 * initialization is done. However, the AT91 resource allocation
	 * system doesn't know how to use pmap_mapdev() yet.
	 */
#if 0
	{
		/*
		 * Add the ohci controller, and anything else that might be
		 * on this chip select for a VA/PA mapping.
		 */
		AT91RM92_OHCI_BASE,
		AT91RM92_OHCI_BASE,
		AT91RM92_OHCI_SIZE,
		VM_PROT_READ|VM_PROT_WRITE,                             
		PTE_NOCACHE,
	},
#endif
	{
		0,
		0,
		0,
		0,
		0,
	}
};

#define SDRAM_START 0xa0000000

#ifdef DDB
extern vm_offset_t ksym_start, ksym_end;
#endif

static long
ramsize(void)
{
	uint32_t *SDRAMC = (uint32_t *)(AT91RM92_BASE + AT91RM92_SDRAMC_BASE);
	uint32_t cr, mr;
	int banks, rows, cols, bw;
	
	cr = SDRAMC[AT91RM92_SDRAMC_CR / 4];
	mr = SDRAMC[AT91RM92_SDRAMC_MR / 4];
	bw = (mr & AT91RM92_SDRAMC_MR_DBW_16) ? 1 : 2;
	banks = (cr & AT91RM92_SDRAMC_CR_NB_4) ? 2 : 1;
	rows = ((cr & AT91RM92_SDRAMC_CR_NR_MASK) >> 2) + 11;
	cols = (cr & AT91RM92_SDRAMC_CR_NC_MASK) + 8;
	return (1 << (cols + rows + banks + bw));
}

static long
board_init(void)
{
	/*
	 * Since the USART supprots RS-485 multidrop mode, it allows the
	 * TX pins to float.  However, for RS-232 operations, we don't want
	 * these pins to float.  Instead, they should be pulled up to avoid
	 * mismatches.  Linux does something similar when it configures the
	 * TX lines.  This implies that we also allow the RX lines to float
	 * rather than be in the state they are left in by the boot loader.
	 * Since they are input pins, I think that this is the right thing
	 * to do.
	 */

	/* PIOA's A periph: Turn USART 0 and 2's TX/RX pins */
	at91_pio_use_periph_a(AT91RM92_PIOA_BASE,
	    AT91C_PA18_RXD0 | AT91C_PA22_RXD2, 0);
	at91_pio_use_periph_a(AT91RM92_PIOA_BASE,
	    AT91C_PA17_TXD0 | AT91C_PA23_TXD2, 1);
	/* PIOA's B periph: Turn USART 3's TX/RX pins */
	at91_pio_use_periph_b(AT91RM92_PIOA_BASE, AT91C_PA6_RXD3, 0);
	at91_pio_use_periph_b(AT91RM92_PIOA_BASE, AT91C_PA5_TXD3, 1);
#ifdef AT91_TSC
	/* We're using TC0's A1 and A2 input */
	at91_pio_use_periph_b(AT91RM92_PIOA_BASE,
	    AT91C_PA19_TIOA1 | AT91C_PA21_TIOA2, 0);
#endif
	/* PIOB's A periph: Turn USART 1's TX/RX pins */
	at91_pio_use_periph_a(AT91RM92_PIOB_BASE, AT91C_PB21_RXD1, 0);
	at91_pio_use_periph_a(AT91RM92_PIOB_BASE, AT91C_PB20_TXD1, 1);

	/* Pin assignment */
#ifdef AT91_TSC
	/* Assert PA24 low -- talk to rubidium */
	at91_pio_use_gpio(AT91RM92_PIOA_BASE, AT91C_PIO_PA24);
	at91_pio_gpio_output(AT91RM92_PIOA_BASE, AT91C_PIO_PA24, 0);
	at91_pio_gpio_clear(AT91RM92_PIOA_BASE, AT91C_PIO_PA24);
#endif

	return (ramsize());
}

void *
initarm(void *arg, void *arg2)
{
	struct pv_addr  kernel_l1pt;
	int loop;
	u_int l1pagetable;
	vm_offset_t freemempos;
	vm_offset_t afterkern;
	int i = 0;
	uint32_t fake_preload[35];
	uint32_t memsize;
	vm_offset_t lastaddr;
#ifdef DDB
	vm_offset_t zstart = 0, zend = 0;
#endif

	i = 0;

	set_cpufuncs();

	fake_preload[i++] = MODINFO_NAME;
	fake_preload[i++] = strlen("elf kernel") + 1;
	strcpy((char*)&fake_preload[i++], "elf kernel");
	i += 2;
	fake_preload[i++] = MODINFO_TYPE;
	fake_preload[i++] = strlen("elf kernel") + 1;
	strcpy((char*)&fake_preload[i++], "elf kernel");
	i += 2;
	fake_preload[i++] = MODINFO_ADDR;
	fake_preload[i++] = sizeof(vm_offset_t);
	fake_preload[i++] = KERNBASE;
	fake_preload[i++] = MODINFO_SIZE;
	fake_preload[i++] = sizeof(uint32_t);
	fake_preload[i++] = (uint32_t)&end - KERNBASE;
#ifdef DDB
	if (*(uint32_t *)KERNVIRTADDR == MAGIC_TRAMP_NUMBER) {
		fake_preload[i++] = MODINFO_METADATA|MODINFOMD_SSYM;
		fake_preload[i++] = sizeof(vm_offset_t);
		fake_preload[i++] = *(uint32_t *)(KERNVIRTADDR + 4);
		fake_preload[i++] = MODINFO_METADATA|MODINFOMD_ESYM;
		fake_preload[i++] = sizeof(vm_offset_t);
		fake_preload[i++] = *(uint32_t *)(KERNVIRTADDR + 8);
		lastaddr = *(uint32_t *)(KERNVIRTADDR + 8);
		zend = lastaddr;
		zstart = *(uint32_t *)(KERNVIRTADDR + 4);
		ksym_start = zstart;
		ksym_end = zend;
	} else
#endif
		lastaddr = (vm_offset_t)&end;
		
	fake_preload[i++] = 0;
	fake_preload[i] = 0;
	preload_metadata = (void *)fake_preload;


	pcpu_init(pcpup, 0, sizeof(struct pcpu));
	PCPU_SET(curthread, &thread0);

#define KERNEL_TEXT_BASE (KERNBASE)
	freemempos = (lastaddr + PAGE_MASK) & ~PAGE_MASK;
	/* Define a macro to simplify memory allocation */
#define valloc_pages(var, np)                   \
	alloc_pages((var).pv_va, (np));         \
	(var).pv_pa = (var).pv_va + (KERNPHYSADDR - KERNVIRTADDR);

#define alloc_pages(var, np)			\
	(var) = freemempos;		\
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
			    KERNPHYSADDR;
		}
		i++;
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
	valloc_pages(kernelstack, KSTACK_PAGES);
	alloc_pages(minidataclean.pv_pa, 1);
	valloc_pages(msgbufpv, round_page(MSGBUF_SIZE) / PAGE_SIZE);
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
		pmap_link_l2pt(l1pagetable, KERNBASE + i * 0x100000,
		    &kernel_pt_table[KERNEL_PT_KERN + i]);
	pmap_map_chunk(l1pagetable, KERNBASE, KERNPHYSADDR,
	   (((uint32_t)(lastaddr) - KERNBASE) + PAGE_SIZE) & ~(PAGE_SIZE - 1),
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	afterkern = round_page((lastaddr + L1_S_SIZE) & ~(L1_S_SIZE 
	    - 1));
	for (i = 0; i < KERNEL_PT_AFKERNEL_NUM; i++) {
		pmap_link_l2pt(l1pagetable, afterkern + i * 0x00100000,
		    &kernel_pt_table[KERNEL_PT_AFKERNEL + i]);
	}
	pmap_map_entry(l1pagetable, afterkern, minidataclean.pv_pa, 
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	

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
	    KSTACK_PAGES * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

	pmap_map_chunk(l1pagetable, kernel_l1pt.pv_va, kernel_l1pt.pv_pa,
	    L1_TABLE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);
	pmap_map_chunk(l1pagetable, msgbufpv.pv_va, msgbufpv.pv_pa,
	    MSGBUF_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);


	for (loop = 0; loop < NUM_KERNEL_PTS; ++loop) {
		pmap_map_chunk(l1pagetable, kernel_pt_table[loop].pv_va,
		    kernel_pt_table[loop].pv_pa, L2_TABLE_SIZE,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);
	}

	pmap_devmap_bootstrap(l1pagetable, kb920x_devmap);
	cpu_domains((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2)) | DOMAIN_CLIENT);
	setttb(kernel_l1pt.pv_pa);
	cpu_tlb_flushID();
	cpu_domains(DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2));
	cninit();
	memsize = board_init();
	physmem = memsize / PAGE_SIZE;

	/*
	 * Pages were allocated during the secondary bootstrap for the
	 * stacks for different CPU modes.
	 * We must now set the r13 registers in the different CPU modes to
	 * point to these stacks.
	 * Since the ARM stacks use STMFD etc. we must set r13 to the top end
	 * of the stack memory.
	 */

	cpu_control(CPU_CONTROL_MMU_ENABLE, CPU_CONTROL_MMU_ENABLE);
	set_stackptr(PSR_IRQ32_MODE,
	    irqstack.pv_va + IRQ_STACK_SIZE * PAGE_SIZE);
	set_stackptr(PSR_ABT32_MODE,
	    abtstack.pv_va + ABT_STACK_SIZE * PAGE_SIZE);
	set_stackptr(PSR_UND32_MODE,
	    undstack.pv_va + UND_STACK_SIZE * PAGE_SIZE);



	/*
	 * We must now clean the cache again....
	 * Cleaning may be done by reading new data to displace any
	 * dirty data in the cache. This will have happened in setttb()
	 * but since we are boot strapping the addresses used for the read
	 * may have just been remapped and thus the cache could be out
	 * of sync. A re-clean after the switch will cure this.
	 * After booting there are no gross reloations of the kernel thus
	 * this problem will not occur after initarm().
	 */
	cpu_idcache_wbinv_all();

	/* Set stack for exception handlers */
	
	data_abort_handler_address = (u_int)data_abort_handler;
	prefetch_abort_handler_address = (u_int)prefetch_abort_handler;
	undefined_handler_address = (u_int)undefinedinstruction_bounce;
	undefined_init();
				
	proc_linkup(&proc0, &ksegrp0, &thread0);
	thread0.td_kstack = kernelstack.pv_va;
	thread0.td_pcb = (struct pcb *)
		(thread0.td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	thread0.td_pcb->pcb_flags = 0;
	thread0.td_frame = &proc0_tf;
	pcpup->pc_curpcb = thread0.td_pcb;
	
	arm_vector_init(ARM_VECTORS_HIGH, ARM_VEC_ALL);

	pmap_curmaxkvaddr = afterkern + 0x100000 * (KERNEL_PT_KERN_NUM - 1);
	pmap_bootstrap(freemempos,
	    KERNVIRTADDR + 3 * memsize,
	    &kernel_l1pt);
	msgbufp = (void*)msgbufpv.pv_va;
	msgbufinit(msgbufp, MSGBUF_SIZE);
	mutex_init();
	
	i = 0;
	dump_avail[0] = KERNPHYSADDR;
	dump_avail[1] = KERNPHYSADDR + memsize;
	dump_avail[2] = 0;
	dump_avail[3] = 0;
	
	phys_avail[0] = virtual_avail - KERNVIRTADDR + KERNPHYSADDR;
	phys_avail[1] = KERNPHYSADDR + memsize;
	phys_avail[2] = 0;
	phys_avail[3] = 0;
	/* Do basic tuning, hz etc */
	init_param1();
	init_param2(physmem);
	avail_end = KERNPHYSADDR + memsize - 1;
	kdb_init();
	return ((void *)(kernelstack.pv_va + USPACE_SVC_STACK_TOP -
	    sizeof(struct pcb)));
}
