/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1996, by Steve Passe
 * Copyright (c) 2003, by Peter Wemm
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
__FBSDID("$FreeBSD$");

#include "opt_acpi.h"
#include "opt_cpu.h"
#include "opt_ddb.h"
#include "opt_kstack_pages.h"
#include "opt_sched.h"
#include "opt_smp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cpuset.h>
#include <sys/domainset.h>
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
#include <vm/vm_page.h>
#include <vm/vm_phys.h>

#include <x86/apicreg.h>
#include <machine/clock.h>
#include <machine/cputypes.h>
#include <machine/cpufunc.h>
#include <x86/mca.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/smp.h>
#include <machine/specialreg.h>
#include <machine/tss.h>
#include <x86/ucode.h>
#include <machine/cpu.h>
#include <x86/init.h>

#ifdef DEV_ACPI
#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>
#endif

#define WARMBOOT_TARGET		0
#define WARMBOOT_OFF		(KERNBASE + 0x0467)
#define WARMBOOT_SEG		(KERNBASE + 0x0469)

#define CMOS_REG		(0x70)
#define CMOS_DATA		(0x71)
#define BIOS_RESET		(0x0f)
#define BIOS_WARM		(0x0a)

#define GiB(v)			(v ## ULL << 30)

#define	AP_BOOTPT_SZ		(PAGE_SIZE * 4)

/* Temporary variables for init_secondary()  */
static char *doublefault_stack;
static char *mce_stack;
static char *nmi_stack;
static char *dbg_stack;
void *bootpcpu;

extern u_int mptramp_la57;
extern u_int mptramp_nx;

/*
 * Local data and functions.
 */

