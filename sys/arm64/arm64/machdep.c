/*-
 * Copyright (c) 2014 Andrew Turner
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/cpu.h>
#include <sys/efi.h>
#include <sys/imgact.h>
#include <sys/kdb.h> 
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/reboot.h>
#include <sys/rwlock.h>
#include <sys/signalvar.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_pager.h>

#include <machine/cpu.h>
#include <machine/devmap.h>
#include <machine/machdep.h>
#include <machine/metadata.h>
#include <machine/pcb.h>
#include <machine/reg.h>
#include <machine/vmparam.h>

#include <dev/ofw/openfirm.h>

#include "opt_platform.h"

struct pcpu __pcpu[MAXCPU];
struct pcpu *pcpup = &__pcpu[0];

static struct trapframe proc0_tf;

vm_paddr_t phys_avail[PHYS_AVAIL_SIZE];

int early_boot = 1;
int cold = 1;
long realmem = 0;

#define	PHYSMAP_SIZE	(2 * (VM_PHYSSEG_MAX - 1))
vm_paddr_t physmap[PHYSMAP_SIZE];
u_int physmap_idx;

struct kva_md_info kmi;

static void
cpu_startup(void *dummy)
{
	vm_ksubmap_init(&kmi);
	bufinit();
	vm_pager_bufferinit();
}

SYSINIT(cpu, SI_SUB_CPU, SI_ORDER_FIRST, cpu_startup, NULL);

void
bzero(void *buf, size_t len)
{
	uint8_t *p;

	p = buf;
	while(len-- > 0)
		*p++ = 0;
}

int
fill_regs(struct thread *td, struct reg *regs)
{

	panic("fill_regs");
}

int
set_regs(struct thread *td, struct reg *regs)
{

	panic("set_regs");
}

int
fill_fpregs(struct thread *td, struct fpreg *regs)
{

	panic("fill_fpregs");
}

int
set_fpregs(struct thread *td, struct fpreg *regs)
{

	panic("set_fpregs");
}

int
fill_dbregs(struct thread *td, struct dbreg *regs)
{

	panic("fill_dbregs");
}

int
set_dbregs(struct thread *td, struct dbreg *regs)
{

	panic("set_dbregs");
}

void
DELAY(int delay)
{

	/* TODO: Implement... */
}

int
ptrace_set_pc(struct thread *td, u_long addr)
{

	panic("ptrace_set_pc");
	return (0);
}

int
ptrace_single_step(struct thread *td)
{

	/* TODO; */
	return (0);
}

int
ptrace_clear_single_step(struct thread *td)
{

	/* TODO; */
	return (0);
}

void
exec_setregs(struct thread *td, struct image_params *imgp, u_long stack)
{

	panic("exec_setregs");
}

int
get_mcontext(struct thread *td, mcontext_t *mcp, int clear_ret)
{

	panic("get_mcontext");
}

int
set_mcontext(struct thread *td, const mcontext_t *mcp)
{

	panic("set_mcontext");
}

void
cpu_idle(int busy)
{

	/* Insert code to halt (until next interrupt) for the idle loop. */
}

void
cpu_halt(void)
{

	panic("cpu_halt");
}

/*
 * Flush the D-cache for non-DMA I/O so that the I-cache can
 * be made coherent later.
 */
void
cpu_flush_dcache(void *ptr, size_t len)
{

	/* TBD */
}

/* Get current clock frequency for the given CPU ID. */
int
cpu_est_clockrate(int cpu_id, uint64_t *rate)
{

	panic("cpu_est_clockrate");
}

void
cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{
}

void
spinlock_enter(void)
{
	struct thread *td;
	register_t daif;

	td = curthread;
	if (td->td_md.md_spinlock_count == 0) {
		daif = intr_disable();
		td->td_md.md_spinlock_count = 1;
		td->td_md.md_saved_daif = daif;
	} else
		td->td_md.md_spinlock_count++;
	critical_enter();
}

void
spinlock_exit(void)
{
	struct thread *td;
	register_t daif;

	td = curthread;
	critical_exit();
	daif = td->td_md.md_saved_daif;
	td->td_md.md_spinlock_count--;
	if (td->td_md.md_spinlock_count == 0)
		intr_restore(daif);
}

#ifndef	_SYS_SYSPROTO_H_
struct sigreturn_args {
	ucontext_t *ucp;
};
#endif

int
sys_sigreturn(struct thread *td, struct sigreturn_args *uap)
{

	panic("sys_sigreturn");
}

