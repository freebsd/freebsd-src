/* sun4d_smp.c: Sparc SS1000/SC2000 SMP support.
 *
 * Copyright (C) 1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 *
 * Based on sun4m's smp.c, which is:
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 */

#include <asm/head.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/threads.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/interrupt.h>
#include <linux/kernel_stat.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/mm.h>

#include <asm/ptrace.h>
#include <asm/atomic.h>

#include <asm/delay.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/oplib.h>
#include <asm/hardirq.h>
#include <asm/softirq.h>
#include <asm/sbus.h>
#include <asm/sbi.h>

#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>

#define IRQ_CROSS_CALL		15

extern ctxd_t *srmmu_ctx_table_phys;
extern int linux_num_cpus;

extern void calibrate_delay(void);

extern struct task_struct *current_set[NR_CPUS];
extern volatile int smp_processors_ready;
extern unsigned long cpu_present_map;
extern int smp_num_cpus;
static int smp_highest_cpu;
extern int smp_threads_ready;
extern unsigned char mid_xlate[NR_CPUS];
extern volatile unsigned long cpu_callin_map[NR_CPUS];
extern unsigned long smp_proc_in_lock[NR_CPUS];
extern struct cpuinfo_sparc cpu_data[NR_CPUS];
extern unsigned long cpu_offset[NR_CPUS];
extern unsigned char boot_cpu_id;
extern int smp_activated;
extern volatile int __cpu_number_map[NR_CPUS];
extern volatile int __cpu_logical_map[NR_CPUS];
extern volatile unsigned long ipi_count;
extern volatile int smp_process_available;
extern volatile int smp_commenced;
extern int __smp4d_processor_id(void);

extern unsigned long totalram_pages;

/* #define SMP_DEBUG */

#ifdef SMP_DEBUG
#define SMP_PRINTK(x)	printk x
#else
#define SMP_PRINTK(x)
#endif

static inline unsigned long swap(volatile unsigned long *ptr, unsigned long val)
{
	__asm__ __volatile__("swap [%1], %0\n\t" :
			     "=&r" (val), "=&r" (ptr) :
			     "0" (val), "1" (ptr));
	return val;
}

static void smp_setup_percpu_timer(void);
extern void cpu_probe(void);
extern void sun4d_distribute_irqs(void);

void __init smp4d_callin(void)
{
	int cpuid = hard_smp4d_processor_id();
	extern spinlock_t sun4d_imsk_lock;
	unsigned long flags;
	
	/* Show we are alive */
	cpu_leds[cpuid] = 0x6;
	show_leds(cpuid);

	/* Enable level15 interrupt, disable level14 interrupt for now */
	cc_set_imsk((cc_get_imsk() & ~0x8000) | 0x4000);

	local_flush_cache_all();
	local_flush_tlb_all();

	/*
	 * Unblock the master CPU _only_ when the scheduler state
	 * of all secondary CPUs will be up-to-date, so after
	 * the SMP initialization the master will be just allowed
	 * to call the scheduler code.
	 */
	init_idle();

	/* Get our local ticker going. */
	smp_setup_percpu_timer();

	calibrate_delay();
	smp_store_cpu_info(cpuid);
	local_flush_cache_all();
	local_flush_tlb_all();

	/* Allow master to continue. */
	swap((unsigned long *)&cpu_callin_map[cpuid], 1);
	local_flush_cache_all();
	local_flush_tlb_all();
	
	cpu_probe();

	while((unsigned long)current_set[cpuid] < PAGE_OFFSET)
		barrier();
		
	while(current_set[cpuid]->processor != cpuid)
		barrier();
		
	/* Fix idle thread fields. */
	__asm__ __volatile__("ld [%0], %%g6\n\t"
			     "sta %%g6, [%%g0] %1\n\t"
			     : : "r" (&current_set[cpuid]), "i" (ASI_M_VIKING_TMP2)
			     : "memory" /* paranoid */);

	cpu_leds[cpuid] = 0x9;
	show_leds(cpuid);
	
	/* Attach to the address space of init_task. */
	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;

	local_flush_cache_all();
	local_flush_tlb_all();
	
	__sti();	/* We don't allow PIL 14 yet */
	
	while(!smp_commenced)
		barrier();

	spin_lock_irqsave(&sun4d_imsk_lock, flags);
	cc_set_imsk(cc_get_imsk() & ~0x4000); /* Allow PIL 14 as well */
	spin_unlock_irqrestore(&sun4d_imsk_lock, flags);
}

