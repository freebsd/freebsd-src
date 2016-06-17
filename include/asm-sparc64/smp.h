/* smp.h: Sparc64 specific SMP stuff.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef _SPARC64_SMP_H
#define _SPARC64_SMP_H

#include <linux/config.h>
#include <linux/threads.h>
#include <linux/cache.h>
#include <asm/asi.h>
#include <asm/starfire.h>
#include <asm/spitfire.h>

#ifndef __ASSEMBLY__
/* PROM provided per-processor information we need
 * to start them all up.
 */

struct prom_cpuinfo {
	int prom_node;
	int mid;
};

extern int linux_num_cpus;	/* number of CPUs probed  */
extern struct prom_cpuinfo linux_cpus[64];

#endif /* !(__ASSEMBLY__) */

#ifdef CONFIG_SMP

#ifndef __ASSEMBLY__

/* Per processor Sparc parameters we need. */

/* Keep this a multiple of 64-bytes for cache reasons. */
typedef struct {
	/* Dcache line 1 */
	unsigned int	__pad0;		/* bh_count moved to irq_stat for consistency. KAO */
	unsigned int	multiplier;
	unsigned int	counter;
	unsigned int	idle_volume;
	unsigned long	clock_tick;	/* %tick's per second */
	unsigned long	udelay_val;

	/* Dcache line 2 */
	unsigned int	pgcache_size;
	unsigned int	pgdcache_size;
	unsigned long	*pte_cache[2];
	unsigned long	*pgd_cache;

	/* Dcache lines 3 and 4 */
	unsigned int	irq_worklists[16];
} ____cacheline_aligned cpuinfo_sparc;

extern cpuinfo_sparc cpu_data[NR_CPUS];

/*
 *	Private routines/data
 */
 
extern unsigned char boot_cpu_id;
extern unsigned long cpu_present_map;
#define cpu_online_map cpu_present_map

/*
 *	General functions that each host system must provide.
 */

extern void smp_callin(void);
extern void smp_boot_cpus(void);
extern void smp_store_cpu_info(int id);

extern __volatile__ int __cpu_number_map[NR_CPUS];
extern __volatile__ int __cpu_logical_map[NR_CPUS];

extern __inline__ int cpu_logical_map(int cpu)
{
	return __cpu_logical_map[cpu];
}
extern __inline__ int cpu_number_map(int cpu)
{
	return __cpu_number_map[cpu];
}

extern __inline__ int hard_smp_processor_id(void)
{
	if (tlb_type == cheetah || tlb_type == cheetah_plus) {
		unsigned long cfg, ver;
		__asm__ __volatile__("rdpr %%ver, %0" : "=r" (ver));
		if ((ver >> 32) == 0x003e0016) {
			__asm__ __volatile__("ldxa [%%g0] %1, %0"
					     : "=r" (cfg)
					     : "i" (ASI_JBUS_CONFIG));
			return ((cfg >> 17) & 0x1f);
		} else {
			__asm__ __volatile__("ldxa [%%g0] %1, %0"
					     : "=r" (cfg)
					     : "i" (ASI_SAFARI_CONFIG));
			return ((cfg >> 17) & 0x3ff);
		}
	} else if (this_is_starfire != 0) {
		return starfire_hard_smp_processor_id();
	} else {
		unsigned long upaconfig;
		__asm__ __volatile__("ldxa	[%%g0] %1, %0"
				     : "=r" (upaconfig)
				     : "i" (ASI_UPA_CONFIG));
		return ((upaconfig >> 17) & 0x1f);
	}
}

#define smp_processor_id() (current->processor)

/* This needn't do anything as we do not sleep the cpu
 * inside of the idler task, so an interrupt is not needed
 * to get a clean fast response.
 *
 * Addendum: We do want it to do something for the signal
 *           delivery case, we detect that by just seeing
 *           if we are trying to send this to an idler or not.
 */
extern __inline__ void smp_send_reschedule(int cpu)
{
	extern void smp_receive_signal(int);
	if(cpu_data[cpu].idle_volume == 0)
		smp_receive_signal(cpu);
}

/* This is a nop as well because we capture all other cpus
 * anyways when making the PROM active.
 */
extern __inline__ void smp_send_stop(void) { }

#endif /* !(__ASSEMBLY__) */

#define PROC_CHANGE_PENALTY	20

#endif /* !(CONFIG_SMP) */

#define NO_PROC_ID		0xFF

#endif /* !(_SPARC64_SMP_H) */
