/*
 * Idle daemon for PowerPC.  Idle daemon will handle any action
 * that needs to be taken when the system becomes idle.
 *
 * Written by Cort Dougan (cort@cs.nmt.edu)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/config.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/stddef.h>
#include <linux/unistd.h>
#include <linux/ptrace.h>
#include <linux/slab.h>

#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <asm/cache.h>
#include <asm/cputable.h>

unsigned long zero_paged_on;
unsigned long powersave_nap;
unsigned long powersave_lowspeed;

extern void power_save(void);

int idled(void)
{
	int do_power_save = 0;

	/* Check if CPU can powersave (get rid of that soon!) */
	if (cur_cpu_spec[smp_processor_id()]->cpu_features &
		(CPU_FTR_CAN_DOZE | CPU_FTR_CAN_NAP))
		do_power_save = 1;

	/* endless loop with no priority at all */
	current->nice = 20;
	current->counter = -100;
	init_idle();
	for (;;) {
#ifdef CONFIG_SMP
		if (!do_power_save) {
			/*
			 * Deal with another CPU just having chosen a thread to
			 * run here:
			 */
			int oldval = xchg(&current->need_resched, -1);

			if (!oldval) {
				while(current->need_resched == -1)
					; /* Do Nothing */
			}
		}
#endif
		if (do_power_save && !current->need_resched)
			power_save();

		if (current->need_resched) {
			schedule();
			check_pgt_cache();
		}
	}
	return 0;
}

/*
 * SMP entry into the idle task - calls the same thing as the
 * non-smp versions. -- Cort
 */
int cpu_idle(void)
{
	idled();
	return 0;
}
