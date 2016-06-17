/*
 * This is the old power_save() function from idle.c:
 *
 * Written by Cort Dougan (cort@cs.nmt.edu)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 */
#include <linux/sched.h>
#include <asm/processor.h>
#include <asm/cputable.h>

void power_save(void)
{
	/*
	 * Make sure the CPU has the DOZE or NAP feature set.
	 * We assume that chip-specific initialization code
	 * has set any other registers necessary (e.g. HID0).
	 */
	if (!(cur_cpu_spec[smp_processor_id()]->cpu_features
	      & (CPU_FTR_CAN_DOZE | CPU_FTR_CAN_NAP)))
		return;

	/*
	 * Disable interrupts to prevent a lost wakeup
	 * when going to sleep.  This is necessary even with
	 * RTLinux since we are not guaranteed an interrupt
	 * didn't come in and is waiting for a __sti() before
	 * emulating one.  This way, we really do hard disable.
	 *
	 * We assume that we're sti-ed when we come in here.  We
	 * are in the idle loop so if we're cli-ed then it's a bug
	 * anyway.
	 *  -- Cort
	 */
	_nmask_and_or_msr(MSR_EE, 0);
	if (!current->need_resched) {
		/* set the POW bit in the MSR, and enable interrupts
		 * so we wake up sometime! */
		_nmask_and_or_msr(0, MSR_POW | MSR_EE);
	}
	_nmask_and_or_msr(0, MSR_EE);
}
