/*
 *  linux/arch/arm/kernel/bios32.c
 *
 *  PCI bios-type initialisation for PCI machines
 *
 *  Bits taken from various places.
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/init.h>

#include <asm/page.h> /* for BUG() */
#include <asm/irq.h>
#include <asm/mach-types.h>
#include <asm/mach/pci.h>

static int debug_pci;
int have_isa_bridge;

struct pci_sys_data {
	/*
	 * The hardware we are attached to
	 */
	struct hw_pci	*hw;

	unsigned long	mem_offset;

	/*
	 * These are the resources for the root bus.
	 */
	struct resource *resource[3];
};

void pcibios_report_status(u_int status_mask, int warn)
{
	struct pci_dev *dev;

	pci_for_each_dev(dev) {
		u16 status;

		/*
		 * ignore host bridge - we handle
		 * that separately
		 */
		if (dev->bus->number == 0 && dev->devfn == 0)
			continue;

		pci_read_config_word(dev, PCI_STATUS, &status);

		status &= status_mask;
		if (status == 0)
			continue;

		/* clear the status errors */
		pci_write_config_word(dev, PCI_STATUS, status);

		if (warn)
			printk("(%02x:%02x.%d: %04X) ", dev->bus->number,
				PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn),
				status);
	}
}

/*
 * We don't use this to fix the device, but initialisation of it.
 * It's not the correct use for this, but it works.
 * Note that the arbiter/ISA bridge appears to be buggy, specifically in
 * the following area:
 * 1. park on CPU
 * 2. ISA bridge ping-pong
 * 3. ISA bridge master handling of target RETRY
 *
 * Bug 3 is responsible for the sound DMA grinding to a halt.  We now
 * live with bug 2.
 */
static void __init pci_fixup_83c553(struct pci_dev *dev)
{
	/*
	 * Set memory region to start at address 0, and enable IO
	 */
	pci_write_config_dword(dev, PCI_BASE_ADDRESS_0, PCI_BASE_ADDRESS_SPACE_MEMORY);
	pci_write_config_word(dev, PCI_COMMAND, PCI_COMMAND_IO);

	dev->resource[0].end -= dev->resource[0].start;
	dev->resource[0].start = 0;

	/*
	 * All memory requests from ISA to be channelled to PCI
	 */
	pci_write_config_byte(dev, 0x48, 0xff);

	/*
	 * Enable ping-pong on bus master to ISA bridge transactions.
	 * This improves the sound DMA substantially.  The fixed
	 * priority arbiter also helps (see below).
	 */
	pci_write_config_byte(dev, 0x42, 0x01);

	/*
	 * Enable PCI retry
	 */
	pci_write_config_byte(dev, 0x40, 0x22);

	/*
	 * We used to set the arbiter to "park on last master" (bit
	 * 1 set), but unfortunately the CyberPro does not park the
	 * bus.  We must therefore park on CPU.  Unfortunately, this
	 * may trigger yet another bug in the 553.
	 */
	pci_write_config_byte(dev, 0x83, 0x02);

	/*
	 * Make the ISA DMA request lowest priority, and disable
	 * rotating priorities completely.
	 */
	pci_write_config_byte(dev, 0x80, 0x11);
	pci_write_config_byte(dev, 0x81, 0x00);

	/*
	 * Route INTA input to IRQ 11, and set IRQ11 to be level
	 * sensitive.
	 */
	pci_write_config_word(dev, 0x44, 0xb000);
	outb(0x08, 0x4d1);
}

static void __init pci_fixup_unassign(struct pci_dev *dev)
{
	dev->resource[0].end -= dev->resource[0].start;
	dev->resource[0].start = 0;
}

/*
 * Prevent the PCI layer from seeing the resources allocated to this device
 * if it is the host bridge by marking it as such.  These resources are of
 * no consequence to the PCI layer (they are handled elsewhere).
 */
static void __init pci_fixup_dec21285(struct pci_dev *dev)
{
	int i;

	if (dev->devfn == 0) {
		dev->class &= 0xff;
		dev->class |= PCI_CLASS_BRIDGE_HOST << 8;
		for (i = 0; i < PCI_NUM_RESOURCES; i++) {
			dev->resource[i].start = 0;
			dev->resource[i].end   = 0;
			dev->resource[i].flags = 0;
		}
	}
}

/*
 * PCI IDE controllers use non-standard I/O port decoding, respect it.
 */
static void __init pci_fixup_ide_bases(struct pci_dev *dev)
{
	struct resource *r;
	int i;

	if ((dev->class >> 8) != PCI_CLASS_STORAGE_IDE)
		return;

	for (i = 0; i < PCI_NUM_RESOURCES; i++) {
		r = dev->resource + i;
		if ((r->start & ~0x80) == 0x374) {
			r->start |= 2;
			r->end = r->start;
		}
	}
}

