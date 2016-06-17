/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * IDE routines for NEC VR4100 series standard configurations.
 *
 * Copyright (C) 1998, 1999, 2001 by Ralf Baechle
 * Copyright (C) 2003 Yoichi Yuasa <yuasa@hh.iij4u.or.jp>
 */
#include <linux/hdreg.h>
#include <linux/ide.h>

#include <asm/hdreg.h>

static int vr41xx_ide_default_irq(ide_ioreg_t base)
{
	return 0;
}

static ide_ioreg_t vr41xx_ide_default_io_base(int index)
{
	return 0;
}

static void vr41xx_ide_init_hwif_ports(hw_regs_t *hw, ide_ioreg_t data_port,
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

struct ide_ops vr41xx_ide_ops = {
	.ide_default_irq	= &vr41xx_ide_default_irq,
	.ide_default_io_base	= &vr41xx_ide_default_io_base,
	.ide_init_hwif_ports	= &vr41xx_ide_init_hwif_ports
};
