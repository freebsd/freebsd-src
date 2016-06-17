/*
 *	Low-Level PCI Support for PC -- Routing of Interrupts
 *
 *	(c) 1999--2000 Martin Mares <mj@ucw.cz>
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <asm/io.h>
#include <asm/smp.h>
#include <asm/io_apic.h>

#include "pci-x86_64.h"

#define PIRQ_SIGNATURE	(('$' << 0) + ('P' << 8) + ('I' << 16) + ('R' << 24))
#define PIRQ_VERSION 0x0100

static struct irq_routing_table *pirq_table;

/*
 * Never use: 0, 1, 2 (timer, keyboard, and cascade)
 * Avoid using: 13, 14 and 15 (FP error and IDE).
 * Penalize: 3, 4, 6, 7, 12 (known ISA uses: serial, floppy, parallel and mouse)
 */
unsigned int pcibios_irq_mask = 0xfff8;

static int pirq_penalty[16] = {
	1000000, 1000000, 1000000, 1000, 1000, 0, 1000, 1000,
	0, 0, 0, 0, 1000, 100000, 100000, 100000
};

struct irq_router {
	char *name;
	u16 vendor, device;
	int (*get)(struct pci_dev *router, struct pci_dev *dev, int pirq);
	int (*set)(struct pci_dev *router, struct pci_dev *dev, int pirq, int new);
};

/*
 *  Search 0xf0000 -- 0xfffff for the PCI IRQ Routing Table.
 */

static struct irq_routing_table * __init pirq_find_routing_table(void)
{
	u8 *addr;
	struct irq_routing_table *rt;
	int i;
	u8 sum;

	for(addr = (u8 *) __va(0xf0000); addr < (u8 *) __va(0x100000); addr += 16) {
		rt = (struct irq_routing_table *) addr;
		if (rt->signature != PIRQ_SIGNATURE ||
		    rt->version != PIRQ_VERSION ||
		    rt->size % 16 ||
		    rt->size < sizeof(struct irq_routing_table))
			continue;
		sum = 0;
		for(i=0; i<rt->size; i++)
			sum += addr[i];
		if (!sum) {
			DBG("PCI: Interrupt Routing Table found at 0x%p\n", rt);
			return rt;
		}
	}
	return NULL;
}

/*
 *  If we have a IRQ routing table, use it to search for peer host
 *  bridges.  It's a gross hack, but since there are no other known
 *  ways how to get a list of buses, we have to go this way.
 *
 *  [maybe x86-64 architecture should define way to query this info in
 more reasonable way?]
 */

static void __init pirq_peer_trick(void)
{
	struct irq_routing_table *rt = pirq_table;
	u8 busmap[256];
	int i;
	struct irq_info *e;

	memset(busmap, 0, sizeof(busmap));
	for(i=0; i < (rt->size - sizeof(struct irq_routing_table)) / sizeof(struct irq_info); i++) {
		e = &rt->slots[i];
#ifdef DEBUG
		{
			int j;
			DBG("%02x:%02x slot=%02x", e->bus, e->devfn/8, e->slot);
			for(j=0; j<4; j++)
				DBG(" %d:%02x/%04x", j, e->irq[j].link, e->irq[j].bitmap);
			DBG("\n");
		}
#endif
		busmap[e->bus] = 1;
	}
	for(i=1; i<256; i++)
		/*
		 *  It might be a secondary bus, but in this case its parent is already
		 *  known (ascending bus order) and therefore pci_scan_bus returns immediately.
		 */
		if (busmap[i] && pci_scan_bus(i, pci_root_bus->ops, NULL))
			printk(KERN_INFO "PCI: Discovered primary peer bus %02x [IRQ]\n", i);
	pcibios_last_bus = -1;
}

/*
 *  Code for querying and setting of IRQ routes on various interrupt routers.
 */

