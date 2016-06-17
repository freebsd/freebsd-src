/*
 * linux/arch/arm/mach-footbridge/netwinder-pci.c
 *
 * PCI bios-type initialisation for PCI machines
 *
 * Bits taken from various places.
 */
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>

#include <asm/irq.h>
#include <asm/mach/pci.h>
#include <asm/hardware/dec21285.h>

/* netwinder host-specific stuff */
static int __init netwinder_map_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
#define DEV(v,d) ((v)<<16|(d))
	switch (DEV(dev->vendor, dev->device)) {
	case DEV(PCI_VENDOR_ID_DEC, PCI_DEVICE_ID_DEC_21142):
	case DEV(PCI_VENDOR_ID_NCR, PCI_DEVICE_ID_NCR_53C885):
	case DEV(PCI_VENDOR_ID_NCR, PCI_DEVICE_ID_NCR_YELLOWFIN):
		return IRQ_NETWINDER_ETHER100;

	case DEV(PCI_VENDOR_ID_WINBOND2, 0x5a5a):
		return IRQ_NETWINDER_ETHER10;

	case DEV(PCI_VENDOR_ID_WINBOND, PCI_DEVICE_ID_WINBOND_83C553):
		return 0;

	case DEV(PCI_VENDOR_ID_WINBOND, PCI_DEVICE_ID_WINBOND_82C105):
		return IRQ_ISA_HARDDISK1;

	case DEV(PCI_VENDOR_ID_INTERG, PCI_DEVICE_ID_INTERG_2000):
	case DEV(PCI_VENDOR_ID_INTERG, PCI_DEVICE_ID_INTERG_2010):
	case DEV(PCI_VENDOR_ID_INTERG, PCI_DEVICE_ID_INTERG_5000):
		return IRQ_NETWINDER_VGA;

	case DEV(PCI_VENDOR_ID_DEC, PCI_DEVICE_ID_DEC_21285):
		return 0;

	default:
		printk(KERN_ERR "PCI: %02X:%02X [%04X:%04X] unknown device\n",
			dev->bus->number, dev->devfn,
			dev->vendor, dev->device);
		return 0;
	}
}

struct hw_pci netwinder_pci __initdata = {
	.setup_resources	= dc21285_setup_resources,
	.init			= dc21285_init,
	.mem_offset		= DC21285_PCI_MEM,
	.swizzle		= no_swizzle,
	.map_irq		= netwinder_map_irq,
};
