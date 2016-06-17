/*
 * Code to handle Baget/MIPS IRQs plus some generic interrupt stuff.
 *
 * Copyright (C) 1998 Vladimir Roganov & Gleb Raiko
 *      Code (mostly sleleton and comments) derived from DECstation IRQ
 *      handling.
 */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/timex.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/delay.h>

#include <asm/bitops.h>
#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mipsregs.h>
#include <asm/system.h>

#include <asm/baget/baget.h>

volatile unsigned long irq_err_count;

/*
 * This table is a correspondence between IRQ numbers and CPU PILs
 */

static int irq_to_pil_map[BAGET_IRQ_NR] = {
	7/*fixme: dma_err -1*/,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, /* 0x00 - 0x0f */
	-1,-1,-1,-1, 3,-1,-1,-1, 2, 2, 2,-1, 3,-1,-1,3/*fixme: lance*/, /* 0x10 - 0x1f */
        -1,-1,-1,-1,-1,-1, 5,-1,-1,-1,-1,-1, 7,-1,-1,-1, /* 0x20 - 0x2f */
	-1, 3, 2/*fixme systimer:3*/, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3  /* 0x30 - 0x3f */
};

static inline int irq_to_pil(int irq_nr)
{
	int pil = -1;

	if (irq_nr >= BAGET_IRQ_NR)
		baget_printk("irq_to_pil: too large irq_nr = 0x%x\n", irq_nr);
	else {
		pil = irq_to_pil_map[irq_nr];
		if (pil == -1)
			baget_printk("irq_to_pil: unknown irq = 0x%x\n", irq_nr);
	}

	return pil;
}

/* Function for careful CP0 interrupt mask access */

static inline void modify_cp0_intmask(unsigned clr_mask, unsigned set_mask)
{
	unsigned long status = read_c0_status();
	status &= ~((clr_mask & 0xFF) << 8);
	status |=   (set_mask & 0xFF) << 8;
	write_c0_status(status);
}

/*
 *  These two functions may be used for unconditional IRQ
 *  masking via their PIL protection.
 */

static inline void mask_irq(unsigned int irq_nr)
{
        modify_cp0_intmask(irq_to_pil(irq_nr), 0);
}

static inline void unmask_irq(unsigned int irq_nr)
{
	modify_cp0_intmask(0, irq_to_pil(irq_nr));
}

/*
 * The following section is introduced for masking/unasking IRQ
 * only while no more IRQs uses same CPU PIL.
 *
 * These functions are used in request_irq, free_irq, but it looks
 * they cannot change something: CP0_STATUS is private for any
 * process, and their action is invisible for system.
 */

static volatile unsigned int pil_in_use[BAGET_PIL_NR] = { 0, };

void mask_irq_count(int irq_nr)
{
	unsigned long flags;
	int pil = irq_to_pil(irq_nr);

	save_and_cli(flags);
	if (!--pil_in_use[pil])
		mask_irq(irq_nr);
	restore_flags(flags);
}

void unmask_irq_count(int irq_nr)
{
	unsigned long flags;
	int pil = irq_to_pil(irq_nr);

	save_and_cli(flags);
	if (!pil_in_use[pil]++)
		unmask_irq(irq_nr);
	restore_flags(flags);
}

/*
 * Two functions below are exported versions of mask/unmask IRQ
 */

void disable_irq(unsigned int irq_nr)
{
	unsigned long flags;

	save_and_cli(flags);
	mask_irq(irq_nr);
	restore_flags(flags);
}

void enable_irq(unsigned int irq_nr)
{
	unsigned long flags;

	save_and_cli(flags);
	unmask_irq(irq_nr);
	restore_flags(flags);
}

/*
 * Pointers to the low-level handlers: first the general ones, then the
 * fast ones, then the bad ones.
 */
static struct irqaction *irq_action[BAGET_IRQ_NR] = { NULL, };

int get_irq_list(char *buf)
{
	int i, len = 0;
	struct irqaction * action;

	for (i = 0 ; i < BAGET_IRQ_NR ; i++) {
		action = irq_action[i];
		if (!action)
			continue;
		len += sprintf(buf+len, "%2d: %8d %c %s",
			i, kstat.irqs[0][i],
			(action->flags & SA_INTERRUPT) ? '+' : ' ',
			action->name);
		for (action=action->next; action; action = action->next) {
			len += sprintf(buf+len, ",%s %s",
				(action->flags & SA_INTERRUPT) ? " +" : "",
				action->name);
		}
		len += sprintf(buf+len, "\n");
	}
	return len;
}


/*
 * do_IRQ handles IRQ's that have been installed without the
 * SA_INTERRUPT flag: it uses the full signal-handling return
 * and runs with other interrupts enabled. All relatively slow
 * IRQ's should use this format: notably the keyboard/timer
 * routines.
 */
