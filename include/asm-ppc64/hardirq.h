#ifdef __KERNEL__
#ifndef __ASM_HARDIRQ_H
#define __ASM_HARDIRQ_H

/*
 * Use a brlock for the global irq lock, based on sparc64.
 * Anton Blanchard <anton@au1.ibm.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/config.h>
#include <linux/brlock.h>
#include <linux/spinlock.h>


typedef struct {
	unsigned long __softirq_pending;
#ifndef CONFIG_SMP
	unsigned int __local_irq_count;
#else
	unsigned int __unused_on_SMP;		/* We use brlocks on SMP */
#endif
	unsigned int __local_bh_count;
	unsigned int __syscall_count;
	unsigned long __unused;
	struct task_struct * __ksoftirqd_task;
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h>	/* Standard mappings for irq_cpustat_t above */
/* Note that local_irq_count() is replaced by ppc64 specific version for SMP */

#ifndef CONFIG_SMP
#define irq_enter(cpu)		(local_irq_count(cpu)++)
#define irq_exit(cpu)		(local_irq_count(cpu)--)
#else
#undef local_irq_count
#define local_irq_count(cpu)	(__brlock_array[cpu][BR_GLOBALIRQ_LOCK])
#define irq_enter(cpu)		br_read_lock(BR_GLOBALIRQ_LOCK)
#define irq_exit(cpu)		br_read_unlock(BR_GLOBALIRQ_LOCK)
#endif

/*
 * Are we in an interrupt context? Either doing bottom half
 * or hardware interrupt processing?
 */
#define in_interrupt() ({ int __cpu = smp_processor_id(); \
	(local_irq_count(__cpu) + local_bh_count(__cpu) != 0); })

/* This tests only the local processors hw IRQ context disposition.  */
#define in_irq() (local_irq_count(smp_processor_id()) != 0)

#ifndef CONFIG_SMP

#define hardirq_trylock(cpu)	(local_irq_count(cpu) == 0)
#define hardirq_endlock(cpu)	do { } while (0)

#define synchronize_irq()	barrier()

#else /* CONFIG_SMP */

static __inline__ int irqs_running(void)
{
	int i;

	for (i = 0; i < smp_num_cpus; i++)
		if (local_irq_count(cpu_logical_map(i)))
			return 1;
	return 0;
}

extern unsigned char global_irq_holder;

static inline void release_irqlock(int cpu)
{
	/* if we didn't own the irq lock, just ignore... */
	if(global_irq_holder == (unsigned char) cpu) {
		global_irq_holder = NO_PROC_ID;
		br_write_unlock(BR_GLOBALIRQ_LOCK);
	}
}

static inline int hardirq_trylock(int cpu)
{
	spinlock_t *lock = &__br_write_locks[BR_GLOBALIRQ_LOCK].lock;

	return (!local_irq_count(cpu) && !spin_is_locked(lock));
}

#define hardirq_endlock(cpu)    do { (void)(cpu); } while (0)

extern void synchronize_irq(void);

#endif /* CONFIG_SMP */

#endif /* __KERNEL__ */
#endif /* __ASM_HARDIRQ_H */
