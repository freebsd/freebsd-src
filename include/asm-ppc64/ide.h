/*
 *  linux/include/asm-ppc/ide.h
 *
 *  Copyright (C) 1994-1996 Linus Torvalds & authors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */ 

/*
 *  This file contains the ppc64 architecture specific IDE code.
 */

#ifndef __ASMPPC64_IDE_H
#define __ASMPPC64_IDE_H

#ifdef __KERNEL__

#ifndef MAX_HWIFS
#define MAX_HWIFS	4
#endif

static __inline__ int ide_default_irq(ide_ioreg_t base) { return 0; }
static __inline__ ide_ioreg_t ide_default_io_base(int index) { return 0; }

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
}

#define __ide_mm_insw(p, a, c)  _insw_ns((volatile u16 *)(p), (a), (c))
#define __ide_mm_insl(p, a, c)  _insl_ns((volatile u32 *)(p), (a), (c))
#define __ide_mm_outsw(p, a, c) _outsw_ns((volatile u16 *)(p), (a), (c))
#define __ide_mm_outsl(p, a, c) _outsl_ns((volatile u32 *)(p), (a), (c))

#endif /* __KERNEL__ */

#endif /* __ASMPPC64_IDE_H */
