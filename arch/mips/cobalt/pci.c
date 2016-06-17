/*
 * Cobalt Qube/Raq PCI support
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1995, 1996, 1997 by Ralf Baechle
 * Copyright (C) 2001, 2002, 2003 by Liam Davies (ldavies@agile.tv)
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <asm/pci.h>
#include <asm/io.h>
#include <asm/gt64120/gt64120.h>
#include <asm/cobalt/cobalt.h>

#ifdef CONFIG_PCI

int cobalt_board_id;

static void qube_expansion_slot_bist(struct pci_dev *dev)
{
	unsigned char ctrl;
	int timeout = 100000;

	pci_read_config_byte(dev, PCI_BIST, &ctrl);
	if(!(ctrl & PCI_BIST_CAPABLE))
		return;

	pci_write_config_byte(dev, PCI_BIST, ctrl|PCI_BIST_START);
	do {
		pci_read_config_byte(dev, PCI_BIST, &ctrl);
		if(!(ctrl & PCI_BIST_START))
			break;
	} while(--timeout > 0);
	if((timeout <= 0) || (ctrl & PCI_BIST_CODE_MASK))
		printk("PCI: Expansion slot card failed BIST with code %x\n",
		       (ctrl & PCI_BIST_CODE_MASK));
}

static void qube_expansion_slot_fixup(struct pci_dev *dev)
{
	unsigned short pci_cmd;
	unsigned long ioaddr_base = 0x10108000; /* It's magic, ask Doug. */
	unsigned long memaddr_base = 0x12001000;
	int i;

	/* Enable bits in COMMAND so driver can talk to it. */
	pci_read_config_word(dev, PCI_COMMAND, &pci_cmd);
	pci_cmd |= (PCI_COMMAND_IO | PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER);
	pci_write_config_word(dev, PCI_COMMAND, pci_cmd);

	/* Give it a working IRQ. */
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, COBALT_QUBE_SLOT_IRQ);
	dev->irq = COBALT_QUBE_SLOT_IRQ;

	ioaddr_base += 0x2000 * PCI_FUNC(dev->devfn);
	memaddr_base += 0x2000 * PCI_FUNC(dev->devfn);

	/* Fixup base addresses, we only support I/O at the moment. */
	for(i = 0; i <= 5; i++) {
		unsigned int regaddr = (PCI_BASE_ADDRESS_0 + (i * 4));
		unsigned int rval, mask, size, alignme, aspace;
		unsigned long *basep = &ioaddr_base;

		/* Check type first, punt if non-IO. */
		pci_read_config_dword(dev, regaddr, &rval);
		aspace = (rval & PCI_BASE_ADDRESS_SPACE);
		if(aspace != PCI_BASE_ADDRESS_SPACE_IO)
			basep = &memaddr_base;

		/* Figure out how much it wants, if anything. */
		pci_write_config_dword(dev, regaddr, 0xffffffff);
		pci_read_config_dword(dev, regaddr, &rval);

		/* Unused? */
		if(rval == 0)
			continue;

		rval &= PCI_BASE_ADDRESS_IO_MASK;
		mask = (~rval << 1) | 0x1;
		size = (mask & rval) & 0xffffffff;
		alignme = size;
		if(alignme < 0x400)
			alignme = 0x400;
		rval = ((*basep + (alignme - 1)) & ~(alignme - 1));
		*basep = (rval + size);
		pci_write_config_dword(dev, regaddr, rval | aspace);
		dev->resource[i].start = rval;
		dev->resource[i].end = *basep - 1;
		if(aspace == PCI_BASE_ADDRESS_SPACE_IO) {
			dev->resource[i].start -= 0x10000000;
			dev->resource[i].end -= 0x10000000;
		}
	}
	qube_expansion_slot_bist(dev);
}

