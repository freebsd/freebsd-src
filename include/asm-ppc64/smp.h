/* 
 * smp.h: PPC64 specific SMP code.
 *
 * Original was a copy of sparc smp.h.  Now heavily modified
 * for PPC.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1996-2001 Cort Dougan <cort@fsmlabs.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifdef __KERNEL__
#ifndef _PPC64_SMP_H
#define _PPC64_SMP_H

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/threads.h>	/* for NR_CPUS */


#ifndef __ASSEMBLY__
#ifdef CONFIG_SMP

#include <asm/paca.h>

struct current_set_struct {
	struct task_struct *task;
	unsigned long *sp_real;
};

extern unsigned long cpu_online_map;

extern void smp_message_pass(int target, int msg, unsigned long data, int wait);
extern void smp_store_cpu_info(int id);
extern void smp_send_tlb_invalidate(int);
extern void smp_send_xmon_break(int cpu);
struct pt_regs;
extern void smp_message_recv(int, struct pt_regs *);

/*
 * Retrieve the state of a CPU:
 * online:    CPU is in a normal run state
 * possible:  CPU is a candidate to be made online
 * available: CPU is candidate for the 'possible' pool
 */
#define cpu_possible(cpu)       paca[cpu].active
#define cpu_available(cpu)      paca[cpu].available

#define NO_PROC_ID		0xFF            /* No processor magic marker */
#define PROC_CHANGE_PENALTY	20

/* 1 to 1 mapping on PPC -- Cort */
#define cpu_logical_map(cpu) (cpu)
#define cpu_number_map(x) (x)
extern volatile unsigned long cpu_callin_map[NR_CPUS];

#define smp_processor_id() (get_paca()->xPacaIndex)
#define hard_smp_processor_id() (get_paca()->xHwProcNum)



/* Since OpenPIC has only 4 IPIs, we use slightly different message numbers.
 *
 * Make sure this matches openpic_request_IPIs in open_pic.c, or what shows up
 * in /proc/interrupts will be wrong!!! --Troy */
#define PPC_MSG_CALL_FUNCTION   0
#define PPC_MSG_RESCHEDULE      1
#define PPC_MSG_INVALIDATE_TLB  2
#define PPC_MSG_XMON_BREAK      3

void smp_init_iSeries(void);
void smp_init_pSeries(void);

#else /* CONFIG_SMP */
#define cpu_possible(cpu)	((cpu) == 0)
#define cpu_available(cpu)	((cpu) == 0)

#endif /* !(CONFIG_SMP) */
#endif /* __ASSEMBLY__ */
#endif /* !(_PPC64_SMP_H) */
#endif /* __KERNEL__ */
