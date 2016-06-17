/*
 *	linux/arch/i386/kernel/visws_apic.c
 *
 *	Copyright (C) 1999 Bent Hagemark, Ingo Molnar
 *
 *  SGI Visual Workstation interrupt controller
 *
 *  The Cobalt system ASIC in the Visual Workstation contains a "Cobalt" APIC
 *  which serves as the main interrupt controller in the system.  Non-legacy
 *  hardware in the system uses this controller directly.  Legacy devices
 *  are connected to the PIIX4 which in turn has its 8259(s) connected to
 *  a of the Cobalt APIC entry.
 */

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
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/smp.h>
#include <asm/pgtable.h>
#include <asm/delay.h>
#include <asm/desc.h>

#include <asm/cobalt.h>

#include <linux/irq.h>

/*
 * This is the PIIX4-based 8259 that is wired up indirectly to Cobalt
 * -- not the manner expected by the normal 8259 code in irq.c.
 *
 * there is a 'master' physical interrupt source that gets sent to
 * the CPU. But in the chipset there are various 'virtual' interrupts
 * waiting to be handled. We represent this to Linux through a 'master'
 * interrupt controller type, and through a special virtual interrupt-
 * controller. Device drivers only see the virtual interrupt sources.
 */

#define	CO_IRQ_BASE	0x20	/* This is the 0x20 in init_IRQ()! */

static void startup_piix4_master_irq(unsigned int irq);
static void shutdown_piix4_master_irq(unsigned int irq);
static void do_piix4_master_IRQ(unsigned int irq, struct pt_regs * regs);
#define enable_piix4_master_irq startup_piix4_master_irq
#define disable_piix4_master_irq shutdown_piix4_master_irq

static struct hw_interrupt_type piix4_master_irq_type = {
	"PIIX4-master",
	startup_piix4_master_irq,
	shutdown_piix4_master_irq,
	do_piix4_master_IRQ,
	enable_piix4_master_irq,
	disable_piix4_master_irq
};

static void enable_piix4_virtual_irq(unsigned int irq);
static void disable_piix4_virtual_irq(unsigned int irq);
#define startup_piix4_virtual_irq enable_piix4_virtual_irq
#define shutdown_piix4_virtual_irq disable_piix4_virtual_irq

static struct hw_interrupt_type piix4_virtual_irq_type = {
	"PIIX4-virtual",
	startup_piix4_virtual_irq,
	shutdown_piix4_virtual_irq,
	0, /* no handler, it's never called physically */
	enable_piix4_virtual_irq,
	disable_piix4_virtual_irq
};

/*
 * This is the SGI Cobalt (IO-)APIC:
 */

static void do_cobalt_IRQ(unsigned int irq, struct pt_regs * regs);
static void enable_cobalt_irq(unsigned int irq);
static void disable_cobalt_irq(unsigned int irq);
static void startup_cobalt_irq(unsigned int irq);
#define shutdown_cobalt_irq disable_cobalt_irq

static spinlock_t irq_controller_lock = SPIN_LOCK_UNLOCKED;

static struct hw_interrupt_type cobalt_irq_type = {
	"Cobalt-APIC",
	startup_cobalt_irq,
	shutdown_cobalt_irq,
	do_cobalt_IRQ,
	enable_cobalt_irq,
	disable_cobalt_irq
};


/*
 * Not an __init, needed by the reboot code
 */
void disable_IO_APIC(void)
{
	/* Nop on Cobalt */
} 

/*
 * Cobalt (IO)-APIC functions to handle PCI devices.
 */

static void disable_cobalt_irq(unsigned int irq)
{
	/* XXX undo the APIC entry here? */

	/*
	 * definitely, we do not want to have IRQ storms from
	 * unused devices --mingo
	 */
}

static void enable_cobalt_irq(unsigned int irq)
{
}

/*
 * Set the given Cobalt APIC Redirection Table entry to point
 * to the given IDT vector/index.
 */
