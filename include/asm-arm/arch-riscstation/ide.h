/*
 *  linux/include/asm-arm/arch-rs/ide.h
 *
 *  Copyright (C) 1997 Russell King
 *  Copyright (C) 2002 Ben Dooks
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Modifications:
 *   29-07-1998	RMK	Major re-work of IDE architecture specific code
 */

#include <asm/irq.h>

/*
 * Set up a hw structure for a specified data port, control port and IRQ.
 * This should follow whatever the default interface uses.
 */

static __inline__ void
ide_init_hwif_ports(hw_regs_t *hw, int data_port, int ctrl_port, int *irq)
{
	ide_ioreg_t reg = (ide_ioreg_t) data_port;
	int i;

	memset(hw, 0, sizeof(*hw));

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		hw->io_ports[i] = reg;
		reg += 1;
	}
	hw->io_ports[IDE_CONTROL_OFFSET] = (ide_ioreg_t) ctrl_port;
	if (irq)
		*irq = 0;
}

/* we initialise our ide devices from the main ide core, due to problems
 * with doing it in this function
*/

static __inline__ void
ide_init_default_hwifs(void)
{
}
