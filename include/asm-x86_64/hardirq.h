#ifndef __ASM_HARDIRQ_H
#define __ASM_HARDIRQ_H

#include <linux/config.h>
#include <linux/threads.h>
#include <linux/irq.h>

/* assembly code in softirq.h is sensitive to the offsets of these fields */
typedef struct {
	unsigned int __softirq_pending;
	unsigned int __local_irq_count;
	unsigned int __local_bh_count;
	unsigned int __syscall_count;
	struct task_struct * __ksoftirqd_task; /* waitqueue is too large */
	unsigned int __nmi_count;	/* arch dependent */
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h>	/* Standard mappings for irq_cpustat_t above */

/*
 * Are we in an interrupt context? Either doing bottom half
 * or hardware interrupt processing?
 */
#define in_interrupt() ({ int __cpu = smp_processor_id(); \
	(local_irq_count(__cpu) + local_bh_count(__cpu) != 0); })

#define in_irq() (local_irq_count(smp_processor_id()) != 0)

#ifndef CONFIG_SMP

#define hardirq_trylock(cpu)	(local_irq_count(cpu) == 0)
#define hardirq_endlock(cpu)	do { } while (0)

#define irq_enter(cpu, irq)	(local_irq_count(cpu)++)
#define irq_exit(cpu, irq)	(local_irq_count(cpu)--)

#define synchronize_irq()	barrier()

#else

#include <asm/atomic.h>
#include <asm/smp.h>

extern unsigned char global_irq_holder;
extern unsigned volatile long global_irq_lock; /* long for set_bit -RR */

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
		clear_bit(0,&global_irq_lock);
	}
}

static inline void irq_enter(int cpu, int irq)
{
	++local_irq_count(cpu);

	smp_mb();
	
	while (test_bit(0,&global_irq_lock)) {
		cpu_relax();
	}
}

static inline void irq_exit(int cpu, int irq)
{
	--local_irq_count(cpu);
}

static inline int hardirq_trylock(int cpu)
{
	return !local_irq_count(cpu) && !test_bit(0,&global_irq_lock);
}

#define hardirq_endlock(cpu)	do { } while (0)

extern void synchronize_irq(void);

#endif /* CONFIG_SMP */

#endif /* __ASM_HARDIRQ_H */
