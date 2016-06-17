/*
 *  linux/arch/arm/kernel/irq.c
 *
 *  Copyright (C) 1992 Linus Torvalds
 *  Modifications for ARM processor Copyright (C) 1995-2000 Russell King.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This file contains the code used by various IRQ handling routines:
 *  asking for different IRQ's should be done through these routines
 *  instead of just grabbing them. Thus setups with different IRQ numbers
 *  shouldn't result in any weird surprises, and installing new handlers
 *  should be easier.
 *
 *  IRQ's are in fact implemented a bit like signal handlers for the kernel.
 *  Naturally it's not a 1:1 relation, but there are similarities.
 */
#include <linux/config.h>
#include <linux/ptrace.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/smp.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>

#include <asm/irq.h>
#include <asm/system.h>
#include <asm/mach/irq.h>

#include <asm/arch/irq.h>	/* pick up fixup_irq definition */

/*
 * Maximum IRQ count.  Currently, this is arbitary.  However, it should
 * not be set too low to prevent false triggering.  Conversely, if it
 * is set too high, then you could miss a stuck IRQ.
 *
 * Maybe we ought to set a timer and re-enable the IRQ at a later time?
 */
#define MAX_IRQ_CNT	100000

static volatile unsigned long irq_err_count;
static spinlock_t irq_controller_lock;
static LIST_HEAD(irq_pending);

struct irqdesc irq_desc[NR_IRQS];
void (*init_arch_irq)(void) __initdata = NULL;

/*
 * Dummy mask/unmask handler
 */
static void dummy_mask_unmask_irq(unsigned int irq)
{
}

/*
 * No architecture-specific irq_finish function defined in arm/arch/irq.h.
 */
#ifndef irq_finish
#define irq_finish(irq) do { } while (0)
#endif

/**
 *	disable_irq - disable an irq and wait for completion
 *	@irq: Interrupt to disable
 *
 *	Disable the selected interrupt line.  We do this lazily.
 *
 *	This function may be called from IRQ context.
 */
void disable_irq(unsigned int irq)
{
	struct irqdesc *desc = irq_desc + irq;
	unsigned long flags;

	spin_lock_irqsave(&irq_controller_lock, flags);
	if (!desc->disable_depth++) {
#ifndef CONFIG_CPU_SA1100
		desc->mask(irq);
#endif
	}
	spin_unlock_irqrestore(&irq_controller_lock, flags);
}

/**
 *	enable_irq - enable interrupt handling on an irq
 *	@irq: Interrupt to enable
 *
 *	Re-enables the processing of interrupts on this IRQ line.
 *	Note that this may call the interrupt handler, so you may
 *	get unexpected results if you hold IRQs disabled.
 *
 *	This function may be called from IRQ context.
 */
void enable_irq(unsigned int irq)
{
	struct irqdesc *desc = irq_desc + irq;
	unsigned long flags;

	spin_lock_irqsave(&irq_controller_lock, flags);
	if (!desc->disable_depth) {
		printk("enable_irq(%u) unbalanced from %p\n", irq,
			__builtin_return_address(0));
	} else if (!--desc->disable_depth) {
		desc->probing = 0;
		desc->unmask(irq);

		/*
		 * If the interrupt is waiting to be processed,
		 * try to re-run it.  We can't directly run it
		 * from here since the caller might be in an
		 * interrupt-protected region.
		 */
		if (desc->pending) {
			desc->pending = 0;
			if (list_empty(&desc->pend))
				list_add(&desc->pend, &irq_pending);
		}
	}
	spin_unlock_irqrestore(&irq_controller_lock, flags);
}

