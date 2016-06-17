/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * IDE routines for typical pc-like standard configurations.
 *
 * Copyright (C) 1998, 1999, 2001 by Ralf Baechle
 */
#include <linux/sched.h>
#include <linux/ide.h>
#include <linux/ioport.h>
#include <linux/hdreg.h>
#include <asm/ptrace.h>
#include <asm/hdreg.h>

static int std_ide_default_irq(ide_ioreg_t base)
{
	switch (base) {
		case 0x1f0: return 14;
		case 0x170: return 15;
		case 0x1e8: return 11;
		case 0x168: return 10;
		case 0x1e0: return 8;
		case 0x160: return 12;
		default:
			return 0;
	}
}

static ide_ioreg_t std_ide_default_io_base(int index)
{
	switch (index) {
		case 0:	return 0x1f0;
		case 1:	return 0x170;
		case 2: return 0x1e8;
		case 3: return 0x168;
		case 4: return 0x1e0;
		case 5: return 0x160;
		default:
			return 0;
	}
}

static void std_ide_init_hwif_ports (hw_regs_t *hw, ide_ioreg_t data_port,
                                     ide_ioreg_t ctrl_port, int *irq)
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

struct ide_ops std_ide_ops = {
	&std_ide_default_irq,
	&std_ide_default_io_base,
	&std_ide_init_hwif_ports
};