/*
 * Put the DEC21142 to sleep
 */
static void __init pci_fixup_dec21142(struct pci_dev *dev)
{
	pci_write_config_dword(dev, 0x40, 0x80000000);
}

/*
 * The CY82C693 needs some rather major fixups to ensure that it does
 * the right thing.  Idea from the Alpha people, with a few additions.
 *
 * We ensure that the IDE base registers are set to 1f0/3f4 for the
 * primary bus, and 170/374 for the secondary bus.  Also, hide them
 * from the PCI subsystem view as well so we won't try to perform
 * our own auto-configuration on them.
 *
 * In addition, we ensure that the PCI IDE interrupts are routed to
 * IRQ 14 and IRQ 15 respectively.
 *
 * The above gets us to a point where the IDE on this device is
 * functional.  However, The CY82C693U _does not work_ in bus
 * master mode without locking the PCI bus solid.
 */
static void __init pci_fixup_cy82c693(struct pci_dev *dev)
{
	if ((dev->class >> 8) == PCI_CLASS_STORAGE_IDE) {
		u32 base0, base1;

		if (dev->class & 0x80) {	/* primary */
			base0 = 0x1f0;
			base1 = 0x3f4;
		} else {			/* secondary */
			base0 = 0x170;
			base1 = 0x374;
		}

		pci_write_config_dword(dev, PCI_BASE_ADDRESS_0,
				       base0 | PCI_BASE_ADDRESS_SPACE_IO);
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_1,
				       base1 | PCI_BASE_ADDRESS_SPACE_IO);

		dev->resource[0].start = 0;
		dev->resource[0].end   = 0;
		dev->resource[0].flags = 0;

		dev->resource[1].start = 0;
		dev->resource[1].end   = 0;
		dev->resource[1].flags = 0;
	} else if (PCI_FUNC(dev->devfn) == 0) {
		/*
		 * Setup IDE IRQ routing.
		 */
		pci_write_config_byte(dev, 0x4b, 14);
		pci_write_config_byte(dev, 0x4c, 15);

		/*
		 * Disable FREQACK handshake, enable USB.
		 */
		pci_write_config_byte(dev, 0x4d, 0x41);

		/*
		 * Enable PCI retry, and PCI post-write buffer.
		 */
		pci_write_config_byte(dev, 0x44, 0x17);

		/*
		 * Enable ISA master and DMA post write buffering.
		 */
		pci_write_config_byte(dev, 0x45, 0x03);
	}
}

struct pci_fixup pcibios_fixups[] = {
	{
		PCI_FIXUP_HEADER,
		PCI_VENDOR_ID_CONTAQ,	PCI_DEVICE_ID_CONTAQ_82C693,
		pci_fixup_cy82c693
	}, {
		PCI_FIXUP_HEADER,
		PCI_VENDOR_ID_DEC,	PCI_DEVICE_ID_DEC_21142,
		pci_fixup_dec21142
	}, {
		PCI_FIXUP_HEADER,
		PCI_VENDOR_ID_DEC,	PCI_DEVICE_ID_DEC_21285,
		pci_fixup_dec21285
	}, {
		PCI_FIXUP_HEADER,
		PCI_VENDOR_ID_WINBOND,	PCI_DEVICE_ID_WINBOND_83C553,
		pci_fixup_83c553
	}, {
		PCI_FIXUP_HEADER,
		PCI_VENDOR_ID_WINBOND2,	PCI_DEVICE_ID_WINBOND2_89C940F,
		pci_fixup_unassign
	}, {
		PCI_FIXUP_HEADER,
		PCI_ANY_ID,		PCI_ANY_ID,
		pci_fixup_ide_bases
	}, { 0 }
};

void __init
pcibios_update_resource(struct pci_dev *dev, struct resource *root,
			struct resource *res, int resource)
{
	struct pci_sys_data *sys = dev->sysdata;
	u32 val, check;
	int reg;

	if (debug_pci)
		printk("PCI: Assigning %3s %08lx to %s\n",
			res->flags & IORESOURCE_IO ? "IO" : "MEM",
			res->start, dev->name);

	if (resource < 6) {
		reg = PCI_BASE_ADDRESS_0 + 4*resource;
	} else if (resource == PCI_ROM_RESOURCE) {
		reg = dev->rom_base_reg;
	} else {
		/* Somebody might have asked allocation of a
		 * non-standard resource.
		 */
		return;
	}

	val = res->start;
	if (res->flags & IORESOURCE_MEM)
		val -= sys->mem_offset;
	val |= res->flags & PCI_REGION_FLAG_MASK;

