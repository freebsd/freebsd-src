/*
 * linux/include/asm-arm/arch-shark/ide.h
 *
 * by Alexander Schulz
 *
 * derived from:
 * linux/include/asm-arm/arch-ebsa285/ide.h
 * Copyright (c) 1998 Russell King
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

/*
 * This registers the standard ports for this architecture with the IDE
 * driver.
 */
static __inline__ void
ide_init_default_hwifs(void)
{
	hw_regs_t hw;

	ide_init_hwif_ports(&hw, 0x1f0, 0x3f6, NULL);
	hw.irq = 14;
	ide_register_hw(&hw, NULL);
}

