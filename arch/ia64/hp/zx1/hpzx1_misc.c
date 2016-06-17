/*
 * Misc. support for HP zx1 chipset support
 *
 * Copyright (C) 2002 Hewlett-Packard Co
 * Copyright (C) 2002 Alex Williamson <alex_williamson@hp.com>
 * Copyright (C) 2002 Bjorn Helgaas <bjorn_helgaas@hp.com>
 */


#include <linux/config.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/acpi.h>
#include <linux/efi.h>

#include <asm/dma.h>
#include <asm/iosapic.h>

#define PFX

static int hpzx1_devices;

struct fake_pci_dev {
	unsigned long csr_base;
	unsigned long csr_size;
	unsigned long mapped_csrs;	// ioremapped
	int sizing;			// in middle of BAR sizing operation?
};

#define PCI_FAKE_DEV(dev)	((struct fake_pci_dev *) \
					PCI_CONTROLLER(dev)->platform_data)

static struct pci_ops *orig_pci_ops;

#define HP_CFG_RD(sz, bits, name) \
static int hp_cfg_read##sz (struct pci_dev *dev, int where, u##bits *value) \
{ \
	struct fake_pci_dev *fake_dev; \
	if (!(fake_dev = PCI_FAKE_DEV(dev))) \
		return orig_pci_ops->name(dev, where, value); \
	\
	if (where == PCI_BASE_ADDRESS_0) { \
		if (fake_dev->sizing) \
			*value = ~(fake_dev->csr_size - 1); \
		else \
			*value = (fake_dev->csr_base & \
				    PCI_BASE_ADDRESS_MEM_MASK) | \
				PCI_BASE_ADDRESS_SPACE_MEMORY; \
		fake_dev->sizing = 0; \
		return PCIBIOS_SUCCESSFUL; \
	} \
	switch (where & ~0x7) { \
		case 0x48: /* initiates config cycles */ \
		case 0x78: /* elroy suspend mode register */ \
			*value = 0; \
			break; \
		default: \
			*value = read##sz(fake_dev->mapped_csrs + where); \
	} \
	if (where == PCI_COMMAND) \
		*value |= PCI_COMMAND_MEMORY; /* SBA omits this */ \
	return PCIBIOS_SUCCESSFUL; \
}

