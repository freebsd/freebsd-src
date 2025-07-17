/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1996, by Steve Passe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include "opt_acpi.h"
#include "opt_apic.h"
#include "opt_cpu.h"
#include "opt_kstack_pages.h"
#include "opt_pmap.h"
#include "opt_sched.h"
#include "opt_smp.h"

#if !defined(lint)
#if !defined(SMP)
#error How did you get here?
#endif

#ifndef DEV_APIC
#error The apic device is required for SMP, add "device apic" to your config file.
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cons.h>	/* cngetc() */
#include <sys/cpuset.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

#include <x86/apicreg.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/cputypes.h>
#include <x86/mca.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/smp.h>
#include <machine/specialreg.h>
#include <x86/ucode.h>

#ifdef DEV_ACPI
#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>
#endif

#define WARMBOOT_TARGET		0
#define WARMBOOT_OFF		(PMAP_MAP_LOW + 0x0467)
#define WARMBOOT_SEG		(PMAP_MAP_LOW + 0x0469)

#define CMOS_REG		(0x70)
#define CMOS_DATA		(0x71)
#define BIOS_RESET		(0x0f)
#define BIOS_WARM		(0x0a)

/*
 * this code MUST be enabled here and in mpboot.s.
 * it follows the very early stages of AP boot by placing values in CMOS ram.
 * it NORMALLY will never be needed and thus the primitive method for enabling.
 *
#define CHECK_POINTS
 */

#if defined(CHECK_POINTS)
#define CHECK_READ(A)	 (outb(CMOS_REG, (A)), inb(CMOS_DATA))
#define CHECK_WRITE(A,D) (outb(CMOS_REG, (A)), outb(CMOS_DATA, (D)))

#define CHECK_INIT(D);				\
	CHECK_WRITE(0x34, (D));			\
	CHECK_WRITE(0x35, (D));			\
	CHECK_WRITE(0x36, (D));			\
	CHECK_WRITE(0x37, (D));			\
	CHECK_WRITE(0x38, (D));			\
	CHECK_WRITE(0x39, (D));

#define CHECK_PRINT(S);				\
	printf("%s: %d, %d, %d, %d, %d, %d\n",	\
	   (S),					\
	   CHECK_READ(0x34),			\
	   CHECK_READ(0x35),			\
	   CHECK_READ(0x36),			\
	   CHECK_READ(0x37),			\
	   CHECK_READ(0x38),			\
	   CHECK_READ(0x39));

#else				/* CHECK_POINTS */

#define CHECK_INIT(D)
#define CHECK_PRINT(S)
#define CHECK_WRITE(A, D)

#endif				/* CHECK_POINTS */

/*
 * Local data and functions.
 */

static void	install_ap_tramp(void);
static int	start_all_aps(void);
static int	start_ap(int apic_id);

static char *ap_copyout_buf;
static char *ap_tramp_stack_base;

unsigned int boot_address;

#define MiB(v)	(v ## ULL << 20)

/* Allocate memory for the AP trampoline. */
void
alloc_ap_trampoline(vm_paddr_t *physmap, unsigned int *physmap_idx)
{
	unsigned int i;
	bool allocated;

	allocated = false;
	for (i = *physmap_idx; i <= *physmap_idx; i -= 2) {
		/*
		 * Find a memory region big enough and below the 1MB boundary
		 * for the trampoline code.
		 * NB: needs to be page aligned.
		 */
		if (physmap[i] >= MiB(1) ||
		    (trunc_page(physmap[i + 1]) - round_page(physmap[i])) <
		    round_page(bootMP_size))
			continue;

		allocated = true;
		/*
		 * Try to steal from the end of the region to mimic previous
		 * behaviour, else fallback to steal from the start.
		 */
		if (physmap[i + 1] < MiB(1)) {
			boot_address = trunc_page(physmap[i + 1]);
			if ((physmap[i + 1] - boot_address) < bootMP_size)
				boot_address -= round_page(bootMP_size);
			physmap[i + 1] = boot_address;
		} else {
			boot_address = round_page(physmap[i]);
			physmap[i] = boot_address + round_page(bootMP_size);
		}
		if (physmap[i] == physmap[i + 1] && *physmap_idx != 0) {
			memmove(&physmap[i], &physmap[i + 2],
			    sizeof(*physmap) * (*physmap_idx - i + 2));
			*physmap_idx -= 2;
		}
		break;
	}

	if (!allocated) {
		boot_address = basemem * 1024 - bootMP_size;
		if (bootverbose)
			printf(
"Cannot find enough space for the boot trampoline, placing it at %#x",
			    boot_address);
	}
}