extern int cpu_idle(void *unused);
extern void init_IRQ(void);
extern void cpu_panic(void);
extern int start_secondary(void *unused);

/*
 *	Cycle through the processors asking the PROM to start each one.
 */
 
extern struct prom_cpuinfo linux_cpus[NR_CPUS];
extern struct linux_prom_registers smp_penguin_ctable;
extern unsigned long trapbase_cpu1[];
extern unsigned long trapbase_cpu2[];
extern unsigned long trapbase_cpu3[];

void __init smp4d_boot_cpus(void)
{
	int cpucount = 0;
	int i = 0;

	printk("Entering SMP Mode...\n");
	
	for (i = 0; i < NR_CPUS; i++)
		cpu_offset[i] = (char *)&cpu_data[i] - (char *)&cpu_data;
		
	if (boot_cpu_id)
		current_set[0] = NULL;

	__sti();
	cpu_present_map = 0;
	for(i=0; i < linux_num_cpus; i++)
		cpu_present_map |= (1<<linux_cpus[i].mid);
	SMP_PRINTK(("cpu_present_map %08lx\n", cpu_present_map));
	for(i=0; i < NR_CPUS; i++)
		__cpu_number_map[i] = -1;
	for(i=0; i < NR_CPUS; i++)
		__cpu_logical_map[i] = -1;
	for(i=0; i < NR_CPUS; i++)
		mid_xlate[i] = i;
	__cpu_number_map[boot_cpu_id] = 0;
	__cpu_logical_map[0] = boot_cpu_id;
	current->processor = boot_cpu_id;
	smp_store_cpu_info(boot_cpu_id);
	smp_setup_percpu_timer();
	init_idle();
	local_flush_cache_all();
	if(linux_num_cpus == 1)
		return;  /* Not an MP box. */
	SMP_PRINTK(("Iterating over CPUs\n"));
	for(i = 0; i < NR_CPUS; i++) {
		if(i == boot_cpu_id)
			continue;

		if(cpu_present_map & (1 << i)) {
			extern unsigned long sun4d_cpu_startup;
			unsigned long *entry = &sun4d_cpu_startup;
			struct task_struct *p;
			int timeout;
			int no;

			/* Cook up an idler for this guy. */
			kernel_thread(start_secondary, NULL, CLONE_PID);

			cpucount++;

			p = init_task.prev_task;
			init_tasks[i] = p;

			p->processor = i;
			p->cpus_runnable = 1 << i; /* we schedule the first task manually */

			current_set[i] = p;

			del_from_runqueue(p);
			unhash_process(p);

			for (no = 0; no < linux_num_cpus; no++)
				if (linux_cpus[no].mid == i)
					break;

			/*
			 * Initialize the contexts table
			 * Since the call to prom_startcpu() trashes the structure,
			 * we need to re-initialize it for each cpu
			 */
			smp_penguin_ctable.which_io = 0;
			smp_penguin_ctable.phys_addr = (unsigned int) srmmu_ctx_table_phys;
			smp_penguin_ctable.reg_size = 0;

			/* whirrr, whirrr, whirrrrrrrrr... */
			SMP_PRINTK(("Starting CPU %d at %p task %d node %08x\n", i, entry, cpucount, linux_cpus[no].prom_node));
			local_flush_cache_all();
			prom_startcpu(linux_cpus[no].prom_node,
				      &smp_penguin_ctable, 0, (char *)entry);
				      
			SMP_PRINTK(("prom_startcpu returned :)\n"));

			/* wheee... it's going... */
			for(timeout = 0; timeout < 10000; timeout++) {
				if(cpu_callin_map[i])
					break;
				udelay(200);
			}
			
			if(cpu_callin_map[i]) {
				/* Another "Red Snapper". */
				__cpu_number_map[i] = cpucount;
				__cpu_logical_map[cpucount] = i;
			} else {
				cpucount--;
				printk("Processor %d is stuck.\n", i);
			}
		}
		if(!(cpu_callin_map[i])) {
			cpu_present_map &= ~(1 << i);
			__cpu_number_map[i] = -1;
		}
	}
	local_flush_cache_all();
	if(cpucount == 0) {
		printk("Error: only one Processor found.\n");
		cpu_present_map = (1 << hard_smp4d_processor_id());
	} else {
		unsigned long bogosum = 0;
		
		for(i = 0; i < NR_CPUS; i++) {
			if(cpu_present_map & (1 << i)) {
				bogosum += cpu_data[i].udelay_val;
				smp_highest_cpu = i;
			}
		}
		SMP_PRINTK(("Total of %d Processors activated (%lu.%02lu BogoMIPS).\n", cpucount + 1, bogosum/(500000/HZ), (bogosum/(5000/HZ))%100));
		printk("Total of %d Processors activated (%lu.%02lu BogoMIPS).\n",
		       cpucount + 1,
		       bogosum/(500000/HZ),
		       (bogosum/(5000/HZ))%100);
		smp_activated = 1;
		smp_num_cpus = cpucount + 1;
	}

	/* Free unneeded trap tables */
	ClearPageReserved(virt_to_page(trapbase_cpu1));
	set_page_count(virt_to_page(trapbase_cpu1), 1);
	free_page((unsigned long)trapbase_cpu1);
	totalram_pages++;
	num_physpages++;

	ClearPageReserved(virt_to_page(trapbase_cpu2));
	set_page_count(virt_to_page(trapbase_cpu2), 1);
	free_page((unsigned long)trapbase_cpu2);
	totalram_pages++;
	num_physpages++;

	ClearPageReserved(virt_to_page(trapbase_cpu3));
	set_page_count(virt_to_page(trapbase_cpu3), 1);
	free_page((unsigned long)trapbase_cpu3);
	totalram_pages++;
	num_physpages++;

	/* Ok, they are spinning and ready to go. */
	smp_processors_ready = 1;
	sun4d_distribute_irqs();
}

