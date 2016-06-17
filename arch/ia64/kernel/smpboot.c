/*
 * SMP boot-related support
 *
 * Copyright (C) 1998-2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * 01/05/16 Rohit Seth <rohit.seth@intel.com>	Moved SMP booting functions from smp.c to here.
 * 01/04/27 David Mosberger <davidm@hpl.hp.com>	Added ITC synching code.
 */


#define __KERNEL_SYSCALLS__

#include <linux/config.h>

#include <linux/bootmem.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/kernel_stat.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/spinlock.h>
#include <linux/efi.h>

#include <asm/atomic.h>
#include <asm/bitops.h>
#include <asm/cache.h>
#include <asm/current.h>
#include <asm/delay.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/machvec.h>
#include <asm/mca.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/sal.h>
#include <asm/system.h>
#include <asm/unistd.h>

#define SMP_DEBUG 0

#if SMP_DEBUG
#define Dprintk(x...)  printk(x)
#else
#define Dprintk(x...)
#endif


/*
 * ITC synchronization related stuff:
 */
#define MASTER	0
#define SLAVE	(SMP_CACHE_BYTES/8)

#define NUM_ROUNDS	64	/* magic value */
#define NUM_ITERS	5	/* likewise */

static spinlock_t itc_sync_lock = SPIN_LOCK_UNLOCKED;
static volatile unsigned long go[SLAVE + 1];

#define DEBUG_ITC_SYNC	0

extern void __init calibrate_delay (void);
extern void start_ap (void);
extern unsigned long ia64_iobase;

int cpucount;

/* Setup configured maximum number of CPUs to activate */
static int max_cpus = -1;

/* Total count of live CPUs */
int smp_num_cpus = 1;

/* Bitmask of currently online CPUs */
volatile unsigned long cpu_online_map;

/* which logical CPU number maps to which CPU (physical APIC ID) */
volatile int ia64_cpu_to_sapicid[NR_CPUS];

static volatile unsigned long cpu_callin_map;

struct smp_boot_data smp_boot_data __initdata;

/* Set when the idlers are all forked */
int smp_threads_ready;

unsigned long ap_wakeup_vector = -1; /* External Int use to wakeup APs */

char __initdata no_int_routing;

unsigned char smp_int_redirect; /* are INT and IPI redirectable by the chipset? */

/*
 * Setup routine for controlling SMP activation
 *
 * Command-line option of "nosmp" or "maxcpus=0" will disable SMP
 * activation entirely (the MPS table probe still happens, though).
 *
 * Command-line option of "maxcpus=<NUM>", where <NUM> is an integer
 * greater than 0, limits the maximum number of CPUs activated in
 * SMP mode to <NUM>.
 */

static int __init
nosmp (char *str)
{
	max_cpus = 0;
	return 1;
}

__setup("nosmp", nosmp);

static int __init
maxcpus (char *str)
{
	get_option(&str, &max_cpus);
	return 1;
}

__setup("maxcpus=", maxcpus);

static int __init
nointroute (char *str)
{
	no_int_routing = 1;
	return 1;
}

__setup("nointroute", nointroute);

void
sync_master (void *arg)
{
	unsigned long flags, i;

	go[MASTER] = 0;

	local_irq_save(flags);
	{
		for (i = 0; i < NUM_ROUNDS*NUM_ITERS; ++i) {
			while (!go[MASTER]);
			go[MASTER] = 0;
			go[SLAVE] = ia64_get_itc();
		}
	}
	local_irq_restore(flags);
}

/*
 * Return the number of cycles by which our itc differs from the itc on the master
 * (time-keeper) CPU.  A positive number indicates our itc is ahead of the master,
 * negative that it is behind.
 */
static inline long
get_delta (long *rt, long *master)
{
	unsigned long best_t0 = 0, best_t1 = ~0UL, best_tm = 0;
	unsigned long tcenter, t0, t1, tm;
	long i;

	for (i = 0; i < NUM_ITERS; ++i) {
		t0 = ia64_get_itc();
		go[MASTER] = 1;
		while (!(tm = go[SLAVE]));
		go[SLAVE] = 0;
		t1 = ia64_get_itc();

		if (t1 - t0 < best_t1 - best_t0)
			best_t0 = t0, best_t1 = t1, best_tm = tm;
	}

	*rt = best_t1 - best_t0;
	*master = best_tm - best_t0;

	/* average best_t0 and best_t1 without overflow: */
	tcenter = (best_t0/2 + best_t1/2);
	if (best_t0 % 2 + best_t1 % 2 == 2)
		++tcenter;
	return tcenter - best_tm;
}

