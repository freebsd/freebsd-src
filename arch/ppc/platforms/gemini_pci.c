#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>

#include <asm/machdep.h>
#include <platforms/gemini.h>
#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/pci-bridge.h>

#define pci_config_addr(bus,dev,offset) \
        (0x80000000 | (bus<<16) | (dev<<8) | offset)


int
gemini_pcibios_read_config_byte(struct pci_dev *dev, int offset, u8 *val)
{
	unsigned long reg;
	reg = grackle_read(pci_config_addr(dev->bus->number, dev->devfn,
					   (offset & ~(0x3))));
	*val = ((reg >> ((offset & 0x3) << 3)) & 0xff);
	return PCIBIOS_SUCCESSFUL;
}

int
gemini_pcibios_read_config_word(struct pci_dev *dev, int offset, u16 *val)
{
	unsigned long reg;
	reg = grackle_read(pci_config_addr(dev->bus->number, dev->devfn,
					   (offset & ~(0x3))));
	*val = ((reg >> ((offset & 0x3) << 3)) & 0xffff);
	return PCIBIOS_SUCCESSFUL;
}

int
gemini_pcibios_read_config_dword(struct pci_dev *dev, int offset, u32 *val)
{
	*val = grackle_read(pci_config_addr(dev->bus->number, dev->devfn,
					    (offset & ~(0x3))));
	return PCIBIOS_SUCCESSFUL;
}

int
gemini_pcibios_write_config_byte(struct pci_dev *dev, int offset, u8 val)
{
	unsigned long reg;
	int shifts = offset & 0x3;
	unsigned int addr = pci_config_addr(dev->bus->number, dev->devfn,
					    (offset & ~(0x3)));

	reg = grackle_read(addr);
	reg = (reg & ~(0xff << (shifts << 3))) | (val << (shifts << 3));
	grackle_write(addr, reg );
	return PCIBIOS_SUCCESSFUL;
}

int
gemini_pcibios_write_config_word(struct pci_dev *dev, int offset, u16 val)
{
	unsigned long reg;
	int shifts = offset & 0x3;
	unsigned int addr = pci_config_addr(dev->bus->number, dev->devfn,
					    (offset & ~(0x3)));

	reg = grackle_read(addr);
	reg = (reg & ~(0xffff << (shifts << 3))) | (val << (shifts << 3));
	grackle_write(addr, reg );
	return PCIBIOS_SUCCESSFUL;
}

int
gemini_pcibios_write_config_dword(struct pci_dev *dev, int offset, u32 val)
{
	grackle_write(pci_config_addr(dev->bus->number, dev->devfn,
				      (offset & ~(0x3))), val);
	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops gemini_pci_ops =
{
	gemini_pcibios_read_config_byte,
	gemini_pcibios_read_config_word,
	gemini_pcibios_read_config_dword,
	gemini_pcibios_write_config_byte,
	gemini_pcibios_write_config_word,
	gemini_pcibios_write_config_dword
};

void __init gemini_pcibios_fixup(void)
{
	int i;
	struct pci_dev *dev;

	pci_for_each_dev(dev) {
		for(i = 0; i < 6; i++) {
			if (dev->resource[i].flags & IORESOURCE_IO) {
				dev->resource[i].start |= (0xfe << 24);
				dev->resource[i].end |= (0xfe << 24);
			}
		}
	}
}


/* The "bootloader" for Synergy boards does none of this for us, so we need to
   lay it all out ourselves... --Dan */
void __init gemini_find_bridges(void)
{
	struct pci_controller* hose;

	ppc_md.pcibios_fixup = gemini_pcibios_fixup;

	hose = pcibios_alloc_controller();
	if (!hose)
		return;
	hose->ops = &gemini_pci_ops;
}
