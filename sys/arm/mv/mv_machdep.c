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
 * from: FreeBSD: //depot/projects/arm/src/sys/arm/at91/kb920x_machdep.c, rev 45
 */

#include "opt_msgbuf.h"
#include "opt_ddb.h"

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
#include <vm/vm_pager.h>
#include <vm/vm_map.h>
#include <vm/vnode_pager.h>
#include <machine/pte.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/pcb.h>
#include <machine/undefined.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/armreg.h>
#include <machine/bus.h>
#include <sys/reboot.h>
#include <machine/bootinfo.h>

#include <arm/mv/mvvar.h>	/* XXX eventually this should be eliminated */

#ifdef  DEBUG
#define debugf(fmt, args...) printf(fmt, ##args)
#else
#define debugf(fmt, args...)
#endif

#define KERNEL_PT_SYS	0	/* Page table for mapping proc0 zero page */
#define KERNEL_PT_KERN	1

/*
 * This is the number of L2 page tables required for covering max
 * (hypothetical) memsize of 4GB and all kernel mappings (vectors, msgbuf,
 * stacks etc.), uprounded to be divisible by 4.
 */
#define KERNEL_PT_MAX	78

/* Define various stack sizes in pages */
#define IRQ_STACK_SIZE	1
#define ABT_STACK_SIZE	1
#define UND_STACK_SIZE	1

/* Maximum number of memory regions */
#define MEM_REGIONS	8

extern unsigned char kernbase[];
extern unsigned char _etext[];
extern unsigned char _edata[];
extern unsigned char __bss_start[];
extern unsigned char _end[];

extern u_int data_abort_handler_address;
extern u_int prefetch_abort_handler_address;
extern u_int undefined_handler_address;

extern const struct pmap_devmap *pmap_devmap_bootstrap_table;
extern vm_offset_t pmap_bootstrap_lastaddr;

struct pv_addr kernel_pt_table[KERNEL_PT_MAX];

extern int *end;

struct pcpu __pcpu;
struct pcpu *pcpup = &__pcpu;

/* Physical and virtual addresses for some global pages */

vm_paddr_t phys_avail[10];
vm_paddr_t dump_avail[4];
vm_offset_t physical_pages;

struct pv_addr systempage;
struct pv_addr msgbufpv;
struct pv_addr irqstack;
struct pv_addr undstack;
struct pv_addr abtstack;
struct pv_addr kernelstack;

static struct trapframe proc0_tf;

struct mem_region {
	vm_offset_t	mr_start;
	vm_size_t	mr_size;
};

static struct mem_region availmem_regions[MEM_REGIONS];
static int availmem_regions_sz;

struct bootinfo *bootinfo;

static void print_kenv(void);
static void print_kernel_section_addr(void);
static void print_bootinfo(void);

static void physmap_init(int);

static char *
kenv_next(char *cp)
{

	if (cp != NULL) {
		while (*cp != 0)
			cp++;
		cp++;
		if (*cp == 0)
			cp = NULL;
	}
	return (cp);
}

static void
print_kenv(void)
{
	int len;
	char *cp;

	debugf("loader passed (static) kenv:\n");
	if (kern_envp == NULL) {
		debugf(" no env, null ptr\n");
		return;
	}
	debugf(" kern_envp = 0x%08x\n", (uint32_t)kern_envp);

	len = 0;
	for (cp = kern_envp; cp != NULL; cp = kenv_next(cp))
		debugf(" %x %s\n", (uint32_t)cp, cp);
}

static void
print_bootinfo(void)
{
	struct bi_mem_region *mr;
	struct bi_eth_addr *eth;
	int i, j;

	debugf("bootinfo:\n");
	if (bootinfo == NULL) {
		debugf(" no bootinfo, null ptr\n");
		return;
	}

	debugf(" version = 0x%08x\n", bootinfo->bi_version);
	debugf(" ccsrbar = 0x%08x\n", bootinfo->bi_bar_base);
	debugf(" cpu_clk = 0x%08x\n", bootinfo->bi_cpu_clk);
	debugf(" bus_clk = 0x%08x\n", bootinfo->bi_bus_clk);

	debugf(" mem regions:\n");
	mr = (struct bi_mem_region *)bootinfo->bi_data;
	for (i = 0; i < bootinfo->bi_mem_reg_no; i++, mr++)
		debugf("    #%d, base = 0x%08x, size = 0x%08x\n", i,
		    mr->mem_base, mr->mem_size);

	debugf(" eth addresses:\n");
	eth = (struct bi_eth_addr *)mr;
	for (i = 0; i < bootinfo->bi_eth_addr_no; i++, eth++) {
		debugf("    #%d, addr = ", i);
		for (j = 0; j < 6; j++)
			debugf("%02x ", eth->mac_addr[j]);
		debugf("\n");
	}
}