/*
 * Synchronize ar.itc of the current (slave) CPU with the ar.itc of the MASTER CPU
 * (normally the time-keeper CPU).  We use a closed loop to eliminate the possibility of
 * unaccounted-for errors (such as getting a machine check in the middle of a calibration
 * step).  The basic idea is for the slave to ask the master what itc value it has and to
 * read its own itc before and after the master responds.  Each iteration gives us three
 * timestamps:
 *
 *	slave		master
 *
 *	t0 ---\
 *             ---\
 *		   --->
 *			tm
 *		   /---
 *	       /---
 *	t1 <---
 *
 *
 * The goal is to adjust the slave's ar.itc such that tm falls exactly half-way between t0
 * and t1.  If we achieve this, the clocks are synchronized provided the interconnect
 * between the slave and the master is symmetric.  Even if the interconnect were
 * asymmetric, we would still know that the synchronization error is smaller than the
 * roundtrip latency (t0 - t1).
 *
 * When the interconnect is quiet and symmetric, this lets us synchronize the itc to
 * within one or two cycles.  However, we can only *guarantee* that the synchronization is
 * accurate to within a round-trip time, which is typically in the range of several
 * hundred cycles (e.g., ~500 cycles).  In practice, this means that the itc's are usually
 * almost perfectly synchronized, but we shouldn't assume that the accuracy is much better
 * than half a micro second or so.
 */
void
ia64_sync_itc (unsigned int master)
{
	long i, delta, adj, adjust_latency = 0, done = 0;
	unsigned long flags, rt, master_time_stamp, bound;
#if DEBUG_ITC_SYNC
	struct {
		long rt;	/* roundtrip time */
		long master;	/* master's timestamp */
		long diff;	/* difference between midpoint and master's timestamp */
		long lat;	/* estimate of itc adjustment latency */
	} t[NUM_ROUNDS];
#endif

	go[MASTER] = 1;

	if (smp_call_function_single(master, sync_master, NULL, 1, 0) < 0) {
		printk(KERN_ERR "sync_itc: failed to get attention of CPU %u!\n", master);
		return;
	}

	while (go[MASTER]);	/* wait for master to be ready */

	spin_lock_irqsave(&itc_sync_lock, flags);
	{
		for (i = 0; i < NUM_ROUNDS; ++i) {
			delta = get_delta(&rt, &master_time_stamp);
			if (delta == 0) {
				done = 1;	/* let's lock on to this... */
				bound = rt;
			}

			if (!done) {
				if (i > 0) {
					adjust_latency += -delta;
					adj = -delta + adjust_latency/4;
				} else
					adj = -delta;

				ia64_set_itc(ia64_get_itc() + adj);
			}
#if DEBUG_ITC_SYNC
			t[i].rt = rt;
			t[i].master = master_time_stamp;
			t[i].diff = delta;
			t[i].lat = adjust_latency/4;
#endif
		}
	}
	spin_unlock_irqrestore(&itc_sync_lock, flags);

#if DEBUG_ITC_SYNC
	for (i = 0; i < NUM_ROUNDS; ++i)
		printk("rt=%5ld master=%5ld diff=%5ld adjlat=%5ld\n",
		       t[i].rt, t[i].master, t[i].diff, t[i].lat);
#endif

	printk(KERN_INFO "CPU %d: synchronized ITC with CPU %u (last diff %ld cycles, "
	       "maxerr %lu cycles)\n", smp_processor_id(), master, delta, rt);
}

/*
 * Ideally sets up per-cpu profiling hooks.  Doesn't do much now...
 */
static inline void __init
smp_setup_percpu_timer (void)
{
	local_cpu_data->prof_counter = 1;
	local_cpu_data->prof_multiplier = 1;
}

/*
 * Architecture specific routine called by the kernel just before init is
 * fired off. This allows the BP to have everything in order [we hope].
 * At the end of this all the APs will hit the system scheduling and off
 * we go. Each AP will jump through the kernel
 * init into idle(). At this point the scheduler will one day take over
 * and give them jobs to do. smp_callin is a standard routine
 * we use to track CPUs as they power up.
 */

static volatile atomic_t smp_commenced = ATOMIC_INIT(0);

void __init
smp_commence (void)
{
	/*
	 * Lets the callins below out of their loop.
	 */
	Dprintk("Setting commenced=1, go go go\n");

	wmb();
	atomic_set(&smp_commenced,1);
}


