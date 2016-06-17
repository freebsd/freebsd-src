/*
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
 *
 * Copyright (C) 2000 - 2001 by Kanoj Sarcar (kanoj@sgi.com)
 * Copyright (C) 2000, 2001, 2002 by Ralf Baechle
 * Copyright (C) 2000 - 2001 by Silicon Graphics, Inc.
 * Copyright (C) 2000, 2001 Broadcom Corporation
 */
#ifndef _ASM_SMP_H
#define _ASM_SMP_H

#include <linux/config.h>

#ifdef CONFIG_SMP

#include <linux/threads.h>
#include <asm/atomic.h>
#include <asm/current.h>

#define smp_processor_id()	(current->processor)

#define PROC_CHANGE_PENALTY	20

/* Map from cpu id to sequential logical cpu number.  This will only
   not be idempotent when cpus failed to come on-line.  */
extern int __cpu_number_map[NR_CPUS];
#define cpu_number_map(cpu)  __cpu_number_map[cpu]

/* The reverse map from sequential logical cpu number to cpu id.  */
extern int __cpu_logical_map[NR_CPUS];
#define cpu_logical_map(cpu)  __cpu_logical_map[cpu]

#define NO_PROC_ID	(-1)

#define SMP_RESCHEDULE_YOURSELF	0x1	/* XXX braindead */
#define SMP_CALL_FUNCTION	0x2

#if (NR_CPUS <= _MIPS_SZLONG)

typedef unsigned long   cpumask_t;

#define CPUMASK_CLRALL(p)	do { (p) = 0; } while(0)
#define CPUMASK_SETB(p, bit)	(p) |= 1UL << (bit)
#define CPUMASK_CLRB(p, bit)	(p) &= ~(1UL << (bit))
#define CPUMASK_TSTB(p, bit)	((p) & (1UL << (bit)))

#elif (NR_CPUS <= 128)

/*
 * The foll should work till 128 cpus.
 */
#define CPUMASK_SIZE		(NR_CPUS/_MIPS_SZLONG)
#define CPUMASK_INDEX(bit)	((bit) >> 6)
#define CPUMASK_SHFT(bit)	((bit) & 0x3f)

typedef struct {
	unsigned long	_bits[CPUMASK_SIZE];
} cpumask_t;

#define	CPUMASK_CLRALL(p)	(p)._bits[0] = 0, (p)._bits[1] = 0
#define CPUMASK_SETB(p, bit)	(p)._bits[CPUMASK_INDEX(bit)] |= \
					(1UL << CPUMASK_SHFT(bit))
#define CPUMASK_CLRB(p, bit)	(p)._bits[CPUMASK_INDEX(bit)] &= \
					~(1UL << CPUMASK_SHFT(bit))
#define CPUMASK_TSTB(p, bit)	((p)._bits[CPUMASK_INDEX(bit)] & \
					(1UL << CPUMASK_SHFT(bit)))

#else
#error cpumask macros only defined for 128p kernels
#endif

struct call_data_struct {
	void		(*func)(void *);
	void		*info;
	atomic_t	started;
	atomic_t	finished;
	int		wait;
};

extern struct call_data_struct *call_data;

extern cpumask_t cpu_online_map;

/* These are defined by the board-specific code. */

/*
 * Cause the function described by call_data to be executed on the passed
 * cpu.  When the function has finished, increment the finished field of
 * call_data.
 */
void core_send_ipi(int cpu, unsigned int action);

/*
 * Clear all undefined state in the cpu, set up sp and gp to the passed
 * values, and kick the cpu into smp_bootstrap();
 */
void prom_boot_secondary(int cpu, unsigned long sp, unsigned long gp);

/*
 *  After we've done initial boot, this function is called to allow the
 *  board code to clean up state, if needed
 */
void prom_init_secondary(void);

/*
 * Do whatever setup needs to be done for SMP at the board level.  Return
 * the number of cpus in the system, including this one
 */
int prom_setup_smp(void);

void prom_smp_finish(void);

extern void asmlinkage smp_bootstrap(void);

#endif /* CONFIG_SMP */
#endif /* _ASM_SMP_H */
