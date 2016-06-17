/*
 *	linux/arch/ia64/kernel/irq.c
 *
 *	Copyright (C) 1992, 1998 Linus Torvalds, Ingo Molnar
 *
 * This file contains the code used by various IRQ handling routines:
 * asking for different IRQ's should be done through these routines
 * instead of just grabbing them. Thus setups with different IRQ numbers
 * shouldn't result in any weird surprises, and installing new handlers
 * should be easier.
 */

/*
 * (mostly architecture independent, will move to kernel/irq.c in 2.5.)
 *
 * IRQs are in fact implemented a bit like signal handlers for the kernel.
 * Naturally it's not a 1:1 relation, but there are similarities.
 */

#include <linux/config.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/irq.h>
#include <linux/proc_fs.h>

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/smp.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>
#include <asm/delay.h>
#include <asm/irq.h>



/*
 * Linux has a controller-independent x86 interrupt architecture.
 * every controller has a 'controller-template', that is used
 * by the main code to do the right thing. Each driver-visible
 * interrupt source is transparently wired to the apropriate
 * controller. Thus drivers need not be aware of the
 * interrupt-controller.
 *
 * Various interrupt controllers we handle: 8259 PIC, SMP IO-APIC,
 * PIIX4's internal 8259 PIC and SGI's Visual Workstation Cobalt (IO-)APIC.
 * (IO-APICs assumed to be messaging to Pentium local-APICs)
 *
 * the code is designed to be easily extended with new/different
 * interrupt controllers, without having to do assembly magic.
 */

/*
 * Controller mappings for all interrupt sources:
 */
irq_desc_t _irq_desc[NR_IRQS] __cacheline_aligned =
	{ [0 ... NR_IRQS-1] = { IRQ_DISABLED, &no_irq_type, NULL, 0, SPIN_LOCK_UNLOCKED}};

#ifdef CONFIG_IA64_GENERIC
struct irq_desc *
__ia64_irq_desc (unsigned int irq)
{
	return _irq_desc + irq;
}

ia64_vector
__ia64_irq_to_vector (unsigned int irq)
{
	return (ia64_vector) irq;
}

unsigned int
__ia64_local_vector_to_irq (ia64_vector vec)
{
	return (unsigned int) vec;
}

#endif

static void register_irq_proc (unsigned int irq);

/*
 * Special irq handlers.
 */

void no_action(int cpl, void *dev_id, struct pt_regs *regs) { }

/*
 * Generic no controller code
 */

static void enable_none(unsigned int irq) { }
static unsigned int startup_none(unsigned int irq) { return 0; }
static void disable_none(unsigned int irq) { }
static void ack_none(unsigned int irq)
{
/*
 * 'what should we do if we get a hw irq event on an illegal vector'.
 * each architecture has to answer this themselves, it doesnt deserve
 * a generic callback i think.
 */
#if CONFIG_X86
	printk(KERN_ERR "unexpected IRQ trap at vector %02x\n", irq);
#ifdef CONFIG_X86_LOCAL_APIC
	/*
	 * Currently unexpected vectors happen only on SMP and APIC.
	 * We _must_ ack these because every local APIC has only N
	 * irq slots per priority level, and a 'hanging, unacked' IRQ
	 * holds up an irq slot - in excessive cases (when multiple
	 * unexpected vectors occur) that might lock up the APIC
	 * completely.
	 */
	ack_APIC_irq();
#endif
#endif
#if CONFIG_IA64
	printk(KERN_ERR "Unexpected irq vector 0x%x on CPU %u!\n", irq, smp_processor_id());
#endif
}

/* startup is the same as "enable", shutdown is same as "disable" */
#define shutdown_none	disable_none
#define end_none	enable_none

struct hw_interrupt_type no_irq_type = {
	"none",
	startup_none,
	shutdown_none,
	enable_none,
	disable_none,
	ack_none,
	end_none
};

atomic_t irq_err_count;
#if defined(CONFIG_X86) && defined(CONFIG_X86_IO_APIC) && defined(APIC_MISMATCH_DEBUG)
atomic_t irq_mis_count;
#endif