/*
 * Construct a PCB from a trapframe. This is called from kdb_trap() where
 * we want to start a backtrace from the function that caused us to enter
 * the debugger. We have the context in the trapframe, but base the trace
 * on the PCB. The PCB doesn't have to be perfect, as long as it contains
 * enough for a backtrace.
 */
void
makectx(struct trapframe *tf, struct pcb *pcb)
{

	panic("makectx");
}

void
sendsig(sig_t catcher, ksiginfo_t *ksi, sigset_t *mask)
{

	panic("sendsig");
}

static void
init_proc0(vm_offset_t kstack)
{
	proc_linkup0(&proc0, &thread0);
	thread0.td_kstack = kstack;
	thread0.td_pcb = (struct pcb *)
	   (thread0.td_kstack + KSTACK_PAGES * PAGE_SIZE) - 1;
	thread0.td_frame = &proc0_tf;
	pcpup->pc_curpcb = thread0.td_pcb;
}

#ifdef EARLY_PRINTF
static void 
foundation_early_putc(int c)
{
	volatile uint32_t *uart = (uint32_t*)0x1c090000;

	/* TODO: Wait for space in the fifo */
	uart[0] = c;
}

early_putc_t *early_putc = foundation_early_putc;
#endif

typedef struct {
	uint32_t type;
	uint64_t phys_start;
	uint64_t virt_start;
	uint64_t num_pages;
	uint64_t attr;
} EFI_MEMORY_DESCRIPTOR;

static int
add_physmap_entry(uint64_t base, uint64_t length, vm_paddr_t *physmap,
    u_int *physmap_idxp)
{
	u_int i, insert_idx, physmap_idx;

	physmap_idx = *physmap_idxp;

	if (length == 0)
		return (1);

	/*
	 * Find insertion point while checking for overlap.  Start off by
	 * assuming the new entry will be added to the end.
	 */
	insert_idx = physmap_idx + 2;
	for (i = 0; i <= physmap_idx; i += 2) {
		if (base < physmap[i + 1]) {
			if (base + length <= physmap[i]) {
				insert_idx = i;
				break;
			}
			if (boothowto & RB_VERBOSE)
				printf(
		    "Overlapping memory regions, ignoring second region\n");
			return (1);
		}
	}

	/* See if we can prepend to the next entry. */
	if (insert_idx <= physmap_idx && base + length == physmap[insert_idx]) {
		physmap[insert_idx] = base;
		return (1);
	}

	/* See if we can append to the previous entry. */
	if (insert_idx > 0 && base == physmap[insert_idx - 1]) {
		physmap[insert_idx - 1] += length;
		return (1);
	}

	physmap_idx += 2;
	*physmap_idxp = physmap_idx;
	if (physmap_idx == PHYSMAP_SIZE) {
		printf(
		"Too many segments in the physical address map, giving up\n");
		return (0);
	}

	/*
	 * Move the last 'N' entries down to make room for the new
	 * entry if needed.
	 */
	for (i = physmap_idx; i > insert_idx; i -= 2) {
		physmap[i] = physmap[i - 2];
		physmap[i + 1] = physmap[i - 1];
	}

	/* Insert the new entry. */
	physmap[insert_idx] = base;
	physmap[insert_idx + 1] = base + length;
	return (1);
}

#define efi_next_descriptor(ptr, size) \
	((struct efi_md *)(((uint8_t *) ptr) + size))