static int start_ap(int apic_id, vm_paddr_t boot_address);

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

	/* Install an inter-CPU IPI for cache and TLB invalidations. */
	setidt(IPI_INVLOP, pti ? IDTVEC(invlop_pti) : IDTVEC(invlop),
	    SDT_SYSIGT, SEL_KPL, 0);

	/* Install an inter-CPU IPI for all-CPU rendezvous */
	setidt(IPI_RENDEZVOUS, pti ? IDTVEC(rendezvous_pti) :
	    IDTVEC(rendezvous), SDT_SYSIGT, SEL_KPL, 0);

	/* Install generic inter-CPU IPI handler */
	setidt(IPI_BITMAP_VECTOR, pti ? IDTVEC(ipi_intr_bitmap_handler_pti) :
	    IDTVEC(ipi_intr_bitmap_handler), SDT_SYSIGT, SEL_KPL, 0);

	/* Install an inter-CPU IPI for CPU stop/restart */
	setidt(IPI_STOP, pti ? IDTVEC(cpustop_pti) : IDTVEC(cpustop),
	    SDT_SYSIGT, SEL_KPL, 0);

	/* Install an inter-CPU IPI for CPU suspend/resume */
	setidt(IPI_SUSPEND, pti ? IDTVEC(cpususpend_pti) : IDTVEC(cpususpend),
	    SDT_SYSIGT, SEL_KPL, 0);

	/* Install an IPI for calling delayed SWI */
	setidt(IPI_SWI, pti ? IDTVEC(ipi_swi_pti) : IDTVEC(ipi_swi),
	    SDT_SYSIGT, SEL_KPL, 0);

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

	mptramp_la57 = la57;
	mptramp_nx = pg_nx != 0;
	MPASS(kernel_pmap->pm_cr3 < (1UL << 32));
	mptramp_pagetables = kernel_pmap->pm_cr3;

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
	struct nmi_pcpu *np;
	struct user_segment_descriptor *gdt;
	struct region_descriptor ap_gdt;
	u_int64_t cr0;
	int cpu, gsel_tss, x;

	/* Set by the startup code for us to use */
	cpu = bootAP;

	/* Update microcode before doing anything else. */
	ucode_load_ap(cpu);

	/* Initialize the PCPU area. */
	pc = bootpcpu;
	pcpu_init(pc, cpu, sizeof(struct pcpu));
	dpcpu_init(dpcpu, cpu);
	pc->pc_apic_id = cpu_apic_ids[cpu];
	pc->pc_prvspace = pc;
	pc->pc_curthread = 0;
	pc->pc_tssp = &pc->pc_common_tss;
	pc->pc_rsp0 = 0;
	pc->pc_pti_rsp0 = (((vm_offset_t)&pc->pc_pti_stack +
	    PC_PTI_STACK_SZ * sizeof(uint64_t)) & ~0xful);
	gdt = pc->pc_gdt;
	pc->pc_tss = (struct system_segment_descriptor *)&gdt[GPROC0_SEL];
	pc->pc_fs32p = &gdt[GUFS32_SEL];
	pc->pc_gs32p = &gdt[GUGS32_SEL];
	pc->pc_ldt = (struct system_segment_descriptor *)&gdt[GUSERLDT_SEL];
	pc->pc_ucr3_load_mask = PMAP_UCR3_NOMASK;
	/* See comment in pmap_bootstrap(). */
	pc->pc_pcid_next = PMAP_PCID_KERN + 2;
	pc->pc_pcid_gen = 1;
	pc->pc_kpmap_store.pm_pcid = PMAP_PCID_KERN;
	pc->pc_kpmap_store.pm_gen = 1;

	pc->pc_smp_tlb_gen = 1;

	/* Init tss */
	pc->pc_common_tss = __pcpu[0].pc_common_tss;
	pc->pc_common_tss.tss_iobase = sizeof(struct amd64tss) +
	    IOPERM_BITMAP_SIZE;
	pc->pc_common_tss.tss_rsp0 = 0;

	/* The doublefault stack runs on IST1. */
	np = ((struct nmi_pcpu *)&doublefault_stack[DBLFAULT_STACK_SIZE]) - 1;
	np->np_pcpu = (register_t)pc;
	pc->pc_common_tss.tss_ist1 = (long)np;

	/* The NMI stack runs on IST2. */
	np = ((struct nmi_pcpu *)&nmi_stack[NMI_STACK_SIZE]) - 1;
	np->np_pcpu = (register_t)pc;
	pc->pc_common_tss.tss_ist2 = (long)np;

	/* The MC# stack runs on IST3. */
	np = ((struct nmi_pcpu *)&mce_stack[MCE_STACK_SIZE]) - 1;
	np->np_pcpu = (register_t)pc;
	pc->pc_common_tss.tss_ist3 = (long)np;

	/* The DB# stack runs on IST4. */
	np = ((struct nmi_pcpu *)&dbg_stack[DBG_STACK_SIZE]) - 1;
	np->np_pcpu = (register_t)pc;
	pc->pc_common_tss.tss_ist4 = (long)np;

	/* Prepare private GDT */
	gdt_segs[GPROC0_SEL].ssd_base = (long)&pc->pc_common_tss;
	for (x = 0; x < NGDT; x++) {
		if (x != GPROC0_SEL && x != GPROC0_SEL + 1 &&
		    x != GUSERLDT_SEL && x != GUSERLDT_SEL + 1)
			ssdtosd(&gdt_segs[x], &gdt[x]);
	}
	ssdtosyssd(&gdt_segs[GPROC0_SEL],
	    (struct system_segment_descriptor *)&gdt[GPROC0_SEL]);
	ap_gdt.rd_limit = NGDT * sizeof(gdt[0]) - 1;
	ap_gdt.rd_base = (u_long)gdt;
	lgdt(&ap_gdt);			/* does magic intra-segment return */

	wrmsr(MSR_FSBASE, 0);		/* User value */
	wrmsr(MSR_GSBASE, (uint64_t)pc);
	wrmsr(MSR_KGSBASE, 0);		/* User value */
	fix_cpuid();

	lidt(&r_idt);

	gsel_tss = GSEL(GPROC0_SEL, SEL_KPL);
	ltr(gsel_tss);

	/*
	 * Set to a known state:
	 * Set by mpboot.s: CR0_PG, CR0_PE
	 * Set by cpu_setregs: CR0_NE, CR0_MP, CR0_TS, CR0_WP, CR0_AM
	 */
	cr0 = rcr0();
	cr0 &= ~(CR0_CD | CR0_NW | CR0_EM);
	load_cr0(cr0);

	amd64_conf_fast_syscall();

	/* signal our startup to the BSP. */
	mp_naps++;

	/* Spin until the BSP releases the AP's. */
	while (atomic_load_acq_int(&aps_ready) == 0)
		ia32_pause();

	init_secondary_tail();
}

static void
amd64_mp_alloc_pcpu(void)
{
	vm_page_t m;
	int cpu;

	/* Allocate pcpu areas to the correct domain. */
	for (cpu = 1; cpu < mp_ncpus; cpu++) {
#ifdef NUMA
		m = NULL;
		if (vm_ndomains > 1) {
			m = vm_page_alloc_noobj_domain(
			    acpi_pxm_get_cpu_locality(cpu_apic_ids[cpu]), 0);
		}
		if (m == NULL)
#endif
			m = vm_page_alloc_noobj(0);
		if (m == NULL)
			panic("cannot alloc pcpu page for cpu %d", cpu);
		pmap_qenter((vm_offset_t)&__pcpu[cpu], &m, 1);
	}
}

/*
 * start each AP in our list
 */