	pci_write_config_dword(dev, reg, val);
	pci_read_config_dword(dev, reg, &check);
	if ((val ^ check) & ((val & PCI_BASE_ADDRESS_SPACE_IO) ?
	    PCI_BASE_ADDRESS_IO_MASK : PCI_BASE_ADDRESS_MEM_MASK)) {
		printk(KERN_ERR "PCI: Error while updating region "
			"%s/%d (%08x != %08x)\n", dev->slot_name,
			resource, val, check);
	}
}

void __init pcibios_update_irq(struct pci_dev *dev, int irq)
{
	if (debug_pci)
		printk("PCI: Assigning IRQ %02d to %s\n", irq, dev->name);
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, irq);
}

/*
 * If the bus contains any of these devices, then we must not turn on
 * parity checking of any kind.  Currently this is CyberPro 20x0 only.
 */
static inline int pdev_bad_for_parity(struct pci_dev *dev)
{
	return (dev->vendor == PCI_VENDOR_ID_INTERG &&
		(dev->device == PCI_DEVICE_ID_INTERG_2000 ||
		 dev->device == PCI_DEVICE_ID_INTERG_2010));
}

/*
 * Adjust the device resources from bus-centric to Linux-centric.
 */
static void __init
pdev_fixup_device_resources(struct pci_sys_data *root, struct pci_dev *dev)
{
	int i;

	for (i = 0; i < PCI_NUM_RESOURCES; i++) {
		if (dev->resource[i].start == 0)
			continue;
		if (dev->resource[i].flags & IORESOURCE_MEM) {
			dev->resource[i].start += root->mem_offset;
			dev->resource[i].end   += root->mem_offset;
		}
	}
}

static void __init
pbus_assign_bus_resources(struct pci_bus *bus, struct pci_sys_data *root)
{
	struct pci_dev *dev = bus->self;
	int i;

	if (!dev) {
		/*
		 * Assign root bus resources.
		 */
		for (i = 0; i < 3; i++)
			bus->resource[i] = root->resource[i];
	}
}

/*
 * pcibios_fixup_bus - Called after each bus is probed,
 * but before its children are examined.
 */
void __init pcibios_fixup_bus(struct pci_bus *bus)
{
	struct pci_sys_data *root = bus->sysdata;
	struct list_head *walk;
	u16 features = PCI_COMMAND_SERR | PCI_COMMAND_PARITY;
	u16 all_status = -1;

	pbus_assign_bus_resources(bus, root);

	/*
	 * Walk the devices on this bus, working out what we can
	 * and can't support.
	 */
	for (walk = bus->devices.next; walk != &bus->devices; walk = walk->next) {
		struct pci_dev *dev = pci_dev_b(walk);
		u16 status;

		pdev_fixup_device_resources(root, dev);

		pci_read_config_word(dev, PCI_STATUS, &status);
		all_status &= status;

		if (pdev_bad_for_parity(dev))
			features &= ~(PCI_COMMAND_SERR | PCI_COMMAND_PARITY);

		/*
		 * If this device is an ISA bridge, set the have_isa_bridge
		 * flag.  We will then go looking for things like keyboard,
		 * etc
		 */
		if (dev->class >> 8 == PCI_CLASS_BRIDGE_ISA ||
		    dev->class >> 8 == PCI_CLASS_BRIDGE_EISA)
			have_isa_bridge = !0;
	}

	/*
	 * If any device on this bus does not support fast back to back
	 * transfers, then the bus as a whole is not able to support them.
	 * Having fast back to back transfers on saves us one PCI cycle
	 * per transaction.
	 */
	if (all_status & PCI_STATUS_FAST_BACK)
		features |= PCI_COMMAND_FAST_BACK;

	/*
	 * Now walk the devices again, this time setting them up.
	 */
	for (walk = bus->devices.next; walk != &bus->devices; walk = walk->next) {
		struct pci_dev *dev = pci_dev_b(walk);
		u16 cmd;

		pci_read_config_word(dev, PCI_COMMAND, &cmd);
		cmd |= features;
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}

	/*
	 * Report what we did for this bus
	 */
	printk(KERN_INFO "PCI: bus%d: Fast back to back transfers %sabled\n",
		bus->number, (features & PCI_COMMAND_FAST_BACK) ? "en" : "dis");
}

/*
 * Convert from Linux-centric to bus-centric addresses for bridge devices.
 */
void __init
pcibios_fixup_pbus_ranges(struct pci_bus *bus, struct pbus_set_ranges_data *ranges)
{
	struct pci_sys_data *root = bus->sysdata;

	ranges->mem_start -= root->mem_offset;
	ranges->mem_end -= root->mem_offset;
	ranges->prefetch_start -= root->mem_offset;
	ranges->prefetch_end -= root->mem_offset;
}

u8 __init no_swizzle(struct pci_dev *dev, u8 *pin)
{
	return 0;
}