void eisa_set_level_irq(unsigned int irq)
{
	unsigned char mask = 1 << (irq & 7);
	unsigned int port = 0x4d0 + (irq >> 3);
	unsigned char val = inb(port);

	if (!(val & mask)) {
		DBG(" -> edge");
		outb(val | mask, port);
	}
}

/*
 * Common IRQ routing practice: nybbles in config space,
 * offset by some magic constant.
 */
static unsigned int read_config_nybble(struct pci_dev *router, unsigned offset, unsigned nr)
{
	u8 x;
	unsigned reg = offset + (nr >> 1);

	pci_read_config_byte(router, reg, &x);
	return (nr & 1) ? (x >> 4) : (x & 0xf);
}

static void write_config_nybble(struct pci_dev *router, unsigned offset, unsigned nr, unsigned int val)
{
	u8 x;
	unsigned reg = offset + (nr >> 1);

	pci_read_config_byte(router, reg, &x);
	x = (nr & 1) ? ((x & 0x0f) | (val << 4)) : ((x & 0xf0) | val);
	pci_write_config_byte(router, reg, x);
}

/*
 * ALI pirq entries are damn ugly, and completely undocumented.
 * This has been figured out from pirq tables, and it's not a pretty
 * picture.
 */
static int pirq_ali_get(struct pci_dev *router, struct pci_dev *dev, int pirq)
{
	static unsigned char irqmap[16] = { 0, 9, 3, 10, 4, 5, 7, 6, 1, 11, 0, 12, 0, 14, 0, 15 };

	return irqmap[read_config_nybble(router, 0x48, pirq-1)];
}

static int pirq_ali_set(struct pci_dev *router, struct pci_dev *dev, int pirq, int irq)
{
	static unsigned char irqmap[16] = { 0, 8, 0, 2, 4, 5, 7, 6, 0, 1, 3, 9, 11, 0, 13, 15 };
	unsigned int val = irqmap[irq];
		
	if (val) {
		write_config_nybble(router, 0x48, pirq-1, val);
		return 1;
	}
	return 0;
}

/*
 * The Intel PIIX4 pirq rules are fairly simple: "pirq" is
 * just a pointer to the config space.
 */
static int pirq_piix_get(struct pci_dev *router, struct pci_dev *dev, int pirq)
{
	u8 x;

	pci_read_config_byte(router, pirq, &x);
	return (x < 16) ? x : 0;
}

static int pirq_piix_set(struct pci_dev *router, struct pci_dev *dev, int pirq, int irq)
{
	pci_write_config_byte(router, pirq, irq);
	return 1;
}

/*
 * The VIA pirq rules are nibble-based, like ALI,
 * but without the ugly irq number munging.
 */
static int pirq_via_get(struct pci_dev *router, struct pci_dev *dev, int pirq)
{
	return read_config_nybble(router, 0x55, pirq);
}

static int pirq_via_set(struct pci_dev *router, struct pci_dev *dev, int pirq, int irq)
{
	write_config_nybble(router, 0x55, pirq, irq);
	return 1;
}

/*
 * OPTI: high four bits are nibble pointer..
 * I wonder what the low bits do?
 */
static int pirq_opti_get(struct pci_dev *router, struct pci_dev *dev, int pirq)
{
	return read_config_nybble(router, 0xb8, pirq >> 4);
}

static int pirq_opti_set(struct pci_dev *router, struct pci_dev *dev, int pirq, int irq)
{
	write_config_nybble(router, 0xb8, pirq >> 4, irq);
	return 1;
}

/*
 * Cyrix: nibble offset 0x5C
 */
static int pirq_cyrix_get(struct pci_dev *router, struct pci_dev *dev, int pirq)
{
	return read_config_nybble(router, 0x5C, pirq-1);
}

static int pirq_cyrix_set(struct pci_dev *router, struct pci_dev *dev, int pirq, int irq)
{
	write_config_nybble(router, 0x5C, pirq-1, irq);
	return 1;
}