int
start_all_aps(void)
{
	vm_page_t m_boottramp, m_pml4, m_pdp, m_pd[4];
	pml5_entry_t old_pml45;
	pml4_entry_t *v_pml4;
	pdp_entry_t *v_pdp;
	pd_entry_t *v_pd;
	vm_paddr_t boot_address;
	u_int32_t mpbioswarmvec;
	int apic_id, cpu, domain, i;
	u_char mpbiosreason;

	amd64_mp_alloc_pcpu();
	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	MPASS(bootMP_size <= PAGE_SIZE);
	m_boottramp = vm_page_alloc_noobj_contig(0, 1, 0,
	    (1ULL << 20), /* Trampoline should be below 1M for real mode */
	    PAGE_SIZE, 0, VM_MEMATTR_DEFAULT);
	boot_address = VM_PAGE_TO_PHYS(m_boottramp);

	/* Create a transient 1:1 mapping of low 4G */
	if (la57) {
		m_pml4 = pmap_page_alloc_below_4g(true);
		v_pml4 = (pml4_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m_pml4));
	} else {
		v_pml4 = &kernel_pmap->pm_pmltop[0];
	}
	m_pdp = pmap_page_alloc_below_4g(true);
	v_pdp = (pdp_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m_pdp));
	m_pd[0] = pmap_page_alloc_below_4g(false);
	v_pd = (pd_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m_pd[0]));
	for (i = 0; i < NPDEPG; i++)
		v_pd[i] = (i << PDRSHIFT) | X86_PG_V | X86_PG_RW | X86_PG_A |
		    X86_PG_M | PG_PS;
	m_pd[1] = pmap_page_alloc_below_4g(false);
	v_pd = (pd_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m_pd[1]));
	for (i = 0; i < NPDEPG; i++)
		v_pd[i] = (NBPDP + (i << PDRSHIFT)) | X86_PG_V | X86_PG_RW |
		    X86_PG_A | X86_PG_M | PG_PS;
	m_pd[2] = pmap_page_alloc_below_4g(false);
	v_pd = (pd_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m_pd[2]));
	for (i = 0; i < NPDEPG; i++)
		v_pd[i] = (2UL * NBPDP + (i << PDRSHIFT)) | X86_PG_V |
		    X86_PG_RW | X86_PG_A | X86_PG_M | PG_PS;
	m_pd[3] = pmap_page_alloc_below_4g(false);
	v_pd = (pd_entry_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m_pd[3]));
	for (i = 0; i < NPDEPG; i++)
		v_pd[i] = (3UL * NBPDP + (i << PDRSHIFT)) | X86_PG_V |
		    X86_PG_RW | X86_PG_A | X86_PG_M | PG_PS;
	v_pdp[0] = VM_PAGE_TO_PHYS(m_pd[0]) | X86_PG_V |
	    X86_PG_RW | X86_PG_A | X86_PG_M;
	v_pdp[1] = VM_PAGE_TO_PHYS(m_pd[1]) | X86_PG_V |
	    X86_PG_RW | X86_PG_A | X86_PG_M;
	v_pdp[2] = VM_PAGE_TO_PHYS(m_pd[2]) | X86_PG_V |
	    X86_PG_RW | X86_PG_A | X86_PG_M;
	v_pdp[3] = VM_PAGE_TO_PHYS(m_pd[3]) | X86_PG_V |
	    X86_PG_RW | X86_PG_A | X86_PG_M;
	old_pml45 = kernel_pmap->pm_pmltop[0];
	if (la57) {
		kernel_pmap->pm_pmltop[0] = VM_PAGE_TO_PHYS(m_pml4) |
		    X86_PG_V | X86_PG_RW | X86_PG_A | X86_PG_M;
	}
	v_pml4[0] = VM_PAGE_TO_PHYS(m_pdp) | X86_PG_V |
	    X86_PG_RW | X86_PG_A | X86_PG_M;
	pmap_invalidate_all(kernel_pmap);

	/* copy the AP 1st level boot code */
	bcopy(mptramp_start, (void *)PHYS_TO_DMAP(boot_address), bootMP_size);
	if (bootverbose)
		printf("AP boot address %#lx\n", boot_address);

	/* save the current value of the warm-start vector */
	if (!efi_boot)
		mpbioswarmvec = *((u_int32_t *) WARMBOOT_OFF);
	outb(CMOS_REG, BIOS_RESET);
	mpbiosreason = inb(CMOS_DATA);

	/* setup a vector to our boot code */
	if (!efi_boot) {
		*((volatile u_short *)WARMBOOT_OFF) = WARMBOOT_TARGET;
		*((volatile u_short *)WARMBOOT_SEG) = (boot_address >> 4);
	}
	outb(CMOS_REG, BIOS_RESET);
	outb(CMOS_DATA, BIOS_WARM);	/* 'warm-start' */

	/* start each AP */
	domain = 0;
	for (cpu = 1; cpu < mp_ncpus; cpu++) {
		apic_id = cpu_apic_ids[cpu];
#ifdef NUMA
		if (vm_ndomains > 1)
			domain = acpi_pxm_get_cpu_locality(apic_id);
#endif
		/* allocate and set up an idle stack data page */
		bootstacks[cpu] = kmem_malloc(kstack_pages * PAGE_SIZE,
		    M_WAITOK | M_ZERO);
		doublefault_stack = kmem_malloc(DBLFAULT_STACK_SIZE,
		    M_WAITOK | M_ZERO);
		mce_stack = kmem_malloc(MCE_STACK_SIZE,
		    M_WAITOK | M_ZERO);
		nmi_stack = kmem_malloc_domainset(
		    DOMAINSET_PREF(domain), NMI_STACK_SIZE, M_WAITOK | M_ZERO);
		dbg_stack = kmem_malloc_domainset(
		    DOMAINSET_PREF(domain), DBG_STACK_SIZE, M_WAITOK | M_ZERO);
		dpcpu = kmem_malloc_domainset(DOMAINSET_PREF(domain),
		    DPCPU_SIZE, M_WAITOK | M_ZERO);

		bootpcpu = &__pcpu[cpu];
		bootSTK = (char *)bootstacks[cpu] +
		    kstack_pages * PAGE_SIZE - 8;
		bootAP = cpu;

		/* attempt to start the Application Processor */
		if (!start_ap(apic_id, boot_address)) {
			/* restore the warmstart vector */
			if (!efi_boot)
				*(u_int32_t *)WARMBOOT_OFF = mpbioswarmvec;
			panic("AP #%d (PHY# %d) failed!", cpu, apic_id);
		}

		CPU_SET(cpu, &all_cpus);	/* record AP in CPU map */
	}

	/* restore the warmstart vector */
	if (!efi_boot)
		*(u_int32_t *)WARMBOOT_OFF = mpbioswarmvec;

	outb(CMOS_REG, BIOS_RESET);
	outb(CMOS_DATA, mpbiosreason);

	/* Destroy transient 1:1 mapping */
	kernel_pmap->pm_pmltop[0] = old_pml45;
	invlpg(0);
	if (la57)
		vm_page_free(m_pml4);
	vm_page_free(m_pd[3]);
	vm_page_free(m_pd[2]);
	vm_page_free(m_pd[1]);
	vm_page_free(m_pd[0]);
	vm_page_free(m_pdp);
	vm_page_free(m_boottramp);

	/* number of APs actually started */
	return (mp_naps);
}