/*
 * Generic, controller-independent functions:
 */

int get_irq_list(char *buf)
{
	int i, j;
	struct irqaction * action;
	irq_desc_t *idesc;
	char *p = buf;

	p += sprintf(p, "           ");
	for (j=0; j<smp_num_cpus; j++)
		p += sprintf(p, "CPU%d       ",j);
	*p++ = '\n';

	for (i = 0 ; i < NR_IRQS ; i++) {
		idesc = irq_desc(i);
		action = idesc->action;
		if (!action)
			continue;
		p += sprintf(p, "%3d: ",i);
#ifndef CONFIG_SMP
		p += sprintf(p, "%10u ", kstat_irqs(i));
#else
		for (j = 0; j < smp_num_cpus; j++)
			p += sprintf(p, "%10u ",
				kstat.irqs[cpu_logical_map(j)][i]);
#endif
		p += sprintf(p, " %14s", idesc->handler->typename);
		p += sprintf(p, "  %s", action->name);

		for (action=action->next; action; action = action->next)
			p += sprintf(p, ", %s", action->name);
		*p++ = '\n';
	}
	p += sprintf(p, "NMI: ");
	for (j = 0; j < smp_num_cpus; j++)
		p += sprintf(p, "%10u ",
			nmi_count(cpu_logical_map(j)));
	p += sprintf(p, "\n");
#if defined(CONFIG_SMP) && defined(CONFIG_X86)
	p += sprintf(p, "LOC: ");
	for (j = 0; j < smp_num_cpus; j++)
		p += sprintf(p, "%10u ",
			apic_timer_irqs[cpu_logical_map(j)]);
	p += sprintf(p, "\n");
#endif
	p += sprintf(p, "ERR: %10u\n", atomic_read(&irq_err_count));
#if defined(CONFIG_X86) && defined(CONFIG_X86_IO_APIC) && defined(APIC_MISMATCH_DEBUG)
	p += sprintf(p, "MIS: %10u\n", atomic_read(&irq_mis_count));
#endif
	return p - buf;
}


/*
 * Global interrupt locks for SMP. Allow interrupts to come in on any
 * CPU, yet make cli/sti act globally to protect critical regions..
 */

#ifdef CONFIG_SMP
unsigned int global_irq_holder = NO_PROC_ID;
unsigned volatile long global_irq_lock; /* pedantic: long for set_bit --RR */

extern void show_stack(unsigned long* esp);

static void show(char * str)
{
	int i;
	int cpu = smp_processor_id();

	printk("\n%s, CPU %d:\n", str, cpu);
	printk("irq:  %d [",irqs_running());
	for(i=0;i < smp_num_cpus;i++)
		printk(" %d",irq_count(i));
	printk(" ]\nbh:   %d [",spin_is_locked(&global_bh_lock) ? 1 : 0);
	for(i=0;i < smp_num_cpus;i++)
		printk(" %d",bh_count(i));

	printk(" ]\nStack dumps:");
#if defined(CONFIG_IA64)
	/*
	 * We can't unwind the stack of another CPU without access to
	 * the registers of that CPU.  And sending an IPI when we're
	 * in a potentially wedged state doesn't sound like a smart
	 * idea.
	 */
#elif defined(CONFIG_X86)
	for(i=0;i< smp_num_cpus;i++) {
		unsigned long esp;
		if(i==cpu)
			continue;
		printk("\nCPU %d:",i);
		esp = init_tss[i].esp0;
		if(esp==NULL) {
			/* tss->esp0 is set to NULL in cpu_init(),
			 * it's initialized when the cpu returns to user
			 * space. -- manfreds
			 */
			printk(" <unknown> ");
			continue;
		}
		esp &= ~(THREAD_SIZE-1);
		esp += sizeof(struct task_struct);
		show_stack((void*)esp);
	}
#else
	You lose...
#endif
	printk("\nCPU %d:",cpu);
	show_stack(NULL);
	printk("\n");
}

#define MAXCOUNT 100000000

