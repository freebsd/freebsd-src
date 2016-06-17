/*
 *	x86 SMP booting functions
 *
 *	(c) 1995 Alan Cox, Building #3 <alan@redhat.com>
 *	(c) 1998, 1999, 2000 Ingo Molnar <mingo@redhat.com>
 *	Copyright 2001 Andi Kleen, SuSE Labs.
 *
 *	Much of the core SMP work is based on previous work by Thomas Radke, to
 *	whom a great many thanks are extended.
 *
 *	Thanks to Intel for making available several different Pentium,
 *	Pentium Pro and Pentium-II/Xeon MP machines.
 *	Original development of Linux SMP code supported by Caldera.
 *
 *	This code is released under the GNU General Public License version 2 or
 *	later.
 *
 *	Fixes
 *		Felix Koop	:	NR_CPUS used properly
 *		Jose Renau	:	Handle single CPU case.
 *		Alan Cox	:	By repeated request 8) - Total BogoMIP report.
 *		Greg Wright	:	Fix for kernel stacks panic.
 *		Erich Boleyn	:	MP v1.4 and additional changes.
 *	Matthias Sattler	:	Changes for 2.1 kernel map.
 *	Michel Lespinasse	:	Changes for 2.1 kernel map.
 *	Michael Chastain	:	Change trampoline.S to gnu as.
 *		Alan Cox	:	Dumb bug: 'B' step PPro's are fine
 *		Ingo Molnar	:	Added APIC timers, based on code
 *					from Jose Renau
 *		Ingo Molnar	:	various cleanups and rewrites
 *		Tigran Aivazian	:	fixed "0.00 in /proc/uptime on SMP" bug.
 *	Maciej W. Rozycki	:	Bits for genuine 82489DX APICs
 *	Andi Kleen		:	Changed for SMP boot into long mode.
 */

#include <linux/config.h>
#include <linux/init.h>

#include <linux/mm.h>
#include <linux/kernel_stat.h>
#include <linux/smp_lock.h>
#include <linux/irq.h>
#include <linux/bootmem.h>

#include <linux/delay.h>
#include <linux/mc146818rtc.h>
#include <asm/mtrr.h>
#include <asm/pgalloc.h>
#include <asm/desc.h>
#include <asm/kdebug.h>
#include <asm/timex.h>
#include <asm/proto.h>
#include <asm/acpi.h>

/* Setup configured maximum number of CPUs to activate */
unsigned int max_cpus = NR_CPUS;

static int cpu_mask = -1; 

/* Total count of live CPUs */
int smp_num_cpus = 1;

/* Number of siblings per CPU package */
int smp_num_siblings = 1;
int __initdata phys_proc_id[NR_CPUS]; /* Package ID of each logical CPU */
int cpu_sibling_map[NR_CPUS] __cacheline_aligned;

/* Bitmask of currently online CPUs */
unsigned long cpu_online_map;

/* which CPU (physical APIC ID) maps to which logical CPU number */
volatile int x86_apicid_to_cpu[NR_CPUS];
/* which logical CPU number maps to which CPU (physical APIC ID) */
volatile int x86_cpu_to_apicid[NR_CPUS];

static volatile unsigned long cpu_callin_map;
static volatile unsigned long cpu_callout_map;

/* Per CPU bogomips and other parameters */
struct cpuinfo_x86 cpu_data[NR_CPUS] __cacheline_aligned;

/* Set when the idlers are all forked */
int smp_threads_ready;

extern void time_init_smp(void);

/*
 * Setup routine for controlling SMP activation
 *
 * Command-line option of "nosmp" or "maxcpus=0" will disable SMP
 * activation entirely (the MPS table probe still happens, though).
 */

static int __init nosmp(char *str)
{
	max_cpus = 0;
	return 1;
}

__setup("nosmp", nosmp);

static int __init cpumask(char *str)
{
	get_option(&str, &cpu_mask);
	return 1;
}

__setup("cpumask=", cpumask); 

