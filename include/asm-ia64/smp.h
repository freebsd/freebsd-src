/*
 * SMP Support
 *
 * Copyright (C) 1999 VA Linux Systems
 * Copyright (C) 1999 Walt Drummond <drummond@valinux.com>
 * Copyright (C) 2001-2003 Hewlett-Packard Co
 *	David Mosberger-Tang <davidm@hpl.hp.com>
 */
#ifndef _ASM_IA64_SMP_H
#define _ASM_IA64_SMP_H

#include <linux/config.h>

#ifdef CONFIG_SMP

#include <linux/init.h>
#include <linux/threads.h>
#include <linux/kernel.h>

#include <asm/io.h>
#include <asm/param.h>
#include <asm/processor.h>
#include <asm/ptrace.h>

#define XTP_OFFSET		0x1e0008

#define SMP_IRQ_REDIRECTION	(1 << 0)
#define SMP_IPI_REDIRECTION	(1 << 1)

#define smp_processor_id()	(current->processor)

extern struct smp_boot_data {
	int cpu_count;
	int cpu_phys_id[NR_CPUS];
} smp_boot_data __initdata;

extern char no_int_routing __initdata;

extern volatile unsigned long cpu_online_map;
extern unsigned long ipi_base_addr;
extern unsigned char smp_int_redirect;
extern int smp_num_cpus;

extern volatile int ia64_cpu_to_sapicid[];
#define cpu_physical_id(i)	ia64_cpu_to_sapicid[i]
#define cpu_number_map(i)	(i)
#define cpu_logical_map(i)	(i)

extern unsigned long ap_wakeup_vector;

#define cpu_online(cpu)		(cpu_online_map & (1UL << (cpu)))

/*
 * Function to map hard smp processor id to logical id.  Slow, so
 * don't use this in performance-critical code.
 */
static inline int
cpu_logical_id (int cpuid)
{
	int i;

	for (i = 0; i < smp_num_cpus; ++i)
		if (cpu_physical_id(i) == cpuid)
			break;
	return i;
}

/*
 * XTP control functions:
 *	min_xtp   : route all interrupts to this CPU
 *	normal_xtp: nominal XTP value
 *	max_xtp   : never deliver interrupts to this CPU.
 */

static inline void
min_xtp (void)
{
	if (smp_int_redirect & SMP_IRQ_REDIRECTION)
		writeb(0x00, ipi_base_addr | XTP_OFFSET); /* XTP to min */
}

static inline void
normal_xtp (void)
{
	if (smp_int_redirect & SMP_IRQ_REDIRECTION)
		writeb(0x08, ipi_base_addr | XTP_OFFSET); /* XTP normal */
}

static inline void
max_xtp (void)
{
	if (smp_int_redirect & SMP_IRQ_REDIRECTION)
		writeb(0x0f, ipi_base_addr | XTP_OFFSET); /* Set XTP to max */
}

static inline unsigned int
hard_smp_processor_id (void)
{
	union {
		struct {
			unsigned long reserved : 16;
			unsigned long eid : 8;
			unsigned long id : 8;
			unsigned long ignored : 32;
		} f;
		unsigned long bits;
	} lid;

	lid.bits = ia64_get_lid();
	return lid.f.id << 8 | lid.f.eid;
}

#define NO_PROC_ID		0xffffffff	/* no processor magic marker */

/*
 * Extra overhead to move a task from one cpu to another (due to TLB and cache misses).
 * Expressed in "negative nice value" units (larger number means higher priority/penalty).
 */
#define PROC_CHANGE_PENALTY	20

extern void __init init_smp_config (void);
extern void smp_do_timer (struct pt_regs *regs);

extern int smp_call_function_single (int cpuid, void (*func) (void *info), void *info,
				     int retry, int wait);

extern void smp_build_cpu_map(void);

#endif /* CONFIG_SMP */
#endif /* _ASM_IA64_SMP_H */