/*
 * I had a lockup scenario where a tight loop doing
 * spin_unlock()/spin_lock() on CPU#1 was racing with
 * spin_lock() on CPU#0. CPU#0 should have noticed spin_unlock(), but
 * apparently the spin_unlock() information did not make it
 * through to CPU#0 ... nasty, is this by design, do we have to limit
 * 'memory update oscillation frequency' artificially like here?
 *
 * Such 'high frequency update' races can be avoided by careful design, but
 * some of our major constructs like spinlocks use similar techniques,
 * it would be nice to clarify this issue. Set this define to 0 if you
 * want to check whether your system freezes.  I suspect the delay done
 * by SYNC_OTHER_CORES() is in correlation with 'snooping latency', but
 * i thought that such things are guaranteed by design, since we use
 * the 'LOCK' prefix.
 */
#define SUSPECTED_CPU_OR_CHIPSET_BUG_WORKAROUND 0

#if SUSPECTED_CPU_OR_CHIPSET_BUG_WORKAROUND
# define SYNC_OTHER_CORES(x) udelay(x+1)
#else
/*
 * We have to allow irqs to arrive between __sti and __cli
 */
# ifdef CONFIG_IA64
#  define SYNC_OTHER_CORES(x) __asm__ __volatile__ ("nop 0")
# else
#  define SYNC_OTHER_CORES(x) __asm__ __volatile__ ("nop")
# endif
#endif

static inline void wait_on_irq(void)
{
	int count = MAXCOUNT;

	for (;;) {

		/*
		 * Wait until all interrupts are gone. Wait
		 * for bottom half handlers unless we're
		 * already executing in one..
		 */
		if (!irqs_running())
			if (really_local_bh_count() || !spin_is_locked(&global_bh_lock))
				break;

		/* Duh, we have to loop. Release the lock to avoid deadlocks */
		smp_mb__before_clear_bit();	/* need barrier before releasing lock... */
		clear_bit(0,&global_irq_lock);

		for (;;) {
			if (!--count) {
				show("wait_on_irq");
				count = ~0;
			}
			__sti();
			SYNC_OTHER_CORES(smp_processor_id());
			__cli();
			if (irqs_running())
				continue;
			if (global_irq_lock)
				continue;
			if (!really_local_bh_count() && spin_is_locked(&global_bh_lock))
				continue;
			if (!test_and_set_bit(0,&global_irq_lock))
				break;
		}
	}
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
	if (irqs_running()) {
		/* Stupid approach */
		cli();
		sti();
	}
}

static inline void get_irqlock(void)
{
	if (test_and_set_bit(0,&global_irq_lock)) {
		/* do we already hold the lock? */
		if (smp_processor_id() == global_irq_holder)
			return;
		/* Uhhuh.. Somebody else got it. Wait.. */
		do {
			do {
#ifdef CONFIG_X86
				rep_nop();
#endif
			} while (test_bit(0,&global_irq_lock));
		} while (test_and_set_bit(0,&global_irq_lock));
	}
	/*
	 * We also to make sure that nobody else is running
	 * in an interrupt context.
	 */
	wait_on_irq();

	/*
	 * Ok, finally..
	 */
	global_irq_holder = smp_processor_id();
}

#define EFLAGS_IF_SHIFT 9

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
	unsigned int flags;

#ifdef CONFIG_IA64
	__save_flags(flags);
	if (flags & IA64_PSR_I) {
		__cli();
		if (!really_local_irq_count())
			get_irqlock();
	}
#else
	__save_flags(flags);
	if (flags & (1 << EFLAGS_IF_SHIFT)) {
		__cli();
		if (!really_local_irq_count())
			get_irqlock();
	}
#endif
}