/*
 * Trampoline 80x86 program as an array.
 */

extern unsigned char trampoline_data [];
extern unsigned char trampoline_end  [];
static unsigned char *trampoline_base;

/*
 * Currently trivial. Write the real->protected mode
 * bootstrap into the page concerned. The caller
 * has made sure it's suitably aligned.
 */

static unsigned long __init setup_trampoline(void)
{
	extern volatile __u32 tramp_gdt_ptr; 
	tramp_gdt_ptr = __pa_symbol(&gdt_table); 
	memcpy(trampoline_base, trampoline_data, trampoline_end - trampoline_data);
	return virt_to_phys(trampoline_base);
}

/*
 * We are called very early to get the low memory for the
 * SMP bootup trampoline page.
 */
void __init smp_alloc_memory(void)
{
	trampoline_base = __va(0x6000); /* reserved in setup.c */
}

/*
 * The bootstrap kernel entry code has set these up. Save them for
 * a given CPU
 */

void __init smp_store_cpu_info(int id)
{
	struct cpuinfo_x86 *c = cpu_data + id;

	*c = boot_cpu_data;
	identify_cpu(c);
}

/*
 * Architecture specific routine called by the kernel just before init is
 * fired off. This allows the BP to have everything in order [we hope].
 * At the end of this all the APs will hit the system scheduling and off
 * we go. Each AP will load the system gdt's and jump through the kernel
 * init into idle(). At this point the scheduler will one day take over
 * and give them jobs to do. smp_callin is a standard routine
 * we use to track CPUs as they power up.
 */

static atomic_t smp_commenced = ATOMIC_INIT(0);

void __init smp_commence(void)
{
	/*
	 * Lets the callins below out of their loop.
	 */
	Dprintk("Setting commenced=1, go go go\n");

	wmb();
	atomic_set(&smp_commenced,1);
}

/*
 * TSC synchronization.
 *
 * We first check wether all CPUs have their TSC's synchronized,
 * then we print a warning if not, and always resync.
 */

static atomic_t tsc_start_flag = ATOMIC_INIT(0);
static atomic_t tsc_count_start = ATOMIC_INIT(0);
static atomic_t tsc_count_stop = ATOMIC_INIT(0);
static unsigned long long tsc_values[NR_CPUS];

#define NR_LOOPS 5

static inline unsigned long long div64 (unsigned long long a, unsigned long b)
{
	return a/b;
}

static void __init synchronize_tsc_bp (void)
{
	int i;
	unsigned long long t0;
	unsigned long long sum, avg;
	long long delta;
	unsigned long one_usec;
	int buggy = 0;

	printk("checking TSC synchronization across CPUs: ");

	one_usec = cpu_khz / 1000;

	atomic_set(&tsc_start_flag, 1);
	wmb();

	/*
	 * We loop a few times to get a primed instruction cache,
	 * then the last pass is more or less synchronized and
	 * the BP and APs set their cycle counters to zero all at
	 * once. This reduces the chance of having random offsets
	 * between the processors, and guarantees that the maximum
	 * delay between the cycle counters is never bigger than
	 * the latency of information-passing (cachelines) between
	 * two CPUs.
	 */
	for (i = 0; i < NR_LOOPS; i++) {
		/*
		 * all APs synchronize but they loop on '== num_cpus'
		 */
		while (atomic_read(&tsc_count_start) != smp_num_cpus-1) mb();
		atomic_set(&tsc_count_stop, 0);
		wmb();
		/*
		 * this lets the APs save their current TSC:
		 */
		atomic_inc(&tsc_count_start);

		sync_core();
		rdtscll(tsc_values[smp_processor_id()]);

		/*
		 * We clear the TSC in the last loop:
		 */

		if (i == NR_LOOPS-1) {
			write_tsc(0, 0);
		}

		/*
		 * Wait for all APs to leave the synchronization point:
		 */
		while (atomic_read(&tsc_count_stop) != smp_num_cpus-1) mb();
		atomic_set(&tsc_count_start, 0);
		wmb();
		atomic_inc(&tsc_count_stop);
	}

	sum = 0;
	for (i = 0; i < smp_num_cpus; i++) {
		t0 = tsc_values[i];
		sum += t0;
	}
	avg = div64(sum, smp_num_cpus);

	sum = 0;
	for (i = 0; i < smp_num_cpus; i++) {
		delta = tsc_values[i] - avg;
		if (delta < 0)
			delta = -delta;
		/*
		 * We report bigger than 2 microseconds clock differences.
		 */
		if (delta > 2*one_usec) {
			long realdelta;
			if (!buggy) {
				buggy = 1;
				printk("\n");
			}
			realdelta = div64(delta, one_usec);
			if (tsc_values[i] < avg)
				realdelta = -realdelta;

			printk("BIOS BUG: CPU#%d improperly initialized, has %ld usecs TSC skew! FIXED.\n",
				i, realdelta);
		}

		sum += delta;
	}
	if (!buggy)
		printk("passed.\n");
}

