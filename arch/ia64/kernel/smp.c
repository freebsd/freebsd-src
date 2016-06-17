/*
 * SMP Support
 *
 * Copyright (C) 1999 Walt Drummond <drummond@valinux.com>
 * Copyright (C) 1999, 2001 David Mosberger-Tang <davidm@hpl.hp.com>
 *
 * Lots of stuff stolen from arch/alpha/kernel/smp.c
 *
 * 01/05/16 Rohit Seth <rohit.seth@intel.com>  IA64-SMP functions. Reorganized
 * the existing code (on the lines of x86 port).
 * 00/09/11 David Mosberger <davidm@hpl.hp.com> Do loops_per_jiffy
 * calibration on each CPU.
 * 00/08/23 Asit Mallick <asit.k.mallick@intel.com> fixed logical processor id
 * 00/03/31 Rohit Seth <rohit.seth@intel.com>	Fixes for Bootstrap Processor
 * & cpu_online_map now gets done here (instead of setup.c)
 * 99/10/05 davidm	Update to bring it in sync with new command-line processing
 *  scheme.
 * 10/13/00 Goutham Rao <goutham.rao@intel.com> Updated smp_call_function and
 *		smp_call_function_single to resend IPI on timeouts
 */
#define __KERNEL_SYSCALLS__

#include <linux/config.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/kernel_stat.h>
#include <linux/mm.h>
#include <linux/delay.h>
#include <linux/cache.h>
#include <linux/efi.h>

#include <asm/atomic.h>
#include <asm/bitops.h>
#include <asm/current.h>
#include <asm/delay.h>
#include <asm/machvec.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/sal.h>
#include <asm/system.h>
#include <asm/unistd.h>
#include <asm/mca.h>

/* The 'big kernel lock' */
spinlock_t kernel_flag __cacheline_aligned_in_smp = SPIN_LOCK_UNLOCKED;

/*
 * Structure and data for smp_call_function(). This is designed to minimise static memory
 * requirements. It also looks cleaner.
 */
static spinlock_t call_lock = SPIN_LOCK_UNLOCKED;

struct call_data_struct {
	void (*func) (void *info);
	void *info;
	long wait;
	atomic_t started;
	atomic_t finished;
};

static volatile struct call_data_struct *call_data;

#define IPI_CALL_FUNC		0
#define IPI_CPU_STOP		1

static void
stop_this_cpu (void)
{
	extern void cpu_halt (void);
	/*
	 * Remove this CPU:
	 */
	clear_bit(smp_processor_id(), &cpu_online_map);
	max_xtp();
	__cli();
	cpu_halt();
}

void
handle_IPI (int irq, void *dev_id, struct pt_regs *regs)
{
	int this_cpu = smp_processor_id();
	unsigned long *pending_ipis = &local_cpu_data->ipi.operation;
	unsigned long ops;

	/* Count this now; we may make a call that never returns. */
	local_cpu_data->ipi_count++;

	mb();	/* Order interrupt and bit testing. */
	while ((ops = xchg(pending_ipis, 0)) != 0) {
	  mb();	/* Order bit clearing and data access. */
	  do {
		unsigned long which;

		which = ffz(~ops);
		ops &= ~(1 << which);

		switch (which) {
		case IPI_CALL_FUNC:
			{
				struct call_data_struct *data;
				void (*func)(void *info);
				void *info;
				int wait;

				/* release the 'pointer lock' */
				data = (struct call_data_struct *) call_data;
				func = data->func;
				info = data->info;
				wait = data->wait;

				mb();
				atomic_inc(&data->started);

				/* At this point the structure may be gone unless wait is true.  */
				(*func)(info);

				/* Notify the sending CPU that the task is done.  */
				mb();
				if (wait)
					atomic_inc(&data->finished);
			}
			break;

		case IPI_CPU_STOP:
			stop_this_cpu();
			break;

		default:
			printk(KERN_CRIT "Unknown IPI on CPU %d: %lu\n", this_cpu, which);
			break;
		} /* Switch */
	  } while (ops);

	  mb();	/* Order data access and bit testing. */
	}
}

static inline void
send_IPI_single (int dest_cpu, int op)
{
	set_bit(op, &cpu_data(dest_cpu)->ipi.operation);
	platform_send_ipi(dest_cpu, IA64_IPI_VECTOR, IA64_IPI_DM_INT, 0);
}

