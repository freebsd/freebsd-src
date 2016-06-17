#ifndef _ALPHA_HARDIRQ_H
#define _ALPHA_HARDIRQ_H

#include <linux/config.h>
#include <linux/threads.h>

/* entry.S is sensitive to the offsets of these fields */
typedef struct {
	unsigned long __softirq_pending;
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

#define in_interrupt()						\
({								\
	int __cpu = smp_processor_id();				\
	(local_irq_count(__cpu) + local_bh_count(__cpu)) != 0;	\
})

#define in_irq() (local_irq_count(smp_processor_id()) != 0)

#ifndef CONFIG_SMP

extern unsigned long __irq_attempt[];
#define irq_attempt(cpu, irq)  ((void)(cpu), __irq_attempt[irq])

#define hardirq_trylock(cpu)	(local_irq_count(cpu) == 0)
#define hardirq_endlock(cpu)	((void) 0)

#define irq_enter(cpu, irq)	(local_irq_count(cpu)++)
#define irq_exit(cpu, irq)	(local_irq_count(cpu)--)

#define synchronize_irq()	barrier()

#else

#define irq_attempt(cpu, irq) (cpu_data[cpu].irq_attempt[irq])

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
	if (global_irq_holder == cpu) {
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
	return !local_irq_count(cpu) && !spin_is_locked(&global_irq_lock);
}

#define hardirq_endlock(cpu)	do { } while (0)

extern void synchronize_irq(void);

#endif /* CONFIG_SMP */
#endif /* _ALPHA_HARDIRQ_H */
