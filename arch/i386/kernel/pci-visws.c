/*
 *	Low-Level PCI Support for SGI Visual Workstation
 *
 *	(c) 1999--2000 Martin Mares <mj@ucw.cz>
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/irq.h>

#include <asm/smp.h>
#include <asm/lithium.h>
#include <asm/io.h>

#include "pci-i386.h"

unsigned int pci_probe = 0;

/*
 *  The VISWS uses configuration access type 1 only.
 */

#define CONFIG_CMD(dev, where)   (0x80000000 | (dev->bus->number << 16) | (dev->devfn << 8) | (where & ~3))

static int pci_conf1_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	outl(CONFIG_CMD(dev,where), 0xCF8);
	*value = inb(0xCFC + (where&3));
	return PCIBIOS_SUCCESSFUL;
}

static int pci_conf1_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	outl(CONFIG_CMD(dev,where), 0xCF8);    
	*value = inw(0xCFC + (where&2));
	return PCIBIOS_SUCCESSFUL;    
}

static int pci_conf1_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	outl(CONFIG_CMD(dev,where), 0xCF8);
	*value = inl(0xCFC);
	return PCIBIOS_SUCCESSFUL;    
}

static int pci_conf1_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	outl(CONFIG_CMD(dev,where), 0xCF8);    
	outb(value, 0xCFC + (where&3));
	return PCIBIOS_SUCCESSFUL;
}

static int pci_conf1_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	outl(CONFIG_CMD(dev,where), 0xCF8);
	outw(value, 0xCFC + (where&2));
	return PCIBIOS_SUCCESSFUL;
}

static int pci_conf1_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	outl(CONFIG_CMD(dev,where), 0xCF8);
	outl(value, 0xCFC);
	return PCIBIOS_SUCCESSFUL;
}

#undef CONFIG_CMD

static struct pci_ops visws_pci_ops = {
	pci_conf1_read_config_byte,
	pci_conf1_read_config_word,
	pci_conf1_read_config_dword,
	pci_conf1_write_config_byte,
	pci_conf1_write_config_word,
	pci_conf1_write_config_dword
};

static void __init pcibios_fixup_irqs(void)
{
	struct pci_dev *dev, *p;
	u8 pin;
	int irq;

	pci_for_each_dev(dev) {
		pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
		dev->irq = 0;
		if (!pin)
			continue;
		pin--;
		if (dev->bus->parent) {
			p = dev->bus->parent->self;
			pin = (pin + PCI_SLOT(dev->devfn)) % 4;
		} else
			p = dev;
		irq = visws_get_PCI_irq_vector(p->bus->number, PCI_SLOT(p->devfn), pin+1);
		if (irq >= 0)
			dev->irq = irq;
		DBG("PCI IRQ: %s pin %d -> %d\n", dev->slot_name, pin, irq);
	}
}

void __init pcibios_fixup_bus(struct pci_bus *b)
{
	pci_read_bridge_bases(b);
}

#if 0
static struct resource visws_pci_bus_resources[2] = {
	{ "Host bus 1", 0xf4000000, 0xf7ffffff, 0 },
	{ "Host bus 2", 0xf0000000, 0xf3ffffff, 0 }
};
#endif

void __init pcibios_init(void)
{
	unsigned int sec_bus = li_pcib_read16(LI_PCI_BUSNUM) & 0xff;

	pcibios_set_cacheline_size();
	printk("PCI: Probing PCI hardware on host buses 00 and %02x\n", sec_bus);
	pci_scan_bus(0, &visws_pci_ops, NULL);
	pci_scan_bus(sec_bus, &visws_pci_ops, NULL);
	pcibios_fixup_irqs();
	pcibios_resource_survey();
}

char * __init pcibios_setup(char *str)
{
	return str;
}

int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	return pcibios_enable_resources(dev, mask);
}

void __init pcibios_penalize_isa_irq(irq)
{
}