/*
 *	PIRQ routing for SiS 85C503 router used in several SiS chipsets
 *	According to the SiS 5595 datasheet (preliminary V1.0, 12/24/1997)
 *	the related registers work as follows:
 *	
 *	general: one byte per re-routable IRQ,
 *		 bit 7      IRQ mapping enabled (0) or disabled (1)
 *		 bits [6:4] reserved
 *		 bits [3:0] IRQ to map to
 *		     allowed: 3-7, 9-12, 14-15
 *		     reserved: 0, 1, 2, 8, 13
 *
 *	individual registers in device config space:
 *
 *	0x41/0x42/0x43/0x44:	PCI INT A/B/C/D - bits as in general case
 *
 *	0x61:			IDEIRQ: bits as in general case - but:
 *				bits [6:5] must be written 01
 *				bit 4 channel-select primary (0), secondary (1)
 *
 *	0x62:			USBIRQ: bits as in general case - but:
 *				bit 4 OHCI function disabled (0), enabled (1)
 *	
 *	0x6a:			ACPI/SCI IRQ - bits as in general case
 *
 *	0x7e:			Data Acq. Module IRQ - bits as in general case
 *
 *	Apparently there are systems implementing PCI routing table using both
 *	link values 0x01-0x04 and 0x41-0x44 for PCI INTA..D, but register offsets
 *	like 0x62 as link values for USBIRQ e.g. So there is no simple
 *	"register = offset + pirq" relation.
 *	Currently we support PCI INTA..D and USBIRQ and try our best to handle
 *	both link mappings.
 *	IDE/ACPI/DAQ mapping is currently unsupported (left untouched as set by BIOS).
 */

static int pirq_sis_get(struct pci_dev *router, struct pci_dev *dev, int pirq)
{
	u8 x;
	int reg = pirq;

	switch(pirq) {
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
			reg += 0x40;
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x62:
	pci_read_config_byte(router, reg, &x);
			if (reg != 0x62)
				break;
			if (!(x & 0x40))
				return 0;
			break;
		case 0x61:
		case 0x6a:
		case 0x7e:
			printk(KERN_INFO "SiS pirq: advanced IDE/ACPI/DAQ mapping not yet implemented\n");
			return 0;
		default:			
			printk(KERN_INFO "SiS router pirq escape (%d)\n", pirq);
			return 0;
	}
	return (x & 0x80) ? 0 : (x & 0x0f);
}

static int pirq_sis_set(struct pci_dev *router, struct pci_dev *dev, int pirq, int irq)
{
	u8 x;
	int reg = pirq;

	switch(pirq) {
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
			reg += 0x40;
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x62:
			x = (irq&0x0f) ? (irq&0x0f) : 0x80;
			if (reg != 0x62)
				break;
			/* always mark OHCI enabled, as nothing else knows about this */
			x |= 0x40;
			break;
		case 0x61:
		case 0x6a:
		case 0x7e:
			printk(KERN_INFO "advanced SiS pirq mapping not yet implemented\n");
			return 0;
		default:			
			printk(KERN_INFO "SiS router pirq escape (%d)\n", pirq);
			return 0;
	}
	pci_write_config_byte(router, reg, x);

	return 1;
}

/*
 * VLSI: nibble offset 0x74 - educated guess due to routing table and
 *       config space of VLSI 82C534 PCI-bridge/router (1004:0102)
 *       Tested on HP OmniBook 800 covering PIRQ 1, 2, 4, 8 for onboard
 *       devices, PIRQ 3 for non-pci(!) soundchip and (untested) PIRQ 6
 *       for the busbridge to the docking station.
 */

static int pirq_vlsi_get(struct pci_dev *router, struct pci_dev *dev, int pirq)
{
	if (pirq > 8) {
		printk(KERN_INFO "VLSI router pirq escape (%d)\n", pirq);
		return 0;
	}
	return read_config_nybble(router, 0x74, pirq-1);
}

static int pirq_vlsi_set(struct pci_dev *router, struct pci_dev *dev, int pirq, int irq)
{
	if (pirq > 8) {
		printk(KERN_INFO "VLSI router pirq escape (%d)\n", pirq);
		return 0;
	}
	write_config_nybble(router, 0x74, pirq-1, irq);
	return 1;
}

