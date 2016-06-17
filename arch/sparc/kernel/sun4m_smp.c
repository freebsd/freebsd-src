/* sun4m_smp.c: Sparc SUN4M SMP support.
 *
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

#define __KERNEL_SYSCALLS__
#include <linux/unistd.h>

#define IRQ_RESCHEDULE		13
#define IRQ_STOP_CPU		14
#define IRQ_CROSS_CALL		15

extern ctxd_t *srmmu_ctx_table_phys;
extern int linux_num_cpus;

extern void calibrate_delay(void);

extern struct task_struct *current_set[NR_CPUS];
extern volatile int smp_processors_ready;
extern unsigned long cpu_present_map;
extern int smp_num_cpus;
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
extern int __smp4m_processor_id(void);

extern unsigned long totalram_pages;

/*#define SMP_DEBUG*/

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

void __init smp4m_callin(void)
{
	int cpuid = hard_smp_processor_id();

	local_flush_cache_all();
	local_flush_tlb_all();

	set_irq_udt(mid_xlate[boot_cpu_id]);

	/* Get our local ticker going. */
	smp_setup_percpu_timer();

	calibrate_delay();
	smp_store_cpu_info(cpuid);

	local_flush_cache_all();
	local_flush_tlb_all();

	/*
	 * Unblock the master CPU _only_ when the scheduler state
	 * of all secondary CPUs will be up-to-date, so after
	 * the SMP initialization the master will be just allowed
	 * to call the scheduler code.
	 */
	init_idle();

	/* Allow master to continue. */
	swap((unsigned long *)&cpu_callin_map[cpuid], 1);

	local_flush_cache_all();
	local_flush_tlb_all();
	
	cpu_probe();

	/* Fix idle thread fields. */
	__asm__ __volatile__("ld [%0], %%g6\n\t"
			     : : "r" (&current_set[cpuid])
			     : "memory" /* paranoid */);

	/* Attach to the address space of init_task. */
	atomic_inc(&init_mm.mm_count);
	current->active_mm = &init_mm;

	while(!smp_commenced)
		barrier();

	local_flush_cache_all();
	local_flush_tlb_all();

	__sti();
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

void __init smp4m_boot_cpus(void)
{
	int cpucount = 0;
	int i = 0;
	int first, prev;

	printk("Entering SMP Mode...\n");

	__sti();
	cpu_present_map = 0;

	for(i=0; i < linux_num_cpus; i++)
		cpu_present_map |= (1<<i);

	for(i=0; i < NR_CPUS; i++) {
		cpu_offset[i] = (char *)&cpu_data[i] - (char *)&cpu_data;
		__cpu_number_map[i] = -1;
		__cpu_logical_map[i] = -1;
	}

	mid_xlate[boot_cpu_id] = (linux_cpus[boot_cpu_id].mid & ~8);
	__cpu_number_map[boot_cpu_id] = 0;
	__cpu_logical_map[0] = boot_cpu_id;
	current->processor = boot_cpu_id;

	smp_store_cpu_info(boot_cpu_id);
	set_irq_udt(mid_xlate[boot_cpu_id]);
	smp_setup_percpu_timer();
	init_idle();
	local_flush_cache_all();
	if(linux_num_cpus == 1)
		return;  /* Not an MP box. */
	for(i = 0; i < NR_CPUS; i++) {
		if(i == boot_cpu_id)
			continue;

		if(cpu_present_map & (1 << i)) {
			extern unsigned long sun4m_cpu_startup;
			unsigned long *entry = &sun4m_cpu_startup;
			struct task_struct *p;
			int timeout;

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

			/* See trampoline.S for details... */
			entry += ((i-1) * 3);

			/*
			 * Initialize the contexts table
			 * Since the call to prom_startcpu() trashes the structure,
			 * we need to re-initialize it for each cpu
			 */
			smp_penguin_ctable.which_io = 0;
			smp_penguin_ctable.phys_addr = (unsigned int) srmmu_ctx_table_phys;
			smp_penguin_ctable.reg_size = 0;

			/* whirrr, whirrr, whirrrrrrrrr... */
			printk("Starting CPU %d at %p\n", i, entry);
			mid_xlate[i] = (linux_cpus[i].mid & ~8);
			local_flush_cache_all();
			prom_startcpu(linux_cpus[i].prom_node,
				      &smp_penguin_ctable, 0, (char *)entry);

			/* wheee... it's going... */
			for(timeout = 0; timeout < 10000; timeout++) {
				if(cpu_callin_map[i])
					break;
				udelay(200);
			}
			if(cpu_callin_map[i]) {
				/* Another "Red Snapper". */
				__cpu_number_map[i] = i;
				__cpu_logical_map[i] = i;
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
		cpu_present_map = (1 << smp_processor_id());
	} else {
		unsigned long bogosum = 0;
		for(i = 0; i < NR_CPUS; i++) {
			if(cpu_present_map & (1 << i))
				bogosum += cpu_data[i].udelay_val;
		}
		printk("Total of %d Processors activated (%lu.%02lu BogoMIPS).\n",
		       cpucount + 1,
		       bogosum/(500000/HZ),
		       (bogosum/(5000/HZ))%100);
		smp_activated = 1;
		smp_num_cpus = cpucount + 1;
	}

	/* Setup CPU list for IRQ distribution scheme. */
	first = prev = -1;
	for(i = 0; i < NR_CPUS; i++) {
		if(cpu_present_map & (1 << i)) {
			if(first == -1)
				first = i;
			if(prev != -1)
				cpu_data[prev].next = i;
			cpu_data[i].mid = mid_xlate[i];
			prev = i;
		}
	}
	cpu_data[prev].next = first;
	
	/* Free unneeded trap tables */
	if (!(cpu_present_map & (1 << 1))) {
		ClearPageReserved(virt_to_page(trapbase_cpu1));
		set_page_count(virt_to_page(trapbase_cpu1), 1);
		free_page((unsigned long)trapbase_cpu1);
		totalram_pages++;
		num_physpages++;
	}
	if (!(cpu_present_map & (1 << 2))) {
		ClearPageReserved(virt_to_page(trapbase_cpu2));
		set_page_count(virt_to_page(trapbase_cpu2), 1);
		free_page((unsigned long)trapbase_cpu2);
		totalram_pages++;
		num_physpages++;
	}
	if (!(cpu_present_map & (1 << 3))) {
		ClearPageReserved(virt_to_page(trapbase_cpu3));
		set_page_count(virt_to_page(trapbase_cpu3), 1);
		free_page((unsigned long)trapbase_cpu3);
		totalram_pages++;
		num_physpages++;
	}

	/* Ok, they are spinning and ready to go. */
	smp_processors_ready = 1;
}

/* At each hardware IRQ, we get this called to forward IRQ reception
 * to the next processor.  The caller must disable the IRQ level being
 * serviced globally so that there are no double interrupts received.
 */
void smp4m_irq_rotate(int cpu)
{
	if(smp_processors_ready)
		set_irq_udt(cpu_data[cpu_data[cpu].next].mid);
}

/* Cross calls, in order to work efficiently and atomically do all
 * the message passing work themselves, only stopcpu and reschedule
 * messages come through here.
 */
void smp4m_message_pass(int target, int msg, unsigned long data, int wait)
{
	static unsigned long smp_cpu_in_msg[NR_CPUS];
	unsigned long mask;
	int me = smp_processor_id();
	int irq, i;

	if(msg == MSG_RESCHEDULE) {
		irq = IRQ_RESCHEDULE;

		if(smp_cpu_in_msg[me])
			return;
	} else if(msg == MSG_STOP_CPU) {
		irq = IRQ_STOP_CPU;
	} else {
		goto barf;
	}

	smp_cpu_in_msg[me]++;
	if(target == MSG_ALL_BUT_SELF || target == MSG_ALL) {
		mask = cpu_present_map;
		if(target == MSG_ALL_BUT_SELF)
			mask &= ~(1 << me);
		for(i = 0; i < 4; i++) {
			if(mask & (1 << i))
				set_cpu_int(mid_xlate[i], irq);
		}
	} else {
		set_cpu_int(mid_xlate[target], irq);
	}
	smp_cpu_in_msg[me]--;

	return;
barf:
	printk("Yeeee, trying to send SMP msg(%d) on cpu %d\n", msg, me);
	panic("Bogon SMP message pass.");
}

static struct smp_funcall {
	smpfunc_t func;
	unsigned long arg1;
	unsigned long arg2;
	unsigned long arg3;
	unsigned long arg4;
	unsigned long arg5;
	unsigned long processors_in[NR_CPUS];  /* Set when ipi entered. */
	unsigned long processors_out[NR_CPUS]; /* Set when ipi exited. */
} ccall_info;

static spinlock_t cross_call_lock = SPIN_LOCK_UNLOCKED;

/* Cross calls must be serialized, at least currently. */
void smp4m_cross_call(smpfunc_t func, unsigned long arg1, unsigned long arg2,
		    unsigned long arg3, unsigned long arg4, unsigned long arg5)
{
	if(smp_processors_ready) {
		register int ncpus = smp_num_cpus;
		unsigned long flags;

		spin_lock_irqsave(&cross_call_lock, flags);

		/* Init function glue. */
		ccall_info.func = func;
		ccall_info.arg1 = arg1;
		ccall_info.arg2 = arg2;
		ccall_info.arg3 = arg3;
		ccall_info.arg4 = arg4;
		ccall_info.arg5 = arg5;

		/* Init receive/complete mapping, plus fire the IPI's off. */
		{
			register unsigned long mask;
			register int i;

			mask = (cpu_present_map & ~(1 << smp_processor_id()));
			for(i = 0; i < ncpus; i++) {
				if(mask & (1 << i)) {
					ccall_info.processors_in[i] = 0;
					ccall_info.processors_out[i] = 0;
					set_cpu_int(mid_xlate[i], IRQ_CROSS_CALL);
				} else {
					ccall_info.processors_in[i] = 1;
					ccall_info.processors_out[i] = 1;
				}
			}
		}

		{
			register int i;

			i = 0;
			do {
				while(!ccall_info.processors_in[i])
					barrier();
			} while(++i < ncpus);

			i = 0;
			do {
				while(!ccall_info.processors_out[i])
					barrier();
			} while(++i < ncpus);
		}

		spin_unlock_irqrestore(&cross_call_lock, flags);
	}
}

/* Running cross calls. */
void smp4m_cross_call_irq(void)
{
	int i = smp_processor_id();

	ccall_info.processors_in[i] = 1;
	ccall_info.func(ccall_info.arg1, ccall_info.arg2, ccall_info.arg3,
			ccall_info.arg4, ccall_info.arg5);
	ccall_info.processors_out[i] = 1;
}

extern unsigned int prof_multiplier[NR_CPUS];
extern unsigned int prof_counter[NR_CPUS];

extern void sparc_do_profile(unsigned long pc, unsigned long o7);

void smp4m_percpu_timer_interrupt(struct pt_regs *regs)
{
	int cpu = smp_processor_id();

	clear_profile_irq(mid_xlate[cpu]);

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
	int cpu = smp_processor_id();

	prof_counter[cpu] = prof_multiplier[cpu] = 1;
	load_profile_irq(mid_xlate[cpu], lvl14_resolution);

	if(cpu == boot_cpu_id)
		enable_pil_irq(14);
}

void __init smp4m_blackbox_id(unsigned *addr)
{
	int rd = *addr & 0x3e000000;
	int rs1 = rd >> 11;
	
	addr[0] = 0x81580000 | rd;		/* rd %tbr, reg */
	addr[1] = 0x8130200c | rd | rs1;    	/* srl reg, 0xc, reg */
	addr[2] = 0x80082003 | rd | rs1;	/* and reg, 3, reg */
}

void __init smp4m_blackbox_current(unsigned *addr)
{
	int rd = *addr & 0x3e000000;
	int rs1 = rd >> 11;
	
	addr[0] = 0x81580000 | rd;		/* rd %tbr, reg */
	addr[2] = 0x8130200a | rd | rs1;    	/* srl reg, 0xa, reg */
	addr[4] = 0x8008200c | rd | rs1;	/* and reg, 3, reg */
}

void __init sun4m_init_smp(void)
{
	BTFIXUPSET_BLACKBOX(smp_processor_id, smp4m_blackbox_id);
	BTFIXUPSET_BLACKBOX(load_current, smp4m_blackbox_current);
	BTFIXUPSET_CALL(smp_cross_call, smp4m_cross_call, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(smp_message_pass, smp4m_message_pass, BTFIXUPCALL_NORM);
	BTFIXUPSET_CALL(__smp_processor_id, __smp4m_processor_id, BTFIXUPCALL_NORM);
}
