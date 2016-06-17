/*
 *	Low-Level PCI Support for PC
 *
 *	(c) 1999--2000 Martin Mares <mj@ucw.cz>
 * 	2001 Andi Kleen. Cleanup for x86-64. Removed PCI-BIOS access and fixups
 *	for hardware that is unlikely to exist on any Hammer platform.
 * 
 * 	On x86-64 we don't have any access to the PCI-BIOS in long mode, so we
 *	cannot sort the pci device table based on what the BIOS did. This might 
 *	change the probing order of some devices compared to an i386 kernel.
 * 	May need to use ACPI to fix this.
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/acpi.h>

#include <asm/segment.h>
#include <asm/io.h>
#include <asm/mpspec.h>
#include <asm/proto.h>

#include "pci-x86_64.h"

unsigned int pci_probe = PCI_PROBE_CONF1 | PCI_PROBE_CONF2;

int pcibios_last_bus = -1;
struct pci_bus *pci_root_bus;
struct pci_ops *pci_root_ops;

int (*pci_config_read)(int seg, int bus, int dev, int fn, int reg, int len, u32 *value) = NULL;
int (*pci_config_write)(int seg, int bus, int dev, int fn, int reg, int len, u32 value) = NULL;

static int pci_using_acpi_prt = 0;

/* XXX: not taken by all accesses currently */
static spinlock_t pci_config_lock = SPIN_LOCK_UNLOCKED;

/*
 * Direct access to PCI hardware...
 */

#ifdef CONFIG_PCI_DIRECT

/*
 * Functions for accessing PCI configuration space with type 1 accesses
 */

#define CONFIG_CMD(dev, where)   (0x80000000 | (dev->bus->number << 16) | (dev->devfn << 8) | (where & ~3))

#define PCI_CONF1_ADDRESS(bus, dev, fn, reg) \
	(0x80000000 | (bus << 16) | (dev << 11) | (fn << 8) | (reg & ~3))

static int pci_conf1_read (int seg, int bus, int dev, int fn, int reg, int len, 
			   u32 *value) 
{
	unsigned long flags;

	if (!value || (bus > 255) || (dev > 31) || (fn > 7) || (reg > 255))
		return -EINVAL;

	spin_lock_irqsave(&pci_config_lock, flags);

	outl(PCI_CONF1_ADDRESS(bus, dev, fn, reg), 0xCF8);

	switch (len) {
	case 1:
		*value = inb(0xCFC + (reg & 3));
		break;
	case 2:
		*value = inw(0xCFC + (reg & 2));
		break;
	case 4:
		*value = inl(0xCFC);
		break;
	}

	spin_unlock_irqrestore(&pci_config_lock, flags);

	return 0;
}

static int pci_conf1_write (int seg, int bus, int dev, int fn, int reg, int len, u32 value)
{
	unsigned long flags;

	if ((bus > 255) || (dev > 31) || (fn > 7) || (reg > 255)) 
		return -EINVAL;

	spin_lock_irqsave(&pci_config_lock, flags);

	outl(PCI_CONF1_ADDRESS(bus, dev, fn, reg), 0xCF8);

	switch (len) {
	case 1:
		outb((u8)value, 0xCFC + (reg & 3));
		break;
	case 2:
		outw((u16)value, 0xCFC + (reg & 2));
		break;
	case 4:
		outl((u32)value, 0xCFC);
		break;
	}

	spin_unlock_irqrestore(&pci_config_lock, flags);

	return 0;
}

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

static struct pci_ops pci_direct_conf1 = {
	pci_conf1_read_config_byte,
	pci_conf1_read_config_word,
	pci_conf1_read_config_dword,
	pci_conf1_write_config_byte,
	pci_conf1_write_config_word,
	pci_conf1_write_config_dword
};

/*
 * Functions for accessing PCI configuration space with type 2 accesses
 */

#define IOADDR(devfn, where)	((0xC000 | ((devfn & 0x78) << 5)) + where)
#define FUNC(devfn)		(((devfn & 7) << 1) | 0xf0)
#define SET(dev)		if (dev->devfn & 0x80) return PCIBIOS_DEVICE_NOT_FOUND;		\
				outb(FUNC(dev->devfn), 0xCF8);					\
				outb(dev->bus->number, 0xCFA);

