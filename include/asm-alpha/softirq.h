#ifndef _ALPHA_SOFTIRQ_H
#define _ALPHA_SOFTIRQ_H

#include <linux/stddef.h>
#include <asm/atomic.h>
#include <asm/hardirq.h>

extern inline void cpu_bh_disable(int cpu)
{
	local_bh_count(cpu)++;
	barrier();
}

extern inline void __cpu_bh_enable(int cpu)
{
	barrier();
	local_bh_count(cpu)--;
}

#define __local_bh_enable()	__cpu_bh_enable(smp_processor_id())
#define local_bh_disable()	cpu_bh_disable(smp_processor_id())

#define local_bh_enable()					\
do {								\
	int cpu;						\
								\
	barrier();						\
	cpu = smp_processor_id();				\
	if (!--local_bh_count(cpu) && softirq_pending(cpu))	\
		do_softirq();					\
} while (0)

#define in_softirq() (local_bh_count(smp_processor_id()) != 0)

#endif /* _ALPHA_SOFTIRQ_H */
