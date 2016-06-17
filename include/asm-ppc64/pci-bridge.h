#ifdef __KERNEL__
#ifndef _ASM_PCI_BRIDGE_H
#define _ASM_PCI_BRIDGE_H

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

struct device_node;
struct pci_controller;

/*
 * pci_io_base returns the memory address at which you can access
 * the I/O space for PCI bus number `bus' (or NULL on error).
 */
extern void *pci_bus_io_base(unsigned int bus);
extern unsigned long pci_bus_io_base_phys(unsigned int bus);
extern unsigned long pci_bus_mem_base_phys(unsigned int bus);

/* Get the PCI host controller for a bus */
extern struct pci_controller* pci_bus_to_hose(int bus);

/* Get the PCI host controller for an OF device */
extern struct pci_controller*
pci_find_hose_for_OF_device(struct device_node* node);

enum phb_types { 
	phb_type_unknown    = 0x0,
	phb_type_hypervisor = 0x1,
	phb_type_python     = 0x10,
	phb_type_speedwagon = 0x11
};

/*
 * Structure of a PCI controller (host bridge)
 */
struct pci_controller {
	char what[8];                     /* Eye catcher      */
	enum phb_types type;              /* Type of hardware */
	struct pci_controller *next;
	struct pci_bus *bus;
	void *arch_data;

	int first_busno;
	int last_busno;

	void *io_base_virt;
	unsigned long io_base_phys;

	/* Some machines (PReP) have a non 1:1 mapping of
	 * the PCI memory space in the CPU bus space
	 */
	unsigned long pci_mem_offset;
	unsigned long pci_io_offset;

	struct pci_ops *ops;
	volatile unsigned long *cfg_addr;
	volatile unsigned char *cfg_data;
	volatile unsigned long *phb_regs;
	volatile unsigned long *chip_regs;

	/* Currently, we limit ourselves to 1 IO range and 3 mem
	 * ranges since the common pci_bus structure can't handle more
	 */
	struct resource io_resource;
	struct resource mem_resources[3];
	int mem_resource_count;
	int    global_number;		
	int    local_number;		
	int    system_bus_number;	
	unsigned long buid;
	unsigned long dma_window_base_cur;
	unsigned long dma_window_size;
};


/* This version handles the new Uni-N host bridge, the iobase is now
 * a per-device thing. I also added the memory base so PReP can
 * be fixed to return 0xc0000000 (I didn't actually implement it)
 *
 * pci_dev_io_base() returns either a virtual (ioremap'ed) address or
 * a physical address. In-kernel clients will use logical while the
 * sys_pciconfig_iobase syscall returns a physical one to userland.
 */
void *pci_dev_io_base(unsigned char bus, unsigned char devfn, int physical);
void *pci_dev_mem_base(unsigned char bus, unsigned char devfn);

/* Returns the root-bridge number (Uni-N number) of a device */
int pci_dev_root_bridge(unsigned char bus, unsigned char devfn);

/*
 * pci_device_loc returns the bus number and device/function number
 * for a device on a PCI bus, given its device_node struct.
 * It returns 0 if OK, -1 on error.
 */
int pci_device_loc(struct device_node *dev, unsigned char *bus_ptr,
		   unsigned char *devfn_ptr);

struct bridge_data {
	volatile unsigned int *cfg_addr;
	volatile unsigned char *cfg_data;
	void *io_base;		/* virtual */
	unsigned long io_base_phys;
	int bus_number;
	int max_bus;
	struct bridge_data *next;
	struct device_node *node;
};

#endif
#endif /* __KERNEL__ */
