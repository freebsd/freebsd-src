/*
 *	$Id: compat.c,v 1.1 1998/02/16 10:35:50 mj Exp $
 *
 *	PCI Bus Services -- Function For Backward Compatibility
 *
 *	Copyright 1998--2000 Martin Mares <mj@ucw.cz>
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>

int
pcibios_present(void)
{
	return !list_empty(&pci_devices);
}

int
pcibios_find_class(unsigned int class, unsigned short index, unsigned char *bus, unsigned char *devfn)
{
	const struct pci_dev *dev = NULL;
	int cnt = 0;

	while ((dev = pci_find_class(class, dev)))
		if (index == cnt++) {
			*bus = dev->bus->number;
			*devfn = dev->devfn;
			return PCIBIOS_SUCCESSFUL;
		}
	return PCIBIOS_DEVICE_NOT_FOUND;
}


int
pcibios_find_device(unsigned short vendor, unsigned short device, unsigned short index,
		    unsigned char *bus, unsigned char *devfn)
{
	const struct pci_dev *dev = NULL;
	int cnt = 0;

	while ((dev = pci_find_device(vendor, device, dev)))
		if (index == cnt++) {
			*bus = dev->bus->number;
			*devfn = dev->devfn;
			return PCIBIOS_SUCCESSFUL;
		}
	return PCIBIOS_DEVICE_NOT_FOUND;
}

#define PCI_OP(rw,size,type)							\
int pcibios_##rw##_config_##size (unsigned char bus, unsigned char dev_fn,	\
				  unsigned char where, unsigned type val)	\
{										\
	struct pci_dev *dev = pci_find_slot(bus, dev_fn);			\
	if (!dev) return PCIBIOS_DEVICE_NOT_FOUND;				\
	return pci_##rw##_config_##size(dev, where, val);			\
}

PCI_OP(read, byte, char *)
PCI_OP(read, word, short *)
PCI_OP(read, dword, int *)
PCI_OP(write, byte, char)
PCI_OP(write, word, short)
PCI_OP(write, dword, int)
