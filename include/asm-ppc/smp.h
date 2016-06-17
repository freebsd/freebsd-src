/* smp.h: PPC specific SMP stuff.
 *
 * Original was a copy of sparc smp.h.  Now heavily modified
 * for PPC.
 *
 * Copyright (C) 1996 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1996-2001 Cort Dougan <cort@fsmlabs.com>
 */
#ifdef __KERNEL__
#ifndef _PPC_SMP_H
#define _PPC_SMP_H

#include <linux/config.h>
#include <linux/kernel.h>

#ifdef CONFIG_SMP

#ifndef __ASSEMBLY__

struct cpuinfo_PPC {
	unsigned long loops_per_jiffy;
	unsigned long pvr;
	unsigned long *pgd_cache;
	unsigned long *pte_cache;
	unsigned long pgtable_cache_sz;
};

extern struct cpuinfo_PPC cpu_data[NR_CPUS];
extern unsigned long cpu_online_map;
extern unsigned long smp_proc_in_lock[NR_CPUS];
extern volatile unsigned long cpu_callin_map[NR_CPUS];
extern int smp_tb_synchronized;

extern void smp_store_cpu_info(int id);
extern void smp_send_tlb_invalidate(int);
extern void smp_send_xmon_break(int cpu);
struct pt_regs;
extern void smp_message_recv(int, struct pt_regs *);
extern void smp_local_timer_interrupt(struct pt_regs *);

#define NO_PROC_ID		0xFF            /* No processor magic marker */
#define PROC_CHANGE_PENALTY	20

/* 1 to 1 mapping on PPC -- Cort */
#define cpu_logical_map(cpu) (cpu)
#define cpu_number_map(x) (x)

#define smp_processor_id() (current->processor)

extern int smp_hw_index[NR_CPUS];
#define hard_smp_processor_id() (smp_hw_index[smp_processor_id()])

struct klock_info_struct {
	unsigned long kernel_flag;
	unsigned char akp;
};

extern struct klock_info_struct klock_info;
#define KLOCK_HELD       0xffffffff
#define KLOCK_CLEAR      0x0

#ifdef CONFIG_750_SMP
#define smp_send_tlb_invalidate(x) smp_ppc750_send_tlb_invalidate(x)
#else
#define smp_send_tlb_invalidate(x) do {} while(0)
#endif

#endif /* __ASSEMBLY__ */

#else /* !(CONFIG_SMP) */

#endif /* !(CONFIG_SMP) */

#endif /* !(_PPC_SMP_H) */
#endif /* __KERNEL__ */