#define PCI_CONF2_ADDRESS(dev, reg)	(u16)(0xC000 | (dev << 8) | reg)

static int pci_conf2_read (int seg, int bus, int dev, int fn, int reg, int len, u32 *value)
{
	unsigned long flags;

	if (!value || (bus > 255) || (dev > 31) || (fn > 7) || (reg > 255))
		return -EINVAL;

	if (dev & 0x10) 
		return PCIBIOS_DEVICE_NOT_FOUND;

	spin_lock_irqsave(&pci_config_lock, flags);

	outb((u8)(0xF0 | (fn << 1)), 0xCF8);
	outb((u8)bus, 0xCFA);

	switch (len) {
	case 1:
		*value = inb(PCI_CONF2_ADDRESS(dev, reg));
		break;
	case 2:
		*value = inw(PCI_CONF2_ADDRESS(dev, reg));
		break;
	case 4:
		*value = inl(PCI_CONF2_ADDRESS(dev, reg));
		break;
	}

	outb (0, 0xCF8);

	spin_unlock_irqrestore(&pci_config_lock, flags);

	return 0;
}

static int pci_conf2_write (int seg, int bus, int dev, int fn, int reg, int len, u32 value)
{
	unsigned long flags;

	if ((bus > 255) || (dev > 31) || (fn > 7) || (reg > 255)) 
		return -EINVAL;

	if (dev & 0x10) 
		return PCIBIOS_DEVICE_NOT_FOUND;

	spin_lock_irqsave(&pci_config_lock, flags);

	outb((u8)(0xF0 | (fn << 1)), 0xCF8);
	outb((u8)bus, 0xCFA);

	switch (len) {
	case 1:
		outb ((u8)value, PCI_CONF2_ADDRESS(dev, reg));
		break;
	case 2:
		outw ((u16)value, PCI_CONF2_ADDRESS(dev, reg));
		break;
	case 4:
		outl ((u32)value, PCI_CONF2_ADDRESS(dev, reg));
		break;
	}

	outb (0, 0xCF8);    

	spin_unlock_irqrestore(&pci_config_lock, flags);

	return 0;
}

static int pci_conf2_read_config_byte(struct pci_dev *dev, int where, u8 *value)
{
	SET(dev);
	*value = inb(IOADDR(dev->devfn,where));
	outb (0, 0xCF8);
	return PCIBIOS_SUCCESSFUL;
}

static int pci_conf2_read_config_word(struct pci_dev *dev, int where, u16 *value)
{
	SET(dev);
	*value = inw(IOADDR(dev->devfn,where));
	outb (0, 0xCF8);
	return PCIBIOS_SUCCESSFUL;
}

static int pci_conf2_read_config_dword(struct pci_dev *dev, int where, u32 *value)
{
	SET(dev);
	*value = inl (IOADDR(dev->devfn,where));    
	outb (0, 0xCF8);    
	return PCIBIOS_SUCCESSFUL;
}

static int pci_conf2_write_config_byte(struct pci_dev *dev, int where, u8 value)
{
	SET(dev);
	outb (value, IOADDR(dev->devfn,where));
	outb (0, 0xCF8);    
	return PCIBIOS_SUCCESSFUL;
}

static int pci_conf2_write_config_word(struct pci_dev *dev, int where, u16 value)
{
	SET(dev);
	outw (value, IOADDR(dev->devfn,where));
	outb (0, 0xCF8);    
	return PCIBIOS_SUCCESSFUL;
}

static int pci_conf2_write_config_dword(struct pci_dev *dev, int where, u32 value)
{
	SET(dev);
	outl (value, IOADDR(dev->devfn,where));    
	outb (0, 0xCF8);    
	return PCIBIOS_SUCCESSFUL;
}

#undef SET
#undef IOADDR
#undef FUNC

static struct pci_ops pci_direct_conf2 = {
	pci_conf2_read_config_byte,
	pci_conf2_read_config_word,
	pci_conf2_read_config_dword,
	pci_conf2_write_config_byte,
	pci_conf2_write_config_word,
	pci_conf2_write_config_dword
};

