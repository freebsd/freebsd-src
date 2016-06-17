#ifndef __ASM_HARDIRQ_H
#define __ASM_HARDIRQ_H

/* only non-SMP supported */

#include <linux/threads.h>

/* entry.S is sensitive to the offsets of these fields */
typedef struct {
	unsigned int __softirq_pending;
	unsigned int __local_irq_count;
	unsigned int __local_bh_count;
	unsigned int __syscall_count;
	struct task_struct * __ksoftirqd_task; /* waitqueue is too large */
} ____cacheline_aligned irq_cpustat_t;

#include <linux/irq_cpustat.h> /* Standard mappings for irq_cpustat_t above */

/*
 * Are we in an interrupt context? Either doing bottom half
 * or hardware interrupt processing?
 */
#define in_interrupt()  ((local_irq_count(smp_processor_id()) + \
			  local_bh_count(smp_processor_id())) != 0)
#define in_irq()        (local_irq_count(smp_processor_id()) != 0)

#define hardirq_trylock(cpu)    (local_irq_count(cpu) == 0)
#define hardirq_endlock(cpu)    do { (void)(cpu); } while (0)

#define irq_enter(cpu)      (local_irq_count(cpu)++)
#define irq_exit(cpu)       (local_irq_count(cpu)--)

#define synchronize_irq()       barrier()

#endif /* __ASM_HARDIRQ_H */
