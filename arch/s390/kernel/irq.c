/*
 *  arch/s390/kernel/irq.c
 *
 *  S390 version
 *    Copyright (C) 1999,2000 IBM Deutschland Entwicklung GmbH, IBM Corporation
 *    Author(s): Ingo Adlung (adlung@de.ibm.com)
 *
 *  Derived from "arch/i386/kernel/irq.c"
 *    Copyright (C) 1992, 1999 Linus Torvalds, Ingo Molnar
 *
 *  S/390 I/O interrupt processing and I/O request processing is
 *   implemented in arch/s390/kernel/s390io.c
 */
#include <linux/module.h>
#include <linux/config.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/random.h>
#include <linux/smp.h>
#include <linux/threads.h>
#include <linux/smp_lock.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/smp.h>
#include <asm/pgtable.h>
#include <asm/delay.h>
#include <asm/lowcore.h>

void          s390_init_IRQ   ( void );
void          s390_free_irq   ( unsigned int irq, void *dev_id);
int           s390_request_irq( unsigned int irq,
                     void           (*handler)(int, void *, struct pt_regs *),
                     unsigned long  irqflags,
                     const char    *devname,
                     void          *dev_id);

#if 0
/*
 * The following vectors are part of the Linux architecture, there
 * is no hardware IRQ pin equivalent for them, they are triggered
 * through the ICC by us (IPIs), via smp_message_pass():
 */
BUILD_SMP_INTERRUPT(reschedule_interrupt)
BUILD_SMP_INTERRUPT(invalidate_interrupt)
BUILD_SMP_INTERRUPT(stop_cpu_interrupt)
BUILD_SMP_INTERRUPT(mtrr_interrupt)
BUILD_SMP_INTERRUPT(spurious_interrupt)
#endif

/*
 * Global interrupt locks for SMP. Allow interrupts to come in on any
 * CPU, yet make cli/sti act globally to protect critical regions..
 */
#ifdef CONFIG_SMP
atomic_t global_irq_holder = ATOMIC_INIT(NO_PROC_ID);
atomic_t global_irq_lock = ATOMIC_INIT(0);
atomic_t global_irq_count = ATOMIC_INIT(0);
atomic_t global_bh_count;

/*
 * "global_cli()" is a special case, in that it can hold the
 * interrupts disabled for a longish time, and also because
 * we may be doing TLB invalidates when holding the global
 * IRQ lock for historical reasons. Thus we may need to check
 * SMP invalidate events specially by hand here (but not in
 * any normal spinlocks)
 *
 * Thankfully we don't need this as we can deliver flush tlbs with
 * interrupts disabled DJB :-)
 */
#define check_smp_invalidate(cpu)

static void show(char * str)
{
	int i;
	unsigned long *stack;
	int cpu = smp_processor_id();

	printk("\n%s, CPU %d:\n", str, cpu);
	printk("irq:  %d [%d]\n",
	       atomic_read(&global_irq_count),local_irq_count(smp_processor_id()));
	printk("bh:   %d [%d]\n",
	       atomic_read(&global_bh_count),local_bh_count(smp_processor_id()));
	stack = (unsigned long *) &str;
	for (i = 40; i ; i--) {
		unsigned long x = *++stack;
		if (x > (unsigned long) &init_task_union && x < (unsigned long) &vsprintf) {
			printk("<[%08lx]> ", x);
		}
	}
}

#define MAXCOUNT 100000000

static inline void wait_on_bh(void)
{
	int count = MAXCOUNT;
	do {
		if (!--count) {
			show("wait_on_bh");
			count = ~0;
		}
		/* nothing .. wait for the other bh's to go away */
	} while (atomic_read(&global_bh_count) != 0);
}

static inline void wait_on_irq(int cpu)
{
	int count = MAXCOUNT;

	for (;;) {

		/*
		 * Wait until all interrupts are gone. Wait
		 * for bottom half handlers unless we're
		 * already executing in one..
		 */
		if (!atomic_read(&global_irq_count)) {
			if (local_bh_count(cpu)||
			    !atomic_read(&global_bh_count))
				break;
		}

		/* Duh, we have to loop. Release the lock to avoid deadlocks */
		atomic_set(&global_irq_lock, 0);

		for (;;) {
			if (!--count) {
				show("wait_on_irq");
				count = ~0;
			}
			__sti();
			SYNC_OTHER_CORES(cpu);
			__cli();
			check_smp_invalidate(cpu);
			if (atomic_read(&global_irq_count))
				continue;
			if (atomic_read(&global_irq_lock))
				continue;
			if (!local_bh_count(cpu)
			    && atomic_read(&global_bh_count))
				continue;
			if (!atomic_compare_and_swap(0, 1, &global_irq_lock))
				 break;
		}
	}
}

