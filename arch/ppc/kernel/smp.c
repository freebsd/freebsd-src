/*
 * Smp support for ppc.
 *
 * Written by Cort Dougan (cort@cs.nmt.edu) borrowing a great
 * deal of code from the sparc and intel versions.
 *
 * Copyright (C) 1999 Cort Dougan <cort@cs.nmt.edu>
 *
 */

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/delay.h>
#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/cache.h>

#include <asm/ptrace.h>
#include <asm/atomic.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/hardirq.h>
#include <asm/softirq.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/smp.h>
#include <asm/residual.h>
#include <asm/time.h>

int smp_threads_ready;
volatile int smp_commenced;
int smp_num_cpus = 1;
int smp_tb_synchronized;
struct cpuinfo_PPC cpu_data[NR_CPUS];
struct klock_info_struct klock_info = { KLOCK_CLEAR, 0 };
atomic_t ipi_recv;
atomic_t ipi_sent;
spinlock_t kernel_flag __cacheline_aligned_in_smp = SPIN_LOCK_UNLOCKED;
unsigned int prof_multiplier[NR_CPUS];
unsigned int prof_counter[NR_CPUS];
cycles_t cacheflush_time;
static int max_cpus __initdata = NR_CPUS;
unsigned long cpu_online_map;
int smp_hw_index[NR_CPUS];
static struct smp_ops_t *smp_ops;

/* all cpu mappings are 1-1 -- Cort */
volatile unsigned long cpu_callin_map[NR_CPUS];

#define TB_SYNC_PASSES 4
volatile unsigned long __initdata tb_sync_flag = 0;
volatile unsigned long __initdata tb_offset = 0;

int start_secondary(void *);
extern int cpu_idle(void *unused);
void smp_call_function_interrupt(void);

/* Low level assembly function used to backup CPU 0 state */
extern void __save_cpu_setup(void);

/* Since OpenPIC has only 4 IPIs, we use slightly different message numbers.
 *
 * Make sure this matches openpic_request_IPIs in open_pic.c, or what shows up
 * in /proc/interrupts will be wrong!!! --Troy */
#define PPC_MSG_CALL_FUNCTION	0
#define PPC_MSG_RESCHEDULE	1
#define PPC_MSG_INVALIDATE_TLB	2
#define PPC_MSG_XMON_BREAK	3

static inline void
smp_message_pass(int target, int msg, unsigned long data, int wait)
{
	if (smp_ops){
		atomic_inc(&ipi_sent);
		smp_ops->message_pass(target,msg,data,wait);
	}
}

/*
 * Common functions
 */
void smp_local_timer_interrupt(struct pt_regs * regs)
{
	int cpu = smp_processor_id();

	if (!--prof_counter[cpu]) {
		update_process_times(user_mode(regs));
		prof_counter[cpu]=prof_multiplier[cpu];
	}
}

void smp_message_recv(int msg, struct pt_regs *regs)
{
	atomic_inc(&ipi_recv);

	switch( msg ) {
	case PPC_MSG_CALL_FUNCTION:
		smp_call_function_interrupt();
		break;
	case PPC_MSG_RESCHEDULE:
		current->need_resched = 1;
		break;
	case PPC_MSG_INVALIDATE_TLB:
		_tlbia();
		break;
#ifdef CONFIG_XMON
	case PPC_MSG_XMON_BREAK:
		xmon(regs);
		break;
#endif /* CONFIG_XMON */
	default:
		printk("SMP %d: smp_message_recv(): unknown msg %d\n",
		       smp_processor_id(), msg);
		break;
	}
}

#ifdef CONFIG_750_SMP
/*
 * 750's don't broadcast tlb invalidates so
 * we have to emulate that behavior.
 *   -- Cort
 */
void smp_ppc750_send_tlb_invalidate(int cpu)
{
	if ( PVR_VER(mfspr(PVR)) == 8 )
		smp_message_pass(MSG_ALL_BUT_SELF, PPC_MSG_INVALIDATE_TLB, 0, 0);
}
#endif

