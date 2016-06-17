/*
 $	$Id: pci-dc.c,v 1.5 2001/08/24 12:38:19 dwmw2 Exp $
 *	Dreamcast PCI: Supports SEGA Broadband Adaptor only.
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/pci.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/dc_sysasic.h>

#define	GAPSPCI_REGS		0x01001400
#define GAPSPCI_DMA_BASE	0x01840000
#define GAPSPCI_DMA_SIZE	32768
#define GAPSPCI_BBA_CONFIG	0x01001600

#define	GAPSPCI_IRQ		HW_EVENT_EXTERNAL

static int gapspci_dma_used;

static struct pci_bus *pci_root_bus;

struct pci_fixup pcibios_fixups[] = {
	{0, 0, 0, NULL}
};

#define BBA_SELECTED(dev) (dev->bus->number==0 && dev->devfn==0)

static int gapspci_read_config_byte(struct pci_dev *dev, int where,
                                    u8 * val)
{
	if (BBA_SELECTED(dev))
		*val = inb(GAPSPCI_BBA_CONFIG+where);
	else
                *val = 0xff;

	return PCIBIOS_SUCCESSFUL;
}

static int gapspci_read_config_word(struct pci_dev *dev, int where,
                                    u16 * val)
{
        if (BBA_SELECTED(dev))
		*val = inw(GAPSPCI_BBA_CONFIG+where);
	else
                *val = 0xffff;

        return PCIBIOS_SUCCESSFUL;
}

static int gapspci_read_config_dword(struct pci_dev *dev, int where,
                                     u32 * val)
{
        if (BBA_SELECTED(dev))
		*val = inl(GAPSPCI_BBA_CONFIG+where);
	else
                *val = 0xffffffff;

        return PCIBIOS_SUCCESSFUL;
}

static int gapspci_write_config_byte(struct pci_dev *dev, int where,
                                     u8 val)
{
        if (BBA_SELECTED(dev))
		outb(val, GAPSPCI_BBA_CONFIG+where);

        return PCIBIOS_SUCCESSFUL;
}


static int gapspci_write_config_word(struct pci_dev *dev, int where,
                                     u16 val)
{
        if (BBA_SELECTED(dev))
		outw(val, GAPSPCI_BBA_CONFIG+where);

        return PCIBIOS_SUCCESSFUL;
}

static int gapspci_write_config_dword(struct pci_dev *dev, int where,
                                      u32 val)
{
        if (BBA_SELECTED(dev))
		outl(val, GAPSPCI_BBA_CONFIG+where);

        return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops pci_config_ops = {
        gapspci_read_config_byte,
        gapspci_read_config_word,
        gapspci_read_config_dword,
        gapspci_write_config_byte,
        gapspci_write_config_word,
        gapspci_write_config_dword
};


void *pci_alloc_consistent(struct pci_dev *hwdev, size_t size,
			   dma_addr_t * dma_handle)
{
	unsigned long buf;

	if (gapspci_dma_used+size > GAPSPCI_DMA_SIZE)
		return NULL;

	buf = GAPSPCI_DMA_BASE+gapspci_dma_used;

	gapspci_dma_used = PAGE_ALIGN(gapspci_dma_used+size);
	
	printk("pci_alloc_consistent: %ld bytes at 0x%lx\n", (long)size, buf);

	*dma_handle = (dma_addr_t)buf;

	return (void *)P2SEGADDR(buf);
}


void pci_free_consistent(struct pci_dev *hwdev, size_t size,
			 void *vaddr, dma_addr_t dma_handle)
{
	/* XXX */
	gapspci_dma_used = 0;
}


void __init pcibios_fixup_pbus_ranges(struct pci_bus *bus, struct pbus_set_ranges_data *ranges)
{
}                                                                                

void __init pcibios_fixup_bus(struct pci_bus *bus)
{
	struct list_head *ln;
	struct pci_dev *dev;

	for (ln=bus->devices.next; ln != &bus->devices; ln=ln->next) {
		dev = pci_dev_b(ln);
		if (!BBA_SELECTED(dev)) continue;

		printk("PCI: MMIO fixup to %s\n", dev->name);
		dev->resource[1].start=0x01001700;
		dev->resource[1].end=0x010017ff;
	}
}


static u8 __init no_swizzle(struct pci_dev *dev, u8 * pin)
{
	return PCI_SLOT(dev->devfn);
}


static int __init map_dc_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	return GAPSPCI_IRQ;
}


void __init pcibios_init(void)
{
	pci_root_bus = pci_scan_bus(0, &pci_config_ops, NULL);
	/* pci_assign_unassigned_resources(); */
	pci_fixup_irqs(no_swizzle, map_dc_irq);
}


/* Haven't done anything here as yet */
char * __init pcibios_setup(char *str)
{
	return str;
}


int __init gapspci_init(void)
{
	int i;
	char idbuf[16];

	for(i=0; i<16; i++)
		idbuf[i]=inb(GAPSPCI_REGS+i);

	if(strncmp(idbuf, "GAPSPCI_BRIDGE_2", 16))
		return -1;

	outl(0x5a14a501, GAPSPCI_REGS+0x18);

	for(i=0; i<1000000; i++);

	if(inl(GAPSPCI_REGS+0x18)!=1)
		return -1;

	outl(0x01000000, GAPSPCI_REGS+0x20);
	outl(0x01000000, GAPSPCI_REGS+0x24);

	outl(GAPSPCI_DMA_BASE, GAPSPCI_REGS+0x28);
	outl(GAPSPCI_DMA_BASE+GAPSPCI_DMA_SIZE, GAPSPCI_REGS+0x2c);

	outl(1, GAPSPCI_REGS+0x14);
	outl(1, GAPSPCI_REGS+0x34);

	gapspci_dma_used=0;

	/* Setting Broadband Adapter */
	outw(0xf900, GAPSPCI_BBA_CONFIG+0x06);
	outl(0x00000000, GAPSPCI_BBA_CONFIG+0x30);
	outb(0x00, GAPSPCI_BBA_CONFIG+0x3c);
	outb(0xf0, GAPSPCI_BBA_CONFIG+0x0d);
	outw(0x0006, GAPSPCI_BBA_CONFIG+0x04);
	outl(0x00002001, GAPSPCI_BBA_CONFIG+0x10);
	outl(0x01000000, GAPSPCI_BBA_CONFIG+0x14);

	return 0;
}