static void __init
smp_callin (void)
{
	int cpuid, phys_id;
	extern void ia64_init_itm(void);
	extern void ia64_cpu_local_tick(void);

#ifdef CONFIG_PERFMON
	extern void pfm_init_percpu(void);
#endif

	cpuid = smp_processor_id();
	phys_id = hard_smp_processor_id();

	if (test_and_set_bit(cpuid, &cpu_online_map)) {
		printk(KERN_ERR "huh, phys CPU#0x%x, CPU#0x%x already present??\n",
		       phys_id, cpuid);
		BUG();
	}

	smp_setup_percpu_timer();

	/*
	 * Get our bogomips.
	 */
	ia64_init_itm();

	/*
	 * Set I/O port base per CPU
	 */
	ia64_set_kr(IA64_KR_IO_BASE, __pa(ia64_iobase));

#ifdef CONFIG_IA64_MCA
	ia64_mca_cmc_vector_setup();	/* Setup vector on AP & enable */
#endif

#ifdef CONFIG_PERFMON
	pfm_init_percpu();
#endif

	local_irq_enable();
	calibrate_delay();
	local_cpu_data->loops_per_jiffy = loops_per_jiffy;

	if (!(sal_platform_features & IA64_SAL_PLATFORM_FEATURE_ITC_DRIFT)) {
		/*
		 * Synchronize the ITC with the BP.  Need to do this after irqs are
		 * enabled because ia64_sync_itc() calls smp_call_function_single(), which
		 * calls spin_unlock_bh(), which calls spin_unlock_bh(), which calls
		 * local_bh_enable(), which bugs out if irqs are not enabled...
		 */
		Dprintk("Going to syncup ITC with BP.\n");
		ia64_sync_itc(0);

		/*
		 * Make sure we didn't sync the itc ahead of the next
		 * timer interrupt, if so, just reset it.
		 */
		if (time_after(ia64_get_itc(),local_cpu_data->itm_next)) {
			Dprintk("oops, jumped a timer.\n");
			ia64_cpu_local_tick();
		}
	}

	/*
	 * Allow the master to continue.
	 */
	set_bit(cpuid, &cpu_callin_map);
	Dprintk("Stack on CPU %d at about %p\n",cpuid, &cpuid);
}


/*
 * Activate a secondary processor.  head.S calls this.
 */
int __init
start_secondary (void *unused)
{
	extern int cpu_idle (void);

	Dprintk("start_secondary: starting CPU 0x%x\n", hard_smp_processor_id());
	efi_map_pal_code();
	cpu_init();
	smp_callin();
	Dprintk("CPU %d is set to go.\n", smp_processor_id());
	while (!atomic_read(&smp_commenced))
		;

	Dprintk("CPU %d is starting idle.\n", smp_processor_id());
	return cpu_idle();
}

static int __init
fork_by_hand (void)
{
	/*
	 * don't care about the eip and regs settings since we'll never reschedule the
	 * forked task.
	 */
	return do_fork(CLONE_VM|CLONE_PID, 0, 0, 0);
}

static void __init
do_boot_cpu (int sapicid)
{
	struct task_struct *idle;
	int timeout, cpu;

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

	task_set_cpu(idle, cpu);	/* we schedule the first task manually */

	ia64_cpu_to_sapicid[cpu] = sapicid;

	del_from_runqueue(idle);
	unhash_process(idle);
	init_tasks[cpu] = idle;

	Dprintk("Sending wakeup vector %lu to AP 0x%x/0x%x.\n", ap_wakeup_vector, cpu, sapicid);

	platform_send_ipi(cpu, ap_wakeup_vector, IA64_IPI_DM_INT, 0);

	/*
	 * Wait 10s total for the AP to start
	 */
	Dprintk("Waiting on callin_map ...");
	for (timeout = 0; timeout < 100000; timeout++) {
		if (test_bit(cpu, &cpu_callin_map))
			break;  /* It has booted */
		udelay(100);
	}
	Dprintk("\n");

	if (test_bit(cpu, &cpu_callin_map)) {
		/* number CPUs logically, starting from 1 (BSP is 0) */
		printk(KERN_INFO "CPU%d: CPU has booted.\n", cpu);
	} else {
		printk(KERN_ERR "Processor 0x%x/0x%x is stuck.\n", cpu, sapicid);
		ia64_cpu_to_sapicid[cpu] = -1;
		cpucount--;
	}
}

