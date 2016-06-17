/*
 *	linux/arch/alpha/kernel/irq_smp.c
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


/* Who has global_irq_lock. */
int global_irq_holder = NO_PROC_ID;

/* This protects IRQ's. */
spinlock_t global_irq_lock = SPIN_LOCK_UNLOCKED;

/* Global IRQ locking depth. */
static void *previous_irqholder = NULL;

#define MAXCOUNT 100000000


static void
show(char * str, void *where)
{
#if 0
	int i;
        unsigned long *stack;
#endif
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
#if 0
        stack = (unsigned long *) &str;
        for (i = 40; i ; i--) {
		unsigned long x = *++stack;
                if (x > (unsigned long) &init_task_union &&
		    x < (unsigned long) &vsprintf) {
			printk("<[%08lx]> ", x);
                }
        }
#endif
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
#ifdef CONFIG_DEBUG_SPINLOCK
	global_irq_lock.task = current;
	global_irq_lock.previous = where;
#endif
	global_irq_holder = cpu;
	previous_irqholder = where;
}

void
__global_cli(void)
{
	int cpu = smp_processor_id();
	void *where = __builtin_return_address(0);

	/*
	 * Maximize ipl.  If ipl was previously 0 and if this thread
	 * is not in an irq, then take global_irq_lock.
	 */
	if (swpipl(IPL_MAX) == IPL_MIN && !local_irq_count(cpu))
		get_irqlock(cpu, where);
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
        local_enabled = (!(flags & 7));
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
#if 0
	/* Joe's version.  */
	int cpu = smp_processor_id();
	int local_count;
	int global_count;
	int countdown = 1<<24;
	void *where = __builtin_return_address(0);

	mb();
	do {
		local_count = local_irq_count(cpu);
		global_count = atomic_read(&global_irq_count);
		if (DEBUG_SYNCHRONIZE_IRQ && (--countdown == 0)) {
			printk("%d:%d/%d\n", cpu, local_count, global_count);
			show("synchronize_irq", where);
			break;
		}
	} while (global_count != local_count);
#else
	/* Jay's version.  */
	if (irqs_running()) {
		cli();
		sti();
	}
#endif
}