int get_irq_list(char *buf)
{
	int i;
	struct irqaction * action;
	char *p = buf;

	for (i = 0 ; i < NR_IRQS ; i++) {
	    	action = irq_desc[i].action;
		if (!action)
			continue;
		p += sprintf(p, "%3d: %10u ", i, kstat_irqs(i));
		p += sprintf(p, "  %s", action->name);
		for (action = action->next; action; action = action->next) {
			p += sprintf(p, ", %s", action->name);
		}
		*p++ = '\n';
	}

#ifdef CONFIG_ARCH_ACORN
	p += get_fiq_list(p);
#endif
	p += sprintf(p, "Err: %10lu\n", irq_err_count);
	return p - buf;
}

/*
 * IRQ lock detection.
 *
 * Hopefully, this should get us out of a few locked situations.
 * However, it may take a while for this to happen, since we need
 * a large number if IRQs to appear in the same jiffie with the
 * same instruction pointer (or within 2 instructions).
 */
static int check_irq_lock(struct irqdesc *desc, int irq, struct pt_regs *regs)
{
	unsigned long instr_ptr = instruction_pointer(regs);

	if (desc->lck_jif == jiffies &&
	    desc->lck_pc >= instr_ptr && desc->lck_pc < instr_ptr + 8) {
		desc->lck_cnt += 1;

		if (desc->lck_cnt > MAX_IRQ_CNT) {
			if (!desc->lck_warned++)
				printk(KERN_ERR "IRQ LOCK: IRQ%d is locking the system, disabled\n", irq);
			mod_timer(&desc->lck_timer, jiffies + 10*HZ);
			return 1;
		}
	} else {
		desc->lck_cnt = 0;
		desc->lck_pc  = instruction_pointer(regs);
		desc->lck_jif = jiffies;
		if (desc->lck_warned < 0)
			desc->lck_warned ++;
	}
	return 0;
}

static void
__do_irq(unsigned int irq, struct irqaction *action, struct pt_regs *regs)
{
	unsigned int status;

	spin_unlock(&irq_controller_lock);

	if (!(action->flags & SA_INTERRUPT))
		local_irq_enable();

	status = 0;
	do {
		status |= action->flags;
		action->handler(irq, action->dev_id, regs);
		action = action->next;
	} while (action);

	if (status & SA_SAMPLE_RANDOM)
		add_interrupt_randomness(irq);

	spin_lock_irq(&irq_controller_lock);
}

/*
 * do_IRQ handles all normal device IRQ's
 */
void do_IRQ(int irq, struct pt_regs * regs)
{
	struct irqdesc *desc = irq_desc + irq;

	desc->triggered = 1;

	/*
	 * Acknowledge and clear the IRQ, but (if its
	 * a level-based IRQ, don't mask it)
	 */
	desc->mask_ack(irq);

	/*
	 * If we're currently running this IRQ, or its disabled,
	 * we shouldn't process the IRQ.  Instead, turn on the
	 * hardware masks.
	 */
	if (desc->running || desc->disable_depth)
		goto running;

	/*
	 * Mark the IRQ currently in progress.
	 */
	desc->running = 1;

	kstat.irqs[smp_processor_id()][irq]++;

	do {
		struct irqaction *action;

		action = desc->action;
		if (!action)
			break;

		if (desc->pending && desc->disable_depth == 0) {
			desc->pending = 0;
			desc->unmask(irq);
		}

		__do_irq(irq, action, regs);
	} while (desc->pending && desc->disable_depth == 0);

	desc->running = 0;

	/*
	 * If we are disabled or freed, shut down the handler.
	 */
	if (desc->action && !check_irq_lock(desc, irq, regs))
		desc->unmask(irq);
	return;

 running:
	/*
	 * We got another IRQ while this one was masked or
	 * currently running.  Delay it.
	 */
	desc->pending = 1;
}