/*
 * Initialize the IPI handlers and start up the AP's.
 */
void
cpu_mp_start(void)
{
	int i;

	/* Initialize the logical ID to APIC ID table. */
	for (i = 0; i < MAXCPU; i++) {
		cpu_apic_ids[i] = -1;
	}

	/* Install an inter-CPU IPI for TLB invalidation */
	setidt(IPI_INVLTLB, IDTVEC(invltlb),
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(IPI_INVLPG, IDTVEC(invlpg),
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(IPI_INVLRNG, IDTVEC(invlrng),
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

	/* Install an inter-CPU IPI for cache invalidation. */
	setidt(IPI_INVLCACHE, IDTVEC(invlcache),
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

	/* Install an inter-CPU IPI for all-CPU rendezvous */
	setidt(IPI_RENDEZVOUS, IDTVEC(rendezvous),
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

	/* Install generic inter-CPU IPI handler */
	setidt(IPI_BITMAP_VECTOR, IDTVEC(ipi_intr_bitmap_handler),
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

	/* Install an inter-CPU IPI for CPU stop/restart */
	setidt(IPI_STOP, IDTVEC(cpustop),
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

	/* Install an inter-CPU IPI for CPU suspend/resume */
	setidt(IPI_SUSPEND, IDTVEC(cpususpend),
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

	/* Install an IPI for calling delayed SWI */
	setidt(IPI_SWI, IDTVEC(ipi_swi),
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));

	/* Set boot_cpu_id if needed. */
	if (boot_cpu_id == -1) {
		boot_cpu_id = PCPU_GET(apic_id);
		cpu_info[boot_cpu_id].cpu_bsp = 1;
	} else
		KASSERT(boot_cpu_id == PCPU_GET(apic_id),
		    ("BSP's APIC ID doesn't match boot_cpu_id"));

	/* Probe logical/physical core configuration. */
	topo_probe();

	assign_cpu_ids();

	/* Start each Application Processor */
	start_all_aps();

	set_interrupt_apic_ids();

#if defined(DEV_ACPI) && MAXMEMDOM > 1
	acpi_pxm_set_cpu_locality();
#endif
}

/*
 * AP CPU's call this to initialize themselves.
 */
void
init_secondary(void)
{
	struct pcpu *pc;
	struct i386tss *common_tssp;
	struct region_descriptor r_gdt, r_idt;
	int gsel_tss, myid, x;
	u_int cr0;

	/* bootAP is set in start_ap() to our ID. */
	myid = bootAP;

	/* Update microcode before doing anything else. */
	ucode_load_ap(myid);

	/* Get per-cpu data */
	pc = &__pcpu[myid];

	/* prime data page for it to use */
	pcpu_init(pc, myid, sizeof(struct pcpu));
	dpcpu_init(dpcpu, myid);
	pc->pc_apic_id = cpu_apic_ids[myid];
	pc->pc_prvspace = pc;
	pc->pc_curthread = 0;
	pc->pc_common_tssp = common_tssp = &(__pcpu[0].pc_common_tssp)[myid];

	fix_cpuid();

	gdt_segs[GPRIV_SEL].ssd_base = (int)pc;
	gdt_segs[GPROC0_SEL].ssd_base = (int)common_tssp;
	gdt_segs[GLDT_SEL].ssd_base = (int)ldt;

	for (x = 0; x < NGDT; x++) {
		ssdtosd(&gdt_segs[x], &gdt[myid * NGDT + x].sd);
	}

	r_gdt.rd_limit = NGDT * sizeof(gdt[0]) - 1;
	r_gdt.rd_base = (int) &gdt[myid * NGDT];
	lgdt(&r_gdt);			/* does magic intra-segment return */

	r_idt.rd_limit = sizeof(struct gate_descriptor) * NIDT - 1;
	r_idt.rd_base = (int)idt;
	lidt(&r_idt);

	lldt(_default_ldt);
	PCPU_SET(currentldt, _default_ldt);

	PCPU_SET(trampstk, (uintptr_t)ap_tramp_stack_base + TRAMP_STACK_SZ -
	    VM86_STACK_SPACE);

	gsel_tss = GSEL(GPROC0_SEL, SEL_KPL);
	gdt[myid * NGDT + GPROC0_SEL].sd.sd_type = SDT_SYS386TSS;
	common_tssp->tss_esp0 = PCPU_GET(trampstk);
	common_tssp->tss_ss0 = GSEL(GDATA_SEL, SEL_KPL);
	common_tssp->tss_ioopt = sizeof(struct i386tss) << 16;
	PCPU_SET(tss_gdt, &gdt[myid * NGDT + GPROC0_SEL].sd);
	PCPU_SET(common_tssd, *PCPU_GET(tss_gdt));
	ltr(gsel_tss);

	PCPU_SET(fsgs_gdt, &gdt[myid * NGDT + GUFS_SEL].sd);
	PCPU_SET(copyout_buf, ap_copyout_buf);

	/*
	 * Set to a known state:
	 * Set by mpboot.s: CR0_PG, CR0_PE
	 * Set by cpu_setregs: CR0_NE, CR0_MP, CR0_TS, CR0_WP, CR0_AM
	 */
	cr0 = rcr0();
	cr0 &= ~(CR0_CD | CR0_NW | CR0_EM);
	load_cr0(cr0);
	CHECK_WRITE(0x38, 5);

	/* signal our startup to the BSP. */
	mp_naps++;
	CHECK_WRITE(0x39, 6);

	/* Spin until the BSP releases the AP's. */
	while (atomic_load_acq_int(&aps_ready) == 0)
		ia32_pause();

	/* BSP may have changed PTD while we were waiting */
	invltlb();

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
	lidt(&r_idt);
#endif

	init_secondary_tail();
}

/*
 * start each AP in our list
 */
#define TMPMAP_START 1
static int
start_all_aps(void)
{
	u_char mpbiosreason;
	u_int32_t mpbioswarmvec;
	int apic_id, cpu;

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	pmap_remap_lower(true);

	/* install the AP 1st level boot code */
	install_ap_tramp();

	/* save the current value of the warm-start vector */
	mpbioswarmvec = *((u_int32_t *) WARMBOOT_OFF);
	outb(CMOS_REG, BIOS_RESET);
	mpbiosreason = inb(CMOS_DATA);

	/* take advantage of the P==V mapping for PTD[0] for AP boot */

	/* start each AP */
	for (cpu = 1; cpu < mp_ncpus; cpu++) {
		apic_id = cpu_apic_ids[cpu];

		/* allocate and set up a boot stack data page */
		bootstacks[cpu] = kmem_malloc(kstack_pages * PAGE_SIZE,
		    M_WAITOK | M_ZERO);
		dpcpu = kmem_malloc(DPCPU_SIZE, M_WAITOK | M_ZERO);
		/* setup a vector to our boot code */
		*((volatile u_short *) WARMBOOT_OFF) = WARMBOOT_TARGET;
		*((volatile u_short *) WARMBOOT_SEG) = (boot_address >> 4);
		outb(CMOS_REG, BIOS_RESET);
		outb(CMOS_DATA, BIOS_WARM);	/* 'warm-start' */

		bootSTK = (char *)bootstacks[cpu] + kstack_pages *
		    PAGE_SIZE - 4;
		bootAP = cpu;

		ap_tramp_stack_base = pmap_trm_alloc(TRAMP_STACK_SZ, M_NOWAIT);
		ap_copyout_buf = pmap_trm_alloc(TRAMP_COPYOUT_SZ, M_NOWAIT);

		/* attempt to start the Application Processor */
		CHECK_INIT(99);	/* setup checkpoints */
		if (!start_ap(apic_id)) {
			printf("AP #%d (PHY# %d) failed!\n", cpu, apic_id);
			CHECK_PRINT("trace");	/* show checkpoints */
			/* better panic as the AP may be running loose */
			printf("panic y/n? [y] ");
			if (cngetc() != 'n')
				panic("bye-bye");
		}
		CHECK_PRINT("trace");		/* show checkpoints */

		CPU_SET(cpu, &all_cpus);	/* record AP in CPU map */
	}

	pmap_remap_lower(false);

	/* restore the warmstart vector */
	*(u_int32_t *) WARMBOOT_OFF = mpbioswarmvec;

	outb(CMOS_REG, BIOS_RESET);
	outb(CMOS_DATA, mpbiosreason);

	/* number of APs actually started */
	return mp_naps;
}

/*
 * load the 1st level AP boot code into base memory.
 */

/* targets for relocation */
extern void bigJump(void);
extern void bootCodeSeg(void);
extern void bootDataSeg(void);
extern void MPentry(void);
extern u_int MP_GDT;
extern u_int mp_gdtbase;

static void
install_ap_tramp(void)
{
	int     x;
	int     size = *(int *) ((u_long) & bootMP_size);
	vm_offset_t va = boot_address;
	u_char *src = (u_char *) ((u_long) bootMP);
	u_char *dst = (u_char *) va;
	u_int   boot_base = (u_int) bootMP;
	u_int8_t *dst8;
	u_int16_t *dst16;
	u_int32_t *dst32;

	KASSERT (size <= PAGE_SIZE,
	    ("'size' do not fit into PAGE_SIZE, as expected."));
	pmap_kenter(va, boot_address);
	pmap_invalidate_page (kernel_pmap, va);
	for (x = 0; x < size; ++x)
		*dst++ = *src++;

	/*
	 * modify addresses in code we just moved to basemem. unfortunately we
	 * need fairly detailed info about mpboot.s for this to work.  changes
	 * to mpboot.s might require changes here.
	 */

	/* boot code is located in KERNEL space */
	dst = (u_char *) va;

	/* modify the lgdt arg */
	dst32 = (u_int32_t *) (dst + ((u_int) & mp_gdtbase - boot_base));
	*dst32 = boot_address + ((u_int) & MP_GDT - boot_base);

	/* modify the ljmp target for MPentry() */
	dst32 = (u_int32_t *) (dst + ((u_int) bigJump - boot_base) + 1);
	*dst32 = (u_int)MPentry;

	/* modify the target for boot code segment */
	dst16 = (u_int16_t *) (dst + ((u_int) bootCodeSeg - boot_base));
	dst8 = (u_int8_t *) (dst16 + 1);
	*dst16 = (u_int) boot_address & 0xffff;
	*dst8 = ((u_int) boot_address >> 16) & 0xff;

	/* modify the target for boot data segment */
	dst16 = (u_int16_t *) (dst + ((u_int) bootDataSeg - boot_base));
	dst8 = (u_int8_t *) (dst16 + 1);
	*dst16 = (u_int) boot_address & 0xffff;
	*dst8 = ((u_int) boot_address >> 16) & 0xff;
}

/*
 * This function starts the AP (application processor) identified
 * by the APIC ID 'physicalCpu'.  It does quite a "song and dance"
 * to accomplish this.  This is necessary because of the nuances
 * of the different hardware we might encounter.  It isn't pretty,
 * but it seems to work.
 */
static int
start_ap(int apic_id)
{
	int vector, ms;
	int cpus;

	/* calculate the vector */
	vector = (boot_address >> 12) & 0xff;

	/* used as a watchpoint to signal AP startup */
	cpus = mp_naps;

	ipi_startup(apic_id, vector);

	/* Wait up to 5 seconds for it to start. */
	for (ms = 0; ms < 5000; ms++) {
		if (mp_naps > cpus)
			return 1;	/* return SUCCESS */
		DELAY(1000);
	}
	return 0;		/* return FAILURE */
}

/*
 * Flush the TLB on other CPU's
 */

/* Variables needed for SMP tlb shootdown. */
vm_offset_t smp_tlb_addr1, smp_tlb_addr2;
pmap_t smp_tlb_pmap;
volatile uint32_t smp_tlb_generation;

/*
 * Used by pmap to request cache or TLB invalidation on local and
 * remote processors.  Mask provides the set of remote CPUs which are
 * to be signalled with the invalidation IPI.  Vector specifies which
 * invalidation IPI is used.  As an optimization, the curcpu_cb
 * callback is invoked on the calling CPU while waiting for remote
 * CPUs to complete the operation.
 *
 * The callback function is called unconditionally on the caller's
 * underlying processor, even when this processor is not set in the
 * mask.  So, the callback function must be prepared to handle such
 * spurious invocations.
 */
static void
smp_targeted_tlb_shootdown(cpuset_t mask, u_int vector, pmap_t pmap,
    vm_offset_t addr1, vm_offset_t addr2, smp_invl_cb_t curcpu_cb)
{
	cpuset_t other_cpus;
	volatile uint32_t *p_cpudone;
	uint32_t generation;
	int cpu;

	/*
	 * It is not necessary to signal other CPUs while booting or
	 * when in the debugger.
	 */
	if (kdb_active || KERNEL_PANICKED() || !smp_started) {
		curcpu_cb(pmap, addr1, addr2);
		return;
	}

	sched_pin();

	/*
	 * Check for other cpus.  Return if none.
	 */
	if (CPU_ISFULLSET(&mask)) {
		if (mp_ncpus <= 1)
			goto nospinexit;
	} else {
		CPU_CLR(PCPU_GET(cpuid), &mask);
		if (CPU_EMPTY(&mask))
			goto nospinexit;
	}

	KASSERT((read_eflags() & PSL_I) != 0,
	    ("smp_targeted_tlb_shootdown: interrupts disabled"));
	mtx_lock_spin(&smp_ipi_mtx);
	smp_tlb_addr1 = addr1;
	smp_tlb_addr2 = addr2;
	smp_tlb_pmap = pmap;
	generation = ++smp_tlb_generation;
	if (CPU_ISFULLSET(&mask)) {
		ipi_all_but_self(vector);
		other_cpus = all_cpus;
		CPU_CLR(PCPU_GET(cpuid), &other_cpus);
	} else {
		other_cpus = mask;
		ipi_selected(mask, vector);
	}
	curcpu_cb(pmap, addr1, addr2);
	CPU_FOREACH_ISSET(cpu, &other_cpus) {
		p_cpudone = &cpuid_to_pcpu[cpu]->pc_smp_tlb_done;
		while (*p_cpudone != generation)
			ia32_pause();
	}
	mtx_unlock_spin(&smp_ipi_mtx);
	sched_unpin();
	return;

nospinexit:
	curcpu_cb(pmap, addr1, addr2);
	sched_unpin();
}

void
smp_masked_invltlb(cpuset_t mask, pmap_t pmap, smp_invl_cb_t curcpu_cb)
{
	smp_targeted_tlb_shootdown(mask, IPI_INVLTLB, pmap, 0, 0, curcpu_cb);
#ifdef COUNT_XINVLTLB_HITS
	ipi_global++;
#endif
}

void
smp_masked_invlpg(cpuset_t mask, vm_offset_t addr, pmap_t pmap,
    smp_invl_cb_t curcpu_cb)
{
	smp_targeted_tlb_shootdown(mask, IPI_INVLPG, pmap, addr, 0, curcpu_cb);
#ifdef COUNT_XINVLTLB_HITS
	ipi_page++;
#endif
}

void
smp_masked_invlpg_range(cpuset_t mask, vm_offset_t addr1, vm_offset_t addr2,
    pmap_t pmap, smp_invl_cb_t curcpu_cb)
{
	smp_targeted_tlb_shootdown(mask, IPI_INVLRNG, pmap, addr1, addr2,
	    curcpu_cb);
#ifdef COUNT_XINVLTLB_HITS
	ipi_range++;
	ipi_range_size += (addr2 - addr1) / PAGE_SIZE;
#endif
}

void
smp_cache_flush(smp_invl_cb_t curcpu_cb)
{
	smp_targeted_tlb_shootdown(all_cpus, IPI_INVLCACHE, NULL, 0, 0,
	    curcpu_cb);
}

/*
 * Handlers for TLB related IPIs
 */
void
invltlb_handler(void)
{
	uint32_t generation;

	trap_check_kstack();
#ifdef COUNT_XINVLTLB_HITS
	xhits_gbl[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invltlb_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	/*
	 * Reading the generation here allows greater parallelism
	 * since invalidating the TLB is a serializing operation.
	 */
	generation = smp_tlb_generation;
	if (smp_tlb_pmap == kernel_pmap)
		invltlb_glob();
	PCPU_SET(smp_tlb_done, generation);
}

void
invlpg_handler(void)
{
	uint32_t generation;

	trap_check_kstack();
#ifdef COUNT_XINVLTLB_HITS
	xhits_pg[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invlpg_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	generation = smp_tlb_generation;	/* Overlap with serialization */
	if (smp_tlb_pmap == kernel_pmap)
		invlpg(smp_tlb_addr1);
	PCPU_SET(smp_tlb_done, generation);
}

void
invlrng_handler(void)
{
	vm_offset_t addr, addr2;
	uint32_t generation;

	trap_check_kstack();
#ifdef COUNT_XINVLTLB_HITS
	xhits_rng[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invlrng_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	addr = smp_tlb_addr1;
	addr2 = smp_tlb_addr2;
	generation = smp_tlb_generation;	/* Overlap with serialization */
	if (smp_tlb_pmap == kernel_pmap) {
		do {
			invlpg(addr);
			addr += PAGE_SIZE;
		} while (addr < addr2);
	}

	PCPU_SET(smp_tlb_done, generation);
}

void
invlcache_handler(void)
{
	uint32_t generation;

	trap_check_kstack();
#ifdef COUNT_IPIS
	(*ipi_invlcache_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	/*
	 * Reading the generation here allows greater parallelism
	 * since wbinvd is a serializing instruction.  Without the
	 * temporary, we'd wait for wbinvd to complete, then the read
	 * would execute, then the dependent write, which must then
	 * complete before return from interrupt.
	 */
	generation = smp_tlb_generation;
	wbinvd();
	PCPU_SET(smp_tlb_done, generation);
}
