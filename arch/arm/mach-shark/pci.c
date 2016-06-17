/*
 *  linux/arch/arm/mach-shark/pci.c
 *
 *  PCI bios-type initialisation for PCI machines
 *
 *  Bits taken from various places.
 */
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/irq.h>
#include <asm/mach/pci.h>

static int __init shark_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	if (dev->bus->number == 0)
		if (dev->devfn == 0) return 255;
		else return 11;
	else return 6;
}

struct hw_pci shark_pci __initdata = {
	.init		= via82c505_init,
	.swizzle	= no_swizzle,
	.map_irq	= shark_map_irq,
};