static void do_pending_irqs(struct pt_regs *regs)
{
	struct list_head head, *l, *n;

	do {
		struct irqdesc *desc;

		/*
		 * First, take the pending interrupts off the list.
		 * The act of calling the handlers may add some IRQs
		 * back onto the list.
		 */
		head = irq_pending;
		INIT_LIST_HEAD(&irq_pending);
		head.next->prev = &head;
		head.prev->next = &head;

		/*
		 * Now run each entry.  We must delete it from our
		 * list before calling the handler.
		 */
		list_for_each_safe(l, n, &head) {
			desc = list_entry(l, struct irqdesc, pend);
			list_del_init(&desc->pend);
			do_IRQ(desc - irq_desc, regs);
		}

		/*
		 * The list must be empty.
		 */
		BUG_ON(!list_empty(&head));
	} while (!list_empty(&irq_pending));
}

/*
 * do_IRQ handles all hardware IRQ's.  Decoded IRQs should not
 * come via this function.  Instead, they should provide their
 * own 'handler'
 */
asmlinkage void asm_do_IRQ(int irq, struct pt_regs *regs)
{
	irq = fixup_irq(irq);

	/*
	 * Some hardware gives randomly wrong interrupts.  Rather
	 * than crashing, do something sensible.
	 */
	if (irq < NR_IRQS) {
		int cpu = smp_processor_id();

		irq_enter(cpu, irq);
		spin_lock(&irq_controller_lock);
		do_IRQ(irq, regs);

		/*
		 * Now re-run any pending interrupts.
		 */
		if (!list_empty(&irq_pending))
			do_pending_irqs(regs);

		spin_unlock(&irq_controller_lock);
		irq_exit(cpu, irq);

		if (softirq_pending(cpu))
			do_softirq();

		irq_finish(irq);
		return;
	}

	irq_err_count += 1;
	printk(KERN_ERR "IRQ: spurious interrupt %d\n", irq);

	irq_finish(irq);
	return;
}

static void irqlck_timeout(unsigned long _data)
{
	struct irqdesc *desc = (struct irqdesc *)_data;

	spin_lock(&irq_controller_lock);

	del_timer(&desc->lck_timer);

	desc->lck_cnt = 0;
	desc->lck_pc  = 0;
	desc->lck_jif = 0;
	desc->lck_warned = -10;

	if (desc->disable_depth == 0)
		desc->unmask(desc - irq_desc);

	spin_unlock(&irq_controller_lock);
}

#ifdef CONFIG_ARCH_ACORN
void do_ecard_IRQ(int irq, struct pt_regs *regs)
{
	struct irqdesc * desc;
	struct irqaction * action;
	int cpu;

	desc = irq_desc + irq;

	cpu = smp_processor_id();
	kstat.irqs[cpu][irq]++;
	desc->triggered = 1;

	action = desc->action;

	if (action) {
		do {
			action->handler(irq, action->dev_id, regs);
			action = action->next;
		} while (action);
	} else {
		spin_lock(&irq_controller_lock);
		desc->mask(irq);
		spin_unlock(&irq_controller_lock);
	}
}
#endif

int setup_arm_irq(int irq, struct irqaction * new)
{
	int shared = 0;
	struct irqaction *old, **p;
	unsigned long flags;
	struct irqdesc *desc;

	/*
	 * Some drivers like serial.c use request_irq() heavily,
	 * so we have to be careful not to interfere with a
	 * running system.
	 */
	if (new->flags & SA_SAMPLE_RANDOM) {
		/*
		 * This function might sleep, we want to call it first,
		 * outside of the atomic block.
		 * Yes, this might clear the entropy pool if the wrong
		 * driver is attempted to be loaded, without actually
		 * installing a new handler, but is this really a problem,
		 * only the sysadmin is able to do this.
		 */
	        rand_initialize_irq(irq);
	}

	/*
	 * The following block of code has to be executed atomically
	 */
	desc = irq_desc + irq;
	spin_lock_irqsave(&irq_controller_lock, flags);
	p = &desc->action;
	if ((old = *p) != NULL) {
		/* Can't share interrupts unless both agree to */
		if (!(old->flags & new->flags & SA_SHIRQ)) {
			spin_unlock_irqrestore(&irq_controller_lock, flags);
			return -EBUSY;
		}

		/* add new interrupt at end of irq queue */
		do {
			p = &old->next;
			old = *p;
		} while (old);
		shared = 1;
	}

	*p = new;

	if (!shared) {
		desc->probing = 0;
		desc->running = 0;
		desc->pending = 0;
		desc->disable_depth = 1;
		if (!desc->noautoenable) {
			desc->disable_depth = 0;
			desc->unmask(irq);
		}
	}

	spin_unlock_irqrestore(&irq_controller_lock, flags);
	return 0;
}