/*
 * Before we decide to use direct hardware access mechanisms, we try to do some
 * trivial checks to ensure it at least _seems_ to be working -- we just test
 * whether bus 00 contains a host bridge (this is similar to checking
 * techniques used in XFree86, but ours should be more reliable since we
 * attempt to make use of direct access hints provided by the PCI BIOS).
 *
 * This should be close to trivial, but it isn't, because there are buggy
 * chipsets (yes, you guessed it, by Intel and Compaq) that have no class ID.
 */
static int __devinit pci_sanity_check(struct pci_ops *o)
{
	u16 x;
	struct pci_bus bus;		/* Fake bus and device */
	struct pci_dev dev;

	if (pci_probe & PCI_NO_CHECKS)
		return 1;
	bus.number = 0;
	dev.bus = &bus;
	for(dev.devfn=0; dev.devfn < 0x100; dev.devfn++)
		if ((!o->read_word(&dev, PCI_CLASS_DEVICE, &x) &&
		     (x == PCI_CLASS_BRIDGE_HOST || x == PCI_CLASS_DISPLAY_VGA)) ||
		    (!o->read_word(&dev, PCI_VENDOR_ID, &x) &&
		     (x == PCI_VENDOR_ID_INTEL || x == PCI_VENDOR_ID_COMPAQ)))
			return 1;
	DBG("PCI: Sanity check failed\n");
	return 0;
}

static struct pci_ops * __devinit pci_check_direct(void)
{
	unsigned int tmp;
	unsigned long flags;

	__save_flags(flags); __cli();

	/*
	 * Check if configuration type 1 works.
	 */
	if (pci_probe & PCI_PROBE_CONF1) {
		outb (0x01, 0xCFB);
		tmp = inl (0xCF8);
		outl (0x80000000, 0xCF8);
		if (inl (0xCF8) == 0x80000000 &&
		    pci_sanity_check(&pci_direct_conf1)) {
			outl (tmp, 0xCF8);
			__restore_flags(flags);
			printk(KERN_INFO "PCI: Using configuration type 1\n");
			request_region(0xCF8, 8, "PCI conf1");
			return &pci_direct_conf1;
		}
		outl (tmp, 0xCF8);
	}

	/*
	 * Check if configuration type 2 works.
	 */
	if (pci_probe & PCI_PROBE_CONF2) {
		outb (0x00, 0xCFB);
		outb (0x00, 0xCF8);
		outb (0x00, 0xCFA);
		if (inb (0xCF8) == 0x00 && inb (0xCFA) == 0x00 &&
		    pci_sanity_check(&pci_direct_conf2)) {
			__restore_flags(flags);
			printk(KERN_INFO "PCI: Using configuration type 2\n");
			request_region(0xCF8, 4, "PCI conf2");
			return &pci_direct_conf2;
		}
	}

	__restore_flags(flags);
	return NULL;
}

#endif

struct pci_bus * __devinit pcibios_scan_root(int busnum)
{
	struct list_head *list;
	struct pci_bus *bus;

	list_for_each(list, &pci_root_buses) {
		bus = pci_bus_b(list);
		if (bus->number == busnum) {
			/* Already scanned */
			return bus;
		}
	}

	printk("PCI: Probing PCI hardware (bus %02x)\n", busnum);

	return pci_scan_bus(busnum, pci_root_ops, NULL);
}

/*
 * Several buggy motherboards address only 16 devices and mirror
 * them to next 16 IDs. We try to detect this `feature' on all
 * primary buses (those containing host bridges as they are
 * expected to be unique) and remove the ghost devices.
 */