static void
add_efi_map_entries(struct efi_map_header *efihdr, vm_paddr_t *physmap,
    u_int *physmap_idx)
{
	struct efi_md *map, *p;
	const char *type;
	size_t efisz;
	int ndesc, i;

	static const char *types[] = {
		"Reserved",
		"LoaderCode",
		"LoaderData",
		"BootServicesCode",
		"BootServicesData",
		"RuntimeServicesCode",
		"RuntimeServicesData",
		"ConventionalMemory",
		"UnusableMemory",
		"ACPIReclaimMemory",
		"ACPIMemoryNVS",
		"MemoryMappedIO",
		"MemoryMappedIOPortSpace",
		"PalCode"
	};

	/*
	 * Memory map data provided by UEFI via the GetMemoryMap
	 * Boot Services API.
	 */
	efisz = (sizeof(struct efi_map_header) + 0xf) & ~0xf;
	map = (struct efi_md *)((uint8_t *)efihdr + efisz); 

	if (efihdr->descriptor_size == 0)
		return;
	ndesc = efihdr->memory_size / efihdr->descriptor_size;

	if (boothowto & RB_VERBOSE)
		printf("%23s %12s %12s %8s %4s\n",
		    "Type", "Physical", "Virtual", "#Pages", "Attr");

	for (i = 0, p = map; i < ndesc; i++,
	    p = efi_next_descriptor(p, efihdr->descriptor_size)) {
		if (boothowto & RB_VERBOSE) {
			if (p->md_type <= EFI_MD_TYPE_PALCODE)
				type = types[p->md_type];
			else
				type = "<INVALID>";
			printf("%23s %012llx %12p %08llx ", type, p->md_phys,
			    p->md_virt, p->md_pages);
			if (p->md_attr & EFI_MD_ATTR_UC)
				printf("UC ");
			if (p->md_attr & EFI_MD_ATTR_WC)
				printf("WC ");
			if (p->md_attr & EFI_MD_ATTR_WT)
				printf("WT ");
			if (p->md_attr & EFI_MD_ATTR_WB)
				printf("WB ");
			if (p->md_attr & EFI_MD_ATTR_UCE)
				printf("UCE ");
			if (p->md_attr & EFI_MD_ATTR_WP)
				printf("WP ");
			if (p->md_attr & EFI_MD_ATTR_RP)
				printf("RP ");
			if (p->md_attr & EFI_MD_ATTR_XP)
				printf("XP ");
			if (p->md_attr & EFI_MD_ATTR_RT)
				printf("RUNTIME");
			printf("\n");
		}

		switch (p->md_type) {
		case EFI_MD_TYPE_CODE:
		case EFI_MD_TYPE_DATA:
		case EFI_MD_TYPE_BS_CODE:
		case EFI_MD_TYPE_BS_DATA:
		case EFI_MD_TYPE_FREE:
			/*
			 * We're allowed to use any entry with these types.
			 */
			break;
		default:
			continue;
		}

		if (!add_physmap_entry(p->md_phys, (p->md_pages * PAGE_SIZE),
		    physmap, physmap_idx))
			break;
	}
}

#ifdef FDT
static void
try_load_dtb(caddr_t kmdp)
{
	vm_offset_t dtboff;
	void *dtbp;

	dtboff = MD_FETCH(kmdp, MODINFOMD_DTB_OFF, vm_offset_t);
	if (dtboff == 0)
		return;

	dtbp = (void *)(KERNBASE + dtboff);
	printf("dtbp = %llx\n", *(uint64_t *)(KERNBASE + dtboff));

	if (OF_install(OFW_FDT, 0) == FALSE)
		panic("Cannot install FDT");

	if (OF_init((void *)dtbp) != 0)
		panic("OF_init failed with the found device tree");
}
#endif

void
initarm(struct arm64_bootparams *abp)
{
	struct efi_map_header *efihdr;
	vm_offset_t lastaddr;
	caddr_t kmdp;
	vm_paddr_t mem_len;
	int i;

	printf("In initarm on arm64\n");

	/* Set the module data location */
	preload_metadata = (caddr_t)(uintptr_t)(abp->modulep);

	/* Find the kernel address */
	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");

	boothowto = MD_FETCH(kmdp, MODINFOMD_HOWTO, int);
#ifdef FDT
	try_load_dtb(kmdp);
#endif

	/* Find the address to start allocating from */
	lastaddr = MD_FETCH(kmdp, MODINFOMD_KERNEND, vm_offset_t);

	/* Load the physical memory ranges */
	physmap_idx = 0;
	efihdr = (struct efi_map_header *)preload_search_info(kmdp,
	    MODINFO_METADATA | MODINFOMD_EFI_MAP);
	add_efi_map_entries(efihdr, physmap, &physmap_idx);

	/* Print the memory map */
	mem_len = 0;
	for (i = 0; i <= physmap_idx; i += 2) {
		mem_len += physmap[i + 1] - physmap[i];
		printf("%llx - %llx\n", physmap[i], physmap[i + 1]);
	}
	printf("Total = %llx\n", mem_len);

	/* Set the pcpu data, this is needed by pmap_bootstrap */
	set_curthread(&thread0);
	pcpu_init(pcpup, 0, sizeof(struct pcpu));
	PCPU_SET(curthread, &thread0);

	/* Do basic tuning, hz etc */
	init_param1();

	/* Bootstrap enough of pmap  to enter the kernel proper */
	pmap_bootstrap(abp->kern_l1pt, KERNBASE - abp->kern_delta,
	    lastaddr - KERNBASE);

	arm_devmap_bootstrap(0, NULL);

	cninit();

	init_proc0(abp->kern_stack);
	mutex_init();
	init_param2(physmem);
	kdb_init();

	early_boot = 0;
	printf("End initarm\n");
}

