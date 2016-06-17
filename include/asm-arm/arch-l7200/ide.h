/*
 * linux/include/asm-arm/arch-l7200/ide.h
 *
 * Copyright (c) 2000 Steve Hill (sjhill@cotw.com)
 *
 * Changelog:
 *  03-29-2000	SJH	Created file placeholder
 */
#include <asm/irq.h>

/*
 * Set up a hw structure for a specified data port, control port and IRQ.
 * This should follow whatever the default interface uses.
 */
static __inline__ void
ide_init_hwif_ports(hw_regs_t *hw, int data_port, int ctrl_port, int *irq)
{
}

/*
 * This registers the standard ports for this architecture with the IDE
 * driver.
 */
static __inline__ void
ide_init_default_hwifs(void)
{
}
