/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * SNI specific PCI support for RM200/RM300.
 *
 * Copyright (C) 1997 - 2000 Ralf Baechle
 */
#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/types.h>
#include <asm/byteorder.h>
#include <asm/sni.h>

#if 0
/* To do:  Bring this uptodate ...  */
static void pcimt_pcibios_fixup (void)
{
	struct pci_dev *dev;

	pci_for_each_dev(dev) {
		/*
		 * TODO: Take care of RM300 revision D boards for where the
		 * network slot became an ordinary PCI slot.
		 */
		if (dev->devfn == PCI_DEVFN(1, 0)) {
			/* Evil hack ...  */
			set_c0_config(CONF_CM_CMASK, CONF_CM_CACHABLE_NO_WA);
			dev->irq = PCIMT_IRQ_SCSI;
			continue;
		}
		if (dev->devfn == PCI_DEVFN(2, 0)) {
			dev->irq = PCIMT_IRQ_ETHERNET;
			continue;
		}

		switch(dev->irq) {
		case 1 ... 4:
			dev->irq += PCIMT_IRQ_INTA - 1;
			break;
		case 0:
			break;
		default:
			printk("PCI device on bus %d, dev %d, function %d "
			       "impossible interrupt configured.\n",
			       dev->bus->number, PCI_SLOT(dev->devfn),
			       PCI_SLOT(dev->devfn));
		}
	}
}
#endif

void __init
pcibios_fixup_bus(struct pci_bus *b)
{
}

extern struct pci_ops sni_pci_ops;

void __init pcibios_init(void)
{
	struct pci_ops *ops = &sni_pci_ops;

	pci_scan_bus(0, ops, NULL);
}

unsigned __init int pcibios_assign_all_busses(void)
{
	return 0;
}

struct pci_fixup pcibios_fixups[] = {
	{ 0 }
};
