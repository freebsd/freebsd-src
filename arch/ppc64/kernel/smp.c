/*
 * 
 *
 * SMP support for ppc.
 *
 * Written by Cort Dougan (cort@cs.nmt.edu) borrowing a great
 * deal of code from the sparc and intel versions.
 *
 * Copyright (C) 1999 Cort Dougan <cort@cs.nmt.edu>
 *
 * PowerPC-64 Support added by Dave Engebretsen, Peter Bergner, and
 * Mike Corrigan {engebret|bergner|mikec}@us.ibm.com
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
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
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/cache.h>

#include <asm/ptrace.h>
#include <asm/atomic.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/hardirq.h>
#include <asm/softirq.h>
#include <asm/init.h>
#include <asm/io.h>
#include <asm/prom.h>
#include <asm/smp.h>
#include <asm/naca.h>
#include <asm/paca.h>
#include <asm/iSeries/LparData.h>
#include <asm/iSeries/HvCall.h>
#include <asm/iSeries/HvCallCfg.h>
#include <asm/time.h>
#include <asm/ppcdebug.h>
#include "open_pic.h"
#include <asm/machdep.h>
#include <asm/cputable.h>
#if defined(CONFIG_DUMP) || defined(CONFIG_DUMP_MODULE)
int (*dump_ipi_function_ptr)(struct pt_regs *);
#include <asm/dump.h>
#endif

#ifdef CONFIG_KDB
#include <linux/kdb.h>
#endif

int smp_threads_ready = 0;
volatile int smp_commenced = 0;
int smp_num_cpus = 1;
int smp_tb_synchronized = 0;
extern atomic_t ipi_recv;
extern atomic_t ipi_sent;
spinlock_t kernel_flag __cacheline_aligned = SPIN_LOCK_UNLOCKED;
cycles_t cacheflush_time;
static int max_cpus __initdata = NR_CPUS;

unsigned long cpu_online_map;

volatile unsigned long cpu_callin_map[NR_CPUS] = {0,};

#define TB_SYNC_PASSES 4
volatile unsigned long __initdata tb_sync_flag = 0;
volatile unsigned long __initdata tb_offset = 0;

extern unsigned char stab_array[];

int start_secondary(void *);
extern int cpu_idle(void *unused);
void smp_call_function_interrupt(void);
void smp_message_pass(int target, int msg, unsigned long data, int wait);
static unsigned long iSeries_smp_message[NR_CPUS];

void xics_setup_cpu(void);
void xics_cause_IPI(int cpu);

long h_register_vpa(unsigned long flags, unsigned long proc,
		    unsigned long vpa);

/*
 * XICS only has a single IPI, so encode the messages per CPU
 */
volatile unsigned long xics_ipi_message[NR_CPUS] = {0};

#define smp_message_pass(t,m,d,w) \
    do {     atomic_inc(&ipi_sent); \
	     ppc_md.smp_message_pass((t),(m),(d),(w)); \
       } while(0)

#ifdef CONFIG_KDB
/* save regs here before calling kdb_ipi */
struct pt_regs *kdb_smp_regs[NR_CPUS]; 

/* called for each processor.. drop each into kdb. */
void smp_kdb_stop_proc(void)
{
    kdb_ipi(kdb_smp_regs[smp_processor_id()], NULL);
}

void smp_kdb_stop(void)
{
  int ret=0;
  ret =    smp_call_function(smp_kdb_stop_proc, NULL, 1, 0);
}

#endif

static inline void set_tb(unsigned int upper, unsigned int lower)
{
	mtspr(SPRN_TBWL, 0);
	mtspr(SPRN_TBWU, upper);
	mtspr(SPRN_TBWL, lower);
}

void iSeries_smp_message_recv( struct pt_regs * regs )
{
	int cpu = smp_processor_id();
	int msg;

	if ( smp_num_cpus < 2 )
		return;

	for ( msg = 0; msg < 4; ++msg )
		if ( test_and_clear_bit( msg, &iSeries_smp_message[cpu] ) )
			smp_message_recv( msg, regs );

}

static void smp_iSeries_message_pass(int target, int msg, unsigned long data, int wait)
{
	int i;
	for (i = 0; i < smp_num_cpus; ++i) {
		if ( (target == MSG_ALL) || 
                    (target == i) || 
                    ((target == MSG_ALL_BUT_SELF) && (i != smp_processor_id())) ) {
			set_bit( msg, &iSeries_smp_message[i] );
			HvCall_sendIPI(&(paca[i]));
		}
	}
}

