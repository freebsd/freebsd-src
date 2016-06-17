#ifndef _ASM_IA64_HARDIRQ_H
#define _ASM_IA64_HARDIRQ_H

/*
 * Copyright (C) 1998-2001 Hewlett-Packard Co
 * Copyright (C) 1998-2001 David Mosberger-Tang <davidm@hpl.hp.com>
 */

#include <linux/config.h>

#include <linux/threads.h>
#include <linux/irq.h>

#include <asm/processor.h>

/*
 * No irq_cpustat_t for IA-64.  The data is held in the per-CPU data structure.
 */
#define softirq_pending(cpu)		(cpu_data(cpu)->softirq_pending)
#define ksoftirqd_task(cpu)		(cpu_data(cpu)->ksoftirqd)
#define irq_count(cpu)			(cpu_data(cpu)->irq_stat.f.irq_count)
#define bh_count(cpu)			(cpu_data(cpu)->irq_stat.f.bh_count)
#define syscall_count(cpu)		/* unused on IA-64 */
#define nmi_count(cpu)			0

#define local_softirq_pending()		(local_cpu_data->softirq_pending)
#define local_ksoftirqd_task()		(local_cpu_data->ksoftirqd)
#define really_local_irq_count()	(local_cpu_data->irq_stat.f.irq_count)	/* XXX fix me */
#define really_local_bh_count()		(local_cpu_data->irq_stat.f.bh_count)	/* XXX fix me */
#define local_syscall_count()		/* unused on IA-64 */
#define local_nmi_count()		0

/*
 * Are we in an interrupt context? Either doing bottom half or hardware interrupt
 * processing?
 */
#define in_interrupt()			(local_cpu_data->irq_stat.irq_and_bh_counts != 0)
#define in_irq()			(local_cpu_data->irq_stat.f.irq_count != 0)

#ifndef CONFIG_SMP
# define local_hardirq_trylock()	(really_local_irq_count() == 0)
# define local_hardirq_endlock()	do { } while (0)

# define local_irq_enter(irq)		(really_local_irq_count()++)
# define local_irq_exit(irq)		(really_local_irq_count()--)

# define synchronize_irq()		barrier()
#else

#include <asm/atomic.h>
#include <asm/smp.h>

extern unsigned int global_irq_holder;
extern volatile unsigned long global_irq_lock;

static inline int
irqs_running (void)
{
	int i;

	for (i = 0; i < smp_num_cpus; i++)
		if (irq_count(i))
			return 1;
	return 0;
}

static inline void
release_irqlock (int cpu)
{
	/* if we didn't own the irq lock, just ignore.. */
	if (global_irq_holder == cpu) {
		global_irq_holder = NO_PROC_ID;
		smp_mb__before_clear_bit();	/* need barrier before releasing lock... */
		clear_bit(0,&global_irq_lock);
        }
}

static inline void
local_irq_enter (int irq)
{
	really_local_irq_count()++;

	while (test_bit(0,&global_irq_lock)) {
		/* nothing */;
	}
}

static inline void
local_irq_exit (int irq)
{
	really_local_irq_count()--;
}

static inline int
local_hardirq_trylock (void)
{
	return !really_local_irq_count() && !test_bit(0,&global_irq_lock);
}

#define local_hardirq_endlock()		do { } while (0)

extern void synchronize_irq (void);

#endif /* CONFIG_SMP */
#endif /* _ASM_IA64_HARDIRQ_H */