/**
 *	request_irq - allocate an interrupt line
 *	@irq: Interrupt line to allocate
 *	@handler: Function to be called when the IRQ occurs
 *	@irqflags: Interrupt type flags
 *	@devname: An ascii name for the claiming device
 *	@dev_id: A cookie passed back to the handler function
 *
 *	This call allocates interrupt resources and enables the
 *	interrupt line and IRQ handling. From the point this
 *	call is made your handler function may be invoked. Since
 *	your handler function must clear any interrupt the board
 *	raises, you must take care both to initialise your hardware
 *	and to set up the interrupt handler in the right order.
 *
 *	Dev_id must be globally unique. Normally the address of the
 *	device data structure is used as the cookie. Since the handler
 *	receives this value it makes sense to use it.
 *
 *	If your interrupt is shared you must pass a non NULL dev_id
 *	as this is required when freeing the interrupt.
 *
 *	Flags:
 *
 *	SA_SHIRQ		Interrupt is shared
 *
 *	SA_INTERRUPT		Disable local interrupts while processing
 *
 *	SA_SAMPLE_RANDOM	The interrupt can be used for entropy
 *
 */
int request_irq(unsigned int irq, void (*handler)(int, void *, struct pt_regs *),
		 unsigned long irq_flags, const char * devname, void *dev_id)
{
	unsigned long retval;
	struct irqaction *action;

	if (irq >= NR_IRQS || !irq_desc[irq].valid || !handler ||
	    (irq_flags & SA_SHIRQ && !dev_id))
		return -EINVAL;

	action = (struct irqaction *)kmalloc(sizeof(struct irqaction), GFP_KERNEL);
	if (!action)
		return -ENOMEM;

	action->handler = handler;
	action->flags = irq_flags;
	action->mask = 0;
	action->name = devname;
	action->next = NULL;
	action->dev_id = dev_id;

	retval = setup_arm_irq(irq, action);

	if (retval)
		kfree(action);
	return retval;
}

/**
 *	free_irq - free an interrupt
 *	@irq: Interrupt line to free
 *	@dev_id: Device identity to free
 *
 *	Remove an interrupt handler. The handler is removed and if the
 *	interrupt line is no longer in use by any driver it is disabled.
 *	On a shared IRQ the caller must ensure the interrupt is disabled
 *	on the card it drives before calling this function.
 *
 *	This function must not be called from interrupt context.
 */
void free_irq(unsigned int irq, void *dev_id)
{
	struct irqaction * action, **p;
	unsigned long flags;

	if (irq >= NR_IRQS || !irq_desc[irq].valid) {
		printk(KERN_ERR "Trying to free IRQ%d\n",irq);
#ifdef CONFIG_DEBUG_ERRORS
		__backtrace();
#endif
		return;
	}

	spin_lock_irqsave(&irq_controller_lock, flags);
	for (p = &irq_desc[irq].action; (action = *p) != NULL; p = &action->next) {
		if (action->dev_id != dev_id)
			continue;

	    	/* Found it - now free it */
		*p = action->next;
		kfree(action);
		goto out;
	}
	printk(KERN_ERR "Trying to free free IRQ%d\n",irq);
#ifdef CONFIG_DEBUG_ERRORS
	__backtrace();
#endif
out:
	spin_unlock_irqrestore(&irq_controller_lock, flags);
}