static int smp_iSeries_numProcs(void)
{
	unsigned np, i;
	struct ItLpPaca * lpPaca;

	np = 0;
        for (i=0; i < MAX_PACAS; ++i) {
                lpPaca = paca[i].xLpPacaPtr;
                if ( lpPaca->xDynProcStatus < 2 ) {
                        ++np;
                }
        }
	return np;
}

static int smp_iSeries_probe(void)
{
	unsigned i;
	unsigned np;
	struct ItLpPaca * lpPaca;

	np = 0;
	for (i=0; i < MAX_PACAS; ++i) {
		lpPaca = paca[i].xLpPacaPtr;
		if ( lpPaca->xDynProcStatus < 2 ) {
			++np;
			paca[i].next_jiffy_update_tb = paca[0].next_jiffy_update_tb;
		}
	}

	smp_tb_synchronized = 1;
	return np;
}

static void smp_iSeries_kick_cpu(int nr)
{
	struct ItLpPaca * lpPaca;
	/* Verify we have a Paca for processor nr */
	if ( ( nr <= 0 ) ||
	     ( nr >= MAX_PACAS ) )
		return;
	/* Verify that our partition has a processor nr */
	lpPaca = paca[nr].xLpPacaPtr;
	if ( lpPaca->xDynProcStatus >= 2 )
		return;

	/* The information for processor bringup must
	 * be written out to main store before we release
	 * the processor.
	 */
	mb();

	/* The processor is currently spinning, waiting
	 * for the xProcStart field to become non-zero
	 * After we set xProcStart, the processor will
	 * continue on to secondary_start in iSeries_head.S
	 */
	paca[nr].xProcStart = 1;
}

static void smp_iSeries_setup_cpu(int nr)
{
}

/* This is called very early. */
void smp_init_iSeries(void)
{
	ppc_md.smp_message_pass = smp_iSeries_message_pass;
	ppc_md.smp_probe        = smp_iSeries_probe;
	ppc_md.smp_kick_cpu     = smp_iSeries_kick_cpu;
	ppc_md.smp_setup_cpu    = smp_iSeries_setup_cpu;

	systemcfg->processorCount	= smp_iSeries_numProcs();
}


static void
smp_openpic_message_pass(int target, int msg, unsigned long data, int wait)
{
	/* make sure we're sending something that translates to an IPI */
	if ( msg > 0x3 ){
		printk("SMP %d: smp_message_pass: unknown msg %d\n",
		       smp_processor_id(), msg);
		return;
	}
	switch ( target )
	{
	case MSG_ALL:
		openpic_cause_IPI(msg, 0xffffffff);
		break;
	case MSG_ALL_BUT_SELF:
		openpic_cause_IPI(msg,
				  0xffffffff & ~(1 << smp_processor_id()));
		break;
	default:
		openpic_cause_IPI(msg, 1<<target);
		break;
	}
}

static int
smp_chrp_probe(void)
{
	if (systemcfg->processorCount > 1)
		openpic_request_IPIs();

	return systemcfg->processorCount;
}

static void
smp_kick_cpu(int nr)
{
	/* Verify we have a Paca for processor nr */
	if ( ( nr <= 0 ) ||
	     ( nr >= MAX_PACAS ) )
		return;

	/* The information for processor bringup must
	 * be written out to main store before we release
	 * the processor.
	 */
	mb();

	/* The processor is currently spinning, waiting
	 * for the xProcStart field to become non-zero
	 * After we set xProcStart, the processor will
	 * continue on to secondary_start in iSeries_head.S
	 */
	paca[nr].xProcStart = 1;
}

static void smp_space_timers( unsigned nr )
{
	unsigned long offset, i;
	
	offset = tb_ticks_per_jiffy / nr;
	for ( i=1; i<nr; ++i ) {
		paca[i].next_jiffy_update_tb = paca[i-1].next_jiffy_update_tb + offset;
	}
}

