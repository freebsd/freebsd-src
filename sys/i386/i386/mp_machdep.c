/*-
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
__FBSDID("$FreeBSD$");

#include "opt_apic.h"
#include "opt_cpu.h"
#include "opt_kstack_pages.h"
#include "opt_mp_watchdog.h"

#if !defined(lint)
#if !defined(SMP)
#error How did you get here?
#endif

#ifndef DEV_APIC
#error The apic device is required for SMP, add "device apic" to your config file.
#endif
#if defined(CPU_DISABLE_CMPXCHG) && !defined(COMPILING_LINT)
#error SMP not supported with CPU_DISABLE_CMPXCHG
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cons.h>	/* cngetc() */
#ifdef GPROF 
#include <sys/gmon.h>
#endif
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

#include <machine/apicreg.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/mp_watchdog.h>
#include <machine/pcb.h>
#include <machine/smp.h>
#include <machine/smptests.h>	/** COUNT_XINVLTLB_HITS */
#include <machine/specialreg.h>
#include <machine/privatespace.h>

#define WARMBOOT_TARGET		0
#define WARMBOOT_OFF		(KERNBASE + 0x0467)
#define WARMBOOT_SEG		(KERNBASE + 0x0469)

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

#if defined(CHECK_POINTS) && !defined(PC98)
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
 * Values to send to the POST hardware.
 */
#define MP_BOOTADDRESS_POST	0x10
#define MP_PROBE_POST		0x11
#define MPTABLE_PASS1_POST	0x12

#define MP_START_POST		0x13
#define MP_ENABLE_POST		0x14
#define MPTABLE_PASS2_POST	0x15

#define START_ALL_APS_POST	0x16
#define INSTALL_AP_TRAMP_POST	0x17
#define START_AP_POST		0x18

#define MP_ANNOUNCE_POST	0x19

/* lock region used by kernel profiling */
int	mcount_lock;

/** XXX FIXME: where does this really belong, isa.h/isa.c perhaps? */
int	current_postcode;

int	mp_naps;		/* # of Applications processors */
int	boot_cpu_id = -1;	/* designated BSP */
extern	int nkpt;

/*
 * CPU topology map datastructures for HTT.
 */
static struct cpu_group mp_groups[MAXCPU];
static struct cpu_top mp_top;

/* AP uses this during bootstrap.  Do not staticize.  */
char *bootSTK;
static int bootAP;

/* Hotwire a 0->4MB V==P mapping */
extern pt_entry_t *KPTphys;

/* SMP page table page */
extern pt_entry_t *SMPpt;

struct pcb stoppcbs[MAXCPU];

/* Variables needed for SMP tlb shootdown. */
vm_offset_t smp_tlb_addr1;
vm_offset_t smp_tlb_addr2;
volatile int smp_tlb_wait;

/*
 * Local data and functions.
 */

static u_int logical_cpus;

/* used to hold the AP's until we are ready to release them */
static struct mtx ap_boot_mtx;

/* Set to 1 once we're ready to let the APs out of the pen. */
static volatile int aps_ready = 0;

/*
 * Store data from cpu_add() until later in the boot when we actually setup
 * the APs.
 */
struct cpu_info {
	int	cpu_present:1;
	int	cpu_bsp:1;
	int	cpu_disabled:1;
} static cpu_info[MAXCPU];
static int cpu_apic_ids[MAXCPU];

/* Holds pending bitmap based IPIs per CPU */
static volatile u_int cpu_ipi_pending[MAXCPU];

static u_int boot_address;

static void	set_logical_apic_ids(void);
static int	start_all_aps(void);
static void	install_ap_tramp(void);
static int	start_ap(int apic_id);
static void	release_aps(void *dummy);

static int	hlt_logical_cpus;
static struct	sysctl_ctx_list logical_cpu_clist;

static void
mem_range_AP_init(void)
{
	if (mem_range_softc.mr_op && mem_range_softc.mr_op->initAP)
		mem_range_softc.mr_op->initAP(&mem_range_softc);
}

void
mp_topology(void)
{
	struct cpu_group *group;
	int logical_cpus;
	int apic_id;
	int groups;
	int cpu;

	/* Build the smp_topology map. */
	/* Nothing to do if there is no HTT support. */
	if ((cpu_feature & CPUID_HTT) == 0)
		return;
	logical_cpus = (cpu_procinfo & CPUID_HTT_CORES) >> 16;
	if (logical_cpus <= 1)
		return;
	group = &mp_groups[0];
	groups = 1;
	for (cpu = 0, apic_id = 0; apic_id < MAXCPU; apic_id++) {
		if (!cpu_info[apic_id].cpu_present)
			continue;
		/*
		 * If the current group has members and we're not a logical
		 * cpu, create a new group.
		 */
		if (group->cg_count != 0 && (apic_id % logical_cpus) == 0) {
			group++;
			groups++;
		}
		group->cg_count++;
		group->cg_mask |= 1 << cpu;
		cpu++;
	}

	mp_top.ct_count = groups;
	mp_top.ct_group = mp_groups;
	smp_topology = &mp_top;
}