void smp_send_reschedule(int cpu)
{
	/*
	 * This is only used if `cpu' is running an idle task,
	 * so it will reschedule itself anyway...
	 *
	 * This isn't the case anymore since the other CPU could be
	 * sleeping and won't reschedule until the next interrupt (such
	 * as the timer).
	 *  -- Cort
	 */
	/* This is only used if `cpu' is running an idle task,
	   so it will reschedule itself anyway... */
	smp_message_pass(cpu, PPC_MSG_RESCHEDULE, 0, 0);
}

#ifdef CONFIG_XMON
void smp_send_xmon_break(int cpu)
{
	smp_message_pass(cpu, PPC_MSG_XMON_BREAK, 0, 0);
}
#endif /* CONFIG_XMON */

static void stop_this_cpu(void *dummy)
{
	__cli();
	while (1)
		;
}

void smp_send_stop(void)
{
	smp_call_function(stop_this_cpu, NULL, 1, 0);
	smp_num_cpus = 1;
}

/*
 * Structure and data for smp_call_function(). This is designed to minimise
 * static memory requirements. It also looks cleaner.
 * Stolen from the i386 version.
 */
static spinlock_t call_lock = SPIN_LOCK_UNLOCKED;

static struct call_data_struct {
	void (*func) (void *info);
	void *info;
	atomic_t started;
	atomic_t finished;
	int wait;
} *call_data;

/*
 * this function sends a 'generic call function' IPI to all other CPUs
 * in the system.
 */

int smp_call_function (void (*func) (void *info), void *info, int nonatomic,
			int wait)
/*
 * [SUMMARY] Run a function on all other CPUs.
 * <func> The function to run. This must be fast and non-blocking.
 * <info> An arbitrary pointer to pass to the function.
 * <nonatomic> currently unused.
 * <wait> If true, wait (atomically) until function has completed on other CPUs.
 * [RETURNS] 0 on success, else a negative status code. Does not return until
 * remote CPUs are nearly ready to execute <<func>> or are or have executed.
 *
 * You must not call this function with disabled interrupts or from a
 * hardware interrupt handler, you may call it from a bottom half handler.
 */
{
	struct call_data_struct data;
	int ret = -1, cpus = smp_num_cpus-1;
	int timeout;

	if (!cpus)
		return 0;

	data.func = func;
	data.info = info;
	atomic_set(&data.started, 0);
	data.wait = wait;
	if (wait)
		atomic_set(&data.finished, 0);

	spin_lock_bh(&call_lock);
	call_data = &data;
	/* Send a message to all other CPUs and wait for them to respond */
	smp_message_pass(MSG_ALL_BUT_SELF, PPC_MSG_CALL_FUNCTION, 0, 0);

	/* Wait for response */
	timeout = 1000000;
	while (atomic_read(&data.started) != cpus) {
		if (--timeout == 0) {
			printk("smp_call_function on cpu %d: other cpus not responding (%d)\n",
			       smp_processor_id(), atomic_read(&data.started));
			goto out;
		}
		barrier();
		udelay(1);
	}

	if (wait) {
		timeout = 1000000;
		while (atomic_read(&data.finished) != cpus) {
			if (--timeout == 0) {
				printk("smp_call_function on cpu %d: other cpus not finishing (%d/%d)\n",
				       smp_processor_id(), atomic_read(&data.finished), atomic_read(&data.started));
				goto out;
			}
			barrier();
			udelay(1);
		}
	}
	ret = 0;

 out:
	spin_unlock_bh(&call_lock);
	return ret;
}

void smp_call_function_interrupt(void)
{
	void (*func) (void *info) = call_data->func;
	void *info = call_data->info;
	int wait = call_data->wait;

	/*
	 * Notify initiating CPU that I've grabbed the data and am
	 * about to execute the function
	 */
	atomic_inc(&call_data->started);
	/*
	 * At this point the info structure may be out of scope unless wait==1
	 */
	(*func)(info);
	if (wait)
		atomic_inc(&call_data->finished);
}

