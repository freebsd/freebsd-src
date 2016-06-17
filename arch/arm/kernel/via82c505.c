#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/ptrace.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/ioport.h>

#include <asm/io.h>
#include <asm/system.h>

#include <asm/mach/pci.h>

#define MAX_SLOTS		7

#define CONFIG_CMD(dev, where)   (0x80000000 | (dev->bus->number << 16) | (dev->devfn << 8) | (where & ~3))

static int
via82c505_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	outl(CONFIG_CMD(dev,where),0xCF8);
	*value=inb(0xCFC + (where&3));
	return PCIBIOS_SUCCESSFUL;
}

static int
via82c505_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	outl(CONFIG_CMD(dev,where),0xCF8);
	*value=inw(0xCFC + (where&2));
	return PCIBIOS_SUCCESSFUL;
}

static int
via82c505_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	outl(CONFIG_CMD(dev,where),0xCF8);
	*value=inl(0xCFC);
	return PCIBIOS_SUCCESSFUL;
}

static int
via82c505_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	outl(CONFIG_CMD(dev,where),0xCF8);
	outb(value, 0xCFC + (where&3));
	return PCIBIOS_SUCCESSFUL;
}

static int
via82c505_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	outl(CONFIG_CMD(dev,where),0xCF8);
	outw(value, 0xCFC + (where&2));
	return PCIBIOS_SUCCESSFUL;
}

static int
via82c505_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	outl(CONFIG_CMD(dev,where),0xCF8);
	outl(value, 0xCFC);
	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops via82c505_ops = {
	via82c505_read_config_byte,
	via82c505_read_config_word,
	via82c505_read_config_dword,
	via82c505_write_config_byte,
	via82c505_write_config_word,
	via82c505_write_config_dword,
};

#ifdef CONFIG_ARCH_SHARK

static char size_wanted;

static int
dummy_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	*value=0;
	return PCIBIOS_SUCCESSFUL;
}

static int
dummy_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	*value=0;
	return PCIBIOS_SUCCESSFUL;
}

static int
dummy_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	if (dev->devfn != 0) *value = 0;
	else
	  switch(where) {
	  case PCI_VENDOR_ID:
	    *value = PCI_VENDOR_ID_INTERG | PCI_DEVICE_ID_INTERG_2010 << 16;
	    break;
	  case PCI_CLASS_REVISION:
	    *value = PCI_CLASS_DISPLAY_VGA << 16;
	    break;
	  case PCI_BASE_ADDRESS_0:
	    if (size_wanted) {
	      /* 0x00900000 bytes long (0xff700000) */
	      *value = 0xff000000;
	      size_wanted = 0;
	    } else {
	      *value = FB_START;
	    }
	    break;
	  case PCI_INTERRUPT_LINE:
	    *value = 6;
	    break;
	  default:
	    *value = 0;
	  }
	return PCIBIOS_SUCCESSFUL;
}

static int
dummy_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	return PCIBIOS_SUCCESSFUL;
}

static int
dummy_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	return PCIBIOS_SUCCESSFUL;
}

static int
dummy_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	if ((dev->devfn == 0) && (where == PCI_BASE_ADDRESS_0) && (value == 0xffffffff))
	  size_wanted = 1;
	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops dummy_ops = {
	dummy_read_config_byte,
	dummy_read_config_word,
	dummy_read_config_dword,
	dummy_write_config_byte,
	dummy_write_config_word,
	dummy_write_config_dword,
};
#endif

void __init via82c505_init(void *sysdata)
{
	struct pci_bus *bus;

	printk(KERN_DEBUG "PCI: VIA 82c505\n");
	if (!request_region(0xA8,2,"via config")) {
		printk(KERN_WARNING"VIA 82c505: Unable to request region 0xA8\n");
		return;
	}
	if (!request_region(0xCF8,8,"pci config")) {
		printk(KERN_WARNING"VIA 82c505: Unable to request region 0xCF8\n");
		release_region(0xA8, 2);
		return;
	}

	/* Enable compatible Mode */
	outb(0x96,0xA8);
	outb(0x18,0xA9);
	outb(0x93,0xA8);
	outb(0xd0,0xA9);

	pci_scan_bus(0, &via82c505_ops, sysdata);

#ifdef CONFIG_ARCH_SHARK
	/* 
	 * Initialize a fake pci-bus number 1 for the CyberPro
         * on the vlbus
	 */
	bus = pci_scan_bus(1, &dummy_ops, sysdata);
#endif
}
