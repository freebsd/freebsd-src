/*
 * Idle daemon for PowerPC.  Idle daemon will handle any action
 * that needs to be taken when the system becomes idle.
 *
 * Originally Written by Cort Dougan (cort@cs.nmt.edu)
 *
 * iSeries supported added by Mike Corrigan <mikejc@us.ibm.com>
 *
 * Additional shared processor, SMT, and firmware support
 *    Copyright (c) 2003 Dave Engebretsen <engebret@us.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */
#include <linux/config.h>
#include <linux/init.h>
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

#include <asm/time.h>
#include <asm/iSeries/LparData.h>
#include <asm/iSeries/HvCall.h>
#include <asm/iSeries/ItLpQueue.h>

int (*idle_loop)(void);

#ifdef CONFIG_PPC_ISERIES
static void yield_shared_processor(void)
{
	struct paca_struct *lpaca = get_paca();

	HvCall_setEnabledInterrupts( HvCall_MaskIPI |
				     HvCall_MaskLpEvent |
				     HvCall_MaskLpProd |
				     HvCall_MaskTimeout );

	if ( ! ItLpQueue_isLpIntPending( paca->lpQueuePtr ) ) {
		/*
		 * Compute future tb value when yield should expire.
		 * We want to be woken up when the next decrementer is
		 * to fire.
		 */
	  
		__cli();
		lpaca->yielded = 1;        /* Indicate a prod is desired */
		lpaca->xLpPaca.xIdle = 1;  /* Inform the HV we are idle  */

		HvCall_yieldProcessor(HvCall_YieldTimed,
				      lpaca->next_jiffy_update_tb);

		lpaca->yielded = 0;        /* Back to IPI's */
		__sti();

		/*
		 * The decrementer stops during the yield.  Force a fake
		 * decrementer here and let the timer_interrupt code sort
		 * out the actual time.
	   */
	  lpaca->xLpPaca.xIntDword.xFields.xDecrInt = 1;
	}
	  
	process_iSeries_events();
}

int idle_iSeries(void)
{
	struct paca_struct *lpaca;
	long oldval;
	unsigned long CTRL;

	/* endless loop with no priority at all */
	current->nice = 20;
	current->counter = -100;

	/* ensure iSeries run light will be out when idle */
	current->thread.flags &= ~PPC_FLAG_RUN_LIGHT;
	CTRL = mfspr(CTRLF);
	CTRL &= ~RUNLATCH;
	mtspr(CTRLT, CTRL);
	init_idle();	

	lpaca = get_paca();

	for (;;) {
		if ( lpaca->xLpPaca.xSharedProc ) {
			if ( ItLpQueue_isLpIntPending( lpaca->lpQueuePtr ) )
				process_iSeries_events();
			if ( !current->need_resched )
				yield_shared_processor();
		} else {
			/* Avoid an IPI by setting need_resched */
			oldval = xchg(&current->need_resched, -1);
			if (!oldval) {
				while(current->need_resched == -1) {
					HMT_medium();
					if ( ItLpQueue_isLpIntPending( lpaca->lpQueuePtr ) )
						process_iSeries_events();
					HMT_low();
				}
			}
		}
		HMT_medium();
		if (current->need_resched) {
			lpaca->xLpPaca.xIdle = 0;
			schedule();
			check_pgt_cache();
		}
	}
	return 0;
}
#endif

int idle_default(void)
{
	long oldval;

	current->nice = 20;
	current->counter = -100;
	init_idle();

	for (;;) {
		/* Avoid an IPI by setting need_resched */
		oldval = xchg(&current->need_resched, -1);
		if (!oldval) {
			while(current->need_resched == -1) {
					HMT_low();
			}
		}
		HMT_medium();
		if (current->need_resched) {
			schedule();
			check_pgt_cache();
		}
	}
	return 0;
}