/*
 * This is called when we want to synchronize with
 * bottom half handlers. We need to wait until
 * no other CPU is executing any bottom half handler.
 *
 * Don't wait if we're already running in an interrupt
 * context or are inside a bh handler.
 */
void synchronize_bh(void)
{
	if (atomic_read(&global_bh_count) && !in_interrupt())
		wait_on_bh();
}

/*
 * This is called when we want to synchronize with
 * interrupts. We may for example tell a device to
 * stop sending interrupts: but to make sure there
 * are no interrupts that are executing on another
 * CPU we need to call this function.
 */
void synchronize_irq(void)
{
	if (atomic_read(&global_irq_count)) {
		/* Stupid approach */
		cli();
		sti();
	}
}

static inline void get_irqlock(int cpu)
{
	if (atomic_compare_and_swap(0, 1, &global_irq_lock) != 0) {
		/* do we already hold the lock? */
		if ( cpu == atomic_read(&global_irq_holder))
			return;
		/* Uhhuh.. Somebody else got it. Wait.. */
		do {
			check_smp_invalidate(cpu);
		} while (atomic_compare_and_swap(0, 1, &global_irq_lock) != 0);
	}
	/*
	 * We also to make sure that nobody else is running
	 * in an interrupt context.
	 */
	wait_on_irq(cpu);

	/*
	 * Ok, finally..
	 */
	atomic_set(&global_irq_holder,cpu);
}

#define EFLAGS_I_SHIFT 25

/*
 * A global "cli()" while in an interrupt context
 * turns into just a local cli(). Interrupts
 * should use spinlocks for the (very unlikely)
 * case that they ever want to protect against
 * each other.
 *
 * If we already have local interrupts disabled,
 * this will not turn a local disable into a
 * global one (problems with spinlocks: this makes
 * save_flags+cli+sti usable inside a spinlock).
 */
void __global_cli(void)
{
	unsigned long flags;

	__save_flags(flags);
	if (flags & (1 << EFLAGS_I_SHIFT)) {
		int cpu = smp_processor_id();
		__cli();
		if (!in_irq())
			get_irqlock(cpu);
	}
}

void __global_sti(void)
{

	if (!in_irq())
		release_irqlock(smp_processor_id());
	__sti();
}

/*
 * SMP flags value to restore to:
 * 0 - global cli
 * 1 - global sti
 * 2 - local cli
 * 3 - local sti
 */
unsigned long __global_save_flags(void)
{
	int retval;
	int local_enabled;
	unsigned long flags;

	__save_flags(flags);
	local_enabled = (flags >> EFLAGS_I_SHIFT) & 1;
	/* default to local */
	retval = 2 + local_enabled;

	/* check for global flags if we're not in an interrupt */
	if (!in_irq())
	{
		if (local_enabled)
			retval = 1;
		if (atomic_read(&global_irq_holder)== smp_processor_id())
			retval = 0;
	}
	return retval;
}

void __global_restore_flags(unsigned long flags)
{
	switch (flags) {
	case 0:
		__global_cli();
		break;
	case 1:
		__global_sti();
		break;
	case 2:
		__cli();
		break;
	case 3:
		__sti();
		break;
	default:
		printk("global_restore_flags: %08lx (%08lx)\n",
		       flags, (&flags)[-1]);
	}
}

#endif


void __init init_IRQ(void)
{
        s390_init_IRQ();
}


void free_irq(unsigned int irq, void *dev_id)
{
   s390_free_irq( irq, dev_id);
}


int request_irq( unsigned int   irq,
                 void           (*handler)(int, void *, struct pt_regs *),
                 unsigned long  irqflags,
                 const char    *devname,
                 void          *dev_id)
{
   return( s390_request_irq( irq, handler, irqflags, devname, dev_id ) );

}

void init_irq_proc(void)
{
        /* For now, nothing... */
}

#ifdef CONFIG_SMP
EXPORT_SYMBOL(__global_cli);
EXPORT_SYMBOL(__global_sti);
EXPORT_SYMBOL(__global_save_flags);
EXPORT_SYMBOL(__global_restore_flags);
EXPORT_SYMBOL(global_irq_holder);
EXPORT_SYMBOL(global_irq_lock);
EXPORT_SYMBOL(global_irq_count);
EXPORT_SYMBOL(global_bh_count);
#endif

EXPORT_SYMBOL(global_bh_lock);