static void
print_kernel_section_addr(void)
{

	debugf("kernel image addresses:\n");
	debugf(" kernbase       = 0x%08x\n", (uint32_t)kernbase);
	debugf(" _etext (sdata) = 0x%08x\n", (uint32_t)_etext);
	debugf(" _edata         = 0x%08x\n", (uint32_t)_edata);
	debugf(" __bss_start    = 0x%08x\n", (uint32_t)__bss_start);
	debugf(" _end           = 0x%08x\n", (uint32_t)_end);
}

struct bi_mem_region *
bootinfo_mr(void)
{

	return ((struct bi_mem_region *)bootinfo->bi_data);
}

static void
physmap_init(int hardcoded)
{
	int i, j, cnt;
	vm_offset_t phys_kernelend, kernload;
	uint32_t s, e, sz;
	struct mem_region *mp, *mp1;

	phys_kernelend = KERNPHYSADDR + (virtual_avail - KERNVIRTADDR);
	kernload = KERNPHYSADDR;

	/*
	 * Use hardcoded physical addresses if we don't use memory regions
	 * from metadata.
	 */
	if (hardcoded) {
		phys_avail[0] = 0;
		phys_avail[1] = kernload;

		phys_avail[2] = phys_kernelend;
		phys_avail[3] = PHYSMEM_SIZE;

		phys_avail[4] = 0;
		phys_avail[5] = 0;
		return;
	}

	/*
	 * Remove kernel physical address range from avail
	 * regions list. Page align all regions.
	 * Non-page aligned memory isn't very interesting to us.
	 * Also, sort the entries for ascending addresses.
	 */
	sz = 0;
	cnt = availmem_regions_sz;
	debugf("processing avail regions:\n");
	for (mp = availmem_regions; mp->mr_size; mp++) {
		s = mp->mr_start;
		e = mp->mr_start + mp->mr_size;
		debugf(" %08x-%08x -> ", s, e);
		/* Check whether this region holds all of the kernel. */
		if (s < kernload && e > phys_kernelend) {
			availmem_regions[cnt].mr_start = phys_kernelend;
			availmem_regions[cnt++].mr_size = e - phys_kernelend;
			e = kernload;
		}
		/* Look whether this regions starts within the kernel. */
		if (s >= kernload && s < phys_kernelend) {
			if (e <= phys_kernelend)
				goto empty;
			s = phys_kernelend;
		}
		/* Now look whether this region ends within the kernel. */
		if (e > kernload && e <= phys_kernelend) {
			if (s >= kernload) {
				goto empty;
			}
			e = kernload;
		}
		/* Now page align the start and size of the region. */
		s = round_page(s);
		e = trunc_page(e);
		if (e < s)
			e = s;
		sz = e - s;
		debugf("%08x-%08x = %x\n", s, e, sz);

		/* Check whether some memory is left here. */
		if (sz == 0) {
		empty:
			printf("skipping\n");
			bcopy(mp + 1, mp,
			    (cnt - (mp - availmem_regions)) * sizeof(*mp));
			cnt--;
			mp--;
			continue;
		}

		/* Do an insertion sort. */
		for (mp1 = availmem_regions; mp1 < mp; mp1++)
			if (s < mp1->mr_start)
				break;
		if (mp1 < mp) {
			bcopy(mp1, mp1 + 1, (char *)mp - (char *)mp1);
			mp1->mr_start = s;
			mp1->mr_size = sz;
		} else {
			mp->mr_start = s;
			mp->mr_size = sz;
		}
	}
	availmem_regions_sz = cnt;

	/* Fill in phys_avail table, based on availmem_regions */
	debugf("fill in phys_avail:\n");
	for (i = 0, j = 0; i < availmem_regions_sz; i++, j += 2) {

		debugf(" region: 0x%08x - 0x%08x (0x%08x)\n",
		    availmem_regions[i].mr_start,
		    availmem_regions[i].mr_start + availmem_regions[i].mr_size,
		    availmem_regions[i].mr_size);

		phys_avail[j] = availmem_regions[i].mr_start;
		phys_avail[j + 1] = availmem_regions[i].mr_start +
		    availmem_regions[i].mr_size;
	}
	phys_avail[j] = 0;
	phys_avail[j + 1] = 0;
}

