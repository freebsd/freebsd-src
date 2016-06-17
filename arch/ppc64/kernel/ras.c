
/*
 * ras.c
 * Copyright (C) 2001 Dave Engebretsen IBM Corporation
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/* Change Activity:
 * 2001/09/21 : engebret : Created with minimal EPOW and HW exception support.
 * End Change Activity 
 */

#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/threads.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/proc_fs.h>
#include <linux/random.h>
#include <linux/sysrq.h>

#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/cache.h>
#include <asm/prom.h>
#include <asm/ptrace.h>
#include <asm/iSeries/LparData.h>
#include <asm/machdep.h>
#include <asm/rtas.h>
#include <asm/ppcdebug.h>

static void ras_epow_interrupt(int irq, void *dev_id, struct pt_regs * regs);
static void ras_error_interrupt(int irq, void *dev_id, struct pt_regs * regs);
void init_ras_IRQ(void);

/* #define DEBUG */

/*
 * Initialize handlers for the set of interrupts caused by hardware errors
 * and power system events.
 */
void init_ras_IRQ(void) {
	struct device_node *np;
	unsigned int *ireg, len, i;

	if((np = find_path_device("/event-sources/internal-errors")) &&
	   (ireg = (unsigned int *)get_property(np, "open-pic-interrupt", 
						&len))) {
		for(i=0; i<(len / sizeof(*ireg)); i++) {
			request_irq(irq_offset_up(*(ireg)),
				    &ras_error_interrupt, 0, 
				    "RAS_ERROR", NULL);
			ireg++;
		}
	}

	if((np = find_path_device("/event-sources/epow-events")) &&
	   (ireg = (unsigned int *)get_property(np, "open-pic-interrupt", 
						&len))) {
		for(i=0; i<(len / sizeof(*ireg)); i++) {
			request_irq(irq_offset_up(*(ireg)),
				    &ras_epow_interrupt, 0, 
				    "RAS_EPOW", NULL);
			ireg++;
		}
	}
}

/*
 * Handle power subsystem events (EPOW).
 *
 * Presently we just log the event has occured.  This should be fixed
 * to examine the type of power failure and take appropriate action where
 * the time horizon permits something useful to be done.
 */
static void
ras_epow_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct rtas_error_log log_entry;
	unsigned int size = sizeof(log_entry);
	long status = 0xdeadbeef;

	status = rtas_call(rtas_token("check-exception"), 6, 1, NULL, 
			   0x500, irq, 
			   RTAS_EPOW_WARNING | RTAS_POWERMGM_EVENTS,
			   1,  /* Time Critical */
			   __pa(&log_entry), size);

	udbg_printf("EPOW <0x%lx 0x%lx>\n", 
		    *((unsigned long *)&log_entry), status); 
	printk(KERN_WARNING 
	       "EPOW <0x%lx 0x%lx>\n",*((unsigned long *)&log_entry), status);

	/* format and print the extended information */
	log_error((char *)&log_entry, ERR_TYPE_RTAS_LOG, 0);
}

/*
 * Handle hardware error interrupts.
 *
 * RTAS check-exception is called to collect data on the exception.  If
 * the error is deemed recoverable, we log a warning and return.
 * For nonrecoverable errors, an error is logged and we stop all processing
 * as quickly as possible in order to prevent propagation of the failure.
 */
static void
ras_error_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct rtas_error_log log_entry;
	unsigned int size = sizeof(log_entry);
	long status = 0xdeadbeef;
	int fatal;

	status = rtas_call(rtas_token("check-exception"), 6, 1, NULL, 
			   0x500, irq, 
			   RTAS_INTERNAL_ERROR,
			   1, /* Time Critical */
			   __pa(&log_entry), size);

	if ((status == 0) && (log_entry.severity >= SEVERITY_ERROR_SYNC))
		fatal = 1;
	else
		fatal = 0;

	/* format and print the extended information */
	log_error((char *)&log_entry, ERR_TYPE_RTAS_LOG, fatal);

	if (fatal) {
		udbg_printf("HW Error <0x%lx 0x%lx>\n",
			    *((unsigned long *)&log_entry), status);
		printk(KERN_EMERG 
		       "Error: Fatal hardware error <0x%lx 0x%lx>\n",
		       *((unsigned long *)&log_entry), status);

#ifndef DEBUG
		/* Don't actually power off when debugging so we can test
		 * without actually failing while injecting errors.
		 * Error data will not be logged to syslog.
		 */
		ppc_md.power_off();
#endif
	} else {
		udbg_printf("Recoverable HW Error <0x%lx 0x%lx>\n",
			    *((unsigned long *)&log_entry), status); 
		printk(KERN_WARNING 
		       "Warning: Recoverable hardware error <0x%lx 0x%lx>\n",
		       *((unsigned long *)&log_entry), status);

		return;
	}
}
