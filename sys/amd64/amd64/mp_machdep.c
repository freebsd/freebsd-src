/*-
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

#include "opt_cpu.h"
#include "opt_kstack_pages.h"
#include "opt_mp_watchdog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
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
#include <machine/psl.h>
#include <machine/smp.h>
#include <machine/specialreg.h>
#include <machine/tss.h>

#define WARMBOOT_TARGET		0
#define WARMBOOT_OFF		(KERNBASE + 0x0467)
#define WARMBOOT_SEG		(KERNBASE + 0x0469)

#define CMOS_REG		(0x70)
#define CMOS_DATA		(0x71)
#define BIOS_RESET		(0x0f)
#define BIOS_WARM		(0x0a)

/* lock region used by kernel profiling */
int	mcount_lock;

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

/* Free these after use */
void *bootstacks[MAXCPU];

/* Hotwire a 0->4MB V==P mapping */
extern pt_entry_t *KPTphys;

/* SMP page table page */
extern pt_entry_t *SMPpt;

struct pcb stoppcbs[MAXCPU];

/* Variables needed for SMP tlb shootdown. */
vm_offset_t smp_tlb_addr1;
vm_offset_t smp_tlb_addr2;
volatile int smp_tlb_wait;

extern inthand_t IDTVEC(fast_syscall), IDTVEC(fast_syscall32);

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
static int	start_ap(int apic_id);
static void	release_aps(void *dummy);

static int	hlt_logical_cpus;
static struct	sysctl_ctx_list logical_cpu_clist;
static u_int	bootMP_size;

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

	bootMP_size = mptramp_end - mptramp_start;
	boot_address = trunc_page(basemem * 1024); /* round down to 4k boundary */
	if (((basemem * 1024) - boot_address) < bootMP_size)
		boot_address -= PAGE_SIZE;	/* not enough, lower by 4k */
	/* 3 levels of page table pages */
	mptramp_pagetables = boot_address - (PAGE_SIZE * 3);

	return mptramp_pagetables;
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
	if (apic_id > mp_maxid)
		mp_maxid = apic_id;
	if (bootverbose)
		printf("SMP: Added CPU %d (%s)\n", apic_id, boot_cpu ? "BSP" :
		    "AP");
	
}