static void
smp_chrp_setup_cpu(int cpu_nr)
{
	static atomic_t ready = ATOMIC_INIT(1);
	static volatile int frozen = 0;

	if (systemcfg->platform == PLATFORM_PSERIES_LPAR) {
		/* timebases already synced under the hypervisor. */
		paca[cpu_nr].next_jiffy_update_tb = tb_last_stamp = get_tb();
		if (cpu_nr == 0) {
			systemcfg->tb_orig_stamp = tb_last_stamp;
			/* Should update naca->stamp_xsec.
			 * For now we leave it which means the time can be some
			 * number of msecs off until someone does a settimeofday()
			 */
		}
		smp_tb_synchronized = 1;
	} else {
		if (cpu_nr == 0) {
			/* wait for all the others */
			while (atomic_read(&ready) < smp_num_cpus)
				barrier();
			atomic_set(&ready, 1);
			/* freeze the timebase */
			rtas_call(rtas_token("freeze-time-base"), 0, 1, NULL);
			mb();
			frozen = 1;
			set_tb(0, 0);
			paca[0].next_jiffy_update_tb = 0;
			smp_space_timers(smp_num_cpus);
			while (atomic_read(&ready) < smp_num_cpus)
				barrier();
			/* thaw the timebase again */
			rtas_call(rtas_token("thaw-time-base"), 0, 1, NULL);
			mb();
			frozen = 0;
			tb_last_stamp = get_tb();
			systemcfg->tb_orig_stamp = tb_last_stamp;
			smp_tb_synchronized = 1;
		} else {
			atomic_inc(&ready);
			while (!frozen)
				barrier();
			set_tb(0, 0);
			mb();
			atomic_inc(&ready);
			while (frozen)
				barrier();
		}
	}

	if (OpenPIC_Addr) {
		do_openpic_setup_cpu();
	} else {
	  if (cpu_nr > 0)
	    xics_setup_cpu();
	}
}

static void
smp_xics_message_pass(int target, int msg, unsigned long data, int wait)
{
	int i;

	for (i = 0; i < smp_num_cpus; ++i) {
		if (target == MSG_ALL || target == i
		    || (target == MSG_ALL_BUT_SELF
			&& i != smp_processor_id())) {
			set_bit(msg, &xics_ipi_message[i]);
			mb();
			xics_cause_IPI(i);
		}
	}
}

static int
smp_xics_probe(void)
{
	return systemcfg->processorCount;
}

/* This is called very early */
void smp_init_pSeries(void)
{
	if(naca->interrupt_controller == IC_OPEN_PIC) {
		ppc_md.smp_message_pass = smp_openpic_message_pass;
		ppc_md.smp_probe        = smp_chrp_probe;
		ppc_md.smp_kick_cpu     = smp_kick_cpu;
		ppc_md.smp_setup_cpu    = smp_chrp_setup_cpu;
	} else {
		ppc_md.smp_message_pass = smp_xics_message_pass;
		ppc_md.smp_probe        = smp_xics_probe;
		ppc_md.smp_kick_cpu     = smp_kick_cpu;
		ppc_md.smp_setup_cpu    = smp_chrp_setup_cpu;
	}
}


void smp_local_timer_interrupt(struct pt_regs * regs)
{
	if (!--(get_paca()->prof_counter)) {
		update_process_times(user_mode(regs));
		(get_paca()->prof_counter)=get_paca()->prof_multiplier;
	}
}

void smp_message_recv(int msg, struct pt_regs *regs)
{
	atomic_inc(&ipi_recv);
	
	switch( msg ) {
	case PPC_MSG_CALL_FUNCTION:
#ifdef CONFIG_KDB
		kdb_smp_regs[smp_processor_id()]=regs;
#endif
		smp_call_function_interrupt();
		break;
	case PPC_MSG_RESCHEDULE: 
		current->need_resched = 1;
		break;
#ifdef CONFIG_XMON
	case PPC_MSG_XMON_BREAK:
	        /* ToDo: need a nmi way to handle this.  Soft disable? */
#if defined(CONFIG_DUMP) || defined(CONFIG_DUMP_MODULE)
	        if (dump_ipi_function_ptr) {
			printk(KERN_ALERT "got dump ipi...\n");
			dump_ipi_function_ptr(regs);
		} else
#endif
			xmon(regs);
		break;
#endif /* CONFIG_XMON */
	default:
		printk("SMP %d: smp_message_recv(): unknown msg %d\n",
		       smp_processor_id(), msg);
		break;
	}
}

void smp_send_reschedule(int cpu)
{
	if ((systemcfg->platform & PLATFORM_LPAR) &&
	    (paca[cpu].yielded == 1)) {
#ifdef CONFIG_PPC_ISERIES
		HvCall_sendLpProd(cpu);
#else
		prod_processor(cpu);
#endif
	} else {
	smp_message_pass(cpu, PPC_MSG_RESCHEDULE, 0, 0);
}
}

#ifdef CONFIG_XMON
void smp_send_xmon_break(int cpu)
{
	smp_message_pass(cpu, PPC_MSG_XMON_BREAK, 0, 0);
}
#endif /* CONFIG_XMON */