void __init smp_boot_cpus(void)
{
	extern struct task_struct *current_set[NR_CPUS];
	int i, cpu_nr;
	struct task_struct *p;

	printk("Entering SMP Mode...\n");
	smp_num_cpus = 1;
        smp_store_cpu_info(0);
	cpu_online_map = 1UL;

	/*
	 * assume for now that the first cpu booted is
	 * cpu 0, the master -- Cort
	 */
	cpu_callin_map[0] = 1;
	current->processor = 0;

	init_idle();

	for (i = 0; i < NR_CPUS; i++) {
		prof_counter[i] = 1;
		prof_multiplier[i] = 1;
	}

	/*
	 * XXX very rough, assumes 20 bus cycles to read a cache line,
	 * timebase increments every 4 bus cycles, 32kB L1 data cache.
	 */
	cacheflush_time = 5 * 1024;

	smp_ops = ppc_md.smp_ops;
	if (smp_ops == NULL) {
		printk("SMP not supported on this machine.\n");
		return;
	}

#ifndef CONFIG_750_SMP
	/* check for 750's, they just don't work with linux SMP.
	 * If you actually have 750 SMP hardware and want to try to get
	 * it to work, send me a patch to make it work and
	 * I'll make CONFIG_750_SMP a config option.  -- Troy (hozer@drgw.net)
	 */
	if ( PVR_VER(mfspr(PVR)) == 8 ){
		printk("SMP not supported on 750 cpus. %s line %d\n",
				__FILE__, __LINE__);
		return;
	}
#endif


	/* Probe arch for CPUs */
	cpu_nr = smp_ops->probe();

	/* Backup CPU 0 state */
	__save_cpu_setup();

	/*
	 * only check for cpus we know exist.  We keep the callin map
	 * with cpus at the bottom -- Cort
	 */
	if (cpu_nr > max_cpus)
		cpu_nr = max_cpus;
	for (i = 1; i < cpu_nr; i++) {
		int c;
		struct pt_regs regs;

		/* create a process for the processor */
		/* only regs.msr is actually used, and 0 is OK for it */
		memset(&regs, 0, sizeof(struct pt_regs));
		if (do_fork(CLONE_VM|CLONE_PID, 0, &regs, 0) < 0)
			panic("failed fork for CPU %d", i);
		p = init_task.prev_task;
		if (!p)
			panic("No idle task for CPU %d", i);
		del_from_runqueue(p);
		unhash_process(p);
		init_tasks[i] = p;

		p->processor = i;
		p->cpus_runnable = 1 << i; /* we schedule the first task manually */
		current_set[i] = p;

		/*
		 * There was a cache flush loop here to flush the cache
		 * to memory for the first 8MB of RAM.  The cache flush
		 * has been pushed into the kick_cpu function for those
		 * platforms that need it.
		 */

		/* wake up cpus */
		smp_ops->kick_cpu(i);

		/*
		 * wait to see if the cpu made a callin (is actually up).
		 * use this value that I found through experimentation.
		 * -- Cort
		 */
		for ( c = 10000; c && !cpu_callin_map[i] ; c-- )
			udelay(100);

		if ( cpu_callin_map[i] )
		{
			char buf[32];
			sprintf(buf, "found cpu %d", i);
			if (ppc_md.progress) ppc_md.progress(buf, 0x350+i);
			printk("Processor %d found.\n", i);
			smp_num_cpus++;
		} else {
			char buf[32];
			sprintf(buf, "didn't find cpu %d", i);
			if (ppc_md.progress) ppc_md.progress(buf, 0x360+i);
			printk("Processor %d is stuck.\n", i);
		}
	}

	/* Setup CPU 0 last (important) */
	smp_ops->setup_cpu(0);

	if (smp_num_cpus < 2)
		smp_tb_synchronized = 1;
}

