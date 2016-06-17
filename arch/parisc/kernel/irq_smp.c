/*
 *  linux/arch/parisc/kernel/irq_smp.c
 *  (90% stolen from alpha port, 9% from ia64, rest is mine -ggg)
 *  
 *  Copyright (C) 2001 Hewlett-Packard Co
 *  Copyright (C) 2001 Grant Grundler <grundler@puffin.external.hp.com>
 *
 */

#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>

#include <asm/system.h>
#include <asm/io.h>


int global_irq_holder = NO_PROC_ID;	/* Who has global_irq_lock. */
spinlock_t global_irq_lock = SPIN_LOCK_UNLOCKED; /* protects IRQ's. */


/* Global IRQ locking depth. */
static void *previous_irqholder = NULL;

#define MAXCOUNT 100000000


static void
show(char * str, void *where)
{
	int cpu = smp_processor_id();

	printk("\n%s, CPU %d: %p\n", str, cpu, where);
	printk("irq:  %d [%d %d]\n",
		irqs_running(),
		local_irq_count(0),
		local_irq_count(1));

	printk("bh:   %d [%d %d]\n",
		spin_is_locked(&global_bh_lock) ? 1 : 0,
		local_bh_count(0),
		local_bh_count(1));
}

static inline void
wait_on_irq(int cpu, void *where)
{
	int count = MAXCOUNT;

	for (;;) {

		/*
		 * Wait until all interrupts are gone. Wait
		 * for bottom half handlers unless we're
		 * already executing in one..
		 */
		if (!irqs_running()) {
			if (local_bh_count(cpu)
			    || !spin_is_locked(&global_bh_lock))
				break;
		}

		/* Duh, we have to loop. Release the lock to avoid deadlocks */
		spin_unlock(&global_irq_lock);

		for (;;) {
			if (!--count) {
				show("wait_on_irq", where);
				count = MAXCOUNT;
			}
			__sti();
			udelay(1); /* make sure to run pending irqs */
			__cli();

			if (irqs_running())
				continue;
			if (spin_is_locked(&global_irq_lock))
				continue;
			if (!local_bh_count(cpu)
			    && spin_is_locked(&global_bh_lock))
				continue;
			if (spin_trylock(&global_irq_lock))
				break;
		}
	}
}

static inline void
get_irqlock(int cpu, void* where)
{
	if (!spin_trylock(&global_irq_lock)) {
		/* Do we already hold the lock?  */
		if (cpu == global_irq_holder)
			return;
		/* Uhhuh.. Somebody else got it.  Wait.  */
		spin_lock(&global_irq_lock);
	}

	/*
	 * Ok, we got the lock bit.
	 * But that's actually just the easy part.. Now
	 * we need to make sure that nobody else is running
	 * in an interrupt context. 
	 */
	wait_on_irq(cpu, where);

	/*
	 * Finally.
	 */
#if DEBUG_SPINLOCK
	global_irq_lock.task = current;
	global_irq_lock.previous = where;
#endif
	global_irq_holder = cpu;
	previous_irqholder = where;
}


/*
** A global "cli()" while in an interrupt context
** turns into just a local cli(). Interrupts
** should use spinlocks for the (very unlikely)
** case that they ever want to protect against
** each other.
** 
** If we already have local interrupts disabled,
** this will not turn a local disable into a
** global one (problems with spinlocks: this makes
** save_flags+cli+sti usable inside a spinlock).
*/
void
__global_cli(void)
{
	unsigned int flags;
	__save_flags(flags);
	if (flags & PSW_I) {
		int cpu = smp_processor_id();
		__cli(); 
		if (!local_irq_count(cpu)) {
			void *where = __builtin_return_address(0);
			get_irqlock(cpu, where);
		}
	}
}

void
__global_sti(void)
{
	int cpu = smp_processor_id();

	if (!local_irq_count(cpu))
		release_irqlock(cpu);
	__sti();
}

/*
 * SMP flags value to restore to:
 * 0 - global cli
 * 1 - global sti
 * 2 - local cli
 * 3 - local sti
 */
unsigned long
__global_save_flags(void)
{
	int retval;
	int local_enabled;
	unsigned long flags;
	int cpu = smp_processor_id();

	__save_flags(flags);
	local_enabled = (flags & PSW_I) != 0;
	/* default to local */
	retval = 2 + local_enabled;

	/* Check for global flags if we're not in an interrupt.  */
	if (!local_irq_count(cpu)) {
		if (local_enabled)
			retval = 1;
		if (global_irq_holder == cpu)
			retval = 0;
	}
	return retval;
}

void
__global_restore_flags(unsigned long flags)
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
		printk(KERN_ERR "global_restore_flags: %08lx (%p)\n",
			flags, __builtin_return_address(0));
	}
}

/*
 * From its use, I infer that synchronize_irq() stalls a thread until
 * the effects of a command to an external device are known to have
 * taken hold.  Typically, the command is to stop sending interrupts.
 * The strategy here is wait until there is at most one processor
 * (this one) in an irq.  The memory barrier serializes the write to
 * the device and the subsequent accesses of global_irq_count.
 * --jmartin
 */
#define DEBUG_SYNCHRONIZE_IRQ 0

void
synchronize_irq(void)
{
	/* Jay's version.  */
	if (irqs_running()) {
		cli();
		sti();
	}
}