#if defined(CONFIG_DUMP) || defined(CONFIG_DUMP_MODULE)
void dump_send_ipi(int (*dump_ipi_callback)(struct pt_regs *))
{
	dump_ipi_function_ptr = dump_ipi_callback;
	if (dump_ipi_callback) {
		printk(KERN_ALERT "dump_send_ipi...\n");
		mb();
		smp_message_pass(MSG_ALL_BUT_SELF, PPC_MSG_XMON_BREAK, 0, 0);
	}
}
#endif

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
static spinlock_t call_lock __cacheline_aligned_in_smp = SPIN_LOCK_UNLOCKED;

static struct call_data_struct {
	void (*func) (void *info);
	void *info;
	atomic_t started;
	atomic_t finished;
	int wait;
} *call_data;

/*
 * This function sends a 'generic call function' IPI to all other CPUs
 * in the system.
 *
 * [SUMMARY] Run a function on all other CPUs.
 * <func> The function to run. This must be fast and non-blocking.
 * <info> An arbitrary pointer to pass to the function.
 * <nonatomic> currently unused.
 * <wait> If true, wait (atomically) until function has completed on other CPUs.
 * [RETURNS] 0 on success, else a negative status code. Does not return until
 * remote CPUs are nearly ready to execute <<func>> or are or have executed.
 *
 * You must not call this function with disabled interrupts or from a
 * hardware interrupt handler or from a bottom half handler.
 */
int smp_call_function (void (*func) (void *info), void *info, int nonatomic,
			int wait)

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
	timeout = 8000000;
	while (atomic_read(&data.started) != cpus) {
		HMT_low();
		if (--timeout == 0) {
			printk("smp_call_function on cpu %d: other cpus not responding (%d)\n",
			       smp_processor_id(), atomic_read(&data.started));
#ifdef CONFIG_XMON
                        xmon(0);
#endif
#ifdef CONFIG_KDB
			kdb(KDB_REASON_CALL,0, (kdb_eframe_t) 0);
#endif

#ifdef CONFIG_PPC_ISERIES
			HvCall_terminateMachineSrc();
#endif
			goto out;
		}
		barrier();
		udelay(1);
	}

	if (wait) {
		timeout = 1000000;
		while (atomic_read(&data.finished) != cpus) {
			HMT_low();
			if (--timeout == 0) {
				printk("smp_call_function on cpu %d: other cpus not finishing (%d/%d)\n",
				       smp_processor_id(), atomic_read(&data.finished), atomic_read(&data.started));
#ifdef CONFIG_PPC_ISERIES
				HvCall_terminateMachineSrc();
#endif
				goto out;
			}
			barrier();
			udelay(1);
		}
	}
	ret = 0;

 out:
	call_data = NULL;
	HMT_medium();
	spin_unlock_bh(&call_lock);
	return ret;
}

