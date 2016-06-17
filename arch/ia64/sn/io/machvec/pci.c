/* 
 *
 * SNI64 specific PCI support for SNI IO.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1997, 1998, 2000-2003 Silicon Graphics, Inc.  All rights reserved.
 */
#include <linux/init.h>
#include <linux/types.h>
#include <linux/config.h>
#include <linux/pci.h>
#include <asm/sn/types.h>
#include <asm/sn/sgi.h>
#include <asm/sn/io.h>
#include <asm/sn/driver.h>
#include <asm/sn/iograph.h>
#include <asm/param.h>
#include <asm/sn/pio.h>
#include <asm/sn/xtalk/xwidget.h>
#include <asm/sn/sn_private.h>
#include <asm/sn/addrs.h>
#include <asm/sn/invent.h>
#include <asm/sn/hcl.h>
#include <asm/sn/hcl_util.h>
#include <asm/sn/pci/pciio.h>
#include <asm/sn/pci/pcibr.h>
#include <asm/sn/pci/pcibr_private.h>
#include <asm/sn/pci/bridge.h>

#ifdef DEBUG_CONFIG
#define DBG(x...) printk(x)
#else
#define DBG(x...)
#endif



#ifdef CONFIG_PCI

extern vertex_hdl_t pci_bus_to_vertex(unsigned char);
extern vertex_hdl_t devfn_to_vertex(unsigned char bus, unsigned char devfn);

/*
 * snia64_read_config_byte - Read a byte from the config area of the device.
 */
static int snia64_read_config_byte (struct pci_dev *dev,
                                   int where, unsigned char *val)
{
	unsigned long res = 0;
	unsigned size = 1;
	vertex_hdl_t device_vertex;

	if ( (dev == (struct pci_dev *)0) || (val == (unsigned char *)0) ) {
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	device_vertex = devfn_to_vertex(dev->bus->number, dev->devfn);
	if (!device_vertex) {
		DBG("%s : nonexistent device: bus= 0x%x  slot= 0x%x  func= 0x%x\n", 
		__FUNCTION__, dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
		return(-1);
	}
	res = pciio_config_get(device_vertex, (unsigned) where, size);
	*val = (unsigned char) res;
	return PCIBIOS_SUCCESSFUL;
}

/*
 * snia64_read_config_word - Read 2 bytes from the config area of the device.
 */
static int snia64_read_config_word (struct pci_dev *dev,
                                   int where, unsigned short *val)
{
	unsigned long res = 0;
	unsigned size = 2; /* 2 bytes */
	vertex_hdl_t device_vertex;

	if ( (dev == (struct pci_dev *)0) || (val == (unsigned short *)0) ) {
		return PCIBIOS_DEVICE_NOT_FOUND;
	}
	device_vertex = devfn_to_vertex(dev->bus->number, dev->devfn);
	if (!device_vertex) {
		DBG("%s : nonexistent device: bus= 0x%x  slot= 0x%x  func= 0x%x\n", 
		__FUNCTION__, dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
		return(-1);
	}
	res = pciio_config_get(device_vertex, (unsigned) where, size);
	*val = (unsigned short) res;
	return PCIBIOS_SUCCESSFUL;
}

/*
 * snia64_read_config_dword - Read 4 bytes from the config area of the device.
 */
static int snia64_read_config_dword (struct pci_dev *dev,
                                    int where, unsigned int *val)
{
	unsigned long res = 0;
	unsigned size = 4; /* 4 bytes */
	vertex_hdl_t device_vertex;

	if (where & 3) {
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}
	if ( (dev == (struct pci_dev *)0) || (val == (unsigned int *)0) ) {
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	device_vertex = devfn_to_vertex(dev->bus->number, dev->devfn);
	if (!device_vertex) {
		DBG("%s : nonexistent device: bus= 0x%x  slot= 0x%x  func= 0x%x\n", 
		__FUNCTION__, dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
		return(-1);
	}
	res = pciio_config_get(device_vertex, (unsigned) where, size);
	*val = (unsigned int) res;
	return PCIBIOS_SUCCESSFUL;
}

/*
 * snia64_write_config_byte - Writes 1 byte to the config area of the device.
 */
static int snia64_write_config_byte (struct pci_dev *dev,
                                    int where, unsigned char val)
{
	vertex_hdl_t device_vertex;

	if ( dev == (struct pci_dev *)0 ) {
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	device_vertex = devfn_to_vertex(dev->bus->number, dev->devfn);
	if (!device_vertex) {
		DBG("%s : nonexistent device: bus= 0x%x  slot= 0x%x  func= 0x%x\n", 
		__FUNCTION__, dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
		return(-1);
	}
	pciio_config_set( device_vertex, (unsigned)where, 1, (uint64_t) val);

	return PCIBIOS_SUCCESSFUL;
}

/*
 * snia64_write_config_word - Writes 2 bytes to the config area of the device.
 */
static int snia64_write_config_word (struct pci_dev *dev,
                                    int where, unsigned short val)
{
	vertex_hdl_t device_vertex = NULL;

	if (where & 1) {
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}
	if ( dev == (struct pci_dev *)0 ) {
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	device_vertex = devfn_to_vertex(dev->bus->number, dev->devfn);
	if (!device_vertex) {
		DBG("%s : nonexistent device: bus= 0x%x  slot= 0x%x  func= 0x%x\n", 
		__FUNCTION__, dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
		return(-1);
	}
	pciio_config_set( device_vertex, (unsigned)where, 2, (uint64_t) val);

	return PCIBIOS_SUCCESSFUL;
}

/*
 * snia64_write_config_dword - Writes 4 bytes to the config area of the device.
 */
static int snia64_write_config_dword (struct pci_dev *dev,
                                     int where, unsigned int val)
{
	vertex_hdl_t device_vertex;

	if (where & 3) {
		return PCIBIOS_BAD_REGISTER_NUMBER;
	}
	if ( dev == (struct pci_dev *)0 ) {
		return PCIBIOS_DEVICE_NOT_FOUND;
	}

	device_vertex = devfn_to_vertex(dev->bus->number, dev->devfn);
	if (!device_vertex) {
		DBG("%s : nonexistent device: bus= 0x%x  slot= 0x%x  func= 0x%x\n", 
		__FUNCTION__, dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
		return(-1);
	}
	pciio_config_set( device_vertex, (unsigned)where, 4, (uint64_t) val);

	return PCIBIOS_SUCCESSFUL;
}

static struct pci_ops snia64_pci_ops = {
	snia64_read_config_byte,
	snia64_read_config_word,
	snia64_read_config_dword,
	snia64_write_config_byte,
	snia64_write_config_word,
	snia64_write_config_dword
};

/*
 * snia64_pci_find_bios - SNIA64 pci_find_bios() platform specific code.
 */
void __init
sn_pci_find_bios(void)
{
	extern struct pci_ops *pci_root_ops;
	/*
	 * Go initialize our IO Infrastructure ..
	 */
	extern void sgi_master_io_infr_init(void);

	sgi_master_io_infr_init();

	/* sn_io_infrastructure_init(); */
	pci_root_ops = &snia64_pci_ops;
}
#else
void sn_pci_find_bios(void) {}
struct list_head pci_root_buses;
struct list_head pci_root_buses;
struct list_head pci_devices;

#endif /* CONFIG_PCI */