int idle_dedicated(void)
{
	long oldval;
	struct paca_struct *lpaca = get_paca(), *ppaca;;
	unsigned long start_snooze;

	ppaca = &paca[(lpaca->xPacaIndex) ^ 1];
	current->nice = 20;
	current->counter = -100;
	init_idle();

	for (;;) {
		/* Indicate to the HV that we are idle.  Now would be
		 * a good time to find other work to dispatch. */
		lpaca->xLpPaca.xIdle = 1;

		/* Avoid an IPI by setting need_resched */
		oldval = xchg(&current->need_resched, -1);
		if (!oldval) {
			start_snooze = __get_tb();
			while(current->need_resched == -1) {
				if (__get_tb() <
				    (start_snooze +
				     naca->smt_snooze_delay*tb_ticks_per_usec)) {
					HMT_low(); /* Low thread priority */
					continue;
				}

				HMT_very_low(); /* Low power mode */

				/* If the SMT mode is system controlled & the
				 * partner thread is doing work, switch into
				 * ST mode.
				 */
				if((naca->smt_state == SMT_DYNAMIC) &&
				   (!(ppaca->xLpPaca.xIdle))) {
					/* need_resched could be 1 or -1 at this
					 * point.  If it is -1, set it to 0, so
					 * an IPI/Prod is sent.  If it is 1, keep
					 * it that way & schedule work.
					 */
					oldval = xchg(&current->need_resched, 0);
					if(oldval == 1) {
						current->need_resched = oldval;
						break;
					}

					/* DRENG: Go HMT_medium here ? */
					__cli();
					lpaca->yielded = 1;

					/* SMT dynamic mode.  Cede will result
					 * in this thread going dormant, if the
					 * partner thread is still doing work.
					 * Thread wakes up if partner goes idle,
					 * an interrupt is presented, or a prod
					 * occurs.  Returning from the cede
					 * enables external interrupts.
					 */
					cede_processor();

					lpaca->yielded = 0;
				} else {
					/* Give the HV an opportunity at the
					 * processor, since we are not doing
					 * any work.
					 */
					poll_pending();
				}
			}
		}
		HMT_medium();
		if (current->need_resched) {
			lpaca->xLpPaca.xIdle = 0;
			schedule();
			check_pgt_cache();
		}
	}
	return 0;
}

int idle_shared(void)
{
	struct paca_struct *lpaca = get_paca();

	/* endless loop with no priority at all */
	current->nice = 20;
	current->counter = -100;

	init_idle();

	for (;;) {
		/* Indicate to the HV that we are idle.  Now would be
		 * a good time to find other work to dispatch. */
		lpaca->xLpPaca.xIdle = 1;

		if (!current->need_resched) {
			__cli();
			lpaca->yielded = 1;

/*
			 * Yield the processor to the hypervisor.  We return if
			 * an external interrupt occurs (which are driven prior
			 * to returning here) or if a prod occurs from another
			 * processor.  When returning here, external interrupts
			 * are enabled.
 */
			cede_processor();

			lpaca->yielded = 0;
		}

		HMT_medium();
		if (current->need_resched) {
			lpaca->xLpPaca.xIdle = 0;
			schedule();
			check_pgt_cache();
		}
	}

	return 0;
}

int cpu_idle(void)
{
	idle_loop();
	return 0; 
}

int idle_setup(void)
{
#ifdef CONFIG_PPC_ISERIES
	idle_loop = idle_iSeries;
#else
	if (systemcfg->platform & PLATFORM_PSERIES) {
		if (cur_cpu_spec->firmware_features & FW_FEATURE_SPLPAR) {
			if(get_paca()->xLpPaca.xSharedProc) {
				printk("idle = idle_shared\n");
				idle_loop = idle_shared;
			} else {
				printk("idle = idle_dedicated\n");
				idle_loop = idle_dedicated;
			}
		} else {
			printk("idle = idle_default\n");
			idle_loop = idle_default;
		}
	} else {
		printk("idle_setup: unknown platform, use idle_default\n");
		idle_loop = idle_default;
	}
#endif

	return 1;
}