/*
 * ServerWorks: PCI interrupts mapped to system IRQ lines through Index
 * and Redirect I/O registers (0x0c00 and 0x0c01).  The Index register
 * format is (PCIIRQ## | 0x10), e.g.: PCIIRQ10=0x1a.  The Redirect
 * register is a straight binary coding of desired PIC IRQ (low nibble).
 *
 * The 'link' value in the PIRQ table is already in the correct format
 * for the Index register.  There are some special index values:
 * 0x00 for ACPI (SCI), 0x01 for USB, 0x02 for IDE0, 0x04 for IDE1,
 * and 0x03 for SMBus.
 */
static int pirq_serverworks_get(struct pci_dev *router, struct pci_dev *dev, int pirq)
{
	outb_p(pirq, 0xc00);
	return inb(0xc01) & 0xf;
}

static int pirq_serverworks_set(struct pci_dev *router, struct pci_dev *dev, int pirq, int irq)
{
	outb_p(pirq, 0xc00);
	outb_p(irq, 0xc01);
	return 1;
}

/* Support for AMD756 PCI IRQ Routing
 * Jhon H. Caicedo <jhcaiced@osso.org.co>
 * Jun/21/2001 0.2.0 Release, fixed to use "nybble" functions... (jhcaiced)
 * Jun/19/2001 Alpha Release 0.1.0 (jhcaiced)
 * The AMD756 pirq rules are nibble-based
 * offset 0x56 0-3 PIRQA  4-7  PIRQB
 * offset 0x57 0-3 PIRQC  4-7  PIRQD
 */
static int pirq_amd756_get(struct pci_dev *router, struct pci_dev *dev, int pirq)
{
	u8 irq;
	irq = 0;
	if (pirq <= 4)
	{
		irq = read_config_nybble(router, 0x56, pirq - 1);
	}
	printk(KERN_INFO "AMD756: dev %04x:%04x, router pirq : %d get irq : %2d\n",
		dev->vendor, dev->device, pirq, irq);
	return irq;
}

static int pirq_amd756_set(struct pci_dev *router, struct pci_dev *dev, int pirq, int irq)
{
	printk(KERN_INFO "AMD756: dev %04x:%04x, router pirq : %d SET irq : %2d\n", 
		dev->vendor, dev->device, pirq, irq);
	if (pirq <= 4)
	{
		write_config_nybble(router, 0x56, pirq - 1, irq);
	}
	return 1;
}

#ifdef CONFIG_PCI_BIOS

static int pirq_bios_set(struct pci_dev *router, struct pci_dev *dev, int pirq, int irq)
{
	struct pci_dev *bridge;
	int pin = pci_get_interrupt_pin(dev, &bridge);
	return pcibios_set_irq_routing(bridge, pin, irq);
}

static struct irq_router pirq_bios_router =
	{ "BIOS", 0, 0, NULL, pirq_bios_set };

#endif

static struct irq_router pirq_routers[] = {
	{ "PIIX", PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371FB_0, pirq_piix_get, pirq_piix_set },
	{ "PIIX", PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371SB_0, pirq_piix_get, pirq_piix_set },
	{ "PIIX", PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371AB_0, pirq_piix_get, pirq_piix_set },
	{ "PIIX", PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82371MX,   pirq_piix_get, pirq_piix_set },
	{ "PIIX", PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82443MX_0, pirq_piix_get, pirq_piix_set },
	{ "PIIX", PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82801BA_0, pirq_piix_get, pirq_piix_set },

	{ "ALI", PCI_VENDOR_ID_AL, PCI_DEVICE_ID_AL_M1533, pirq_ali_get, pirq_ali_set },

	{ "VIA", PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C586_0, pirq_via_get, pirq_via_set },
	{ "VIA", PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C596, pirq_via_get, pirq_via_set },
	{ "VIA", PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C686, pirq_via_get, pirq_via_set },

