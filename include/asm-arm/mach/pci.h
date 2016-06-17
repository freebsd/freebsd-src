/*
 *  linux/include/asm-arm/mach/pci.h
 *
 *  Copyright (C) 2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
struct hw_pci {
	/* Initialise the hardware */
	void		(*init)(void *);

	/* Setup bus resources */
	void		(*setup_resources)(struct resource **);

	/*
	 * This is the offset of PCI memory base registers
	 * to physical memory.
	 */
	unsigned long	mem_offset;

	/* IRQ swizzle */
	u8		(*swizzle)(struct pci_dev *dev, u8 *pin);

	/* IRQ mapping */
	int		(*map_irq)(struct pci_dev *dev, u8 slot, u8 pin);
};

extern u8 no_swizzle(struct pci_dev *dev, u8 *pin);
extern void __init dc21285_setup_resources(struct resource **resource);
extern void __init dc21285_init(void *sysdata);
extern void __init via82c505_init(void *sysdata);