/*
 * Cycle through the APs sending Wakeup IPIs to boot each.
 */
void __init
smp_boot_cpus (void)
{
	int sapicid, cpu;
	int boot_cpu_id = hard_smp_processor_id();

	/*
	 * Initialize the logical to physical CPU number mapping
	 * and the per-CPU profiling counter/multiplier
	 */

	for (cpu = 0; cpu < NR_CPUS; cpu++)
		ia64_cpu_to_sapicid[cpu] = -1;
	smp_setup_percpu_timer();

	/*
	 * We have the boot CPU online for sure.
	 */
	set_bit(0, &cpu_online_map);
	set_bit(0, &cpu_callin_map);

	local_cpu_data->loops_per_jiffy = loops_per_jiffy;
	ia64_cpu_to_sapicid[0] = boot_cpu_id;

	printk(KERN_INFO "Boot processor id 0x%x/0x%x\n", 0, boot_cpu_id);

	global_irq_holder = 0;
	current->processor = 0;
	init_idle();

	/*
	 * If SMP should be disabled, then really disable it!
	 */
	if (!max_cpus || (max_cpus < -1)) {
		printk(KERN_INFO "SMP mode deactivated.\n");
		cpu_online_map =  1;
		smp_num_cpus = 1;
		goto smp_done;
	}
	if  (max_cpus != -1)
		printk(KERN_INFO "Limiting CPUs to %d\n", max_cpus);

	if (smp_boot_data.cpu_count > 1) {

		printk(KERN_INFO "SMP: starting up secondaries.\n");

		for (cpu = 0; cpu < smp_boot_data.cpu_count; cpu++) {
			/*
			 * Don't even attempt to start the boot CPU!
			 */
			sapicid = smp_boot_data.cpu_phys_id[cpu];
			if ((sapicid == -1) || (sapicid == hard_smp_processor_id()))
				continue;

			if ((max_cpus > 0) && (cpucount + 1 >= max_cpus))
				break;

			do_boot_cpu(sapicid);
		}

		smp_num_cpus = cpucount + 1;

		/*
		 * Allow the user to impress friends.
		 */

		printk("Before bogomips.\n");
		if (!cpucount) {
			printk(KERN_WARNING "Warning: only one processor found.\n");
		} else {
			unsigned long bogosum = 0;
  			for (cpu = 0; cpu < NR_CPUS; cpu++)
				if (cpu_online_map & (1UL << cpu))
					bogosum += cpu_data(cpu)->loops_per_jiffy;

			printk(KERN_INFO "Total of %d processors activated (%lu.%02lu BogoMIPS).\n",
			       cpucount + 1, bogosum/(500000/HZ), (bogosum/(5000/HZ))%100);
		}
	}
  smp_done:
	;
}

/*
 * Assume that CPU's have been discovered by some platform-dependent interface.  For
 * SoftSDV/Lion, that would be ACPI.
 *
 * Setup of the IPI irq handler is done in irq.c:init_IRQ_SMP().
 */
void __init
init_smp_config(void)
{
	struct fptr {
		unsigned long fp;
		unsigned long gp;
	} *ap_startup;
	long sal_ret;

	/* Tell SAL where to drop the AP's.  */
	ap_startup = (struct fptr *) start_ap;
	sal_ret = ia64_sal_set_vectors(SAL_VECTOR_OS_BOOT_RENDEZ,
				      ia64_tpa(ap_startup->fp), ia64_tpa(ap_startup->gp), 0, 0, 0, 0);
	if (sal_ret < 0) {
		printk(KERN_ERR "SMP: Can't set SAL AP Boot Rendezvous: %s\n     Forcing UP mode\n",
		       ia64_sal_strerror(sal_ret));
		max_cpus = 0;
		smp_num_cpus = 1;
	}
}

/*
 * Initialize the logical CPU number to SAPICID mapping
 */
void __init
smp_build_cpu_map (void)
{
	int sapicid, cpu, i;
	int boot_cpu_id = hard_smp_processor_id();

	for (cpu = 0; cpu < NR_CPUS; cpu++)
		ia64_cpu_to_sapicid[cpu] = -1;

	ia64_cpu_to_sapicid[0] = boot_cpu_id;

	for (cpu = 1, i = 0; i < smp_boot_data.cpu_count; i++) {
		sapicid = smp_boot_data.cpu_phys_id[i];
		if (sapicid == boot_cpu_id)
			continue;
		ia64_cpu_to_sapicid[cpu] = sapicid;
		cpu++;
	}
}