static void __init synchronize_tsc_ap (void)
{
	int i;

	/*
	 * smp_num_cpus is not necessarily known at the time
	 * this gets called, so we first wait for the BP to
	 * finish SMP initialization:
	 */
	while (!atomic_read(&tsc_start_flag)) mb();

	for (i = 0; i < NR_LOOPS; i++) {
		atomic_inc(&tsc_count_start);
		while (atomic_read(&tsc_count_start) != smp_num_cpus) mb();

		sync_core();
		rdtscll(tsc_values[smp_processor_id()]);
		if (i == NR_LOOPS-1)
			write_tsc(0, 0);

		atomic_inc(&tsc_count_stop);
		while (atomic_read(&tsc_count_stop) != smp_num_cpus) mb();
	}
}
#undef NR_LOOPS

extern void calibrate_delay(void);

static atomic_t init_deasserted;

void __init smp_callin(void)
{
	int cpuid, phys_id;
	unsigned long timeout;

	/*
	 * If waken up by an INIT in an 82489DX configuration
	 * we may get here before an INIT-deassert IPI reaches
	 * our local APIC.  We have to wait for the IPI or we'll
	 * lock up on an APIC access.
	 */
	while (!atomic_read(&init_deasserted));

	/*
	 * (This works even if the APIC is not enabled.)
	 */
	phys_id = GET_APIC_ID(apic_read(APIC_ID));
	cpuid = current->processor;
	if (test_and_set_bit(cpuid, &cpu_online_map)) {
		printk("huh, phys CPU#%d, CPU#%d already present??\n",
					phys_id, cpuid);
		BUG();
	}
	Dprintk("CPU#%d (phys ID: %d) waiting for CALLOUT\n", cpuid, phys_id);

	/*
	 * STARTUP IPIs are fragile beasts as they might sometimes
	 * trigger some glue motherboard logic. Complete APIC bus
	 * silence for 1 second, this overestimates the time the
	 * boot CPU is spending to send the up to 2 STARTUP IPIs
	 * by a factor of two. This should be enough.
	 */

	/*
	 * Waiting 2s total for startup (udelay is not yet working)
	 */
	timeout = jiffies + 2*HZ;
	while (time_before(jiffies, timeout)) {
		/*
		 * Has the boot CPU finished it's STARTUP sequence?
		 */
		if (test_bit(cpuid, &cpu_callout_map))
			break;
		rep_nop();
	}

	if (!time_before(jiffies, timeout)) {
		printk("BUG: CPU%d started up but did not get a callout!\n",
			cpuid);
		BUG();
	}

	/*
	 * the boot CPU has finished the init stage and is spinning
	 * on callin_map until we finish. We are free to set up this
	 * CPU, first the APIC. (this is probably redundant on most
	 * boards)
	 */

	Dprintk("CALLIN, before setup_local_APIC().\n");
	setup_local_APIC();

	if (nmi_watchdog == NMI_IO_APIC) {
		disable_8259A_irq(0);
		enable_NMI_through_LVT0(NULL);
		enable_8259A_irq(0);
	}

	sti();

#ifdef CONFIG_MTRR
	/*
	 * Must be done before calibration delay is computed
	 */
	mtrr_init_secondary_cpu ();
#endif
	/*
	 * Get our bogomips.
	 */
	calibrate_delay();
	Dprintk("Stack at about %p\n",&cpuid);

	/*
	 * Save our processor parameters
	 */
 	smp_store_cpu_info(cpuid);

	/*
	 * Allow the master to continue.
	 */
	set_bit(cpuid, &cpu_callin_map);

	/*
	 *      Synchronize the TSC with the BP
	 */
	if (cpu_has_tsc)
		synchronize_tsc_ap();
}

