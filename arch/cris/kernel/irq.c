/*
 *	linux/arch/cris/kernel/irq.c
 *
 *      Copyright (c) 2000, 2001, 2002, 2003 Axis Communications AB
 *
 *      Authors: Bjorn Wesen (bjornw@axis.com)
 *
 * This file contains the code used by various IRQ handling routines:
 * asking for different IRQ's should be done through these routines
 * instead of just grabbing them. Thus setups with different IRQ numbers
 * shouldn't result in any weird surprises, and installing new handlers
 * should be easier.
 *
 * Notice Linux/CRIS: these routines do not care about SMP
 *
 */

/*
 * IRQ's are in fact implemented a bit like signal handlers for the kernel.
 * Naturally it's not a 1:1 relation, but there are similarities.
 */

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
#include <linux/random.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>

#include <asm/svinto.h>

char *hw_bp_msg = "BP 0x%x\n";

static inline void
mask_irq(unsigned int irq_nr)
{
	*R_VECT_MASK_CLR = 1 << irq_nr;
}

static inline void
unmask_irq(unsigned int irq_nr)
{
	*R_VECT_MASK_SET = 1 << irq_nr;
}

void
disable_irq(unsigned int irq_nr)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	mask_irq(irq_nr);
	restore_flags(flags);
}

void
enable_irq(unsigned int irq_nr)
{
	unsigned long flags;

	save_flags(flags);
	cli();
	unmask_irq(irq_nr);
	restore_flags(flags);
}

unsigned long
probe_irq_on()
{
	return 0;
}

int
probe_irq_off(unsigned long x)
{
	return 0;
}

/* vector of shortcut jumps after the irq prologue */
irqvectptr irq_shortcuts[NR_IRQS];

/* don't use set_int_vector, it bypasses the linux interrupt handlers. it is
 * global just so that the kernel gdb can use it.
 */

void
set_int_vector(int n, irqvectptr addr, irqvectptr saddr)
{
	/* remember the shortcut entry point, after the prologue */

	irq_shortcuts[n] = saddr;

	etrax_irv->v[n + 0x20] = (irqvectptr)addr;
}

/* the breakpoint vector is obviously not made just like the normal irq
 * handlers but needs to contain _code_ to jump to addr.
 *
 * the BREAK n instruction jumps to IBR + n * 8
 */

void
set_break_vector(int n, irqvectptr addr)
{
	unsigned short *jinstr = (unsigned short *)&etrax_irv->v[n*2];
	unsigned long *jaddr = (unsigned long *)(jinstr + 1);

	/* if you don't know what this does, do not touch it! */

	*jinstr = 0x0d3f;
	*jaddr = (unsigned long)addr;

	/* 00000026 <clrlop+1a> 3f0d82000000     jump  0x82 */
}


/*
 * This builds up the IRQ handler stubs using some ugly macros in irq.h
 *
 * These macros create the low-level assembly IRQ routines that do all
 * the operations that are needed. They are also written to be fast - and to
 * disable interrupts as little as humanly possible.
 *
 */

/* IRQ0 and 1 are special traps */
void hwbreakpoint(void);
void IRQ1_interrupt(void);
BUILD_TIMER_IRQ(2, 0x04)       /* the timer interrupt is somewhat special */
BUILD_IRQ(3, 0x08)
BUILD_IRQ(4, 0x10)
BUILD_IRQ(5, 0x20)
BUILD_IRQ(6, 0x40)
BUILD_IRQ(7, 0x80)
BUILD_IRQ(8, 0x100)
BUILD_IRQ(9, 0x200)
BUILD_IRQ(10, 0x400)
BUILD_IRQ(11, 0x800)
BUILD_IRQ(12, 0x1000)
BUILD_IRQ(13, 0x2000)
void mmu_bus_fault(void);      /* IRQ 14 is the bus fault interrupt */
void multiple_interrupt(void); /* IRQ 15 is the multiple IRQ interrupt */
BUILD_IRQ(16, 0x10000)
BUILD_IRQ(17, 0x20000)
BUILD_IRQ(18, 0x40000)
BUILD_IRQ(19, 0x80000)
BUILD_IRQ(20, 0x100000)
BUILD_IRQ(21, 0x200000)
BUILD_IRQ(22, 0x400000)
BUILD_IRQ(23, 0x800000)
BUILD_IRQ(24, 0x1000000)
BUILD_IRQ(25, 0x2000000)
/* IRQ 26-30 are reserved */
BUILD_IRQ(31, 0x80000000)

/*
 * Pointers to the low-level handlers
 */

static void (*interrupt[NR_IRQS])(void) = {
	NULL, NULL, IRQ2_interrupt, IRQ3_interrupt,
	IRQ4_interrupt, IRQ5_interrupt, IRQ6_interrupt, IRQ7_interrupt,
	IRQ8_interrupt, IRQ9_interrupt, IRQ10_interrupt, IRQ11_interrupt,
	IRQ12_interrupt, IRQ13_interrupt, NULL, NULL,
	IRQ16_interrupt, IRQ17_interrupt, IRQ18_interrupt, IRQ19_interrupt,
	IRQ20_interrupt, IRQ21_interrupt, IRQ22_interrupt, IRQ23_interrupt,
	IRQ24_interrupt, IRQ25_interrupt, NULL, NULL, NULL, NULL, NULL,
	IRQ31_interrupt
};