void *
initarm(void *mdp, void *unused __unused)
{
	struct pv_addr kernel_l1pt;
	vm_offset_t freemempos, l2_start, lastaddr;
	uint32_t memsize, l2size;
	struct bi_mem_region *mr;
	void *kmdp;
	u_int l1pagetable;
	int i = 0;

	kmdp = NULL;
	lastaddr = 0;
	memsize = 0;

	set_cpufuncs();

	/*
	 * Mask metadata pointer: it is supposed to be on page boundary. If
	 * the first argument (mdp) doesn't point to a valid address the
	 * bootloader must have passed us something else than the metadata
	 * ptr... In this case we want to fall back to some built-in settings.
	 */
	mdp = (void *)((uint32_t)mdp & ~PAGE_MASK);

	/* Parse metadata and fetch parameters */
	if (mdp != NULL) {
		preload_metadata = mdp;
		kmdp = preload_search_by_type("elf kernel");
		if (kmdp != NULL) {
			bootinfo = (struct bootinfo *)preload_search_info(kmdp,
			    MODINFO_METADATA|MODINFOMD_BOOTINFO);

			boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
			kern_envp = MD_FETCH(kmdp, MODINFOMD_ENVP, char *);
			lastaddr = MD_FETCH(kmdp, MODINFOMD_KERNEND, vm_offset_t);
		}

		/* Initialize memory regions table */
		mr = bootinfo_mr();
		for (i = 0; i < bootinfo->bi_mem_reg_no; i++, mr++) {
			if (i == MEM_REGIONS)
				break;
			availmem_regions[i].mr_start = mr->mem_base;
			availmem_regions[i].mr_size = mr->mem_size;
			memsize += mr->mem_size;
		}
		availmem_regions_sz = i;
	} else {
		/* Fall back to hardcoded boothowto flags and metadata. */
		boothowto = RB_VERBOSE | RB_SINGLE;
		lastaddr = fake_preload_metadata();

		/*
		 * Assume a single memory region of size specified in board
		 * configuration file.
		 */
		memsize = PHYSMEM_SIZE;
	}

	/*
	 * If memsize is invalid, we can neither proceed nor panic (too
	 * early for console output).
	 */
	if (memsize == 0)
		while (1);

	/* Platform-specific initialisation */
	if (platform_pmap_init() != 0)
		return (NULL);

	pcpu_init(pcpup, 0, sizeof(struct pcpu));
	PCPU_SET(curthread, &thread0);

	/* Calculate number of L2 tables needed for mapping vm_page_array */
	l2size = (memsize / PAGE_SIZE) * sizeof(struct vm_page);
	l2size = (l2size >> L1_S_SHIFT) + 1;

	/*
	 * Add one table for end of kernel map, one for stacks, msgbuf and
	 * L1 and L2 tables map and one for vectors map.
	 */
	l2size += 3;

	/* Make it divisible by 4 */
	l2size = (l2size + 3) & ~3;

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

	for (i = 0; i < l2size; ++i) {
		if (!(i % (PAGE_SIZE / L2_TABLE_SIZE_REAL))) {
			valloc_pages(kernel_pt_table[i],
			    L2_TABLE_SIZE / PAGE_SIZE);
		} else {
			kernel_pt_table[i].pv_va = freemempos -
			    (i % (PAGE_SIZE / L2_TABLE_SIZE_REAL)) *
			    L2_TABLE_SIZE_REAL;
			kernel_pt_table[i].pv_pa =
			    kernel_pt_table[i].pv_va - KERNVIRTADDR +
			    KERNPHYSADDR;
		}
	}
	/*
	 * Allocate a page for the system page mapped to 0x00000000
	 * or 0xffff0000. This page will just contain the system vectors
	 * and can be shared by all processes.
	 */
	valloc_pages(systempage, 1);

	/* Allocate stacks for all modes */
	valloc_pages(irqstack, IRQ_STACK_SIZE);
	valloc_pages(abtstack, ABT_STACK_SIZE);
	valloc_pages(undstack, UND_STACK_SIZE);
	valloc_pages(kernelstack, KSTACK_PAGES);
	valloc_pages(msgbufpv, round_page(MSGBUF_SIZE) / PAGE_SIZE);

	/*
	 * Now we start construction of the L1 page table
	 * We start by mapping the L2 page tables into the L1.
	 * This means that we can replace L1 mappings later on if necessary
	 */
	l1pagetable = kernel_l1pt.pv_va;

	/*
	 * Try to map as much as possible of kernel text and data using
	 * 1MB section mapping and for the rest of initial kernel address
	 * space use L2 coarse tables.
	 *
	 * Link L2 tables for mapping remainder of kernel (modulo 1MB)
	 * and kernel structures
	 */
	l2_start = lastaddr & ~(L1_S_OFFSET);
	for (i = 0 ; i < l2size - 1; i++)
		pmap_link_l2pt(l1pagetable, l2_start + i * L1_S_SIZE,
		    &kernel_pt_table[KERNEL_PT_KERN + i]);

	pmap_curmaxkvaddr = l2_start + (l2size - 1) * L1_S_SIZE;
	
	/* Map kernel code and data */
	pmap_map_chunk(l1pagetable, KERNVIRTADDR, KERNPHYSADDR,
	   (((uint32_t)(lastaddr) - KERNVIRTADDR) + PAGE_MASK) & ~PAGE_MASK,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);


	/* Map L1 directory and allocated L2 page tables */
	pmap_map_chunk(l1pagetable, kernel_l1pt.pv_va, kernel_l1pt.pv_pa,
	    L1_TABLE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);

	pmap_map_chunk(l1pagetable, kernel_pt_table[0].pv_va,
	    kernel_pt_table[0].pv_pa,
	    L2_TABLE_SIZE_REAL * l2size,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);

	/* Map allocated stacks and msgbuf */
	pmap_map_chunk(l1pagetable, irqstack.pv_va, irqstack.pv_pa,
	    freemempos - irqstack.pv_va,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

	/* Link and map the vector page */
	pmap_link_l2pt(l1pagetable, ARM_VECTORS_HIGH,
	    &kernel_pt_table[KERNEL_PT_SYS]);
	pmap_map_entry(l1pagetable, ARM_VECTORS_HIGH, systempage.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

	pmap_devmap_bootstrap(l1pagetable, pmap_devmap_bootstrap_table);
	cpu_domains((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL * 2)) |
	    DOMAIN_CLIENT);
	setttb(kernel_l1pt.pv_pa);
	cpu_tlb_flushID();
	cpu_domains(DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL * 2));
	cninit();
	physmem = memsize / PAGE_SIZE;

	debugf("initarm: console initialized\n");
	debugf(" arg1 mdp = 0x%08x\n", (uint32_t)mdp);
	debugf(" boothowto = 0x%08x\n", boothowto);
	print_bootinfo();
	print_kernel_section_addr();
	print_kenv();

	/*
	 * Re-initialise decode windows
	 */
	if (soc_decode_win() != 0)
		printf("WARNING: could not re-initialise decode windows! "
		    "Running with existing settings...\n");
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

	proc_linkup0(&proc0, &thread0);
	thread0.td_kstack = kernelstack.pv_va;
	thread0.td_kstack_pages = KSTACK_PAGES;
	thread0.td_pcb = (struct pcb *)
	    (thread0.td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	thread0.td_pcb->pcb_flags = 0;
	thread0.td_frame = &proc0_tf;
	pcpup->pc_curpcb = thread0.td_pcb;

	arm_vector_init(ARM_VECTORS_HIGH, ARM_VEC_ALL);

	dump_avail[0] = KERNPHYSADDR;
	dump_avail[1] = KERNPHYSADDR + memsize;
	dump_avail[2] = 0;
	dump_avail[3] = 0;

	pmap_bootstrap(freemempos, pmap_bootstrap_lastaddr, &kernel_l1pt);
	msgbufp = (void *)msgbufpv.pv_va;
	msgbufinit(msgbufp, MSGBUF_SIZE);
	mutex_init();

	/*
	 * Prepare map of physical memory regions available to vm subsystem.
	 * If metadata pointer doesn't point to a valid address, use hardcoded
	 * values.
	 */
	physmap_init((mdp != NULL) ? 0 : 1);

	/* Do basic tuning, hz etc */
	init_param1();
	init_param2(physmem);
	kdb_init();
	return ((void *)(kernelstack.pv_va + USPACE_SVC_STACK_TOP -
	    sizeof(struct pcb)));
}

struct arm32_dma_range *
bus_dma_get_range(void)
{

	return (NULL);
}

int
bus_dma_get_range_nb(void)
{

	return (0);
}