int cpucount;

/*
 * Activate a secondary processor.
 */
int __init start_secondary(void *unused)
{
	/*
	 * Dont put anything before smp_callin(), SMP
	 * booting is too fragile that we want to limit the
	 * things done here to the most necessary things.
	 */
	cpu_init();
	smp_callin();
	while (!atomic_read(&smp_commenced))
		rep_nop();
	/*
	 * low-memory mappings have been cleared, flush them from
	 * the local TLBs too.
	 */
	local_flush_tlb();

	cpu_idle();
	return 0;
}

/*
 * Everything has been set up for the secondary
 * CPUs - they just need to reload everything
 * from the task structure
 * This function must not return.
 */
void __init initialize_secondary(void)
{
	struct task_struct *me = stack_current();

	/*
	 * We don't actually need to load the full TSS,
	 * basically just the stack pointer and the eip.
	 */

	asm volatile(
		"movq %0,%%rsp\n\t"
		"jmp *%1"
		:
		:"r" (me->thread.rsp),"r" (me->thread.rip));
}

extern volatile void *init_rsp; 
extern void (*initial_code)(void);

static int __init fork_by_hand(void)
{
	struct pt_regs regs;
	/*
	 * don't care about the eip and regs settings since
	 * we'll never reschedule the forked task.
	 */
	return do_fork(CLONE_VM|CLONE_PID, 0, &regs, 0);
}

#if APIC_DEBUG
static inline void inquire_remote_apic(int apicid)
{
	int i, regs[] = { APIC_ID >> 4, APIC_LVR >> 4, APIC_SPIV >> 4 };
	char *names[] = { "ID", "VERSION", "SPIV" };
	int timeout, status;

	printk("Inquiring remote APIC #%d...\n", apicid);

	for (i = 0; i < sizeof(regs) / sizeof(*regs); i++) {
		printk("... APIC #%d %s: ", apicid, names[i]);

		/*
		 * Wait for idle.
		 */
		apic_wait_icr_idle();

		apic_write_around(APIC_ICR2, SET_APIC_DEST_FIELD(apicid));
		apic_write_around(APIC_ICR, APIC_DM_REMRD | regs[i]);

		timeout = 0;
		do {
			udelay(100);
			status = apic_read(APIC_ICR) & APIC_ICR_RR_MASK;
		} while (status == APIC_ICR_RR_INPROG && timeout++ < 1000);

		switch (status) {
		case APIC_ICR_RR_VALID:
			status = apic_read(APIC_RRR);
			printk("%08x\n", status);
			break;
		default:
			printk("failed\n");
		}
	}
}
#endif