static void qube_raq_via_bmIDE_fixup(struct pci_dev *dev)
{
	unsigned short cfgword;
	unsigned char lt;

	/* Enable Bus Mastering and fast back to back. */
	pci_read_config_word(dev, PCI_COMMAND, &cfgword);
	cfgword |= (PCI_COMMAND_FAST_BACK | PCI_COMMAND_MASTER);
	pci_write_config_word(dev, PCI_COMMAND, cfgword);

	/* Enable both ide interfaces. ROM only enables primary one.  */
	pci_write_config_byte(dev, 0x40, 0xb);

	/* Set latency timer to reasonable value. */
	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &lt);
	if(lt < 64)
		pci_write_config_byte(dev, PCI_LATENCY_TIMER, 64);
	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, 7);
}

static void qube_raq_tulip_fixup(struct pci_dev *dev)
{
	unsigned short pci_cmd;

	/* Fixup the first tulip located at device PCICONF_ETH0 */
	if (PCI_SLOT(dev->devfn) == COBALT_PCICONF_ETH0) {
		/* Setup the first Tulip */
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE,
				      COBALT_ETH0_IRQ);
		dev->irq = COBALT_ETH0_IRQ;

		dev->resource[0].start = 0x100000;
		dev->resource[0].end = 0x10007f;

		dev->resource[1].start = 0x12000000;
		dev->resource[1].end = dev->resource[1].start + 0x3ff;
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_1, dev->resource[1].start);

	/* Fixup the second tulip located at device PCICONF_ETH1 */
	} else if (PCI_SLOT(dev->devfn) == COBALT_PCICONF_ETH1) {

		/* Enable the second Tulip device. */
		pci_read_config_word(dev, PCI_COMMAND, &pci_cmd);
		pci_cmd |= (PCI_COMMAND_IO | PCI_COMMAND_MASTER);
		pci_write_config_word(dev, PCI_COMMAND, pci_cmd);

		/* Give it it's IRQ. */
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE,
                                      COBALT_ETH1_IRQ);
		dev->irq = COBALT_ETH1_IRQ;

		/* And finally, a usable I/O space allocation, right after what
		 * the first Tulip uses.
		 */
		dev->resource[0].start = 0x101000;
		dev->resource[0].end = 0x10107f;

		dev->resource[1].start = 0x12000400;
		dev->resource[1].end = dev->resource[1].start + 0x3ff;
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_1, dev->resource[1].start);
	}
}

static void qube_raq_scsi_fixup(struct pci_dev *dev)
{
	unsigned short pci_cmd;

        /*
         * Tell the SCSI device that we expect an interrupt at
         * IRQ 7 and not the default 0.
         */
        pci_write_config_byte(dev, PCI_INTERRUPT_LINE, COBALT_SCSI_IRQ);
	dev->irq = COBALT_SCSI_IRQ;

	if (cobalt_board_id == COBALT_BRD_ID_RAQ2) {

		/* Enable the device. */
		pci_read_config_word(dev, PCI_COMMAND, &pci_cmd);

		pci_cmd |= (PCI_COMMAND_IO | PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY
			| PCI_COMMAND_INVALIDATE);
		pci_write_config_word(dev, PCI_COMMAND, pci_cmd);

		/* Give it it's RAQ IRQ. */
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, COBALT_RAQ_SCSI_IRQ);
		dev->irq = COBALT_RAQ_SCSI_IRQ;

		/* And finally, a usable I/O space allocation, right after what
		 * the second Tulip uses.
		 */
		dev->resource[0].start = 0x102000;
		dev->resource[0].end = dev->resource[0].start + 0xff;
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_0, 0x10102000);

		pci_write_config_dword(dev, PCI_BASE_ADDRESS_1, 0x00002000);
		pci_write_config_dword(dev, PCI_BASE_ADDRESS_2, 0x00100000);
	}
}