static DECLARE_MUTEX(probe_sem);

/* Start the interrupt probing.  Unlike other architectures,
 * we don't return a mask of interrupts from probe_irq_on,
 * but return the number of interrupts enabled for the probe.
 * The interrupts which have been enabled for probing is
 * instead recorded in the irq_desc structure.
 */
unsigned long probe_irq_on(void)
{
	unsigned int i, irqs = 0;
	unsigned long delay;

	down(&probe_sem);

	/*
	 * first snaffle up any unassigned but
	 * probe-able interrupts
	 */
	spin_lock_irq(&irq_controller_lock);
	for (i = 0; i < NR_IRQS; i++) {
		if (!irq_desc[i].probe_ok || irq_desc[i].action)
			continue;

		irq_desc[i].probing = 1;
		irq_desc[i].triggered = 0;
		irq_desc[i].unmask(i);
		irqs += 1;
	}
	spin_unlock_irq(&irq_controller_lock);

	/*
	 * wait for spurious interrupts to mask themselves out again
	 */
	for (delay = jiffies + HZ/10; time_before(jiffies, delay); )
		/* min 100ms delay */;

	/*
	 * now filter out any obviously spurious interrupts
	 */
	spin_lock_irq(&irq_controller_lock);
	for (i = 0; i < NR_IRQS; i++) {
		if (irq_desc[i].probing && irq_desc[i].triggered) {
			irq_desc[i].probing = 0;
			irqs -= 1;
		}
	}
	spin_unlock_irq(&irq_controller_lock);

	return irqs;
}

unsigned int probe_irq_mask(unsigned long irqs)
{
	unsigned int mask = 0, i;

	spin_lock_irq(&irq_controller_lock);
	for (i = 0; i < 16 && i < NR_IRQS; i++)
		if (irq_desc[i].probing && irq_desc[i].triggered)
			mask |= 1 << i;
	spin_unlock_irq(&irq_controller_lock);

	up(&probe_sem);

	return mask;
}

/*
 * Possible return values:
 *  >= 0 - interrupt number
 *    -1 - no interrupt/many interrupts
 */
int probe_irq_off(unsigned long irqs)
{
	unsigned int i;
	int irq_found = NO_IRQ;

	/*
	 * look at the interrupts, and find exactly one
	 * that we were probing has been triggered
	 */
	spin_lock_irq(&irq_controller_lock);
	for (i = 0; i < NR_IRQS; i++) {
		if (irq_desc[i].probing &&
		    irq_desc[i].triggered) {
			if (irq_found != NO_IRQ) {
				irq_found = NO_IRQ;
				goto out;
			}
			irq_found = i;
		}
	}

	if (irq_found == -1)
		irq_found = NO_IRQ;
out:
	spin_unlock_irq(&irq_controller_lock);

	up(&probe_sem);

	return irq_found;
}

void __init init_irq_proc(void)
{
}

void __init init_IRQ(void)
{
	extern void init_dma(void);
	int irq;

	for (irq = 0; irq < NR_IRQS; irq++) {
		irq_desc[irq].disable_depth = 1;
		irq_desc[irq].probe_ok = 0;
		irq_desc[irq].valid    = 0;
		irq_desc[irq].noautoenable = 0;
		irq_desc[irq].mask_ack = dummy_mask_unmask_irq;
		irq_desc[irq].mask     = dummy_mask_unmask_irq;
		irq_desc[irq].unmask   = dummy_mask_unmask_irq;
		INIT_LIST_HEAD(&irq_desc[irq].pend);
		init_timer(&irq_desc[irq].lck_timer);
		irq_desc[irq].lck_timer.data = (unsigned long)&irq_desc[irq];
		irq_desc[irq].lck_timer.function = irqlck_timeout;
	}

	init_arch_irq();
	init_dma();
}
