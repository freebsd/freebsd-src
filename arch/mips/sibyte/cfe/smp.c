/*
 * Copyright (C) 2000, 2001, 2002, 2003 Broadcom Corporation
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

#include <linux/sched.h>
#include <linux/smp.h>

#include <asm/mipsregs.h>

#include "cfe_api.h"
#include "cfe_error.h"

/* Boot all other cpus in the system, initialize them, and
   bring them into the boot fn */
void prom_boot_secondary(int cpu, unsigned long sp, unsigned long gp)
{
	int retval;
	
	retval = cfe_cpu_start(cpu, smp_bootstrap, sp, gp, 0);
	if (retval != 0) {
		printk("cfe_start_cpu(%i) returned %i\n" , cpu, retval);
	}
}

void prom_init_secondary(void)
{
	extern void load_mmu(void);
	unsigned int imask = STATUSF_IP4 | STATUSF_IP3 | STATUSF_IP2 |
		STATUSF_IP1 | STATUSF_IP0;

	/* cache and TLB setup */
	load_mmu();

	/* Enable basic interrupts */
	change_c0_status(ST0_IM, imask);
	set_c0_status(ST0_IE);
}

/*
 * Set up state, return the total number of cpus in the system, including
 * the master
 */
int prom_setup_smp(void)
{
	int i;
	int num_cpus = 1;

	/* Use CFE to find out how many CPUs are available */
	for (i=1; i<NR_CPUS; i++) {
		if (cfe_cpu_stop(i) == 0) {
			num_cpus++;
		}
	}
	printk("Detected %i available CPU(s)\n", num_cpus);
	return num_cpus;
}

void prom_smp_finish(void)
{
	extern void sb1250_smp_finish(void);
	sb1250_smp_finish();
}