static int __init do_boot_cpu (int apicid)
{
	struct task_struct *idle;
	unsigned long send_status, accept_status, boot_status, maxlvt;
	int timeout, num_starts, j, cpu;
	unsigned long start_eip;

	cpu = ++cpucount;

	/*
	 * We can't use kernel_thread since we must avoid to
	 * reschedule the child.
	 */
	if (fork_by_hand() < 0)
		panic("failed fork for CPU %d", cpu);

	/*
	 * We remove it from the pidhash and the runqueue
	 * once we got the process:
	 */
	idle = init_task.prev_task;
	if (!idle)
		panic("No idle process for CPU %d", cpu);

	idle->processor = cpu;
	x86_cpu_to_apicid[cpu] = apicid;
	x86_apicid_to_cpu[apicid] = cpu;
	idle->cpus_runnable = 1<<cpu; 
	idle->cpus_allowed = 1<<cpu;
	idle->thread.rip = (unsigned long)start_secondary;
	idle->thread.rsp = (unsigned long)idle + THREAD_SIZE - 8;

	del_from_runqueue(idle);
	unhash_process(idle);
	cpu_pda[cpu].pcurrent = init_tasks[cpu] = idle;

	/* start_eip had better be page-aligned! */
	start_eip = setup_trampoline();

	/* So we see what's up   */
	printk("Booting processor %d/%d rip %lx page %p\n", cpu, apicid, start_eip, idle);
	init_rsp = (void *) (THREAD_SIZE + (char *)idle - 16);
	initial_code = initialize_secondary; 

	/*
	 * This grunge runs the startup process for
	 * the targeted processor.
	 */

	atomic_set(&init_deasserted, 0);

	Dprintk("Setting warm reset code and vector.\n");

	CMOS_WRITE(0xa, 0xf);
	local_flush_tlb();
	Dprintk("1.\n");
	*((volatile unsigned short *) phys_to_virt(0x469)) = start_eip >> 4;
	Dprintk("2.\n");
	*((volatile unsigned short *) phys_to_virt(0x467)) = start_eip & 0xf;
	Dprintk("3.\n");

	/*
	 * Be paranoid about clearing APIC errors.
	 */
	if (APIC_INTEGRATED(apic_version[apicid])) {
		apic_read_around(APIC_SPIV);
		apic_write(APIC_ESR, 0);
		apic_read(APIC_ESR);
	}

	/*
	 * Status is now clean
	 */
	send_status = 0;
	accept_status = 0;
	boot_status = 0;

	/*
	 * Starting actual IPI sequence...
	 */

	Dprintk("Asserting INIT.\n");

	/*
	 * Turn INIT on target chip
	 */
	apic_write_around(APIC_ICR2, SET_APIC_DEST_FIELD(apicid));

	/*
	 * Send IPI
	 */
	apic_write_around(APIC_ICR, APIC_INT_LEVELTRIG | APIC_INT_ASSERT
				| APIC_DM_INIT);

	Dprintk("Waiting for send to finish...\n");
	timeout = 0;
	do {
		Dprintk("+");
		udelay(100);
		send_status = apic_read(APIC_ICR) & APIC_ICR_BUSY;
	} while (send_status && (timeout++ < 1000));

	mdelay(10);

	Dprintk("Deasserting INIT.\n");

	/* Target chip */
	apic_write_around(APIC_ICR2, SET_APIC_DEST_FIELD(apicid));

	/* Send IPI */
	apic_write_around(APIC_ICR, APIC_INT_LEVELTRIG | APIC_DM_INIT);

	Dprintk("Waiting for send to finish...\n");
	timeout = 0;
	do {
		Dprintk("+");
		udelay(100);
		send_status = apic_read(APIC_ICR) & APIC_ICR_BUSY;
	} while (send_status && (timeout++ < 1000));

	atomic_set(&init_deasserted, 1);

	/*
	 * Should we send STARTUP IPIs ?
	 *
	 * Determine this based on the APIC version.
	 * If we don't have an integrated APIC, don't
	 * send the STARTUP IPIs.
	 */
	if (APIC_INTEGRATED(apic_version[apicid]))
		num_starts = 2;
	else
		num_starts = 0;

	/*
	 * Run STARTUP IPI loop.
	 */
	Dprintk("#startup loops: %d.\n", num_starts);

	maxlvt = get_maxlvt();

	for (j = 1; j <= num_starts; j++) {
		Dprintk("Sending STARTUP #%d.\n",j);
		apic_read_around(APIC_SPIV);
		apic_write(APIC_ESR, 0);
		apic_read(APIC_ESR);
		Dprintk("After apic_write.\n");

		/*
		 * STARTUP IPI
		 */

		/* Target chip */
		Dprintk("target apic %x\n", SET_APIC_DEST_FIELD(apicid));
		apic_write_around(APIC_ICR2, SET_APIC_DEST_FIELD(apicid));

		Dprintk("after target chip\n"); 

		/* Boot on the stack */
		/* Kick the second */
		apic_write_around(APIC_ICR, APIC_DM_STARTUP
					| (start_eip >> 12));

		Dprintk("after eip write\n"); 

		/*
		 * Give the other CPU some time to accept the IPI.
		 */
		udelay(300);

		Dprintk("Startup point 1.\n");

		Dprintk("Waiting for send to finish...\n");
		timeout = 0;
		do {
			Dprintk("+");
			udelay(100);
			send_status = apic_read(APIC_ICR) & APIC_ICR_BUSY;
		} while (send_status && (timeout++ < 1000));

		/*
		 * Give the other CPU some time to accept the IPI.
		 */
		udelay(200);
		/*
		 * Due to the Pentium erratum 3AP.
		 */
		if (maxlvt > 3) {
			apic_read_around(APIC_SPIV);
			apic_write(APIC_ESR, 0);
		}
		accept_status = (apic_read(APIC_ESR) & 0xEF);
		if (send_status || accept_status)
			break;
	}
	Dprintk("After Startup.\n");

	if (send_status)
		printk("APIC never delivered???\n");
	if (accept_status)
		printk("APIC delivery error (%lx).\n", accept_status);

	if (!send_status && !accept_status) {
		/*
		 * allow APs to start initializing.
		 */
		Dprintk("Before Callout %d.\n", cpu);
		set_bit(cpu, &cpu_callout_map);
		Dprintk("After Callout %d.\n", cpu);

		/*
		 * Wait 5s total for a response
		 */
		for (timeout = 0; timeout < 50000; timeout++) {
			if (test_bit(cpu, &cpu_callin_map))
				break;	/* It has booted */
			udelay(100);
		}

		if (test_bit(cpu, &cpu_callin_map)) {
			/* number CPUs logically, starting from 1 (BSP is 0) */
			Dprintk("OK.\n");
			printk("CPU%d: ", cpu);
			print_cpu_info(&cpu_data[cpu]);
			Dprintk("CPU has booted.\n");
		} else {
			boot_status = 1;
			if (*((volatile unsigned char *)phys_to_virt(8192))
					== 0xA5)
				/* trampoline started but...? */
				printk("Stuck ??\n");
			else
				/* trampoline code not run */
				printk("Not responding.\n");
#if APIC_DEBUG
			inquire_remote_apic(apicid);
#endif
		}
	}
	if (send_status || accept_status || boot_status) {
		x86_cpu_to_apicid[cpu] = -1;
		x86_apicid_to_cpu[apicid] = -1;
		cpucount--;
	}

	/* mark "stuck" area as not stuck */
	*((volatile unsigned int *)phys_to_virt(8192)) = 0;
	
	return cpu; 
}

