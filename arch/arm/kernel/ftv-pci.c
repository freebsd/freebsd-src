/*
 *  linux/arch/arm/kernel/ftv-pci.c
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

/*
 * Owing to a PCB cockup, issue A backplanes are wired thus:
 *
 * Slot 1    2    3    4    5   Bridge   S1    S2    S3    S4
 * IRQ  D    C    B    A    A            C     B     A     D
 *      A    D    C    B    B            D     C     B     A
 *      B    A    D    C    C            A     D     C     B
 *      C    B    A    D    D            B     A     D     C
 *
 * ID A31  A30  A29  A28  A27   A26      DEV4  DEV5  DEV6  DEV7
 *
 * Actually, this isn't too bad, because with the processor card
 * in slot 5 on the primary bus, the IRQs rotate on both sides
 * as you'd expect.
 */

static int irqmap_ftv[] __initdata = { IRQ_PCI_D, IRQ_PCI_C, IRQ_PCI_B, IRQ_PCI_A };

static int __init ftv_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	if (slot > 0x10)
		slot--;
	return irqmap_ftv[(slot - pin) & 3];
}

static u8 __init ftv_swizzle(struct pci_dev *dev, u8 *pin)
{
	return PCI_SLOT(dev->devfn);
}

/* ftv host-specific stuff */
struct hw_pci ftv_pci __initdata = {
	.init		= plx90x0_init,
	.swizzle	= ftv_swizzle,
	.map_irq	= ftv_map_irq,
};

