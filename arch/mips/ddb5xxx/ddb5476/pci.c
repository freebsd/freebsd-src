#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>

#include <asm/pci_channel.h>
#include <asm/debug.h>

#include <asm/ddb5xxx/ddb5xxx.h>

static struct resource extpci_io_resource = {
	"pci IO space",
	0x1000,				/* leave some room for ISA bus */
	DDB_PCI_IO_SIZE -1,
	IORESOURCE_IO};

static struct resource extpci_mem_resource = {
	"pci memory space",
	DDB_PCI_MEM_BASE + 0x00100000,	/* leave 1 MB for RTC */
	DDB_PCI_MEM_BASE + DDB_PCI_MEM_SIZE -1,
	IORESOURCE_MEM};

extern struct pci_ops ddb5476_ext_pci_ops;

struct pci_channel mips_pci_channels[] = {
	{ &ddb5476_ext_pci_ops, &extpci_io_resource, &extpci_mem_resource },
	{ NULL, NULL, NULL}
};


/*
 * we fix up irqs based on the slot number.
 * The first entry is at AD:11.
 *
 * This does not work for devices on sub-buses yet.
 */

/*
 * temporary
 */

#define		PCI_EXT_INTA		8
#define		PCI_EXT_INTB		9
#define		PCI_EXT_INTC		10
#define		PCI_EXT_INTD		11
#define		PCI_EXT_INTE		12

/*
 * based on ddb5477 manual page 11
 */
#define		MAX_SLOT_NUM		21
static unsigned char irq_map[MAX_SLOT_NUM] = {
	/* SLOT:  0, AD:11 */ 0xff,
	/* SLOT:  1, AD:12 */ 0xff,
	/* SLOT:  2, AD:13 */ 9,		/* USB */
	/* SLOT:  3, AD:14 */ 10,		/* PMU */
	/* SLOT:  4, AD:15 */ 0xff,
	/* SLOT:  5, AD:16 */ 0x0,		/* P2P bridge */
	/* SLOT:  6, AD:17 */ nile4_to_irq(PCI_EXT_INTB),
	/* SLOT:  7, AD:18 */ nile4_to_irq(PCI_EXT_INTC),
	/* SLOT:  8, AD:19 */ nile4_to_irq(PCI_EXT_INTD),
	/* SLOT:  9, AD:20 */ nile4_to_irq(PCI_EXT_INTA),
	/* SLOT: 10, AD:21 */ 0xff,
	/* SLOT: 11, AD:22 */ 0xff,
	/* SLOT: 12, AD:23 */ 0xff,
	/* SLOT: 13, AD:24 */ 14,		/* HD controller, M5229 */
	/* SLOT: 14, AD:25 */ 0xff,
	/* SLOT: 15, AD:26 */ 0xff,
	/* SLOT: 16, AD:27 */ 0xff,
	/* SLOT: 17, AD:28 */ 0xff,
	/* SLOT: 18, AD:29 */ 0xff,
	/* SLOT: 19, AD:30 */ 0xff,
	/* SLOT: 20, AD:31 */ 0xff
};

extern int vrc5477_irq_to_irq(int irq);
void __init pcibios_fixup_irqs(void)
{
        struct pci_dev *dev;
        int slot_num;

	pci_for_each_dev(dev) {
		slot_num = PCI_SLOT(dev->devfn);

		/* we don't do IRQ fixup for sub-bus yet */
		if (dev->bus->parent != NULL) {
			db_run(printk("Don't know how to fixup irq for PCI device %d on sub-bus %d\n",
				slot_num, dev->bus->number));
			continue;
		}

		db_assert(slot_num < MAX_SLOT_NUM);
		db_assert(irq_map[slot_num] != 0xff);

		pci_write_config_byte(dev,
				      PCI_INTERRUPT_LINE,
				      irq_map[slot_num]);
		dev->irq = irq_map[slot_num];
	}
}

void __init ddb_pci_reset_bus(void)
{
	u32 temp;

	/*
	 * I am not sure about the "official" procedure, the following
	 * steps work as far as I know:
	 * We first set PCI cold reset bit (bit 31) in PCICTRL-H.
	 * Then we clear the PCI warm reset bit (bit 30) to 0 in PCICTRL-H.
	 * The same is true for both PCI channels.
	 */
	temp = ddb_in32(DDB_PCICTRL+4);
	temp |= 0x80000000;
	ddb_out32(DDB_PCICTRL+4, temp);
	temp &= ~0xc0000000;
	ddb_out32(DDB_PCICTRL+4, temp);

}

unsigned __init int pcibios_assign_all_busses(void)
{
	/* we hope pci_auto has assigned the bus numbers to all buses */
	return 1;
}

void __init pcibios_fixup_resources(struct pci_dev *dev)
{
}

void __init pcibios_fixup(void)
{
}

