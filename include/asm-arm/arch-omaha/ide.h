/*
 * linux/include/asm-arm/arch-omaha/ide.h
 *
 * Copyright (c) 2002 ARM Limited.
 * Copyright (c) 2000 Steve Hill (sjhill@cotw.com)
 *
 * Changelog:
 *  03-29-2000	SJH	Created file placeholder
 */
#include <linux/config.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/arch/hardware.h>

/*
 * Set up a hw structure for a specified data port, control port and IRQ.
 * This should follow whatever the default interface uses.
 */
static __inline__ void
ide_init_hwif_ports(hw_regs_t *hw, int data_port, int ctrl_port, int *irq)
{
	ide_ioreg_t reg;
	int i;
	int regincr = 1;
	
	memset(hw, 0, sizeof(*hw));

	reg = (ide_ioreg_t)data_port;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
	
		hw->io_ports[i] = reg;

		/* Special location in nCS2 for data register, as
		 * we need to be able to do 16-bit r/w.
		 */

		if(i == IDE_DATA_OFFSET)
			hw->io_ports[i] = reg + 0x04000000;

		reg += regincr;
	}
	
	hw->io_ports[IDE_CONTROL_OFFSET] = (ide_ioreg_t) ctrl_port;
	
	if(irq)
		*irq = 1;
}

/*
 * This registers the standard ports for this architecture with the IDE
 * driver.
 */
static __inline__ void
ide_init_default_hwifs(void)
{
        hw_regs_t hw;
	
        /* init the interface */
	ide_init_hwif_ports(&hw, IO_ADDRESS(0x01C00000), IO_ADDRESS(0x01C00006), NULL);
        hw.irq = 1;	// pld irq really
        ide_register_hw(&hw, NULL);
}
