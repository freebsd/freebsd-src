/*
 *  linux/include/asm-parisc/ide.h
 *
 *  Copyright (C) 1994-1996  Linus Torvalds & authors
 */

/*
 *  This file contains the PARISC architecture specific IDE code.
 */

#ifndef __ASM_PARISC_IDE_H
#define __ASM_PARISC_IDE_H

#ifdef __KERNEL__

#include <linux/config.h>
#include <asm/superio.h>

#ifndef MAX_HWIFS
#define MAX_HWIFS	2
#endif

static __inline__ int ide_default_irq(ide_ioreg_t base)
{
	switch (base) {
#ifdef CONFIG_SUPERIO
		case 0x1f0: 
		case 0x170:
			return superio_get_ide_irq();
#endif /* CONFIG_SUPERIO */
		default:
			return 0;
	}
}

static __inline__ ide_ioreg_t ide_default_io_base(int index)
{
	switch (index) {
#ifdef CONFIG_SUPERIO 
		case 0:	return (superio_get_ide_irq() ? 0x1f0 : 0);
		case 1:	return (superio_get_ide_irq() ? 0x170 : 0);
#endif /* CONFIG_SUPERIO */
		default:
			return 0;
	}
}

static __inline__ void ide_init_hwif_ports(hw_regs_t *hw, ide_ioreg_t data_port, ide_ioreg_t ctrl_port, int *irq)
{
	ide_ioreg_t reg = data_port;
	int i;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		hw->io_ports[i] = reg;
		reg += 1;
	}
	if (ctrl_port) {
		hw->io_ports[IDE_CONTROL_OFFSET] = ctrl_port;
	} else {
		hw->io_ports[IDE_CONTROL_OFFSET] = hw->io_ports[IDE_DATA_OFFSET] + 0x206;
	}
	if (irq != NULL)
		*irq = 0;
	hw->io_ports[IDE_IRQ_OFFSET] = 0;
}

static __inline__ void ide_init_default_hwifs(void)
{
#ifndef CONFIG_BLK_DEV_IDEPCI
	hw_regs_t hw;
	int index;

	for(index = 0; index < MAX_HWIFS; index++) {
		ide_init_hwif_ports(&hw, ide_default_io_base(index), 0, NULL);
		hw.irq = ide_default_irq(ide_default_io_base(index));
		ide_register_hw(&hw, NULL);
	}
#endif /* CONFIG_BLK_DEV_IDEPCI */
}

#include <asm-generic/ide_iops.h>

#endif /* __KERNEL__ */

#endif /* __ASM_PARISC_IDE_H */