/*
 * This function starts the AP (application processor) identified
 * by the APIC ID 'physicalCpu'.  It does quite a "song and dance"
 * to accomplish this.  This is necessary because of the nuances
 * of the different hardware we might encounter.  It isn't pretty,
 * but it seems to work.
 */
static int
start_ap(int apic_id, vm_paddr_t boot_address)
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

/*
 * Invalidation request.  PCPU pc_smp_tlb_op uses u_int instead of the
 * enum to avoid both namespace and ABI issues (with enums).
 */
enum invl_op_codes {
      INVL_OP_TLB		= 1,
      INVL_OP_TLB_INVPCID	= 2,
      INVL_OP_TLB_INVPCID_PTI	= 3,
      INVL_OP_TLB_PCID		= 4,
      INVL_OP_PGRNG		= 5,
      INVL_OP_PGRNG_INVPCID	= 6,
      INVL_OP_PGRNG_PCID	= 7,
      INVL_OP_PG		= 8,
      INVL_OP_PG_INVPCID	= 9,
      INVL_OP_PG_PCID		= 10,
      INVL_OP_CACHE		= 11,
};

/*
 * These variables are initialized at startup to reflect how each of
 * the different kinds of invalidations should be performed on the
 * current machine and environment.
 */
static enum invl_op_codes invl_op_tlb;
static enum invl_op_codes invl_op_pgrng;
static enum invl_op_codes invl_op_pg;

/*
 * Scoreboard of IPI completion notifications from target to IPI initiator.
 *
 * Each CPU can initiate shootdown IPI independently from other CPUs.
 * Initiator enters critical section, then fills its local PCPU
 * shootdown info (pc_smp_tlb_ vars), then clears scoreboard generation
 * at location (cpu, my_cpuid) for each target cpu.  After that IPI is
 * sent to all targets which scan for zeroed scoreboard generation
 * words.  Upon finding such word the shootdown data is read from
 * corresponding cpu's pcpu, and generation is set.  Meantime initiator
 * loops waiting for all zeroed generations in scoreboard to update.
 */
static uint32_t *invl_scoreboard;

