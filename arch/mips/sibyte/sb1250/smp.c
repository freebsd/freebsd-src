/*
 * Copyright (C) 2001 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <linux/init.h>
#include <linux/delay.h>
#include <linux/smp.h>
#include <linux/kernel_stat.h>

#include <asm/mmu_context.h>
#include <asm/sibyte/64bit.h>
#include <asm/sibyte/sb1250.h>
#include <asm/sibyte/sb1250_regs.h>
#include <asm/sibyte/sb1250_int.h>

extern void smp_call_function_interrupt(void);

/*
 * These are routines for dealing with the sb1250 smp capabilities
 * independent of board/firmware
 */

static u64 mailbox_set_regs[] = {
	KSEG1 + A_IMR_CPU0_BASE + R_IMR_MAILBOX_SET_CPU,
	KSEG1 + A_IMR_CPU1_BASE + R_IMR_MAILBOX_SET_CPU
};

static u64 mailbox_clear_regs[] = {
	KSEG1 + A_IMR_CPU0_BASE + R_IMR_MAILBOX_CLR_CPU,
	KSEG1 + A_IMR_CPU1_BASE + R_IMR_MAILBOX_CLR_CPU
};

static u64 mailbox_regs[] = {
	KSEG1 + A_IMR_CPU0_BASE + R_IMR_MAILBOX_CPU,
	KSEG1 + A_IMR_CPU1_BASE + R_IMR_MAILBOX_CPU
};


/*
 * Simple enough; everything is set up, so just poke the appropriate mailbox
 * register, and we should be set
 */
void core_send_ipi(int cpu, unsigned int action)
{
	out64((((u64)action)<< 48), mailbox_set_regs[cpu]);
}


void sb1250_smp_finish(void)
{
	sb1250_time_init();
}

void sb1250_mailbox_interrupt(struct pt_regs *regs)
{
	int cpu = smp_processor_id();
	unsigned int action;

	kstat.irqs[cpu][K_INT_MBOX_0]++;
	/* Load the mailbox register to figure out what we're supposed to do */
	action = (in64(mailbox_regs[cpu]) >> 48) & 0xffff;

	/* Clear the mailbox to clear the interrupt */
	out64(((u64)action)<<48, mailbox_clear_regs[cpu]);

	/*
	 * Nothing to do for SMP_RESCHEDULE_YOURSELF; returning from the
	 * interrupt will do the reschedule for us
	 */

	if (action & SMP_CALL_FUNCTION) {
		smp_call_function_interrupt();
	}
}

extern atomic_t cpus_booted;
extern atomic_t smp_commenced;

/*
 * Hook for doing final board-specific setup after the generic smp setup
 * is done
 */
asmlinkage void start_secondary(void)
{
	unsigned int cpu = smp_processor_id();

	cpu_probe();
	prom_init_secondary();
	per_cpu_trap_init();

	/*
	 * XXX parity protection should be folded in here when it's converted
	 * to an option instead of something based on .cputype
	 */
	pgd_current[cpu] = init_mm.pgd;
	cpu_data[cpu].udelay_val = loops_per_jiffy;
	prom_smp_finish();
	printk("Slave cpu booted successfully\n");
	CPUMASK_SETB(cpu_online_map, cpu);
	atomic_inc(&cpus_booted);
	while (!atomic_read(&smp_commenced));
	cpu_idle();
}

void __init smp_boot_cpus(void)
{
	int i;
	int cur_cpu = 0;

	smp_num_cpus = prom_setup_smp();
	init_new_context(current, &init_mm);
	current->processor = 0;
	cpu_data[0].udelay_val = loops_per_jiffy;
	cpu_data[0].asid_cache = ASID_FIRST_VERSION;
	CPUMASK_CLRALL(cpu_online_map);
	CPUMASK_SETB(cpu_online_map, 0);
	atomic_set(&cpus_booted, 1);  /* Master CPU is already booted... */
	init_idle();
	__cpu_number_map[0] = 0;
	__cpu_logical_map[0] = 0;
	/* smp_tune_scheduling();  XXX */

	/*
	 * This loop attempts to compensate for "holes" in the CPU
	 * numbering.  It's overkill, but general.
	 */
	for (i = 1; i < smp_num_cpus; ) {
		struct task_struct *p;
		struct pt_regs regs;
		printk("Starting CPU %d... ", i);

		/* Spawn a new process normally.  Grab a pointer to
		   its task struct so we can mess with it */
		do_fork(CLONE_VM|CLONE_PID, 0, &regs, 0);
		p = init_task.prev_task;

		/* Schedule the first task manually */
		p->processor = i;
		p->cpus_runnable = 1 << i; /* we schedule the first task manually */

		init_tasks[i] = p;

		del_from_runqueue(p);
		unhash_process(p);

		do {
			/* Iterate until we find a CPU that comes up */
			cur_cpu++;
			prom_boot_secondary(cur_cpu,
					    (unsigned long)p + KERNEL_STACK_SIZE - 32,
					    (unsigned long)p);
		} while (cur_cpu < NR_CPUS);
		__cpu_number_map[cur_cpu] = i;
		__cpu_logical_map[i] = cur_cpu;
		i++;
	}

	/* Wait for everyone to come up */
	while (atomic_read(&cpus_booted) != smp_num_cpus);
	smp_threads_ready = 1;
}
