/* hardirq.h: PA-RISC hard IRQ support.
 *
 * Copyright (C) 2001 Matthew Wilcox <matthew@wil.cx>
 *
 * The locking is really quite interesting.  There's a cpu-local
 * count of how many interrupts are being handled, and a global
 * lock.  An interrupt can only be serviced if the global lock
 * is free.  You can't be sure no more interrupts are being
 * serviced until you've acquired the lock and then checked
 * all the per-cpu interrupt counts are all zero.  It's a specialised
 * br_lock, and that's exactly how Sparc does it.  We don't because
 * it's more locking for us.  This way is lock-free in the interrupt path.
 */

#ifndef _PARISC_HARDIRQ_H
#define _PARISC_HARDIRQ_H

#include <linux/config.h>
#include <linux/threads.h>

typedef struct {
	unsigned long __softirq_pending; /* set_bit is used on this */
	unsigned int __local_irq_count;
	unsigned int __local_bh_count;
	unsigned int __syscall_count;
	struct task_struct * __ksoftirqd_task;
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h>	/* Standard mappings for irq_cpustat_t above */

/*
 * Are we in an interrupt context? Either doing bottom half
 * or hardware interrupt processing?
 */
#define in_interrupt() ({ int __cpu = smp_processor_id(); \
	(local_irq_count(__cpu) + local_bh_count(__cpu) != 0); })

#define in_irq() ({ int __cpu = smp_processor_id(); \
	(local_irq_count(__cpu) != 0); })

#ifndef CONFIG_SMP

#define hardirq_trylock(cpu)	(local_irq_count(cpu) == 0)
#define hardirq_endlock(cpu)	do { } while (0)

#define irq_enter(cpu, irq)	(local_irq_count(cpu)++)
#define irq_exit(cpu, irq)	(local_irq_count(cpu)--)

#define synchronize_irq()	barrier()

#else

#include <asm/system.h>
#include <asm/atomic.h>
#include <linux/spinlock.h>
#include <asm/smp.h>

extern int global_irq_holder;
extern spinlock_t global_irq_lock;

static inline int irqs_running (void)
{
	int i;

	for (i = 0; i < smp_num_cpus; i++)
		if (local_irq_count(i))
			return 1;
	return 0;
}


static inline void release_irqlock(int cpu)
{
	/* if we didn't own the irq lock, just ignore.. */
	if (global_irq_holder == (unsigned char) cpu) {
		global_irq_holder = NO_PROC_ID;
		spin_unlock(&global_irq_lock);
	}
}

static inline void irq_enter(int cpu, int irq)
{
	++local_irq_count(cpu);

	while (spin_is_locked(&global_irq_lock))
		barrier();
}

static inline void irq_exit(int cpu, int irq)
{
	--local_irq_count(cpu);
}

static inline int hardirq_trylock(int cpu)
{
	return !local_irq_count(cpu) && !spin_is_locked (&global_irq_lock);
}

#define hardirq_endlock(cpu)	do { } while (0)

extern void synchronize_irq(void);

#endif /* CONFIG_SMP */

#endif /* _PARISC_HARDIRQ_H */