extern struct hw_pci ebsa285_pci;
extern struct hw_pci cats_pci;
extern struct hw_pci netwinder_pci;
extern struct hw_pci personal_server_pci;
extern struct hw_pci ftv_pci;
extern struct hw_pci shark_pci;
extern struct hw_pci integrator_pci;

void __init pcibios_init(void)
{
	struct pci_sys_data *root;
	struct hw_pci *hw = NULL;

	do {
#ifdef CONFIG_ARCH_EBSA285
		if (machine_is_ebsa285()) {
			hw = &ebsa285_pci;
			break;
		}
#endif
#ifdef CONFIG_ARCH_SHARK
		if (machine_is_shark()) {
			hw = &shark_pci;
			break;
		}
#endif
#ifdef CONFIG_ARCH_CATS
		if (machine_is_cats()) {
			hw = &cats_pci;
			break;
		}
#endif
#ifdef CONFIG_ARCH_NETWINDER
		if (machine_is_netwinder()) {
			hw = &netwinder_pci;
			break;
		}
#endif
#ifdef CONFIG_ARCH_PERSONAL_SERVER
		if (machine_is_personal_server()) {
			hw = &personal_server_pci;
			break;
		}
#endif
#ifdef CONFIG_ARCH_FTVPCI
		if (machine_is_ftvpci()) {
			hw = &ftv_pci;
			break;
		}
#endif
#ifdef CONFIG_ARCH_INTEGRATOR
		if (machine_is_integrator()) {
			hw = &integrator_pci;
			break;
		}
#endif
	} while (0);

	if (hw == NULL)
		return;

	root = kmalloc(sizeof(*root), GFP_KERNEL);
	if (!root)
		panic("PCI: unable to allocate root data!");

	root->hw = hw;
	root->mem_offset = hw->mem_offset;

	memset(root->resource, 0, sizeof(root->resource));

	/*
	 * Setup the resources for this bus.
	 *   resource[0] - IO ports
	 *   resource[1] - non-prefetchable memory
	 *   resource[2] - prefetchable memory
	 */
	if (root->hw->setup_resources)
		root->hw->setup_resources(root->resource);
	else {
		root->resource[0] = &ioport_resource;
		root->resource[1] = &iomem_resource;
		root->resource[2] = NULL;
	}

	/*
	 * Set up the host bridge, and scan the bus.
	 */
	root->hw->init(root);

	/*
	 * Assign any unassigned resources.
	 */
	pci_assign_unassigned_resources();
	pci_fixup_irqs(root->hw->swizzle, root->hw->map_irq);
}

char * __init pcibios_setup(char *str)
{
	if (!strcmp(str, "debug")) {
		debug_pci = 1;
		return NULL;
	}
	return str;
}

/*
 * From arch/i386/kernel/pci-i386.c:
 *
 * We need to avoid collisions with `mirrored' VGA ports
 * and other strange ISA hardware, so we always want the
 * addresses to be allocated in the 0x000-0x0ff region
 * modulo 0x400.
 *
 * Why? Because some silly external IO cards only decode
 * the low 10 bits of the IO address. The 0x00-0xff region
 * is reserved for motherboard devices that decode all 16
 * bits, so it's ok to allocate at, say, 0x2800-0x28ff,
 * but we want to try to avoid allocating at 0x2900-0x2bff
 * which might be mirrored at 0x0100-0x03ff..
 */
void pcibios_align_resource(void *data, struct resource *res,
			    unsigned long size, unsigned long align)
{
	if (res->flags & IORESOURCE_IO) {
		unsigned long start = res->start;

		if (start & 0x300)
			res->start = (start + 0x3ff) & ~0x3ff;
	}
}

/**
 * pcibios_enable_device - Enable I/O and memory.
 * @dev: PCI device to be enabled
 */
int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	u16 cmd, old_cmd;
	int idx;
	struct resource *r;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	old_cmd = cmd;
	for (idx = 0; idx < 6; idx++) {
		/* Only set up the requested stuff */
		if (!(mask & (1 << idx)))
			continue;

		r = dev->resource + idx;
		if (!r->start && r->end) {
			printk(KERN_ERR "PCI: Device %s not available because"
			       " of resource collisions\n", dev->slot_name);
			return -EINVAL;
		}
		if (r->flags & IORESOURCE_IO)
			cmd |= PCI_COMMAND_IO;
		if (r->flags & IORESOURCE_MEM)
			cmd |= PCI_COMMAND_MEMORY;
	}

	/*
	 * Bridges (eg, cardbus bridges) need to be fully enabled
	 */
	if ((dev->class >> 16) == PCI_BASE_CLASS_BRIDGE)
		cmd |= PCI_COMMAND_IO | PCI_COMMAND_MEMORY;

	if (cmd != old_cmd) {
		printk("PCI: enabling device %s (%04x -> %04x)\n",
		       dev->slot_name, old_cmd, cmd);
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}
	return 0;
}