	{ "OPTI", PCI_VENDOR_ID_OPTI, PCI_DEVICE_ID_OPTI_82C700, pirq_opti_get, pirq_opti_set },

	{ "NatSemi", PCI_VENDOR_ID_CYRIX, PCI_DEVICE_ID_CYRIX_5520, pirq_cyrix_get, pirq_cyrix_set },
	{ "SIS", PCI_VENDOR_ID_SI, PCI_DEVICE_ID_SI_503, pirq_sis_get, pirq_sis_set },
	{ "VLSI 82C534", PCI_VENDOR_ID_VLSI, PCI_DEVICE_ID_VLSI_82C534, pirq_vlsi_get, pirq_vlsi_set },
	{ "ServerWorks", PCI_VENDOR_ID_SERVERWORKS, PCI_DEVICE_ID_SERVERWORKS_OSB4,
	  pirq_serverworks_get, pirq_serverworks_set },
	{ "AMD756 VIPER", PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_VIPER_740B,
		pirq_amd756_get, pirq_amd756_set },
	{ "AMD766", PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_VIPER_7413,
		pirq_amd756_get, pirq_amd756_set },
	{ "AMD768", PCI_VENDOR_ID_AMD, PCI_DEVICE_ID_AMD_VIPER_7443,
		pirq_amd756_get, pirq_amd756_set },

	{ "default", 0, 0, NULL, NULL }
};

static struct irq_router *pirq_router;
static struct pci_dev *pirq_router_dev;

static void __init pirq_find_router(void)
{
	struct irq_routing_table *rt = pirq_table;
	struct irq_router *r;

#ifdef CONFIG_PCI_BIOS
	if (!rt->signature) {
		printk(KERN_INFO "PCI: Using BIOS for IRQ routing\n");
		pirq_router = &pirq_bios_router;
		return;
	}
#endif

	DBG("PCI: Attempting to find IRQ router for %04x:%04x\n",
	    rt->rtr_vendor, rt->rtr_device);

	/* fall back to default router if nothing else found */
	pirq_router = &pirq_routers[ARRAY_SIZE(pirq_routers) - 1];

	pirq_router_dev = pci_find_slot(rt->rtr_bus, rt->rtr_devfn);
	if (!pirq_router_dev) {
		DBG("PCI: Interrupt router not found at %02x:%02x\n", rt->rtr_bus, rt->rtr_devfn);
		return;
	}

	for(r=pirq_routers; r->vendor; r++) {
		/* Exact match against router table entry? Use it! */
		if (r->vendor == rt->rtr_vendor && r->device == rt->rtr_device) {
			pirq_router = r;
			break;
		}
		/* Match against router device entry? Use it as a fallback */
		if (r->vendor == pirq_router_dev->vendor && r->device == pirq_router_dev->device) {
	pirq_router = r;
		}
	}
	printk(KERN_INFO "PCI: Using IRQ router %s [%04x/%04x] at %s\n",
		pirq_router->name,
		pirq_router_dev->vendor,
		pirq_router_dev->device,
		pirq_router_dev->slot_name);
}

static struct irq_info *pirq_get_info(struct pci_dev *dev)
{
	struct irq_routing_table *rt = pirq_table;
	int entries = (rt->size - sizeof(struct irq_routing_table)) / sizeof(struct irq_info);
	struct irq_info *info;

	for (info = rt->slots; entries--; info++)
		if (info->bus == dev->bus->number && PCI_SLOT(info->devfn) == PCI_SLOT(dev->devfn))
			return info;
	return NULL;
}

static void pcibios_test_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
}