static void (*sinterrupt[NR_IRQS])(void) = {
	NULL, NULL, sIRQ2_interrupt, sIRQ3_interrupt,
	sIRQ4_interrupt, sIRQ5_interrupt, sIRQ6_interrupt, sIRQ7_interrupt,
	sIRQ8_interrupt, sIRQ9_interrupt, sIRQ10_interrupt, sIRQ11_interrupt,
	sIRQ12_interrupt, sIRQ13_interrupt, NULL, NULL,
	sIRQ16_interrupt, sIRQ17_interrupt, sIRQ18_interrupt, sIRQ19_interrupt,
	sIRQ20_interrupt, sIRQ21_interrupt, sIRQ22_interrupt, sIRQ23_interrupt,
	sIRQ24_interrupt, sIRQ25_interrupt, NULL, NULL, NULL, NULL, NULL,
	sIRQ31_interrupt
};

static void (*bad_interrupt[NR_IRQS])(void) = {
        NULL, NULL,
	NULL, bad_IRQ3_interrupt,
	bad_IRQ4_interrupt, bad_IRQ5_interrupt,
	bad_IRQ6_interrupt, bad_IRQ7_interrupt,
	bad_IRQ8_interrupt, bad_IRQ9_interrupt,
	bad_IRQ10_interrupt, bad_IRQ11_interrupt,
	bad_IRQ12_interrupt, bad_IRQ13_interrupt,
	NULL, NULL,
	bad_IRQ16_interrupt, bad_IRQ17_interrupt,
	bad_IRQ18_interrupt, bad_IRQ19_interrupt,
	bad_IRQ20_interrupt, bad_IRQ21_interrupt,
	bad_IRQ22_interrupt, bad_IRQ23_interrupt,
	bad_IRQ24_interrupt, bad_IRQ25_interrupt,
	NULL, NULL, NULL, NULL, NULL,
	bad_IRQ31_interrupt
};

/*
 * Initial irq handlers.
 */

static struct irqaction *irq_action[NR_IRQS] = {
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL
};

int get_irq_list(char *buf)
{
	int i, len = 0;
	struct irqaction * action;

	for (i = 0; i < NR_IRQS; i++) {
		action = irq_action[i];
		if (!action)
			continue;
		len += sprintf(buf+len, "%2d: %10u %c %s",
			i, kstat.irqs[0][i],
			(action->flags & SA_INTERRUPT) ? '+' : ' ',
			action->name);
		for (action = action->next; action; action = action->next) {
			len += sprintf(buf+len, ",%s %s",
				(action->flags & SA_INTERRUPT) ? " +" : "",
				action->name);
		}
		len += sprintf(buf+len, "\n");
	}
	return len;
}

/* called by the assembler IRQ entry functions defined in irq.h
 * to dispatch the interrupts to registred handlers
 * interrupts are disabled upon entry - depending on if the
 * interrupt was registred with SA_INTERRUPT or not, interrupts
 * are re-enabled or not.
 */

asmlinkage void do_IRQ(int irq, struct pt_regs * regs)
{
	struct irqaction *action;
	int do_random, cpu;

        cpu = smp_processor_id();
        irq_enter(cpu);
	kstat.irqs[cpu][irq]++;

	action = irq_action[irq];
        if (action) {
                if (!(action->flags & SA_INTERRUPT))
                        __sti();
                action = irq_action[irq];
                do_random = 0;
                do {
                        do_random |= action->flags;
                        action->handler(irq, action->dev_id, regs);
                        action = action->next;
                } while (action);
                if (do_random & SA_SAMPLE_RANDOM)
                        add_interrupt_randomness(irq);
                __cli();
        }
        irq_exit(cpu);

	if (softirq_pending(cpu))
                do_softirq();

        /* unmasking and bottom half handling is done magically for us. */
}

/* this function links in a handler into the chain of handlers for the
 * given irq, and if the irq has never been registred, the appropriate
 * handler is entered into the interrupt vector
 */

int setup_etrax_irq(int irq, struct irqaction * new)
{
	int shared = 0;
	struct irqaction *old, **p;
	unsigned long flags;

	p = irq_action + irq;
	if ((old = *p) != NULL) {
		/* Can't share interrupts unless both agree to */
		if (!(old->flags & new->flags & SA_SHIRQ))
			return -EBUSY;

		/* Can't share interrupts unless both are same type */
		if ((old->flags ^ new->flags) & SA_INTERRUPT)
			return -EBUSY;

		/* add new interrupt at end of irq queue */
		do {
			p = &old->next;
			old = *p;
		} while (old);
		shared = 1;
	}

	if (new->flags & SA_SAMPLE_RANDOM)
		rand_initialize_irq(irq);

	save_flags(flags);
	cli();
	*p = new;

	if (!shared) {
		/* if the irq wasn't registred before, enter it into the vector
		 * table and unmask it physically
		 */
		set_int_vector(irq, interrupt[irq], sinterrupt[irq]);
		unmask_irq(irq);
	}

	restore_flags(flags);
	return 0;
}