cycles_t cacheflush_time;

static __init void smp_tune_scheduling (void)
{
	unsigned long cachesize;       /* kB   */
	unsigned long bandwidth = 2000; /* MB/s */
	/*
	 * Rough estimation for SMP scheduling, this is the number of
	 * cycles it takes for a fully memory-limited process to flush
	 * the SMP-local cache.
	 *
	 * (For a P5 this pretty much means we will choose another idle
	 *  CPU almost always at wakeup time (this is due to the small
	 *  L1 cache), on PIIs it's around 50-100 usecs, depending on
	 *  the cache size)
	 */

	if (!cpu_khz) {
		/*
		 * this basically disables processor-affinity
		 * scheduling on SMP without a TSC.
		 */
		cacheflush_time = 0;
		return;
	} else {
		cachesize = boot_cpu_data.x86_cache_size;
		if (cachesize == -1) {
			cachesize = 16; /* Pentiums, 2x8kB cache */
			bandwidth = 100;
		}

		cacheflush_time = (cpu_khz>>10) * (cachesize<<10) / bandwidth;
	}

	cacheflush_time *= 10;  /* Add an NUMA factor */

	printk("per-CPU timeslice cutoff: %ld.%02ld usecs.\n",
		(long)cacheflush_time/(cpu_khz/1000),
		((long)cacheflush_time*100/(cpu_khz/1000)) % 100);
}