#define HP_CFG_WR(sz, bits, name) \
static int hp_cfg_write##sz (struct pci_dev *dev, int where, u##bits value) \
{ \
	struct fake_pci_dev *fake_dev; \
	\
	if (!(fake_dev = PCI_FAKE_DEV(dev))) \
		return orig_pci_ops->name(dev, where, value); \
	\
	if (where == PCI_BASE_ADDRESS_0) { \
		if (value == (u##bits) ~0) \
			fake_dev->sizing = 1; \
		return PCIBIOS_SUCCESSFUL; \
	} \
	switch (where & ~0x7) { \
		case 0x48: /* initiates config cycles */ \
		case 0x78: /* elroy suspend mode register */ \
			break; \
		default: \
			write##sz(value, fake_dev->mapped_csrs + where); \
	} \
	return PCIBIOS_SUCCESSFUL; \
}

HP_CFG_RD(b,  8, read_byte)
HP_CFG_RD(w, 16, read_word)
HP_CFG_RD(l, 32, read_dword)
HP_CFG_WR(b,  8, write_byte)
HP_CFG_WR(w, 16, write_word)
HP_CFG_WR(l, 32, write_dword)

static struct pci_ops hp_pci_conf = {
	hp_cfg_readb,
	hp_cfg_readw,
	hp_cfg_readl,
	hp_cfg_writeb,
	hp_cfg_writew,
	hp_cfg_writel,
};

static void
hpzx1_fake_pci_dev(char *name, unsigned int busnum, unsigned long addr, unsigned int size)
{
	struct pci_controller *controller;
	struct fake_pci_dev *fake;
	int slot;
	struct pci_dev *dev;
	struct pci_bus *b, *bus = NULL;
	u8 hdr;

	controller = kmalloc(sizeof(*controller), GFP_KERNEL);
	if (!controller) {
		printk(KERN_ERR PFX "No memory for %s (0x%p) sysdata\n", name,
			(void *) addr);
		return;
	}
	memset(controller, 0, sizeof(*controller));

        fake = kmalloc(sizeof(*fake), GFP_KERNEL);
	if (!fake) {
		printk(KERN_ERR PFX "No memory for %s (0x%p) sysdata\n", name,
			(void *) addr);
		kfree(controller);
		return;
	}

	memset(fake, 0, sizeof(*fake));
	fake->csr_base = addr;
	fake->csr_size = size;
	fake->mapped_csrs = (unsigned long) ioremap(addr, size);
	fake->sizing = 0;
	controller->platform_data = fake;

	pci_for_each_bus(b)
		if (busnum == b->number) {
			bus = b;
			break;
		}

	if (!bus) {
		printk(KERN_ERR PFX "No host bus 0x%02x for %s (0x%p)\n",
			busnum, name, (void *) addr);
		kfree(fake);
		kfree(controller);
		return;
	}

	for (slot = 0x1e; slot; slot--)
		if (!pci_find_slot(busnum, PCI_DEVFN(slot, 0)))
			break;

	if (slot < 0) {
		printk(KERN_ERR PFX "No space for %s (0x%p) on bus 0x%02x\n",
			name, (void *) addr, busnum);
		kfree(fake);
		kfree(controller);
		return;
	}

        dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		printk(KERN_ERR PFX "No memory for %s (0x%p)\n", name,
			(void *) addr);
		kfree(fake);
		kfree(controller);
		return;
	}

	bus->ops = &hp_pci_conf;	// replace pci ops for this bus

	memset(dev, 0, sizeof(*dev));
	dev->bus = bus;
	dev->sysdata = controller;
	dev->devfn = PCI_DEVFN(slot, 0);
	pci_read_config_word(dev, PCI_VENDOR_ID, &dev->vendor);
	pci_read_config_word(dev, PCI_DEVICE_ID, &dev->device);
	pci_read_config_byte(dev, PCI_HEADER_TYPE, &hdr);
	dev->hdr_type = hdr & 0x7f;

	pci_setup_device(dev);

	// pci_insert_device() without running /sbin/hotplug
	list_add_tail(&dev->bus_list, &bus->devices);
	list_add_tail(&dev->global_list, &pci_devices);

	printk(KERN_INFO PFX "%s at 0x%lx; pci dev %s\n", name, addr,
		dev->slot_name);

	hpzx1_devices++;
}

static acpi_status
hpzx1_sba_probe(acpi_handle obj, u32 depth, void *context, void **ret)
{
	u64 csr_base = 0, csr_length = 0;
	acpi_status status;
	char *name = context;
	char fullname[16];

	status = acpi_hp_csr_space(obj, &csr_base, &csr_length);
	if (ACPI_FAILURE(status))
		return status;

	/*
	 * Only SBA shows up in ACPI namespace, so its CSR space
	 * includes both SBA and IOC.  Make SBA and IOC show up
	 * separately in PCI space.
	 */
	sprintf(fullname, "%s SBA", name);
	hpzx1_fake_pci_dev(fullname, 0, csr_base, 0x1000);
	sprintf(fullname, "%s IOC", name);
	hpzx1_fake_pci_dev(fullname, 0, csr_base + 0x1000, 0x1000);

	return AE_OK;
}

static acpi_status
hpzx1_lba_probe(acpi_handle obj, u32 depth, void *context, void **ret)
{
	u64 csr_base = 0, csr_length = 0;
	acpi_status status;
	acpi_native_uint busnum;
	char *name = context;
	char fullname[32];

	status = acpi_hp_csr_space(obj, &csr_base, &csr_length);
	if (ACPI_FAILURE(status))
		return status;

	status = acpi_evaluate_integer(obj, METHOD_NAME__BBN, NULL, &busnum);
	if (ACPI_FAILURE(status)) {
		printk(KERN_WARNING PFX "evaluate _BBN fail=0x%x\n", status);
		busnum = 0;	// no _BBN; stick it on bus 0
	}

	sprintf(fullname, "%s _BBN 0x%02x", name, (unsigned int) busnum);
	hpzx1_fake_pci_dev(fullname, busnum, csr_base, csr_length);

	return AE_OK;
}

static void
hpzx1_acpi_dev_init(void)
{
	extern struct pci_ops *pci_root_ops;

	orig_pci_ops = pci_root_ops;

	/*
	 * Make fake PCI devices for the following hardware in the
	 * ACPI namespace.  This makes it more convenient for drivers
	 * because they can claim these devices based on PCI
	 * information, rather than needing to know about ACPI.  The
	 * 64-bit "HPA" space for this hardware is available as BAR
	 * 0/1.
	 *
	 * HWP0001: Single IOC SBA w/o IOC in namespace
	 * HWP0002: LBA device
	 * HWP0003: AGP LBA device
	 */
	acpi_get_devices("HWP0001", hpzx1_sba_probe, "HWP0001", NULL);
	acpi_get_devices("HWP0002", hpzx1_lba_probe, "HWP0002 PCI LBA", NULL);
	acpi_get_devices("HWP0003", hpzx1_lba_probe, "HWP0003 AGP LBA", NULL);
}

extern void sba_init(void);

void
hpzx1_pci_fixup (int phase)
{
	iosapic_pci_fixup(phase);
	switch (phase) {
	      case 0:
		/* zx1 has a hardware I/O TLB which lets us DMA from any device to any address */
		MAX_DMA_ADDRESS = ~0UL;
		break;

	      case 1:
		hpzx1_acpi_dev_init();
		sba_init();
		break;
	}
}