static void
invl_scoreboard_init(void *arg __unused)
{
	u_int i;

	invl_scoreboard = malloc(sizeof(uint32_t) * (mp_maxid + 1) *
	    (mp_maxid + 1), M_DEVBUF, M_WAITOK);
	for (i = 0; i < (mp_maxid + 1) * (mp_maxid + 1); i++)
		invl_scoreboard[i] = 1;

	if (pmap_pcid_enabled) {
		if (invpcid_works) {
			if (pti)
				invl_op_tlb = INVL_OP_TLB_INVPCID_PTI;
			else
				invl_op_tlb = INVL_OP_TLB_INVPCID;
			invl_op_pgrng = INVL_OP_PGRNG_INVPCID;
			invl_op_pg = INVL_OP_PG_INVPCID;
		} else {
			invl_op_tlb = INVL_OP_TLB_PCID;
			invl_op_pgrng = INVL_OP_PGRNG_PCID;
			invl_op_pg = INVL_OP_PG_PCID;
		}
	} else {
		invl_op_tlb = INVL_OP_TLB;
		invl_op_pgrng = INVL_OP_PGRNG;
		invl_op_pg = INVL_OP_PG;
	}
}
SYSINIT(invl_ops, SI_SUB_SMP - 1, SI_ORDER_ANY, invl_scoreboard_init, NULL);

static uint32_t *
invl_scoreboard_getcpu(u_int cpu)
{
	return (invl_scoreboard + cpu * (mp_maxid + 1));
}

static uint32_t *
invl_scoreboard_slot(u_int cpu)
{
	return (invl_scoreboard_getcpu(cpu) + PCPU_GET(cpuid));
}

/*
 * Used by the pmap to request cache or TLB invalidation on local and
 * remote processors.  Mask provides the set of remote CPUs that are
 * to be signalled with the invalidation IPI.  As an optimization, the
 * curcpu_cb callback is invoked on the calling CPU in a critical
 * section while waiting for the remote CPUs to complete the operation.
 *
 * The callback function is called unconditionally on the caller's
 * underlying processor, even when this processor is not set in the
 * mask.  So, the callback function must be prepared to handle such
 * spurious invocations.
 *
 * Interrupts must be enabled when calling the function with smp
 * started, to avoid deadlock with other IPIs that are protected with
 * smp_ipi_mtx spinlock at the initiator side.
 *
 * Function must be called with the thread pinned, and it unpins on
 * completion.
 */
static void
smp_targeted_tlb_shootdown(pmap_t pmap, vm_offset_t addr1, vm_offset_t addr2,
    smp_invl_cb_t curcpu_cb, enum invl_op_codes op)
{
	cpuset_t mask;
	uint32_t generation, *p_cpudone;
	int cpu;
	bool is_all;

	/*
	 * It is not necessary to signal other CPUs while booting or
	 * when in the debugger.
	 */
	if (__predict_false(kdb_active || KERNEL_PANICKED() || !smp_started))
		goto local_cb;

	KASSERT(curthread->td_pinned > 0, ("curthread not pinned"));

	/*
	 * Make a stable copy of the set of CPUs on which the pmap is active.
	 * See if we have to interrupt other CPUs.
	 */
	CPU_COPY(pmap_invalidate_cpu_mask(pmap), &mask);
	is_all = CPU_CMP(&mask, &all_cpus) == 0;
	CPU_CLR(curcpu, &mask);
	if (CPU_EMPTY(&mask))
		goto local_cb;

	/*
	 * Initiator must have interrupts enabled, which prevents
	 * non-invalidation IPIs that take smp_ipi_mtx spinlock,
	 * from deadlocking with us.  On the other hand, preemption
	 * must be disabled to pin initiator to the instance of the
	 * pcpu pc_smp_tlb data and scoreboard line.
	 */
	KASSERT((read_rflags() & PSL_I) != 0,
	    ("smp_targeted_tlb_shootdown: interrupts disabled"));
	critical_enter();

	PCPU_SET(smp_tlb_addr1, addr1);
	PCPU_SET(smp_tlb_addr2, addr2);
	PCPU_SET(smp_tlb_pmap, pmap);
	generation = PCPU_GET(smp_tlb_gen);
	if (++generation == 0)
		generation = 1;
	PCPU_SET(smp_tlb_gen, generation);
	PCPU_SET(smp_tlb_op, op);
	/* Fence between filling smp_tlb fields and clearing scoreboard. */
	atomic_thread_fence_rel();

	CPU_FOREACH_ISSET(cpu, &mask) {
		KASSERT(*invl_scoreboard_slot(cpu) != 0,
		    ("IPI scoreboard is zero, initiator %d target %d",
		    curcpu, cpu));
		*invl_scoreboard_slot(cpu) = 0;
	}

	/*
	 * IPI acts as a fence between writing to the scoreboard above
	 * (zeroing slot) and reading from it below (wait for
	 * acknowledgment).
	 */
	if (is_all) {
		ipi_all_but_self(IPI_INVLOP);
	} else {
		ipi_selected(mask, IPI_INVLOP);
	}
	curcpu_cb(pmap, addr1, addr2);
	CPU_FOREACH_ISSET(cpu, &mask) {
		p_cpudone = invl_scoreboard_slot(cpu);
		while (atomic_load_int(p_cpudone) != generation)
			ia32_pause();
	}

	/*
	 * Unpin before leaving critical section.  If the thread owes
	 * preemption, this allows scheduler to select thread on any
	 * CPU from its cpuset.
	 */
	sched_unpin();
	critical_exit();

	return;

local_cb:
	critical_enter();
	curcpu_cb(pmap, addr1, addr2);
	sched_unpin();
	critical_exit();
}