static inline void
send_IPI_allbutself (int op)
{
	int i;

	for (i = 0; i < smp_num_cpus; i++) {
		if (i != smp_processor_id())
			send_IPI_single(i, op);
	}
}

static inline void
send_IPI_all (int op)
{
	int i;

	for (i = 0; i < smp_num_cpus; i++)
		send_IPI_single(i, op);
}

static inline void
send_IPI_self (int op)
{
	send_IPI_single(smp_processor_id(), op);
}

void
smp_send_reschedule (int cpu)
{
	platform_send_ipi(cpu, IA64_IPI_RESCHEDULE, IA64_IPI_DM_INT, 0);
}

void
smp_flush_tlb_all (void)
{
	smp_call_function((void (*)(void *))local_flush_tlb_all, 0, 1, 1);
	local_flush_tlb_all();
}

void
smp_flush_tlb_mm (struct mm_struct *mm)
{
	local_flush_tlb_mm(mm);

	/* this happens for the common case of a single-threaded fork():  */
	if (likely(mm == current->active_mm && atomic_read(&mm->mm_users) == 1))
		return;

	smp_call_function((void (*)(void *))local_flush_tlb_mm, mm, 1, 1);
}

/*
 * Run a function on another CPU
 *  <func>	The function to run. This must be fast and non-blocking.
 *  <info>	An arbitrary pointer to pass to the function.
 *  <nonatomic>	Currently unused.
 *  <wait>	If true, wait until function has completed on other CPUs.
 *  [RETURNS]   0 on success, else a negative status code.
 *
 * Does not return until the remote CPU is nearly ready to execute <func>
 * or is or has executed.
 */

int
smp_call_function_single (int cpuid, void (*func) (void *info), void *info, int nonatomic,
			  int wait)
{
	struct call_data_struct data;
	int cpus = 1;

	if (cpuid == smp_processor_id()) {
		printk("%s: trying to call self\n", __FUNCTION__);
		return -EBUSY;
	}

	data.func = func;
	data.info = info;
	atomic_set(&data.started, 0);
	data.wait = wait;
	if (wait)
		atomic_set(&data.finished, 0);

	spin_lock_bh(&call_lock);

	call_data = &data;
	mb();	/* ensure store to call_data precedes setting of IPI_CALL_FUNC */
  	send_IPI_single(cpuid, IPI_CALL_FUNC);

	/* Wait for response */
	while (atomic_read(&data.started) != cpus)
		barrier();

	if (wait)
		while (atomic_read(&data.finished) != cpus)
			barrier();
	call_data = NULL;

	spin_unlock_bh(&call_lock);
	return 0;
}

/*
 * this function sends a 'generic call function' IPI to all other CPUs
 * in the system.
 */

/*
 *  [SUMMARY]	Run a function on all other CPUs.
 *  <func>	The function to run. This must be fast and non-blocking.
 *  <info>	An arbitrary pointer to pass to the function.
 *  <nonatomic>	currently unused.
 *  <wait>	If true, wait (atomically) until function has completed on other CPUs.
 *  [RETURNS]   0 on success, else a negative status code.
 *
 * Does not return until remote CPUs are nearly ready to execute <func> or are or have
 * executed.
 *
 * You must not call this function with disabled interrupts or from a hardware interrupt
 * handler, you may call it from a bottom half handler.
 */
int
smp_call_function (void (*func) (void *info), void *info, int nonatomic, int wait)
{
	struct call_data_struct data;
	int cpus = smp_num_cpus-1;

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
	mb();	/* ensure store to call_data precedes setting of IPI_CALL_FUNC */
	send_IPI_allbutself(IPI_CALL_FUNC);

	/* Wait for response */
	while (atomic_read(&data.started) != cpus)
		barrier();

	if (wait)
		while (atomic_read(&data.finished) != cpus)
			barrier();
	call_data = NULL;

	spin_unlock_bh(&call_lock);
	return 0;
}

void
smp_do_timer (struct pt_regs *regs)
{
	int user = user_mode(regs);

	if (--local_cpu_data->prof_counter <= 0) {
		local_cpu_data->prof_counter = local_cpu_data->prof_multiplier;
		update_process_times(user);
	}
}

/*
 * this function calls the 'stop' function on all other CPUs in the system.
 */
void
smp_send_stop (void)
{
	send_IPI_allbutself(IPI_CPU_STOP);
	smp_num_cpus = 1;
}

int __init
setup_profiling_timer (unsigned int multiplier)
{
	return -EINVAL;
}
