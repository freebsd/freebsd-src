/* hardirq.h: 64-bit Sparc hard IRQ support.
 *
 * Copyright (C) 1997, 1998 David S. Miller (davem@caip.rutgers.edu)
 */

#ifndef __SPARC64_HARDIRQ_H
#define __SPARC64_HARDIRQ_H

#include <linux/config.h>
#include <linux/threads.h>
#include <linux/brlock.h>
#include <linux/spinlock.h>

/* entry.S is sensitive to the offsets of these fields */
/* rtrap.S is sensitive to the size of this structure */
typedef struct {
	unsigned int __softirq_pending;
	unsigned int __unused_1;
#ifndef CONFIG_SMP
	unsigned int __local_irq_count;
#else
	unsigned int __unused_on_SMP;	/* DaveM says use brlock for SMP irq. KAO */
#endif
	unsigned int __local_bh_count;
	unsigned int __syscall_count;
        struct task_struct * __ksoftirqd_task;
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h>	/* Standard mappings for irq_cpustat_t above */
/* Note that local_irq_count() is replaced by sparc64 specific version for SMP */

#ifndef CONFIG_SMP
#define irq_enter(cpu, irq)	((void)(irq), local_irq_count(cpu)++)
#define irq_exit(cpu, irq)	((void)(irq), local_irq_count(cpu)--)
#else
#undef local_irq_count
#define local_irq_count(cpu)	(__brlock_array[cpu][BR_GLOBALIRQ_LOCK])
#define irq_enter(cpu, irq)	br_read_lock(BR_GLOBALIRQ_LOCK)
#define irq_exit(cpu, irq)	br_read_unlock(BR_GLOBALIRQ_LOCK)
#endif

/*
 * Are we in an interrupt context? Either doing bottom half
 * or hardware interrupt processing?
 */
#define in_interrupt() ((local_irq_count(smp_processor_id()) + \
		         local_bh_count(smp_processor_id())) != 0)

/* This tests only the local processors hw IRQ context disposition.  */
#define in_irq() (local_irq_count(smp_processor_id()) != 0)

#ifndef CONFIG_SMP

#define hardirq_trylock(cpu)	((void)(cpu), local_irq_count(smp_processor_id()) == 0)
#define hardirq_endlock(cpu)	do { (void)(cpu); } while(0)

#define synchronize_irq()	barrier()

#else /* (CONFIG_SMP) */

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

#define hardirq_endlock(cpu)	do { (void)(cpu); } while (0)

extern void synchronize_irq(void);

#endif /* CONFIG_SMP */

#endif /* !(__SPARC64_HARDIRQ_H) */