static void co_apic_set(int entry, int idtvec)
{
	co_apic_write(CO_APIC_LO(entry), CO_APIC_LEVEL | (CO_IRQ_BASE+idtvec));
	co_apic_write(CO_APIC_HI(entry), 0);

	printk("Cobalt APIC Entry %d IDT Vector %d\n", entry, idtvec);
}

/*
 * "irq" really just serves to identify the device.  Here is where we
 * map this to the Cobalt APIC entry where it's physically wired.
 * This is called via request_irq -> setup_x86_irq -> irq_desc->startup()
 */
static void startup_cobalt_irq(unsigned int irq)
{
	/*
	 * These "irq"'s are wired to the same Cobalt APIC entries
	 * for all (known) motherboard types/revs
	 */
	switch (irq) {
	case CO_IRQ_TIMER:	co_apic_set(CO_APIC_CPU, CO_IRQ_TIMER);
				return;

	case CO_IRQ_ENET:	co_apic_set(CO_APIC_ENET, CO_IRQ_ENET);
				return;

	case CO_IRQ_SERIAL:	return; /* XXX move to piix4-8259 "virtual" */

	case CO_IRQ_8259:	co_apic_set(CO_APIC_8259, CO_IRQ_8259);
				return;

	case CO_IRQ_IDE:
		switch (visws_board_type) {
		case VISWS_320:
			switch (visws_board_rev) {
			case 5:
				co_apic_set(CO_APIC_0_5_IDE0, CO_IRQ_IDE);
				co_apic_set(CO_APIC_0_5_IDE1, CO_IRQ_IDE);
					return;
			case 6:
				co_apic_set(CO_APIC_0_6_IDE0, CO_IRQ_IDE);
				co_apic_set(CO_APIC_0_6_IDE1, CO_IRQ_IDE);
					return;
			}
		case VISWS_540:
			switch (visws_board_rev) {
			case 2:
				co_apic_set(CO_APIC_1_2_IDE0, CO_IRQ_IDE);
					return;
			}
		}
		break;
	default:
		panic("huh?");
	}
}

/*
 * This is the handle() op in do_IRQ()
 */
static void do_cobalt_IRQ(unsigned int irq, struct pt_regs * regs)
{
	struct irqaction * action;
	irq_desc_t *desc = irq_desc + irq;

	spin_lock(&irq_controller_lock);
	{
		unsigned int status;
		/* XXX APIC EOI? */
		status = desc->status & ~(IRQ_REPLAY | IRQ_WAITING);
		action = NULL;
		if (!(status & (IRQ_DISABLED | IRQ_INPROGRESS))) {
			action = desc->action;
			status |= IRQ_INPROGRESS;
		}
		desc->status = status;
	}
	spin_unlock(&irq_controller_lock);

	/* Exit early if we had no action or it was disabled */
	if (!action)
		return;

	handle_IRQ_event(irq, regs, action);

	(void)co_cpu_read(CO_CPU_REV); /* Sync driver ack to its h/w */
	apic_write(APIC_EOI, APIC_EIO_ACK); /* Send EOI to Cobalt APIC */

	spin_lock(&irq_controller_lock);
	{
		unsigned int status = desc->status & ~IRQ_INPROGRESS;
		desc->status = status;
		if (!(status & IRQ_DISABLED))
			enable_cobalt_irq(irq);
	}
	spin_unlock(&irq_controller_lock);
}

/*
 * PIIX4-8259 master/virtual functions to handle:
 *
 *	floppy
 *	parallel
 *	serial
 *	audio (?)
 *
 * None of these get Cobalt APIC entries, neither do they have IDT
 * entries. These interrupts are purely virtual and distributed from
 * the 'master' interrupt source: CO_IRQ_8259.
 *
 * When the 8259 interrupts its handler figures out which of these
 * devices is interrupting and dispatches to it's handler.
 *
 * CAREFUL: devices see the 'virtual' interrupt only. Thus disable/
 * enable_irq gets the right irq. This 'master' irq is never directly
 * manipulated by any driver.
 */

