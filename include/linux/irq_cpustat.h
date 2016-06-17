#ifndef __irq_cpustat_h
#define __irq_cpustat_h

/*
 * Contains default mappings for irq_cpustat_t, used by almost every
 * architecture.  Some arch (like s390) have per cpu hardware pages and
 * they define their own mappings for irq_stat.
 *
 * Keith Owens <kaos@ocs.com.au> July 2000.
 */

#include <linux/config.h>

/*
 * Simple wrappers reducing source bloat.  Define all irq_stat fields
 * here, even ones that are arch dependent.  That way we get common
 * definitions instead of differing sets for each arch.
 */

extern irq_cpustat_t irq_stat[];			/* defined in asm/hardirq.h */

#ifdef CONFIG_SMP
#define __IRQ_STAT(cpu, member)	(irq_stat[cpu].member)
#else
#define __IRQ_STAT(cpu, member)	((void)(cpu), irq_stat[0].member)
#endif	

  /* arch independent irq_stat fields */
#define softirq_pending(cpu)	__IRQ_STAT((cpu), __softirq_pending)
#define local_irq_count(cpu)	__IRQ_STAT((cpu), __local_irq_count)
#define local_bh_count(cpu)	__IRQ_STAT((cpu), __local_bh_count)
#define syscall_count(cpu)	__IRQ_STAT((cpu), __syscall_count)
#define ksoftirqd_task(cpu)	__IRQ_STAT((cpu), __ksoftirqd_task)
  /* arch dependent irq_stat fields */
#define nmi_count(cpu)		__IRQ_STAT((cpu), __nmi_count)		/* i386, ia64 */

#endif	/* __irq_cpustat_h */