/* this function is called by a driver to register an irq handler
 * Valid flags:
 *   SA_INTERRUPT: it's a fast interrupt, handler called with irq disabled and
 *                 no signal checking etc is performed upon exit
 *   SA_SHIRQ:     the interrupt can be shared between different handlers, the
 *                 handler is required to check if the irq was "aimed" at it
 *                 explicitely
 *   SA_RANDOM:    the interrupt will add to the random generators entropy
 */

int request_irq(unsigned int irq,
		void (*handler)(int, void *, struct pt_regs *),
		unsigned long irqflags,
		const char * devname,
		void *dev_id)
{
	int retval;
	struct irqaction * action;

	/* interrupts 0 and 1 are hardware breakpoint and NMI and we can't
	 * support these yet. interrupt 15 is the multiple irq, it's special.
	 */

	if(irq < 2 || irq == 15 || irq >= NR_IRQS)
		return -EINVAL;

	if(!handler)
		return -EINVAL;

	/* allocate and fill in a handler structure and setup the irq */

	action = kmalloc(sizeof *action, GFP_KERNEL);
	if (!action)
		return -ENOMEM;

	action->handler = handler;
	action->flags = irqflags;
	action->mask = 0;
	action->name = devname;
	action->next = NULL;
	action->dev_id = dev_id;

	retval = setup_etrax_irq(irq, action);

	if (retval)
		kfree(action);
	return retval;
}

void free_irq(unsigned int irq, void *dev_id)
{
	struct irqaction * action, **p;
	unsigned long flags;

	if (irq >= NR_IRQS) {
		printk("Trying to free IRQ%d\n",irq);
		return;
	}
	for (p = irq + irq_action; (action = *p) != NULL; p = &action->next) {
		if (action->dev_id != dev_id)
			continue;

		/* Found it - now free it */
		save_flags(flags);
		cli();
		*p = action->next;
		if (!irq_action[irq]) {
			mask_irq(irq);
			set_int_vector(irq, bad_interrupt[irq], 0);
		}
		restore_flags(flags);
		kfree(action);
		return;
	}
	printk("Trying to free free IRQ%d\n",irq);
}

void weird_irq(void)
{
	__asm__("di");
	printk("weird irq\n");
	while(1);
}

/* init_IRQ() is called by start_kernel and is responsible for fixing IRQ masks
 * and setting the irq vector table to point to bad_interrupt ptrs.
 */

void system_call(void);  /* from entry.S */
void do_sigtrap(void); /* from entry.S */
void gdb_handle_breakpoint(void); /* from entry.S */

void __init
init_IRQ(void)
{
	int i;

	/* clear all interrupt masks */

#ifndef CONFIG_SVINTO_SIM
	*R_IRQ_MASK0_CLR = 0xffffffff;
	*R_IRQ_MASK1_CLR = 0xffffffff;
	*R_IRQ_MASK2_CLR = 0xffffffff;
#endif

	*R_VECT_MASK_CLR = 0xffffffff;

	/* clear the shortcut entry points */

	for(i = 0; i < NR_IRQS; i++)
		irq_shortcuts[i] = NULL;

	for (i = 0; i < 256; i++)
		etrax_irv->v[i] = weird_irq;

	/* the entries in the break vector contain actual code to be
	 * executed by the associated break handler, rather than just a jump
	 * address. therefore we need to setup a default breakpoint handler
	 * for all breakpoints
	 */

	for (i = 0; i < 16; i++)
		set_break_vector(i, do_sigtrap);

	/* set all etrax irq's to the bad handlers */
	for (i = 2; i < NR_IRQS; i++)
		set_int_vector(i, bad_interrupt[i], 0);

	/* except IRQ 15 which is the multiple-IRQ handler on Etrax100 */

	set_int_vector(15, multiple_interrupt, 0);

	/* 0 and 1 which are special breakpoint/NMI traps */

	set_int_vector(0, hwbreakpoint, 0);
	set_int_vector(1, IRQ1_interrupt, 0);

	/* and irq 14 which is the mmu bus fault handler */

	set_int_vector(14, mmu_bus_fault, 0);

	/* setup the system-call trap, which is reached by BREAK 13 */

	set_break_vector(13, system_call);

	/* setup a breakpoint handler for debugging used for both user and
	 * kernel mode debugging  (which is why it is not inside an ifdef
	 * CONFIG_ETRAX_KGDB)
	 */
	set_break_vector(8, gdb_handle_breakpoint);

#ifdef CONFIG_ETRAX_KGDB
	/* setup kgdb if its enabled, and break into the debugger */
	kgdb_init();
	breakpoint();
#endif
}

#if defined(CONFIG_PROC_FS) && defined(CONFIG_SYSCTL)
/* Used by other archs to show/control IRQ steering during SMP */
void __init
init_irq_proc(void)
{
}
#endif