static void startup_piix4_master_irq(unsigned int irq)
{
	/* ICW1 */
	outb(0x11, 0x20);
	outb(0x11, 0xa0);

	/* ICW2 */
	outb(0x08, 0x21);
	outb(0x70, 0xa1);

	/* ICW3 */
	outb(0x04, 0x21);
	outb(0x02, 0xa1);

	/* ICW4 */
	outb(0x01, 0x21);
	outb(0x01, 0xa1);

	/* OCW1 - disable all interrupts in both 8259's */
	outb(0xff, 0x21);
	outb(0xff, 0xa1);

	startup_cobalt_irq(irq);
}

static void shutdown_piix4_master_irq(unsigned int irq)
{
	/*
	 * [we skip the 8259 magic here, not strictly necessary]
	 */

	shutdown_cobalt_irq(irq);
}

static void do_piix4_master_IRQ(unsigned int irq, struct pt_regs * regs)
{
	int realirq, mask;

	/* Find out what's interrupting in the PIIX4 8259 */

	spin_lock(&irq_controller_lock);
	outb(0x0c, 0x20);		/* OCW3 Poll command */
	realirq = inb(0x20);

	if (!(realirq & 0x80)) {
		/*
		 * Bit 7 == 0 means invalid/spurious
		 */
		goto out_unlock;
	}
	realirq &= 0x7f;

	/*
	 * mask and ack the 8259
	 */
	mask = inb(0x21);
	if ((mask >> realirq) & 0x01)
		/*
		 * This IRQ is masked... ignore
		 */
		goto out_unlock;

	outb(mask | (1<<realirq), 0x21);
	/*
	 * OCW2 - non-specific EOI
	 */
	outb(0x20, 0x20);

	spin_unlock(&irq_controller_lock);

	/*
	 * handle this 'virtual interrupt' as a Cobalt one now.
	 */
	kstat.irqs[smp_processor_id()][irq]++;
	do_cobalt_IRQ(realirq, regs);

	spin_lock(&irq_controller_lock);
	{
		irq_desc_t *desc = irq_desc + realirq;

		if (!(desc->status & IRQ_DISABLED))
			enable_piix4_virtual_irq(realirq);
	}
	spin_unlock(&irq_controller_lock);
	return;

out_unlock:
	spin_unlock(&irq_controller_lock);
	return;
}

static void enable_piix4_virtual_irq(unsigned int irq)
{
	/*
	 * assumes this irq is one of the legacy devices
	 */

	unsigned int mask = inb(0x21);
 	mask &= ~(1 << irq);
	outb(mask, 0x21);
	enable_cobalt_irq(irq);
}

/*
 * assumes this irq is one of the legacy devices
 */
static void disable_piix4_virtual_irq(unsigned int irq)
{
	unsigned int mask;

	disable_cobalt_irq(irq);

	mask = inb(0x21);
 	mask &= ~(1 << irq);
	outb(mask, 0x21);
}

static struct irqaction master_action =
		{ no_action, 0, 0, "PIIX4-8259", NULL, NULL };

void init_VISWS_APIC_irqs(void)
{
	int i;

	for (i = 0; i < 16; i++) {
		irq_desc[i].status = IRQ_DISABLED;
		irq_desc[i].action = 0;
		irq_desc[i].depth = 1;

		/*
		 * Cobalt IRQs are mapped to standard ISA
		 * interrupt vectors:
		 */
		switch (i) {
			/*
			 * Only CO_IRQ_8259 will be raised
			 * externally.
			 */
		case CO_IRQ_8259:
			irq_desc[i].handler = &piix4_master_irq_type;
			break;
		case CO_IRQ_FLOPPY:
		case CO_IRQ_PARLL:
			irq_desc[i].handler = &piix4_virtual_irq_type;
			break;
		default:
			irq_desc[i].handler = &cobalt_irq_type;
			break;
		}
	}

	/*
	 * The master interrupt is always present:
	 */
	setup_x86_irq(CO_IRQ_8259, &master_action);
}

