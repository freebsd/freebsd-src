#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>

#include <asm/pci_channel.h>
#include <asm/debug.h>

#include <asm/ddb5xxx/ddb5xxx.h>

static struct resource extpci_io_resource = {
	"pci IO space",
	0x1000,			/* leave some room for ISA bus */
	DDB_PCI_IO_SIZE - 1,
	IORESOURCE_IO
};

static struct resource extpci_mem_resource = {
	"pci memory space",
	DDB_PCI_MEM_BASE + 0x00100000,	/* leave 1 MB for RTC */
	DDB_PCI_MEM_BASE + DDB_PCI_MEM_SIZE - 1,
	IORESOURCE_MEM
};

extern struct pci_ops ddb5476_ext_pci_ops;

struct pci_channel mips_pci_channels[] = {
	{&ddb5476_ext_pci_ops, &extpci_io_resource, &extpci_mem_resource},
	{NULL, NULL, NULL}
};

#define     PCI_EXT_INTA        8
#define     PCI_EXT_INTB        9
#define     PCI_EXT_INTC        10
#define     PCI_EXT_INTD        11
#define     PCI_EXT_INTE        12

#define     MAX_SLOT_NUM        14

static unsigned char irq_map[MAX_SLOT_NUM] = {
	/* SLOT:  0 */ nile4_to_irq(PCI_EXT_INTE),
	/* SLOT:  1 */ nile4_to_irq(PCI_EXT_INTA),
	/* SLOT:  2 */ nile4_to_irq(PCI_EXT_INTA),
	/* SLOT:  3 */ nile4_to_irq(PCI_EXT_INTB),
	/* SLOT:  4 */ nile4_to_irq(PCI_EXT_INTC),
	/* SLOT:  5 */ nile4_to_irq(NILE4_INT_UART),
	/* SLOT:  6 */ 0xff,
	/* SLOT:  7 */ 0xff,
	/* SLOT:  8 */ 0xff,
	/* SLOT:  9 */ 0xff,
	/* SLOT:  10 */ nile4_to_irq(PCI_EXT_INTE),
	/* SLOT:  11 */ 0xff,
	/* SLOT:  12 */ 0xff,
	/* SLOT:  13 */ nile4_to_irq(PCI_EXT_INTE),
};

void __init pcibios_fixup_irqs(void)
{

	struct pci_dev *dev;
	int slot_num;

	pci_for_each_dev(dev) {
		slot_num = PCI_SLOT(dev->devfn);
		db_assert(slot_num < MAX_SLOT_NUM);
		printk("irq_map[%d]: %02x\n", slot_num, irq_map[slot_num]);
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
	temp = ddb_in32(DDB_PCICTRL + 4);
	temp |= 0x80000000;
	ddb_out32(DDB_PCICTRL + 4, temp);
	temp &= ~0xc0000000;
	ddb_out32(DDB_PCICTRL + 4, temp);

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
	struct pci_dev *dev;

	pci_for_each_dev(dev) {
		if (dev->vendor == PCI_VENDOR_ID_AL &&
		    dev->device == PCI_DEVICE_ID_AL_M7101) {
			/*
			 * It's nice to have the LEDs on the GPIO pins
			 * available for debugging
			 */
			extern struct pci_dev *pci_pmu;
			u8 t8;

			pci_pmu = dev;	/* for LEDs D2 and D3 */
			/* Program the lines for LEDs D2 and D3 to output */
			pci_read_config_byte(dev, 0x7d, &t8);
			t8 |= 0xc0;
			pci_write_config_byte(dev, 0x7d, t8);
			/* Turn LEDs D2 and D3 off */
			pci_read_config_byte(dev, 0x7e, &t8);
			t8 |= 0xc0;
			pci_write_config_byte(dev, 0x7e, t8);

		}
	}
}
