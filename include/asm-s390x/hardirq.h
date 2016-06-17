/*
 *  include/asm-s390/hardirq.h
 *
 *  S390 version
 *    Copyright (C) 1999 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
 *               Denis Joseph Barrow (djbarrow@de.ibm.com,barrow_dj@yahoo.com)
 *
 *  Derived from "include/asm-i386/hardirq.h"
 */

#ifndef __ASM_HARDIRQ_H
#define __ASM_HARDIRQ_H

#include <linux/config.h>
#include <linux/threads.h>
#include <asm/lowcore.h>
#include <linux/sched.h>

/* entry.S is sensitive to the offsets of these fields */
typedef struct {
	unsigned int __softirq_pending;
	unsigned int __local_irq_count;
	unsigned int __local_bh_count;
	unsigned int __syscall_count;
	struct task_struct * __ksoftirqd_task; /* waitqueue is too large */
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
  
#define hardirq_enter(cpu)	(local_irq_count(cpu)++)
#define hardirq_exit(cpu)	(local_irq_count(cpu)--)

#define synchronize_irq()	do { } while (0)

#else

#include <asm/atomic.h>
#include <asm/smp.h>

extern atomic_t global_irq_holder;
extern atomic_t global_irq_lock;
extern atomic_t global_irq_count;

static inline void release_irqlock(int cpu)
{
	/* if we didn't own the irq lock, just ignore.. */
	if (atomic_read(&global_irq_holder) ==  cpu) {
		atomic_set(&global_irq_holder,NO_PROC_ID);
                atomic_set(&global_irq_lock,0);
	}
}

static inline void hardirq_enter(int cpu)
{
        ++local_irq_count(cpu);
	atomic_inc(&global_irq_count);
}

static inline void hardirq_exit(int cpu)
{
	atomic_dec(&global_irq_count);
        --local_irq_count(cpu);
}

static inline int hardirq_trylock(int cpu)
{
	return !atomic_read(&global_irq_count) && 
               !atomic_read(&global_irq_lock);
}

#define hardirq_endlock(cpu)	do { } while (0)

extern void synchronize_irq(void);

#endif /* CONFIG_SMP */

#endif /* __ASM_HARDIRQ_H */
