/*
 * Copyright 2003 PMC-Sierra
 * Author: Manish Lachwani (lachwani@pmc-sierra.com)
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
#include <asm/trace.h>

extern void smp_call_function_interrupt(void);

/*
 * Send inter-processor interrupt
 */
void core_send_ipi(int cpu, unsigned int action)
{
        /*
         * Generate and INTMSG so that it can be sent over to the destination CPU
         * The INTMSG will put the STATUS bits based on the action desired
         */
        switch(action) {
                case SMP_RESCHEDULE_YOURSELF:
                        /* Do nothing */
                        break;
                case SMP_CALL_FUNCTION:
                        if (cpu == 1)
                                *(volatile u_int32_t *)(0xbb000a00) = 0x00610002;
                        else
                                *(volatile u_int32_t *)(0xbb000a00) = 0x00610001;
                        break;

                default:
                        panic("core_send_ipi \n");
        }
}

/*
 * Mailbox interrupt to handle IPI
 */
void jaguar_mailbox_irq(struct pt_regs *regs)
{
        int cpu = smp_processor_id();

        /* SMP_CALL_FUNCTION */
        smp_call_function_interrupt();
}

extern atomic_t cpus_booted;

void __init start_secondary(void)
{
        unsigned int cpu = smp_processor_id();
        extern atomic_t smp_commenced;

        if (current->processor != 1) {
                printk("Impossible CPU %d \n", cpu);
                current->processor = 1;
                current->cpus_runnable = 1 << 1;
                cpu = current->processor;
        }

        if (current->mm)
                current->mm = NULL;

        prom_init_secondary();
        per_cpu_trap_init();

        /*
         * XXX parity protection should be folded in here when it's converted
         * to an option instead of something based on .cputype
         */
        pgd_current[cpu] = init_mm.pgd;
        cpu_data[cpu].udelay_val = loops_per_jiffy;
        prom_smp_finish();
        CPUMASK_SETB(cpu_online_map, cpu);
        atomic_inc(&cpus_booted);
        __flush_cache_all();

        printk("Slave cpu booted successfully  \n");
        *(volatile u_int32_t *)(0xbb000a68) = 0x00000000;
        *(volatile u_int32_t *)(0xbb000a68) = 0x80000000;

        while (*(volatile u_int32_t *)(0xbb000a68) != 0x00000000);

        return cpu_idle();
}

void __init smp_boot_cpus(void)
{
        int i;
        int cur_cpu = 0;

        smp_num_cpus = prom_setup_smp();
        printk("Detected %d available CPUs \n", smp_num_cpus);

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

        /*
         * This loop attempts to compensate for "holes" in the CPU
         * numbering.  It's overkill, but general.
         */
        for (i = 1; i < smp_num_cpus; ) {
                struct task_struct *p;
                struct pt_regs regs;
                int retval;
                printk("Starting CPU %d... \n", i);

                /* Spawn a new process normally.  Grab a pointer to
                   its task struct so we can mess with it */
                do_fork(CLONE_VM|CLONE_PID, 0, &regs, 0);

                p = init_task.prev_task;
                if (!p)
                        panic("failed fork for CPU %d", i);

                /* This is current for the second processor */
                p->processor = i;
                p->cpus_runnable = 1 << i; /* we schedule the first task manually */
                p->thread.reg31 = (unsigned long) start_secondary;

                del_from_runqueue(p);
                unhash_process(p);
                init_tasks[i] = p;

                __flush_cache_all();

                do {
                        /* Iterate until we find a CPU that comes up */
                        cur_cpu++;
                        retval = prom_boot_secondary(cur_cpu,
                                            (unsigned long)p + KERNEL_STACK_SIZE - 32,
                                            (unsigned long)p);

                } while (!retval && (cur_cpu < NR_CPUS));
                if (retval) {
                        __cpu_number_map[cur_cpu] = i;
                        __cpu_logical_map[i] = cur_cpu;
                        i++;
                } else {
                        panic("CPU discovery disaster");
                }
        }

        /* Local semaphore to both the CPUs */

        *(volatile u_int32_t *)(0xbb000a68) = 0x80000000;
        while (*(volatile u_int32_t *)(0xbb000a68) != 0x00000000);

        smp_threads_ready = 1;
}