void __init smp_software_tb_sync(int cpu)
{
#define PASSES 4	/* 4 passes.. */
	int pass;
	int i, j;

	/* stop - start will be the number of timebase ticks it takes for cpu0
	 * to send a message to all others and the first reponse to show up.
	 *
	 * ASSUMPTION: this time is similiar for all cpus
	 * ASSUMPTION: the time to send a one-way message is ping/2
	 */
	register unsigned long start = 0;
	register unsigned long stop = 0;
	register unsigned long temp = 0;

	set_tb(0, 0);

	/* multiple passes to get in l1 cache.. */
	for (pass = 2; pass < 2+PASSES; pass++){
		if (cpu == 0){
			mb();
			for (i = j = 1; i < smp_num_cpus; i++, j++){
				/* skip stuck cpus */
				while (!cpu_callin_map[j])
					++j;
				while (cpu_callin_map[j] != pass)
					barrier();
			}
			mb();
			tb_sync_flag = pass;
			start = get_tbl();	/* start timing */
			while (tb_sync_flag)
				mb();
			stop = get_tbl();	/* end timing */
			/* theoretically, the divisor should be 2, but
			 * I get better results on my dual mtx. someone
			 * please report results on other smp machines..
			 */
			tb_offset = (stop-start)/4;
			mb();
			tb_sync_flag = pass;
			udelay(10);
			mb();
			tb_sync_flag = 0;
			mb();
			set_tb(0,0);
			mb();
		} else {
			cpu_callin_map[cpu] = pass;
			mb();
			while (!tb_sync_flag)
				mb();		/* wait for cpu0 */
			mb();
			tb_sync_flag = 0;	/* send response for timing */
			mb();
			while (!tb_sync_flag)
				mb();
			temp = tb_offset;	/* make sure offset is loaded */
			while (tb_sync_flag)
				mb();
			set_tb(0,temp);		/* now, set the timebase */
			mb();
		}
	}
	if (cpu == 0) {
		smp_tb_synchronized = 1;
		printk("smp_software_tb_sync: %d passes, final offset: %ld\n",
			PASSES, tb_offset);
	}
	/* so time.c doesn't get confused */
	set_dec(tb_ticks_per_jiffy);
	last_jiffy_stamp(cpu) = 0;
}

void __init smp_commence(void)
{
	/*
	 *	Lets the callin's below out of their loop.
	 */
	if (ppc_md.progress) ppc_md.progress("smp_commence", 0x370);
	wmb();
	smp_commenced = 1;

	/* if the smp_ops->setup_cpu function has not already synched the
	 * timebases with a nicer hardware-based method, do so now
	 *
	 * I am open to suggestions for improvements to this method
	 * -- Troy <hozer@drgw.net>
	 *
	 * NOTE: if you are debugging, set smp_tb_synchronized for now
	 * since if this code runs pretty early and needs all cpus that
	 * reported in in smp_callin_map to be working
	 *
	 * NOTE2: this code doesn't seem to work on > 2 cpus. -- paulus/BenH
	 */
	if (!smp_tb_synchronized && smp_num_cpus == 2) {
		unsigned long flags;
		__save_and_cli(flags);
		smp_software_tb_sync(0);
		__restore_flags(flags);
	}
}

void __init smp_callin(void)
{
	int cpu = current->processor;

        smp_store_cpu_info(cpu);
	smp_ops->setup_cpu(cpu);
	set_dec(tb_ticks_per_jiffy);
	cpu_online_map |= 1UL << cpu;
	mb();
	cpu_callin_map[cpu] = 1;

	while(!smp_commenced)
		barrier();

	/* see smp_commence for more info */
	if (!smp_tb_synchronized && smp_num_cpus == 2) {
		smp_software_tb_sync(cpu);
	}
	__sti();
}

/* intel needs this */
void __init initialize_secondary(void)
{
}

/* Activate a secondary processor. */
int __init start_secondary(void *unused)
{
	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;
	smp_callin();
	return cpu_idle(NULL);
}

void __init smp_setup(char *str, int *ints)
{
}

int __init setup_profiling_timer(unsigned int multiplier)
{
	return 0;
}

void __init smp_store_cpu_info(int id)
{
        struct cpuinfo_PPC *c = &cpu_data[id];

	/* assume bogomips are same for everything */
        c->loops_per_jiffy = loops_per_jiffy;
        c->pvr = mfspr(PVR);
}

static int __init maxcpus(char *str)
{
	get_option(&str, &max_cpus);
	return 1;
}

__setup("maxcpus=", maxcpus);