void smp_call_function_interrupt(void)
{
	void (*func) (void *info);
	void *info;
	int wait;


	/* call_data will be NULL if the sender timed out while
	 * waiting on us to receive the call.
	 */
	if (!call_data)
		return;

	func = call_data->func;
	info = call_data->info;
	wait = call_data->wait;

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


extern unsigned long decr_overclock;

void __init smp_boot_cpus(void)
{
	extern struct current_set_struct current_set[];
	extern void __secondary_start_chrp(void);
	int i, cpu_nr;
	struct task_struct *p;
	unsigned long sp;

	printk("Entering SMP Mode...\n");
	PPCDBG(PPCDBG_SMP, "smp_boot_cpus: start.  NR_CPUS = 0x%lx\n", NR_CPUS);

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
		paca[i].prof_counter = 1;
		paca[i].prof_multiplier = 1;
		if(i != 0) {
		        /*
			 * Processor 0's segment table is statically 
			 * initialized to real address STAB0_PHYS_ADDR.  The
			 * Other processor's tables are created and
			 * initialized here.
			 */
			paca[i].xStab_data.virt = (unsigned long)&stab_array[PAGE_SIZE * (i-1)];
			memset((void *)paca[i].xStab_data.virt, 0, PAGE_SIZE); 
			paca[i].xStab_data.real = __v2a(paca[i].xStab_data.virt);
			paca[i].default_decr = tb_ticks_per_jiffy / decr_overclock;
		}
	}

	/*
	 * XXX very rough, assumes 20 bus cycles to read a cache line,
	 * timebase increments every 4 bus cycles, 32kB L1 data cache.
	 */
	cacheflush_time = 5 * 1024;

	/* Probe arch for CPUs */
	cpu_nr = ppc_md.smp_probe();

	printk("Probe found %d CPUs\n", cpu_nr);

	/*
	 * only check for cpus we know exist.  We keep the callin map
	 * with cpus at the bottom -- Cort
	 */
	if (cpu_nr > max_cpus)
		cpu_nr = max_cpus;

#ifdef CONFIG_PPC_ISERIES
	smp_space_timers( cpu_nr );
#endif

	printk("Waiting for %d CPUs\n", cpu_nr-1);

	for ( i = 1 ; i < cpu_nr; i++ ) {
		int c;
		struct pt_regs regs;
		
		/* create a process for the processor */
		/* we don't care about the values in regs since we'll
		   never reschedule the forked task. */
		/* We DO care about one bit in the pt_regs we
		   pass to do_fork.  That is the MSR_FP bit in
		   regs.msr.  If that bit is on, then do_fork
		   (via copy_thread) will call giveup_fpu.
		   giveup_fpu will get a pointer to our (current's)
		   last register savearea via current->thread.regs
		   and using that pointer will turn off the MSR_FP,
		   MSR_FE0 and MSR_FE1 bits.  At this point, this
		   pointer is pointing to some arbitrary point within
		   our stack */

		memset(&regs, 0, sizeof(struct pt_regs));

		if (do_fork(CLONE_VM|CLONE_PID, 0, &regs, 0) < 0)
			panic("failed fork for CPU %d", i);
		p = init_task.prev_task;
		if (!p)
			panic("No idle task for CPU %d", i);

		PPCDBG(PPCDBG_SMP,"\tProcessor %d, task = 0x%lx\n", i, p);

		del_from_runqueue(p);
		unhash_process(p);
		init_tasks[i] = p;

		p->processor = i;
		p->cpus_runnable = 1 << i; /* we schedule the first task manually */
		current_set[i].task = p;
		sp = ((unsigned long)p) + sizeof(union task_union)
			- STACK_FRAME_OVERHEAD;
		current_set[i].sp_real = (void *)__v2a(sp);

		/* wake up cpus */
		ppc_md.smp_kick_cpu(i);

		/*
		 * wait to see if the cpu made a callin (is actually up).
		 * use this value that I found through experimentation.
		 * -- Cort
		 */
		for ( c = 5000; c && !cpu_callin_map[i] ; c-- ) {
			udelay(100);
		}
		
		if ( cpu_callin_map[i] )
		{
			printk("Processor %d found.\n", i);
			PPCDBG(PPCDBG_SMP, "\tProcessor %d found.\n", i);
			/* this sync's the decr's -- Cort */
			smp_num_cpus++;
		} else {
			printk("Processor %d is stuck.\n", i);
			PPCDBG(PPCDBG_SMP, "\tProcessor %d is stuck.\n", i);
		}
	}

	/* Setup CPU 0 last (important) */
	ppc_md.smp_setup_cpu(0);
	
	if (smp_num_cpus < 2) {
	        tb_last_stamp = get_tb();
		smp_tb_synchronized = 1;
	}
}

void __init smp_commence(void)
{
	/*
	 *	Lets the callin's below out of their loop.
	 */
	PPCDBG(PPCDBG_SMP, "smp_commence: start\n"); 
	wmb();
	smp_commenced = 1;
}

void __init smp_callin(void)
{
	int cpu = current->processor;
	
        smp_store_cpu_info(cpu);
	set_dec(paca[cpu].default_decr);
	cpu_callin_map[cpu] = 1;

	ppc_md.smp_setup_cpu(cpu);

	init_idle();

	set_bit(smp_processor_id(), &cpu_online_map);
	
	while(!smp_commenced) {
		barrier();
	}
	__sti();
}

/* intel needs this */
void __init initialize_secondary(void)
{
}

/* Activate a secondary processor. */
int start_secondary(void *unused)
{
	int cpu; 

	cpu = current->processor;
	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;
	smp_callin();

	get_paca()->yielded = 0;

	if (cur_cpu_spec->firmware_features & FW_FEATURE_SPLPAR) {
		vpa_init(cpu);
	}

	/* Go into the idle loop. */
	return cpu_idle(NULL);
}

void __init smp_setup(char *str, int *ints)
{
}

int setup_profiling_timer(unsigned int multiplier)
{
	return 0;
}

/* this function is called for each processor
 */
void __init smp_store_cpu_info(int id)
{
        paca[id].pvr = _get_PVR();
}

static int __init maxcpus(char *str)
{
	get_option(&str, &max_cpus);
	return 1;
}

__setup("maxcpus=", maxcpus);