/*
 * Cycle through the processors sending APIC IPIs to boot each.
 */

extern int prof_multiplier[NR_CPUS];
extern int prof_old_multiplier[NR_CPUS];
extern int prof_counter[NR_CPUS];

void __init smp_boot_cpus(void)
{
	int apicid, cpu, maxcpu;

#ifdef CONFIG_MTRR
	/*  Must be done before other processors booted  */
	mtrr_init_boot_cpu ();
#endif
	/*
	 * Initialize the logical to physical CPU number mapping
	 * and the per-CPU profiling counter/multiplier
	 */

	for (apicid = 0; apicid < NR_CPUS; apicid++) {
		x86_apicid_to_cpu[apicid] = -1;
		prof_counter[apicid] = 1;
		prof_old_multiplier[apicid] = 1;
		prof_multiplier[apicid] = 1;
	}

	/*
	 * Setup boot CPU information
	 */
	smp_store_cpu_info(0); /* Final full version of the data */
	printk("CPU%d: ", 0);
	print_cpu_info(&cpu_data[0]);

	/*
	 * We have the boot CPU online for sure.
	 */
	set_bit(0, &cpu_online_map);
	x86_apicid_to_cpu[boot_cpu_id] = 0;
	x86_cpu_to_apicid[0] = boot_cpu_id;
	global_irq_holder = 0;
	current->processor = 0;
	init_idle();
	smp_tune_scheduling();

	/*
	 * If we couldnt find an SMP configuration at boot time,
	 * get out of here now!
	 */
	if (!smp_found_config && !acpi_lapic) {
		printk(KERN_NOTICE "SMP motherboard not detected.\n");
		io_apic_irqs = 0;
		cpu_online_map = phys_cpu_present_map = 1;
		smp_num_cpus = 1;
		if (APIC_init_uniprocessor())
			printk(KERN_NOTICE "Local APIC not detected."
					   " Using dummy APIC emulation.\n");
		goto smp_done;
	}

	/*
	 * Should not be necessary because the MP table should list the boot
	 * CPU too, but we do it for the sake of robustness anyway.
	 */
	if (!test_bit(boot_cpu_id, &phys_cpu_present_map)) {
		printk("weird, boot CPU (#%d) not listed by the BIOS.\n",
								 boot_cpu_id);
		phys_cpu_present_map |= (1 << hard_smp_processor_id());
	}

	/*
	 * If we couldn't find a local APIC, then get out of here now!
	 */
	if (APIC_INTEGRATED(apic_version[boot_cpu_id]) &&
	    !test_bit(X86_FEATURE_APIC, boot_cpu_data.x86_capability)) {
		printk(KERN_ERR "BIOS bug, local APIC #%d not detected!...\n",
			boot_cpu_id);
		printk(KERN_ERR "... forcing use of dummy APIC emulation. (tell your hw vendor)\n");
		io_apic_irqs = 0;
		cpu_online_map = phys_cpu_present_map = 1;
		smp_num_cpus = 1;
		apic_disabled = 1;
		goto smp_done;
	}

	verify_local_APIC();

	/*
	 * If SMP should be disabled, then really disable it!
	 */
	if (!max_cpus) {
		smp_found_config = 0;
		printk(KERN_INFO "SMP mode deactivated, forcing use of dummy APIC emulation.\n");
		cpu_online_map = phys_cpu_present_map = 1;
		smp_num_cpus = 1;
		goto smp_done;
	}

	connect_bsp_APIC();
	setup_local_APIC();

	if (GET_APIC_ID(apic_read(APIC_ID)) != boot_cpu_id)
		BUG();

	/*
	 * Now scan the CPU present map and fire up the other CPUs.
	 */
	Dprintk("CPU present map: %lx\n", phys_cpu_present_map);

	maxcpu = 0;
	for (apicid = 0; apicid < NR_CPUS; apicid++) {
		/*
		 * Don't even attempt to start the boot CPU!
		 */
		if (apicid == boot_cpu_id)
			continue;

		if (!(phys_cpu_present_map & (1 << apicid)))
			continue;
		if (((1<<apicid) & cpu_mask) == 0) 
			continue;

		cpu = do_boot_cpu(apicid);

		/*
		 * Make sure we unmap all failed CPUs
		 */
		if ((x86_apicid_to_cpu[apicid] == -1) &&
				(phys_cpu_present_map & (1 << apicid)))
			printk("phys CPU #%d not responding - cannot use it.\n",apicid);
		else if (cpu > maxcpu) 
			maxcpu = cpu; 
	}

	/*
	 * Cleanup possible dangling ends...
	 */
	{
		/*
		 * Install writable page 0 entry to set BIOS data area.
		 */
		local_flush_tlb();

		/*
		 * Paranoid:  Set warm reset code and vector here back
		 * to default values.
		 */
		CMOS_WRITE(0, 0xf);

		*((volatile int *) phys_to_virt(0x467)) = 0;
	}

	/*
	 * Allow the user to impress friends.
	 */

	Dprintk("Before bogomips.\n");
	if (!cpucount) {
		printk(KERN_ERR "Only one processor found.\n");
	} else {
		unsigned long bogosum = 0;
		for (cpu = 0; cpu < NR_CPUS; cpu++)
			if (cpu_online_map & (1<<cpu))
				bogosum += cpu_data[cpu].loops_per_jiffy;
		printk(KERN_INFO "Total of %d processors activated (%lu.%02lu BogoMIPS).\n",
			cpucount+1,
			bogosum/(500000/HZ),
			(bogosum/(5000/HZ))%100);
		Dprintk("Before bogocount - setting activated=1.\n");
	}
	smp_num_cpus = maxcpu + 1;

	Dprintk("Boot done.\n");

	/*
	 * If Hyper-Threading is avaialble, construct cpu_sibling_map[], so
	 * that we can tell the sibling CPU efficiently.
	 */
	if (test_bit(X86_FEATURE_HT, boot_cpu_data.x86_capability)
	    && smp_num_siblings > 1) {
		for (cpu = 0; cpu < NR_CPUS; cpu++)
			cpu_sibling_map[cpu] = NO_PROC_ID;
		
		for (cpu = 0; cpu < smp_num_cpus; cpu++) {
			int 	i;
			
			for (i = 0; i < smp_num_cpus; i++) {
				if (i == cpu)
					continue;
				if (phys_proc_id[cpu] == phys_proc_id[i]) {
					cpu_sibling_map[cpu] = i;
					printk("cpu_sibling_map[%d] = %d\n", cpu, cpu_sibling_map[cpu]);
					break;
				}
			}
			if (cpu_sibling_map[cpu] == NO_PROC_ID) {
				smp_num_siblings = 1;
				printk(KERN_WARNING "WARNING: No sibling found for CPU %d.\n", cpu);
			}
		}
	}
	     
	/*
	 * Here we can be sure that there is an IO-APIC in the system. Let's
	 * go and set it up:
	 */
	if (!skip_ioapic_setup && nr_ioapics)
		setup_IO_APIC();
	else
		nr_ioapics = 0;

	/*
	 * Set up all local APIC timers in the system:
	 */
	setup_APIC_clocks();

	/*
	 * Synchronize the TSC with the AP
	 */
	if (cpu_has_tsc && cpucount)
		synchronize_tsc_bp();

	if (nmi_watchdog != 0) 
		check_nmi_watchdog(); 

smp_done:
	zap_low_mappings();
	time_init_smp();
}