static struct smp_funcall {
	smpfunc_t func;
	unsigned long arg1;
	unsigned long arg2;
	unsigned long arg3;
	unsigned long arg4;
	unsigned long arg5;
	unsigned char processors_in[NR_CPUS];  /* Set when ipi entered. */
	unsigned char processors_out[NR_CPUS]; /* Set when ipi exited. */
} ccall_info __attribute__((aligned(8)));

static spinlock_t cross_call_lock = SPIN_LOCK_UNLOCKED;

/* Cross calls must be serialized, at least currently. */
void smp4d_cross_call(smpfunc_t func, unsigned long arg1, unsigned long arg2,
		    unsigned long arg3, unsigned long arg4, unsigned long arg5)
{
	if(smp_processors_ready) {
		register int high = smp_highest_cpu;
		unsigned long flags;

		spin_lock_irqsave(&cross_call_lock, flags);

		{
			/* If you make changes here, make sure gcc generates proper code... */
			smpfunc_t f asm("i0") = func;
			unsigned long a1 asm("i1") = arg1;
			unsigned long a2 asm("i2") = arg2;
			unsigned long a3 asm("i3") = arg3;
			unsigned long a4 asm("i4") = arg4;
			unsigned long a5 asm("i5") = arg5;

			__asm__ __volatile__(
				"std %0, [%6]\n\t"
				"std %2, [%6 + 8]\n\t"
				"std %4, [%6 + 16]\n\t" : :
				"r"(f), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
				"r" (&ccall_info.func));
		}

		/* Init receive/complete mapping, plus fire the IPI's off. */
		{
			register unsigned long mask;
			register int i;

			mask = (cpu_present_map & ~(1 << hard_smp4d_processor_id()));
			for(i = 0; i <= high; i++) {
				if(mask & (1 << i)) {
					ccall_info.processors_in[i] = 0;
					ccall_info.processors_out[i] = 0;
					sun4d_send_ipi(i, IRQ_CROSS_CALL);
				}
			}
		}

		{
			register int i;

			i = 0;
			do {
				while(!ccall_info.processors_in[i])
					barrier();
			} while(++i <= high);

			i = 0;
			do {
				while(!ccall_info.processors_out[i])
					barrier();
			} while(++i <= high);
		}

		spin_unlock_irqrestore(&cross_call_lock, flags);
	}
}

/* Running cross calls. */
void smp4d_cross_call_irq(void)
{
	int i = hard_smp4d_processor_id();

	ccall_info.processors_in[i] = 1;
	ccall_info.func(ccall_info.arg1, ccall_info.arg2, ccall_info.arg3,
			ccall_info.arg4, ccall_info.arg5);
	ccall_info.processors_out[i] = 1;
}

static int smp4d_stop_cpu_sender;

static void smp4d_stop_cpu(void)
{
	int me = hard_smp4d_processor_id();
	
	if (me != smp4d_stop_cpu_sender)
		while(1) barrier();
}

/* Cross calls, in order to work efficiently and atomically do all
 * the message passing work themselves, only stopcpu and reschedule
 * messages come through here.
 */