static void qube_raq_galileo_fixup(struct pci_dev *dev)
{
	unsigned short galileo_id;

	/* Fix PCI latency-timer and cache-line-size values in Galileo
	 * host bridge.
	 */
	pci_write_config_byte(dev, PCI_LATENCY_TIMER, 64);
	pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, 7);

	/* On all machines prior to Q2, we had the STOP line disconnected
	 * from Galileo to VIA on PCI.  The new Galileo does not function
	 * correctly unless we have it connected.
	 *
	 * Therefore we must set the disconnect/retry cycle values to
	 * something sensible when using the new Galileo.
	 */
	pci_read_config_word(dev, PCI_REVISION_ID, &galileo_id);
	galileo_id &= 0xff;     /* mask off class info */
	if (galileo_id >= 0x10) {
		/* New Galileo, assumes PCI stop line to VIA is connected. */
		GALILEO_OUTL(0x4020, GT_PCI0_TOR_OFS);
	} else if (galileo_id == 0x1 || galileo_id == 0x2) {
		signed int timeo;
		/* XXX WE MUST DO THIS ELSE GALILEO LOCKS UP! -DaveM */
		timeo = GALILEO_INL(GT_PCI0_TOR_OFS);
		/* Old Galileo, assumes PCI STOP line to VIA is disconnected. */
		GALILEO_OUTL(0xffff, GT_PCI0_TOR_OFS);
	}
}

static void
qube_pcibios_fixup(struct pci_dev *dev)
{
	if (PCI_SLOT(dev->devfn) == COBALT_PCICONF_PCISLOT) {
		unsigned int tmp;

		/* See if there is a device in the expansion slot, if so
		 * discover its resources and fixup whatever we need to
		 */
		pci_read_config_dword(dev, PCI_VENDOR_ID, &tmp);
		if(tmp != 0xffffffff && tmp != 0x00000000)
			qube_expansion_slot_fixup(dev);
	}
}

struct pci_fixup pcibios_fixups[] = {
	{ PCI_FIXUP_HEADER, PCI_VENDOR_ID_VIA, PCI_DEVICE_ID_VIA_82C586_1, qube_raq_via_bmIDE_fixup },
	{ PCI_FIXUP_HEADER, PCI_VENDOR_ID_DEC, PCI_DEVICE_ID_DEC_21142, qube_raq_tulip_fixup },
	{ PCI_FIXUP_HEADER, PCI_VENDOR_ID_GALILEO, PCI_ANY_ID, qube_raq_galileo_fixup },
	{ PCI_FIXUP_HEADER, PCI_VENDOR_ID_NCR, PCI_DEVICE_ID_NCR_53C860, qube_raq_scsi_fixup },
	{ PCI_FIXUP_HEADER, PCI_ANY_ID, PCI_ANY_ID, qube_pcibios_fixup }
};


static __inline__ int pci_range_ck(struct pci_dev *dev)
{
       if ((dev->bus->number == 0)
           && ((PCI_SLOT (dev->devfn) == 0)
               || ((PCI_SLOT (dev->devfn) > 6)
                   && (PCI_SLOT (dev->devfn) <= 12))))
		return 0;  /* OK device number  */

	return -1;  /* NOT ok device number */
}

#define PCI_CFG_SET(dev,where) \
       GALILEO_OUTL((0x80000000 | (((dev)->devfn) << 8) | \
                           (where)), GT_PCI0_CFGADDR_OFS)