static int pcibios_lookup_irq(struct pci_dev *dev, int assign)
{
	u8 pin;
	struct irq_info *info;
	int i, pirq, newirq;
	int irq = 0;
	u32 mask;
	struct irq_router *r = pirq_router;
	struct pci_dev *dev2;
	char *msg = NULL;

	if (!pirq_table)
		return 0;

	/* Find IRQ routing entry */
	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
	if (!pin) {
		DBG(" -> no interrupt pin\n");
		return 0;
	}
	pin = pin - 1;
	
	DBG("IRQ for %s:%d", dev->slot_name, pin);
	info = pirq_get_info(dev);
	if (!info) {
		DBG(" -> not found in routing table\n");
		return 0;
	}
	pirq = info->irq[pin].link;
	mask = info->irq[pin].bitmap;
	if (!pirq) {
		DBG(" -> not routed\n");
		return 0;
	}
	DBG(" -> PIRQ %02x, mask %04x, excl %04x", pirq, mask, pirq_table->exclusive_irqs);
	mask &= pcibios_irq_mask;

	/*
	 * Find the best IRQ to assign: use the one
	 * reported by the device if possible.
	 */
	newirq = dev->irq;
	if (!newirq && assign) {
		for (i = 0; i < 16; i++) {
			if (!(mask & (1 << i)))
				continue;
			if (pirq_penalty[i] < pirq_penalty[newirq] &&
			    !request_irq(i, pcibios_test_irq_handler, SA_SHIRQ, "pci-test", dev)) {
				free_irq(i, dev);
				newirq = i;
			}
		}
	}
	DBG(" -> newirq=%d", newirq);

	/* Check if it is hardcoded */
	if ((pirq & 0xf0) == 0xf0) {
		irq = pirq & 0xf;
		DBG(" -> hardcoded IRQ %d\n", irq);
		msg = "Hardcoded";
	} else if (r->get && (irq = r->get(pirq_router_dev, dev, pirq))) {
		DBG(" -> got IRQ %d\n", irq);
		msg = "Found";
	} else if (newirq && r->set && (dev->class >> 8) != PCI_CLASS_DISPLAY_VGA) {
		DBG(" -> assigning IRQ %d", newirq);
		if (r->set(pirq_router_dev, dev, pirq, newirq)) {
			eisa_set_level_irq(newirq);
			DBG(" ... OK\n");
			msg = "Assigned";
			irq = newirq;
		}
	}

	if (!irq) {
		DBG(" ... failed\n");
		if (newirq && mask == (1 << newirq)) {
			msg = "Guessed";
			irq = newirq;
		} else
			return 0;
	}
	printk(KERN_INFO "PCI: %s IRQ %d for device %s\n", msg, irq, dev->slot_name);

	/* Update IRQ for all devices with the same pirq value */
	pci_for_each_dev(dev2) {
		pci_read_config_byte(dev2, PCI_INTERRUPT_PIN, &pin);
		if (!pin)
			continue;
		pin--;
		info = pirq_get_info(dev2);
		if (!info)
			continue;
		if (info->irq[pin].link == pirq) {
			/* We refuse to override the dev->irq information. Give a warning! */
		    	if (dev2->irq && dev2->irq != irq) {
		    		printk(KERN_INFO "IRQ routing conflict for %s, have irq %d, want irq %d\n",
				       dev2->slot_name, dev2->irq, irq);
		    		continue;
		    	}
			dev2->irq = irq;
			pirq_penalty[irq]++;
			if (dev != dev2)
				printk(KERN_INFO "PCI: Sharing IRQ %d with %s\n", irq, dev2->slot_name);
		}
	}
	return 1;
}

void __init pcibios_irq_init(void)
{
	DBG("PCI: IRQ init\n");
	pirq_table = pirq_find_routing_table();
#ifdef CONFIG_PCI_BIOS
	if (!pirq_table && (pci_probe & PCI_BIOS_IRQ_SCAN))
		pirq_table = pcibios_get_irq_routing_table();
#endif
	if (pirq_table) {
		pirq_peer_trick();
		pirq_find_router();
		if (pirq_table->exclusive_irqs) {
			int i;
			for (i=0; i<16; i++)
				if (!(pirq_table->exclusive_irqs & (1 << i)))
					pirq_penalty[i] += 100;
		}
		/* If we're using the I/O APIC, avoid using the PCI IRQ routing table */
		if (io_apic_assign_pci_irqs)
			pirq_table = NULL;
	}
}