void __global_sti(void)
{
	if (!really_local_irq_count())
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
	int cpu = smp_processor_id();

	__save_flags(flags);
#ifdef CONFIG_IA64
	local_enabled = (flags & IA64_PSR_I) != 0;
#else
	local_enabled = (flags >> EFLAGS_IF_SHIFT) & 1;
#endif
	/* default to local */
	retval = 2 + local_enabled;

	/* check for global flags if we're not in an interrupt */
	if (!really_local_irq_count()) {
		if (local_enabled)
			retval = 1;
		if (global_irq_holder == cpu)
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

/*
 * This should really return information about whether
 * we should do bottom half handling etc. Right now we
 * end up _always_ checking the bottom half, which is a
 * waste of time and is not what some drivers would
 * prefer.
 */
int handle_IRQ_event(unsigned int irq, struct pt_regs * regs, struct irqaction * action)
{
	int status;

	local_irq_enter(irq);

	status = 1;	/* Force the "do bottom halves" bit */

	if (!(action->flags & SA_INTERRUPT))
		__sti();

	do {
		status |= action->flags;
		action->handler(irq, action->dev_id, regs);
		action = action->next;
	} while (action);
	if (status & SA_SAMPLE_RANDOM)
		add_interrupt_randomness(irq);
	__cli();

	local_irq_exit(irq);

	return status;
}

/**
 *	disable_irq_nosync - disable an irq without waiting
 *	@irq: Interrupt to disable
 *
 *	Disable the selected interrupt line.  Disables and Enables are
 *	nested.
 *	Unlike disable_irq(), this function does not ensure existing
 *	instances of the IRQ handler have completed before returning.
 *
 *	This function may be called from IRQ context.
 */

inline void disable_irq_nosync(unsigned int irq)
{
	irq_desc_t *desc = irq_desc(irq);
	unsigned long flags;

	spin_lock_irqsave(&desc->lock, flags);
	if (!desc->depth++) {
		desc->status |= IRQ_DISABLED;
		desc->handler->disable(irq);
	}
	spin_unlock_irqrestore(&desc->lock, flags);
}

/**
 *	disable_irq - disable an irq and wait for completion
 *	@irq: Interrupt to disable
 *
 *	Disable the selected interrupt line.  Enables and Disables are
 *	nested.
 *	This function waits for any pending IRQ handlers for this interrupt
 *	to complete before returning. If you use this function while
 *	holding a resource the IRQ handler may need you will deadlock.
 *
 *	This function may be called - with care - from IRQ context.
 */

void disable_irq(unsigned int irq)
{
	disable_irq_nosync(irq);

#ifdef CONFIG_SMP
	if (!really_local_irq_count()) {
		do {
			barrier();
		} while (irq_desc(irq)->status & IRQ_INPROGRESS);
	}
#endif
}

/**
 *	enable_irq - enable handling of an irq
 *	@irq: Interrupt to enable
 *
 *	Undoes the effect of one call to disable_irq().  If this
 *	matches the last disable, processing of interrupts on this
 *	IRQ line is re-enabled.
 *
 *	This function may be called from IRQ context.
 */

void enable_irq(unsigned int irq)
{
	irq_desc_t *desc = irq_desc(irq);
	unsigned long flags;

	spin_lock_irqsave(&desc->lock, flags);
	switch (desc->depth) {
	case 1: {
		unsigned int status = desc->status & ~IRQ_DISABLED;
		desc->status = status;
		if ((status & (IRQ_PENDING | IRQ_REPLAY)) == IRQ_PENDING) {
			desc->status = status | IRQ_REPLAY;
			hw_resend_irq(desc->handler,irq);
		}
		desc->handler->enable(irq);
		/* fall-through */
	}
	default:
		desc->depth--;
		break;
	case 0:
		printk(KERN_ERR "enable_irq(%u) unbalanced from %p\n",
		       irq, (void *) __builtin_return_address(0));
	}
	spin_unlock_irqrestore(&desc->lock, flags);
}

/*
 * do_IRQ handles all normal device IRQ's (the special
 * SMP cross-CPU interrupts have their own specific
 * handlers).
 */
unsigned int do_IRQ(unsigned long irq, struct pt_regs *regs)
{
	/*
	 * We ack quickly, we don't want the irq controller
	 * thinking we're snobs just because some other CPU has
	 * disabled global interrupts (we have already done the
	 * INT_ACK cycles, it's too late to try to pretend to the
	 * controller that we aren't taking the interrupt).
	 *
	 * 0 return value means that this irq is already being
	 * handled by some other CPU. (or is disabled)
	 */
	int cpu = smp_processor_id();
	irq_desc_t *desc = irq_desc(irq);
	struct irqaction * action;
	unsigned int status;

	kstat.irqs[cpu][irq]++;

	if (desc->status & IRQ_PER_CPU) {
		/* no locking required for CPU-local interrupts: */
		desc->handler->ack(irq);
		handle_IRQ_event(irq, regs, desc->action);
		desc->handler->end(irq);
	} else {
		spin_lock(&desc->lock);
		desc->handler->ack(irq);
		/*
		 * REPLAY is when Linux resends an IRQ that was dropped earlier
		 * WAITING is used by probe to mark irqs that are being tested
		 */
		status = desc->status & ~(IRQ_REPLAY | IRQ_WAITING);
		status |= IRQ_PENDING; /* we _want_ to handle it */

		/*
		 * If the IRQ is disabled for whatever reason, we cannot
		 * use the action we have.
		 */
		action = NULL;
		if (!(status & (IRQ_DISABLED | IRQ_INPROGRESS))) {
			action = desc->action;
			status &= ~IRQ_PENDING; /* we commit to handling */
			status |= IRQ_INPROGRESS; /* we are handling it */
		}
		desc->status = status;

		/*
		 * If there is no IRQ handler or it was disabled, exit early.
		 * Since we set PENDING, if another processor is handling
		 * a different instance of this same irq, the other processor
		 * will take care of it.
		 */
		if (!action)
			goto out;

		/*
		 * Edge triggered interrupts need to remember
		 * pending events.
		 * This applies to any hw interrupts that allow a second
		 * instance of the same irq to arrive while we are in do_IRQ
		 * or in the handler. But the code here only handles the _second_
		 * instance of the irq, not the third or fourth. So it is mostly
		 * useful for irq hardware that does not mask cleanly in an
		 * SMP environment.
		 */
		for (;;) {
			spin_unlock(&desc->lock);
			handle_IRQ_event(irq, regs, action);
			spin_lock(&desc->lock);

			if (!(desc->status & IRQ_PENDING))
				break;
			desc->status &= ~IRQ_PENDING;
		}
		desc->status &= ~IRQ_INPROGRESS;
	  out:
		/*
		 * The ->end() handler has to deal with interrupts which got
		 * disabled while the handler was running.
		 */
		desc->handler->end(irq);
		spin_unlock(&desc->lock);
	}
	return 1;
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

int request_irq(unsigned int irq,
		void (*handler)(int, void *, struct pt_regs *),
		unsigned long irqflags,
		const char * devname,
		void *dev_id)
{
	int retval;
	struct irqaction * action;

#if 1
	/*
	 * Sanity-check: shared interrupts should REALLY pass in
	 * a real dev-ID, otherwise we'll have trouble later trying
	 * to figure out which interrupt is which (messes up the
	 * interrupt freeing logic etc).
	 */
	if (irqflags & SA_SHIRQ) {
		if (!dev_id)
			printk(KERN_ERR "Bad boy: %s called us without a dev_id!\n", devname);
	}
#endif

	if (irq >= NR_IRQS)
		return -EINVAL;
	if (!handler)
		return -EINVAL;

	action = (struct irqaction *)
			kmalloc(sizeof(struct irqaction), GFP_KERNEL);
	if (!action)
		return -ENOMEM;

	action->handler = handler;
	action->flags = irqflags;
	action->mask = 0;
	action->name = devname;
	action->next = NULL;
	action->dev_id = dev_id;

	retval = setup_irq(irq, action);
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
 *	on the card it drives before calling this function. The function
 *	does not return until any executing interrupts for this IRQ
 *	have completed.
 *
 *	This function may be called from interrupt context. 
 *
 *	Bugs: Attempting to free an irq in a handler for the same irq hangs
 *	      the machine.
 */

void free_irq(unsigned int irq, void *dev_id)
{
	irq_desc_t *desc;
	struct irqaction **p;
	unsigned long flags;

	if (irq >= NR_IRQS)
		return;

	desc = irq_desc(irq);
	spin_lock_irqsave(&desc->lock,flags);
	p = &desc->action;
	for (;;) {
		struct irqaction * action = *p;
		if (action) {
			struct irqaction **pp = p;
			p = &action->next;
			if (action->dev_id != dev_id)
				continue;

			/* Found it - now remove it from the list of entries */
			*pp = action->next;
			if (!desc->action) {
				desc->status |= IRQ_DISABLED;
				desc->handler->shutdown(irq);
			}
			spin_unlock_irqrestore(&desc->lock,flags);

#ifdef CONFIG_SMP
			/* Wait to make sure it's not being used on another CPU */
			while (desc->status & IRQ_INPROGRESS)
				barrier();
#endif
			kfree(action);
			return;
		}
		printk(KERN_ERR "Trying to free free IRQ%d\n",irq);
		spin_unlock_irqrestore(&desc->lock,flags);
		return;
	}
}

/*
 * IRQ autodetection code..
 *
 * This depends on the fact that any interrupt that
 * comes in on to an unassigned handler will get stuck
 * with "IRQ_WAITING" cleared and the interrupt
 * disabled.
 */

static DECLARE_MUTEX(probe_sem);

/**
 *	probe_irq_on	- begin an interrupt autodetect
 *
 *	Commence probing for an interrupt. The interrupts are scanned
 *	and a mask of potential interrupt lines is returned.
 *
 */

unsigned long probe_irq_on(void)
{
	unsigned int i;
	irq_desc_t *desc;
	unsigned long val;
	unsigned long delay;

	down(&probe_sem);
	/*
	 * something may have generated an irq long ago and we want to
	 * flush such a longstanding irq before considering it as spurious.
	 */
	for (i = NR_IRQS-1; i > 0; i--)  {
		desc = irq_desc(i);

		spin_lock_irq(&desc->lock);
		if (!desc->action)
			desc->handler->startup(i);
		spin_unlock_irq(&desc->lock);
	}

	/* Wait for longstanding interrupts to trigger. */
	for (delay = jiffies + HZ/50; time_after(delay, jiffies); )
		/* about 20ms delay */ synchronize_irq();

	/*
	 * enable any unassigned irqs
	 * (we must startup again here because if a longstanding irq
	 * happened in the previous stage, it may have masked itself)
	 */
	for (i = NR_IRQS-1; i > 0; i--) {
		desc = irq_desc(i);

		spin_lock_irq(&desc->lock);
		if (!desc->action) {
			desc->status |= IRQ_AUTODETECT | IRQ_WAITING;
			if (desc->handler->startup(i))
				desc->status |= IRQ_PENDING;
		}
		spin_unlock_irq(&desc->lock);
	}

	/*
	 * Wait for spurious interrupts to trigger
	 */
	for (delay = jiffies + HZ/10; time_after(delay, jiffies); )
		/* about 100ms delay */ synchronize_irq();

	/*
	 * Now filter out any obviously spurious interrupts
	 */
	val = 0;
	for (i = 0; i < NR_IRQS; i++) {
		irq_desc_t *desc = irq_desc(i);
		unsigned int status;

		spin_lock_irq(&desc->lock);
		status = desc->status;

		if (status & IRQ_AUTODETECT) {
			/* It triggered already - consider it spurious. */
			if (!(status & IRQ_WAITING)) {
				desc->status = status & ~IRQ_AUTODETECT;
				desc->handler->shutdown(i);
			} else
				if (i < 32)
					val |= 1 << i;
		}
		spin_unlock_irq(&desc->lock);
	}

	return val;
}

/**
 *	probe_irq_mask - scan a bitmap of interrupt lines
 *	@val:	mask of interrupts to consider
 *
 *	Scan the ISA bus interrupt lines and return a bitmap of
 *	active interrupts. The interrupt probe logic state is then
 *	returned to its previous value.
 *
 *	Note: we need to scan all the irq's even though we will
 *	only return ISA irq numbers - just so that we reset them
 *	all to a known state.
 */

unsigned int probe_irq_mask(unsigned long val)
{
	int i;
	unsigned int mask;

	mask = 0;
	for (i = 0; i < 16; i++) {
		irq_desc_t *desc = irq_desc(i);
		unsigned int status;

		spin_lock_irq(&desc->lock);
		status = desc->status;

		if (status & IRQ_AUTODETECT) {
			if (!(status & IRQ_WAITING))
				mask |= 1 << i;

			desc->status = status & ~IRQ_AUTODETECT;
			desc->handler->shutdown(i);
		}
		spin_unlock_irq(&desc->lock);
	}
	up(&probe_sem);

	return mask & val;
}

/**
 *	probe_irq_off	- end an interrupt autodetect
 *	@val: mask of potential interrupts (unused)
 *
 *	Scans the unused interrupt lines and returns the line which
 *	appears to have triggered the interrupt. If no interrupt was
 *	found then zero is returned. If more than one interrupt is
 *	found then minus the first candidate is returned to indicate
 *	their is doubt.
 *
 *	The interrupt probe logic state is returned to its previous
 *	value.
 *
 *	BUGS: When used in a module (which arguably shouldnt happen)
 *	nothing prevents two IRQ probe callers from overlapping. The
 *	results of this are non-optimal.
 */

int probe_irq_off(unsigned long val)
{
	int i, irq_found, nr_irqs;

	nr_irqs = 0;
	irq_found = 0;
	for (i = 0; i < NR_IRQS; i++) {
		irq_desc_t *desc = irq_desc(i);
		unsigned int status;

		spin_lock_irq(&desc->lock);
		status = desc->status;

		if (status & IRQ_AUTODETECT) {
			if (!(status & IRQ_WAITING)) {
				if (!nr_irqs)
					irq_found = i;
				nr_irqs++;
			}
			desc->status = status & ~IRQ_AUTODETECT;
			desc->handler->shutdown(i);
		}
		spin_unlock_irq(&desc->lock);
	}
	up(&probe_sem);

	if (nr_irqs > 1)
		irq_found = -irq_found;
	return irq_found;
}

int setup_irq(unsigned int irq, struct irqaction * new)
{
	int shared = 0;
	unsigned long flags;
	struct irqaction *old, **p;
	irq_desc_t *desc = irq_desc(irq);

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

	if (new->flags & SA_PERCPU_IRQ) {
		desc->status |= IRQ_PER_CPU;
		desc->handler = &irq_type_ia64_lsapic;
	}

	/*
	 * The following block of code has to be executed atomically
	 */
	spin_lock_irqsave(&desc->lock,flags);
	p = &desc->action;
	if ((old = *p) != NULL) {
		/* Can't share interrupts unless both agree to */
		if (!(old->flags & new->flags & SA_SHIRQ)) {
			spin_unlock_irqrestore(&desc->lock,flags);
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
		desc->depth = 0;
		desc->status &= ~(IRQ_DISABLED | IRQ_AUTODETECT | IRQ_WAITING | IRQ_INPROGRESS);
		desc->handler->startup(irq);
	}
	spin_unlock_irqrestore(&desc->lock,flags);

	register_irq_proc(irq);
	return 0;
}

static struct proc_dir_entry * root_irq_dir;
static struct proc_dir_entry * irq_dir [NR_IRQS];

#define HEX_DIGITS 8

static unsigned int parse_hex_value (const char *buffer,
		unsigned long count, unsigned long *ret)
{
	unsigned char hexnum [HEX_DIGITS];
	unsigned long value;
	int i;

	if (!count)
		return -EINVAL;
	if (count > HEX_DIGITS)
		count = HEX_DIGITS;
	if (copy_from_user(hexnum, buffer, count))
		return -EFAULT;

	/*
	 * Parse the first 8 characters as a hex string, any non-hex char
	 * is end-of-string. '00e1', 'e1', '00E1', 'E1' are all the same.
	 */
	value = 0;

	for (i = 0; i < count; i++) {
		unsigned int c = hexnum[i];

		switch (c) {
			case '0' ... '9': c -= '0'; break;
			case 'a' ... 'f': c -= 'a'-10; break;
			case 'A' ... 'F': c -= 'A'-10; break;
		default:
			goto out;
		}
		value = (value << 4) | c;
	}
out:
	*ret = value;
	return 0;
}

#if CONFIG_SMP

static struct proc_dir_entry * smp_affinity_entry [NR_IRQS];

static unsigned long irq_affinity [NR_IRQS] = { [0 ... NR_IRQS-1] = ~0UL };
static char irq_redir [NR_IRQS]; // = { [0 ... NR_IRQS-1] = 1 };

void set_irq_affinity_info(int irq, int hwid, int redir)
{
	unsigned long mask = 1UL<<cpu_logical_id(hwid);

	if (irq >= 0 && irq < NR_IRQS) {
		irq_affinity[irq] = mask;
		irq_redir[irq] = (char) (redir & 0xff);
	}
}

static int irq_affinity_read_proc (char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	if (count < HEX_DIGITS+3)
		return -EINVAL;
	return sprintf (page, "%s%08lx\n", irq_redir[(long)data] ? "r " : "",
			irq_affinity[(long)data]);
}

static int irq_affinity_write_proc (struct file *file, const char *buffer,
					unsigned long count, void *data)
{
	int irq = (long) data, full_count = count, err;
	unsigned long new_value;
	const char *buf = buffer;
	int redir;

	if (!irq_desc(irq)->handler->set_affinity)
		return -EIO;

	if (buf[0] == 'r' || buf[0] == 'R') {
		++buf;
		while (*buf == ' ') ++buf;
		redir = 1;
	} else
		redir = 0;

	err = parse_hex_value(buf, count, &new_value);

	/*
	 * Do not allow disabling IRQs completely - it's a too easy
	 * way to make the system unusable accidentally :-) At least
	 * one online CPU still has to be targeted.
	 */
	if (!(new_value & cpu_online_map))
		return -EINVAL;

	irq_desc(irq)->handler->set_affinity(irq | (redir? IA64_IRQ_REDIRECTED :0), new_value);

	return full_count;
}

#endif /* CONFIG_SMP */

static int prof_cpu_mask_read_proc (char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	unsigned long *mask = (unsigned long *) data;
	if (count < HEX_DIGITS+1)
		return -EINVAL;
	return sprintf (page, "%08lx\n", *mask);
}

static int prof_cpu_mask_write_proc (struct file *file, const char *buffer,
					unsigned long count, void *data)
{
	unsigned long *mask = (unsigned long *) data, full_count = count, err;
	unsigned long new_value;

	err = parse_hex_value(buffer, count, &new_value);
	if (err)
		return err;

	*mask = new_value;
	return full_count;
}

#define MAX_NAMELEN 10

static void register_irq_proc (unsigned int irq)
{
	char name [MAX_NAMELEN];

	if (!root_irq_dir || (irq_desc(irq)->handler == &no_irq_type) || irq_dir[irq])
		return;

	memset(name, 0, MAX_NAMELEN);
	sprintf(name, "%d", irq);

	/* create /proc/irq/1234 */
	irq_dir[irq] = proc_mkdir(name, root_irq_dir);

#if CONFIG_SMP
	{
		struct proc_dir_entry *entry;
		/* create /proc/irq/1234/smp_affinity */
		entry = create_proc_entry("smp_affinity", 0600, irq_dir[irq]);

		if (entry) {
			entry->nlink = 1;
			entry->data = (void *)(long)irq;
			entry->read_proc = irq_affinity_read_proc;
			entry->write_proc = irq_affinity_write_proc;
		}

		smp_affinity_entry[irq] = entry;
	}
#endif
}

unsigned long prof_cpu_mask = -1;

void init_irq_proc (void)
{
	struct proc_dir_entry *entry;
	int i;

	/* create /proc/irq */
	root_irq_dir = proc_mkdir("irq", 0);

	/* create /proc/irq/prof_cpu_mask */
	entry = create_proc_entry("prof_cpu_mask", 0600, root_irq_dir);

	if (!entry)
		return;

	entry->nlink = 1;
	entry->data = (void *)&prof_cpu_mask;
	entry->read_proc = prof_cpu_mask_read_proc;
	entry->write_proc = prof_cpu_mask_write_proc;

	/*
	 * Create entries for all existing IRQs.
	 */
	for (i = 0; i < NR_IRQS; i++) {
		if (irq_desc(i)->handler == &no_irq_type)
			continue;
		register_irq_proc(i);
	}
}