static int qube_pci_read_config_dword (struct pci_dev *dev,
                                          int where,
                                          u32 *val)
{
	if (where & 0x3)
		return PCIBIOS_BAD_REGISTER_NUMBER;
	if (pci_range_ck (dev)) {
		*val = 0xFFFFFFFF;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	PCI_CFG_SET(dev, where);
	*val = GALILEO_INL(GT_PCI0_CFGDATA_OFS);
	return PCIBIOS_SUCCESSFUL;
}

static int qube_pci_read_config_word (struct pci_dev *dev,
                                         int where,
                                         u16 *val)
{
        if (where & 0x1)
		return PCIBIOS_BAD_REGISTER_NUMBER;
	if (pci_range_ck (dev)) {
		*val = 0xffff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	PCI_CFG_SET(dev, (where & ~0x3));
	*val = GALILEO_INL(GT_PCI0_CFGDATA_OFS) >> ((where & 3) * 8);
	return PCIBIOS_SUCCESSFUL;
}

static int qube_pci_read_config_byte (struct pci_dev *dev,
                                         int where,
                                         u8 *val)
{
	if (pci_range_ck (dev)) {
		*val = 0xff;
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	PCI_CFG_SET(dev, (where & ~0x3));
	*val = GALILEO_INL(GT_PCI0_CFGDATA_OFS) >> ((where & 3) * 8);
	return PCIBIOS_SUCCESSFUL;
}

static int qube_pci_write_config_dword (struct pci_dev *dev,
                                           int where,
                                           u32 val)
{
	if(where & 0x3)
		return PCIBIOS_BAD_REGISTER_NUMBER;
	if (pci_range_ck (dev))
		return PCIBIOS_DEVICE_NOT_FOUND;
	PCI_CFG_SET(dev, where);
	GALILEO_OUTL(val, GT_PCI0_CFGDATA_OFS);
	return PCIBIOS_SUCCESSFUL;
}

static int
qube_pci_write_config_word (struct pci_dev *dev,
                                int where,
                               u16 val)
{
	unsigned long tmp;

	if (where & 0x1)
		return PCIBIOS_BAD_REGISTER_NUMBER;
	if (pci_range_ck (dev))
		return PCIBIOS_DEVICE_NOT_FOUND;
	PCI_CFG_SET(dev, (where & ~0x3));
	tmp = GALILEO_INL(GT_PCI0_CFGDATA_OFS);
	tmp &= ~(0xffff << ((where & 0x3) * 8));
	tmp |=  (val << ((where & 0x3) * 8));
	GALILEO_OUTL(tmp, GT_PCI0_CFGDATA_OFS);
	return PCIBIOS_SUCCESSFUL;
}

static int
qube_pci_write_config_byte (struct pci_dev *dev,
                                int where,
                               u8 val)
{
	unsigned long tmp;

	if (pci_range_ck (dev))
		return PCIBIOS_DEVICE_NOT_FOUND;
	PCI_CFG_SET(dev, (where & ~0x3));
	tmp = GALILEO_INL(GT_PCI0_CFGDATA_OFS);
	tmp &= ~(0xff << ((where & 0x3) * 8));
	tmp |=  (val << ((where & 0x3) * 8));
	GALILEO_OUTL(tmp, GT_PCI0_CFGDATA_OFS);
	return PCIBIOS_SUCCESSFUL;
}


struct pci_ops qube_pci_ops = {
	qube_pci_read_config_byte,
	qube_pci_read_config_word,
	qube_pci_read_config_dword,
	qube_pci_write_config_byte,
	qube_pci_write_config_word,
	qube_pci_write_config_dword
};

void __init pcibios_init(void)
{
	struct pci_dev dev;

	printk("PCI: Probing PCI hardware\n");

	/* Read the cobalt id register out of the PCI config space */
	dev.devfn = PCI_DEVFN(COBALT_PCICONF_VIA, 0);
	PCI_CFG_SET(&dev, (VIA_COBALT_BRD_ID_REG & ~0x3));
	cobalt_board_id = GALILEO_INL(GT_PCI0_CFGDATA_OFS) >> ((VIA_COBALT_BRD_ID_REG & 3) * 8);
	cobalt_board_id = VIA_COBALT_BRD_REG_to_ID(cobalt_board_id);

	printk("Cobalt Board ID: %d\n", cobalt_board_id);

	ioport_resource.start = 0x00000000;
	ioport_resource.end = 0x0fffffff;

	iomem_resource.start = 0x01000000;
	iomem_resource.end = 0xffffffff;

	pci_scan_bus(0, &qube_pci_ops, NULL);
}

void __init pcibios_fixup_bus(struct pci_bus *bus)
{
	/* We don't have sub-busses to fixup here */
}

unsigned int __init pcibios_assign_all_busses(void)
{
	return 1;
}

#endif /* CONFIG_PCI */