void
smp_masked_invltlb(pmap_t pmap, smp_invl_cb_t curcpu_cb)
{
	smp_targeted_tlb_shootdown(pmap, 0, 0, curcpu_cb, invl_op_tlb);
#ifdef COUNT_XINVLTLB_HITS
	ipi_global++;
#endif
}

void
smp_masked_invlpg(vm_offset_t addr, pmap_t pmap, smp_invl_cb_t curcpu_cb)
{
	smp_targeted_tlb_shootdown(pmap, addr, 0, curcpu_cb, invl_op_pg);
#ifdef COUNT_XINVLTLB_HITS
	ipi_page++;
#endif
}

void
smp_masked_invlpg_range(vm_offset_t addr1, vm_offset_t addr2, pmap_t pmap,
    smp_invl_cb_t curcpu_cb)
{
	smp_targeted_tlb_shootdown(pmap, addr1, addr2, curcpu_cb,
	    invl_op_pgrng);
#ifdef COUNT_XINVLTLB_HITS
	ipi_range++;
	ipi_range_size += (addr2 - addr1) / PAGE_SIZE;
#endif
}

void
smp_cache_flush(smp_invl_cb_t curcpu_cb)
{
	smp_targeted_tlb_shootdown(kernel_pmap, 0, 0, curcpu_cb, INVL_OP_CACHE);
}

/*
 * Handlers for TLB related IPIs
 */