void smp4d_message_pass(int target, int msg, unsigned long data, int wait)
{
	int me = hard_smp4d_processor_id();

	SMP_PRINTK(("smp4d_message_pass %d %d %08lx %d\n", target, msg, data, wait));
	if (msg == MSG_STOP_CPU && target == MSG_ALL_BUT_SELF) {
		unsigned long flags;
		static spinlock_t stop_cpu_lock = SPIN_LOCK_UNLOCKED;
		spin_lock_irqsave(&stop_cpu_lock, flags);
		smp4d_stop_cpu_sender = me;
		smp4d_cross_call((smpfunc_t)smp4d_stop_cpu, 0, 0, 0, 0, 0);
		spin_unlock_irqrestore(&stop_cpu_lock, flags);
	}
	printk("Yeeee, trying to send SMP msg(%d) to %d on cpu %d\n", msg, target, me);
	panic("Bogon SMP message pass.");
}

extern unsigned int prof_multiplier[NR_CPUS];
extern unsigned int prof_counter[NR_CPUS];

extern void sparc_do_profile(unsigned long pc, unsigned long o7);

void smp4d_percpu_timer_interrupt(struct pt_regs *regs)
{
	int cpu = hard_smp4d_processor_id();
	static int cpu_tick[NR_CPUS];
	static char led_mask[] = { 0xe, 0xd, 0xb, 0x7, 0xb, 0xd };

	bw_get_prof_limit(cpu);	
	bw_clear_intr_mask(0, 1);	/* INTR_TABLE[0] & 1 is Profile IRQ */

	cpu_tick[cpu]++;
	if (!(cpu_tick[cpu] & 15)) {
		if (cpu_tick[cpu] == 0x60)
			cpu_tick[cpu] = 0;
		cpu_leds[cpu] = led_mask[cpu_tick[cpu] >> 4];
		show_leds(cpu);
	}

	if(!user_mode(regs))
		sparc_do_profile(regs->pc, regs->u_regs[UREG_RETPC]);

	if(!--prof_counter[cpu]) {
		int user = user_mode(regs);

		irq_enter(cpu, 0);
		update_process_times(user);
		irq_exit(cpu, 0);

		prof_counter[cpu] = prof_multiplier[cpu];
	}
}

extern unsigned int lvl14_resolution;

static void __init smp_setup_percpu_timer(void)
{
	int cpu = hard_smp4d_processor_id();

	prof_counter[cpu] = prof_multiplier[cpu] = 1;
	load_profile_irq(cpu, lvl14_resolution);
}

void __init smp4d_blackbox_id(unsigned *addr)
{
	int rd = *addr & 0x3e000000;
	
	addr[0] = 0xc0800800 | rd;		/* lda [%g0] ASI_M_VIKING_TMP1, reg */
	addr[1] = 0x01000000;    		/* nop */
	addr[2] = 0x01000000;    		/* nop */
}

void __init smp4d_blackbox_current(unsigned *addr)
{
	/* We have a nice Linux current register :) */
	int rd = addr[1] & 0x3e000000;
	
	addr[0] = 0x10800006;			/* b .+24 */
	addr[1] = 0xc0800820 | rd;		/* lda [%g0] ASI_M_VIKING_TMP2, reg */
}

void __init sun4d_init_smp(void)
{
	int i;
	extern unsigned int patchme_store_new_current[];
	extern unsigned int t_nmi[], linux_trap_ipi15_sun4d[], linux_trap_ipi15_sun4m[];

	/* Store current into Linux current register :) */
	__asm__ __volatile__("sta %%g6, [%%g0] %0" : : "i"(ASI_M_VIKING_TMP2));
	
	/* Patch switch_to */
	patchme_store_new_current[0] = (patchme_store_new_current[0] & 0x3e000000) | 0xc0a00820;
	
	/* Patch ipi15 trap table */
	t_nmi[1] = t_nmi[1] + (linux_trap_ipi15_sun4d - linux_trap_ipi15_sun4m);
	
	/* And set btfixup... */
	BTFIXUPSET_BLACKBOX(smp_processor_id, smp4d_blackbox_id);
	BTFIXUPSET_BLACKBOX(load_current, smp4d_blackbox_current);
	BTFIXUPSET_CALL(smp_cross_call, smp4d_cross_call, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(smp_message_pass, smp4d_message_pass, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(__smp_processor_id, __smp4d_processor_id, BTFIXUPCALL_NORM);
	
	for (i = 0; i < NR_CPUS; i++) {
		ccall_info.processors_in[i] = 1;
		ccall_info.processors_out[i] = 1;
	}
}