/*
 * Calculate usable address in base memory for AP trampoline code.
 */
u_int
mp_bootaddress(u_int basemem)
{
	POSTCODE(MP_BOOTADDRESS_POST);

	boot_address = trunc_page(basemem);	/* round down to 4k boundary */
	if ((basemem - boot_address) < bootMP_size)
		boot_address -= PAGE_SIZE;	/* not enough, lower by 4k */

	return boot_address;
}

void
cpu_add(u_int apic_id, char boot_cpu)
{

	if (apic_id >= MAXCPU) {
		printf("SMP: CPU %d exceeds maximum CPU %d, ignoring\n",
		    apic_id, MAXCPU - 1);
		return;
	}
	KASSERT(cpu_info[apic_id].cpu_present == 0, ("CPU %d added twice",
	    apic_id));
	cpu_info[apic_id].cpu_present = 1;
	if (boot_cpu) {
		KASSERT(boot_cpu_id == -1,
		    ("CPU %d claims to be BSP, but CPU %d already is", apic_id,
		    boot_cpu_id));
		boot_cpu_id = apic_id;
		cpu_info[apic_id].cpu_bsp = 1;
	}
	mp_ncpus++;
	if (bootverbose)
		printf("SMP: Added CPU %d (%s)\n", apic_id, boot_cpu ? "BSP" :
		    "AP");
	
}

void
cpu_mp_setmaxid(void)
{

	mp_maxid = MAXCPU - 1;
}

int
cpu_mp_probe(void)
{

	/*
	 * Always record BSP in CPU map so that the mbuf init code works
	 * correctly.
	 */
	all_cpus = 1;
	if (mp_ncpus == 0) {
		/*
		 * No CPUs were found, so this must be a UP system.  Setup
		 * the variables to represent a system with a single CPU
		 * with an id of 0.
		 */
		mp_ncpus = 1;
		return (0);
	}

	/* At least one CPU was found. */
	if (mp_ncpus == 1) {
		/*
		 * One CPU was found, so this must be a UP system with
		 * an I/O APIC.
		 */
		return (0);
	}

	/* At least two CPUs were found. */
	return (1);
}

/*
 * Initialize the IPI handlers and start up the AP's.
 */