static void __devinit pcibios_fixup_ghosts(struct pci_bus *b)
{
	struct list_head *ln, *mn;
	struct pci_dev *d, *e;
	int mirror = PCI_DEVFN(16,0);
	int seen_host_bridge = 0;
	int i;

	DBG("PCI: Scanning for ghost devices on bus %d\n", b->number);
	for (ln=b->devices.next; ln != &b->devices; ln=ln->next) {
		d = pci_dev_b(ln);
		if ((d->class >> 8) == PCI_CLASS_BRIDGE_HOST)
			seen_host_bridge++;
		for (mn=ln->next; mn != &b->devices; mn=mn->next) {
			e = pci_dev_b(mn);
			if (e->devfn != d->devfn + mirror ||
			    e->vendor != d->vendor ||
			    e->device != d->device ||
			    e->class != d->class)
				continue;
			for(i=0; i<PCI_NUM_RESOURCES; i++)
				if (e->resource[i].start != d->resource[i].start ||
				    e->resource[i].end != d->resource[i].end ||
				    e->resource[i].flags != d->resource[i].flags)
					continue;
			break;
		}
		if (mn == &b->devices)
			return;
	}
	if (!seen_host_bridge)
		return;
	printk(KERN_INFO "PCI: Ignoring ghost devices on bus %02x\n", b->number);

	ln = &b->devices;
	while (ln->next != &b->devices) {
		d = pci_dev_b(ln->next);
		if (d->devfn >= mirror) {
			list_del(&d->global_list);
			list_del(&d->bus_list);
			kfree(d);
		} else
			ln = ln->next;
	}
}

/*
 * Discover remaining PCI buses in case there are peer host bridges.
 */
static void __devinit pcibios_fixup_peer_bridges(void)
{
	int n;
	struct pci_bus bus;
	struct pci_dev dev;
	u16 l;

	if (pcibios_last_bus <= 0 || pcibios_last_bus >= 0xff)
		return;
	DBG("PCI: Peer bridge fixup\n");
	for (n=0; n <= pcibios_last_bus; n++) {
		if (pci_bus_exists(&pci_root_buses, n))
			continue;
		bus.number = n;
		bus.ops = pci_root_ops;
		dev.bus = &bus;
		for(dev.devfn=0; dev.devfn<256; dev.devfn += 8)
			if (!pci_read_config_word(&dev, PCI_VENDOR_ID, &l) &&
			    l != 0x0000 && l != 0xffff) {
				DBG("Found device at %02x:%02x [%04x]\n", n, dev.devfn, l);
				printk(KERN_INFO "PCI: Discovered peer bus %02x\n", n);
				pci_scan_bus(n, pci_root_ops, NULL);
				break;
			}
	}
}

static void __devinit pci_scan_mptable(void)
{ 
	int i; 

	/* Handle ACPI here */
	if (!smp_found_config) { 
		printk(KERN_WARNING "PCI: Warning: no mptable. Scanning busses upto 0xff\n"); 
		pcibios_last_bus = 0xff; 
		return;
	} 

	pcibios_last_bus = 0xff;

	for (i = 0; i < MAX_MP_BUSSES; i++) {
		int n = mp_bus_id_to_pci_bus[i]; 
		if (n < 0 || n >= 0xff)
			continue; 
		if (pci_bus_exists(&pci_root_buses, n))
			continue;
		printk(KERN_INFO "PCI: Scanning bus %02x from mptable\n", n); 
		pci_scan_bus(n, pci_root_ops, NULL); 
	} 			
} 

static void __devinit pci_fixup_ide_bases(struct pci_dev *d)
{
	int i;

	/*
	 * PCI IDE controllers use non-standard I/O port decoding, respect it.
	 */
	if ((d->class >> 8) != PCI_CLASS_STORAGE_IDE)
		return;
	DBG("PCI: IDE base address fixup for %s\n", d->slot_name);
	for(i=0; i<4; i++) {
		struct resource *r = &d->resource[i];
		if ((r->start & ~0x80) == 0x374) {
			r->start |= 2;
			r->end = r->start;
		}
	}
}

struct pci_fixup pcibios_fixups[] = {
	{ PCI_FIXUP_HEADER, PCI_ANY_ID,	PCI_ANY_ID, pci_fixup_ide_bases },
	{ 0 }
};

/*
 *  Called after each bus is probed, but before its children
 *  are examined.
 */

void __devinit pcibios_fixup_bus(struct pci_bus *b)
{
	pcibios_fixup_ghosts(b);
	pci_read_bridge_bases(b);
}