void
cpu_mp_setmaxid(void)
{

	/*
	 * mp_maxid should be already set by calls to cpu_add().
	 * Just sanity check its value here.
	 */
	if (mp_ncpus == 0)
		KASSERT(mp_maxid == 0,
		    ("%s: mp_ncpus is zero, but mp_maxid is not", __func__));
	else if (mp_ncpus == 1)
		mp_maxid = 0;
	else
		KASSERT(mp_maxid >= mp_ncpus - 1,
		    ("%s: counters out of sync: max %d, count %d", __func__,
			mp_maxid, mp_ncpus));
		
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
		mp_maxid = 0;
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

	/* Initialize the logical ID to APIC ID table. */
	for (i = 0; i < MAXCPU; i++) {
		cpu_apic_ids[i] = -1;
		cpu_ipi_pending[i] = 0;
	}

	/* Install an inter-CPU IPI for TLB invalidation */
	setidt(IPI_INVLTLB, IDTVEC(invltlb), SDT_SYSIGT, SEL_KPL, 0);
	setidt(IPI_INVLPG, IDTVEC(invlpg), SDT_SYSIGT, SEL_KPL, 0);
	setidt(IPI_INVLRNG, IDTVEC(invlrng), SDT_SYSIGT, SEL_KPL, 0);
	
	/* Install an inter-CPU IPI for all-CPU rendezvous */
	setidt(IPI_RENDEZVOUS, IDTVEC(rendezvous), SDT_SYSIGT, SEL_KPL, 0);

	/* Install generic inter-CPU IPI handler */
	setidt(IPI_BITMAP_VECTOR, IDTVEC(ipi_intr_bitmap_handler),
	       SDT_SYSIGT, SEL_KPL, 0);

	/* Install an inter-CPU IPI for CPU stop/restart */
	setidt(IPI_STOP, IDTVEC(cpustop), SDT_SYSIGT, SEL_KPL, 0);

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
	struct pcpu *pc;
	u_int64_t msr, cr0;
	int cpu, gsel_tss;

	/* Set by the startup code for us to use */
	cpu = bootAP;

	/* Init tss */
	common_tss[cpu] = common_tss[0];
	common_tss[cpu].tss_rsp0 = 0;   /* not used until after switch */
	common_tss[cpu].tss_iobase = sizeof(struct amd64tss);

	gdt_segs[GPROC0_SEL].ssd_base = (long) &common_tss[cpu];
	ssdtosyssd(&gdt_segs[GPROC0_SEL],
	   (struct system_segment_descriptor *)&gdt[GPROC0_SEL]);

	lgdt(&r_gdt);			/* does magic intra-segment return */

	/* Get per-cpu data */
	pc = &__pcpu[cpu];

	/* prime data page for it to use */
	pcpu_init(pc, cpu, sizeof(struct pcpu));
	pc->pc_apic_id = cpu_apic_ids[cpu];
	pc->pc_prvspace = pc;
	pc->pc_curthread = 0;
	pc->pc_tssp = &common_tss[cpu];
	pc->pc_rsp0 = 0;

	wrmsr(MSR_FSBASE, 0);		/* User value */
	wrmsr(MSR_GSBASE, (u_int64_t)pc);
	wrmsr(MSR_KGSBASE, (u_int64_t)pc);	/* XXX User value while we're in the kernel */

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

	/* Set up the fast syscall stuff */
	msr = rdmsr(MSR_EFER) | EFER_SCE;
	wrmsr(MSR_EFER, msr);
	wrmsr(MSR_LSTAR, (u_int64_t)IDTVEC(fast_syscall));
	wrmsr(MSR_CSTAR, (u_int64_t)IDTVEC(fast_syscall32));
	msr = ((u_int64_t)GSEL(GCODE_SEL, SEL_KPL) << 32) |
	      ((u_int64_t)GSEL(GUCODE32_SEL, SEL_UPL) << 48);
	wrmsr(MSR_STAR, msr);
	wrmsr(MSR_SF_MASK, PSL_NT|PSL_T|PSL_I|PSL_C|PSL_D);

	/* Disable local apic just to be sure. */
	lapic_disable();

	/* signal our startup to the BSP. */
	mp_naps++;

	/* Spin until the BSP releases the AP's. */
	while (!aps_ready)
		ia32_pause();

	/* set up CPU registers and state */
	cpu_setregs();

	/* set up SSE/NX registers */
	initializecpu();

	/* set up FPU state on the AP */
	fpuinit();

	/* A quick check from sanity claus */
	if (PCPU_GET(apic_id) != lapic_id()) {
		printf("SMP: cpuid = %d\n", PCPU_GET(cpuid));
		printf("SMP: actual apic_id = %d\n", lapic_id());
		printf("SMP: correct apic_id = %d\n", PCPU_GET(apic_id));
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
	u_char mpbiosreason;
	u_int32_t mpbioswarmvec;
	int apic_id, cpu, i;
	u_int64_t *pt4, *pt3, *pt2;
	vm_offset_t va = boot_address + KERNBASE;

	mtx_init(&ap_boot_mtx, "ap boot", NULL, MTX_SPIN);

	/* install the AP 1st level boot code */
	pmap_kenter(va, boot_address);
	pmap_invalidate_page(kernel_pmap, va);
	bcopy(mptramp_start, (void *)va, bootMP_size);

	/* Locate the page tables, they'll be below the trampoline */
	pt4 = (u_int64_t *)(uintptr_t)(mptramp_pagetables + KERNBASE);
	pt3 = pt4 + (PAGE_SIZE) / sizeof(u_int64_t);
	pt2 = pt3 + (PAGE_SIZE) / sizeof(u_int64_t);

	/* Create the initial 1GB replicated page tables */
	for (i = 0; i < 512; i++) {
		/* Each slot of the level 4 pages points to the same level 3 page */
		pt4[i] = (u_int64_t)(uintptr_t)(mptramp_pagetables + PAGE_SIZE);
		pt4[i] |= PG_V | PG_RW | PG_U;

		/* Each slot of the level 3 pages points to the same level 2 page */
		pt3[i] = (u_int64_t)(uintptr_t)(mptramp_pagetables + (2 * PAGE_SIZE));
		pt3[i] |= PG_V | PG_RW | PG_U;

		/* The level 2 page slots are mapped with 2MB pages for 1GB. */
		pt2[i] = i * (2 * 1024 * 1024);
		pt2[i] |= PG_V | PG_RW | PG_PS | PG_U;
	}

	/* save the current value of the warm-start vector */
	mpbioswarmvec = *((u_int32_t *) WARMBOOT_OFF);
	outb(CMOS_REG, BIOS_RESET);
	mpbiosreason = inb(CMOS_DATA);

	/* setup a vector to our boot code */
	*((volatile u_short *) WARMBOOT_OFF) = WARMBOOT_TARGET;
	*((volatile u_short *) WARMBOOT_SEG) = (boot_address >> 4);
	outb(CMOS_REG, BIOS_RESET);
	outb(CMOS_DATA, BIOS_WARM);	/* 'warm-start' */

	/* start each AP */
	cpu = 0;
	for (apic_id = 0; apic_id < MAXCPU; apic_id++) {

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

		/* allocate and set up an idle stack data page */
		bootstacks[cpu] = (char *)kmem_alloc(kernel_map, KSTACK_PAGES * PAGE_SIZE);

		bootSTK = (char *)bootstacks[cpu] + KSTACK_PAGES * PAGE_SIZE - 8;
		bootAP = cpu;

		/* attempt to start the Application Processor */
		if (!start_ap(apic_id)) {
			/* restore the warmstart vector */
			*(u_int32_t *) WARMBOOT_OFF = mpbioswarmvec;
			panic("AP #%d (PHY# %d) failed!", cpu, apic_id);
		}

		all_cpus |= (1 << cpu);		/* record AP in CPU map */
	}

	/* build our map of 'other' CPUs */
	PCPU_SET(other_cpus, all_cpus & ~PCPU_GET(cpumask));

	/* restore the warmstart vector */
	*(u_int32_t *) WARMBOOT_OFF = mpbioswarmvec;

	outb(CMOS_REG, BIOS_RESET);
	outb(CMOS_DATA, mpbiosreason);

	/* number of APs actually started */
	return mp_naps;
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
	for (ms = 0; ms < 50; ms++) {
		if (mp_naps > cpus)
			return 1;	/* return SUCCESS */
		DELAY(100000);
	}
	return 0;		/* return FAILURE */
}

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

	if (smp_started)
		smp_tlb_shootdown(IPI_INVLTLB, 0, 0);
}

void
smp_invlpg(vm_offset_t addr)
{

	if (smp_started)
		smp_tlb_shootdown(IPI_INVLPG, addr, 0);
}

void
smp_invlpg_range(vm_offset_t addr1, vm_offset_t addr2)
{

	if (smp_started)
		smp_tlb_shootdown(IPI_INVLRNG, addr1, addr2);
}

void
smp_masked_invltlb(u_int mask)
{

	if (smp_started)
		smp_targeted_tlb_shootdown(mask, IPI_INVLTLB, 0, 0);
}

void
smp_masked_invlpg(u_int mask, vm_offset_t addr)
{

	if (smp_started)
		smp_targeted_tlb_shootdown(mask, IPI_INVLPG, addr, 0);
}

void
smp_masked_invlpg_range(u_int mask, vm_offset_t addr1, vm_offset_t addr2)
{

	if (smp_started)
		smp_targeted_tlb_shootdown(mask, IPI_INVLRNG, addr1, addr2);
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
