/*
 * linux/include/asm-arm/arch-anakin/ide.h
 *
 * Copyright 2001 Blue Mug Inc. for Acunia N.V.
 *
 * 08-jun-2001: Initial clone of arch-sa1100/ide.h by Jon McClintock
 *              (jonm@bluemug.com).
 */
 
#include <asm/irq.h>
#include <asm/hardware.h>

/*
 * Set up a hw structure for a specified data port, control port and IRQ.
 * This should follow whatever the default interface uses.
 */
static __inline__ void
ide_init_hwif_ports(hw_regs_t *hw, int data_port, int ctrl_port, int *irq)
{
	ide_ioreg_t reg;
	int i;
	int regincr = 4;
	
	memset(hw, 0, sizeof(*hw));

	reg = (ide_ioreg_t)data_port;

	for (i = IDE_DATA_OFFSET; i <= IDE_STATUS_OFFSET; i++) {
		hw->io_ports[i] = reg;
		reg += regincr;
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

    ide_init_hwif_ports(&hw, IO_BASE + COMPACTFLASH, 
    			IO_BASE + COMPACTFLASH + IDE_CONTROL_OFFSET, NULL);
    hw.irq = IRQ_COMPACTFLASH;
    ide_register_hw(&hw, NULL);
}


