#ifndef BUSDEVICE_H
#define BUSDEVICE_H

#include <linux/types.h>
#include <linux/interrupt.h>
#include <asm/hardware.h>
#include <asm/gsc.h>

#define OFFSET_IRR 0x0000   /* Interrupt request register */
#define OFFSET_IMR 0x0004   /* Interrupt mask register */
#define OFFSET_IPR 0x0008   /* Interrupt pending register */
#define OFFSET_ICR 0x000C   /* Interrupt control register */
#define OFFSET_IAR 0x0010   /* Interrupt address register */


struct busdevice {
	struct parisc_device *gsc;
	unsigned long hpa;
	char *name;
	int version;
	int type;
	int parent_irq;
	int eim;
	struct irq_region *busdev_region;
};

/* short cut to keep the compiler happy */
#define BUSDEV_DEV(x)	((struct busdevice *) (x))

int gsc_common_irqsetup(struct parisc_device *parent, struct busdevice *busdev);

void busdev_barked(int busdev_irq, void *dev, struct pt_regs *regs);

#endif /* BUSDEVICE_H */