static void do_IRQ(int irq, struct pt_regs * regs)
{
	struct irqaction *action;
	int do_random, cpu;

	cpu = smp_processor_id();
	irq_enter(cpu, irq);
	kstat.irqs[cpu][irq]++;

	mask_irq(irq);
	action = *(irq + irq_action);
	if (action) {
		if (!(action->flags & SA_INTERRUPT))
			__sti();
		action = *(irq + irq_action);
		do_random = 0;
        	do {
			do_random |= action->flags;
			action->handler(irq, action->dev_id, regs);
			action = action->next;
        	} while (action);
		if (do_random & SA_SAMPLE_RANDOM)
			add_interrupt_randomness(irq);
		__cli();
	} else {
		printk("do_IRQ: Unregistered IRQ (0x%X) occurred\n", irq);
	}
	unmask_irq(irq);
	irq_exit(cpu, irq);

	/* unmasking and bottom half handling is done magically for us. */
}

/*
 *  What to do in case of 'no VIC register available' for current interrupt
 */
static void vic_reg_error(unsigned long address, unsigned char active_pils)
{
	printk("\nNo VIC register found: reg=%08lx active_pils=%02x\n"
	       "Current interrupt mask from CP0_CAUSE: %02x\n",
	       address, 0xff & active_pils,
	       0xff & (read_c0_cause()>>8));
	{ int i; for (i=0; i<10000; i++) udelay(1000); }
}

static char baget_fpu_irq = BAGET_FPU_IRQ;
#define BAGET_INT_FPU {(unsigned long)&baget_fpu_irq, 1}

/*
 *  Main interrupt handler: interrupt demultiplexer
 */
asmlinkage void baget_interrupt(struct pt_regs *regs)
{
	static struct baget_int_reg int_reg[BAGET_PIL_NR] = {
		BAGET_INT_NONE, BAGET_INT_NONE, BAGET_INT0_ACK, BAGET_INT1_ACK,
		BAGET_INT_NONE, BAGET_INT_FPU,  BAGET_INT_NONE, BAGET_INT5_ACK
	};
	unsigned char active_pils;
	while ((active_pils = read_c0_cause()>>8)) {
		int pil;
		struct baget_int_reg* reg;

                for (pil = 0; pil < BAGET_PIL_NR; pil++) {
                        if (!(active_pils & (1<<pil))) continue;

			reg = &int_reg[pil];

			if (reg->address) {
                                extern int try_read(unsigned long,int);
				int irq  = try_read(reg->address, reg->size);

				if (irq != -1)
				      do_IRQ(BAGET_IRQ_MASK(irq), regs);
				else
				      vic_reg_error(reg->address, active_pils);
			} else {
				printk("baget_interrupt: unknown interrupt "
				       "(pil = %d)\n", pil);
			}
		}
	}
}

/*
 * Idea is to put all interrupts
 * in a single table and differenciate them just by number.
 */
int setup_baget_irq(int irq, struct irqaction * new)
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

	save_and_cli(flags);
	*p = new;
	restore_flags(flags);

	if (!shared) {
		unmask_irq_count(irq);
	}

	return 0;
}

int request_irq(unsigned int irq,
		void (*handler)(int, void *, struct pt_regs *),
		unsigned long irqflags,
		const char * devname,
		void *dev_id)
{
	int retval;
	struct irqaction * action;

	if (irq >= BAGET_IRQ_NR)
		return -EINVAL;
	if (!handler)
		return -EINVAL;
	if (irq_to_pil_map[irq] < 0)
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

	retval = setup_baget_irq(irq, action);

	if (retval)
		kfree(action);

	return retval;
}

void free_irq(unsigned int irq, void *dev_id)
{
	struct irqaction * action, **p;
	unsigned long flags;

	if (irq >= BAGET_IRQ_NR)
		printk("Trying to free IRQ%d\n",irq);

	for (p = irq + irq_action; (action = *p) != NULL; p = &action->next) {
		if (action->dev_id != dev_id)
			continue;

		/* Found it - now free it */
		save_and_cli(flags);
		*p = action->next;
		if (!irq[irq_action])
			unmask_irq_count(irq);
		restore_flags(flags);
		kfree(action);
		return;
	}
	printk("Trying to free free IRQ%d\n",irq);
}

unsigned long probe_irq_on (void)
{
	/* TODO */
	return 0;
}

int probe_irq_off (unsigned long irqs)
{
	/* TODO */
	return 0;
}


static void write_err_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	*(volatile char*) BAGET_WRERR_ACK = 0;
}

static struct irqaction irq0  =
{ write_err_interrupt, SA_INTERRUPT, 0, "bus write error", NULL, NULL};

void __init init_IRQ(void)
{
	irq_setup();

	/* Enable access to VIC interrupt registers */
	vac_outw(0xacef | 0x8200, VAC_PIO_FUNC);

	/* Enable interrupts for pils 2 and 3 (lines 0 and 1) */
	modify_cp0_intmask(0, (1<<2)|(1<<3));

	if (setup_baget_irq(0, &irq0) < 0)
		printk("init_IRQ: unable to register write_err irq\n");
}