void __devinit pcibios_config_init(void)
{
	/*
	 * Try all known PCI access methods. Note that we support using 
	 * both PCI BIOS and direct access, with a preference for direct.
	 */

#ifdef CONFIG_PCI_DIRECT
	if ((pci_probe & (PCI_PROBE_CONF1 | PCI_PROBE_CONF2)) 
		&& (pci_root_ops = pci_check_direct())) {
		if (pci_root_ops == &pci_direct_conf1) {
			pci_config_read = pci_conf1_read;
			pci_config_write = pci_conf1_write;
		}
		else {
			pci_config_read = pci_conf2_read;
			pci_config_write = pci_conf2_write;
		}
	} else
		printk("??? no pci access\n"); 
#endif

	return;
}

void __devinit pcibios_init(void)
{
	struct pci_ops *dir = NULL;

	if (!pci_root_ops)
		pcibios_config_init();

#ifdef CONFIG_PCI_DIRECT
	if (pci_probe & (PCI_PROBE_CONF1 | PCI_PROBE_CONF2))
		dir = pci_check_direct();
#endif
	if (dir)
		pci_root_ops = dir;
	else {
		printk(KERN_INFO "PCI: No PCI bus detected\n");
		return;
	}

	printk(KERN_INFO "PCI: Probing PCI hardware\n");
#ifdef CONFIG_ACPI_PCI
 	if (!acpi_disabled && !acpi_noirq && !acpi_pci_irq_init())
 		pci_using_acpi_prt = 1;
#endif
 	if (!pci_using_acpi_prt) {
 		pci_root_bus = pcibios_scan_root(0);
	pcibios_irq_init();
	pci_scan_mptable(); 
	pcibios_fixup_peer_bridges();
	pcibios_fixup_irqs();
 	}

	pcibios_resource_survey();

#ifdef CONFIG_GART_IOMMU
	pci_iommu_init();
#endif
}

char * __devinit pcibios_setup(char *str)
{
	if (!strcmp(str, "off")) {
		pci_probe = 0;
		return NULL;
	}
	else if (!strncmp(str, "bios", 4)) {
		printk(KERN_WARNING "PCI: No PCI bios access on x86-64. BIOS hint ignored.\n");
		return NULL;
	} else if (!strcmp(str, "nobios")) {
		pci_probe &= ~PCI_PROBE_BIOS;
		return NULL;
	} else if (!strcmp(str, "nosort")) { /* Default */ 
		pci_probe |= PCI_NO_SORT;
		return NULL;
	} 
#ifdef CONFIG_PCI_DIRECT
	else if (!strcmp(str, "conf1")) {
		pci_probe = PCI_PROBE_CONF1 | PCI_NO_CHECKS;
		return NULL;
	}
	else if (!strcmp(str, "conf2")) {
		pci_probe = PCI_PROBE_CONF2 | PCI_NO_CHECKS;
		return NULL;
	}
#endif
	else if (!strcmp(str, "rom")) {
		pci_probe |= PCI_ASSIGN_ROMS;
		return NULL;
	} else if (!strcmp(str, "assign-busses")) {
		pci_probe |= PCI_ASSIGN_ALL_BUSSES;
		return NULL;
	} else if (!strncmp(str, "irqmask=", 8)) {
		pcibios_irq_mask = simple_strtol(str+8, NULL, 0);
		return NULL;
	} else if (!strncmp(str, "lastbus=", 8)) {
		pcibios_last_bus = simple_strtol(str+8, NULL, 0);
		return NULL;
	} else if (!strncmp(str, "noacpi", 6)) {
		acpi_noirq_set();
		return NULL;
	}
	return str;
}

unsigned int pcibios_assign_all_busses(void)
{
	return (pci_probe & PCI_ASSIGN_ALL_BUSSES) ? 1 : 0;
}

int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	int err;

	if ((err = pcibios_enable_resources(dev, mask)) < 0)
		return err;

#ifdef CONFIG_ACPI_PCI
	if (!acpi_noirq && pci_using_acpi_prt) {
		acpi_pci_irq_enable(dev);
		return 0;
	}
#endif

	pcibios_enable_irq(dev);

	return 0;
}