static void
invltlb_handler(pmap_t smp_tlb_pmap)
{
#ifdef COUNT_XINVLTLB_HITS
	xhits_gbl[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invltlb_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	if (smp_tlb_pmap == kernel_pmap)
		invltlb_glob();
	else
		invltlb();
}

static void
invltlb_invpcid_handler(pmap_t smp_tlb_pmap)
{
	struct invpcid_descr d;

#ifdef COUNT_XINVLTLB_HITS
	xhits_gbl[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invltlb_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	d.pcid = pmap_get_pcid(smp_tlb_pmap);
	d.pad = 0;
	d.addr = 0;
	invpcid(&d, smp_tlb_pmap == kernel_pmap ? INVPCID_CTXGLOB :
	    INVPCID_CTX);
}

static void
invltlb_invpcid_pti_handler(pmap_t smp_tlb_pmap)
{
	struct invpcid_descr d;

#ifdef COUNT_XINVLTLB_HITS
	xhits_gbl[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invltlb_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	d.pcid = pmap_get_pcid(smp_tlb_pmap);
	d.pad = 0;
	d.addr = 0;
	if (smp_tlb_pmap == kernel_pmap) {
		/*
		 * This invalidation actually needs to clear kernel
		 * mappings from the TLB in the current pmap, but
		 * since we were asked for the flush in the kernel
		 * pmap, achieve it by performing global flush.
		 */
		invpcid(&d, INVPCID_CTXGLOB);
	} else {
		invpcid(&d, INVPCID_CTX);
		if (smp_tlb_pmap == PCPU_GET(curpmap) &&
		    smp_tlb_pmap->pm_ucr3 != PMAP_NO_CR3)
			PCPU_SET(ucr3_load_mask, ~CR3_PCID_SAVE);
	}
}

static void
invltlb_pcid_handler(pmap_t smp_tlb_pmap)
{
#ifdef COUNT_XINVLTLB_HITS
	xhits_gbl[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invltlb_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	if (smp_tlb_pmap == kernel_pmap) {
		invltlb_glob();
	} else {
		/*
		 * The current pmap might not be equal to
		 * smp_tlb_pmap.  The clearing of the pm_gen in
		 * pmap_invalidate_all() takes care of TLB
		 * invalidation when switching to the pmap on this
		 * CPU.
		 */
		if (smp_tlb_pmap == PCPU_GET(curpmap)) {
			load_cr3(smp_tlb_pmap->pm_cr3 |
			    pmap_get_pcid(smp_tlb_pmap));
			if (smp_tlb_pmap->pm_ucr3 != PMAP_NO_CR3)
				PCPU_SET(ucr3_load_mask, ~CR3_PCID_SAVE);
		}
	}
}

static void
invlpg_handler(vm_offset_t smp_tlb_addr1)
{
#ifdef COUNT_XINVLTLB_HITS
	xhits_pg[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invlpg_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	invlpg(smp_tlb_addr1);
}

static void
invlpg_invpcid_handler(pmap_t smp_tlb_pmap, vm_offset_t smp_tlb_addr1)
{
	struct invpcid_descr d;

#ifdef COUNT_XINVLTLB_HITS
	xhits_pg[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invlpg_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	pmap_invlpg(smp_tlb_pmap, smp_tlb_addr1);
	if (smp_tlb_pmap == PCPU_GET(curpmap) &&
	    smp_tlb_pmap->pm_ucr3 != PMAP_NO_CR3 &&
	    PCPU_GET(ucr3_load_mask) == PMAP_UCR3_NOMASK) {
		d.pcid = pmap_get_pcid(smp_tlb_pmap) | PMAP_PCID_USER_PT;
		d.pad = 0;
		d.addr = smp_tlb_addr1;
		invpcid(&d, INVPCID_ADDR);
	}
}

static void
invlpg_pcid_handler(pmap_t smp_tlb_pmap, vm_offset_t smp_tlb_addr1)
{
	uint64_t kcr3, ucr3;
	uint32_t pcid;

#ifdef COUNT_XINVLTLB_HITS
	xhits_pg[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invlpg_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	invlpg(smp_tlb_addr1);
	if (smp_tlb_pmap == PCPU_GET(curpmap) &&
	    (ucr3 = smp_tlb_pmap->pm_ucr3) != PMAP_NO_CR3 &&
	    PCPU_GET(ucr3_load_mask) == PMAP_UCR3_NOMASK) {
		pcid = pmap_get_pcid(smp_tlb_pmap);
		kcr3 = smp_tlb_pmap->pm_cr3 | pcid | CR3_PCID_SAVE;
		ucr3 |= pcid | PMAP_PCID_USER_PT | CR3_PCID_SAVE;
		pmap_pti_pcid_invlpg(ucr3, kcr3, smp_tlb_addr1);
	}
}

static void
invlrng_handler(vm_offset_t smp_tlb_addr1, vm_offset_t smp_tlb_addr2)
{
	vm_offset_t addr;

#ifdef COUNT_XINVLTLB_HITS
	xhits_rng[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invlrng_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	addr = smp_tlb_addr1;
	do {
		invlpg(addr);
		addr += PAGE_SIZE;
	} while (addr < smp_tlb_addr2);
}

static void
invlrng_invpcid_handler(pmap_t smp_tlb_pmap, vm_offset_t smp_tlb_addr1,
    vm_offset_t smp_tlb_addr2)
{
	struct invpcid_descr d;
	vm_offset_t addr;

#ifdef COUNT_XINVLTLB_HITS
	xhits_rng[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invlrng_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	addr = smp_tlb_addr1;
	if (smp_tlb_pmap == kernel_pmap && PCPU_GET(pcid_invlpg_workaround)) {
		struct invpcid_descr d = { 0 };

		invpcid(&d, INVPCID_CTXGLOB);
	} else {
		do {
			invlpg(addr);
			addr += PAGE_SIZE;
		} while (addr < smp_tlb_addr2);
	}
	if (smp_tlb_pmap == PCPU_GET(curpmap) &&
	    smp_tlb_pmap->pm_ucr3 != PMAP_NO_CR3 &&
	    PCPU_GET(ucr3_load_mask) == PMAP_UCR3_NOMASK) {
		d.pcid = pmap_get_pcid(smp_tlb_pmap) | PMAP_PCID_USER_PT;
		d.pad = 0;
		d.addr = smp_tlb_addr1;
		do {
			invpcid(&d, INVPCID_ADDR);
			d.addr += PAGE_SIZE;
		} while (d.addr < smp_tlb_addr2);
	}
}

static void
invlrng_pcid_handler(pmap_t smp_tlb_pmap, vm_offset_t smp_tlb_addr1,
    vm_offset_t smp_tlb_addr2)
{
	vm_offset_t addr;
	uint64_t kcr3, ucr3;
	uint32_t pcid;

#ifdef COUNT_XINVLTLB_HITS
	xhits_rng[PCPU_GET(cpuid)]++;
#endif /* COUNT_XINVLTLB_HITS */
#ifdef COUNT_IPIS
	(*ipi_invlrng_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */

	addr = smp_tlb_addr1;
	do {
		invlpg(addr);
		addr += PAGE_SIZE;
	} while (addr < smp_tlb_addr2);
	if (smp_tlb_pmap == PCPU_GET(curpmap) &&
	    (ucr3 = smp_tlb_pmap->pm_ucr3) != PMAP_NO_CR3 &&
	    PCPU_GET(ucr3_load_mask) == PMAP_UCR3_NOMASK) {
		pcid = pmap_get_pcid(smp_tlb_pmap);
		kcr3 = smp_tlb_pmap->pm_cr3 | pcid | CR3_PCID_SAVE;
		ucr3 |= pcid | PMAP_PCID_USER_PT | CR3_PCID_SAVE;
		pmap_pti_pcid_invlrng(ucr3, kcr3, smp_tlb_addr1, smp_tlb_addr2);
	}
}

static void
invlcache_handler(void)
{
#ifdef COUNT_IPIS
	(*ipi_invlcache_counts[PCPU_GET(cpuid)])++;
#endif /* COUNT_IPIS */
	wbinvd();
}

static void
invlop_handler_one_req(enum invl_op_codes smp_tlb_op, pmap_t smp_tlb_pmap,
    vm_offset_t smp_tlb_addr1, vm_offset_t smp_tlb_addr2)
{
	switch (smp_tlb_op) {
	case INVL_OP_TLB:
		invltlb_handler(smp_tlb_pmap);
		break;
	case INVL_OP_TLB_INVPCID:
		invltlb_invpcid_handler(smp_tlb_pmap);
		break;
	case INVL_OP_TLB_INVPCID_PTI:
		invltlb_invpcid_pti_handler(smp_tlb_pmap);
		break;
	case INVL_OP_TLB_PCID:
		invltlb_pcid_handler(smp_tlb_pmap);
		break;
	case INVL_OP_PGRNG:
		invlrng_handler(smp_tlb_addr1, smp_tlb_addr2);
		break;
	case INVL_OP_PGRNG_INVPCID:
		invlrng_invpcid_handler(smp_tlb_pmap, smp_tlb_addr1,
		    smp_tlb_addr2);
		break;
	case INVL_OP_PGRNG_PCID:
		invlrng_pcid_handler(smp_tlb_pmap, smp_tlb_addr1,
		    smp_tlb_addr2);
		break;
	case INVL_OP_PG:
		invlpg_handler(smp_tlb_addr1);
		break;
	case INVL_OP_PG_INVPCID:
		invlpg_invpcid_handler(smp_tlb_pmap, smp_tlb_addr1);
		break;
	case INVL_OP_PG_PCID:
		invlpg_pcid_handler(smp_tlb_pmap, smp_tlb_addr1);
		break;
	case INVL_OP_CACHE:
		invlcache_handler();
		break;
	default:
		__assert_unreachable();
		break;
	}
}

void
invlop_handler(void)
{
	struct pcpu *initiator_pc;
	pmap_t smp_tlb_pmap;
	vm_offset_t smp_tlb_addr1, smp_tlb_addr2;
	u_int initiator_cpu_id;
	enum invl_op_codes smp_tlb_op;
	uint32_t *scoreboard, smp_tlb_gen;

	scoreboard = invl_scoreboard_getcpu(PCPU_GET(cpuid));
	for (;;) {
		for (initiator_cpu_id = 0; initiator_cpu_id <= mp_maxid;
		    initiator_cpu_id++) {
			if (atomic_load_int(&scoreboard[initiator_cpu_id]) == 0)
				break;
		}
		if (initiator_cpu_id > mp_maxid)
			break;
		initiator_pc = cpuid_to_pcpu[initiator_cpu_id];

		/*
		 * This acquire fence and its corresponding release
		 * fence in smp_targeted_tlb_shootdown() is between
		 * reading zero scoreboard slot and accessing PCPU of
		 * initiator for pc_smp_tlb values.
		 */
		atomic_thread_fence_acq();
		smp_tlb_pmap = initiator_pc->pc_smp_tlb_pmap;
		smp_tlb_addr1 = initiator_pc->pc_smp_tlb_addr1;
		smp_tlb_addr2 = initiator_pc->pc_smp_tlb_addr2;
		smp_tlb_op = initiator_pc->pc_smp_tlb_op;
		smp_tlb_gen = initiator_pc->pc_smp_tlb_gen;

		/*
		 * Ensure that we do not make our scoreboard
		 * notification visible to the initiator until the
		 * pc_smp_tlb values are read.  The corresponding
		 * fence is implicitly provided by the barrier in the
		 * IPI send operation before the APIC ICR register
		 * write.
		 *
		 * As an optimization, the request is acknowledged
		 * before the actual invalidation is performed.  It is
		 * safe because target CPU cannot return to userspace
		 * before handler finishes. Only NMI can preempt the
		 * handler, but NMI would see the kernel handler frame
		 * and not touch not-invalidated user page table.
		 */
		atomic_thread_fence_acq();
		atomic_store_int(&scoreboard[initiator_cpu_id], smp_tlb_gen);

		invlop_handler_one_req(smp_tlb_op, smp_tlb_pmap, smp_tlb_addr1,
		    smp_tlb_addr2);
	}
}