void __init pcibios_fixup_irqs(void)
{
	struct pci_dev *dev;
	u8 pin;

	DBG("PCI: IRQ fixup\n");
	pci_for_each_dev(dev) {
		/*
		 * If the BIOS has set an out of range IRQ number, just ignore it.
		 * Also keep track of which IRQ's are already in use.
		 */
		if (dev->irq >= 16) {
			DBG("%s: ignoring bogus IRQ %d\n", dev->slot_name, dev->irq);
			dev->irq = 0;
		}
		/* If the IRQ is already assigned to a PCI device, ignore its ISA use penalty */
		if (pirq_penalty[dev->irq] >= 100 && pirq_penalty[dev->irq] < 100000)
			pirq_penalty[dev->irq] = 0;
		pirq_penalty[dev->irq]++;
	}

	pci_for_each_dev(dev) {
		pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
#ifdef CONFIG_X86_IO_APIC
		/*
		 * Recalculate IRQ numbers if we use the I/O APIC.
		 */
		if (io_apic_assign_pci_irqs)
		{
			int irq;

			if (pin) {
				pin--;		/* interrupt pins are numbered starting from 1 */
				irq = IO_APIC_get_PCI_irq_vector(dev->bus->number, PCI_SLOT(dev->devfn), pin);
	/*
	 * Busses behind bridges are typically not listed in the MP-table.
	 * In this case we have to look up the IRQ based on the parent bus,
	 * parent slot, and pin number. The SMP code detects such bridged
	 * busses itself so we should get into this branch reliably.
	 */
				if (irq < 0 && dev->bus->parent) { /* go back to the bridge */
					struct pci_dev * bridge = dev->bus->self;

					pin = (pin + PCI_SLOT(dev->devfn)) % 4;
					irq = IO_APIC_get_PCI_irq_vector(bridge->bus->number, 
							PCI_SLOT(bridge->devfn), pin);
					if (irq >= 0)
						printk(KERN_WARNING "PCI: using PPB(B%d,I%d,P%d) to get irq %d\n", 
							bridge->bus->number, PCI_SLOT(bridge->devfn), pin, irq);
				}
				if (irq >= 0) {
					printk(KERN_INFO "PCI->APIC IRQ transform: (B%d,I%d,P%d) -> %d\n",
						dev->bus->number, PCI_SLOT(dev->devfn), pin, irq);
					dev->irq = irq;
				}
			}
		}
#endif
		/*
		 * Still no IRQ? Try to lookup one...
		 */
		if (pin && !dev->irq)
			pcibios_lookup_irq(dev, 0);
	}
}

void pcibios_penalize_isa_irq(int irq)
{
	/*
	 *  If any ISAPnP device reports an IRQ in its list of possible
	 *  IRQ's, we try to avoid assigning it to PCI devices.
	 */
	pirq_penalty[irq] += 100;
}

void pcibios_enable_irq(struct pci_dev *dev)
{
		u8 pin;
	extern int interrupt_line_quirk;

		pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
	if (pin && !pcibios_lookup_irq(dev, 1) && !dev->irq) {
			char *msg;
		/* With IDE legacy devices the IRQ lookup failure is not a problem.. */
		if (dev->class >> 8 == PCI_CLASS_STORAGE_IDE && !(dev->class & 0x5))
			return;

			if (io_apic_assign_pci_irqs)
				msg = " Probably buggy MP table.";
			else if (pci_probe & PCI_BIOS_IRQ_SCAN)
				msg = "";
			else
				msg = " Please try using pci=biosirq.";
			printk(KERN_WARNING "PCI: No IRQ known for interrupt pin %c of device %s.%s\n",
			       'A' + pin - 1, dev->slot_name, msg);
	}
	/* VIA bridges use interrupt line for apic/pci steering across
	   the V-Link */
	else if (interrupt_line_quirk)
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
}