void
cpu_mp_start(void)
{
	int i;

	POSTCODE(MP_START_POST);

	/* Initialize the logical ID to APIC ID table. */
	for (i = 0; i < MAXCPU; i++) {
		cpu_apic_ids[i] = -1;
		cpu_ipi_pending[i] = 0;
	}

	/* Install an inter-CPU IPI for TLB invalidation */
	setidt(IPI_INVLTLB, IDTVEC(invltlb),
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(IPI_INVLPG, IDTVEC(invlpg),
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	setidt(IPI_INVLRNG, IDTVEC(invlrng),
	       SDT_SYS386IGT, SEL_KPL, GSEL(GCODE_SEL, SEL_KPL));
	
	/* Install an inter-CPU IPI for lazy pmap release */
	setidt(IPI_LAZYPMAP, IDTVEC(lazypmap),
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


	/* Set boot_cpu_id if needed. */
	if (boot_cpu_id == -1) {
		boot_cpu_id = PCPU_GET(apic_id);
		cpu_info[boot_cpu_id].cpu_bsp = 1;
	} else
		KASSERT(boot_cpu_id == PCPU_GET(apic_id),
		    ("BSP's APIC ID doesn't match boot_cpu_id"));
	cpu_apic_ids[0] = boot_cpu_id;

	/* Start each Application Processor */
	start_all_aps();

	/* Setup the initial logical CPUs info. */
	logical_cpus = logical_cpus_mask = 0;
	if (cpu_feature & CPUID_HTT)
		logical_cpus = (cpu_procinfo & CPUID_HTT_CORES) >> 16;

	set_logical_apic_ids();
}


/*
 * Print various information about the SMP system hardware and setup.
 */
void
cpu_mp_announce(void)
{
	int i, x;

	POSTCODE(MP_ANNOUNCE_POST);

	/* List CPUs */
	printf(" cpu0 (BSP): APIC ID: %2d\n", boot_cpu_id);
	for (i = 1, x = 0; x < MAXCPU; x++) {
		if (!cpu_info[x].cpu_present || cpu_info[x].cpu_bsp)
			continue;
		if (cpu_info[x].cpu_disabled)
			printf("  cpu (AP): APIC ID: %2d (disabled)\n", x);
		else {
			KASSERT(i < mp_ncpus,
			    ("mp_ncpus and actual cpus are out of whack"));
			printf(" cpu%d (AP): APIC ID: %2d\n", i++, x);
		}
	}
}

/*
 * AP CPU's call this to initialize themselves.
 */
void
init_secondary(void)
{
	vm_offset_t addr;
	int	gsel_tss;
	int	x, myid;
	u_int	cr0;

	/* bootAP is set in start_ap() to our ID. */
	myid = bootAP;
	gdt_segs[GPRIV_SEL].ssd_base = (int) &SMP_prvspace[myid];
	gdt_segs[GPROC0_SEL].ssd_base =
		(int) &SMP_prvspace[myid].pcpu.pc_common_tss;
	SMP_prvspace[myid].pcpu.pc_prvspace =
		&SMP_prvspace[myid].pcpu;

	for (x = 0; x < NGDT; x++) {
		ssdtosd(&gdt_segs[x], &gdt[myid * NGDT + x].sd);
	}

	r_gdt.rd_limit = NGDT * sizeof(gdt[0]) - 1;
	r_gdt.rd_base = (int) &gdt[myid * NGDT];
	lgdt(&r_gdt);			/* does magic intra-segment return */

	lidt(&r_idt);

	lldt(_default_ldt);
	PCPU_SET(currentldt, _default_ldt);

	gsel_tss = GSEL(GPROC0_SEL, SEL_KPL);
	gdt[myid * NGDT + GPROC0_SEL].sd.sd_type = SDT_SYS386TSS;
	PCPU_SET(common_tss.tss_esp0, 0); /* not used until after switch */
	PCPU_SET(common_tss.tss_ss0, GSEL(GDATA_SEL, SEL_KPL));
	PCPU_SET(common_tss.tss_ioopt, (sizeof (struct i386tss)) << 16);
	PCPU_SET(tss_gdt, &gdt[myid * NGDT + GPROC0_SEL].sd);
	PCPU_SET(common_tssd, *PCPU_GET(tss_gdt));
	ltr(gsel_tss);

	PCPU_SET(fsgs_gdt, &gdt[myid * NGDT + GUFS_SEL].sd);

	/*
	 * Set to a known state:
	 * Set by mpboot.s: CR0_PG, CR0_PE
	 * Set by cpu_setregs: CR0_NE, CR0_MP, CR0_TS, CR0_WP, CR0_AM
	 */
	cr0 = rcr0();
	cr0 &= ~(CR0_CD | CR0_NW | CR0_EM);
	load_cr0(cr0);
	CHECK_WRITE(0x38, 5);
	
	/* Disable local APIC just to be sure. */
	lapic_disable();

	/* signal our startup to the BSP. */
	mp_naps++;
	CHECK_WRITE(0x39, 6);

	/* Spin until the BSP releases the AP's. */
	while (!aps_ready)
		ia32_pause();

	/* BSP may have changed PTD while we were waiting */
	invltlb();
	for (addr = 0; addr < NKPT * NBPDR - 1; addr += PAGE_SIZE)
		invlpg(addr);

#if defined(I586_CPU) && !defined(NO_F00F_HACK)
	lidt(&r_idt);
#endif

	/* set up CPU registers and state */
	cpu_setregs();

	/* set up FPU state on the AP */
	npxinit(__INITIAL_NPXCW__);

	/* set up SSE registers */
	enable_sse();

	/* A quick check from sanity claus */
	if (PCPU_GET(apic_id) != lapic_id()) {
		printf("SMP: cpuid = %d\n", PCPU_GET(cpuid));
		printf("SMP: actual apic_id = %d\n", lapic_id());
		printf("SMP: correct apic_id = %d\n", PCPU_GET(apic_id));
		printf("PTD[MPPTDI] = %#jx\n", (uintmax_t)PTD[MPPTDI]);
		panic("cpuid mismatch! boom!!");
	}

	/* Initialize curthread. */
	KASSERT(PCPU_GET(idlethread) != NULL, ("no idle thread"));
	PCPU_SET(curthread, PCPU_GET(idlethread));

	mtx_lock_spin(&ap_boot_mtx);

	/* Init local apic for irq's */
	lapic_setup();

	/* Set memory range attributes for this CPU to match the BSP */
	mem_range_AP_init();

	smp_cpus++;

	CTR1(KTR_SMP, "SMP: AP CPU #%d Launched", PCPU_GET(cpuid));
	printf("SMP: AP CPU #%d Launched!\n", PCPU_GET(cpuid));

	/* Determine if we are a logical CPU. */
	if (logical_cpus > 1 && PCPU_GET(apic_id) % logical_cpus != 0)
		logical_cpus_mask |= PCPU_GET(cpumask);
	
	/* Build our map of 'other' CPUs. */
	PCPU_SET(other_cpus, all_cpus & ~PCPU_GET(cpumask));

	if (bootverbose)
		lapic_dump("AP");

	if (smp_cpus == mp_ncpus) {
		/* enable IPI's, tlb shootdown, freezes etc */
		atomic_store_rel_int(&smp_started, 1);
		smp_active = 1;	 /* historic */
	}

	mtx_unlock_spin(&ap_boot_mtx);

	/* wait until all the AP's are up */
	while (smp_started == 0)
		ia32_pause();

	/* ok, now grab sched_lock and enter the scheduler */
	mtx_lock_spin(&sched_lock);

	/*
	 * Correct spinlock nesting.  The idle thread context that we are
	 * borrowing was created so that it would start out with a single
	 * spin lock (sched_lock) held in fork_trampoline().  Since we've
	 * explicitly acquired locks in this function, the nesting count
	 * is now 2 rather than 1.  Since we are nested, calling
	 * spinlock_exit() will simply adjust the counts without allowing
	 * spin lock using code to interrupt us.
	 */
	spinlock_exit();
	KASSERT(curthread->td_md.md_spinlock_count == 1, ("invalid count"));

	binuptime(PCPU_PTR(switchtime));
	PCPU_SET(switchticks, ticks);

	cpu_throw(NULL, choosethread());	/* doesn't return */

	panic("scheduler returned us to %s", __func__);
	/* NOTREACHED */
}

/*******************************************************************
 * local functions and data
 */

/*
 * Set the APIC logical IDs.
 *
 * We want to cluster logical CPU's within the same APIC ID cluster.
 * Since logical CPU's are aligned simply filling in the clusters in
 * APIC ID order works fine.  Note that this does not try to balance
 * the number of CPU's in each cluster. (XXX?)
 */
static void
set_logical_apic_ids(void)
{
	u_int apic_id, cluster, cluster_id;

	/* Force us to allocate cluster 0 at the start. */
	cluster = -1;
	cluster_id = APIC_MAX_INTRACLUSTER_ID;
	for (apic_id = 0; apic_id < MAXCPU; apic_id++) {
		if (!cpu_info[apic_id].cpu_present)
			continue;
		if (cluster_id == APIC_MAX_INTRACLUSTER_ID) {
			cluster = ioapic_next_logical_cluster();
			cluster_id = 0;
		} else
			cluster_id++;
		if (bootverbose)
			printf("APIC ID: physical %u, logical %u:%u\n",
			    apic_id, cluster, cluster_id);
		lapic_set_logical_id(apic_id, cluster, cluster_id);
	}
}

/*
 * start each AP in our list
 */
static int
start_all_aps(void)
{
#ifndef PC98
	u_char mpbiosreason;
#endif
	u_long mpbioswarmvec;
	struct pcpu *pc;
	char *stack;
	uintptr_t kptbase;
	int i, pg, apic_id, cpu;

	POSTCODE(START_ALL_APS_POST);

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	/* install the AP 1st level boot code */
	install_ap_tramp();

	/* save the current value of the warm-start vector */
	mpbioswarmvec = *((u_long *) WARMBOOT_OFF);
#ifndef PC98
	outb(CMOS_REG, BIOS_RESET);
	mpbiosreason = inb(CMOS_DATA);
#endif

	/* set up temporary P==V mapping for AP boot */
	/* XXX this is a hack, we should boot the AP on its own stack/PTD */
	kptbase = (uintptr_t)(void *)KPTphys;
	for (i = 0; i < NKPT; i++)
		PTD[i] = (pd_entry_t)(PG_V | PG_RW |
		    ((kptbase + i * PAGE_SIZE) & PG_FRAME));
	invltlb();

	/* start each AP */
	for (cpu = 0, apic_id = 0; apic_id < MAXCPU; apic_id++) {

		/* Ignore non-existent CPUs and the BSP. */
		if (!cpu_info[apic_id].cpu_present ||
		    cpu_info[apic_id].cpu_bsp)
			continue;

		/* Don't use this CPU if it has been disabled by a tunable. */
		if (resource_disabled("lapic", apic_id)) {
			cpu_info[apic_id].cpu_disabled = 1;
			mp_ncpus--;
			continue;
		}

		cpu++;

		/* save APIC ID for this logical ID */
		cpu_apic_ids[cpu] = apic_id;

		/* first page of AP's private space */
		pg = cpu * i386_btop(sizeof(struct privatespace));

		/* allocate a new private data page */
		pc = (struct pcpu *)kmem_alloc(kernel_map, PAGE_SIZE);

		/* wire it into the private page table page */
		SMPpt[pg] = (pt_entry_t)(PG_V | PG_RW | vtophys(pc));

		/* allocate and set up an idle stack data page */
		stack = (char *)kmem_alloc(kernel_map, KSTACK_PAGES * PAGE_SIZE); /* XXXKSE */
		for (i = 0; i < KSTACK_PAGES; i++)
			SMPpt[pg + 1 + i] = (pt_entry_t)
			    (PG_V | PG_RW | vtophys(PAGE_SIZE * i + stack));

		/* prime data page for it to use */
		pcpu_init(pc, cpu, sizeof(struct pcpu));
		pc->pc_apic_id = apic_id;

		/* setup a vector to our boot code */
		*((volatile u_short *) WARMBOOT_OFF) = WARMBOOT_TARGET;
		*((volatile u_short *) WARMBOOT_SEG) = (boot_address >> 4);
#ifndef PC98
		outb(CMOS_REG, BIOS_RESET);
		outb(CMOS_DATA, BIOS_WARM);	/* 'warm-start' */
#endif

		bootSTK = &SMP_prvspace[cpu].idlekstack[KSTACK_PAGES *
		    PAGE_SIZE];
		bootAP = cpu;

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

		all_cpus |= (1 << cpu);		/* record AP in CPU map */
	}

	/* build our map of 'other' CPUs */
	PCPU_SET(other_cpus, all_cpus & ~PCPU_GET(cpumask));

	/* restore the warmstart vector */
	*(u_long *) WARMBOOT_OFF = mpbioswarmvec;
#ifndef PC98
	outb(CMOS_REG, BIOS_RESET);
	outb(CMOS_DATA, mpbiosreason);
#endif

	/*
	 * Set up the idle context for the BSP.  Similar to above except
	 * that some was done by locore, some by pmap.c and some is implicit
	 * because the BSP is cpu#0 and the page is initially zero and also
	 * because we can refer to variables by name on the BSP..
	 */

	/* Allocate and setup BSP idle stack */
	stack = (char *)kmem_alloc(kernel_map, KSTACK_PAGES * PAGE_SIZE);
	for (i = 0; i < KSTACK_PAGES; i++)
		SMPpt[1 + i] = (pt_entry_t)
		    (PG_V | PG_RW | vtophys(PAGE_SIZE * i + stack));

	for (i = 0; i < NKPT; i++)
		PTD[i] = 0;
	pmap_invalidate_range(kernel_pmap, 0, NKPT * NBPDR - 1);

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
	vm_offset_t va = boot_address + KERNBASE;
	u_char *src = (u_char *) ((u_long) bootMP);
	u_char *dst = (u_char *) va;
	u_int   boot_base = (u_int) bootMP;
	u_int8_t *dst8;
	u_int16_t *dst16;
	u_int32_t *dst32;

	POSTCODE(INSTALL_AP_TRAMP_POST);

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
	*dst32 = ((u_int) MPentry - KERNBASE);

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

	POSTCODE(START_AP_POST);

	/* calculate the vector */
	vector = (boot_address >> 12) & 0xff;

	/* used as a watchpoint to signal AP startup */
	cpus = mp_naps;

	/*
	 * first we do an INIT/RESET IPI this INIT IPI might be run, reseting
	 * and running the target CPU. OR this INIT IPI might be latched (P5
	 * bug), CPU waiting for STARTUP IPI. OR this INIT IPI might be
	 * ignored.
	 */

	/* do an INIT IPI: assert RESET */
	lapic_ipi_raw(APIC_DEST_DESTFLD | APIC_TRIGMOD_EDGE |
	    APIC_LEVEL_ASSERT | APIC_DESTMODE_PHY | APIC_DELMODE_INIT, apic_id);

	/* wait for pending status end */
	lapic_ipi_wait(-1);

	/* do an INIT IPI: deassert RESET */
	lapic_ipi_raw(APIC_DEST_ALLESELF | APIC_TRIGMOD_LEVEL |
	    APIC_LEVEL_DEASSERT | APIC_DESTMODE_PHY | APIC_DELMODE_INIT, 0);

	/* wait for pending status end */
	DELAY(10000);		/* wait ~10mS */
	lapic_ipi_wait(-1);

	/*
	 * next we do a STARTUP IPI: the previous INIT IPI might still be
	 * latched, (P5 bug) this 1st STARTUP would then terminate
	 * immediately, and the previously started INIT IPI would continue. OR
	 * the previous INIT IPI has already run. and this STARTUP IPI will
	 * run. OR the previous INIT IPI was ignored. and this STARTUP IPI
	 * will run.
	 */

	/* do a STARTUP IPI */
	lapic_ipi_raw(APIC_DEST_DESTFLD | APIC_TRIGMOD_EDGE |
	    APIC_LEVEL_DEASSERT | APIC_DESTMODE_PHY | APIC_DELMODE_STARTUP |
	    vector, apic_id);
	lapic_ipi_wait(-1);
	DELAY(200);		/* wait ~200uS */

	/*
	 * finally we do a 2nd STARTUP IPI: this 2nd STARTUP IPI should run IF
	 * the previous STARTUP IPI was cancelled by a latched INIT IPI. OR
	 * this STARTUP IPI will be ignored, as only ONE STARTUP IPI is
	 * recognized after hardware RESET or INIT IPI.
	 */

	lapic_ipi_raw(APIC_DEST_DESTFLD | APIC_TRIGMOD_EDGE |
	    APIC_LEVEL_DEASSERT | APIC_DESTMODE_PHY | APIC_DELMODE_STARTUP |
	    vector, apic_id);
	lapic_ipi_wait(-1);
	DELAY(200);		/* wait ~200uS */

	/* Wait up to 5 seconds for it to start. */
	for (ms = 0; ms < 5000; ms++) {
		if (mp_naps > cpus)
			return 1;	/* return SUCCESS */
		DELAY(1000);
	}
	return 0;		/* return FAILURE */
}

#ifdef COUNT_XINVLTLB_HITS
u_int xhits_gbl[MAXCPU];
u_int xhits_pg[MAXCPU];
u_int xhits_rng[MAXCPU];
SYSCTL_NODE(_debug, OID_AUTO, xhits, CTLFLAG_RW, 0, "");
SYSCTL_OPAQUE(_debug_xhits, OID_AUTO, global, CTLFLAG_RW, &xhits_gbl,
    sizeof(xhits_gbl), "IU", "");
SYSCTL_OPAQUE(_debug_xhits, OID_AUTO, page, CTLFLAG_RW, &xhits_pg,
    sizeof(xhits_pg), "IU", "");
SYSCTL_OPAQUE(_debug_xhits, OID_AUTO, range, CTLFLAG_RW, &xhits_rng,
    sizeof(xhits_rng), "IU", "");

u_int ipi_global;
u_int ipi_page;
u_int ipi_range;
u_int ipi_range_size;
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_global, CTLFLAG_RW, &ipi_global, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_page, CTLFLAG_RW, &ipi_page, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_range, CTLFLAG_RW, &ipi_range, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_range_size, CTLFLAG_RW, &ipi_range_size,
    0, "");

u_int ipi_masked_global;
u_int ipi_masked_page;
u_int ipi_masked_range;
u_int ipi_masked_range_size;
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_masked_global, CTLFLAG_RW,
    &ipi_masked_global, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_masked_page, CTLFLAG_RW,
    &ipi_masked_page, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_masked_range, CTLFLAG_RW,
    &ipi_masked_range, 0, "");
SYSCTL_INT(_debug_xhits, OID_AUTO, ipi_masked_range_size, CTLFLAG_RW,
    &ipi_masked_range_size, 0, "");
#endif /* COUNT_XINVLTLB_HITS */

/*
 * Flush the TLB on all other CPU's
 */
static void
smp_tlb_shootdown(u_int vector, vm_offset_t addr1, vm_offset_t addr2)
{
	u_int ncpu;

	ncpu = mp_ncpus - 1;	/* does not shootdown self */
	if (ncpu < 1)
		return;		/* no other cpus */
	mtx_assert(&smp_ipi_mtx, MA_OWNED);
	smp_tlb_addr1 = addr1;
	smp_tlb_addr2 = addr2;
	atomic_store_rel_int(&smp_tlb_wait, 0);
	ipi_all_but_self(vector);
	while (smp_tlb_wait < ncpu)
		ia32_pause();
}

/*
 * This is about as magic as it gets.  fortune(1) has got similar code
 * for reversing bits in a word.  Who thinks up this stuff??
 *
 * Yes, it does appear to be consistently faster than:
 * while (i = ffs(m)) {
 *	m >>= i;
 *	bits++;
 * }
 * and
 * while (lsb = (m & -m)) {	// This is magic too
 * 	m &= ~lsb;		// or: m ^= lsb
 *	bits++;
 * }
 * Both of these latter forms do some very strange things on gcc-3.1 with
 * -mcpu=pentiumpro and/or -march=pentiumpro and/or -O or -O2.
 * There is probably an SSE or MMX popcnt instruction.
 *
 * I wonder if this should be in libkern?
 *
 * XXX Stop the presses!  Another one:
 * static __inline u_int32_t
 * popcnt1(u_int32_t v)
 * {
 *	v -= ((v >> 1) & 0x55555555);
 *	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
 *	v = (v + (v >> 4)) & 0x0F0F0F0F;
 *	return (v * 0x01010101) >> 24;
 * }
 * The downside is that it has a multiply.  With a pentium3 with
 * -mcpu=pentiumpro and -march=pentiumpro then gcc-3.1 will use
 * an imull, and in that case it is faster.  In most other cases
 * it appears slightly slower.
 *
 * Another variant (also from fortune):
 * #define BITCOUNT(x) (((BX_(x)+(BX_(x)>>4)) & 0x0F0F0F0F) % 255)
 * #define  BX_(x)     ((x) - (((x)>>1)&0x77777777)            \
 *                          - (((x)>>2)&0x33333333)            \
 *                          - (((x)>>3)&0x11111111))
 */
static __inline u_int32_t
popcnt(u_int32_t m)
{

	m = (m & 0x55555555) + ((m & 0xaaaaaaaa) >> 1);
	m = (m & 0x33333333) + ((m & 0xcccccccc) >> 2);
	m = (m & 0x0f0f0f0f) + ((m & 0xf0f0f0f0) >> 4);
	m = (m & 0x00ff00ff) + ((m & 0xff00ff00) >> 8);
	m = (m & 0x0000ffff) + ((m & 0xffff0000) >> 16);
	return m;
}

static void
smp_targeted_tlb_shootdown(u_int mask, u_int vector, vm_offset_t addr1, vm_offset_t addr2)
{
	int ncpu, othercpus;

	othercpus = mp_ncpus - 1;
	if (mask == (u_int)-1) {
		ncpu = othercpus;
		if (ncpu < 1)
			return;
	} else {
		mask &= ~PCPU_GET(cpumask);
		if (mask == 0)
			return;
		ncpu = popcnt(mask);
		if (ncpu > othercpus) {
			/* XXX this should be a panic offence */
			printf("SMP: tlb shootdown to %d other cpus (only have %d)\n",
			    ncpu, othercpus);
			ncpu = othercpus;
		}
		/* XXX should be a panic, implied by mask == 0 above */
		if (ncpu < 1)
			return;
	}
	mtx_assert(&smp_ipi_mtx, MA_OWNED);
	smp_tlb_addr1 = addr1;
	smp_tlb_addr2 = addr2;
	atomic_store_rel_int(&smp_tlb_wait, 0);
	if (mask == (u_int)-1)
		ipi_all_but_self(vector);
	else
		ipi_selected(mask, vector);
	while (smp_tlb_wait < ncpu)
		ia32_pause();
}

void
smp_invltlb(void)
{
	if (smp_started) {
		smp_tlb_shootdown(IPI_INVLTLB, 0, 0);
#ifdef COUNT_XINVLTLB_HITS
		ipi_global++;
#endif
	}
}

void
smp_invlpg(vm_offset_t addr)
{
	if (smp_started) {
		smp_tlb_shootdown(IPI_INVLPG, addr, 0);
#ifdef COUNT_XINVLTLB_HITS
		ipi_page++;
#endif
	}
}

void
smp_invlpg_range(vm_offset_t addr1, vm_offset_t addr2)
{
	if (smp_started) {
		smp_tlb_shootdown(IPI_INVLRNG, addr1, addr2);
#ifdef COUNT_XINVLTLB_HITS
		ipi_range++;
		ipi_range_size += (addr2 - addr1) / PAGE_SIZE;
#endif
	}
}

void
smp_masked_invltlb(u_int mask)
{
	if (smp_started) {
		smp_targeted_tlb_shootdown(mask, IPI_INVLTLB, 0, 0);
#ifdef COUNT_XINVLTLB_HITS
		ipi_masked_global++;
#endif
	}
}

void
smp_masked_invlpg(u_int mask, vm_offset_t addr)
{
	if (smp_started) {
		smp_targeted_tlb_shootdown(mask, IPI_INVLPG, addr, 0);
#ifdef COUNT_XINVLTLB_HITS
		ipi_masked_page++;
#endif
	}
}

void
smp_masked_invlpg_range(u_int mask, vm_offset_t addr1, vm_offset_t addr2)
{
	if (smp_started) {
		smp_targeted_tlb_shootdown(mask, IPI_INVLRNG, addr1, addr2);
#ifdef COUNT_XINVLTLB_HITS
		ipi_masked_range++;
		ipi_masked_range_size += (addr2 - addr1) / PAGE_SIZE;
#endif
	}
}


void
ipi_bitmap_handler(struct clockframe frame)
{
	int cpu = PCPU_GET(cpuid);
	u_int ipi_bitmap;

	ipi_bitmap = atomic_readandclear_int(&cpu_ipi_pending[cpu]);

	/* Nothing to do for AST */
}

/*
 * send an IPI to a set of cpus.
 */
void
ipi_selected(u_int32_t cpus, u_int ipi)
{
	int cpu;
	u_int bitmap = 0;
	u_int old_pending;
	u_int new_pending;

	if (IPI_IS_BITMAPED(ipi)) { 
		bitmap = 1 << ipi;
		ipi = IPI_BITMAP_VECTOR;
	}

	CTR3(KTR_SMP, "%s: cpus: %x ipi: %x", __func__, cpus, ipi);
	while ((cpu = ffs(cpus)) != 0) {
		cpu--;
		cpus &= ~(1 << cpu);

		KASSERT(cpu_apic_ids[cpu] != -1,
		    ("IPI to non-existent CPU %d", cpu));

		if (bitmap) {
			do {
				old_pending = cpu_ipi_pending[cpu];
				new_pending = old_pending | bitmap;
			} while  (!atomic_cmpset_int(&cpu_ipi_pending[cpu],old_pending, new_pending));	

			if (old_pending)
				continue;
		}

		lapic_ipi_vectored(ipi, cpu_apic_ids[cpu]);
	}

}

/*
 * send an IPI INTerrupt containing 'vector' to all CPUs, including myself
 */
void
ipi_all(u_int ipi)
{

	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	lapic_ipi_vectored(ipi, APIC_IPI_DEST_ALL);
}

/*
 * send an IPI to all CPUs EXCEPT myself
 */
void
ipi_all_but_self(u_int ipi)
{

	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	lapic_ipi_vectored(ipi, APIC_IPI_DEST_OTHERS);
}

/*
 * send an IPI to myself
 */
void
ipi_self(u_int ipi)
{

	CTR2(KTR_SMP, "%s: ipi: %x", __func__, ipi);
	lapic_ipi_vectored(ipi, APIC_IPI_DEST_SELF);
}

/*
 * This is called once the rest of the system is up and running and we're
 * ready to let the AP's out of the pen.
 */
static void
release_aps(void *dummy __unused)
{

	if (mp_ncpus == 1) 
		return;
	mtx_lock_spin(&sched_lock);
	atomic_store_rel_int(&aps_ready, 1);
	while (smp_started == 0)
		ia32_pause();
	mtx_unlock_spin(&sched_lock);
}
SYSINIT(start_aps, SI_SUB_SMP, SI_ORDER_FIRST, release_aps, NULL);

static int
sysctl_hlt_cpus(SYSCTL_HANDLER_ARGS)
{
	u_int mask;
	int error;

	mask = hlt_cpus_mask;
	error = sysctl_handle_int(oidp, &mask, 0, req);
	if (error || !req->newptr)
		return (error);

	if (logical_cpus_mask != 0 &&
	    (mask & logical_cpus_mask) == logical_cpus_mask)
		hlt_logical_cpus = 1;
	else
		hlt_logical_cpus = 0;

	if ((mask & all_cpus) == all_cpus)
		mask &= ~(1<<0);
	hlt_cpus_mask = mask;
	return (error);
}
SYSCTL_PROC(_machdep, OID_AUTO, hlt_cpus, CTLTYPE_INT|CTLFLAG_RW,
    0, 0, sysctl_hlt_cpus, "IU",
    "Bitmap of CPUs to halt.  101 (binary) will halt CPUs 0 and 2.");

static int
sysctl_hlt_logical_cpus(SYSCTL_HANDLER_ARGS)
{
	int disable, error;

	disable = hlt_logical_cpus;
	error = sysctl_handle_int(oidp, &disable, 0, req);
	if (error || !req->newptr)
		return (error);

	if (disable)
		hlt_cpus_mask |= logical_cpus_mask;
	else
		hlt_cpus_mask &= ~logical_cpus_mask;

	if ((hlt_cpus_mask & all_cpus) == all_cpus)
		hlt_cpus_mask &= ~(1<<0);

	hlt_logical_cpus = disable;
	return (error);
}

static void
cpu_hlt_setup(void *dummy __unused)
{

	if (logical_cpus_mask != 0) {
		TUNABLE_INT_FETCH("machdep.hlt_logical_cpus",
		    &hlt_logical_cpus);
		sysctl_ctx_init(&logical_cpu_clist);
		SYSCTL_ADD_PROC(&logical_cpu_clist,
		    SYSCTL_STATIC_CHILDREN(_machdep), OID_AUTO,
		    "hlt_logical_cpus", CTLTYPE_INT|CTLFLAG_RW, 0, 0,
		    sysctl_hlt_logical_cpus, "IU", "");
		SYSCTL_ADD_UINT(&logical_cpu_clist,
		    SYSCTL_STATIC_CHILDREN(_machdep), OID_AUTO,
		    "logical_cpus_mask", CTLTYPE_INT|CTLFLAG_RD,
		    &logical_cpus_mask, 0, "");

		if (hlt_logical_cpus)
			hlt_cpus_mask |= logical_cpus_mask;
	}
}
SYSINIT(cpu_hlt, SI_SUB_SMP, SI_ORDER_ANY, cpu_hlt_setup, NULL);

int
mp_grab_cpu_hlt(void)
{
	u_int mask = PCPU_GET(cpumask);
#ifdef MP_WATCHDOG
	u_int cpuid = PCPU_GET(cpuid);
#endif
	int retval;

#ifdef MP_WATCHDOG
	ap_watchdog(cpuid);
#endif

	retval = mask & hlt_cpus_mask;
	while (mask & hlt_cpus_mask)
		__asm __volatile("sti; hlt" : : : "memory");
	return (retval);
}
